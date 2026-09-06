// core/src/memory_store.cpp
#include "../include/memory_store.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cwchar>
#include <locale>
#include <codecvt>

MemoryStore::MemoryStore()
    : m_currentChatId(0),
      m_model(L"qwen3.5:4b"),
      m_mode(L"default"),
      m_generationMode(L"normal")
{
    m_chats.push_back({0, L"Default"});
    m_history.push_back({});
}

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
    if (chatId == 0) return false; // нельзя удалить Default
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

void MemoryStore::setModel(const std::wstring& model) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_model = model;
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

bool MemoryStore::saveToFile(const std::string& filename) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::wofstream file(filename, std::ios::binary);
    if (!file) return false;
    // Не используем imbue, чтобы избежать проблем с локалью

    file << m_model << L"\n";
    file << m_mode << L"\n";
    file << m_generationMode << L"\n";
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
    // Не используем imbue

    std::getline(file, m_model);
    std::getline(file, m_mode);
    std::getline(file, m_generationMode);
    if (m_generationMode.empty()) m_generationMode = L"normal";

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