#pragma once
#include <string>
#include <vector>
#include <functional>

std::vector<std::wstring> getAvailableModels();
bool pullModel(const std::wstring& modelName);

std::wstring generateBlocking(const std::wstring& model,
                              const std::wstring& prompt,
                              const std::wstring& systemPrompt,
                              const std::wstring& generationMode,
                              std::wstring* errorMsg);

std::wstring generateBlockingWithImages(const std::wstring& model,
                                        const std::wstring& prompt,
                                        const std::wstring& systemPrompt,
                                        const std::vector<std::string>& imagesBase64,
                                        const std::wstring& generationMode,
                                        std::wstring* errorMsg);

std::wstring generateBlockingWithParams(const std::wstring& model,
                                        const std::wstring& prompt,
                                        const std::wstring& systemPrompt,
                                        double temperature,
                                        int numPredict,
                                        double topP,
                                        std::wstring* errorMsg);

std::wstring generateBlockingWithImagesWithParams(const std::wstring& model,
                                                  const std::wstring& prompt,
                                                  const std::wstring& systemPrompt,
                                                  const std::vector<std::string>& imagesBase64,
                                                  double temperature,
                                                  int numPredict,
                                                  double topP,
                                                  std::wstring* errorMsg);

bool generateStreamingOllama(const std::wstring& model,
                             const std::wstring& prompt,
                             const std::wstring& systemPrompt,
                             const std::wstring& generationMode,
                             std::function<void(const std::wstring&)> chunkCallback,
                             std::wstring* errorMsg);

bool generateStreamingOllamaWithParams(const std::wstring& model,
                                       const std::wstring& prompt,
                                       const std::wstring& systemPrompt,
                                       double temperature,
                                       int numPredict,
                                       double topP,
                                       std::function<void(const std::wstring&)> chunkCallback,
                                       std::wstring* errorMsg);

bool generateWarmup(const std::wstring& model,
                    const std::wstring& systemPrompt,
                    std::wstring* errorMsg);

std::wstring searchWeb(const std::wstring& query);
std::wstring fetchWeather(const std::wstring& city);