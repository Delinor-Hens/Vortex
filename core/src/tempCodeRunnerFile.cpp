// core/src/llm_engine.cpp
#include "../include/llm_engine.h"

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <string>
#include <vector>
#include <mutex>
#include <cstring>
#include <stdexcept>
#include <sstream>

#pragma comment(lib, "kernel32")
#pragma comment(lib, "user32")

// ==================== Динамическая загрузка Winsock ====================
typedef int (WINAPI *WSAStartup_t)(WORD, void*);
typedef SOCKET (WINAPI *socket_t)(int, int, int);
typedef int (WINAPI *connect_t)(SOCKET, const struct sockaddr*, int);
typedef int (WINAPI *send_t)(SOCKET, const char*, int, int);
typedef int (WINAPI *recv_t)(SOCKET, char*, int, int);
typedef int (WINAPI *closesocket_t)(SOCKET);
typedef unsigned short (WINAPI *htons_t)(unsigned short);
typedef unsigned long (WINAPI *inet_addr_t)(const char*);
typedef int (WINAPI *WSACleanup_t)(void);

static WSAStartup_t pWSAStartup = nullptr;
static socket_t psocket = nullptr;
static connect_t pconnect = nullptr;
static send_t psend = nullptr;
static recv_t precv = nullptr;
static closesocket_t pclosesocket = nullptr;
static htons_t phtons = nullptr;
static inet_addr_t pinet_addr = nullptr;
static WSACleanup_t pWSACleanup = nullptr;

static std::mutex g_netMutex;
static bool g_netInitialized = false;
static std::string g_ollamaHost = "127.0.0.1";

// ==================== Загрузка функций ====================
static bool loadWinsockFunctions() {
    HMODULE hWs2 = LoadLibraryA("ws2_32.dll");
    if (!hWs2) return false;
    pWSAStartup = (WSAStartup_t)GetProcAddress(hWs2, "WSAStartup");
    psocket = (socket_t)GetProcAddress(hWs2, "socket");
    pconnect = (connect_t)GetProcAddress(hWs2, "connect");
    psend = (send_t)GetProcAddress(hWs2, "send");
    precv = (recv_t)GetProcAddress(hWs2, "recv");
    pclosesocket = (closesocket_t)GetProcAddress(hWs2, "closesocket");
    phtons = (htons_t)GetProcAddress(hWs2, "htons");
    pinet_addr = (inet_addr_t)GetProcAddress(hWs2, "inet_addr");
    pWSACleanup = (WSACleanup_t)GetProcAddress(hWs2, "WSACleanup");
    return pWSAStartup && psocket && pconnect && psend && precv &&
           pclosesocket && phtons && pinet_addr && pWSACleanup;
}

static void ensureNetworkInitialized() {
    if (!g_netInitialized) {
        std::lock_guard<std::mutex> lock(g_netMutex);
        if (!g_netInitialized) {
            if (!loadWinsockFunctions()) throw std::runtime_error("Failed to load Winsock");
            char wsaBuf[512];
            if (pWSAStartup(0x0202, reinterpret_cast<void*>(wsaBuf)) != 0)
                throw std::runtime_error("WSAStartup failed");
            g_netInitialized = true;
        }
    }
}

// ==================== Конвертация UTF-8 <-> UTF-16 ====================
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

// ==================== HTTP GET для Ollama ====================
static std::string httpGetOllama(const std::string& path) {
    ensureNetworkInitialized();
    SOCKET sock = psocket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) return "";
    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = phtons(11434);
    addr.sin_addr.s_addr = pinet_addr(g_ollamaHost.c_str());
    if (pconnect(sock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        pclosesocket(sock);
        return "";
    }
    std::string request = "GET " + path + " HTTP/1.1\r\nHost: 127.0.0.1:11434\r\nConnection: close\r\n\r\n";
    if (psend(sock, request.c_str(), request.length(), 0) == SOCKET_ERROR) {
        pclosesocket(sock);
        return "";
    }
    std::string response;
    char buf[4096];
    int received;
    while ((received = precv(sock, buf, sizeof(buf), 0)) > 0) {
        response.append(buf, received);
    }
    pclosesocket(sock);
    size_t pos = response.find("\r\n\r\n");
    if (pos == std::string::npos) return "";
    return response.substr(pos + 4);
}

// ==================== HTTP POST для Ollama ====================
static std::string httpPostOllama(const std::string& body) {
    ensureNetworkInitialized();
    SOCKET sock = psocket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) return "";
    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = phtons(11434);
    addr.sin_addr.s_addr = pinet_addr(g_ollamaHost.c_str());
    if (pconnect(sock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        pclosesocket(sock);
        return "";
    }
    std::string request = "POST /api/generate HTTP/1.1\r\n"
                          "Host: 127.0.0.1:11434\r\n"
                          "Content-Type: application/json\r\n"
                          "Content-Length: " + std::to_string(body.length()) + "\r\n"
                          "Connection: close\r\n\r\n" + body;
    if (psend(sock, request.c_str(), request.length(), 0) == SOCKET_ERROR) {
        pclosesocket(sock);
        return "";
    }
    std::string response;
    char buf[4096];
    int received;
    while ((received = precv(sock, buf, sizeof(buf), 0)) > 0) {
        response.append(buf, received);
    }
    pclosesocket(sock);
    size_t pos = response.find("\r\n\r\n");
    if (pos == std::string::npos) return "";
    return response.substr(pos + 4);
}

// ==================== Извлечение поля "response" из JSON-строки ====================
static std::string extractResponseField(const std::string& jsonLine) {
    const std::string key = "\"response\":\"";
    size_t pos = jsonLine.find(key);
    if (pos == std::string::npos) return "";
    pos += key.length();
    size_t endPos = jsonLine.find('"', pos);
    if (endPos == std::string::npos) return "";
    std::string result = jsonLine.substr(pos, endPos - pos);
    // Заменяем экранированные \n на реальные переводы строк
    size_t start = 0;
    while ((start = result.find("\\n", start)) != std::string::npos) {
        result.replace(start, 2, "\n");
        start += 1;
    }
    return result;
}

// ==================== Реализация функций из llm_engine.h ====================
std::vector<std::wstring> getAvailableModels() {
    ensureNetworkInitialized();
    std::vector<std::wstring> models;
    std::string resp = httpGetOllama("/api/tags");
    if (resp.empty()) return models;
    size_t pos = 0;
    while ((pos = resp.find("\"name\":\"", pos)) != std::string::npos) {
        pos += 8;
        size_t end = resp.find('"', pos);
        if (end != std::string::npos) {
            models.push_back(utf8_to_wstring(resp.substr(pos, end - pos)));
            pos = end + 1;
        } else break;
    }
    return models;
}

bool pullModel(const std::wstring& modelName) {
    // Заглушка: не используется в текущей версии
    return false;
}

std::wstring generateBlocking(const std::wstring& model,
                              const std::wstring& prompt,
                              const std::wstring& systemPrompt,
                              std::wstring* errorMsg) {
    ensureNetworkInitialized();
    std::wstring fullResponse;
    bool error = false;
    std::wstring localError;

    std::string utf8prompt = wstring_to_utf8(prompt);
    std::string escapedPrompt;
    for (char c : utf8prompt) {
        switch (c) {
            case '\\': escapedPrompt += "\\\\"; break;
            case '"':  escapedPrompt += "\\\""; break;
            case '\n': escapedPrompt += "\\n"; break;
            case '\r': escapedPrompt += "\\r"; break;
            case '\t': escapedPrompt += "\\t"; break;
            default:   escapedPrompt += c;
        }
    }
    std::string utf8model = wstring_to_utf8(model);
    std::string utf8system = wstring_to_utf8(systemPrompt);
    // Системный промпт тоже нужно экранировать (на всякий случай)
    std::string escapedSystem;
    for (char c : utf8system) {
        switch (c) {
            case '\\': escapedSystem += "\\\\"; break;
            case '"':  escapedSystem += "\\\""; break;
            case '\n': escapedSystem += "\\n"; break;
            case '\r': escapedSystem += "\\r"; break;
            case '\t': escapedSystem += "\\t"; break;
            default:   escapedSystem += c;
        }
    }

    // Добавляем "think":false, чтобы модель не генерировала размышления
    std::string jsonBody = "{\"model\":\"" + utf8model +
                           "\",\"prompt\":\"" + escapedPrompt +
                           "\",\"stream\":false,\"think\":false,\"system\":\"" + escapedSystem +
                           "\",\"options\":{\"num_predict\":512,\"temperature\":0.6,\"top_p\":0.9}}";

    std::string response = httpPostOllama(jsonBody);
    if (response.empty()) {
        error = true;
        localError = L"Empty response from Ollama";
    } else {
        // НОВЫЙ НАДЁЖНЫЙ ПАРСЕР
        const std::string key = "\"response\":\"";
        size_t pos = response.find(key);
        if (pos != std::string::npos) {
            pos += key.length();
            std::string answer;
            bool escaped = false;
            for (size_t i = pos; i < response.length(); ++i) {
                char c = response[i];
                if (escaped) {
                    if (c == 'n') answer += '\n';
                    else if (c == 't') answer += '\t';
                    else if (c == 'r') answer += '\r';
                    else if (c == '\\') answer += '\\';
                    else if (c == '"') answer += '"';
                    else answer += c;
                    escaped = false;
                } else {
                    if (c == '\\') {
                        escaped = true;
                    } else if (c == '"') {
                        break; // конец значения
                    } else {
                        answer += c;
                    }
                }
            }
            fullResponse = utf8_to_wstring(answer);
        } else {
            // Проверяем наличие ошибки
            size_t errPos = response.find("\"error\":\"");
            if (errPos != std::string::npos) {
                errPos += 10;
                std::string errMsg;
                bool esc = false;
                for (size_t i = errPos; i < response.length(); ++i) {
                    char c = response[i];
                    if (esc) {
                        errMsg += c;
                        esc = false;
                    } else if (c == '\\') {
                        esc = true;
                    } else if (c == '"') {
                        break;
                    } else {
                        errMsg += c;
                    }
                }
                error = true;
                localError = L"Ollama error: " + utf8_to_wstring(errMsg);
            } else {
                error = true;
                localError = L"Response field not found";
            }
        }
    }

    if (error) {
        if (errorMsg) *errorMsg = localError;
        return L"";
    }
    return fullResponse;
}

// ==================== Заглушки для поиска и погоды ====================
std::wstring searchWeb(const std::wstring& query) {
    // TODO: реализовать при необходимости
    return L"";
}

std::wstring fetchWeather(const std::wstring& city) {
    // TODO: реализовать при необходимости
    return L"";
}