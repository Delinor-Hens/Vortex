// core/src/api_bridge.cpp
#include "../include/api_bridge.h"
#include "../include/memory_store.h"
#include "../include/llm_engine.h"
#include "../include/text_processor.h"

#include <windows.h>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <queue>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <cwchar>
#include <algorithm>

#pragma comment(lib, "kernel32")
#pragma comment(lib, "user32")

// ==================== UTF-8 <-> UTF-16 ====================
static std::wstring utf8_to_wstring(const std::string& str) {
    if (str.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    if (len <= 0) return L"";
    std::wstring result(len - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &result[0], len);
    return result;
}

static std::string wstring_to_utf8(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (len <= 0) return "";
    std::string result(len - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &result[0], len, nullptr, nullptr);
    return result;
}

static char* allocateString(const std::string& str) {
    char* buf = (char*)malloc(str.size() + 1);
    if (buf) memcpy(buf, str.c_str(), str.size() + 1);
    return buf;
}

static char* allocateWString(const std::wstring& wstr) {
    return allocateString(wstring_to_utf8(wstr));
}

// ==================== Глобальные объекты ====================
static MemoryStore g_memory;
static std::mutex g_mutex;
static std::mutex g_sendMutex;

// ==================== Экранирование JSON ====================
static std::wstring escapeJson(const std::wstring& s) {
    std::wstring result;
    result.reserve(s.size());
    for (wchar_t c : s) {
        switch (c) {
            case L'"': result += L"\\\""; break;
            case L'\\': result += L"\\\\"; break;
            case L'\b': result += L"\\b"; break;
            case L'\f': result += L"\\f"; break;
            case L'\n': result += L"\\n"; break;
            case L'\r': result += L"\\r"; break;
            case L'\t': result += L"\\t"; break;
            default:
                if (c < 0x20) {
                    wchar_t buf[8];
                    swprintf(buf, 8, L"\\u%04x", (unsigned)c);
                    result += buf;
                } else {
                    result += c;
                }
        }
    }
    return result;
}

// ==================== Системный промпт ====================
static std::wstring getSystemPrompt() {
    std::wstring base = L"Your name is Vortex. You are a helpful AI assistant created by Delinor. "
                        L"Always respond in the language the user uses. "
                        L"If the user writes in Russian, respond in Russian. "
                        L"Match the user's formality: if they use 'ты', use 'ты'; if they use 'вы', use 'вы'. "
                        L"Pay attention to correct Russian grammar and cases (e.g., 'помочь вам', not 'помочь для вас'). "
                        L"Never use Chinese characters or mix languages unless asked. "
                        L"Be intelligent, accurate, and moderately detailed. "
                        L"Provide answers of 3-5 sentences unless the user asks for brevity. "
                        L"Do not mention Qwen, Alibaba Cloud, or any underlying model. "
                        L"If real-time data (like weather) is not available in the prompt, say you cannot provide it and ask for clarification. ";
    std::wstring mode = g_memory.getMode();
    if (mode == L"creative") base += L"Be creative and imaginative.";
    else if (mode == L"formal") base += L"Be formal and precise.";
    else if (mode == L"concise") base += L"Be concise and direct.";
    else base += L"Be friendly and helpful.";
    return base;
}

// ==================== Добавление сообщений ====================
static void addUserMessage(const std::wstring& prompt) {
    g_memory.addMessage(L"user", prompt);
    g_memory.saveToFile("vortex_chats.txt");
}

static void addAssistantMessage(const std::wstring& response) {
    g_memory.addMessage(L"assistant", response);
    g_memory.saveToFile("vortex_chats.txt");
}

// ==================== Экспортируемые функции ====================
extern "C" {

int vortex_init(const char* ollama_host) {
    g_memory.loadFromFile("vortex_chats.txt");
    // Проверяем модель
    std::wstring model = g_memory.getModel();
    if (model.empty() || model.find(L"[CHAT]") != std::wstring::npos) {
        g_memory.setModel(L"qwen3.5:4b");
    }
    return 0;
}

void vortex_shutdown() {
    g_memory.saveToFile("vortex_chats.txt");
}

char* vortex_get_models() {
    std::lock_guard<std::mutex> lock(g_mutex);
    auto models = getAvailableModels();
    std::wstring json = L"[";
    for (size_t i = 0; i < models.size(); ++i) {
        if (i > 0) json += L",";
        json += L"\"" + escapeJson(models[i]) + L"\"";
    }
    json += L"]";
    return allocateWString(json);
}

int vortex_pull_model(const char* model_name) {
    if (!model_name) return 1;
    return 1;
}

int vortex_set_model(const char* model_name) {
    if (!model_name) return 1;
    std::lock_guard<std::mutex> lock(g_mutex);
    g_memory.setModel(utf8_to_wstring(model_name));
    return 0;
}

int vortex_set_mode(const char* mode_name) {
    if (!mode_name) return 1;
    std::lock_guard<std::mutex> lock(g_mutex);
    g_memory.setMode(utf8_to_wstring(mode_name));
    return 0;
}

int vortex_set_generation_mode(const char* mode_name) {
    if (!mode_name) return 1;
    std::lock_guard<std::mutex> lock(g_mutex);
    g_memory.setGenerationMode(utf8_to_wstring(mode_name));
    return 0;
}

char* vortex_get_current_model() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return allocateWString(g_memory.getModel());
}

char* vortex_get_current_mode() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return allocateWString(g_memory.getMode());
}

char* vortex_get_current_generation_mode() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return allocateWString(g_memory.getGenerationMode());
}

int vortex_create_chat(const char* chat_name) {
    std::lock_guard<std::mutex> lock(g_mutex);
    std::wstring name = chat_name ? utf8_to_wstring(chat_name) : L"";
    int id = g_memory.createChat(name);
    g_memory.saveToFile("vortex_chats.txt");
    return id;
}

int vortex_delete_chat(int chat_id) {
    std::lock_guard<std::mutex> lock(g_mutex);
    bool ok = g_memory.deleteChat(chat_id);
    if (ok) g_memory.saveToFile("vortex_chats.txt");
    return ok ? 0 : 1;
}

int vortex_clear_chat(int chat_id) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_memory.clearChat(chat_id)) return 1;
    g_memory.saveToFile("vortex_chats.txt");
    return 0;
}

int vortex_select_chat(int chat_id) {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_memory.selectChat(chat_id) ? 0 : 1;
}

char* vortex_get_chats() {
    std::lock_guard<std::mutex> lock(g_mutex);
    auto list = g_memory.getChatList();
    std::wstring json = L"[";
    for (size_t i = 0; i < list.size(); ++i) {
        if (i > 0) json += L",";
        json += L"{\"id\":" + std::to_wstring(list[i].first) +
                L",\"name\":\"" + escapeJson(list[i].second) + L"\"}";
    }
    json += L"]";
    return allocateWString(json);
}

char* vortex_get_history() {
    std::lock_guard<std::mutex> lock(g_mutex);
    auto history = g_memory.getHistory();
    std::wstring json = L"[";
    for (size_t i = 0; i < history.size(); ++i) {
        if (i > 0) json += L",";
        json += L"{\"role\":\"" + std::wstring(history[i].role == L"user" ? L"user" : L"assistant") +
                L"\",\"content\":\"" + escapeJson(history[i].content) + L"\"}";
    }
    json += L"]";
    return allocateWString(json);
}

char* vortex_send_message(const char* user_message) {
    if (!user_message) return nullptr;
    std::lock_guard<std::mutex> lock(g_sendMutex);

    std::wstring currentMsg = utf8_to_wstring(user_message);
    addUserMessage(currentMsg);

    auto history = g_memory.getHistory();
    std::wstring context;
    size_t start = (history.size() > 6) ? history.size() - 6 : 0;
    for (size_t i = start; i < history.size(); ++i) {
        if (history[i].role == L"user") {
            context += L"Пользователь: " + history[i].content + L" ";
        } else {
            context += L"Vortex: " + history[i].content + L" ";
        }
    }

    std::wstring systemPrompt = getSystemPrompt();
    std::wstring fullPrompt = context + L"Текущий запрос: " + currentMsg;

    // Убираем переводы строк и табуляции
    std::replace(fullPrompt.begin(), fullPrompt.end(), L'\n', L' ');
    std::replace(fullPrompt.begin(), fullPrompt.end(), L'\r', L' ');
    std::replace(fullPrompt.begin(), fullPrompt.end(), L'\t', L' ');

    std::wstring generationMode = g_memory.getGenerationMode();

    // Защита модели
    std::wstring model = g_memory.getModel();
    if (model.empty() || model.find(L"[CHAT]") != std::wstring::npos) {
        model = L"qwen3.5:4b";
        g_memory.setModel(model);
    }

    // Лог
    {
        std::ofstream log("debug_vortex.log", std::ios::app);
        log << "=== vortex_send_message ===" << std::endl;
        log << "Model: " << wstring_to_utf8(model) << std::endl;
        log << "GenerationMode: " << wstring_to_utf8(generationMode) << std::endl;
        log << "SystemPrompt: " << wstring_to_utf8(systemPrompt) << std::endl;
        log << "FullPrompt: " << wstring_to_utf8(fullPrompt) << std::endl;
        log.close();
    }

    std::wstring errorMsg;
    std::wstring response = generateBlocking(model, fullPrompt, systemPrompt,
                                             generationMode, &errorMsg);

    if (response.empty()) {
        if (!errorMsg.empty()) {
            response = L"[Ошибка: " + errorMsg + L"]";
        } else {
            response = L"[Ошибка: не удалось получить ответ]";
        }
    }

    addAssistantMessage(response);
    return allocateWString(response);
}

int vortex_start_stream(const char* user_message) { return 1; }
char* vortex_get_stream_chunk() { return nullptr; }
int vortex_is_generating() { return 0; }

void vortex_free_string(char* str) { free(str); }

int vortex_warmup() {
    if (!g_memory.getModel().empty()) {
        std::wstring systemPrompt = getSystemPrompt();
        std::wstring generationMode = g_memory.getGenerationMode();
        generateBlocking(g_memory.getModel(), L"", systemPrompt, generationMode, nullptr);
    }
    return 0;
}

} // extern "C"
