# gui/theme_window.py
import tkinter as tk
from tkinter import ttk

class ThemeWindow(tk.Toplevel):
    def __init__(self, parent):
        super().__init__(parent)
        self.parent = parent
        self.title("Темы оформления")
        self.geometry("400x300")
        self.configure(bg="#121212")
        self.transient(parent)
        self.grab_set()

        # Предустановленные темы
        self.themes = {
            "Тёмная": {
                "bg": "#121212",
                "fg": "#e0e0e0",
                "accent": "#6c5ce7",
                "entry": "#1e1e1e",
                "button": "#2a2a2a"
            },
            "Светлая": {
                "bg": "#f0f0f0",
                "fg": "#333333",
                "accent": "#6c5ce7",
                "entry": "#ffffff",
                "button": "#dddddd"
            },
            "OLED": {
                "bg": "#000000",
                "fg": "#ffffff",
                "accent": "#6c5ce7",
                "entry": "#1a1a1a",
                "button": "#0a0a0a"
            },
            "Синяя": {
                "bg": "#0d1b2a",
                "fg": "#e0e0e0",
                "accent": "#1b98e0",
                "entry": "#1b263b",
                "button": "#1b263b"
            }
        }

        # Заголовок
        ttk.Label(self, text="Выберите тему:", font=("Segoe UI", 12)).pack(pady=10)

        # Радиокнопки для выбора темы
        self.theme_var = tk.StringVar(value=parent.current_theme)
        for theme_name in self.themes.keys():
            ttk.Radiobutton(
                self,
                text=theme_name,
                variable=self.theme_var,
                value=theme_name,
                command=self._apply_theme
            ).pack(anchor='w', padx=20, pady=5)

    def _apply_theme(self):
        theme = self.themes[self.theme_var.get()]
        self.parent.apply_theme(theme)