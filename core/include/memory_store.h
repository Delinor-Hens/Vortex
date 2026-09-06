#pragma once
#include <string>
#include <vector>
#include <mutex>

struct ChatMessage {
    std::wstring role;    // L"user" или L"assistant"
    std::wstring content;
};

class MemoryStore {
public:
    MemoryStore();

    // Чаты
    int createChat(const std::wstring& name = L"");
    bool deleteChat(int chatId);
    bool clearChat(int chatId);
    bool selectChat(int chatId);
    std::vector<std::pair<int, std::wstring>> getChatList() const;
    int getCurrentChatId() const;

    // Сообщения
    void addMessage(const std::wstring& role, const std::wstring& content);
    std::vector<ChatMessage> getHistory() const;

    // Модель и режим
    void setModel(const std::wstring& model);
    std::wstring getModel() const;
    void setMode(const std::wstring& mode);          // creative, formal, concise, default
    std::wstring getMode() const;
    void setGenerationMode(const std::wstring& mode); // instant, normal, thinking
    std::wstring getGenerationMode() const;

    // Сохранение/загрузка
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
};