#pragma once
#include <string>
#include <vector>
#include <mutex>

struct ChatMessage {
    std::wstring role;    // L"user" или L"assistant"
    std::wstring content;
};

// Новые структуры для провайдеров
enum class ProviderType {
    Ollama = 0,
    OpenAI = 1,
    Anthropic = 2,
    Gemini = 3,
    Mistral = 4,
    DeepSeek = 5,
    OpenRouter = 6,
    Groq = 7,
    Together = 8
};

struct ProviderConfig {
    ProviderType type;
    std::wstring name;
    std::wstring baseUrl;
    std::wstring apiKey;
    std::wstring model;
};

class MemoryStore {
public:
    MemoryStore();

    // Чаты (без изменений)
    int createChat(const std::wstring& name = L"");
    bool deleteChat(int chatId);
    bool clearChat(int chatId);
    bool selectChat(int chatId);
    std::vector<std::pair<int, std::wstring>> getChatList() const;
    int getCurrentChatId() const;

    // Сообщения (без изменений)
    void addMessage(const std::wstring& role, const std::wstring& content);
    std::vector<ChatMessage> getHistory() const;

    // Модель и режим (локальные)
    void setModel(const std::wstring& model);
    std::wstring getModel() const;
    void setMode(const std::wstring& mode);
    std::wstring getMode() const;
    void setGenerationMode(const std::wstring& mode);
    std::wstring getGenerationMode() const;

    // Новые методы для провайдеров
    void setActiveProvider(ProviderType type);
    ProviderType getActiveProviderType() const;
    std::wstring getActiveProviderName() const;
    std::wstring getActiveProviderModel() const;
    std::wstring getActiveProviderApiKey() const;
    std::wstring getActiveProviderBaseUrl() const;

    void setProviderConfig(ProviderType type, const std::wstring& name,
                           const std::wstring& baseUrl, const std::wstring& apiKey,
                           const std::wstring& model);
    ProviderConfig getProviderConfig(ProviderType type) const;
    std::vector<ProviderConfig> getAllProviderConfigs() const;

    // Сохранение/загрузка (с поддержкой провайдеров)
    bool saveToFile(const std::string& filename);
    bool loadFromFile(const std::string& filename);

private:
    mutable std::mutex m_mutex;
    std::vector<std::pair<int, std::wstring>> m_chats;
    std::vector<std::vector<ChatMessage>> m_history;
    int m_currentChatId;
    std::wstring m_model;
    std::wstring m_mode;
    std::wstring m_generationMode;

    // Провайдеры
    ProviderType m_activeProvider;
    std::vector<ProviderConfig> m_providers;
};