# gui/code_widget.py
import tkinter as tk
from tkinter import ttk, scrolledtext
import threading

class CodeWidget(ttk.Frame):
    def __init__(self, master, core):
        super().__init__(master)
        self.core = core
        self._setup_styles()
        self._build_ui()

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
        # Верхняя панель с описанием задачи и языком
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

        # Кнопки действий
        btn_frame = ttk.Frame(self)
        btn_frame.pack(fill=tk.X, padx=10, pady=5)

        actions = [
            ("Генерировать код", self._generate_code),
            ("Объяснить код", self._explain_code),
            ("Исправить ошибки", self._fix_code),
            ("Оптимизировать", self._optimize_code)
        ]
        for text, cmd in actions:
            ttk.Button(btn_frame, text=text, command=cmd, style="Code.TButton").pack(side=tk.LEFT, padx=2)

        # Область ввода/вставки кода
        code_frame = ttk.LabelFrame(self, text="Ваш код", style="Code.TLabelframe")
        code_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=5)

        self.code_input = scrolledtext.ScrolledText(code_frame, wrap=tk.WORD,
                                                    font=("Consolas", 10),
                                                    bg="#1e1e1e", fg="#e0e0e0",
                                                    insertbackground="#ffffff")
        self.code_input.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)

        # Область вывода результата
        output_frame = ttk.LabelFrame(self, text="Результат", style="Code.TLabelframe")
        output_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=(0,10))

        self.output_text = scrolledtext.ScrolledText(output_frame, wrap=tk.WORD,
                                                     font=("Consolas", 10),
                                                     bg="#1a1a1a", fg="#d4d4d4",
                                                     insertbackground="#ffffff",
                                                     state=tk.DISABLED)
        self.output_text.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)

        # Индикатор выполнения
        self.progress = ttk.Progressbar(self, mode='indeterminate')
        self.progress.pack(fill=tk.X, padx=10, pady=(0,5))

    def _set_output(self, text):
        self.output_text.config(state=tk.NORMAL)
        self.output_text.delete("1.0", tk.END)
        self.output_text.insert("1.0", text)
        self.output_text.config(state=tk.DISABLED)

    def _run_action(self, mode):
        # Собираем запрос
        language = self.language_var.get()
        task = self.task_entry.get().strip()
        code = self.code_input.get("1.0", tk.END).strip()

        if not code and not task:
            self._set_output("Введите код или описание задачи.")
            return

        # Формируем промпт в зависимости от режима
        if mode == "generate":
            prompt = f"[Задача: генерация кода]\nЯзык: {language}\nОписание: {task}\nОтвет представь только кодом без пояснений."
        elif mode == "explain":
            prompt = f"[Задача: объяснение кода]\nЯзык: {language}\nКод:\n{code}\nОбъясни что делает этот код."
        elif mode == "fix":
            prompt = f"[Задача: исправление кода]\nЯзык: {language}\nКод:\n{code}\nНайди и исправь ошибки, верни исправленный код."
        elif mode == "optimize":
            prompt = f"[Задача: оптимизация кода]\nЯзык: {language}\nКод:\n{code}\nОптимизируй код и объясни изменения."

        # Блокируем кнопки и запускаем поток
        self.progress.start(10)
        threading.Thread(target=self._worker, args=(prompt,), daemon=True).start()

    def _worker(self, prompt):
        try:
            response = self.core.send_message(prompt)
            if response is None:
                response = "[Ошибка: не удалось получить ответ]"
            self.after(0, self._on_response, response)
        except Exception as e:
            self.after(0, self._on_response, f"[Ошибка: {e}]")

    def _on_response(self, text):
        self.progress.stop()
        self._set_output(text)

    def _generate_code(self):
        self._run_action("generate")

    def _explain_code(self):
        self._run_action("explain")

    def _fix_code(self):
        self._run_action("fix")

    def _optimize_code(self):
        self._run_action("optimize")