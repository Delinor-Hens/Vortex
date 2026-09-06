# gui/runtime_manager.py
import os
import shutil
import subprocess

class Runtime:
    def __init__(self, name, exe_names, install_command=None, version_args=["--version"]):
        self.name = name
        self.exe_names = exe_names
        self.install_command = install_command
        self.version_args = version_args
        self.path = None

    def detect(self):
        """Проверяет наличие исполняемого файла в PATH или стандартных местах."""
        # Проверка PATH
        for exe in self.exe_names:
            path = shutil.which(exe)
            if path:
                self.path = path
                return True

        # Дополнительные стандартные пути
        extra_paths = []
        if self.name == "Python":
            extra_paths = [
                os.path.expandvars(r"%LOCALAPPDATA%\Programs\Python\Python312\python.exe"),
                os.path.expandvars(r"%LOCALAPPDATA%\Programs\Python\Python311\python.exe"),
                r"C:\Python312\python.exe",
                r"C:\Python311\python.exe",
                r"C:\Python310\python.exe",
            ]
        elif self.name == "Node.js":
            extra_paths = [
                os.path.expandvars(r"%ProgramFiles%\nodejs\node.exe"),
                os.path.expandvars(r"%ProgramFiles(x86)%\nodejs\node.exe"),
            ]
        elif self.name == "Java":
            extra_paths = [
                os.path.expandvars(r"%ProgramFiles%\Java\jdk-17\bin\java.exe"),
                os.path.expandvars(r"%ProgramFiles%\Java\jdk-21\bin\java.exe"),
                os.path.expandvars(r"%JAVA_HOME%\bin\java.exe"),
            ]
        elif self.name == "C++ (MinGW)":
            extra_paths = [
                r"C:\msys64\mingw64\bin\g++.exe",
                r"C:\msys64\ucrt64\bin\g++.exe",
                r"C:\MinGW\bin\g++.exe",
            ]
        elif self.name == "C# (.NET)":
            extra_paths = [
                os.path.expandvars(r"%ProgramFiles%\dotnet\dotnet.exe"),
                os.path.expandvars(r"%ProgramFiles(x86)%\dotnet\dotnet.exe"),
            ]
        elif self.name == "Go":
            extra_paths = [
                os.path.expandvars(r"%ProgramFiles%\Go\bin\go.exe"),
                r"C:\Go\bin\go.exe",
            ]
        elif self.name == "Rust":
            extra_paths = [
                os.path.expandvars(r"%USERPROFILE%\.cargo\bin\rustc.exe"),
            ]

        for p in extra_paths:
            if os.path.isfile(p):
                self.path = p
                return True
        return False

    def get_version(self):
        if not self.path:
            return ""
        try:
            result = subprocess.run(
                [self.path] + self.version_args,
                capture_output=True,
                text=True,
                timeout=5
            )
            return result.stdout.strip().split('\n')[0]
        except:
            return "неизвестно"

    def install(self):
        """Пытается установить среду через winget."""
        if not self.install_command:
            return False
        try:
            subprocess.run(self.install_command, shell=True, check=False)
            # Проверяем, появилась ли среда
            return self.detect()
        except:
            return False


# Список предопределённых сред выполнения
RUNTIMES = [
    Runtime(
        "Python",
        ["python", "python3"],
        install_command="winget install Python.Python.3.12 --silent --accept-package-agreements --accept-source-agreements"
    ),
    Runtime(
        "Node.js",
        ["node", "nodejs"],
        install_command="winget install OpenJS.NodeJS.LTS --silent --accept-package-agreements --accept-source-agreements"
    ),
    Runtime(
        "Java",
        ["java", "javac"],
        install_command="winget install EclipseAdoptium.Temurin.17.JDK --silent --accept-package-agreements --accept-source-agreements"
    ),
    Runtime(
        "C++ (MinGW)",
        ["g++", "c++"],
        install_command="winget install BrechtSanders.WinLibs.POSIX.UCRT --silent --accept-package-agreements --accept-source-agreements"
    ),
    Runtime(
        "C# (.NET)",
        ["dotnet"],
        install_command="winget install Microsoft.DotNet.SDK.8 --silent --accept-package-agreements --accept-source-agreements"
    ),
    Runtime(
        "Go",
        ["go"],
        install_command="winget install GoLang.Go --silent --accept-package-agreements --accept-source-agreements"
    ),
    Runtime(
        "Rust",
        ["rustc", "cargo"],
        install_command="winget install Rustlang.Rustup --silent --accept-package-agreements --accept-source-agreements"
    ),
]