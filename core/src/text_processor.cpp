#include "../include/text_processor.h"
#include <algorithm>
#include <cctype>

std::wstring cleanText(const std::wstring& input) {
    std::wstring result;
    result.reserve(input.size());
    bool prevSpace = false;
    for (wchar_t ch : input) {
        if (ch == L'\r' || ch == L'\n') {
            if (!result.empty() && result.back() != L'\n')
                result += L'\n';
            prevSpace = false;
        }
        else if (std::isspace(ch)) {
            if (!prevSpace && !result.empty())
                result += L' ';
            prevSpace = true;
        }
        else {
            result += ch;
            prevSpace = false;
        }
    }
    // Обрезать пробелы и переводы строк в начале и конце
    size_t start = 0;
    while (start < result.size() && std::isspace(result[start])) ++start;
    size_t end = result.size();
    while (end > start && std::isspace(result[end-1])) --end;
    return result.substr(start, end - start);
}

std::vector<std::wstring> splitIntoChunks(const std::wstring& text, size_t maxChunkSize) {
    std::vector<std::wstring> chunks;
    if (text.empty()) return chunks;
    if (text.size() <= maxChunkSize) {
        chunks.push_back(text);
        return chunks;
    }
    size_t start = 0;
    while (start < text.size()) {
        size_t end = std::min(start + maxChunkSize, text.size());
        // Попытаться разбить по пробелу или новой строке для читаемости
        if (end < text.size()) {
            size_t lastSpace = text.find_last_of(L" \n", end);
            if (lastSpace != std::wstring::npos && lastSpace > start) {
                end = lastSpace;
            }
        }
        chunks.push_back(text.substr(start, end - start));
        start = end;
        // Пропустить пробел/новую строку в начале следующего куска
        while (start < text.size() && (text[start] == L' ' || text[start] == L'\n')) ++start;
    }
    return chunks;
}

std::wstring joinChunks(const std::vector<std::wstring>& chunks) {
    std::wstring result;
    for (const auto& chunk : chunks) {
        if (!result.empty())
            result += L"\n";
        result += chunk;
    }
    return result;
}