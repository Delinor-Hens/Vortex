// core/src/memory_store.cpp
#include "../include/memory_store.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cwchar>
#include <locale>
#include <codecvt>

// ==================== Кроссплатформенное шифрование ====================
// Временная замена DPAPI: XOR с фиксированным ключом и hex-кодирование.
// Для продакшена рекомендуется заменить на OpenSSL (AES) или libsodium.

static const std::wstring XOR_KEY = L"VortexSecretKey123"; // ключ

static std::wstring ByteToHex(const std::vector<unsigned char>& data) {
    const wchar_t* hex = L"0123456789ABCDEF";
    std::wstring result;
    result.reserve(data.size() * 2);
    for (unsigned char b : data) {
        result += hex[(b >> 4) & 0x0F];
        result += hex[b & 0x0F];
    }
    return result;
}

static std::vector<unsigned char> HexToBytes(const std::wstring& hex) {
    std::vector<unsigned char> bytes;
    if (hex.size() % 2 != 0) return bytes;
    bytes.reserve(hex.size() / 2);
    for (size_t i = 0; i < hex.size(); i += 2) {
        wchar_t c1 = hex[i];
        wchar_t c2 = hex[i+1];
        auto hexVal = [](wchar_t c) -> int {
            if (c >= L'0' && c <= L'9') return c - L'0';
            if (c >= L'A' && c <= L'F') return c - L'A' + 10;
            if (c >= L'a' && c <= L'f') return c - L'a' + 10;
            return -1;
        };
        int v1 = hexVal(c1);
        int v2 = hexVal(c2);
        if (v1 < 0 || v2 < 0) return {};
        bytes.push_back(static_cast<unsigned char>((v1 << 4) | v2));
    }
    return bytes;
}

static std::wstring EncryptString(const std::wstring& plainText) {
    if (plainText.empty()) return L"";
    std::vector<unsigned char> data(plainText.begin(), plainText.end()); // копируем как есть (wchar_t -> unsigned char)
    // XOR с ключом
    for (size_t i = 0; i < data.size(); ++i) {
        data[i] ^= static_cast<unsigned char>(XOR_KEY[i % XOR_KEY.size()]);
    }
    return ByteToHex(data);
}

static std::wstring DecryptString(const std::wstring& encryptedHex) {
    if (encryptedHex.empty()) return L"";
    std::vector<unsigned char> data = HexToBytes(encryptedHex);
    if (data.empty()) return L"";
    for (size_t i = 0; i < data.size(); ++i) {
        data[i] ^= static_cast<unsigned char>(XOR_KEY[i % XOR_KEY.size()]);
    }
    // преобразуем обратно в wstring (важно: если исходная строка была wchar_t, то данные содержат пары байт)
    // Здесь мы предполагаем, что исходный текст был wchar_t (UTF-16LE на Windows). Для кроссплатформенности это не идеально,
    // но временно сохраняем поведение как с DPAPI (также работало с wchar_t).
    if (data.size() % sizeof(wchar_t) != 0) {
        // если не кратно размеру wchar_t, вернём пустую строку, чтобы избежать ошибок
        return L"";
    }
    std::wstring decrypted(data.size() / sizeof(wchar_t), L'\0');
    memcpy(&decrypted[0], data.data(), data.size());
    return decrypted;
}

// ==================== Конструктор и инициализация ====================
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
    // ВАЖНО: не вызываем imbue, чтобы избежать 0xc00000ff

    file << m_model << L"\n";
    file << m_mode << L"\n";
    file << m_generationMode << L"\n";
    file << static_cast<int>(m_activeProvider) << L"\n";

    file << m_providers.size() << L"\n";
    for (const auto& p : m_providers) {
        file << static_cast<int>(p.type) << L"\n";
        file << p.name << L"\n";
        file << p.baseUrl << L"\n";
        file << p.apiKey << L"\n";   // уже зашифрован
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
    // ВАЖНО: не вызываем imbue

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
        std::getline(file, p.apiKey);
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