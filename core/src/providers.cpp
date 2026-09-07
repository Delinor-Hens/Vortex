// core/src/providers.cpp
#include "../include/providers.h"
#include <string>
#include <vector>
#include <mutex>
#include <cstring>
#include <sstream>
#include <stdexcept>
#include <algorithm>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <codecvt>
    #include <locale>
#endif

#include <curl/curl.h>

#pragma comment(lib, "libcurl.lib") // для MSVC, для mingw не нужно, используется -lcurl

// ==================== Конвертация UTF-8 <-> wide string ====================
#ifdef _WIN32
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
#else
static std::wstring string_to_wstring(const std::string& str) {
    std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
    return conv.from_bytes(str);
}

static std::string wstring_to_string(const std::wstring& wstr) {
    std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
    return conv.to_bytes(wstr);
}
#endif

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

// ==================== Отправка HTTP-запроса через libcurl ====================
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

static std::string httpPost(const std::string& url,
                            const std::string& body,
                            const std::vector<std::string>& headers) {
    CURL* curl = curl_easy_init();
    if (!curl) return "";

    std::string response;
    struct curl_slist* headerList = nullptr;
    for (const auto& h : headers) {
        headerList = curl_slist_append(headerList, h.c_str());
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)body.size());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerList);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L); // временно отключаем проверку сертификата
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headerList);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) return "";
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

    std::string baseUrl = wstring_to_string(config.baseUrl);
    std::string apiKey = wstring_to_string(config.apiKey);
    std::string fullUrl = baseUrl + "/chat/completions";

    std::vector<std::string> headers;
    headers.push_back("Content-Type: application/json");
    headers.push_back("Authorization: Bearer " + apiKey);

    std::string response = httpPost(fullUrl, jsonBody, headers);
    if (response.empty()) {
        if (errorMsg) *errorMsg = L"Пустой ответ от провайдера";
        return L"";
    }

    std::string content = extractJsonString(response, "content");
    if (content.empty()) {
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

    std::string baseUrl = wstring_to_string(config.baseUrl);
    std::string apiKey = wstring_to_string(config.apiKey);
    std::string fullUrl = baseUrl + "/messages";

    std::vector<std::string> headers;
    headers.push_back("Content-Type: application/json");
    headers.push_back("x-api-key: " + apiKey);
    headers.push_back("anthropic-version: 2023-06-01");

    std::string response = httpPost(fullUrl, jsonBody, headers);
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

    std::string baseUrl = wstring_to_string(config.baseUrl);
    std::string apiKey = wstring_to_string(config.apiKey);
    std::string fullUrl = baseUrl + "/models/" + model + ":generateContent?key=" + apiKey;

    std::vector<std::string> headers;
    headers.push_back("Content-Type: application/json");

    std::string response = httpPost(fullUrl, jsonBody, headers);
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