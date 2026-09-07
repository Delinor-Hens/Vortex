// build_gui.cpp
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <string>
#include <filesystem>

namespace fs = std::filesystem;

void RunBuild(const char* target, bool noArchive);
void ShowLog();

// Глобальная переменная для чекбокса
HWND hNoArchiveCheck = nullptr;

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            CreateWindowW(L"BUTTON", L"Windows",
                          WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                          20, 20, 150, 40, hWnd, (HMENU)1, NULL, NULL);
            CreateWindowW(L"BUTTON", L"Linux",
                          WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                          20, 70, 150, 40, hWnd, (HMENU)2, NULL, NULL);
            CreateWindowW(L"BUTTON", L"macOS",
                          WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                          20, 120, 150, 40, hWnd, (HMENU)3, NULL, NULL);
            CreateWindowW(L"BUTTON", L"Android",
                          WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                          20, 170, 150, 40, hWnd, (HMENU)4, NULL, NULL);

            hNoArchiveCheck = CreateWindowW(L"BUTTON", L"Без архива (портативная)",
                                            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                                            20, 220, 200, 25, hWnd, NULL, NULL, NULL);

            CreateWindowW(L"BUTTON", L"Открыть лог",
                          WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                          20, 250, 150, 30, hWnd, (HMENU)5, NULL, NULL);
            break;
        }
        case WM_COMMAND: {
            int id = LOWORD(wParam);
            bool noArchive = (SendMessageW(hNoArchiveCheck, BM_GETCHECK, 0, 0) == BST_CHECKED);
            switch (id) {
                case 1: RunBuild("windows", noArchive); break;
                case 2: RunBuild("linux", false); break;
                case 3: RunBuild("macos", false); break;
                case 4: MessageBoxW(hWnd, L"Сборка под Android пока не поддерживается.",
                                    L"Информация", MB_ICONINFORMATION); break;
                case 5: ShowLog(); break;
            }
            break;
        }
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
    return 0;
}

void ShowLog() {
    if (fs::exists("build.log")) {
        ShellExecuteW(NULL, L"open", L"build.log", NULL, NULL, SW_SHOWNORMAL);
    } else {
        MessageBoxW(NULL, L"Лог-файл build.log не найден.", L"Информация", MB_ICONINFORMATION);
    }
}

void RunBuild(const char* target, bool noArchive) {
    std::string cmdLine = "build_tool.exe --target " + std::string(target);
    if (noArchive) cmdLine += " --no-archive";

    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
    HANDLE hLogFile = CreateFileA("build.log", GENERIC_WRITE, FILE_SHARE_READ, &sa,
                                  CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hLogFile == INVALID_HANDLE_VALUE) {
        MessageBoxW(NULL, L"Не удалось создать лог-файл build.log", L"Ошибка", MB_ICONERROR);
        return;
    }

    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi = { 0 };
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = hLogFile;
    si.hStdError = hLogFile;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

    if (!CreateProcessA(NULL, const_cast<char*>(cmdLine.c_str()), NULL, NULL, TRUE,
                        CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        CloseHandle(hLogFile);
        MessageBoxW(NULL, L"Не удалось запустить build_tool.exe", L"Ошибка", MB_ICONERROR);
        return;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hLogFile);

    if (exitCode == 0) {
        MessageBoxW(NULL, L"Сборка успешно завершена. Готовые файлы находятся в E:\\Vortex\\Vortex Setup",
                    L"Успех", MB_OK | MB_ICONINFORMATION);
    } else {
        MessageBoxW(NULL, L"Сборка завершилась с ошибкой. Открываю лог.",
                    L"Ошибка", MB_OK | MB_ICONERROR);
        ShellExecuteW(NULL, L"open", L"build.log", NULL, NULL, SW_SHOWNORMAL);
    }
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR lpCmdLine, int nCmdShow) {
    WNDCLASSW wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"BuildToolGUI";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassW(&wc);

    HWND hWnd = CreateWindowW(L"BuildToolGUI", L"Vortex Build Tool",
                              WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                              CW_USEDEFAULT, CW_USEDEFAULT, 250, 310,
                              NULL, NULL, hInstance, NULL);
    if (!hWnd) return 0;

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}