# gui/minigames.py
import tkinter as tk
import random

class BaseMiniGame(tk.Frame):
    """Базовый класс для мини-игр (для совместимости)."""
    def __init__(self, master, on_close=None):
        super().__init__(master, bg="#1e1e1e")
        self.on_close = on_close
        self.pack(fill=tk.BOTH, expand=True)
        self._create_widgets()
        self._bind_keys()
        self._start()

    def _create_widgets(self):
        pass

    def _bind_keys(self):
        pass

    def _start(self):
        pass

    def close(self):
        if self.on_close:
            self.on_close()
        self.destroy()


class PlatformerShooter(BaseMiniGame):
    """
    Ультракачественная мини-игра: платформер-стрелялка.
    Управление:
        ← → — движение
        Пробел — прыжок
        Z — выстрел
        R — переиграть после смерти
    """
    WIDTH = 800
    HEIGHT = 400
    GROUND_Y = 360
    GRAVITY = 0.6
    JUMP_SPEED = -14
    MOVE_SPEED = 5
    BULLET_SPEED = 10
    ENEMY_SPEED = 2

    def __init__(self, master, on_close=None):
        self.running = True
        self.game_over = False

        # Игровые объекты
        self.player = {
            "x": 50, "y": self.GROUND_Y - 30,
            "w": 30, "h": 30,
            "vx": 0, "vy": 0,
            "on_ground": False,
            "facing": 1,        # 1 = вправо, -1 = влево
            "health": 3,
            "invincible": 0
        }
        self.bullets = []
        self.enemies = []
        self.score = 0

        # Платформы: (x1, y1, x2, y2)
        self.platforms = [
            (0, self.GROUND_Y, self.WIDTH, self.GROUND_Y + 20),
            (100, 280, 250, 300),
            (400, 220, 550, 240),
            (600, 160, 750, 180),
            (300, 120, 450, 140)
        ]

        # Спавн врагов
        self._spawn_enemies()

        super().__init__(master, on_close)

    def _spawn_enemies(self):
        # Несколько врагов на разных высотах
        positions = [(200, self.GROUND_Y - 20), (500, 200), (700, 140)]
        for x, y in positions:
            self.enemies.append({
                "x": x, "y": y,
                "w": 25, "h": 20,
                "dir": random.choice([-1, 1]),
                "alive": True
            })

    def _create_widgets(self):
        self.canvas = tk.Canvas(self, width=self.WIDTH, height=self.HEIGHT,
                                bg="#87CEEB", highlightthickness=0)
        self.canvas.pack(pady=10)

        self.hud = tk.Label(self, text="Здоровье: 3   Очки: 0",
                            fg="#ffffff", bg="#1e1e1e", font=("Segoe UI", 12))
        self.hud.pack()

        self.restart_btn = tk.Button(self, text="Играть снова", command=self._restart,
                                     bg="#2a2a2a", fg="#ffffff", relief=tk.FLAT,
                                     font=("Segoe UI", 10))
        self.restart_btn.pack(pady=5)
        self.restart_btn.pack_forget()  # скрываем до Game Over

        self._draw()

    def _bind_keys(self):
        self.bind_all("<Left>", lambda e: self._key("left", True))
        self.bind_all("<Right>", lambda e: self._key("right", True))
        self.bind_all("<KeyRelease-Left>", lambda e: self._key("left", False))
        self.bind_all("<KeyRelease-Right>", lambda e: self._key("right", False))
        self.bind_all("<space>", lambda e: self._jump())
        self.bind_all("z", lambda e: self._shoot())
        self.bind_all("r", lambda e: self._restart())

    def _key(self, direction, pressed):
        if self.game_over:
            return
        if direction == "left":
            if pressed:
                self.player["vx"] = -self.MOVE_SPEED
                self.player["facing"] = -1
            else:
                if self.player["vx"] < 0:
                    self.player["vx"] = 0
        elif direction == "right":
            if pressed:
                self.player["vx"] = self.MOVE_SPEED
                self.player["facing"] = 1
            else:
                if self.player["vx"] > 0:
                    self.player["vx"] = 0

    def _jump(self):
        if self.game_over:
            return
        if self.player["on_ground"]:
            self.player["vy"] = self.JUMP_SPEED
            self.player["on_ground"] = False

    def _shoot(self):
        if self.game_over:
            return
        # Создаём пулю из центра персонажа
        px = self.player["x"] + self.player["w"]/2
        py = self.player["y"] + self.player["h"]/2
        bullet = {
            "x": px,
            "y": py,
            "dir": self.player["facing"],
            "w": 8, "h": 4
        }
        self.bullets.append(bullet)

    def _start(self):
        self._game_loop()

    def _game_loop(self):
        if self.game_over:
            return
        self._update()
        self._draw()
        self.after(16, self._game_loop)  # ~60 FPS

    def _update(self):
        # Обновление игрока
        p = self.player
        p["x"] += p["vx"]
        p["vy"] += self.GRAVITY
        p["y"] += p["vy"]

        # Границы экрана
        if p["x"] < 0: p["x"] = 0
        if p["x"] + p["w"] > self.WIDTH: p["x"] = self.WIDTH - p["w"]

        # Платформы (только верхнее столкновение)
        p["on_ground"] = False
        for plat in self.platforms:
            x1, y1, x2, y2 = plat
            if (p["x"] + p["w"] > x1 and p["x"] < x2 and
                p["y"] + p["h"] >= y1 and p["y"] + p["h"] <= y1 + 10 and
                p["vy"] >= 0):
                p["y"] = y1 - p["h"]
                p["vy"] = 0
                p["on_ground"] = True

        # Пол
        if p["y"] + p["h"] > self.GROUND_Y:
            p["y"] = self.GROUND_Y - p["h"]
            p["vy"] = 0
            p["on_ground"] = True

        # Неуязвимость после удара
        if p["invincible"] > 0:
            p["invincible"] -= 1

        # Обновление пуль
        for bullet in self.bullets[:]:
            bullet["x"] += bullet["dir"] * self.BULLET_SPEED
            if bullet["x"] < 0 or bullet["x"] > self.WIDTH:
                self.bullets.remove(bullet)

        # Обновление врагов
        for enemy in self.enemies[:]:
            if not enemy["alive"]:
                continue
            enemy["x"] += enemy["dir"] * self.ENEMY_SPEED
            if enemy["x"] < 10 or enemy["x"] + enemy["w"] > self.WIDTH - 10:
                enemy["dir"] *= -1

            # Столкновение с пулей
            for bullet in self.bullets[:]:
                if (bullet["x"] < enemy["x"] + enemy["w"] and
                    bullet["x"] + bullet["w"] > enemy["x"] and
                    bullet["y"] < enemy["y"] + enemy["h"] and
                    bullet["y"] + bullet["h"] > enemy["y"]):
                    enemy["alive"] = False
                    self.score += 1
                    if bullet in self.bullets:
                        self.bullets.remove(bullet)
                    break

            # Столкновение с игроком
            if enemy["alive"] and p["invincible"] == 0:
                if (p["x"] < enemy["x"] + enemy["w"] and
                    p["x"] + p["w"] > enemy["x"] and
                    p["y"] < enemy["y"] + enemy["h"] and
                    p["y"] + p["h"] > enemy["y"]):
                    p["health"] -= 1
                    p["invincible"] = 30  # полсекунды неуязвимости
                    # Отскок
                    if p["x"] < enemy["x"]:
                        p["x"] -= 20
                    else:
                        p["x"] += 20
                    if p["health"] <= 0:
                        self._game_over()
                        return

        # Удаляем мёртвых врагов
        self.enemies = [e for e in self.enemies if e["alive"]]

        # Если все враги убиты — победа
        if not self.enemies:
            self._game_over(win=True)

    def _draw(self):
        self.canvas.delete("all")

        # Платформы
        for plat in self.platforms:
            x1, y1, x2, y2 = plat
            self.canvas.create_rectangle(x1, y1, x2, y2,
                                         fill="#654321", outline="")

        # Враги
        for enemy in self.enemies:
            if enemy["alive"]:
                x, y, w, h = enemy["x"], enemy["y"], enemy["w"], enemy["h"]
                self.canvas.create_rectangle(x, y, x+w, y+h,
                                             fill="#ff5555", outline="")

        # Пули
        for bullet in self.bullets:
            x, y, w, h = bullet["x"], bullet["y"], bullet["w"], bullet["h"]
            self.canvas.create_oval(x-w/2, y-h/2, x+w/2, y+h/2,
                                    fill="#ffff00", outline="")

        # Игрок
        p = self.player
        self.canvas.create_rectangle(p["x"], p["y"], p["x"]+p["w"], p["y"]+p["h"],
                                     fill="#00cc00", outline="")
        # Глаза (направление)
        eye_x = p["x"] + (p["w"]*0.7 if p["facing"] == 1 else p["w"]*0.3)
        self.canvas.create_oval(eye_x-2, p["y"]+5, eye_x+2, p["y"]+9,
                                fill="#000000", outline="")

        # Обновление HUD
        self.hud.config(text=f"Здоровье: {self.player['health']}   Очки: {self.score}")

    def _game_over(self, win=False):
        self.game_over = True
        msg = "Победа!" if win else "Игра окончена"
        self.canvas.create_text(self.WIDTH//2, self.HEIGHT//2,
                                text=f"{msg}\nСчёт: {self.score}",
                                fill="#ffffff", font=("Segoe UI", 20, "bold"),
                                justify="center")
        self.restart_btn.pack()

    def _restart(self):
        # Полный сброс
        self.player = {
            "x": 50, "y": self.GROUND_Y - 30,
            "w": 30, "h": 30,
            "vx": 0, "vy": 0,
            "on_ground": False,
            "facing": 1,
            "health": 3,
            "invincible": 0
        }
        self.bullets = []
        self.score = 0
        self.game_over = False
        self.restart_btn.pack_forget()
        self._spawn_enemies()
        self._draw()
        self._game_loop()


# Для обратной совместимости с прежними вызовами
class SnakeGame(PlatformerShooter):
    """Теперь запускает платформер-стрелялку."""
    pass

class Game2048(PlatformerShooter):
    """Теперь запускает платформер-стрелялку."""
    pass

class TicTacToe(PlatformerShooter):
    """Теперь запускает платформер-стрелялку."""
    pass