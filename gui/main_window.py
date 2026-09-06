# gui/main_window.py
import tkinter as tk
from tkinter import ttk, messagebox
import threading
import ctypes
import os
import sys
import json
import random
from .core_wrapper import VortexCore
from .chat_widget import ChatWidget
from .code_widget import CodeWidget
from .minigames import SnakeGame, Game2048, TicTacToe
from .theme_window import ThemeWindow

class MainWindow(tk.Tk):
    def __init__(self, core: VortexCore):
        super().__init__()
        self.core = core
        self.title("Vortex 2.1.12")
        self.geometry("860x640")
        self.minsize(720, 520)
        self.configure(bg="#121212")
        self.protocol("WM_DELETE_WINDOW", self.on_close)

        self.current_chat_id = -1
        self.music_volume = 100
        self.warmup_done = False
        self.current_theme = "Тёмная"

        self.minigame_window = None
        self.minigame_after_id = None

        self._load_music_volume()
        self._setup_styles()
        self._build_ui()
        self._refresh_chats()
        self._start_background_music()
        self._apply_music_volume()
        self.state('zoomed')

        try:
            icon_path = os.path.join(self._get_app_dir(), 'assets', 'logo.ico')
            if not os.path.exists(icon_path):
                icon_path = self._resource_path(os.path.join('assets', 'logo.ico'))
            self.iconbitmap(default=icon_path)
        except:
            pass

        self._show_loading_overlay()
        self._set_input_state(False)
        self._start_warmup()

    # ---------- Вспомогательные методы ----------
    def _resource_path(self, relative_path):
        if hasattr(sys, '_MEIPASS'):
            return os.path.join(sys._MEIPASS, relative_path)
        return os.path.join(os.path.dirname(__file__), '..', relative_path)

    def _get_app_dir(self):
        if getattr(sys, 'frozen', False):
            return os.path.dirname(sys.executable)
        return os.path.dirname(os.path.abspath(__file__))

    def _load_music_volume(self):
        try:
            config_path = os.path.join(self._get_app_dir(), 'config.json')
            with open(config_path, 'r', encoding='utf-8') as f:
                config = json.load(f)
                self.music_volume = int(config.get('music_volume', 100))
                self.music_volume = max(0, min(100, self.music_volume))
        except:
            self.music_volume = 100

    def _save_music_volume(self):
        try:
            config_path = os.path.join(self._get_app_dir(), 'config.json')
            with open(config_path, 'w', encoding='utf-8') as f:
                json.dump({'music_volume': self.music_volume}, f)
        except:
            pass

    def _start_background_music(self):
        music_file = os.path.join(self._get_app_dir(), 'assets', 'music.mp3')
        if not os.path.exists(music_file):
            music_file = self._resource_path(os.path.join('assets', 'music.mp3'))
        if not os.path.exists(music_file):
            return
        try:
            ctypes.windll.winmm.mciSendStringW(f'open "{music_file}" type mpegvideo alias vortex_music', None, 0, None)
            ctypes.windll.winmm.mciSendStringW('play vortex_music repeat', None, 0, None)
        except:
            pass

    def _apply_music_volume(self):
        volume = int(self.music_volume * 10)
        volume = max(0, min(1000, volume))
        try:
            ctypes.windll.winmm.mciSendStringW(f'setaudio vortex_music volume to {volume}', None, 0, None)
        except:
            pass

    def set_music_volume(self, volume):
        self.music_volume = max(0, min(100, volume))
        self._apply_music_volume()
        self._save_music_volume()

    def _stop_background_music(self):
        try:
            ctypes.windll.winmm.mciSendStringW('stop vortex_music', None, 0, None)
            ctypes.windll.winmm.mciSendStringW('close vortex_music', None, 0, None)
        except:
            pass

    def _setup_styles(self):
        style = ttk.Style()
        style.theme_use("clam")

        bg = "#121212"
        fg = "#e0e0e0"
        accent = "#6c5ce7"
        accent_hover = "#7d6df0"
        entry_bg = "#1e1e1e"
        button_bg = "#2a2a2a"
        button_hover = "#3a3a3a"

        style.configure("TFrame", background=bg)
        style.configure("TLabel", background=bg, foreground=fg)
        style.configure("TButton",
                        background=button_bg,
                        foreground=fg,
                        borderwidth=0,
                        focusthickness=0,
                        padding=(10, 6),
                        font=("Segoe UI", 10))
        style.map("TButton",
                  background=[("active", button_hover), ("pressed", accent)],
                  foreground=[("pressed", "#ffffff")])
        style.configure("Accent.TButton",
                        background=accent,
                        foreground="#ffffff",
                        borderwidth=0,
                        focusthickness=0,
                        padding=(10, 6),
                        font=("Segoe UI", 10))
        style.map("Accent.TButton",
                  background=[("active", accent_hover), ("pressed", "#5a4bd1")])
        style.configure("TEntry",
                        fieldbackground=entry_bg,
                        foreground=fg,
                        insertcolor=fg,
                        bordercolor="#3a3a3a",
                        lightcolor="#3a3a3a",
                        darkcolor="#3a3a3a",
                        padding=6)
        style.configure("TCombobox",
                        fieldbackground=entry_bg,
                        background=button_bg,
                        foreground=fg,
                        arrowcolor=fg,
                        bordercolor="#3a3a3a",
                        lightcolor="#3a3a3a",
                        darkcolor="#3a3a3a",
                        padding=5)
        style.map("TCombobox",
                  fieldbackground=[("readonly", entry_bg)],
                  foreground=[("readonly", fg)])

    def apply_theme(self, theme):
        """Применяет тему ко всему интерфейсу."""
        self.current_theme = theme.get("name", "Custom")
        bg = theme["bg"]
        fg = theme["fg"]
        accent = theme["accent"]
        entry_bg = theme["entry"]
        button_bg = theme["button"]

        self.configure(bg=bg)

        style = ttk.Style()
        style.configure("TFrame", background=bg)
        style.configure("TLabel", background=bg, foreground=fg)
        style.configure("TButton", background=button_bg, foreground=fg)
        style.map("TButton",
                  background=[("active", accent), ("pressed", accent)],
                  foreground=[("pressed", "#ffffff")])
        style.configure("Accent.TButton", background=accent, foreground="#ffffff")
        style.map("Accent.TButton",
                  background=[("active", accent), ("pressed", accent)])
        style.configure("TEntry", fieldbackground=entry_bg, foreground=fg, insertcolor=fg)
        style.configure("TCombobox", fieldbackground=entry_bg, background=button_bg, foreground=fg)

        if hasattr(self, 'chat_widget'):
            self.chat_widget.configure(bg=bg, fg=fg)
        if hasattr(self, 'model_label'):
            self.model_label.configure(foreground=fg)

    def _build_ui(self):
        self.notebook = ttk.Notebook(self)
        self.notebook.pack(fill=tk.BOTH, expand=True)

        # Вкладка "Чат"
        self.chat_frame = ttk.Frame(self.notebook)
        self.notebook.add(self.chat_frame, text="Чат")

        # Вкладка "Код"
        self.code_widget = CodeWidget(self.notebook, self.core)
        self.notebook.add(self.code_widget, text="Код")

        self._build_chat_ui()

        self.progress = ttk.Progressbar(self, mode='indeterminate', length=200)
        self.progress.pack(side=tk.BOTTOM, anchor=tk.W, padx=12, pady=(0,6))

    def _build_chat_ui(self):
        top = ttk.Frame(self.chat_frame)
        top.pack(fill=tk.X, padx=12, pady=8)

        self.chat_combo = ttk.Combobox(top, state="readonly", width=25)
        self.chat_combo.pack(side=tk.LEFT, fill=tk.X, expand=True)
        self.chat_combo.bind("<<ComboboxSelected>>", self._on_chat_selected)

        ttk.Button(top, text="Новый", command=self._new_chat).pack(side=tk.LEFT, padx=(8,0))
        ttk.Button(top, text="Удалить", command=self._delete_chat).pack(side=tk.LEFT, padx=(8,0))
        ttk.Button(top, text="Очистить", command=self._clear_chat).pack(side=tk.LEFT, padx=(8,0))
        ttk.Button(top, text="Настройки", command=self._open_settings).pack(side=tk.LEFT, padx=(8,0))

        self.model_label = ttk.Label(self.chat_frame, text="Модель: Vortex 2.1.12",
                                     foreground="#888888", font=("Segoe UI", 9))
        self.model_label.pack(side=tk.TOP, anchor=tk.E, padx=12, pady=(0,5))

        self.chat_widget = ChatWidget(self.chat_frame)
        self.chat_widget.pack(fill=tk.BOTH, expand=True, padx=12, pady=5)

        bottom = ttk.Frame(self.chat_frame)
        bottom.pack(fill=tk.X, padx=12, pady=8)

        self.entry = ttk.Entry(bottom, font=("Segoe UI", 11))
        self.entry.pack(side=tk.LEFT, fill=tk.X, expand=True, ipady=3)
        self.entry.bind("<Return>", lambda e: self._send())
        self.entry.focus_set()

        self.send_button = ttk.Button(bottom, text="Отправить",
                                      style="Accent.TButton", command=self._send)
        self.send_button.pack(side=tk.LEFT, padx=(8,0))

        mode_frame = ttk.Frame(self.chat_frame)
        mode_frame.pack(fill=tk.X, padx=12, pady=(0, 5))

        ttk.Label(mode_frame, text="Режим:", font=("Segoe UI", 9)).pack(side=tk.LEFT, padx=(0, 5))
        self.mode_buttons = {}
        self.mode_var = tk.StringVar(value="normal")
        for text, mode in [("Instant", "instant"), ("Normal", "normal"), ("Thinking", "thinking")]:
            btn = ttk.Button(mode_frame, text=text,
                             command=lambda m=mode: self._set_generation_mode(m))
            btn.pack(side=tk.LEFT, padx=2)
            self.mode_buttons[mode] = btn

        self._update_mode_buttons()

    # ---------- Логика чата ----------
    def _set_generation_mode(self, mode):
        self.core.set_generation_mode(mode)
        self.mode_var.set(mode)
        self._update_mode_buttons()

    def _update_mode_buttons(self):
        current = self.core.get_current_generation_mode() or "normal"
        for mode, btn in self.mode_buttons.items():
            if mode == current:
                btn.configure(style="Accent.TButton")
            else:
                btn.configure(style="TButton")

    # ---------- Экран загрузки ----------
    def _show_loading_overlay(self):
        self.loading_overlay = tk.Frame(self, bg="#121212")
        self.loading_overlay.place(relx=0, rely=0, relwidth=1, relheight=1)
        self.loading_overlay.lift()

        label = tk.Label(self.loading_overlay, text="V", font=("Segoe UI", 100, "bold"),
                         fg="#6c5ce7", bg="#121212")
        label.pack(expand=True)

        self.loading_text = tk.Label(self.loading_overlay, text="Загрузка модели...",
                                     font=("Segoe UI", 14), fg="#e0e0e0", bg="#121212")
        self.loading_text.pack(pady=10)
        self._animate_loading_text()

    def _animate_loading_text(self):
        if not hasattr(self, 'loading_overlay'):
            return
        current = self.loading_text.cget("text")
        dots = current.split("...")[0].count(".")
        dots = (dots + 1) % 4
        self.loading_text.config(text=f"Загрузка модели{'.' * dots}")
        self.after(400, self._animate_loading_text)

    def _hide_loading_overlay(self):
        if hasattr(self, 'loading_overlay'):
            self.loading_overlay.destroy()
            del self.loading_overlay
        self._set_input_state(True)

    def _set_input_state(self, enabled):
        state = tk.NORMAL if enabled else tk.DISABLED
        self.entry.config(state=state)
        self.send_button.config(state=state)

    def _start_warmup(self):
        def warmup():
            try:
                self.core.warmup()
            except Exception as e:
                print(f"[DEBUG] Warmup error: {e}")
            finally:
                self.warmup_done = True
                self.after(0, self._hide_loading_overlay)
        threading.Thread(target=warmup, daemon=True).start()

    # ---------- Прогресс-бар ----------
    def _start_progress(self):
        self.progress.start(15)

    def _stop_progress(self):
        self.progress.stop()

    # ---------- Чаты ----------
    def _refresh_chats(self):
        try:
            print("[DEBUG] Запрос списка чатов...")
            chats = self.core.get_chats()
            print(f"[DEBUG] Получено чатов: {chats}")
            if not chats:
                print("[DEBUG] Чатов нет, создаём Default")
                self.core.create_chat("Default")
                chats = self.core.get_chats()
                print(f"[DEBUG] После создания: {chats}")
            self.chat_list = chats
            self.chat_combo['values'] = [f"{i}: {c['name']}" for i, c in enumerate(chats)]
            if chats:
                self.chat_combo.current(0)
                self.current_chat_id = 0
                self.core.select_chat(0)
                print("[DEBUG] Выбран чат с id=0")
                self._load_history()
        except Exception as e:
            print(f"[DEBUG] Ошибка загрузки чатов: {e}")
            messagebox.showerror("Ошибка", f"Не удалось загрузить чаты: {e}")

    def _load_history(self):
        print("[DEBUG] Загрузка истории...")
        history = self.core.get_history()
        print(f"[DEBUG] История содержит {len(history)} сообщений")
        self.chat_widget.load_history(history)

    def _append_message(self, sender, text, tag, animated=False):
        self.chat_widget.append_message(sender, text, tag, animated)

    def _on_chat_selected(self, event=None):
        sel = self.chat_combo.current()
        print(f"[DEBUG] Выбран чат из комбобокса: index={sel}")
        if sel >= 0:
            self.current_chat_id = sel
            self.core.select_chat(sel)
            self._load_history()

    def _new_chat(self):
        try:
            print("[DEBUG] Создание нового чата...")
            new_id = self.core.create_chat()
            print(f"[DEBUG] Новый чат id={new_id}")
            self._refresh_chats()
            self.chat_combo.current(new_id)
            self.current_chat_id = new_id
            self.core.select_chat(new_id)
            self._load_history()
        except Exception as e:
            print(f"[DEBUG] Ошибка создания чата: {e}")
            messagebox.showerror("Ошибка", str(e))

    def _delete_chat(self):
        if self.current_chat_id < 0:
            return
        if messagebox.askyesno("Подтверждение", "Удалить текущий чат?"):
            print(f"[DEBUG] Удаление чата id={self.current_chat_id}")
            if self.core.delete_chat(self.current_chat_id) == 0:
                self._refresh_chats()
            else:
                print("[DEBUG] Ошибка удаления чата")
                messagebox.showerror("Ошибка", "Не удалось удалить чат")

    def _clear_chat(self):
        if self.current_chat_id < 0:
            return
        if not messagebox.askyesno("Очистить чат", "Удалить все сообщения в текущем чате?"):
            return
        print(f"[DEBUG] Очистка чата id={self.current_chat_id}")
        if self.core.clear_chat(self.current_chat_id) != 0:
            print("[DEBUG] Ошибка очистки чата")
            messagebox.showerror("Ошибка", "Не удалось очистить чат")
            return
        self._load_history()

    def _open_settings(self):
        from .settings_window import SettingsWindow
        SettingsWindow(self, self.core)

    # ---------- Отправка сообщений ----------
    def _send(self):
        if not self.warmup_done:
            return
        print(f"[DEBUG] Нажата кнопка отправки, current_chat_id={self.current_chat_id}")
        if self.current_chat_id < 0:
            print("[DEBUG] Нет активного чата, пробуем создать")
            self._refresh_chats()
            if self.current_chat_id < 0:
                messagebox.showwarning("Внимание", "Нет активного чата")
                return
        msg = self.entry.get().strip()
        print(f"[DEBUG] Текст сообщения: '{msg}'")
        if not msg:
            print("[DEBUG] Пустое сообщение")
            return
        self.entry.delete(0, tk.END)
        self._append_message("Вы", msg, "user")
        self._start_progress()
        self.send_button.config(state=tk.DISABLED)
        print("[DEBUG] Запуск потока отправки...")
        threading.Thread(target=self._send_worker, args=(msg,), daemon=True).start()

    def _send_worker(self, msg):
        self._start_minigame_timer()
        try:
            # Проверяем провайдера
            if self.core.get_active_provider() == 0:  # Ollama
                self.core.start_stream(msg)
                self.after(0, self._start_stream_message)
                self.after(50, self._poll_stream_chunks)
            else:
                response = self.core.send_message(msg)
                if response is None:
                    response = "[Ошибка: не удалось получить ответ]"
                self.after(0, self._on_response_received, response)
        except Exception as e:
            print(f"[DEBUG] Ошибка в потоке: {e}")
            self.after(0, self._on_error, str(e))
        finally:
            if self.minigame_after_id:
                self.after_cancel(self.minigame_after_id)
                self.minigame_after_id = None
            if self.minigame_window:
                self._on_minigame_close()

    def _start_stream_message(self):
        self.chat_widget.start_stream_message("Vortex", "assistant")

    def _poll_stream_chunks(self):
        if not self.core.is_generating():
            # Проверяем остаток чанков
            chunk = self.core.get_stream_chunk()
            if chunk:
                self.chat_widget.append_stream_chunk(chunk, "assistant")
            # Добавляем пустую строку после сообщения ассистента
            self.chat_widget.append_stream_chunk("\n\n", "assistant")
            self.send_button.config(state=tk.NORMAL)
            self._stop_progress()
            return

        chunk = self.core.get_stream_chunk()
        if chunk:
            self.chat_widget.append_stream_chunk(chunk, "assistant")

        self.after(50, self._poll_stream_chunks)

    def _on_response_received(self, response):
        print("[DEBUG] Ответ получен в UI")
        self._stop_progress()
        self._append_message("Vortex", response, "assistant", animated=True)
        self.send_button.config(state=tk.NORMAL)

    def _on_error(self, error_msg):
        print(f"[DEBUG] Обработка ошибки: {error_msg}")
        self._stop_progress()
        self._append_message("Vortex", f"[Ошибка: {error_msg}]", "system")
        self.send_button.config(state=tk.NORMAL)

    # ---------- Мини-игры ----------
    def _start_minigame_timer(self):
        self.minigame_after_id = self.after(15000, self._show_minigame)

    def _show_minigame(self):
        if self.minigame_window is not None:
            return
        self.minigame_window = tk.Toplevel(self)
        self.minigame_window.title("Vortex ждёт")
        self.minigame_window.configure(bg="#1e1e1e")
        self.minigame_window.geometry("+%d+%d" % (self.winfo_rootx()+100, self.winfo_rooty()+100))
        self.minigame_window.transient(self)
        self.minigame_window.protocol("WM_DELETE_WINDOW", self._on_minigame_close)

        info_label = tk.Label(self.minigame_window, text="Модель готовит ответ. Вы можете пока сыграть в мини-игру",
                              fg="#ffffff", bg="#1e1e1e", font=("Segoe UI", 11))
        info_label.pack(pady=10)

        games = [SnakeGame, Game2048, TicTacToe]
        game_class = random.choice(games)
        game = game_class(self.minigame_window, on_close=self._on_minigame_close)
        game.pack()

    def _on_minigame_close(self):
        if self.minigame_window:
            self.minigame_window.destroy()
            self.minigame_window = None

    def on_close(self):
        print("[DEBUG] Закрытие приложения")
        self._stop_background_music()
        try:
            self.core.shutdown()
        except:
            pass
        self.destroy()