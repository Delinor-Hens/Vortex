# gui/chat_widget.py
import tkinter as tk
import base64
import os
import tempfile

class ChatWidget(tk.Text):
    def __init__(self, master, **kwargs):
        super().__init__(master, **kwargs)

        self.tag_configure("user", foreground="#6c5ce7", font=("Segoe UI", 10, "bold"))
        self.tag_configure("assistant", foreground="#e0e0e0", font=("Segoe UI", 10))
        self.tag_configure("system", foreground="#888888", font=("Segoe UI", 9, "italic"))

        self.configure(state=tk.DISABLED, wrap=tk.WORD, bg="#121212", fg="#e0e0e0",
                       insertbackground="#ffffff", selectbackground="#6c5ce7")

        self._animation_job = None
        self._image_refs = []  # чтобы сборщик мусора не удалял изображения

    def clear(self):
        self.configure(state=tk.NORMAL)
        self.delete("1.0", tk.END)
        self.configure(state=tk.DISABLED)
        self._image_refs.clear()

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

    def add_image(self, image_base64):
        """Вставляет изображение в чат (для пользователя)."""
        try:
            image_data = base64.b64decode(image_base64)
            # Сохраняем во временный файл
            temp_file = tempfile.NamedTemporaryFile(delete=False, suffix=".png")
            temp_file.write(image_data)
            temp_file.close()

            photo = tk.PhotoImage(file=temp_file.name)
            self._image_refs.append(photo)  # сохраняем ссылку

            self.configure(state=tk.NORMAL)
            self.image_create(tk.END, image=photo)
            self.insert(tk.END, "\n")
            self.configure(state=tk.DISABLED)
            self.see(tk.END)

            os.unlink(temp_file.name)
        except Exception as e:
            print(f"[DEBUG] Ошибка вставки изображения: {e}")

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

    def start_stream_message(self, sender, tag):
        if self._animation_job:
            self.after_cancel(self._animation_job)
            self._animation_job = None

        self.configure(state=tk.NORMAL)
        self.insert(tk.END, f"{sender}: ", tag)
        self.configure(state=tk.DISABLED)
        self.see(tk.END)

    def append_stream_chunk(self, text, tag):
        self.configure(state=tk.NORMAL)
        self.insert(tk.END, text, tag)
        self.configure(state=tk.DISABLED)
        self.see(tk.END)