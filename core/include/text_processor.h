#pragma once

#include <string>
#include <vector>

std::wstring cleanText(const std::wstring& input);
std::vector<std::wstring> splitIntoChunks(const std::wstring& text, size_t maxChunkSize);
std::wstring joinChunks(const std::vector<std::wstring>& chunks);