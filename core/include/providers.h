#pragma once
#include <string>
#include <vector>
#include "memory_store.h" // для ProviderType и ProviderConfig

// Отправляет запрос к выбранному провайдеру и возвращает ответ
// prompt – текст пользователя (уже сформированный контекст)
// systemPrompt – системная инструкция
// errorMsg – сюда записывается сообщение об ошибке, если ответ пуст
std::wstring callProvider(const ProviderConfig& config,
                          const std::wstring& prompt,
                          const std::wstring& systemPrompt,
                          std::wstring* errorMsg);