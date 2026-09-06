import tkinter as tk
from tkinter import ttk, messagebox
from .core_wrapper import VortexCore

class SettingsWindow(tk.Toplevel):
    def __init__(self, master, core: VortexCore):
        super().__init__(master)
        self.master = master  # сохраняем ссылку на главное окно
        self.core = core
        self.title("Настройки")
        self.geometry("350x280")
        self.configure(bg="#1e1e1e")
        self.resizable(False, False)
        self.transient(master)
        self.grab_set()

        self._setup_styles()
        self._build_ui()
        self._load_current()

    def _setup_styles(self):
        style = ttk.Style()
        style.theme_use("clam")
        style.configure("TFrame", background="#1e1e1e")
        style.configure("TLabel", background="#1e1e1e", foreground="#ffffff")
        style.configure("TButton", background="#2d2d2d", foreground="#ffffff", borderwidth=0)
        style.map("TButton", background=[("active", "#3e3e3e")])
        style.configure("TCombobox", fieldbackground="#2d2d2d", foreground="#ffffff", arrowcolor="#ffffff")

    def _build_ui(self):
        main_frame = ttk.Frame(self)
        main_frame.pack(fill=tk.BOTH, expand=True, padx=20, pady=20)

        # Режим генерации
        ttk.Label(main_frame, text="Режим генерации:").grid(row=0, column=0, sticky=tk.W, pady=(0,5))
        self.mode_combo = ttk.Combobox(main_frame, state="readonly", width=25,
                                       values=["default", "creative", "formal", "concise"])
        self.mode_combo.grid(row=1, column=0, sticky=tk.EW, pady=(0,20))

        # Громкость музыки
        ttk.Label(main_frame, text="Громкость музыки:").grid(row=2, column=0, sticky=tk.W, pady=(0,5))
        self.volume_scale = tk.Scale(main_frame, from_=0, to=100, orient=tk.HORIZONTAL,
                             bg="#1e1e1e", fg="#ffffff", troughcolor="#2d2d2d",
                             highlightthickness=0)  
        self.volume_scale.bind("<ButtonRelease-1>", lambda e: self._on_volume_change(self.volume_scale.get()))
        self.volume_scale.grid(row=3, column=0, sticky=tk.EW, pady=(0,20))

        # Кнопки
        button_frame = ttk.Frame(main_frame)
        button_frame.grid(row=4, column=0, sticky=tk.E)
        ttk.Button(button_frame, text="Сохранить", command=self._save).pack(side=tk.LEFT, padx=(0,5))
        ttk.Button(button_frame, text="Отмена", command=self.destroy).pack(side=tk.LEFT)

    def _on_volume_change(self, value):
        # Мгновенно применяем громкость
        if hasattr(self.master, 'set_music_volume'):
            self.master.set_music_volume(int(value))

    def _load_current(self):
        try:
            current_mode = self.core.get_current_mode()
            if current_mode in ["default", "creative", "formal", "concise"]:
                self.mode_combo.set(current_mode)
            else:
                self.mode_combo.set("default")
            # Установить ползунок на текущую громкость из главного окна
            if hasattr(self.master, 'music_volume'):
                self.volume_scale.set(self.master.music_volume)
            else:
                self.volume_scale.set(100)
        except:
            self.mode_combo.set("default")
            self.volume_scale.set(50)

    def _save(self):
        mode = self.mode_combo.get()
        volume = self.volume_scale.get()
        try:
            self.core.set_mode(mode)
            # Сохранить громкость
            if hasattr(self.master, 'set_music_volume'):
                self.master.set_music_volume(int(volume))
            messagebox.showinfo("Успех", "Настройки сохранены")
            self.destroy()
        except Exception as e:
            messagebox.showerror("Ошибка", str(e))