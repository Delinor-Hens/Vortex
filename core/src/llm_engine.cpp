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
#include <functional>
#include <thread>
#include <atomic>
#include <chrono>
#include <algorithm>

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

// ==================== Декодирование \uXXXX и сохранение слэшей ====================
static std::string decodeUnicodeEscape(const std::string& input) {
    std::string output;
    for (size_t i = 0; i < input.length(); ++i) {
        if (input[i] == '\\' && i + 1 < input.length()) {
            if (input[i+1] == 'u' && i + 5 < input.length()) {
                std::string hex = input.substr(i+2, 4);
                char* endptr = nullptr;
                unsigned long code = strtoul(hex.c_str(), &endptr, 16);
                if (endptr && *endptr == '\0') {
                    if (code <= 0x7F) {
                        output += static_cast<char>(code);
                    } else if (code <= 0x7FF) {
                        output += static_cast<char>(0xC0 | (code >> 6));
                        output += static_cast<char>(0x80 | (code & 0x3F));
                    } else if (code <= 0xFFFF) {
                        output += static_cast<char>(0xE0 | (code >> 12));
                        output += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
                        output += static_cast<char>(0x80 | (code & 0x3F));
                    } else {
                        output += '?';
                    }
                    i += 5;
                    continue;
                }
            }
            // Обработка обычных escape-символов
            switch (input[i+1]) {
                case 'n': output += '\n'; i++; break;
                case 't': output += '\t'; i++; break;
                case 'r': output += '\r'; i++; break;
                case '\\': output += '\\'; i++; break;
                case '"': output += '"'; i++; break;
                case 'b': output += '\\'; output += 'b'; i++; break;   // сохраняем \b
                case 'f': output += '\\'; output += 'f'; i++; break;   // сохраняем \f
                default: output += '\\'; output += input[i+1]; i++; break; // сохраняем слэш и символ
            }
            continue;
        }
        output += input[i];
    }
    return output;
}

static std::wstring extractResponse(const std::string& json) {
    const std::string key = "\"response\":\"";
    size_t pos = json.find(key);
    if (pos == std::string::npos) return L"";

    pos += key.length();
    std::string answer;
    bool escaped = false;
    for (size_t i = pos; i < json.length(); ++i) {
        char c = json[i];
        if (escaped) {
            switch (c) {
                case 'n': answer += '\n'; break;
                case 't': answer += '\t'; break;
                case 'r': answer += '\r'; break;
                case '\\': answer += '\\'; break;
                case '"': answer += '"'; break;
                case 'b': answer += '\\'; answer += 'b'; break;   // сохраняем \b
                case 'f': answer += '\\'; answer += 'f'; break;   // сохраняем \f
                case 'u': {
                    if (i + 4 < json.length()) {
                        std::string hex = json.substr(i+1, 4);
                        char* endptr = nullptr;
                        unsigned long code = strtoul(hex.c_str(), &endptr, 16);
                        if (endptr && *endptr == '\0') {
                            if (code <= 0x7F) {
                                answer += static_cast<char>(code);
                            } else if (code <= 0x7FF) {
                                answer += static_cast<char>(0xC0 | (code >> 6));
                                answer += static_cast<char>(0x80 | (code & 0x3F));
                            } else if (code <= 0xFFFF) {
                                answer += static_cast<char>(0xE0 | (code >> 12));
                                answer += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
                                answer += static_cast<char>(0x80 | (code & 0x3F));
                            } else {
                                answer += '?';
                            }
                            i += 4;
                        } else {
                            answer += 'u';
                        }
                    } else {
                        answer += 'u';
                    }
                    break;
                }
                default: answer += '\\'; answer += c; break;  // сохраняем обратный слэш
            }
            escaped = false;
        } else {
            if (c == '\\') escaped = true;
            else if (c == '"') break;
            else answer += c;
        }
    }
    return utf8_to_wstring(decodeUnicodeEscape(answer));
}

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

static std::string httpPostOllamaGenerate(const std::string& body) {
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

// Общая функция генерации (без изображений) с явными параметрами
static std::wstring generateBlockingInternalWithParams(const std::wstring& model,
                                                       const std::wstring& prompt,
                                                       const std::wstring& systemPrompt,
                                                       const std::vector<std::string>* imagesBase64,
                                                       double temperature,
                                                       int numPredict,
                                                       double topP,
                                                       std::wstring* errorMsg) {
    ensureNetworkInitialized();

    std::string utf8model = wstring_to_utf8(model);
    std::string utf8prompt = wstring_to_utf8(prompt);
    std::string utf8system = wstring_to_utf8(systemPrompt);

    auto jsonEscape = [](const std::string& s) {
        std::string out;
        for (unsigned char c : s) {
            switch (c) {
                case '"': out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\b': out += "\\b"; break;
                case '\f': out += "\\f"; break;
                case '\n': out += "\\n"; break;
                case '\r': out += "\\r"; break;
                case '\t': out += "\\t"; break;
                default:
                    if (c < 0x20) {
                        char buf[8];
                        snprintf(buf, sizeof(buf), "\\u%04x", c);
                        out += buf;
                    } else {
                        out += c;
                    }
            }
        }
        return out;
    };

    std::string escapedModel = jsonEscape(utf8model);
    std::string escapedPrompt = jsonEscape(utf8prompt);
    std::string escapedSystem = jsonEscape(utf8system);

    std::string jsonBody = "{\"model\":\"" + escapedModel +
                           "\",\"prompt\":\"" + escapedPrompt +
                           "\",\"stream\":false,\"think\":false,\"system\":\"" + escapedSystem +
                           "\",\"options\":{\"num_predict\":" + std::to_string(numPredict) +
                           ",\"temperature\":" + std::to_string(temperature) +
                           ",\"top_p\":" + std::to_string(topP) + "}}";

    if (imagesBase64 && !imagesBase64->empty()) {
        size_t pos = jsonBody.find("\"options\"");
        if (pos != std::string::npos) {
            std::string imagesJson = "\"images\":[";
            for (size_t i = 0; i < imagesBase64->size(); ++i) {
                if (i > 0) imagesJson += ",";
                imagesJson += "\"" + (*imagesBase64)[i] + "\"";
            }
            imagesJson += "],";
            jsonBody.insert(pos, imagesJson);
        }
    }

    for (int attempt = 0; attempt < 2; ++attempt) {
        std::string response = httpPostOllamaGenerate(jsonBody);
        if (response.empty()) {
            if (errorMsg) *errorMsg = L"Empty response from Ollama";
            std::this_thread::sleep_for(std::chrono::seconds(2));
            continue;
        }

        if (response.find("\"done_reason\":\"load\"") != std::string::npos) {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            continue;
        }

        std::wstring answer = extractResponse(response);
        if (!answer.empty()) {
            return answer;
        }

        size_t errPos = response.find("\"error\":\"");
        if (errPos != std::string::npos) {
            errPos += 10;
            std::string errMsg;
            bool esc = false;
            for (size_t i = errPos; i < response.length(); ++i) {
                char c = response[i];
                if (esc) { errMsg += c; esc = false; }
                else if (c == '\\') esc = true;
                else if (c == '"') break;
                else errMsg += c;
            }
            if (errorMsg) *errorMsg = L"Ollama error: " + utf8_to_wstring(errMsg);
            return L"";
        }
    }

    if (errorMsg) *errorMsg = L"Модель не ответила";
    return L"";
}

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
    return false;
}

std::wstring generateBlocking(const std::wstring& model,
                              const std::wstring& prompt,
                              const std::wstring& systemPrompt,
                              const std::wstring& generationMode,
                              std::wstring* errorMsg) {
    int numPredict = 256;
    double temperature = 0.6;
    double topP = 0.9;
    if (generationMode == L"instant") { numPredict = 128; temperature = 0.8; }
    else if (generationMode == L"normal") { numPredict = 256; temperature = 0.6; }
    else if (generationMode == L"thinking") { numPredict = -1; temperature = 0.3; }
    return generateBlockingInternalWithParams(model, prompt, systemPrompt, nullptr,
                                              temperature, numPredict, topP, errorMsg);
}

std::wstring generateBlockingWithImages(const std::wstring& model,
                                        const std::wstring& prompt,
                                        const std::wstring& systemPrompt,
                                        const std::vector<std::string>& imagesBase64,
                                        const std::wstring& generationMode,
                                        std::wstring* errorMsg) {
    int numPredict = 256;
    double temperature = 0.6;
    double topP = 0.9;
    if (generationMode == L"instant") { numPredict = 128; temperature = 0.8; }
    else if (generationMode == L"normal") { numPredict = 256; temperature = 0.6; }
    else if (generationMode == L"thinking") { numPredict = -1; temperature = 0.3; }
    return generateBlockingInternalWithParams(model, prompt, systemPrompt, &imagesBase64,
                                              temperature, numPredict, topP, errorMsg);
}

std::wstring generateBlockingWithParams(const std::wstring& model,
                                        const std::wstring& prompt,
                                        const std::wstring& systemPrompt,
                                        double temperature,
                                        int numPredict,
                                        double topP,
                                        std::wstring* errorMsg) {
    return generateBlockingInternalWithParams(model, prompt, systemPrompt, nullptr,
                                              temperature, numPredict, topP, errorMsg);
}

std::wstring generateBlockingWithImagesWithParams(const std::wstring& model,
                                                  const std::wstring& prompt,
                                                  const std::wstring& systemPrompt,
                                                  const std::vector<std::string>& imagesBase64,
                                                  double temperature,
                                                  int numPredict,
                                                  double topP,
                                                  std::wstring* errorMsg) {
    return generateBlockingInternalWithParams(model, prompt, systemPrompt, &imagesBase64,
                                              temperature, numPredict, topP, errorMsg);
}

bool generateStreamingOllama(const std::wstring& model,
                             const std::wstring& prompt,
                             const std::wstring& systemPrompt,
                             const std::wstring& generationMode,
                             std::function<void(const std::wstring&)> chunkCallback,
                             std::wstring* errorMsg) {
    int numPredict = 256;
    double temperature = 0.6;
    double topP = 0.9;
    if (generationMode == L"instant") { numPredict = 128; temperature = 0.8; }
    else if (generationMode == L"normal") { numPredict = 256; temperature = 0.6; }
    else if (generationMode == L"thinking") { numPredict = -1; temperature = 0.3; }
    return generateStreamingOllamaWithParams(model, prompt, systemPrompt,
                                             temperature, numPredict, topP,
                                             chunkCallback, errorMsg);
}

bool generateStreamingOllamaWithParams(const std::wstring& model,
                                       const std::wstring& prompt,
                                       const std::wstring& systemPrompt,
                                       double temperature,
                                       int numPredict,
                                       double topP,
                                       std::function<void(const std::wstring&)> chunkCallback,
                                       std::wstring* errorMsg) {
    ensureNetworkInitialized();

    std::string utf8model = wstring_to_utf8(model);
    std::string utf8prompt = wstring_to_utf8(prompt);
    std::string utf8system = wstring_to_utf8(systemPrompt);

    auto jsonEscape = [](const std::string& s) {
        std::string out;
        for (unsigned char c : s) {
            switch (c) {
                case '"': out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\b': out += "\\b"; break;
                case '\f': out += "\\f"; break;
                case '\n': out += "\\n"; break;
                case '\r': out += "\\r"; break;
                case '\t': out += "\\t"; break;
                default:
                    if (c < 0x20) {
                        char buf[8];
                        snprintf(buf, sizeof(buf), "\\u%04x", c);
                        out += buf;
                    } else {
                        out += c;
                    }
            }
        }
        return out;
    };

    std::string escapedModel = jsonEscape(utf8model);
    std::string escapedPrompt = jsonEscape(utf8prompt);
    std::string escapedSystem = jsonEscape(utf8system);

    std::string jsonBody = "{\"model\":\"" + escapedModel +
                           "\",\"prompt\":\"" + escapedPrompt +
                           "\",\"stream\":true,\"think\":false,\"system\":\"" + escapedSystem +
                           "\",\"options\":{\"num_predict\":" + std::to_string(numPredict) +
                           ",\"temperature\":" + std::to_string(temperature) +
                           ",\"top_p\":" + std::to_string(topP) + "}}";

    SOCKET sock = psocket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        if (errorMsg) *errorMsg = L"Failed to create socket";
        return false;
    }
    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = phtons(11434);
    addr.sin_addr.s_addr = pinet_addr(g_ollamaHost.c_str());
    if (pconnect(sock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        pclosesocket(sock);
        if (errorMsg) *errorMsg = L"Failed to connect to Ollama";
        return false;
    }

    std::string request = "POST /api/generate HTTP/1.1\r\n"
                          "Host: 127.0.0.1:11434\r\n"
                          "Content-Type: application/json\r\n"
                          "Content-Length: " + std::to_string(jsonBody.length()) + "\r\n"
                          "Connection: close\r\n\r\n" + jsonBody;

    if (psend(sock, request.c_str(), request.length(), 0) == SOCKET_ERROR) {
        pclosesocket(sock);
        if (errorMsg) *errorMsg = L"Failed to send request";
        return false;
    }

    std::string buffer;
    char recvBuf[16384]; // увеличенный буфер
    int received;
    bool headerParsed = false;
    std::string body;

    while ((received = precv(sock, recvBuf, sizeof(recvBuf), 0)) > 0) {
        buffer.append(recvBuf, received);
        if (!headerParsed) {
            size_t headerEnd = buffer.find("\r\n\r\n");
            if (headerEnd != std::string::npos) {
                body = buffer.substr(headerEnd + 4);
                buffer.clear();
                headerParsed = true;
            }
        } else {
            body.append(recvBuf, received);
        }

        size_t pos;
        while ((pos = body.find('\n')) != std::string::npos) {
            std::string line = body.substr(0, pos);
            body.erase(0, pos + 1);
            if (line.empty()) continue;
            std::wstring chunk = extractResponse(line);
            if (!chunk.empty()) {
                chunkCallback(chunk);
            }
        }
    }

    pclosesocket(sock);
    return true;
}

bool generateWarmup(const std::wstring& model, const std::wstring& systemPrompt, std::wstring* errorMsg) {
    ensureNetworkInitialized();

    std::string utf8model = wstring_to_utf8(model);
    std::string utf8system = wstring_to_utf8(systemPrompt);
    auto jsonEscape = [](const std::string& s) {
        std::string out;
        for (unsigned char c : s) {
            switch (c) {
                case '"': out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\n': out += "\\n"; break;
                case '\r': out += "\\r"; break;
                case '\t': out += "\\t"; break;
                default: out += c;
            }
        }
        return out;
    };

    std::string escapedModel = jsonEscape(utf8model);
    std::string escapedSystem = jsonEscape(utf8system);
    std::string jsonBody = "{\"model\":\"" + escapedModel +
                           "\",\"prompt\":\"\",\"stream\":false,\"think\":false,\"system\":\"" + escapedSystem +
                           "\",\"options\":{\"num_predict\":1,\"temperature\":0.0,\"top_p\":1.0}}";

    std::string response = httpPostOllamaGenerate(jsonBody);
    if (response.empty()) {
        if (errorMsg) *errorMsg = L"Empty response";
        return false;
    }
    return true;
}

std::wstring searchWeb(const std::wstring& query) { return L""; }
std::wstring fetchWeather(const std::wstring& city) { return L""; }