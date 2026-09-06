# gui/minigames.py
import tkinter as tk
import random

class BaseMiniGame(tk.Frame):
    """Базовый класс для мини-игр."""
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


class SnakeGame(BaseMiniGame):
    """Классическая змейка."""
    CELL_SIZE = 20
    GRID_W = 20
    GRID_H = 15

    def __init__(self, master, on_close=None):
        self.snake = [(5, 5), (4, 5), (3, 5)]
        self.direction = "Right"
        self.food = None
        self.running = True
        self.score = 0
        super().__init__(master, on_close)

    def _create_widgets(self):
        self.canvas = tk.Canvas(self, width=self.GRID_W*self.CELL_SIZE,
                                height=self.GRID_H*self.CELL_SIZE,
                                bg="#000000", highlightthickness=0)
        self.canvas.pack(pady=10)
        self.score_label = tk.Label(self, text="Score: 0", fg="#ffffff", bg="#1e1e1e",
                                    font=("Segoe UI", 12))
        self.score_label.pack()
        self._spawn_food()

    def _bind_keys(self):
        self.bind_all("<Up>", lambda e: self._change_direction("Up"))
        self.bind_all("<Down>", lambda e: self._change_direction("Down"))
        self.bind_all("<Left>", lambda e: self._change_direction("Left"))
        self.bind_all("<Right>", lambda e: self._change_direction("Right"))

    def _start(self):
        self._update()

    def _spawn_food(self):
        while True:
            x = random.randint(0, self.GRID_W-1)
            y = random.randint(0, self.GRID_H-1)
            if (x, y) not in self.snake:
                self.food = (x, y)
                break

    def _change_direction(self, new_dir):
        opposites = {"Up": "Down", "Down": "Up", "Left": "Right", "Right": "Left"}
        if new_dir != opposites.get(self.direction, ""):
            self.direction = new_dir

    def _update(self):
        if not self.running:
            return
        head = self.snake[0]
        if self.direction == "Right":
            new_head = (head[0]+1, head[1])
        elif self.direction == "Left":
            new_head = (head[0]-1, head[1])
        elif self.direction == "Up":
            new_head = (head[0], head[1]-1)
        else:
            new_head = (head[0], head[1]+1)

        if (new_head[0] < 0 or new_head[0] >= self.GRID_W or
            new_head[1] < 0 or new_head[1] >= self.GRID_H or
            new_head in self.snake):
            self.running = False
            self._game_over()
            return

        self.snake.insert(0, new_head)
        if new_head == self.food:
            self.score += 1
            self.score_label.config(text=f"Score: {self.score}")
            self._spawn_food()
        else:
            self.snake.pop()

        self._draw()
        self.after(150, self._update)

    def _draw(self):
        self.canvas.delete("all")
        for x, y in self.snake:
            self.canvas.create_rectangle(x*self.CELL_SIZE, y*self.CELL_SIZE,
                                         (x+1)*self.CELL_SIZE, (y+1)*self.CELL_SIZE,
                                         fill="#00ff00", outline="")
        if self.food:
            x, y = self.food
            self.canvas.create_oval(x*self.CELL_SIZE, y*self.CELL_SIZE,
                                    (x+1)*self.CELL_SIZE, (y+1)*self.CELL_SIZE,
                                    fill="#ff0000", outline="")

    def _game_over(self):
        self.canvas.delete("all")
        self.canvas.create_text(self.GRID_W*self.CELL_SIZE//2,
                                self.GRID_H*self.CELL_SIZE//2,
                                text=f"Игра окончена\nСчёт: {self.score}",
                                fill="#ffffff", font=("Segoe UI", 16),
                                justify="center")
        self.after(2000, self.close)


class Game2048(BaseMiniGame):
    """Упрощённый 2048."""
    SIZE = 4
    CELL_SIZE = 80
    COLORS = {
        0: "#cdc1b4", 2: "#eee4da", 4: "#ede0c8", 8: "#f2b179",
        16: "#f59563", 32: "#f67c5f", 64: "#f65e3b", 128: "#edcf72",
        256: "#edcc61", 512: "#edc850", 1024: "#edc53f", 2048: "#edc22e"
    }

    def __init__(self, master, on_close=None):
        self.board = [[0]*self.SIZE for _ in range(self.SIZE)]
        self.running = True
        super().__init__(master, on_close)

    def _create_widgets(self):
        self.canvas = tk.Canvas(self, width=self.SIZE*self.CELL_SIZE,
                                height=self.SIZE*self.CELL_SIZE,
                                bg="#bbada0", highlightthickness=0)
        self.canvas.pack(pady=10)
        self._add_random_tile()
        self._add_random_tile()
        self._draw()

    def _bind_keys(self):
        self.bind_all("<Up>", lambda e: self._move("up"))
        self.bind_all("<Down>", lambda e: self._move("down"))
        self.bind_all("<Left>", lambda e: self._move("left"))
        self.bind_all("<Right>", lambda e: self._move("right"))

    def _start(self):
        pass

    def _add_random_tile(self):
        empty = [(r,c) for r in range(self.SIZE) for c in range(self.SIZE) if self.board[r][c]==0]
        if empty:
            r,c = random.choice(empty)
            self.board[r][c] = 2 if random.random() < 0.9 else 4

    def _draw(self):
        self.canvas.delete("all")
        for r in range(self.SIZE):
            for c in range(self.SIZE):
                val = self.board[r][c]
                color = self.COLORS.get(val, "#cdc1b4")
                self.canvas.create_rectangle(c*self.CELL_SIZE, r*self.CELL_SIZE,
                                             (c+1)*self.CELL_SIZE, (r+1)*self.CELL_SIZE,
                                             fill=color, outline="")
                if val:
                    self.canvas.create_text(c*self.CELL_SIZE+self.CELL_SIZE//2,
                                            r*self.CELL_SIZE+self.CELL_SIZE//2,
                                            text=str(val), font=("Segoe UI", 24, "bold"),
                                            fill="#776e65")

    def _move(self, direction):
        if not self.running:
            return
        moved = False
        if direction in ("left", "right"):
            for r in range(self.SIZE):
                line = [self.board[r][c] for c in range(self.SIZE)]
                if direction == "right":
                    line = line[::-1]
                new_line, changed = self._merge(line)
                if direction == "right":
                    new_line = new_line[::-1]
                if new_line != [self.board[r][c] for c in range(self.SIZE)]:
                    moved = True
                for c in range(self.SIZE):
                    self.board[r][c] = new_line[c]
        else:
            for c in range(self.SIZE):
                line = [self.board[r][c] for r in range(self.SIZE)]
                if direction == "down":
                    line = line[::-1]
                new_line, changed = self._merge(line)
                if direction == "down":
                    new_line = new_line[::-1]
                if new_line != [self.board[r][c] for r in range(self.SIZE)]:
                    moved = True
                for r in range(self.SIZE):
                    self.board[r][c] = new_line[r]

        if moved:
            self._add_random_tile()
            self._draw()
            if self._check_win():
                self._game_over("Победа!")
            elif self._check_game_over():
                self._game_over("Игра окончена")

    def _merge(self, line):
        non_zero = [x for x in line if x != 0]
        merged = []
        i = 0
        while i < len(non_zero):
            if i+1 < len(non_zero) and non_zero[i] == non_zero[i+1]:
                merged.append(non_zero[i]*2)
                i += 2
            else:
                merged.append(non_zero[i])
                i += 1
        merged += [0]*(self.SIZE - len(merged))
        return merged, merged != line

    def _check_win(self):
        for r in range(self.SIZE):
            for c in range(self.SIZE):
                if self.board[r][c] == 2048:
                    return True
        return False

    def _check_game_over(self):
        for r in range(self.SIZE):
            for c in range(self.SIZE):
                if self.board[r][c] == 0:
                    return False
                if r+1 < self.SIZE and self.board[r][c] == self.board[r+1][c]:
                    return False
                if c+1 < self.SIZE and self.board[r][c] == self.board[r][c+1]:
                    return False
        return True

    def _game_over(self, msg):
        self.running = False
        self.canvas.create_text(self.SIZE*self.CELL_SIZE//2,
                                self.SIZE*self.CELL_SIZE//2,
                                text=msg, fill="#ffffff", font=("Segoe UI", 20, "bold"))
        self.after(2000, self.close)


class TicTacToe(BaseMiniGame):
    """Крестики-нолики против простого ИИ."""
    SIZE = 3
    CELL_SIZE = 100

    def __init__(self, master, on_close=None):
        self.board = [[""]*self.SIZE for _ in range(self.SIZE)]
        self.current = "X"
        self.running = True
        super().__init__(master, on_close)

    def _create_widgets(self):
        self.canvas = tk.Canvas(self, width=self.SIZE*self.CELL_SIZE,
                                height=self.SIZE*self.CELL_SIZE,
                                bg="#2a2a2a", highlightthickness=0)
        self.canvas.pack(pady=10)
        self.canvas.bind("<Button-1>", self._click)
        self._draw_grid()

    def _draw_grid(self):
        for i in range(1, self.SIZE):
            self.canvas.create_line(i*self.CELL_SIZE, 0, i*self.CELL_SIZE, self.SIZE*self.CELL_SIZE, fill="#ffffff")
            self.canvas.create_line(0, i*self.CELL_SIZE, self.SIZE*self.CELL_SIZE, i*self.CELL_SIZE, fill="#ffffff")

    def _click(self, event):
        if not self.running or self.current != "X":
            return
        col = event.x // self.CELL_SIZE
        row = event.y // self.CELL_SIZE
        if self.board[row][col] == "":
            self.board[row][col] = "X"
            self._draw_symbol(row, col, "X")
            if self._check_win("X"):
                self._game_over("Вы выиграли!")
                return
            if self._is_full():
                self._game_over("Ничья")
                return
            self.current = "O"
            self.after(500, self._ai_move)

    def _ai_move(self):
        if not self.running:
            return
        empty = [(r,c) for r in range(self.SIZE) for c in range(self.SIZE) if self.board[r][c]==""]
        if empty:
            r,c = random.choice(empty)
            self.board[r][c] = "O"
            self._draw_symbol(r,c,"O")
            if self._check_win("O"):
                self._game_over("ИИ выиграл!")
                return
            if self._is_full():
                self._game_over("Ничья")
                return
            self.current = "X"

    def _draw_symbol(self, row, col, symbol):
        x = col*self.CELL_SIZE + self.CELL_SIZE//2
        y = row*self.CELL_SIZE + self.CELL_SIZE//2
        if symbol == "X":
            self.canvas.create_line(x-20, y-20, x+20, y+20, fill="#ff5555", width=3)
            self.canvas.create_line(x+20, y-20, x-20, y+20, fill="#ff5555", width=3)
        else:
            self.canvas.create_oval(x-20, y-20, x+20, y+20, outline="#5555ff", width=3)

    def _check_win(self, symbol):
        for i in range(self.SIZE):
            if all(self.board[i][j] == symbol for j in range(self.SIZE)):
                return True
            if all(self.board[j][i] == symbol for j in range(self.SIZE)):
                return True
        if all(self.board[i][i] == symbol for i in range(self.SIZE)):
            return True
        if all(self.board[i][self.SIZE-1-i] == symbol for i in range(self.SIZE)):
            return True
        return False

    def _is_full(self):
        return all(self.board[r][c] != "" for r in range(self.SIZE) for c in range(self.SIZE))

    def _game_over(self, msg):
        self.running = False
        self.canvas.create_text(self.SIZE*self.CELL_SIZE//2,
                                self.SIZE*self.CELL_SIZE//2,
                                text=msg, fill="#ffffff", font=("Segoe UI", 20, "bold"))
        self.after(2000, self.close)