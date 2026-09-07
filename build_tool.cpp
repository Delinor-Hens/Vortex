// build_tool.cpp
#include <iostream>
#include <string>
#include <cstdlib>
#include <filesystem>
#include <vector>
#include <algorithm>
#include <fstream>

#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;

enum class OS { Windows, Linux, macOS, Android, Unknown };

OS getCurrentOS() {
#ifdef _WIN32
    return OS::Windows;
#elif __linux__
    return OS::Linux;
#elif __APPLE__
    return OS::macOS;
#else
    return OS::Unknown;
#endif
}

std::string osToString(OS os) {
    switch (os) {
        case OS::Windows: return "Windows";
        case OS::Linux: return "Linux";
        case OS::macOS: return "macOS";
        case OS::Android: return "Android";
        default: return "Unknown";
    }
}

bool runCommand(const std::string& cmd) {
    std::cout << "> " << cmd << std::endl;
    int result = std::system(cmd.c_str());
    return result == 0;
}

// Упаковка папки Vortex в data0/data1 (без GUI)
bool packVortexFolder(const std::string& vortexFolder,
                      const std::string& data0Path,
                      const std::string& data1Path) {
    try {
        std::vector<fs::path> files;
        for (const auto& entry : fs::recursive_directory_iterator(vortexFolder, fs::directory_options::skip_permission_denied)) {
            if (entry.is_regular_file())
                files.push_back(entry.path());
        }
        std::sort(files.begin(), files.end());

        std::ofstream out0(data0Path, std::ios::binary | std::ios::trunc);
        std::ofstream out1(data1Path, std::ios::binary | std::ios::trunc);
        if (!out0 || !out1) {
            out0.close();
            out1.close();
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
            std::wstring relW = rel.wstring();
            std::replace(relW.begin(), relW.end(), L'\\', L'/');
            uint32_t pathLen = static_cast<uint32_t>(relW.size());
            out0.write(reinterpret_cast<const char*>(&pathLen), sizeof(pathLen));
            out0.write(reinterpret_cast<const char*>(relW.data()), pathLen * sizeof(wchar_t));
            uint64_t fileSize = fs::file_size(file);
            out0.write(reinterpret_cast<const char*>(&fileSize), sizeof(fileSize));
            out0.write(reinterpret_cast<const char*>(&currentOffset), sizeof(currentOffset));
            currentOffset += fileSize;
        }

        const uint64_t MAX_PART_SIZE = 100ULL * 1024ULL * 1024ULL;
        uint64_t bytesWrittenToData0 = 0;
        for (const auto& file : files) {
            std::ifstream in(file, std::ios::binary);
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
    } catch (...) {
        return false;
    }
}

bool compileCore(OS os) {
    std::string cmd;
    if (os == OS::Windows) {
        cmd = "g++ -O2 -shared -o build\\vortex_core.dll "
              "core\\src\\api_bridge.cpp core\\src\\llm_engine.cpp core\\src\\memory_store.cpp "
              "core\\src\\text_processor.cpp core\\src\\providers.cpp "
              "-DVORTEX_CORE_EXPORTS -std=c++11 -mwindows "
              "-static-libgcc -static-libstdc++ "
              "-Wl,-Bstatic -lstdc++ -lwinpthread -Wl,-Bdynamic "
              "-lws2_32 -lcurl -Icore/include";
    } else {
        return true; // для не-Windows пропускаем
    }
    return runCommand(cmd);
}

bool buildPythonApp(OS os) {
    if (os != OS::Windows) {
        return true;
    }
    std::string cmd = "python -m PyInstaller --onefile --windowed --noconfirm "
                      "--add-binary \"build/vortex_core.dll;build\" "
                      "--add-data \"assets;assets\" --add-data \"config.json;.\" vortex.py";
    bool ok = runCommand(cmd);
    if (ok) {
        std::error_code ec;
        fs::remove("vortex.spec", ec);
    }
    return ok;
}

bool buildLauncher(OS os) {
    if (os == OS::Windows) {
        return runCommand("g++ -O2 vortex_launcher.cpp -o Vortex.exe "
                          "-municode -mwindows -static -lole32 -lshell32 -luuid -lshlwapi -lcomctl32");
    }
    return true;
}

bool createWindowsInstaller() {
    if (!runCommand("g++ -O2 vortex_setup.cpp -o vortex_setup.exe "
                    "-municode -mwindows -static -lole32 -lshell32 -luuid -lshlwapi "
                    "-lcomctl32 -loleaut32 -lurlmon"))
        return false;

    fs::path tempVortex = fs::path("build") / "temp_vortex_pack";
    std::error_code ec;
    fs::remove_all(tempVortex, ec);
    fs::create_directories(tempVortex);

    fs::copy("dist/vortex.exe", tempVortex / "vortex.exe", fs::copy_options::overwrite_existing);
    fs::copy("build/vortex_core.dll", tempVortex / "vortex_core.dll", fs::copy_options::overwrite_existing);
    fs::copy("assets", tempVortex / "assets", fs::copy_options::recursive | fs::copy_options::overwrite_existing);
    fs::copy("config.json", tempVortex / "config.json", fs::copy_options::overwrite_existing);

    std::cout << "Упаковка data0/data1...\n";
    if (!packVortexFolder(tempVortex.string(), "data0", "data1")) {
        std::cerr << "Ошибка упаковки data-файлов.\n";
        fs::remove_all(tempVortex, ec);
        return false;
    }

    fs::remove_all(tempVortex, ec);
    return true;
}

// Копирование готовых бинарников Windows без упаковки (портативная версия)
bool createPortableWindows() {
    fs::path tempVortex = fs::path("build") / "portable_vortex";
    std::error_code ec;
    fs::remove_all(tempVortex, ec);
    fs::create_directories(tempVortex);

    fs::copy("dist/vortex.exe", tempVortex / "vortex.exe", fs::copy_options::overwrite_existing);
    fs::copy("build/vortex_core.dll", tempVortex / "vortex_core.dll", fs::copy_options::overwrite_existing);
    fs::copy("assets", tempVortex / "assets", fs::copy_options::recursive | fs::copy_options::overwrite_existing);
    fs::copy("config.json", tempVortex / "config.json", fs::copy_options::overwrite_existing);

    return true;
}

bool copyPrebuiltArtifact(OS os) {
    std::string sourceFile;
    std::string targetFile;
    if (os == OS::macOS) {
        sourceFile = "Vortex_macOS_installer.tar.gz";
        targetFile = sourceFile;
    } else if (os == OS::Linux) {
        sourceFile = "Vortex_Linux_installer.tar.gz";
        targetFile = sourceFile;
    } else if (os == OS::Android) {
        sourceFile = "Vortex.apk";
        targetFile = sourceFile;
    }

    if (sourceFile.empty()) return false;

    if (!fs::exists(sourceFile)) {
        std::cerr << "Файл " << sourceFile << " не найден. Поместите готовый пакет в корень проекта.\n";
        return false;
    }

    try {
        fs::path targetDir = L"E:\\Vortex\\Vortex Setup";
        fs::create_directories(targetDir);
        fs::copy(sourceFile, targetDir / targetFile, fs::copy_options::overwrite_existing);
        return true;
    } catch (...) {
        std::cerr << "Не удалось скопировать файл в E:\\Vortex\\Vortex Setup.\n";
        return false;
    }
}

void printUsage() {
    std::cout << "Использование: build_tool [--target windows|linux|macos|android] [--no-archive]\n";
    std::cout << "--no-archive: для Windows создаёт портативную папку вместо установщика.\n";
}

int main(int argc, char* argv[]) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    OS targetOS = getCurrentOS();
    bool noArchive = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--target" && i + 1 < argc) {
            std::string target = argv[++i];
            if (target == "windows") targetOS = OS::Windows;
            else if (target == "linux") targetOS = OS::Linux;
            else if (target == "macos") targetOS = OS::macOS;
            else if (target == "android") targetOS = OS::Android;
            else {
                std::cerr << "Неизвестная платформа: " << target << "\n";
                printUsage();
                return 1;
            }
        } else if (arg == "--no-archive") {
            noArchive = true;
        } else {
            printUsage();
            return 1;
        }
    }

    std::cout << "Целевая ОС: " << osToString(targetOS) << (noArchive ? " (без архива)" : "") << std::endl;

    if (targetOS == OS::Windows) {
        if (!runCommand("g++ --version")) {
            std::cerr << "Ошибка: g++ не найден.\n";
            return 1;
        }

        std::error_code ec;
        fs::remove_all("build", ec);
        fs::remove_all("dist", ec);
        fs::remove("data0", ec);
        fs::remove("data1", ec);
        fs::remove("vortex_setup.exe", ec);
        fs::remove("Vortex.exe", ec);

        fs::create_directories("build");
        fs::create_directories("dist");

        std::cout << "\n=== Компиляция C++ ядра ===\n";
        if (!compileCore(targetOS)) {
            std::cerr << "Ошибка компиляции ядра.\n";
            return 1;
        }

        std::cout << "\n=== Сборка Python-приложения ===\n";
        if (!buildPythonApp(targetOS)) {
            std::cerr << "Ошибка сборки Python.\n";
            return 1;
        }

        std::cout << "\n=== Сборка лаунчера ===\n";
        if (!buildLauncher(targetOS)) {
            std::cerr << "Ошибка сборки лаунчера.\n";
            return 1;
        }

        if (!noArchive) {
            std::cout << "\n=== Создание установщика ===\n";
            if (!createWindowsInstaller()) {
                std::cerr << "Ошибка создания установщика.\n";
                return 1;
            }
        } else {
            std::cout << "\n=== Создание портативной версии ===\n";
            if (!createPortableWindows()) {
                std::cerr << "Ошибка создания портативной версии.\n";
                return 1;
            }
        }

        // Копируем результат в E:\Vortex\Vortex Setup
        try {
            fs::path targetDir = L"E:\\Vortex\\Vortex Setup";
            fs::create_directories(targetDir);
            if (!noArchive) {
                fs::copy("vortex_setup.exe", targetDir / "vortex_setup.exe", fs::copy_options::overwrite_existing);
                fs::copy("data0", targetDir / "data0", fs::copy_options::overwrite_existing);
                fs::copy("data1", targetDir / "data1", fs::copy_options::overwrite_existing);
                fs::copy("Vortex.exe", targetDir / "Vortex.exe", fs::copy_options::overwrite_existing);
            } else {
                // Копируем папку portable_vortex
                fs::copy("build/portable_vortex", targetDir / "Vortex", fs::copy_options::recursive | fs::copy_options::overwrite_existing);
            }
        } catch (...) {
            std::cerr << "Не удалось скопировать файлы в E:\\Vortex\\Vortex Setup.\n";
        }

        // Очистка рабочей папки
        fs::remove_all("build", ec);
        fs::remove_all("dist", ec);
        fs::remove("data0", ec);
        fs::remove("data1", ec);
        fs::remove("vortex_setup.exe", ec);
        fs::remove("Vortex.exe", ec);

        std::cout << "\nСборка Windows завершена!\n";
    } else {
        if (copyPrebuiltArtifact(targetOS)) {
            std::cout << "\nГотовый пакет скопирован в E:\\Vortex\\Vortex Setup\n";
        } else {
            std::cout << "\nСначала соберите пакет на целевой ОС и положите файл в корень проекта.\n";
            return 1;
        }
    }

    return 0;
}