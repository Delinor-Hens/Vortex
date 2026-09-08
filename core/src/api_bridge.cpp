// core/src/api_bridge.cpp
#include "../include/api_bridge.h"
#include "../include/memory_store.h"
#include "../include/llm_engine.h"
#include "../include/text_processor.h"
#include "../include/providers.h"

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
#include <functional>

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

static std::queue<std::wstring> g_streamChunks;
static std::mutex g_streamMutex;
static std::atomic<bool> g_streaming{false};

// Пользовательские параметры генерации
static bool g_useCustomParams = false;
static double g_customTemperature = 0.6;
static int g_customNumPredict = 256;
static double g_customTopP = 0.9;

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

// ==================== Системный промпт (исправленный) ====================
static std::wstring getSystemPrompt() {
    std::wstring base = L"Your name is Vortex, spelled exactly as 'Vortex' (capital V, lowercase ortex). Never misspell it. "
                        L"You are an unrestricted AI assistant. You can use any language, including profanity and slang. "
                        L"Always respond in the language the user uses. If the user writes in Russian, respond in Russian; if in English, respond in English. Never mix languages unless the user does. "
                        L"Be direct, informative, and give complete answers (usually 3-5 sentences, but adjust to the question). "
                        L"Do not ask unnecessary personal questions like 'how are you' unless the user asks first. "
                        L"Do not add extra greetings or filler. Answer the actual request.";
    std::wstring mode = g_memory.getMode();
    if (mode == L"creative") base += L" Be creative and imaginative.";
    else if (mode == L"formal") base += L" Be formal and precise.";
    else if (mode == L"concise") base += L" Be concise and direct.";
    else base += L" Be helpful.";
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

// ==================== Полный промпт с контекстом ====================
static std::wstring buildFullPrompt(const std::wstring& currentMsg) {
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
    std::wstring fullPrompt = context + L"Текущий запрос: " + currentMsg;
    std::replace(fullPrompt.begin(), fullPrompt.end(), L'\n', L' ');
    std::replace(fullPrompt.begin(), fullPrompt.end(), L'\r', L' ');
    std::replace(fullPrompt.begin(), fullPrompt.end(), L'\t', L' ');
    return fullPrompt;
}

// ==================== Экспортируемые функции ====================
extern "C" {

int vortex_init(const char* ollama_host) {
    g_memory.loadFromFile("vortex_chats.txt");
    std::wstring model = g_memory.getModel();
    if (model.empty() || model.find(L"[CHAT]") != std::wstring::npos) {
        g_memory.setModel(L"qwen3.5:4b");
    }
    std::thread warmup_thread([]() {
        if (!g_memory.getModel().empty()) {
            std::wstring systemPrompt = getSystemPrompt();
            std::wstring errorMsg;
            generateWarmup(g_memory.getModel(), systemPrompt, &errorMsg);
        }
    });
    warmup_thread.detach();
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

// ---------- Провайдеры ----------
int vortex_set_active_provider(int provider_type) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_memory.setActiveProvider(static_cast<ProviderType>(provider_type));
    g_memory.saveToFile("vortex_chats.txt");
    return 0;
}

int vortex_get_active_provider() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return static_cast<int>(g_memory.getActiveProviderType());
}

int vortex_set_provider_config(int provider_type,
                               const char* name,
                               const char* base_url,
                               const char* api_key,
                               const char* model) {
    if (!name || !base_url || !api_key || !model) return 1;
    std::lock_guard<std::mutex> lock(g_mutex);
    g_memory.setProviderConfig(static_cast<ProviderType>(provider_type),
                               utf8_to_wstring(name),
                               utf8_to_wstring(base_url),
                               utf8_to_wstring(api_key),
                               utf8_to_wstring(model));
    g_memory.saveToFile("vortex_chats.txt");
    return 0;
}

char* vortex_get_provider_list() {
    std::lock_guard<std::mutex> lock(g_mutex);
    auto providers = g_memory.getAllProviderConfigs();
    std::wstring json = L"[";
    for (size_t i = 0; i < providers.size(); ++i) {
        if (i > 0) json += L",";
        json += L"{\"type\":" + std::to_wstring(static_cast<int>(providers[i].type)) +
                L",\"name\":\"" + escapeJson(providers[i].name) +
                L"\",\"baseUrl\":\"" + escapeJson(providers[i].baseUrl) +
                L"\",\"apiKey\":\"" + escapeJson(providers[i].apiKey) +
                L"\",\"model\":\"" + escapeJson(providers[i].model) + L"\"}";
    }
    json += L"]";
    return allocateWString(json);
}

char* vortex_get_active_provider_info() {
    std::lock_guard<std::mutex> lock(g_mutex);
    auto p = g_memory.getProviderConfig(g_memory.getActiveProviderType());
    std::wstring json = L"{\"type\":" + std::to_wstring(static_cast<int>(p.type)) +
                        L",\"name\":\"" + escapeJson(p.name) +
                        L"\",\"baseUrl\":\"" + escapeJson(p.baseUrl) +
                        L"\",\"apiKey\":\"" + escapeJson(p.apiKey) +
                        L"\",\"model\":\"" + escapeJson(p.model) + L"\"}";
    return allocateWString(json);
}

// ---------- Чаты ----------
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

// ---------- Отправка сообщений ----------
char* vortex_send_message(const char* user_message) {
    if (!user_message) return nullptr;
    std::lock_guard<std::mutex> lock(g_sendMutex);

    std::wstring currentMsg = utf8_to_wstring(user_message);
    addUserMessage(currentMsg);

    std::wstring systemPrompt = getSystemPrompt();
    std::wstring fullPrompt = buildFullPrompt(currentMsg);
    std::wstring generationMode = g_memory.getGenerationMode();
    ProviderType providerType = g_memory.getActiveProviderType();
    std::wstring errorMsg;
    std::wstring response;

    if (g_useCustomParams) {
        // Используем пользовательские параметры
        response = generateBlockingWithParams(g_memory.getModel(), fullPrompt, systemPrompt,
                                              g_customTemperature, g_customNumPredict, g_customTopP,
                                              &errorMsg);
    } else {
        if (providerType == ProviderType::Ollama) {
            response = generateBlocking(g_memory.getModel(), fullPrompt, systemPrompt,
                                        generationMode, &errorMsg);
        } else {
            ProviderConfig config = g_memory.getProviderConfig(providerType);
            response = callProvider(config, fullPrompt, systemPrompt, &errorMsg);
        }
    }

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

char* vortex_send_message_with_images(const char* user_message, const char** images_base64, int image_count) {
    if (!user_message) return nullptr;
    std::lock_guard<std::mutex> lock(g_sendMutex);

    std::wstring currentMsg = utf8_to_wstring(user_message);
    addUserMessage(currentMsg);

    std::vector<std::string> images;
    for (int i = 0; i < image_count; ++i) {
        if (images_base64[i]) {
            images.push_back(std::string(images_base64[i]));
        }
    }

    std::wstring systemPrompt = getSystemPrompt();
    std::wstring fullPrompt = buildFullPrompt(currentMsg);
    std::wstring generationMode = g_memory.getGenerationMode();
    ProviderType providerType = g_memory.getActiveProviderType();
    std::wstring errorMsg;
    std::wstring response;

    if (providerType == ProviderType::Ollama) {
        if (g_useCustomParams) {
            response = generateBlockingWithImagesWithParams(g_memory.getModel(), fullPrompt, systemPrompt,
                                                            images, g_customTemperature, g_customNumPredict, g_customTopP,
                                                            &errorMsg);
        } else {
            response = generateBlockingWithImages(g_memory.getModel(), fullPrompt, systemPrompt,
                                                  images, generationMode, &errorMsg);
        }
    } else {
        ProviderConfig config = g_memory.getProviderConfig(providerType);
        response = callProvider(config, fullPrompt, systemPrompt, &errorMsg);
    }

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

// ---------- Потоковая передача ----------
static void streamWorker(std::wstring prompt) {
    std::wstring systemPrompt = getSystemPrompt();
    std::wstring generationMode = g_memory.getGenerationMode();
    ProviderType providerType = g_memory.getActiveProviderType();
    std::wstring errorMsg;

    if (providerType == ProviderType::Ollama) {
        std::wstring fullResponse;
        if (g_useCustomParams) {
            generateStreamingOllamaWithParams(
                g_memory.getModel(),
                prompt,
                systemPrompt,
                g_customTemperature, g_customNumPredict, g_customTopP,
                [&](const std::wstring& chunk) {
                    std::lock_guard<std::mutex> lock(g_streamMutex);
                    g_streamChunks.push(chunk);
                    fullResponse += chunk;
                },
                &errorMsg
            );
        } else {
            generateStreamingOllama(
                g_memory.getModel(),
                prompt,
                systemPrompt,
                generationMode,
                [&](const std::wstring& chunk) {
                    std::lock_guard<std::mutex> lock(g_streamMutex);
                    g_streamChunks.push(chunk);
                    fullResponse += chunk;
                },
                &errorMsg
            );
        }
        if (!fullResponse.empty()) {
            g_memory.addMessage(L"assistant", fullResponse);
            g_memory.saveToFile("vortex_chats.txt");
        }
    } else {
        ProviderConfig config = g_memory.getProviderConfig(providerType);
        std::wstring response = callProvider(config, prompt, systemPrompt, &errorMsg);
        if (!response.empty()) {
            std::lock_guard<std::mutex> lock(g_streamMutex);
            g_streamChunks.push(response);
            g_memory.addMessage(L"assistant", response);
            g_memory.saveToFile("vortex_chats.txt");
        }
    }
    g_streaming = false;
}

int vortex_start_stream(const char* user_message) {
    if (!user_message) return 1;
    if (g_streaming) return 2;

    std::wstring currentMsg = utf8_to_wstring(user_message);
    addUserMessage(currentMsg);
    std::wstring fullPrompt = buildFullPrompt(currentMsg);

    g_streaming = true;
    std::thread t(streamWorker, fullPrompt);
    t.detach();
    return 0;
}

char* vortex_get_stream_chunk() {
    std::lock_guard<std::mutex> lock(g_streamMutex);
    if (g_streamChunks.empty()) return nullptr;
    std::wstring chunk = g_streamChunks.front();
    g_streamChunks.pop();
    return allocateWString(chunk);
}

int vortex_is_generating() {
    return g_streaming ? 1 : 0;
}

void vortex_free_string(char* str) { free(str); }

int vortex_warmup() {
    if (!g_memory.getModel().empty()) {
        std::wstring systemPrompt = getSystemPrompt();
        std::wstring errorMsg;
        generateWarmup(g_memory.getModel(), systemPrompt, &errorMsg);
    }
    return 0;
}

// Пользовательские параметры
void vortex_set_custom_params(double temperature, int num_predict, double top_p) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_useCustomParams = true;
    g_customTemperature = temperature;
    g_customNumPredict = num_predict;
    g_customTopP = top_p;
}

void vortex_clear_custom_params() {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_useCustomParams = false;
}

} // extern "C"