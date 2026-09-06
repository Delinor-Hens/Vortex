# 📚 Инструкция для разработчиков Vortex

Этот документ поможет вам настроить среду разработки, собрать проект из исходников и внести изменения, используя отдельную ветку `develop`.

---

## 1. Требования

- **Windows 10 / 11 (64-bit)**
- **MinGW-w64** (рекомендуется MSYS2 или standalone версия)
- **Python 3.12+ (64-bit)** с установленным `pip`
- **PyInstaller** для сборки автономного exe
- **Git** для управления версиями

Убедитесь, что `g++`, `python` и `git` доступны из командной строки (PATH).

---

## 2. Клонирование репозитория

```bash
git clone https://github.com/Delinor-Hens/Vortex.git
cd Vortex
```

Все команды ниже выполняются из корня проекта (`Vortex/`).

---

## 3. Ветки

Основная разработка ведётся в ветке `develop`, стабильные релизы попадают в `main`.

### 3.1. Создание ветки `develop` локально

```bash
git checkout -b develop
```

### 3.2. Если ветка `develop` уже есть на GitHub

```bash
git fetch origin
git checkout develop
git pull origin develop
```

### 3.3. Переключение между ветками

```bash
git checkout main       # переключиться на main
git checkout develop    # переключиться на develop
```

---

## 4. Сборка DLL (C++ бэкенд)

Перейдите в корень проекта и выполните:

```bat
g++ -shared -o build\vortex_core.dll core\src\api_bridge.cpp core\src\llm_engine.cpp core\src\memory_store.cpp core\src\text_processor.cpp core\src\providers.cpp -DVORTEX_CORE_EXPORTS -std=c++11 -mwindows -static-libgcc -static-libstdc++ -lws2_32 -lwinhttp -lcrypt32 -Icore/include
```

**Важно:**  
- `-static-libgcc -static-libstdc++` обязательно, иначе DLL будет требовать `libgcc_s_seh-1.dll` на чистых системах.  
- Если вы не используете облачных провайдеров, можно убрать `providers.cpp` и библиотеки `-lwinhttp -lcrypt32`, но тогда провайдеры не будут работать.

Результат: `build\vortex_core.dll`.

---

## 5. Сборка автономного exe (Python GUI)

### 5.1. Установите PyInstaller

```bash
pip install pyinstaller
```

### 5.2. Соберите exe

```bat
python -m PyInstaller --onefile --windowed --add-binary "build/vortex_core.dll;build" --add-data "assets;assets" --add-data "config.json;." vortex.py
```

Результат: `dist\vortex.exe`.  
Этот exe уже самодостаточен и не требует Python или MinGW.

---

## 6. Сборка лаунчера (если используется)

Некоторые версии используют `vortex_launcher.cpp` для запуска `dist\vortex.exe`. Соберите его так:

```bat
g++ vortex_launcher.cpp -o Vortex.exe -municode -mwindows -static -static-libgcc -static-libstdc++ -lole32 -lshell32 -luuid -lshlwapi -lcomctl32
```

Убедитесь, что в коде используется `wWinMain`, иначе будет ошибка линковки.

---

## 7. Сборка установщика

Если вы меняли исходники установщика (`vortex_setup.cpp`), пересоберите его:

```bat
g++ vortex_setup.cpp -o vortex_setup.exe -municode -mwindows -static -static-libgcc -static-libstdc++ -lole32 -lshell32 -luuid -lshlwapi -lcomctl32 -loleaut32 -lurlmon
```

---

## 8. Обновление data-файлов установщика

Установщик использует файлы `data0` и `data1` для распаковки приложения.

1. Удалите старые `data0` и `data1` рядом с `vortex_setup.exe`.
2. Положите рядом с установщиком папку `Vortex`, содержащую:
   - `Vortex.exe` (лаунчер или PyInstaller exe)
   - `dist\vortex.exe` (если используется лаунчер)
   - `build\vortex_core.dll`
   - `assets\`
   - `config.json`
   - `gui\` (если нужно для отладки)
3. Запустите `vortex_setup.exe`. Он автоматически создаст новые `data0`/`data1`.

---

## 9. Тестирование

Проверьте зависимости перед отправкой:

```bat
objdump -p build\vortex_core.dll | findstr "DLL Name"
objdump -p dist\vortex.exe | findstr "DLL Name"
objdump -p vortex_setup.exe | findstr "DLL Name"
```

В выводах не должно быть строк `libgcc_s_seh-1.dll` и `libstdc++-6.dll`.

Также протестируйте на чистой Windows (виртуальная машина или второй ПК), чтобы убедиться, что всё работает.

---

## 10. Работа с Git и веткой `develop`

### 10.1. Создание новой ветки для фичи

```bash
git checkout develop
git checkout -b feature/my-feature
```

### 10.2. Внесение изменений и коммит

```bash
git add .
git commit -m "Описание изменений"
```

### 10.3. Отправка ветки на GitHub

```bash
git push origin feature/my-feature
```

### 10.4. Слияние с `develop`

```bash
git checkout develop
git pull origin develop
git merge feature/my-feature
git push origin develop
```

### 10.5. Релиз в `main`

Когда `develop` стабилен:

```bash
git checkout main
git pull origin main
git merge develop
git push origin main
```

---

## 11. Рекомендации

- **Не коммитьте** файлы `debug_llm.log`, `debug_vortex.log`, `data0`, `data1` (если они генерируются). Добавьте их в `.gitignore`.
- **Проверяйте зависимости** после каждой сборки DLL и exe.
- **Тестируйте на чистой системе** перед релизом.
- **Обновляйте README** при изменении функциональности.

---

Следуя этой инструкции, вы сможете безопасно разрабатывать Vortex, не ломая стабильные версии в `main`. Удачи! 🚀