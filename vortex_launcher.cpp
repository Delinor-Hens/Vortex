#include <windows.h>
#include <shellapi.h>
#include <string>

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    std::string dir(path);
    size_t pos = dir.find_last_of("\\/");
    dir = (pos != std::string::npos) ? dir.substr(0, pos) : ".";

    // Путь к автономному exe, созданному PyInstaller
    std::string appPath = dir + "\\dist\\vortex.exe";

    if (GetFileAttributesA(appPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        MessageBoxA(NULL,
                    "vortex.exe not found in dist folder.\n\n"
                    "Please run PyInstaller build first.",
                    "Vortex Launcher",
                    MB_ICONERROR);
        return 1;
    }

    HINSTANCE result = ShellExecuteA(NULL, "open", appPath.c_str(), NULL, dir.c_str(), SW_SHOW);
    if ((INT_PTR)result <= 32) {
        MessageBoxA(NULL, "Failed to start vortex.exe.", "Vortex Launcher", MB_ICONERROR);
        return 1;
    }
    return 0;
}