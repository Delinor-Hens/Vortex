# gui/chat_widget.py
import tkinter as tk
from tkinter import scrolledtext
import threading
import time

class ChatWidget(tk.Text):
    def __init__(self, master, **kwargs):
        super().__init__(master, **kwargs)

        # Настройка тегов для форматирования
        self.tag_configure("user", foreground="#6c5ce7", font=("Segoe UI", 10, "bold"))
        self.tag_configure("assistant", foreground="#e0e0e0", font=("Segoe UI", 10))
        self.tag_configure("system", foreground="#888888", font=("Segoe UI", 9, "italic"))

        self.configure(state=tk.DISABLED, wrap=tk.WORD, bg="#121212", fg="#e0e0e0",
                       insertbackground="#ffffff", selectbackground="#6c5ce7")

        self._animation_job = None

    def clear(self):
        """Очистить виджет."""
        self.configure(state=tk.NORMAL)
        self.delete("1.0", tk.END)
        self.configure(state=tk.DISABLED)

    def append_message(self, sender, text, tag, animated=False):
        """Добавить сообщение с опциональной анимацией."""
        if animated:
            self._animate_text(sender, text, tag)
        else:
            self._insert_instant(sender, text, tag)

    def load_history(self, history):
        """Загрузить историю сообщений (без анимации)."""
        self.clear()
        for msg in history:
            role = msg.get('role', '')
            content = msg.get('content', '')
            if role == 'user':
                self._insert_instant("Вы", content, "user")
            elif role == 'assistant':
                self._insert_instant("Vortex", content, "assistant")
            else:
                self._insert_instant("Система", content, "system")

    def _insert_instant(self, sender, text, tag):
        """Мгновенная вставка сообщения."""
        self.configure(state=tk.NORMAL)
        self.insert(tk.END, f"{sender}: ", tag)
        self.insert(tk.END, text + "\n\n", tag)
        self.configure(state=tk.DISABLED)
        self.see(tk.END)

    def _animate_text(self, sender, text, tag):
        """Анимация посимвольного вывода."""
        # Отменяем предыдущую анимацию, если она была
        if self._animation_job:
            self.after_cancel(self._animation_job)
            self._animation_job = None

        # Вставляем метку отправителя сразу
        self.configure(state=tk.NORMAL)
        self.insert(tk.END, f"{sender}: ", tag)
        self.configure(state=tk.DISABLED)
        self.see(tk.END)

        # Запускаем посимвольный вывод
        self._animate_chars(text, tag, 0)

    def _animate_chars(self, text, tag, index):
        """Рекурсивно выводит символы с небольшой задержкой."""
        if index >= len(text):
            # Завершение: добавляем два перевода строки
            self.configure(state=tk.NORMAL)
            self.insert(tk.END, "\n\n", tag)
            self.configure(state=tk.DISABLED)
            self.see(tk.END)
            self._animation_job = None
            return

        self.configure(state=tk.NORMAL)
        self.insert(tk.END, text[index], tag)
        self.configure(state=tk.DISABLED)
        self.see(tk.END)

        # Задержка между символами (можно регулировать)
        delay = 20  # миллисекунды
        self._animation_job = self.after(delay, self._animate_chars, text, tag, index+1)