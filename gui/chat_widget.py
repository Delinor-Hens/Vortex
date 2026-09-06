import tkinter as tk
from tkinter import ttk
import re

class ChatWidget(tk.Frame):
    def __init__(self, master, **kwargs):
        super().__init__(master, bg="#121212", **kwargs)
        self._create_widgets()
        self._configure_tags()

    def _create_widgets(self):
        self.text = tk.Text(
            self,
            bg="#1e1e1e",
            fg="#e0e0e0",
            insertbackground="#e0e0e0",
            relief=tk.FLAT,
            wrap=tk.WORD,
            font=("Segoe UI", 11),
            padx=12,
            pady=10,
            state=tk.DISABLED
        )
        scrollbar = ttk.Scrollbar(self, orient=tk.VERTICAL, command=self.text.yview)
        self.text.configure(yscrollcommand=scrollbar.set)
        self.text.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        scrollbar.pack(side=tk.RIGHT, fill=tk.Y)

    def _configure_tags(self):
        self.text.tag_configure("user", foreground="#4fc3f7", spacing1=1, spacing3=1,
                                font=("Segoe UI Semibold", 11))
        self.text.tag_configure("assistant", foreground="#e0e0e0", spacing1=1, spacing3=1)
        self.text.tag_configure("system", foreground="#d4d4d4", spacing1=1, spacing3=1)
        self.text.tag_configure("bold", font=("Segoe UI", 11, "bold"))
        self.text.tag_configure("italic", font=("Segoe UI", 11, "italic"))
        self.text.tag_configure("code", font=("Consolas", 10), background="#2d2d2d",
                                foreground="#e0e0e0")
        self.text.tag_configure("heading", font=("Segoe UI Semibold", 14, "bold"),
                                foreground="#ffffff", spacing1=4, spacing3=2)

    def clear(self):
        self.text.configure(state=tk.NORMAL)
        self.text.delete("1.0", tk.END)
        self.text.configure(state=tk.DISABLED)

    def _decode_escapes(self, s):
        replacements = {
            '\\n': '\n',
            '\\t': '\t',
            '\\r': '\r',
            '\\"': '"',
            '\\\\': '\\',
            '\\u003c': '<',
            '\\u003e': '>',
            '\\u003C': '<',
            '\\u003E': '>',
        }
        for old, new in replacements.items():
            s = s.replace(old, new)
        return s

    def append_message(self, sender, content, tag, animated=False):
        self.text.configure(state=tk.NORMAL)
        self.text.insert(tk.END, f"{sender}: ", tag)
        content = self._decode_escapes(content)
        if animated:
            self._animate_text(content, tag)
        else:
            self._insert_markdown(content)
            self.text.insert(tk.END, "\n", tag)
            self.text.configure(state=tk.DISABLED)
            self.text.see(tk.END)

    def _animate_text(self, content, tag, index=0):
        if index >= len(content):
            self.text.insert(tk.END, "\n", tag)
            self.text.configure(state=tk.DISABLED)
            self.text.see(tk.END)
            return
        self.text.insert(tk.END, content[index], tag)
        self.text.see(tk.END)
        self.after(1, lambda: self._animate_text(content, tag, index+1))

    def _insert_markdown(self, text):
        lines = text.split('\n')
        for line in lines:
            if line.startswith('### '):
                self.text.insert(tk.END, line[4:], "heading")
            elif line.startswith('## '):
                self.text.insert(tk.END, line[3:], "heading")
            elif line.startswith('# '):
                self.text.insert(tk.END, line[2:], "heading")
            elif line.startswith('- '):
                self.text.insert(tk.END, "• " + line[2:], "assistant")
            else:
                self._insert_inline_markdown(line)
            self.text.insert(tk.END, "\n")

    def _insert_inline_markdown(self, text):
        pattern = r'(\*\*.*?\*\*|\*.*?\*|`.*?`)'
        parts = re.split(pattern, text)
        for part in parts:
            if not part:
                continue
            if part.startswith('**') and part.endswith('**'):
                self.text.insert(tk.END, part[2:-2], "bold")
            elif part.startswith('`') and part.endswith('`'):
                self.text.insert(tk.END, part[1:-1], "code")
            elif part.startswith('*') and part.endswith('*'):
                self.text.insert(tk.END, part[1:-1], "italic")
            else:
                self.text.insert(tk.END, part)

    def load_history(self, history):
        self.clear()
        for msg in history:
            sender = "Вы" if msg['role'] == 'user' else "Vortex"
            tag = "user" if msg['role'] == 'user' else "assistant"
            self.append_message(sender, msg['content'], tag)