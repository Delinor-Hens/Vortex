// vortex_pack_builder.cpp
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

bool runCommand(const std::string& cmd) {
    std::cout << "> " << cmd << std::endl;
    int result = std::system(cmd.c_str());
    return result == 0;
}

void Log(const std::string& msg) {
    std::ofstream log("builder_log.txt", std::ios::app);
    if (log) log << msg << std::endl;
    std::cout << msg << std::endl;
}

int main() {
    Log("=== Запуск vortex_pack_builder ===");

    if (!fs::exists("vortex_setup.exe") ||
        !fs::exists("data0") ||
        !fs::exists("data1")) {
        Log("Ошибка: отсутствуют необходимые файлы.");
        Log("Требуются: vortex_setup.exe, data0, data1");
        return 1;
    }

    Log("Файлы найдены:");
    Log("  vortex_setup.exe: " + std::to_string(fs::file_size("vortex_setup.exe")) + " байт");
    Log("  data0: " + std::to_string(fs::file_size("data0")) + " байт");
    Log("  data1: " + std::to_string(fs::file_size("data1")) + " байт");

    // Генерация embed.s
    std::ofstream asmFile("embed.s");
    if (!asmFile) {
        Log("Ошибка: не удалось создать embed.s");
        return 1;
    }
    asmFile << ".section .rodata\n";
    asmFile << ".global _binary_vortex_setup_start\n";
    asmFile << ".global _binary_vortex_setup_end\n";
    asmFile << "_binary_vortex_setup_start:\n";
    asmFile << ".incbin \"vortex_setup.exe\"\n";
    asmFile << "_binary_vortex_setup_end:\n\n";

    asmFile << ".global _binary_data0_start\n";
    asmFile << ".global _binary_data0_end\n";
    asmFile << "_binary_data0_start:\n";
    asmFile << ".incbin \"data0\"\n";
    asmFile << "_binary_data0_end:\n\n";

    asmFile << ".global _binary_data1_start\n";
    asmFile << ".global _binary_data1_end\n";
    asmFile << "_binary_data1_start:\n";
    asmFile << ".incbin \"data1\"\n";
    asmFile << "_binary_data1_end:\n";
    asmFile.close();
    Log("embed.s сгенерирован");

    if (!runCommand("gcc -c embed.s -o embed.o")) {
        Log("Ошибка компиляции embed.s");
        return 1;
    }
    Log("embed.o создан");

    if (!runCommand("g++ vortex_single_pack.cpp embed.o -o Setup.exe -municode -mwindows -static -lole32 -lshell32")) {
        Log("Ошибка сборки Setup.exe");
        return 1;
    }
    Log("Setup.exe успешно создан");
    Log("=== Завершение работы ===");
    return 0;
}