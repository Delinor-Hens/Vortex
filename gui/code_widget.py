# gui/code_widget.py
import tkinter as tk
from tkinter import ttk, scrolledtext, messagebox, filedialog
import threading
import os
import subprocess
import tempfile
import shutil
import time
import re

class CodeWidget(ttk.Frame):
    def __init__(self, master, core, app_dir=None):
        super().__init__(master)
        self.core = core
        if app_dir is None:
            if hasattr(master, '_get_app_dir'):
                self.app_dir = master._get_app_dir()
            else:
                self.app_dir = os.path.dirname(os.path.abspath(__file__))
        else:
            self.app_dir = app_dir

        self.current_project_dir = None
        self.current_file_path = None
        self.last_run_result = None
        self.last_run_error = None

        self._setup_styles()
        self._build_ui()
        self._refresh_projects()

    def _setup_styles(self):
        style = ttk.Style()
        style.configure("Code.TButton",
                        background="#2a2a2a",
                        foreground="#e0e0e0",
                        borderwidth=0,
                        focusthickness=0,
                        padding=(8, 4))
        style.map("Code.TButton",
                  background=[("active", "#3a3a3a"), ("pressed", "#6c5ce7")])
        style.configure("Code.TLabelframe",
                        background="#121212",
                        foreground="#e0e0e0",
                        bordercolor="#333333")

    def _build_ui(self):
        top = ttk.Frame(self)
        top.pack(fill=tk.X, padx=10, pady=8)

        ttk.Label(top, text="Язык:").pack(side=tk.LEFT, padx=(0,5))
        self.language_var = tk.StringVar(value="Python")
        language_combo = ttk.Combobox(top, textvariable=self.language_var,
                                      values=["Python", "JavaScript", "C++", "C#", "Java",
                                              "Go", "Rust", "TypeScript", "PHP", "Ruby",
                                              "SQL", "HTML", "CSS"],
                                      state="readonly", width=12)
        language_combo.pack(side=tk.LEFT, padx=(0,10))

        ttk.Label(top, text="Задача:").pack(side=tk.LEFT, padx=(0,5))
        self.task_entry = ttk.Entry(top, width=40)
        self.task_entry.pack(side=tk.LEFT, fill=tk.X, expand=True)

        # Панель выбора проекта
        project_frame = ttk.Frame(self)
        project_frame.pack(fill=tk.X, padx=10, pady=5)

        ttk.Label(project_frame, text="Проекты:").pack(side=tk.LEFT, padx=(0,5))
        self.project_combo = ttk.Combobox(project_frame, state="readonly", width=25)
        self.project_combo.pack(side=tk.LEFT, fill=tk.X, expand=True, padx=(0,5))
        self.project_combo.bind("<<ComboboxSelected>>", lambda e: self._load_project())

        ttk.Button(project_frame, text="🔄", width=3, command=self._refresh_projects).pack(side=tk.LEFT, padx=2)
        ttk.Button(project_frame, text="Открыть", command=self._load_project).pack(side=tk.LEFT, padx=2)
        ttk.Button(project_frame, text="Сохранить", command=self._save_current_file).pack(side=tk.LEFT, padx=2)
        ttk.Button(project_frame, text="▶ Запустить", command=self._run_selected_project).pack(side=tk.LEFT, padx=2)
        ttk.Button(project_frame, text="🗑 Удалить", command=self._delete_selected_project).pack(side=tk.LEFT, padx=2)

        # Кнопки генерации и запуска
        btn_frame = ttk.Frame(self)
        btn_frame.pack(fill=tk.X, padx=10, pady=5)

        ttk.Button(btn_frame, text="🤖 Сгенерировать проект", command=self._generate_project, style="Code.TButton").pack(side=tk.LEFT, padx=2)
        ttk.Button(btn_frame, text="▶ Запустить код из поля", command=self._run_code, style="Code.TButton").pack(side=tk.LEFT, padx=2)
        self.fix_button = ttk.Button(btn_frame, text="🛠 Исправить", command=self._on_fix_clicked, style="Code.TButton")
        self.fix_button.pack(side=tk.LEFT, padx=2)
        self.fix_button.state(['disabled'])

        # Редактор кода
        code_frame = ttk.LabelFrame(self, text="Код (для открытого файла или вставки)", style="Code.TLabelframe")
        code_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=5)

        self.code_input = scrolledtext.ScrolledText(code_frame, wrap=tk.WORD,
                                                    font=("Consolas", 10),
                                                    bg="#1e1e1e", fg="#e0e0e0",
                                                    insertbackground="#ffffff")
        self.code_input.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)

        # Вывод отчёта
        output_frame = ttk.LabelFrame(self, text="Отчёт агента", style="Code.TLabelframe")
        output_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=(0,10))

        self.output_text = scrolledtext.ScrolledText(output_frame, wrap=tk.WORD,
                                                     font=("Consolas", 10),
                                                     bg="#1a1a1a", fg="#d4d4d4",
                                                     insertbackground="#ffffff",
                                                     state=tk.DISABLED)
        self.output_text.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)

        self.progress = ttk.Progressbar(self, mode='indeterminate')
        self.progress.pack(fill=tk.X, padx=10, pady=(0,5))

    def _append_output(self, text, end='\n'):
        self.output_text.config(state=tk.NORMAL)
        self.output_text.insert(tk.END, text + end)
        self.output_text.config(state=tk.DISABLED)
        self.output_text.see(tk.END)

    def _set_output(self, text):
        self.output_text.config(state=tk.NORMAL)
        self.output_text.delete("1.0", tk.END)
        self.output_text.insert("1.0", text)
        self.output_text.config(state=tk.DISABLED)
        self.output_text.see(tk.END)

    # ---------- Управление проектами ----------
    def _get_projects_dir(self):
        path = os.path.join(self.app_dir, 'Projects')
        os.makedirs(path, exist_ok=True)
        return path

    def _refresh_projects(self):
        projects_dir = self._get_projects_dir()
        items = [d for d in os.listdir(projects_dir) if os.path.isdir(os.path.join(projects_dir, d))]
        items.sort()
        self.project_combo['values'] = items
        if items:
            self.project_combo.current(0)
        else:
            self.project_combo.set('')

    def _load_project(self):
        if not self.project_combo.get():
            return
        projects_dir = self._get_projects_dir()
        project_dir = os.path.join(projects_dir, self.project_combo.get())
        if not os.path.isdir(project_dir):
            messagebox.showerror("Vortex", "Папка проекта не найдена.")
            return
        self.current_project_dir = project_dir
        main_file = self._find_main_file(project_dir)
        if main_file:
            self.current_file_path = main_file
            with open(main_file, 'r', encoding='utf-8') as f:
                content = f.read()
            self.code_input.delete("1.0", tk.END)
            self.code_input.insert("1.0", content)
            self._append_output(f"Открыт файл: {os.path.basename(main_file)}")
            self.last_run_result = None
            self.last_run_error = None
            self.fix_button.state(['disabled'])
        else:
            self.current_file_path = None
            self.code_input.delete("1.0", tk.END)
            self._append_output("В проекте не найден главный файл.")
            self.fix_button.state(['disabled'])

    def _find_main_file(self, project_dir):
        priority_names = ['main.py', 'app.py', 'run.py', 'main.js', 'app.js', 'index.js',
                          'main.cpp', 'app.cpp', 'Main.java', 'Program.cs', 'main.go', 'main.rs']
        for name in priority_names:
            path = os.path.join(project_dir, name)
            if os.path.isfile(path):
                return path
        for entry in os.listdir(project_dir):
            full = os.path.join(project_dir, entry)
            if os.path.isfile(full):
                return full
        return None

    def _save_current_file(self):
        if not self.current_file_path:
            messagebox.showinfo("Vortex", "Сначала откройте файл проекта.")
            return
        content = self.code_input.get("1.0", tk.END)
        with open(self.current_file_path, 'w', encoding='utf-8') as f:
            f.write(content)
        self._append_output(f"Файл сохранён: {os.path.basename(self.current_file_path)}")

    def _run_selected_project(self):
        if not self.current_project_dir:
            messagebox.showinfo("Vortex", "Сначала откройте проект.")
            return
        language = self.language_var.get()
        main_file = self._find_main_file(self.current_project_dir)
        if not main_file:
            self._append_output("❌ Не найден исполняемый файл.")
            return
        self._append_output(f"▶ Запуск проекта: {os.path.basename(main_file)}")
        self.progress.start(10)
        threading.Thread(target=self._run_project_worker, args=(main_file, language, ''), daemon=True).start()

    def _run_project_worker(self, file_path, language, input_data=''):
        try:
            result = self._run_file(file_path, language, input_data)
            self.last_run_result = result
            if result and ("Traceback" in result or "Error" in result or "SyntaxError" in result or "Exception" in result):
                self.last_run_error = True
                self.fix_button.state(['!disabled'])
            else:
                self.last_run_error = False
                self.fix_button.state(['disabled'])
            self.after(0, self._append_output, result if result else "Проект завершён без вывода.")
        finally:
            self.after(0, lambda: self.progress.stop())

    def _delete_selected_project(self):
        if not self.project_combo.get():
            return
        if not messagebox.askyesno("Vortex", "Удалить выбранный проект и все его файлы?"):
            return
        projects_dir = self._get_projects_dir()
        project_dir = os.path.join(projects_dir, self.project_combo.get())
        if os.path.isdir(project_dir):
            shutil.rmtree(project_dir, ignore_errors=True)
            self._refresh_projects()
            self.code_input.delete("1.0", tk.END)
            self.current_project_dir = None
            self.current_file_path = None
            self.last_run_result = None
            self.last_run_error = None
            self.fix_button.state(['disabled'])
            self._append_output("🗑 Проект удалён.")

    # ---------- Агентский режим с потоковой генерацией ----------
    def _generate_project(self):
        task = self.task_entry.get().strip()
        language = self.language_var.get()
        if not task:
            self._set_output("Опишите проект.")
            return

        self._set_output("🤖 Агент начал работу...\n")
        self.progress.start(10)
        threading.Thread(target=self._agent_worker, args=(task, language), daemon=True).start()

    def _agent_worker(self, task, language):
        previous_mode = self.core.get_current_generation_mode()  # сохраняем режим
        try:
            self.core.set_generation_mode("thinking")  # принудительно включаем thinking для длинного ответа
            self.after(0, self._append_output, "📝 Генерация кода...\n")

            prompt = (
                f"Ты — AI-ассистент. Напиши полный код программы на языке {language}, "
                f"решающий следующую задачу: {task}\n\n"
                f"Верни ТОЛЬКО сам код. Без markdown-обёрток, без пояснений, без комментариев вне кода. "
                f"Никакого текста до или после кода."
            )

            if self.core.get_active_provider() == 0:  # Ollama
                code = self._generate_with_stream(prompt)
            else:
                code = self.core.send_message(prompt)

            if not code or code.startswith("[Ошибка:"):
                self.after(0, self._append_output, f"❌ Ошибка при генерации: {code if code else 'пустой ответ'}")
                return

            code = re.sub(r'^```[a-zA-Z0-9_+-]*\s*\n', '', code, count=1)
            code = re.sub(r'\n```\s*$', '', code, count=1)
            if '```' in code:
                parts = code.split('```')
                if len(parts) >= 3:
                    code = parts[1]
                elif len(parts) == 2:
                    code = parts[0]
            code = code.strip()

            if not code:
                self.after(0, self._append_output, "❌ Получен пустой код после очистки.")
                return

            file_ext = {
                "Python": "py", "JavaScript": "js", "C++": "cpp",
                "C#": "cs", "Java": "java", "Go": "go",
                "Rust": "rs", "TypeScript": "ts", "PHP": "php",
                "Ruby": "rb", "SQL": "sql", "HTML": "html", "CSS": "css"
            }.get(language, "txt")
            main_filename = f"main.{file_ext}"

            projects_dir = self._get_projects_dir()
            project_dir = os.path.join(projects_dir, f"project_{int(time.time())}")
            os.makedirs(project_dir, exist_ok=True)

            file_path = os.path.join(project_dir, main_filename)
            with open(file_path, 'w', encoding='utf-8') as f:
                f.write(code)

            self.after(0, self._append_output, f"✅ Код сохранён в {main_filename}")

            # Вставка кода в редактор
            self.after(0, lambda: self.code_input.delete("1.0", tk.END))
            self.after(0, lambda: self.code_input.insert("1.0", code))
            self.after(0, self._append_output, "📄 Код загружен в редактор.")
            self.after(0, self._append_output, "🏁 Генерация завершена. Запустите проект вручную.")

            # Обновляем список проектов и выбираем новый
            self.after(0, self._refresh_projects)
            self.after(0, lambda: self.project_combo.set(os.path.basename(project_dir)))
            self.after(0, self._load_project)

        except Exception as e:
            self.after(0, self._append_output, f"❌ Ошибка агента: {e}")
        finally:
            if previous_mode:
                self.core.set_generation_mode(previous_mode)  # восстанавливаем режим
            self.after(0, lambda: self.progress.stop())

    def _generate_with_stream(self, prompt):
        full_code = []
        self.core.start_stream(prompt)

        buffer = []
        last_flush = time.time()
        while self.core.is_generating():
            chunk = self.core.get_stream_chunk()
            if chunk:
                full_code.append(chunk)
                buffer.append(chunk)
                if time.time() - last_flush > 0.1:
                    flush_text = ''.join(buffer)
                    if flush_text:
                        self.after(0, self._append_output, flush_text, end='')
                    buffer.clear()
                    last_flush = time.time()
            else:
                time.sleep(0.005)

        while True:
            chunk = self.core.get_stream_chunk()
            if not chunk:
                break
            full_code.append(chunk)
            buffer.append(chunk)

        if buffer:
            self.after(0, self._append_output, ''.join(buffer), end='')

        self.after(0, self._append_output, "\n", end='')
        return ''.join(full_code)

    # ---------- Ручное исправление кода ----------
    def _on_fix_clicked(self):
        if not self.current_file_path:
            messagebox.showinfo("Vortex", "Сначала откройте проект с ошибкой.")
            return
        if not self.last_run_error or not self.last_run_result:
            messagebox.showinfo("Vortex", "Нет сохранённой ошибки для исправления.")
            return
        language = self.language_var.get()
        self.progress.start(10)
        threading.Thread(target=self._fix_worker, args=(self.current_file_path, language, self.last_run_result), daemon=True).start()

    def _fix_worker(self, file_path, language, error_text):
        previous_mode = self.core.get_current_generation_mode()
        try:
            self.core.set_generation_mode("thinking")
            self.after(0, self._append_output, "🛠 Запрос на исправление...")

            error_prompt = (
                f"Твоя программа на {language} выдала ошибку:\n{error_text}\n\n"
                f"Исправь код и верни ТОЛЬКО исправленный код. Без пояснений, без markdown-обёрток."
            )

            if self.core.get_active_provider() == 0:
                fixed = self._generate_with_stream(error_prompt)
            else:
                fixed = self.core.send_message(error_prompt)

            if not fixed or fixed.startswith("[Ошибка:"):
                self.after(0, self._append_output, f"❌ Не удалось получить исправление: {fixed if fixed else 'пустой ответ'}")
                return

            fixed = re.sub(r'^```[a-zA-Z0-9_+-]*\s*\n', '', fixed, count=1)
            fixed = re.sub(r'\n```\s*$', '', fixed, count=1)
            if '```' in fixed:
                parts = fixed.split('```')
                if len(parts) >= 3:
                    fixed = parts[1]
            fixed = fixed.strip()

            if not fixed:
                self.after(0, self._append_output, "❌ Исправленный код пуст.")
                return

            with open(file_path, 'w', encoding='utf-8') as f:
                f.write(fixed)

            self.after(0, lambda: self.code_input.delete("1.0", tk.END))
            self.after(0, lambda: self.code_input.insert("1.0", fixed))

            self.after(0, self._append_output, "🔧 Код обновлён. Повторный запуск...")
            run_result = self._run_file(file_path, language, '')

            self.last_run_result = run_result
            if run_result and ("Traceback" in run_result or "Error" in run_result or "SyntaxError" in run_result or "Exception" in run_result):
                self.last_run_error = True
                self.fix_button.state(['!disabled'])
            else:
                self.last_run_error = False
                self.fix_button.state(['disabled'])

            self.after(0, self._append_output, f"🔄 Результат повторного запуска:\n{run_result if run_result else 'Программа завершилась без вывода.'}")

        except Exception as e:
            self.after(0, self._append_output, f"❌ Ошибка при исправлении: {e}")
        finally:
            if previous_mode:
                self.core.set_generation_mode(previous_mode)
            self.after(0, lambda: self.progress.stop())

    # ---------- Запуск файла ----------
    def _run_file(self, file_path, language, input_data=''):
        try:
            if language == "Python":
                proc = subprocess.run(['python', file_path],
                                      input=input_data,
                                      capture_output=True,
                                      text=True,
                                      timeout=10,
                                      cwd=os.path.dirname(file_path))
                return proc.stdout + proc.stderr
            elif language == "JavaScript":
                proc = subprocess.run(['node', file_path],
                                      input=input_data,
                                      capture_output=True,
                                      text=True,
                                      timeout=10,
                                      cwd=os.path.dirname(file_path))
                return proc.stdout + proc.stderr
            elif language == "C++":
                exe_path = os.path.join(os.path.dirname(file_path), 'app.exe')
                compile_cmd = f'g++ "{file_path}" -o "{exe_path}"'
                compile_result = subprocess.run(compile_cmd, shell=True, capture_output=True, timeout=10, cwd=os.path.dirname(file_path))
                if compile_result.returncode != 0:
                    return compile_result.stderr
                if os.path.exists(exe_path):
                    proc = subprocess.run([exe_path],
                                          input=input_data,
                                          capture_output=True,
                                          text=True,
                                          timeout=10,
                                          cwd=os.path.dirname(file_path))
                    return proc.stdout + proc.stderr
            elif language == "Java":
                compile_cmd = f'javac "{file_path}"'
                compile_result = subprocess.run(compile_cmd, shell=True, capture_output=True, timeout=10, cwd=os.path.dirname(file_path))
                if compile_result.returncode != 0:
                    return compile_result.stderr
                class_name = os.path.splitext(os.path.basename(file_path))[0]
                proc = subprocess.run(['java', '-cp', os.path.dirname(file_path), class_name],
                                      input=input_data,
                                      capture_output=True,
                                      text=True,
                                      timeout=10,
                                      cwd=os.path.dirname(file_path))
                return proc.stdout + proc.stderr
            elif language == "C#":
                proc = subprocess.run(['dotnet', 'run', '--project', os.path.dirname(file_path)],
                                      input=input_data,
                                      capture_output=True,
                                      text=True,
                                      timeout=10,
                                      cwd=os.path.dirname(file_path))
                return proc.stdout + proc.stderr
            elif language == "Go":
                proc = subprocess.run(['go', 'run', file_path],
                                      input=input_data,
                                      capture_output=True,
                                      text=True,
                                      timeout=10,
                                      cwd=os.path.dirname(file_path))
                return proc.stdout + proc.stderr
            elif language == "Rust":
                exe_path = os.path.join(os.path.dirname(file_path), 'app.exe')
                compile_cmd = f'rustc "{file_path}" -o "{exe_path}"'
                compile_result = subprocess.run(compile_cmd, shell=True, capture_output=True, timeout=10, cwd=os.path.dirname(file_path))
                if compile_result.returncode != 0:
                    return compile_result.stderr
                if os.path.exists(exe_path):
                    proc = subprocess.run([exe_path],
                                          input=input_data,
                                          capture_output=True,
                                          text=True,
                                          timeout=10,
                                          cwd=os.path.dirname(file_path))
                    return proc.stdout + proc.stderr
        except subprocess.TimeoutExpired:
            return "Превышено время выполнения."
        except FileNotFoundError:
            return "Не найден интерпретатор для запуска."
        except Exception as e:
            return f"Ошибка запуска: {e}"
        return None

    # ---------- Запуск кода из поля ----------
    def _run_code(self):
        language = self.language_var.get()
        code = self.code_input.get("1.0", tk.END).strip()
        if not code:
            self._set_output("Нет кода для запуска.")
            return

        temp_dir = tempfile.mkdtemp()
        try:
            ext = {"Python":"py","JavaScript":"js","C++":"cpp","C#":"cs","Java":"java","Go":"go","Rust":"rs"}.get(language, "txt")
            file_path = os.path.join(temp_dir, f"script.{ext}")
            with open(file_path, 'w', encoding='utf-8') as f:
                f.write(code)
            result = self._run_file(file_path, language, '')
            self._set_output(result if result else "Программа завершилась без вывода.")
        except Exception as e:
            self._set_output(f"Ошибка: {e}")
        finally:
            shutil.rmtree(temp_dir, ignore_errors=True)