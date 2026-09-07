// vortex_setup.cpp
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <shldisp.h>
#include <string>
#include <vector>
#include <filesystem>
#include <algorithm>
#include <thread>
#include <cwchar>
#include <fstream>
#include <sstream>
#include <cstdint>
#include <urlmon.h>
#include <winioctl.h>

#pragma comment(lib, "urlmon.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")

namespace fs = std::filesystem;

// ====================== КОНСТАНТЫ ======================
#define DATA0_FILE L"data0"
#define DATA1_FILE L"data1"
#define VORTEX_FOLDER L"Vortex"
#define MAX_PART_SIZE (100ULL * 1024ULL * 1024ULL)

// ====================== ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ ======================
HWND hMainWnd;
HWND hPageWelcome, hPageOptions, hPageInstall, hPageFinish;
HWND hBtnNext, hBtnBack, hBtnInstall, hBtnCancel, hBtnFinish;
HWND hEditPath, hBtnBrowse;
HWND hChkCreateDesktopShortcut;
HWND hStaticStatus;
HWND hProgressBar;
HWND hStaticProgressText;

std::wstring installPath;
bool createDesktopShortcut = true;
bool vortexFound = false;
bool ollamaFound = false;
bool ollamaInstallationComplete = false;
bool ollamaModelFailed = false;

enum Page { PAGE_WELCOME, PAGE_OPTIONS, PAGE_INSTALL, PAGE_FINISH };
Page currentPage = PAGE_WELCOME;

// Прототипы функций
void ShowPage(Page page);
void UpdateButtons();
void OnNext();
void OnBack();
void OnInstall();
void OnCancel();
void OnFinish();
void BrowseFolder();
void UpdateStatusText();
void PerformInstall();
void PerformUninstall();
bool IsVortexInstalled();
bool IsOllamaInstalled();
void DeleteFolder(const fs::path& folder);
std::wstring GetDefaultInstallPath();
std::wstring FindVortexExe(const std::wstring& rootFolder);
void CreateShortcut(const std::wstring& target, const std::wstring& shortcut, const std::wstring& iconPath = L"");
void RemoveShortcut(const std::wstring& shortcut);
void WriteUninstallRegistry(const std::wstring& installPath);
void RemoveRegistryKeys();
void SetProgressText(const std::wstring& text);
void SetProgressMarquee(bool active);
void SetProgressValue(int percent);
DWORD WINAPI InstallOllamaThread(LPVOID lpParam);

bool PackVortexToDataFiles(const std::wstring& vortexFolder,
                           const std::wstring& data0Path,
                           const std::wstring& data1Path);
bool ExtractDataFilesTo(const std::wstring& data0Path,
                        const std::wstring& data1Path,
                        const std::wstring& destFolder);
std::wstring GetCurrentExeDir();
std::string WStringToUTF8(const std::wstring& wstr);
bool RunProcessAndWait(const std::wstring& cmdLine, DWORD timeoutMs = INFINITE);
std::wstring FindOllamaExecutable();
bool IsOllamaModelAvailable(const std::wstring& modelName);
bool WaitForOllamaServer(DWORD timeoutMs = 30000);
std::wstring utf8_to_wstring(const std::string& str);
void LogMessage(const std::wstring& msg);

bool IsSsdDrive(wchar_t driveLetter);
ULONGLONG GetFreeSpaceOnDrive(wchar_t driveLetter);
std::wstring FindBestModelDirectory();
void SetOllamaModelsEnvironment(const std::wstring& modelsPath);
void RemoveOllamaModelsEnvironment();
void RefreshEnvironmentPath();

// ====================== ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ ======================
std::wstring GetCurrentExeDir() {
    wchar_t buf[MAX_PATH];
    GetModuleFileNameW(NULL, buf, MAX_PATH);
    fs::path p(buf);
    return p.parent_path().wstring();
}

std::wstring GetDefaultInstallPath() {
    wchar_t localAppData[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, localAppData))) {
        return std::wstring(localAppData);
    }
    return L"C:\\";
}

std::wstring GetInstallPathFromRegistry() {
    HKEY hKey;
    wchar_t path[MAX_PATH] = {0};
    DWORD size = sizeof(path);
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Vortex", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        if (RegQueryValueExW(hKey, L"InstallPath", NULL, NULL, (LPBYTE)path, &size) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            return std::wstring(path);
        }
        RegCloseKey(hKey);
    }
    return L"";
}

bool FileExists(const std::wstring& fullPath) {
    return GetFileAttributesW(fullPath.c_str()) != INVALID_FILE_ATTRIBUTES;
}

std::wstring FindVortexExe(const std::wstring& rootFolder) {
    if (!fs::exists(rootFolder) || !fs::is_directory(rootFolder)) return L"";
    std::vector<std::wstring> exeNames = {
        L"vortex.exe", L"Vortex.exe", L"vortex_launcher.exe", L"Vortex_launcher.exe"
    };
    for (const auto& name : exeNames) {
        std::wstring fullPath = rootFolder + L"\\" + name;
        if (FileExists(fullPath)) return fullPath;
    }
    try {
        for (const auto& entry : fs::directory_iterator(rootFolder, fs::directory_options::skip_permission_denied)) {
            if (entry.is_directory()) {
                for (const auto& name : exeNames) {
                    std::wstring fullPath = entry.path().wstring() + L"\\" + name;
                    if (FileExists(fullPath)) return fullPath;
                }
            }
        }
    } catch (...) {}
    return L"";
}

bool IsVortexInstalled() {
    std::wstring regPath = GetInstallPathFromRegistry();
    if (!regPath.empty()) {
        if (!FindVortexExe(regPath).empty()) return true;
        if (!FindVortexExe(regPath + L"\\Vortex").empty()) return true;
    }
    wchar_t localAppData[MAX_PATH];
    SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, localAppData);
    std::vector<std::wstring> paths = {
        std::wstring(localAppData) + L"\\Vortex",
        std::wstring(localAppData) + L"\\Vortex\\Vortex",
        L"C:\\Program Files\\Vortex",
        L"C:\\Program Files\\Vortex\\Vortex",
        L"C:\\Vortex",
        L"C:\\Vortex\\Vortex"
    };
    for (const auto& p : paths) {
        if (!FindVortexExe(p).empty()) return true;
    }
    return false;
}

// Обновление переменной PATH в текущем процессе
void RefreshEnvironmentPath() {
    wchar_t sysPath[32767];
    wchar_t userPath[32767];
    DWORD sysLen = GetEnvironmentVariableW(L"PATH", sysPath, 32767);
    if (sysLen == 0) sysPath[0] = L'\0';

    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Environment", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD size = sizeof(userPath);
        if (RegQueryValueExW(hKey, L"Path", NULL, NULL, (LPBYTE)userPath, &size) == ERROR_SUCCESS) {
            std::wstring combined = std::wstring(sysPath) + L";" + std::wstring(userPath);
            SetEnvironmentVariableW(L"PATH", combined.c_str());
        } else {
            SetEnvironmentVariableW(L"PATH", sysPath);
        }
        RegCloseKey(hKey);
    }
}

std::wstring FindOllamaExecutable() {
    RefreshEnvironmentPath(); // обновляем PATH перед поиском

    wchar_t path[MAX_PATH];
    DWORD len = GetEnvironmentVariableW(L"PATH", path, MAX_PATH);
    if (len > 0 && len < MAX_PATH) {
        std::wstring pathEnv(path, len);
        size_t pos = 0;
        while ((pos = pathEnv.find(L';')) != std::wstring::npos) {
            std::wstring dir = pathEnv.substr(0, pos);
            pathEnv.erase(0, pos + 1);
            if (!dir.empty() && FileExists(dir + L"\\ollama.exe"))
                return dir + L"\\ollama.exe";
        }
    }

    std::vector<std::wstring> paths = {
        L"C:\\Program Files\\Ollama\\ollama.exe",
        L"C:\\Program Files (x86)\\Ollama\\ollama.exe",
        L"C:\\Users\\Public\\Ollama\\ollama.exe"
    };
    wchar_t expanded[MAX_PATH];
    for (const auto& p : paths) {
        ExpandEnvironmentStringsW(p.c_str(), expanded, MAX_PATH);
        if (FileExists(expanded)) return expanded;
    }

    wchar_t localAppData[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, localAppData))) {
        std::wstring localPath = std::wstring(localAppData) + L"\\Programs\\Ollama\\ollama.exe";
        if (FileExists(localPath)) return localPath;
        localPath = std::wstring(localAppData) + L"\\Ollama\\ollama.exe";
        if (FileExists(localPath)) return localPath;
        localPath = std::wstring(localAppData) + L"\\Microsoft\\WinGet\\Links\\ollama.exe";
        if (FileExists(localPath)) return localPath;
    }
    return L"";
}

bool IsOllamaInstalled() {
    return !FindOllamaExecutable().empty();
}

void DeleteFolder(const fs::path& folder) {
    if (fs::exists(folder)) fs::remove_all(folder);
}

void CreateShortcut(const std::wstring& target, const std::wstring& shortcut, const std::wstring& iconPath) {
    CoInitialize(NULL);
    IShellLinkW* pShellLink = nullptr;
    IPersistFile* pPersistFile = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
                                  IID_IShellLinkW, (void**)&pShellLink);
    if (SUCCEEDED(hr)) {
        pShellLink->SetPath(target.c_str());
        pShellLink->SetDescription(L"Vortex Chat");
        if (!iconPath.empty()) {
            pShellLink->SetIconLocation(iconPath.c_str(), 0);
        }
        pShellLink->QueryInterface(IID_IPersistFile, (void**)&pPersistFile);
        if (pPersistFile) {
            pPersistFile->Save(shortcut.c_str(), TRUE);
            pPersistFile->Release();
        }
        pShellLink->Release();
    }
    CoUninitialize();
}

void RemoveShortcut(const std::wstring& shortcut) {
    DeleteFileW(shortcut.c_str());
}

void WriteUninstallRegistry(const std::wstring& installPath) {
    HKEY hKey;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Vortex",
                        0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        std::wstring displayName = L"Vortex 2.1.12";
        std::wstring displayVersion = L"2.1.12";
        std::wstring publisher = L"Delinor";
        wchar_t exePath[MAX_PATH];
        GetModuleFileNameW(NULL, exePath, MAX_PATH);
        std::wstring uninstallString = L"\"" + std::wstring(exePath) + L"\" /uninstall";
        std::wstring installLocation = installPath;
        std::wstring displayIcon = installPath + L"\\logo.ico";
        RegSetValueExW(hKey, L"DisplayName", 0, REG_SZ, (const BYTE*)displayName.c_str(), (displayName.size()+1)*sizeof(wchar_t));
        RegSetValueExW(hKey, L"DisplayVersion", 0, REG_SZ, (const BYTE*)displayVersion.c_str(), (displayVersion.size()+1)*sizeof(wchar_t));
        RegSetValueExW(hKey, L"Publisher", 0, REG_SZ, (const BYTE*)publisher.c_str(), (publisher.size()+1)*sizeof(wchar_t));
        RegSetValueExW(hKey, L"InstallLocation", 0, REG_SZ, (const BYTE*)installLocation.c_str(), (installLocation.size()+1)*sizeof(wchar_t));
        RegSetValueExW(hKey, L"UninstallString", 0, REG_SZ, (const BYTE*)uninstallString.c_str(), (uninstallString.size()+1)*sizeof(wchar_t));
        RegSetValueExW(hKey, L"DisplayIcon", 0, REG_SZ, (const BYTE*)displayIcon.c_str(), (displayIcon.size()+1)*sizeof(wchar_t));
        DWORD noModify = 1, noRepair = 1;
        RegSetValueExW(hKey, L"NoModify", 0, REG_DWORD, (const BYTE*)&noModify, sizeof(noModify));
        RegSetValueExW(hKey, L"NoRepair", 0, REG_DWORD, (const BYTE*)&noRepair, sizeof(noRepair));
        RegCloseKey(hKey);
    }
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Vortex", 0, NULL, 0,
                        KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        RegSetValueExW(hKey, L"InstallPath", 0, REG_SZ, (const BYTE*)installPath.c_str(), (installPath.size()+1)*sizeof(wchar_t));
        RegCloseKey(hKey);
    }
}

void RemoveRegistryKeys() {
    RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Vortex");
    RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Vortex");
}

void SetProgressText(const std::wstring& text) {
    SetWindowTextW(hStaticProgressText, text.c_str());
}

void SetProgressMarquee(bool active) {
    if (active) {
        SendMessageW(hProgressBar, PBM_SETMARQUEE, TRUE, 30);
    } else {
        SendMessageW(hProgressBar, PBM_SETMARQUEE, FALSE, 0);
        SendMessageW(hProgressBar, PBM_SETPOS, 100, 0);
    }
}

void SetProgressValue(int percent) {
    SendMessageW(hProgressBar, PBM_SETPOS, percent, 0);
}

void LogMessage(const std::wstring& msg) {
    std::wofstream log(L"install.log", std::ios::app);
    if (log) log << msg << std::endl;
}

std::string WStringToUTF8(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string result(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &result[0], size_needed, NULL, NULL);
    return result;
}

std::wstring utf8_to_wstring(const std::string& str) {
    if (str.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    if (len <= 0) return L"";
    std::wstring result(len - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &result[0], len);
    return result;
}

bool RunProcessAndWait(const std::wstring& cmdLine, DWORD timeoutMs) {
    STARTUPINFOW si = {0};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

    PROCESS_INFORMATION pi = {0};
    if (!CreateProcessW(NULL, const_cast<wchar_t*>(cmdLine.c_str()), NULL, NULL, FALSE,
                        CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        return false;
    }
    DWORD waitResult = WaitForSingleObject(pi.hProcess, timeoutMs);
    if (waitResult != WAIT_OBJECT_0) {
        TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return false;
    }
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return exitCode == 0;
}

bool IsOllamaModelAvailable(const std::wstring& modelName) {
    std::wstring ollamaPath = FindOllamaExecutable();
    if (ollamaPath.empty()) return false;

    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
    HANDLE hReadPipe, hWritePipe;
    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) return false;

    STARTUPINFOW si = {0};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hWritePipe;
    si.hStdError = hWritePipe;

    std::wstring cmd = L"\"" + ollamaPath + L"\" list";
    PROCESS_INFORMATION pi = {0};
    if (!CreateProcessW(NULL, const_cast<wchar_t*>(cmd.c_str()), NULL, NULL, TRUE,
                        CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        CloseHandle(hReadPipe);
        CloseHandle(hWritePipe);
        return false;
    }

    CloseHandle(hWritePipe);
    std::string output;
    char buffer[4096];
    DWORD bytesRead;
    while (ReadFile(hReadPipe, buffer, sizeof(buffer), &bytesRead, NULL) && bytesRead > 0) {
        output.append(buffer, bytesRead);
    }
    CloseHandle(hReadPipe);
    WaitForSingleObject(pi.hProcess, 10000);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    std::wstring woutput = utf8_to_wstring(output);
    std::transform(woutput.begin(), woutput.end(), woutput.begin(), ::towlower);
    std::wstring lowerModel = modelName;
    std::transform(lowerModel.begin(), lowerModel.end(), lowerModel.begin(), ::towlower);
    return (woutput.find(lowerModel) != std::wstring::npos);
}

bool WaitForOllamaServer(DWORD timeoutMs) {
    DWORD start = GetTickCount();
    while (GetTickCount() - start < timeoutMs) {
        std::wstring ollamaPath = FindOllamaExecutable();
        if (!ollamaPath.empty()) {
            std::wstring cmd = L"\"" + ollamaPath + L"\" list";
            if (RunProcessAndWait(cmd, 3000)) return true;
        }
        Sleep(1000);
    }
    return false;
}

// ==================== ОПРЕДЕЛЕНИЕ ДИСКА ====================
bool IsSsdDrive(wchar_t driveLetter) {
    wchar_t path[8] = L"\\\\.\\X:";
    path[4] = driveLetter;

    HANDLE hDevice = CreateFileW(path, 0, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                 nullptr, OPEN_EXISTING, 0, nullptr);
    if (hDevice == INVALID_HANDLE_VALUE)
        return false;

    STORAGE_PROPERTY_QUERY query = {};
    query.PropertyId = StorageDeviceProperty;
    query.QueryType = PropertyStandardQuery;

    std::vector<BYTE> buffer(sizeof(STORAGE_DEVICE_DESCRIPTOR) + 512);
    STORAGE_DEVICE_DESCRIPTOR* desc = reinterpret_cast<STORAGE_DEVICE_DESCRIPTOR*>(buffer.data());
    desc->Size = sizeof(STORAGE_DEVICE_DESCRIPTOR);

    DWORD bytesReturned = 0;
    bool isSsd = false;
    if (DeviceIoControl(hDevice,
                        IOCTL_STORAGE_QUERY_PROPERTY,
                        &query,
                        sizeof(query),
                        desc,
                        buffer.size(),
                        &bytesReturned,
                        nullptr)) {
        if (desc->BusType == BusTypeSata || desc->BusType == BusTypeNvme) {
            isSsd = true;
        }
    }

    CloseHandle(hDevice);
    return isSsd;
}

ULONGLONG GetFreeSpaceOnDrive(wchar_t driveLetter) {
    wchar_t root[4] = L"X:\\";
    root[0] = driveLetter;
    ULARGE_INTEGER freeBytesAvailable;
    ULARGE_INTEGER totalBytes;
    ULARGE_INTEGER totalFreeBytes;
    if (!GetDiskFreeSpaceExW(root, &freeBytesAvailable, &totalBytes, &totalFreeBytes))
        return 0;
    return freeBytesAvailable.QuadPart;
}

std::wstring FindBestModelDirectory() {
    const DWORD driveMask = GetLogicalDrives();
    std::vector<std::pair<wchar_t, ULONGLONG>> ssdCandidates;
    std::vector<std::pair<wchar_t, ULONGLONG>> hddCandidates;

    for (wchar_t letter = L'A'; letter <= L'Z'; ++letter) {
        if (!(driveMask & (1 << (letter - L'A'))))
            continue;

        UINT driveType = GetDriveTypeW((std::wstring(1, letter) + L":\\").c_str());
        if (driveType != DRIVE_FIXED)
            continue;

        ULONGLONG freeSpace = GetFreeSpaceOnDrive(letter);
        if (IsSsdDrive(letter)) {
            ssdCandidates.push_back({letter, freeSpace});
        } else {
            hddCandidates.push_back({letter, freeSpace});
        }
    }

    auto sortByFreeDesc = [](const auto& a, const auto& b) {
        return a.second > b.second;
    };
    std::sort(ssdCandidates.begin(), ssdCandidates.end(), sortByFreeDesc);
    std::sort(hddCandidates.begin(), hddCandidates.end(), sortByFreeDesc);

    const ULONGLONG MIN_SSD_SPACE = 10ULL * 1024ULL * 1024ULL * 1024ULL; // 10 ГБ

    for (const auto& [letter, freeSpace] : ssdCandidates) {
        if (freeSpace >= MIN_SSD_SPACE) {
            return std::wstring(1, letter) + L":\\ollama_models";
        }
    }

    if (!hddCandidates.empty()) {
        return std::wstring(1, hddCandidates.front().first) + L":\\ollama_models";
    }

    return L"C:\\ollama_models";
}

void SetOllamaModelsEnvironment(const std::wstring& modelsPath) {
    CreateDirectoryW(modelsPath.c_str(), NULL);
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Environment", 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        RegSetValueExW(hKey, L"OLLAMA_MODELS", 0, REG_SZ,
                       (const BYTE*)modelsPath.c_str(),
                       (modelsPath.size() + 1) * sizeof(wchar_t));
        RegCloseKey(hKey);
    }
    SetEnvironmentVariableW(L"OLLAMA_MODELS", modelsPath.c_str());
    SendMessageTimeoutW(HWND_BROADCAST, WM_SETTINGCHANGE, 0,
                        (LPARAM)L"Environment", SMTO_ABORTIFHUNG, 5000, nullptr);
}

void RemoveOllamaModelsEnvironment() {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Environment", 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        RegDeleteValueW(hKey, L"OLLAMA_MODELS");
        RegCloseKey(hKey);
    }
    SetEnvironmentVariableW(L"OLLAMA_MODELS", nullptr);
    SendMessageTimeoutW(HWND_BROADCAST, WM_SETTINGCHANGE, 0,
                        (LPARAM)L"Environment", SMTO_ABORTIFHUNG, 5000, nullptr);
}

// ==================== УПАКОВКА / РАСПАКОВКА ====================
bool PackVortexToDataFiles(const std::wstring& vortexFolder,
                           const std::wstring& data0Path,
                           const std::wstring& data1Path) {
    try {
        std::vector<fs::path> files;
        for (const auto& entry : fs::recursive_directory_iterator(vortexFolder, fs::directory_options::skip_permission_denied)) {
            if (entry.is_regular_file())
                files.push_back(entry.path());
        }
        std::sort(files.begin(), files.end());

        std::ofstream out0(WStringToUTF8(data0Path), std::ios::binary | std::ios::trunc);
        std::ofstream out1(WStringToUTF8(data1Path), std::ios::binary | std::ios::trunc);
        if (!out0 || !out1) {
            out0.close();
            out1.close();
            DeleteFileW(data0Path.c_str());
            DeleteFileW(data1Path.c_str());
            MessageBoxW(NULL, L"Не удалось создать файлы data0/data1. Проверьте права доступа.", L"Ошибка", MB_ICONERROR);
            return false;
        }

        const char magic[8] = {'V','O','R','T','E','X','P','K'};
        out0.write(magic, 8);
        uint32_t version = 1;
        out0.write(reinterpret_cast<const char*>(&version), sizeof(version));
        uint32_t fileCount = static_cast<uint32_t>(files.size());
        out0.write(reinterpret_cast<const char*>(&fileCount), sizeof(fileCount));

        uint64_t currentOffset = 0;
        for (const auto& file : files) {
            fs::path rel = fs::relative(file, vortexFolder);
            std::wstring relStr = rel.wstring();
            std::replace(relStr.begin(), relStr.end(), L'\\', L'/');
            uint32_t pathLen = static_cast<uint32_t>(relStr.size());
            out0.write(reinterpret_cast<const char*>(&pathLen), sizeof(pathLen));
            out0.write(reinterpret_cast<const char*>(relStr.data()), pathLen * sizeof(wchar_t));
            uint64_t fileSize = fs::file_size(file);
            out0.write(reinterpret_cast<const char*>(&fileSize), sizeof(fileSize));
            out0.write(reinterpret_cast<const char*>(&currentOffset), sizeof(currentOffset));
            currentOffset += fileSize;
        }

        uint64_t bytesWrittenToData0 = 0;
        for (const auto& file : files) {
            std::ifstream in(WStringToUTF8(file.wstring()), std::ios::binary);
            if (!in) continue;
            char buffer[8192];
            while (in) {
                in.read(buffer, sizeof(buffer));
                std::streamsize n = in.gcount();
                if (n <= 0) break;
                if (bytesWrittenToData0 + n <= MAX_PART_SIZE) {
                    out0.write(buffer, n);
                    bytesWrittenToData0 += n;
                } else {
                    uint64_t spaceLeft = MAX_PART_SIZE - bytesWrittenToData0;
                    out0.write(buffer, spaceLeft);
                    out1.write(buffer + spaceLeft, n - spaceLeft);
                    bytesWrittenToData0 = MAX_PART_SIZE;
                }
            }
        }

        out0.close();
        out1.close();
        return true;
    } catch (const std::exception& e) {
        MessageBoxW(NULL, (L"Ошибка упаковки: " + std::wstring(e.what(), e.what() + strlen(e.what()))).c_str(), L"Ошибка", MB_ICONERROR);
        return false;
    }
}

bool ExtractDataFilesTo(const std::wstring& data0Path,
                        const std::wstring& data1Path,
                        const std::wstring& destFolder) {
    std::ifstream in0(WStringToUTF8(data0Path), std::ios::binary);
    std::ifstream in1(WStringToUTF8(data1Path), std::ios::binary);
    if (!in0 || !in1) return false;

    char magic[8];
    in0.read(magic, 8);
    if (memcmp(magic, "VORTEXPK", 8) != 0) return false;

    uint32_t version;
    in0.read(reinterpret_cast<char*>(&version), sizeof(version));
    if (version != 1) return false;

    uint32_t fileCount;
    in0.read(reinterpret_cast<char*>(&fileCount), sizeof(fileCount));

    struct FileEntry {
        std::wstring path;
        uint64_t size;
        uint64_t offset;
    };
    std::vector<FileEntry> entries;
    entries.reserve(fileCount);

    for (uint32_t i = 0; i < fileCount; ++i) {
        uint32_t pathLen;
        in0.read(reinterpret_cast<char*>(&pathLen), sizeof(pathLen));
        std::wstring path(pathLen, L'\0');
        in0.read(reinterpret_cast<char*>(&path[0]), pathLen * sizeof(wchar_t));
        uint64_t size, offset;
        in0.read(reinterpret_cast<char*>(&size), sizeof(size));
        in0.read(reinterpret_cast<char*>(&offset), sizeof(offset));
        entries.push_back({path, size, offset});
    }

    std::streampos dataStart = in0.tellg();

    for (const auto& entry : entries) {
        fs::path fullPath = fs::path(destFolder) / entry.path;
        fs::create_directories(fullPath.parent_path());

        std::ofstream out(WStringToUTF8(fullPath.wstring()), std::ios::binary);
        if (!out) return false;

        uint64_t remaining = entry.size;
        uint64_t currentOffset = entry.offset;

        while (remaining > 0) {
            char buffer[8192];
            uint64_t toRead = std::min<uint64_t>(remaining, sizeof(buffer));

            if (currentOffset < MAX_PART_SIZE) {
                uint64_t availInData0 = MAX_PART_SIZE - currentOffset;
                uint64_t readFromData0 = std::min<uint64_t>(toRead, availInData0);
                in0.seekg(dataStart + std::streamoff(currentOffset));
                in0.read(buffer, readFromData0);
                out.write(buffer, readFromData0);
                remaining -= readFromData0;
                currentOffset += readFromData0;
            } else {
                uint64_t data1Offset = currentOffset - MAX_PART_SIZE;
                in1.seekg(data1Offset);
                in1.read(buffer, toRead);
                out.write(buffer, toRead);
                remaining -= toRead;
                currentOffset += toRead;
            }
        }
        out.close();
    }

    in0.close();
    in1.close();
    return true;
}

// ==================== УСТАНОВКА OLLAMA ====================
DWORD WINAPI InstallOllamaThread(LPVOID lpParam) {
    LogMessage(L"Начало установки Ollama/модели");
    SetProgressText(L"Проверка Ollama...");
    SetProgressValue(30);

    RefreshEnvironmentPath();

    bool ollamaInstalled = IsOllamaInstalled();
    if (!ollamaInstalled) {
        SetProgressText(L"Установка Visual C++ Redistributable...");
        SetProgressValue(40);
        wchar_t tempPath[MAX_PATH];
        GetTempPathW(MAX_PATH, tempPath);
        std::wstring vcPath = std::wstring(tempPath) + L"vc_redist.x64.exe";
        HRESULT hrVc = URLDownloadToFileW(NULL, L"https://aka.ms/vs/17/release/vc_redist.x64.exe", vcPath.c_str(), 0, NULL);
        if (SUCCEEDED(hrVc) && FileExists(vcPath)) {
            std::wstring cmd = L"\"" + vcPath + L"\" /install /quiet /norestart";
            RunProcessAndWait(cmd, 300000);
            DeleteFileW(vcPath.c_str());
        }

        SetProgressText(L"Установка Ollama...");
        SetProgressValue(50);
        bool wingetAvailable = false;
        {
            std::wstring cmd = L"cmd.exe /c where winget >nul 2>nul";
            if (RunProcessAndWait(cmd, 5000)) wingetAvailable = true;
        }
        bool installSuccess = false;
        if (wingetAvailable) {
            SetProgressText(L"Установка Ollama через winget...");
            SetProgressValue(60);
            std::wstring cmd = L"cmd.exe /c winget install Ollama.Ollama --silent --accept-package-agreements --accept-source-agreements";
            installSuccess = RunProcessAndWait(cmd, 300000);
            if (installSuccess) { Sleep(2000); RefreshEnvironmentPath(); ollamaInstalled = IsOllamaInstalled(); }
        }
        if (!ollamaInstalled) {
            SetProgressText(L"Загрузка установщика Ollama...");
            SetProgressValue(70);
            GetTempPathW(MAX_PATH, tempPath);
            std::wstring installerPath = std::wstring(tempPath) + L"OllamaSetup.exe";
            HRESULT hr = URLDownloadToFileW(NULL, L"https://ollama.com/download/OllamaSetup.exe", installerPath.c_str(), 0, NULL);
            if (SUCCEEDED(hr) && FileExists(installerPath)) {
                SetProgressText(L"Установка Ollama (запуск установщика)...");
                SetProgressValue(80);
                std::wstring cmd = L"\"" + installerPath + L"\" /VERYSILENT /NORESTART";
                installSuccess = RunProcessAndWait(cmd, 300000);
                if (installSuccess) { Sleep(2000); RefreshEnvironmentPath(); ollamaInstalled = IsOllamaInstalled(); }
                DeleteFileW(installerPath.c_str());
            }
        }
        if (!ollamaInstalled) {
            SetProgressText(L"Не удалось установить Ollama. Установите её вручную с https://ollama.com/download");
            SetProgressValue(0);
            SetProgressMarquee(false);
            ollamaInstallationComplete = true;
            ollamaModelFailed = true;
            return 1;
        }
    } else {
        SetProgressText(L"Ollama уже установлена.");
    }

    SetProgressText(L"Ожидание запуска Ollama...");
    if (!WaitForOllamaServer(30000)) {
        SetProgressText(L"Сервис Ollama не запустился. Попробуйте запустить вручную: ollama serve");
        SetProgressValue(0);
        SetProgressMarquee(false);
        ollamaInstallationComplete = true;
        ollamaModelFailed = true;
        return 1;
    }

    const std::wstring modelName = L"qwen3.5:4b";
    SetProgressText(L"Проверка модели " + modelName + L"...");
    if (!IsOllamaModelAvailable(modelName)) {
        SetProgressText(L"Загрузка модели " + modelName + L"... Это может занять несколько минут.");
        SetProgressValue(90);
        std::wstring pullCmd = L"cmd.exe /c ollama pull " + modelName;
        bool pullSuccess = RunProcessAndWait(pullCmd, 600000);
        if (!pullSuccess || !IsOllamaModelAvailable(modelName)) {
            SetProgressText(L"Не удалось автоматически загрузить модель. Загрузите вручную: ollama pull " + modelName);
            SetProgressValue(0);
            SetProgressMarquee(false);
            ollamaInstallationComplete = true;
            ollamaModelFailed = true;
            return 1;
        }
    }

    SetProgressText(L"Установка Ollama и модели завершена");
    SetProgressValue(100);
    SetProgressMarquee(false);
    ollamaInstallationComplete = true;
    ollamaModelFailed = false;
    LogMessage(L"Установка Ollama/модели завершена успешно");
    return 0;
}

// ==================== ОСНОВНАЯ УСТАНОВКА ====================
void PerformInstall() {
    LogMessage(L"Начало основной установки");
    while (!installPath.empty() && (installPath.back() == L'\\' || installPath.back() == L'/'))
        installPath.pop_back();

    if (installPath.length() == 2 && installPath[1] == L':') {
        installPath += L"\\Vortex";
    } else {
        installPath += L"\\Vortex";
    }

    fs::create_directories(installPath);

    wchar_t selfPath[MAX_PATH];
    GetModuleFileNameW(NULL, selfPath, MAX_PATH);
    std::wstring uninstallExe = installPath + L"\\uninstall.exe";
    if (!CopyFileW(selfPath, uninstallExe.c_str(), FALSE)) {
        LogMessage(L"Не удалось скопировать uninstall.exe");
    }

    std::wstring modelDir = FindBestModelDirectory();
    SetOllamaModelsEnvironment(modelDir);
    LogMessage(L"Папка моделей Ollama: " + modelDir);

    std::wstring exeDir = GetCurrentExeDir();
    std::wstring vortexFolder = exeDir + L"\\" + VORTEX_FOLDER;
    std::wstring data0Path = exeDir + L"\\" + DATA0_FILE;
    std::wstring data1Path = exeDir + L"\\" + DATA1_FILE;

    bool hasVortexFolder = fs::exists(vortexFolder) && fs::is_directory(vortexFolder);
    bool hasDataFiles = FileExists(data0Path) && FileExists(data1Path);

    if (!hasVortexFolder && !hasDataFiles) {
        MessageBoxW(hMainWnd, L"Рядом с установщиком не найдены ни папка Vortex, ни файлы data0/data1.",
                    L"Ошибка", MB_ICONERROR);
        return;
    }

    if (!hasDataFiles && hasVortexFolder) {
        SetProgressText(L"Создание автономного пакета (data0/data1)...");
        SetProgressValue(10);
        bool packOk = PackVortexToDataFiles(vortexFolder, data0Path, data1Path);
        if (!packOk) {
            MessageBoxW(hMainWnd, L"Не удалось создать data-файлы.", L"Ошибка", MB_ICONERROR);
            return;
        }
        hasDataFiles = true;
    }

    SetProgressText(L"Распаковка файлов...");
    SetProgressValue(20);
    bool extractOk = ExtractDataFilesTo(data0Path, data1Path, installPath);
    if (!extractOk) {
        MessageBoxW(hMainWnd, L"Ошибка при распаковке data-файлов.", L"Ошибка", MB_ICONERROR);
        return;
    }

    std::wstring targetExe = FindVortexExe(installPath);
    if (targetExe.empty()) {
        MessageBoxW(hMainWnd, L"Не удалось найти исполняемый файл Vortex.", L"Ошибка", MB_ICONERROR);
        return;
    }

    std::wstring iconPath = installPath + L"\\logo.ico";
    if (!FileExists(iconPath)) iconPath = L"";

    if (createDesktopShortcut) {
        wchar_t desktop[MAX_PATH];
        SHGetSpecialFolderPathW(NULL, desktop, CSIDL_DESKTOPDIRECTORY, FALSE);
        std::wstring desktopShortcut = std::wstring(desktop) + L"\\Vortex.lnk";
        CreateShortcut(targetExe, desktopShortcut, iconPath);
    }

    wchar_t startMenu[MAX_PATH];
    SHGetSpecialFolderPathW(NULL, startMenu, CSIDL_PROGRAMS, FALSE);
    std::wstring startMenuShortcut = std::wstring(startMenu) + L"\\Vortex.lnk";
    CreateShortcut(targetExe, startMenuShortcut, iconPath);

    WriteUninstallRegistry(installPath);

    SetProgressText(L"Подготовка к установке Ollama и модели...");
    SetProgressValue(30);
    CreateThread(NULL, 0, InstallOllamaThread, NULL, 0, NULL);
}

// ==================== УДАЛЕНИЕ ====================
void PerformUninstall() {
    LogMessage(L"Начало удаления");
    std::wstring localAppDataPath = GetDefaultInstallPath() + L"\\Vortex";
    DeleteFolder(fs::path(localAppDataPath));
    DeleteFolder(fs::path(L"C:\\Program Files\\Vortex"));
    DeleteFolder(fs::path(L"C:\\Program Files\\Vortex\\Vortex"));
    DeleteFolder(fs::path(L"C:\\Vortex"));
    DeleteFolder(fs::path(L"C:\\Vortex\\Vortex"));

    wchar_t desktop[MAX_PATH];
    SHGetSpecialFolderPathW(NULL, desktop, CSIDL_DESKTOPDIRECTORY, FALSE);
    RemoveShortcut(std::wstring(desktop) + L"\\Vortex.lnk");
    wchar_t startMenu[MAX_PATH];
    SHGetSpecialFolderPathW(NULL, startMenu, CSIDL_PROGRAMS, FALSE);
    RemoveShortcut(std::wstring(startMenu) + L"\\Vortex.lnk");

    RemoveRegistryKeys();
    RemoveOllamaModelsEnvironment();
    LogMessage(L"Удаление завершено");
    MessageBoxW(NULL, L"Удаление завершено.", L"Vortex", MB_OK);
}

// ==================== ОСТАЛЬНОЙ ИНТЕРФЕЙС ====================
void UpdateStatusText() {
    vortexFound = IsVortexInstalled();
    ollamaFound = IsOllamaInstalled();
    std::wstring status;
    if (vortexFound) status += L"Vortex: установлен\n";
    else status += L"Vortex: не установлен\n";
    if (ollamaFound) status += L"Ollama: найдена\n";
    else status += L"Ollama: не найдена\n";
    SetWindowTextW(hStaticStatus, status.c_str());
}

void ShowPage(Page page) {
    currentPage = page;
    ShowWindow(hPageWelcome, page == PAGE_WELCOME ? SW_SHOW : SW_HIDE);
    ShowWindow(hPageOptions, page == PAGE_OPTIONS ? SW_SHOW : SW_HIDE);
    ShowWindow(hPageInstall, page == PAGE_INSTALL ? SW_SHOW : SW_HIDE);
    ShowWindow(hPageFinish, page == PAGE_FINISH ? SW_SHOW : SW_HIDE);
    UpdateButtons();
}

void UpdateButtons() {
    switch (currentPage) {
        case PAGE_WELCOME:
            ShowWindow(hBtnNext, SW_SHOW);
            ShowWindow(hBtnBack, SW_HIDE);
            ShowWindow(hBtnInstall, SW_HIDE);
            ShowWindow(hBtnFinish, SW_HIDE);
            EnableWindow(hBtnNext, TRUE);
            break;
        case PAGE_OPTIONS:
            ShowWindow(hBtnNext, SW_HIDE);
            ShowWindow(hBtnBack, SW_SHOW);
            ShowWindow(hBtnInstall, SW_SHOW);
            ShowWindow(hBtnFinish, SW_HIDE);
            EnableWindow(hBtnBack, TRUE);
            EnableWindow(hBtnInstall, TRUE);
            break;
        case PAGE_INSTALL:
            ShowWindow(hBtnNext, SW_HIDE);
            ShowWindow(hBtnBack, SW_HIDE);
            ShowWindow(hBtnInstall, SW_HIDE);
            ShowWindow(hBtnFinish, SW_HIDE);
            break;
        case PAGE_FINISH:
            ShowWindow(hBtnNext, SW_HIDE);
            ShowWindow(hBtnBack, SW_HIDE);
            ShowWindow(hBtnInstall, SW_HIDE);
            ShowWindow(hBtnFinish, SW_SHOW);
            EnableWindow(hBtnFinish, TRUE);
            break;
    }
    ShowWindow(hBtnCancel, SW_SHOW);
    EnableWindow(hBtnCancel, TRUE);
}

void OnNext() {
    if (currentPage == PAGE_WELCOME) {
        ShowPage(PAGE_OPTIONS);
        UpdateStatusText();
    }
}

void OnBack() {
    if (currentPage == PAGE_OPTIONS) {
        ShowPage(PAGE_WELCOME);
    }
}

void BrowseFolder() {
    BROWSEINFOW bi = {0};
    bi.hwndOwner = hMainWnd;
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
    if (pidl) {
        wchar_t path[MAX_PATH];
        if (SHGetPathFromIDListW(pidl, path)) {
            SetWindowTextW(hEditPath, path);
        }
        CoTaskMemFree(pidl);
    }
}

void OnInstall() {
    wchar_t buf[MAX_PATH];
    GetWindowTextW(hEditPath, buf, MAX_PATH);
    installPath = buf;
    if (installPath.empty()) {
        MessageBoxW(hMainWnd, L"Укажите путь установки.", L"Ошибка", MB_ICONERROR);
        return;
    }
    createDesktopShortcut = (SendMessageW(hChkCreateDesktopShortcut, BM_GETCHECK, 0, 0) == BST_CHECKED);

    ShowPage(PAGE_INSTALL);
    ollamaInstallationComplete = false;
    ollamaModelFailed = false;

    PerformInstall();

    while (!ollamaInstallationComplete) {
        MSG msg;
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        Sleep(100);
    }

    if (ollamaModelFailed) {
        int ret = MessageBoxW(hMainWnd,
                              L"Модель Vortex не была загружена автоматически.\n"
                              L"Хотите попробовать ещё раз?",
                              L"Загрузка модели",
                              MB_YESNO | MB_ICONQUESTION);
        if (ret == IDYES) {
            ollamaInstallationComplete = false;
            ollamaModelFailed = false;
            CreateThread(NULL, 0, InstallOllamaThread, NULL, 0, NULL);
            while (!ollamaInstallationComplete) {
                MSG msg;
                while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
                    TranslateMessage(&msg);
                    DispatchMessageW(&msg);
                }
                Sleep(100);
            }
        }
    }

    ShowPage(PAGE_FINISH);
}

void OnFinish() {
    DestroyWindow(hMainWnd);
}

void OnCancel() {
    DestroyWindow(hMainWnd);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            std::wstring logoPath = L"Vortex\\logo.ico";
            if (FileExists(logoPath)) {
                HANDLE hIcon = LoadImageW(NULL, logoPath.c_str(), IMAGE_ICON, 0, 0, LR_LOADFROMFILE);
                if (hIcon) {
                    SendMessageW(hWnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
                    SendMessageW(hWnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
                }
            }

            hPageWelcome = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE, 10, 10, 460, 300, hWnd, NULL, NULL, NULL);

            HWND hVLabel = CreateWindowW(L"STATIC", L"V",
                                         WS_CHILD | WS_VISIBLE | SS_CENTER,
                                         10, 10, 460, 80,
                                         hPageWelcome, NULL, NULL, NULL);
            HFONT hFont = CreateFontW(64, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                      DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                      CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            SendMessageW(hVLabel, WM_SETFONT, (WPARAM)hFont, TRUE);
            HDC hdc = GetDC(hVLabel);
            SetTextColor(hdc, RGB(108, 92, 231));
            ReleaseDC(hVLabel, hdc);

            CreateWindowW(L"STATIC", L"Vortex 2.1.12", WS_CHILD | WS_VISIBLE | SS_CENTER,
                          10, 100, 460, 30, hPageWelcome, NULL, NULL, NULL);
            CreateWindowW(L"STATIC",
                L"Создатель: Delinor\n\n"
                L"Vortex — это локальный ИИ-ассистент.\n"
                L"Он работает полностью на вашем компьютере и не отправляет данные в интернет.\n"
                L"Vortex способен отвечать на вопросы, писать код, объяснять сложные темы и помогать в решении задач.\n\n"
                L"Нажмите «Далее», чтобы перейти к настройке установки.",
                WS_CHILD | WS_VISIBLE | SS_LEFT,
                20, 140, 440, 160, hPageWelcome, NULL, NULL, NULL);

            hPageOptions = CreateWindowW(L"STATIC", L"", WS_CHILD, 10, 10, 460, 300, hWnd, NULL, NULL, NULL);
            CreateWindowW(L"STATIC", L"Папка установки:", WS_CHILD | WS_VISIBLE | SS_LEFT,
                          20, 30, 150, 20, hPageOptions, NULL, NULL, NULL);
            hEditPath = CreateWindowW(L"EDIT", GetDefaultInstallPath().c_str(),
                                      WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                                      20, 55, 350, 25, hPageOptions, NULL, NULL, NULL);
            hBtnBrowse = CreateWindowW(L"BUTTON", L"Обзор...", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                       380, 55, 70, 25, hPageOptions, (HMENU)10, NULL, NULL);
            hChkCreateDesktopShortcut = CreateWindowW(L"BUTTON", L"Создать ярлык на рабочем столе",
                                                      WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                                                      20, 100, 300, 30, hPageOptions, NULL, NULL, NULL);
            SendMessageW(hChkCreateDesktopShortcut, BM_SETCHECK, BST_CHECKED, 0);
            hStaticStatus = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_LEFT,
                                          20, 150, 420, 60, hPageOptions, NULL, NULL, NULL);

            hPageInstall = CreateWindowW(L"STATIC", L"", WS_CHILD, 10, 10, 460, 300, hWnd, NULL, NULL, NULL);
            hProgressBar = CreateWindowW(PROGRESS_CLASSW, NULL, WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
                                         20, 120, 440, 30, hPageInstall, NULL, NULL, NULL);
            hStaticProgressText = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_CENTER,
                                                20, 160, 440, 40, hPageInstall, NULL, NULL, NULL);
            SendMessageW(hProgressBar, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
            SendMessageW(hProgressBar, PBM_SETPOS, 0, 0);

            hPageFinish = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE, 10, 10, 460, 300, hWnd, NULL, NULL, NULL);
            CreateWindowW(L"STATIC", L"Установка завершена!", WS_CHILD | WS_VISIBLE | SS_CENTER,
                          10, 80, 460, 40, hPageFinish, NULL, NULL, NULL);

            hBtnBack = CreateWindowW(L"BUTTON", L"< Назад", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                     150, 330, 80, 30, hWnd, (HMENU)1, NULL, NULL);
            hBtnNext = CreateWindowW(L"BUTTON", L"Далее >", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                     240, 330, 80, 30, hWnd, (HMENU)2, NULL, NULL);
            hBtnInstall = CreateWindowW(L"BUTTON", L"Установить", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                        240, 330, 90, 30, hWnd, (HMENU)3, NULL, NULL);
            hBtnFinish = CreateWindowW(L"BUTTON", L"Готово", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                       240, 330, 80, 30, hWnd, (HMENU)5, NULL, NULL);
            hBtnCancel = CreateWindowW(L"BUTTON", L"Отмена", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                       340, 330, 80, 30, hWnd, (HMENU)4, NULL, NULL);

            ShowPage(PAGE_WELCOME);
            break;
        }
        case WM_COMMAND: {
            int id = LOWORD(wParam);
            switch (id) {
                case 1: OnBack(); break;
                case 2: OnNext(); break;
                case 3: OnInstall(); break;
                case 4: OnCancel(); break;
                case 5: OnFinish(); break;
                case 10: BrowseFolder(); break;
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

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);

    if (!(lpCmdLine && (wcsstr(lpCmdLine, L"/uninstall") || wcsstr(lpCmdLine, L"-uninstall")))) {
        if (IsVortexInstalled()) {
            int choice = MessageBoxW(NULL,
                                     L"Vortex уже установлен.\n\n"
                                     L"Хотите удалить программу?\n"
                                     L"Нажмите 'Да', чтобы удалить.\n"
                                     L"Нажмите 'Нет', чтобы переустановить.\n"
                                     L"Нажмите 'Отмена', чтобы выйти.",
                                     L"Vortex Setup",
                                     MB_YESNOCANCEL | MB_ICONQUESTION);
            if (choice == IDYES) {
                PerformUninstall();
                return 0;
            } else if (choice == IDCANCEL) {
                return 0;
            }
        }
    }

    if (lpCmdLine && (wcsstr(lpCmdLine, L"/uninstall") || wcsstr(lpCmdLine, L"-uninstall"))) {
        PerformUninstall();
        return 0;
    }

    WNDCLASSW wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"VortexSetupClass";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassW(&wc);

    hMainWnd = CreateWindowW(L"VortexSetupClass", L"Установка Vortex 2.1.12",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 500, 400,
        NULL, NULL, hInstance, NULL);

    if (!hMainWnd) return 0;

    ShowWindow(hMainWnd, nCmdShow);
    UpdateWindow(hMainWnd);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return (int)msg.wParam;
}