import sys
import os
from gui.main_window import MainWindow
from gui.core_wrapper import VortexCore

def main():
    core = VortexCore()
    if core.init() != 0:
        print("Не удалось инициализировать ядро")
        sys.exit(1)
    app = MainWindow(core)
    app.mainloop()

if __name__ == "__main__":
    main()