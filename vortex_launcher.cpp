#include <windows.h>
#include <shellapi.h>
#include <string>

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    // Получаем путь к текущему исполняемому файлу (лаунчеру)
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(NULL, path, MAX_PATH);

    std::wstring dir(path);
    size_t pos = dir.find_last_of(L"\\/");
    if (pos != std::wstring::npos) {
        dir = dir.substr(0, pos);
    } else {
        dir = L".";
    }

    // Путь к автономному exe, созданному PyInstaller
    std::wstring appPath = dir + L"\\dist\\vortex.exe";

    if (GetFileAttributesW(appPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        MessageBoxW(NULL,
                    L"vortex.exe not found in dist folder.\n\n"
                    L"Please run PyInstaller build first.",
                    L"Vortex Launcher",
                    MB_ICONERROR);
        return 1;
    }

    HINSTANCE result = ShellExecuteW(NULL, L"open", appPath.c_str(), NULL, dir.c_str(), SW_SHOW);
    if ((INT_PTR)result <= 32) {
        MessageBoxW(NULL, L"Failed to start vortex.exe.", L"Vortex Launcher", MB_ICONERROR);
        return 1;
    }
    return 0;
}