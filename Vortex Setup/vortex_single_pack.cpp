// vortex_single_pack.cpp
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <cstring>

namespace fs = std::filesystem;

extern "C" {
    extern unsigned char _binary_vortex_setup_start[];
    extern unsigned char _binary_vortex_setup_end[];
    extern unsigned char _binary_data0_start[];
    extern unsigned char _binary_data0_end[];
    extern unsigned char _binary_data1_start[];
    extern unsigned char _binary_data1_end[];
}

// Логирование в файл рядом с exe
void Log(const std::wstring& msg) {
    std::wofstream log(L"setup_log.txt", std::ios::app);
    if (log) log << msg << L"\n";
}

bool WriteDataToFile(const std::wstring& path, unsigned char* start, unsigned char* end, size_t& writtenSize) {
    size_t size = end - start;
    writtenSize = 0;
    std::ofstream out(path.c_str(), std::ios::binary);
    if (!out) {
        Log(L"Ошибка: не удалось открыть файл для записи: " + path);
        return false;
    }
    out.write(reinterpret_cast<const char*>(start), size);
    out.close();
    writtenSize = size;
    return true;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR lpCmdLine, int nCmdShow) {
    Log(L"=== Запуск Setup.exe ===");

    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    fs::path exeDir = fs::path(exePath).parent_path();
    Log(L"Текущая папка: " + exeDir.wstring());

    // Создание временной папки
    wchar_t tempDir[MAX_PATH];
    GetTempPathW(MAX_PATH, tempDir);
    std::wstring workDir = std::wstring(tempDir) + L"VortexSetup_" + std::to_wstring(GetTickCount());
    CreateDirectoryW(workDir.c_str(), NULL);
    Log(L"Временная папка: " + workDir);

    std::wstring setupPath = workDir + L"\\vortex_setup.exe";
    std::wstring data0Path = workDir + L"\\data0";
    std::wstring data1Path = workDir + L"\\data1";

    size_t sizeSetup, sizeData0, sizeData1;

    Log(L"Извлечение vortex_setup.exe...");
    if (!WriteDataToFile(setupPath, _binary_vortex_setup_start, _binary_vortex_setup_end, sizeSetup)) {
        Log(L"Ошибка извлечения vortex_setup.exe");
        fs::remove_all(workDir);
        MessageBoxW(NULL, L"Не удалось извлечь встроенные файлы.", L"Ошибка", MB_ICONERROR);
        return 1;
    }
    Log(L"vortex_setup.exe извлечён, размер: " + std::to_wstring(sizeSetup));

    Log(L"Извлечение data0...");
    if (!WriteDataToFile(data0Path, _binary_data0_start, _binary_data0_end, sizeData0)) {
        Log(L"Ошибка извлечения data0");
        fs::remove_all(workDir);
        MessageBoxW(NULL, L"Не удалось извлечь встроенные файлы.", L"Ошибка", MB_ICONERROR);
        return 1;
    }
    Log(L"data0 извлечён, размер: " + std::to_wstring(sizeData0));

    Log(L"Извлечение data1...");
    if (!WriteDataToFile(data1Path, _binary_data1_start, _binary_data1_end, sizeData1)) {
        Log(L"Ошибка извлечения data1");
        fs::remove_all(workDir);
        MessageBoxW(NULL, L"Не удалось извлечь встроенные файлы.", L"Ошибка", MB_ICONERROR);
        return 1;
    }
    Log(L"data1 извлечён, размер: " + std::to_wstring(sizeData1));

    // Запуск установщика
    Log(L"Запуск установщика...");
    SHELLEXECUTEINFOW sei = { sizeof(sei) };
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.lpVerb = L"runas";
    sei.lpFile = setupPath.c_str();
    sei.lpParameters = L"";
    sei.nShow = SW_SHOWNORMAL;

    if (!ShellExecuteExW(&sei)) {
        Log(L"runas не удался, пробуем open");
        sei.lpVerb = L"open";
        if (!ShellExecuteExW(&sei)) {
            Log(L"Не удалось запустить установщик");
            fs::remove_all(workDir);
            MessageBoxW(NULL, L"Не удалось запустить установщик.", L"Ошибка", MB_ICONERROR);
            return 1;
        }
    }

    if (sei.hProcess) {
        WaitForSingleObject(sei.hProcess, INFINITE);
        CloseHandle(sei.hProcess);
    }

    Log(L"Установщик завершил работу, удаляю временные файлы");
    fs::remove_all(workDir);
    Log(L"=== Завершение Setup.exe ===");

    return 0;
}