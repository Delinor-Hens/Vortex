// core/src/memory_store.cpp
#include "../include/memory_store.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cwchar>
#include <locale>
#include <codecvt>
#include <windows.h>
#include <dpapi.h>

#pragma comment(lib, "crypt32.lib")

MemoryStore::MemoryStore()
    : m_currentChatId(0),
      m_model(L"qwen3.5:4b"),
      m_mode(L"default"),
      m_generationMode(L"normal"),
      m_activeProvider(ProviderType::Ollama)
{
    m_chats.push_back({0, L"Default"});
    m_history.push_back({});

    m_providers = {
        {ProviderType::Ollama, L"Vortex", L"", L"", L"qwen3.5:4b"},
        {ProviderType::OpenAI, L"OpenAI", L"https://api.openai.com/v1", L"", L"gpt-4o-mini"},
        {ProviderType::Anthropic, L"Anthropic", L"https://api.anthropic.com/v1", L"", L"claude-3-5-sonnet-20241022"},
        {ProviderType::Gemini, L"Google Gemini", L"https://generativelanguage.googleapis.com/v1beta", L"", L"gemini-1.5-flash"},
        {ProviderType::Mistral, L"Mistral AI", L"https://api.mistral.ai/v1", L"", L"mistral-small-latest"},
        {ProviderType::DeepSeek, L"DeepSeek", L"https://api.deepseek.com", L"", L"deepseek-chat"},
        {ProviderType::OpenRouter, L"OpenRouter", L"https://openrouter.ai/api/v1", L"", L"openai/gpt-4o-mini"},
        {ProviderType::Groq, L"Groq", L"https://api.groq.com/openai/v1", L"", L"llama3-8b-8192"},
        {ProviderType::Together, L"Together AI", L"https://api.together.xyz/v1", L"", L"meta-llama/Llama-3-8b-chat-hf"}
    };
}

// ---------- Шифрование строк через DPAPI ----------
static std::wstring EncryptString(const std::wstring& plainText) {
    if (plainText.empty()) return L"";

    DATA_BLOB inBlob;
    inBlob.pbData = (BYTE*)plainText.c_str();
    inBlob.cbData = (DWORD)(plainText.size() * sizeof(wchar_t));

    DATA_BLOB outBlob;
    if (!CryptProtectData(&inBlob, L"Vortex API Key", NULL, NULL, NULL, CRYPTPROTECT_UI_FORBIDDEN, &outBlob))
        return L"";

    // Конвертируем бинарные данные в hex-строку
    std::wstring encrypted;
    encrypted.reserve(outBlob.cbData * 2);
    for (DWORD i = 0; i < outBlob.cbData; ++i) {
        wchar_t buf[3];
        swprintf(buf, 3, L"%02X", outBlob.pbData[i]);
        encrypted += buf;
    }

    LocalFree(outBlob.pbData);
    return encrypted;
}

static std::wstring DecryptString(const std::wstring& encryptedHex) {
    if (encryptedHex.empty()) return L"";

    // Переводим hex-строку в бинарные данные
    size_t len = encryptedHex.size() / 2;
    std::vector<BYTE> data(len);
    for (size_t i = 0; i < len; ++i) {
        swscanf(encryptedHex.c_str() + i * 2, L"%2hhx", &data[i]);
    }

    DATA_BLOB inBlob;
    inBlob.pbData = data.data();
    inBlob.cbData = (DWORD)data.size();

    DATA_BLOB outBlob;
    if (!CryptUnprotectData(&inBlob, NULL, NULL, NULL, NULL, CRYPTPROTECT_UI_FORBIDDEN, &outBlob))
        return L"";

    std::wstring decrypted((wchar_t*)outBlob.pbData, outBlob.cbData / sizeof(wchar_t));
    LocalFree(outBlob.pbData);
    return decrypted;
}

// ---------- Чаты и сообщения (без изменений) ----------
int MemoryStore::createChat(const std::wstring& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    int newId = m_chats.empty() ? 0 : m_chats.back().first + 1;
    m_chats.push_back({newId, name.empty() ? L"Новый чат" : name});
    m_history.push_back({});
    m_currentChatId = newId;
    return newId;
}

bool MemoryStore::deleteChat(int chatId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (chatId == 0) return false;
    auto it = std::find_if(m_chats.begin(), m_chats.end(),
                           [chatId](const std::pair<int, std::wstring>& p) {
                               return p.first == chatId;
                           });
    if (it == m_chats.end()) return false;
    size_t idx = std::distance(m_chats.begin(), it);
    m_chats.erase(it);
    m_history.erase(m_history.begin() + idx);
    if (m_currentChatId == chatId) {
        m_currentChatId = m_chats.empty() ? 0 : m_chats.front().first;
    }
    return true;
}

bool MemoryStore::clearChat(int chatId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = std::find_if(m_chats.begin(), m_chats.end(),
                           [chatId](const std::pair<int, std::wstring>& p) {
                               return p.first == chatId;
                           });
    if (it == m_chats.end()) return false;
    size_t idx = std::distance(m_chats.begin(), it);
    m_history[idx].clear();
    return true;
}

bool MemoryStore::selectChat(int chatId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = std::find_if(m_chats.begin(), m_chats.end(),
                           [chatId](const std::pair<int, std::wstring>& p) {
                               return p.first == chatId;
                           });
    if (it == m_chats.end()) return false;
    m_currentChatId = chatId;
    return true;
}

std::vector<std::pair<int, std::wstring>> MemoryStore::getChatList() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_chats;
}

int MemoryStore::getCurrentChatId() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_currentChatId;
}

void MemoryStore::addMessage(const std::wstring& role, const std::wstring& content) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = std::find_if(m_chats.begin(), m_chats.end(),
                           [this](const std::pair<int, std::wstring>& p) {
                               return p.first == m_currentChatId;
                           });
    if (it != m_chats.end()) {
        size_t idx = std::distance(m_chats.begin(), it);
        m_history[idx].push_back({role, content});
    }
}

std::vector<ChatMessage> MemoryStore::getHistory() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = std::find_if(m_chats.begin(), m_chats.end(),
                           [this](const std::pair<int, std::wstring>& p) {
                               return p.first == m_currentChatId;
                           });
    if (it != m_chats.end()) {
        size_t idx = std::distance(m_chats.begin(), it);
        return m_history[idx];
    }
    return {};
}

// ---------- Модель и режимы ----------
void MemoryStore::setModel(const std::wstring& model) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_model = model;
    for (auto& p : m_providers) {
        if (p.type == ProviderType::Ollama) {
            p.model = model;
            break;
        }
    }
}

std::wstring MemoryStore::getModel() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_model;
}

void MemoryStore::setMode(const std::wstring& mode) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_mode = mode;
}

std::wstring MemoryStore::getMode() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_mode;
}

void MemoryStore::setGenerationMode(const std::wstring& mode) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (mode == L"instant" || mode == L"normal" || mode == L"thinking") {
        m_generationMode = mode;
    }
}

std::wstring MemoryStore::getGenerationMode() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_generationMode;
}

// ---------- Провайдеры ----------
void MemoryStore::setActiveProvider(ProviderType type) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_activeProvider = type;
}

ProviderType MemoryStore::getActiveProviderType() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_activeProvider;
}

std::wstring MemoryStore::getActiveProviderName() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& p : m_providers) {
        if (p.type == m_activeProvider) return p.name;
    }
    return L"";
}

std::wstring MemoryStore::getActiveProviderModel() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& p : m_providers) {
        if (p.type == m_activeProvider) return p.model;
    }
    return L"";
}

std::wstring MemoryStore::getActiveProviderApiKey() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& p : m_providers) {
        if (p.type == m_activeProvider) {
            // Расшифровываем ключ
            return DecryptString(p.apiKey);
        }
    }
    return L"";
}

std::wstring MemoryStore::getActiveProviderBaseUrl() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& p : m_providers) {
        if (p.type == m_activeProvider) return p.baseUrl;
    }
    return L"";
}

void MemoryStore::setProviderConfig(ProviderType type, const std::wstring& name,
                                    const std::wstring& baseUrl, const std::wstring& apiKey,
                                    const std::wstring& model) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& p : m_providers) {
        if (p.type == type) {
            p.name = name;
            p.baseUrl = baseUrl;
            // Шифруем ключ перед сохранением
            p.apiKey = EncryptString(apiKey);
            p.model = model;
            break;
        }
    }
}

ProviderConfig MemoryStore::getProviderConfig(ProviderType type) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& p : m_providers) {
        if (p.type == type) {
            ProviderConfig config = p;
            // Расшифровываем ключ для возврата
            config.apiKey = DecryptString(p.apiKey);
            return config;
        }
    }
    return ProviderConfig();
}

std::vector<ProviderConfig> MemoryStore::getAllProviderConfigs() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<ProviderConfig> result;
    for (const auto& p : m_providers) {
        ProviderConfig config = p;
        // Расшифровываем ключ
        config.apiKey = DecryptString(p.apiKey);
        result.push_back(config);
    }
    return result;
}

// ---------- Сохранение / загрузка ----------
bool MemoryStore::saveToFile(const std::string& filename) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::wofstream file(filename, std::ios::binary);
    if (!file) return false;

    file << m_model << L"\n";
    file << m_mode << L"\n";
    file << m_generationMode << L"\n";
    file << static_cast<int>(m_activeProvider) << L"\n";

    file << m_providers.size() << L"\n";
    for (const auto& p : m_providers) {
        file << static_cast<int>(p.type) << L"\n";
        file << p.name << L"\n";
        file << p.baseUrl << L"\n";
        // apiKey уже зашифрован, просто сохраняем
        file << p.apiKey << L"\n";
        file << p.model << L"\n";
    }

    file << m_chats.size() << L"\n";
    for (size_t i = 0; i < m_chats.size(); ++i) {
        file << m_chats[i].first << L"|" << m_chats[i].second << L"\n";
        file << m_history[i].size() << L"\n";
        for (const auto& msg : m_history[i]) {
            file << msg.role << L"|" << msg.content << L"\n";
        }
    }
    file << m_currentChatId << L"\n";
    return true;
}

bool MemoryStore::loadFromFile(const std::string& filename) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::wifstream file(filename, std::ios::binary);
    if (!file) return false;

    std::getline(file, m_model);
    std::getline(file, m_mode);
    std::getline(file, m_generationMode);
    if (m_generationMode.empty()) m_generationMode = L"normal";

    int activeProvider;
    file >> activeProvider;
    file.ignore();
    m_activeProvider = static_cast<ProviderType>(activeProvider);

    size_t providerCount;
    file >> providerCount;
    file.ignore();
    m_providers.clear();
    for (size_t i = 0; i < providerCount; ++i) {
        ProviderConfig p;
        int typeInt;
        file >> typeInt;
        file.ignore();
        p.type = static_cast<ProviderType>(typeInt);
        std::getline(file, p.name);
        std::getline(file, p.baseUrl);
        std::getline(file, p.apiKey); // уже зашифрован
        std::getline(file, p.model);
        m_providers.push_back(p);
    }

    size_t chatCount;
    file >> chatCount;
    file.ignore();
    m_chats.clear();
    m_history.clear();
    for (size_t i = 0; i < chatCount; ++i) {
        int id;
        wchar_t sep;
        std::wstring name;
        file >> id >> sep;
        std::getline(file, name);
        m_chats.push_back({id, name});

        size_t msgCount;
        file >> msgCount;
        file.ignore();
        std::vector<ChatMessage> msgs;
        for (size_t j = 0; j < msgCount; ++j) {
            std::wstring role, content;
            std::getline(file, role, L'|');
            std::getline(file, content);
            msgs.push_back({role, content});
        }
        m_history.push_back(msgs);
    }
    file >> m_currentChatId;
    return true;
}