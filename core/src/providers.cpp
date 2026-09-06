// core/src/providers.cpp
#include "../include/providers.h"
#include "../include/llm_engine.h" // для конвертаций utf8/wstring (продублируем)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>
#include <string>
#include <vector>
#include <mutex>
#include <cstring>
#include <sstream>
#include <stdexcept>
#include <algorithm>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "kernel32")
#pragma comment(lib, "user32")

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

static std::wstring string_to_wstring(const std::string& str) {
    if (str.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    if (len <= 0) return L"";
    std::wstring result(len - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &result[0], len);
    return result;
}

static std::string wstring_to_string(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (len <= 0) return "";
    std::string result(len - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &result[0], len, nullptr, nullptr);
    return result;
}

// ==================== Экранирование JSON ====================
static std::string jsonEscape(const std::string& s) {
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
}

// ==================== Разбор URL ====================
struct URLParts {
    std::wstring host;
    unsigned short port;
    std::wstring path;
    bool isHttps;
};

static bool parseUrl(const std::wstring& url, URLParts& parts) {
    std::wstring u = url;
    // Убираем пробелы
    u.erase(std::remove_if(u.begin(), u.end(), ::isspace), u.end());

    if (u.substr(0, 8) == L"https://") {
        parts.isHttps = true;
        u = u.substr(8);
    } else if (u.substr(0, 7) == L"http://") {
        parts.isHttps = false;
        u = u.substr(7);
    } else {
        // по умолчанию https
        parts.isHttps = true;
    }

    size_t slash = u.find(L'/');
    if (slash == std::wstring::npos) {
        parts.host = u;
        parts.path = L"/";
    } else {
        parts.host = u.substr(0, slash);
        parts.path = u.substr(slash);
    }

    parts.port = parts.isHttps ? 443 : 80;
    size_t colon = parts.host.find(L':');
    if (colon != std::wstring::npos) {
        parts.port = static_cast<unsigned short>(_wtoi(parts.host.substr(colon + 1).c_str()));
        parts.host = parts.host.substr(0, colon);
    }

    return !parts.host.empty();
}

// ==================== Отправка HTTPS-запроса через WinHTTP ====================
static std::string winHttpPost(const std::wstring& url,
                               const std::string& body,
                               const std::vector<std::wstring>& headers) {
    URLParts parts;
    if (!parseUrl(url, parts)) return "";

    HINTERNET hSession = WinHttpOpen(L"Vortex/2.1.12",
                                     WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     WINHTTP_NO_PROXY_NAME,
                                     WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return "";

    HINTERNET hConnect = WinHttpConnect(hSession, parts.host.c_str(), parts.port, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return "";
    }

    DWORD flags = (parts.isHttps) ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", parts.path.c_str(),
                                            NULL, WINHTTP_NO_REFERER,
                                            WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "";
    }

    // Добавляем заголовки
    for (const auto& h : headers) {
        WinHttpAddRequestHeaders(hRequest, h.c_str(), -1, WINHTTP_ADDREQ_FLAG_ADD);
    }

    // Отправляем запрос
    BOOL result = WinHttpSendRequest(hRequest,
                                     WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                     (LPVOID)body.c_str(), (DWORD)body.size(),
                                     (DWORD)body.size(), 0);
    if (!result) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "";
    }

    // Получаем ответ
    result = WinHttpReceiveResponse(hRequest, NULL);
    if (!result) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "";
    }

    std::string response;
    DWORD bytesRead = 0;
    char buffer[4096];
    do {
        if (!WinHttpReadData(hRequest, buffer, sizeof(buffer), &bytesRead)) break;
        if (bytesRead > 0) {
            response.append(buffer, bytesRead);
        }
    } while (bytesRead > 0);

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    return response;
}

// ==================== Извлечение полей из JSON ====================
static std::string extractJsonString(const std::string& json, const std::string& key) {
    std::string searchKey = "\"" + key + "\":\"";
    size_t pos = json.find(searchKey);
    if (pos == std::string::npos) return "";
    pos += searchKey.length();
    std::string result;
    bool escaped = false;
    for (size_t i = pos; i < json.length(); ++i) {
        char c = json[i];
        if (escaped) {
            switch (c) {
                case 'n': result += '\n'; break;
                case 't': result += '\t'; break;
                case 'r': result += '\r'; break;
                case '\\': result += '\\'; break;
                case '"': result += '"'; break;
                default: result += c;
            }
            escaped = false;
        } else {
            if (c == '\\') escaped = true;
            else if (c == '"') break;
            else result += c;
        }
    }
    return result;
}

// ==================== Отправка к OpenAI-совместимым провайдерам ====================
static std::wstring sendOpenAICompatible(const ProviderConfig& config,
                                         const std::wstring& prompt,
                                         const std::wstring& systemPrompt,
                                         std::wstring* errorMsg) {
    std::string model = wstring_to_string(config.model);
    std::string sys = wstring_to_string(systemPrompt);
    std::string user = wstring_to_string(prompt);

    std::string jsonBody = "{\"model\":\"" + jsonEscape(model) +
                           "\",\"messages\":[{\"role\":\"system\",\"content\":\"" + jsonEscape(sys) +
                           "\"},{\"role\":\"user\",\"content\":\"" + jsonEscape(user) +
                           "\"}],\"stream\":false}";

    std::wstring fullUrl = config.baseUrl + L"/chat/completions";
    std::vector<std::wstring> headers;
    headers.push_back(L"Content-Type: application/json");
    headers.push_back(L"Authorization: Bearer " + config.apiKey);

    std::string response = winHttpPost(fullUrl, jsonBody, headers);
    if (response.empty()) {
        if (errorMsg) *errorMsg = L"Пустой ответ от провайдера";
        return L"";
    }

    // Извлекаем content
    std::string content = extractJsonString(response, "content");
    if (content.empty()) {
        // Возможно, ошибка
        std::string error = extractJsonString(response, "message");
        if (error.empty()) error = extractJsonString(response, "error");
        if (errorMsg) *errorMsg = L"Ошибка провайдера: " + string_to_wstring(error);
        return L"";
    }

    return string_to_wstring(content);
}

// ==================== Отправка к Anthropic ====================
static std::wstring sendAnthropic(const ProviderConfig& config,
                                  const std::wstring& prompt,
                                  const std::wstring& systemPrompt,
                                  std::wstring* errorMsg) {
    std::string model = wstring_to_string(config.model);
    std::string sys = wstring_to_string(systemPrompt);
    std::string user = wstring_to_string(prompt);

    std::string jsonBody = "{\"model\":\"" + jsonEscape(model) +
                           "\",\"system\":\"" + jsonEscape(sys) +
                           "\",\"messages\":[{\"role\":\"user\",\"content\":\"" + jsonEscape(user) +
                           "\"}],\"max_tokens\":1024}";

    std::wstring fullUrl = config.baseUrl + L"/messages";
    std::vector<std::wstring> headers;
    headers.push_back(L"Content-Type: application/json");
    headers.push_back(L"x-api-key: " + config.apiKey);
    headers.push_back(L"anthropic-version: 2023-06-01");

    std::string response = winHttpPost(fullUrl, jsonBody, headers);
    if (response.empty()) {
        if (errorMsg) *errorMsg = L"Пустой ответ от Anthropic";
        return L"";
    }

    std::string content = extractJsonString(response, "text");
    if (content.empty()) {
        std::string error = extractJsonString(response, "message");
        if (errorMsg) *errorMsg = L"Ошибка Anthropic: " + string_to_wstring(error);
        return L"";
    }

    return string_to_wstring(content);
}

// ==================== Отправка к Google Gemini ====================
static std::wstring sendGemini(const ProviderConfig& config,
                               const std::wstring& prompt,
                               const std::wstring& systemPrompt,
                               std::wstring* errorMsg) {
    std::string model = wstring_to_string(config.model);
    std::string user = wstring_to_string(prompt);

    std::string jsonBody = "{\"contents\":[{\"parts\":[{\"text\":\"" + jsonEscape(user) +
                           "\"}]}]}";

    std::wstring fullUrl = config.baseUrl + L"/models/" + config.model + L":generateContent?key=" + config.apiKey;
    std::vector<std::wstring> headers;
    headers.push_back(L"Content-Type: application/json");

    std::string response = winHttpPost(fullUrl, jsonBody, headers);
    if (response.empty()) {
        if (errorMsg) *errorMsg = L"Пустой ответ от Gemini";
        return L"";
    }

    std::string text = extractJsonString(response, "text");
    if (text.empty()) {
        std::string error = extractJsonString(response, "message");
        if (errorMsg) *errorMsg = L"Ошибка Gemini: " + string_to_wstring(error);
        return L"";
    }

    return string_to_wstring(text);
}

// ==================== Диспетчер провайдеров ====================
std::wstring callProvider(const ProviderConfig& config,
                          const std::wstring& prompt,
                          const std::wstring& systemPrompt,
                          std::wstring* errorMsg) {
    switch (config.type) {
        case ProviderType::OpenAI:
        case ProviderType::Mistral:
        case ProviderType::DeepSeek:
        case ProviderType::OpenRouter:
        case ProviderType::Groq:
        case ProviderType::Together:
            return sendOpenAICompatible(config, prompt, systemPrompt, errorMsg);
        case ProviderType::Anthropic:
            return sendAnthropic(config, prompt, systemPrompt, errorMsg);
        case ProviderType::Gemini:
            return sendGemini(config, prompt, systemPrompt, errorMsg);
        default:
            if (errorMsg) *errorMsg = L"Неподдерживаемый тип провайдера";
            return L"";
    }
}