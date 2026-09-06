#pragma once
#include <string>
#include <vector>

std::vector<std::wstring> getAvailableModels();
bool pullModel(const std::wstring& modelName);
std::wstring generateBlocking(const std::wstring& model,
                              const std::wstring& prompt,
                              const std::wstring& systemPrompt,
                              const std::wstring& generationMode,
                              std::wstring* errorMsg);
std::wstring searchWeb(const std::wstring& query);
std::wstring fetchWeather(const std::wstring& city);