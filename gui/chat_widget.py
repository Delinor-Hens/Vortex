# gui/chat_widget.py
import tkinter as tk
import threading

class ChatWidget(tk.Text):
    def __init__(self, master, **kwargs):
        super().__init__(master, **kwargs)

        self.tag_configure("user", foreground="#6c5ce7", font=("Segoe UI", 10, "bold"))
        self.tag_configure("assistant", foreground="#e0e0e0", font=("Segoe UI", 10))
        self.tag_configure("system", foreground="#888888", font=("Segoe UI", 9, "italic"))

        self.configure(state=tk.DISABLED, wrap=tk.WORD, bg="#121212", fg="#e0e0e0",
                       insertbackground="#ffffff", selectbackground="#6c5ce7")

        self._animation_job = None

    def clear(self):
        self.configure(state=tk.NORMAL)
        self.delete("1.0", tk.END)
        self.configure(state=tk.DISABLED)

    def append_message(self, sender, text, tag, animated=False):
        if animated:
            self._animate_text(sender, text, tag)
        else:
            self._insert_instant(sender, text, tag)

    def load_history(self, history):
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
        self.configure(state=tk.NORMAL)
        self.insert(tk.END, f"{sender}: ", tag)
        self.insert(tk.END, text + "\n\n", tag)
        self.configure(state=tk.DISABLED)
        self.see(tk.END)

    def _animate_text(self, sender, text, tag):
        if self._animation_job:
            self.after_cancel(self._animation_job)
            self._animation_job = None

        self.configure(state=tk.NORMAL)
        self.insert(tk.END, f"{sender}: ", tag)
        self.configure(state=tk.DISABLED)
        self.see(tk.END)

        self._animate_chars(text, tag, 0)

    def _animate_chars(self, text, tag, index):
        if index >= len(text):
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

        delay = 20
        self._animation_job = self.after(delay, self._animate_chars, text, tag, index+1)

    # ---------- Методы для потоковой передачи ----------
    def start_stream_message(self, sender, tag):
        """Начинает новое сообщение от ассистента для потокового вывода."""
        if self._animation_job:
            self.after_cancel(self._animation_job)
            self._animation_job = None

        self.configure(state=tk.NORMAL)
        self.insert(tk.END, f"{sender}: ", tag)
        self.configure(state=tk.DISABLED)
        self.see(tk.END)

    def append_stream_chunk(self, text, tag):
        """Добавляет очередной чанк к текущему сообщению."""
        self.configure(state=tk.NORMAL)
        self.insert(tk.END, text, tag)
        self.configure(state=tk.DISABLED)
        self.see(tk.END)