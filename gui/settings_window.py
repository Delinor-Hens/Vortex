# gui/settings_window.py
import tkinter as tk
from tkinter import ttk, messagebox
import json
import os
from .theme_window import ThemeWindow

class SettingsWindow(tk.Toplevel):
    def __init__(self, parent, core):
        super().__init__(parent)
        self.parent = parent
        self.core = core

        self.title("Настройки Vortex")
        self.geometry("640x560")
        self.minsize(560, 480)
        self.configure(bg="#121212")
        self.transient(parent)
        self.grab_set()

        self._setup_styles()
        self._build_ui()
        self._load_values()

    def _setup_styles(self):
        style = ttk.Style()
        style.theme_use("clam")

        bg = "#121212"
        fg = "#e0e0e0"
        accent = "#6c5ce7"
        entry_bg = "#1e1e1e"
        button_bg = "#2a2a2a"

        style.configure("TFrame", background=bg)
        style.configure("TLabel", background=bg, foreground=fg)
        style.configure("TButton",
                        background=button_bg,
                        foreground=fg,
                        borderwidth=0,
                        focusthickness=0,
                        padding=(10, 6))
        style.map("TButton",
                  background=[("active", "#3a3a3a"), ("pressed", accent)],
                  foreground=[("pressed", "#ffffff")])
        style.configure("Accent.TButton",
                        background=accent,
                        foreground="#ffffff",
                        borderwidth=0,
                        focusthickness=0,
                        padding=(10, 6))
        style.map("Accent.TButton",
                  background=[("active", "#7d6df0"), ("pressed", "#5a4bd1")])
        style.configure("TEntry",
                        fieldbackground=entry_bg,
                        foreground=fg,
                        insertcolor=fg,
                        bordercolor="#3a3a3a",
                        lightcolor="#3a3a3a",
                        darkcolor="#3a3a3a",
                        padding=6)
        style.configure("TNotebook", background=bg, borderwidth=0)
        style.configure("TNotebook.Tab", background=button_bg, foreground=fg,
                        padding=(10, 4), font=("Segoe UI", 9))
        style.map("TNotebook.Tab",
                  background=[("selected", accent)],
                  foreground=[("selected", "#ffffff")])

    def _build_ui(self):
        self.notebook = ttk.Notebook(self)
        self.notebook.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)

        # Вкладка "Основные"
        self.basic_frame = ttk.Frame(self.notebook)
        self.notebook.add(self.basic_frame, text="Основные")

        ttk.Label(self.basic_frame, text="Режим генерации:").pack(anchor='w', pady=(10,0))
        self.generation_mode_var = tk.StringVar(value="normal")
        ttk.Combobox(self.basic_frame, textvariable=self.generation_mode_var,
                     values=["instant", "normal", "thinking"],
                     state="readonly", width=15).pack(anchor='w', pady=5)

        ttk.Label(self.basic_frame, text="Громкость музыки:").pack(anchor='w', pady=(10,0))
        self.music_volume_var = tk.IntVar(value=100)
        ttk.Scale(self.basic_frame, from_=0, to=100, variable=self.music_volume_var,
                  command=self._update_volume_label).pack(fill=tk.X, pady=5)
        self.volume_label = ttk.Label(self.basic_frame, text="100%")
        self.volume_label.pack(anchor='w')

        ttk.Button(self.basic_frame, text="Темы оформления", command=self._open_theme_window).pack(anchor='w', pady=10)

        ttk.Button(self.basic_frame, text="Сохранить", style="Accent.TButton",
                   command=self._save_basic).pack(anchor='w', pady=20)

        # Вкладка "Провайдеры"
        self.provider_frame = ttk.Frame(self.notebook)
        self.notebook.add(self.provider_frame, text="Провайдеры")

        ttk.Label(self.provider_frame, text="Активный провайдер:").pack(anchor='w', pady=(10,0))
        self.provider_combo = ttk.Combobox(self.provider_frame, state="readonly", width=30)
        self.provider_combo.pack(fill=tk.X, pady=5)
        self.provider_combo.bind("<<ComboboxSelected>>", self._on_provider_selected)

        ttk.Label(self.provider_frame, text="Название:").pack(anchor='w', pady=(10,0))
        self.p_name = ttk.Entry(self.provider_frame)
        self.p_name.pack(fill=tk.X, pady=5)

        ttk.Label(self.provider_frame, text="Base URL:").pack(anchor='w')
        self.p_base = ttk.Entry(self.provider_frame)
        self.p_base.pack(fill=tk.X, pady=5)

        ttk.Label(self.provider_frame, text="API Key:").pack(anchor='w')
        self.p_key = ttk.Entry(self.provider_frame, show="*")
        self.p_key.pack(fill=tk.X, pady=5)

        ttk.Label(self.provider_frame, text="Модель:").pack(anchor='w')
        self.p_model = ttk.Entry(self.provider_frame)
        self.p_model.pack(fill=tk.X, pady=5)

        button_frame = ttk.Frame(self.provider_frame)
        button_frame.pack(fill=tk.X, pady=10)
        ttk.Button(button_frame, text="Сохранить провайдера", command=self._save_provider).pack(side=tk.LEFT)
        ttk.Button(button_frame, text="Сделать активным", command=self._set_active).pack(side=tk.LEFT, padx=5)
        ttk.Button(button_frame, text="Тест", command=self._test_provider).pack(side=tk.LEFT, padx=5)

        self.provider_status = ttk.Label(self.provider_frame, text="", foreground="#888888")
        self.provider_status.pack(anchor='w', pady=5)

    def _load_values(self):
        # Основные
        self.generation_mode_var.set(self.core.get_current_generation_mode() or "normal")
        self.music_volume_var.set(self.parent.music_volume)
        self._update_volume_label()

        # Провайдеры
        self.providers = self.core.get_provider_list()
        if self.providers:
            self.provider_combo['values'] = [f"{p['type']}: {p['name']}" for p in self.providers]
            self.provider_combo.current(0)
            self._on_provider_selected()

    def _update_volume_label(self, *args):
        self.volume_label.config(text=f"{int(self.music_volume_var.get())}%")

    def _save_basic(self):
        self.core.set_generation_mode(self.generation_mode_var.get())
        self.parent.set_music_volume(int(self.music_volume_var.get()))
        messagebox.showinfo("Vortex", "Настройки сохранены")

    def _open_theme_window(self):
        ThemeWindow(self.parent)

    def _on_provider_selected(self, event=None):
        idx = self.provider_combo.current()
        if idx < 0 or not self.providers:
            return
        p = self.providers[idx]
        self.p_name.delete(0, tk.END); self.p_name.insert(0, p.get('name',''))
        self.p_base.delete(0, tk.END); self.p_base.insert(0, p.get('baseUrl',''))
        self.p_key.delete(0, tk.END); self.p_key.insert(0, p.get('apiKey',''))
        self.p_model.delete(0, tk.END); self.p_model.insert(0, p.get('model',''))

    def _save_provider(self):
        idx = self.provider_combo.current()
        if idx < 0:
            return
        p = self.providers[idx]
        self.core.set_provider_config(
            int(p['type']),
            self.p_name.get().strip(),
            self.p_base.get().strip(),
            self.p_key.get().strip(),
            self.p_model.get().strip()
        )
        self.providers = self.core.get_provider_list()
        self.provider_combo['values'] = [f"{x['type']}: {x['name']}" for x in self.providers]
        self.provider_combo.current(idx)
        self.provider_status.config(text="Сохранено", foreground="#6c5ce7")

    def _set_active(self):
        idx = self.provider_combo.current()
        if idx < 0:
            return
        p = self.providers[idx]
        self.core.set_active_provider(int(p['type']))
        self.provider_status.config(text="Активный провайдер обновлён", foreground="#6c5ce7")

    def _test_provider(self):
        # В будущем можно добавить реальный тест
        self.provider_status.config(text="Тест пока не реализован", foreground="#ff5555")