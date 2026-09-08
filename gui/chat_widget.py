# gui/chat_widget.py
import tkinter as tk
import base64
import os
import tempfile
import re

class ChatWidget(tk.Text):
    def __init__(self, master, **kwargs):
        super().__init__(master, **kwargs)

        self.tag_configure("user", foreground="#6c5ce7", font=("Segoe UI", 10, "bold"))
        self.tag_configure("assistant", foreground="#e0e0e0", font=("Segoe UI", 10))
        self.tag_configure("system", foreground="#888888", font=("Segoe UI", 9, "italic"))

        self.tag_configure("bold", font=("Segoe UI", 10, "bold"))
        self.tag_configure("italic", font=("Segoe UI", 10, "italic"))
        self.tag_configure("code", font=("Consolas", 9), background="#2a2a2a", foreground="#e0e0e0")
        self.tag_configure("code_block", font=("Consolas", 9), background="#1a1a1a",
                           foreground="#d4d4d4", lmargin1=20, lmargin2=20)
        self.tag_configure("formula", font=("Segoe UI", 10, "bold"), foreground="#ffaa00")

        self.tag_configure("h1", font=("Segoe UI", 16, "bold"), foreground="#ffffff")
        self.tag_configure("h2", font=("Segoe UI", 14, "bold"), foreground="#ffffff")
        self.tag_configure("h3", font=("Segoe UI", 12, "bold"), foreground="#ffffff")
        self.tag_configure("h4", font=("Segoe UI", 11, "bold"), foreground="#ffffff")
        self.tag_configure("h5", font=("Segoe UI", 10, "bold"), foreground="#ffffff")
        self.tag_configure("h6", font=("Segoe UI", 10, "bold"), foreground="#dddddd")

        self.configure(state=tk.DISABLED, wrap=tk.WORD, bg="#121212", fg="#e0e0e0",
                       insertbackground="#ffffff", selectbackground="#6c5ce7")

        self._animation_job = None
        self._image_refs = []

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
        try:
            image_data = base64.b64decode(image_base64)
            temp_file = tempfile.NamedTemporaryFile(delete=False, suffix=".png")
            temp_file.write(image_data)
            temp_file.close()

            photo = tk.PhotoImage(file=temp_file.name)
            self._image_refs.append(photo)

            self.configure(state=tk.NORMAL)
            self.image_create(tk.END, image=photo)
            self.insert(tk.END, "\n")
            self.configure(state=tk.DISABLED)
            self.see(tk.END)

            os.unlink(temp_file.name)
        except Exception as e:
            print(f"[DEBUG] Ошибка вставки изображения: {e}")

    def _insert_markdown(self, text):
        text = self._clean_artifacts(text)
        self.configure(state=tk.NORMAL)

        text = self._process_dollar_blocks(text)

        lines = text.split('\n')
        i = 0
        while i < len(lines):
            line = lines[i]

            if line.strip().startswith("```"):
                code_lines = []
                i += 1
                while i < len(lines) and not lines[i].strip().startswith("```"):
                    code_lines.append(lines[i])
                    i += 1
                if i < len(lines):
                    i += 1
                self.insert(tk.END, "\n".join(code_lines) + "\n", "code_block")
                self.insert(tk.END, "\n", "assistant")
                continue

            header_match = re.match(r'^(#{1,6})\s+(.*)', line)
            if header_match:
                level = len(header_match.group(1))
                header_text = header_match.group(2)
                tag = f"h{level}"
                self.insert(tk.END, header_text + "\n", tag)
                i += 1
                continue

            self._insert_inline_markdown(line)
            self.insert(tk.END, "\n")
            i += 1

        self.configure(state=tk.DISABLED)
        self.see(tk.END)

    def _clean_artifacts(self, text):
        # Удаляем все управляющие символы (кроме \n, \t, \r)
        text = re.sub(r'[\x00-\x08\x0B\x0C\x0E-\x1F]', '', text)
        # Двойные слэши -> одинарные
        text = text.replace('\\\\', '\\')
        # Восстанавливаем потерянные слэши для LaTeX-команд
        replacements = {
            'rac{': r'\frac{',
            'sqrt{': r'\sqrt{',
            'pm': r'\pm',
            'cdot': r'\cdot',
            'leq': r'\leq',
            'geq': r'\geq',
            'neq': r'\neq',
            'times': r'\times',
            'div': r'\div',
            'infty': r'\infty',
            'alpha': r'\alpha',
            'beta': r'\beta',
            'gamma': r'\gamma',
            'delta': r'\delta',
            'pi': r'\pi',
            'theta': r'\theta',
            'lambda': r'\lambda',
            'mu': r'\mu',
            'sigma': r'\sigma',
            'omega': r'\omega',
        }
        for bad, good in replacements.items():
            text = text.replace(bad, good)
        return text

    def _process_dollar_blocks(self, text):
        pattern = r'(\${1,10})(.*?)\1'
        def repl(m):
            content = m.group(2)
            return f"⟦formula⟧{content}⟦/formula⟧"
        text = re.sub(pattern, repl, text, flags=re.DOTALL)
        text = text.replace('$', '')
        return text

    def _insert_inline_markdown(self, text):
        pos = 0
        n = len(text)
        while pos < n:
            formula_start = text.find("⟦formula⟧", pos)
            if formula_start != -1:
                formula_end = text.find("⟦/formula⟧", formula_start + len("⟦formula⟧"))
                if formula_end != -1:
                    if formula_start > pos:
                        self._insert_inline_markdown(text[pos:formula_start])
                    content = text[formula_start + len("⟦formula⟧"):formula_end]
                    self.insert(tk.END, content, "formula")
                    pos = formula_end + len("⟦/formula⟧")
                    continue
                else:
                    self.insert(tk.END, text[pos:], "assistant")
                    return

            bold_start = text.find("**", pos)
            italic_start = text.find("*", pos)
            code_start = text.find("`", pos)

            candidates = []
            if bold_start != -1: candidates.append(bold_start)
            if italic_start != -1: candidates.append(italic_start)
            if code_start != -1: candidates.append(code_start)

            if not candidates:
                self.insert(tk.END, text[pos:], "assistant")
                return

            start = min(candidates)

            if start > pos:
                self.insert(tk.END, text[pos:start], "assistant")

            if bold_start == start:
                end = text.find("**", start + 2)
                if end != -1:
                    content = text[start+2:end]
                    self.insert(tk.END, content, "bold")
                    pos = end + 2
                else:
                    self.insert(tk.END, text[start:start+2], "assistant")
                    pos = start + 2
            elif italic_start == start:
                end = text.find("*", start + 1)
                if end != -1 and end != start+1:
                    content = text[start+1:end]
                    self.insert(tk.END, content, "italic")
                    pos = end + 1
                else:
                    self.insert(tk.END, "*", "assistant")
                    pos = start + 1
            elif code_start == start:
                end = text.find("`", start + 1)
                if end != -1:
                    content = text[start+1:end]
                    self.insert(tk.END, content, "code")
                    pos = end + 1
                else:
                    self.insert(tk.END, "`", "assistant")
                    pos = start + 1

        if pos < n:
            self.insert(tk.END, text[pos:], "assistant")

    def _insert_instant(self, sender, text, tag):
        self.configure(state=tk.NORMAL)
        self.insert(tk.END, f"{sender}: ", tag)
        self.configure(state=tk.DISABLED)
        self.see(tk.END)

        if tag == "assistant":
            self._insert_markdown(text)
        else:
            self.configure(state=tk.NORMAL)
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
        text = self._clean_artifacts(text)
        self.configure(state=tk.NORMAL)
        self.insert(tk.END, text, tag)
        self.configure(state=tk.DISABLED)
        self.see(tk.END)