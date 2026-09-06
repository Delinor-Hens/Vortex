import ctypes
import json
import os

class VortexCore:
    def __init__(self, dll_path=None):
        if dll_path is None:
            dll_path = os.path.join(os.path.dirname(__file__), '..', 'build', 'vortex_core.dll')
        dll_path = os.path.abspath(dll_path)
        self.dll = ctypes.CDLL(dll_path)

        # Инициализация и завершение
        self.dll.vortex_init.argtypes = [ctypes.c_char_p]
        self.dll.vortex_init.restype = ctypes.c_int

        self.dll.vortex_shutdown.argtypes = []
        self.dll.vortex_shutdown.restype = None

        # Чаты
        self.dll.vortex_get_chats.argtypes = []
        self.dll.vortex_get_chats.restype = ctypes.c_void_p

        self.dll.vortex_create_chat.argtypes = [ctypes.c_char_p]
        self.dll.vortex_create_chat.restype = ctypes.c_int

        self.dll.vortex_delete_chat.argtypes = [ctypes.c_int]
        self.dll.vortex_delete_chat.restype = ctypes.c_int

        self.dll.vortex_clear_chat.argtypes = [ctypes.c_int]
        self.dll.vortex_clear_chat.restype = ctypes.c_int

        self.dll.vortex_select_chat.argtypes = [ctypes.c_int]
        self.dll.vortex_select_chat.restype = ctypes.c_int

        self.dll.vortex_get_history.argtypes = []
        self.dll.vortex_get_history.restype = ctypes.c_void_p

        # Модель и режимы
        self.dll.vortex_set_model.argtypes = [ctypes.c_char_p]
        self.dll.vortex_set_model.restype = ctypes.c_int

        self.dll.vortex_get_models.argtypes = []
        self.dll.vortex_get_models.restype = ctypes.c_void_p

        self.dll.vortex_pull_model.argtypes = [ctypes.c_char_p]
        self.dll.vortex_pull_model.restype = ctypes.c_int

        self.dll.vortex_set_mode.argtypes = [ctypes.c_char_p]
        self.dll.vortex_set_mode.restype = ctypes.c_int

        self.dll.vortex_set_generation_mode.argtypes = [ctypes.c_char_p]
        self.dll.vortex_set_generation_mode.restype = ctypes.c_int

        self.dll.vortex_get_current_model.argtypes = []
        self.dll.vortex_get_current_model.restype = ctypes.c_void_p

        self.dll.vortex_get_current_mode.argtypes = []
        self.dll.vortex_get_current_mode.restype = ctypes.c_void_p

        self.dll.vortex_get_current_generation_mode.argtypes = []
        self.dll.vortex_get_current_generation_mode.restype = ctypes.c_void_p

        # Провайдеры
        self.dll.vortex_set_active_provider.argtypes = [ctypes.c_int]
        self.dll.vortex_set_active_provider.restype = ctypes.c_int

        self.dll.vortex_get_active_provider.argtypes = []
        self.dll.vortex_get_active_provider.restype = ctypes.c_int

        self.dll.vortex_set_provider_config.argtypes = [ctypes.c_int, ctypes.c_char_p,
                                                        ctypes.c_char_p, ctypes.c_char_p,
                                                        ctypes.c_char_p]
        self.dll.vortex_set_provider_config.restype = ctypes.c_int

        self.dll.vortex_get_provider_list.argtypes = []
        self.dll.vortex_get_provider_list.restype = ctypes.c_void_p

        self.dll.vortex_get_active_provider_info.argtypes = []
        self.dll.vortex_get_active_provider_info.restype = ctypes.c_void_p

        # Отправка сообщений
        self.dll.vortex_send_message.argtypes = [ctypes.c_char_p]
        self.dll.vortex_send_message.restype = ctypes.c_void_p

        self.dll.vortex_start_stream.argtypes = [ctypes.c_char_p]
        self.dll.vortex_start_stream.restype = ctypes.c_int

        self.dll.vortex_get_stream_chunk.argtypes = []
        self.dll.vortex_get_stream_chunk.restype = ctypes.c_void_p

        self.dll.vortex_is_generating.argtypes = []
        self.dll.vortex_is_generating.restype = ctypes.c_int

        self.dll.vortex_free_string.argtypes = [ctypes.c_void_p]
        self.dll.vortex_free_string.restype = None

        self.dll.vortex_warmup.argtypes = []
        self.dll.vortex_warmup.restype = ctypes.c_int

    # ---------- Вспомогательные методы ----------
    def _free(self, ptr):
        if ptr:
            self.dll.vortex_free_string(ptr)

    def _get_string(self, func):
        ptr = func()
        if not ptr:
            return None
        result = ctypes.string_at(ptr).decode('utf-8')
        self._free(ptr)
        return result

    # ---------- Основные методы ----------
    def init(self, host="127.0.0.1"):
        return self.dll.vortex_init(host.encode('utf-8'))

    def shutdown(self):
        self.dll.vortex_shutdown()

    def get_models(self):
        raw = self._get_string(self.dll.vortex_get_models)
        if raw:
            try:
                return json.loads(raw)
            except json.JSONDecodeError:
                return []
        return []

    def pull_model(self, model_name):
        return self.dll.vortex_pull_model(model_name.encode('utf-8'))

    def set_model(self, model_name):
        return self.dll.vortex_set_model(model_name.encode('utf-8'))

    def set_mode(self, mode_name):
        return self.dll.vortex_set_mode(mode_name.encode('utf-8'))

    def set_generation_mode(self, mode_name):
        return self.dll.vortex_set_generation_mode(mode_name.encode('utf-8'))

    def get_current_model(self):
        return self._get_string(self.dll.vortex_get_current_model)

    def get_current_mode(self):
        return self._get_string(self.dll.vortex_get_current_mode)

    def get_current_generation_mode(self):
        return self._get_string(self.dll.vortex_get_current_generation_mode)

    # ---------- Провайдеры ----------
    def set_active_provider(self, provider_type: int):
        return self.dll.vortex_set_active_provider(provider_type)

    def get_active_provider(self) -> int:
        return self.dll.vortex_get_active_provider()

    def set_provider_config(self, provider_type: int, name: str, base_url: str,
                            api_key: str, model: str):
        return self.dll.vortex_set_provider_config(provider_type,
                                                   name.encode('utf-8'),
                                                   base_url.encode('utf-8'),
                                                   api_key.encode('utf-8'),
                                                   model.encode('utf-8'))

    def get_provider_list(self):
        raw = self._get_string(self.dll.vortex_get_provider_list)
        if raw:
            try:
                return json.loads(raw)
            except json.JSONDecodeError:
                return []
        return []

    def get_active_provider_info(self):
        raw = self._get_string(self.dll.vortex_get_active_provider_info)
        if raw:
            try:
                return json.loads(raw)
            except json.JSONDecodeError:
                return {}
        return {}

    # ---------- Чаты ----------
    def create_chat(self, chat_name=None):
        if chat_name:
            return self.dll.vortex_create_chat(chat_name.encode('utf-8'))
        return self.dll.vortex_create_chat(None)

    def delete_chat(self, chat_id):
        return self.dll.vortex_delete_chat(chat_id)

    def clear_chat(self, chat_id):
        return self.dll.vortex_clear_chat(chat_id)

    def select_chat(self, chat_id):
        return self.dll.vortex_select_chat(chat_id)

    def get_chats(self):
        raw = self._get_string(self.dll.vortex_get_chats)
        if raw:
            try:
                return json.loads(raw)
            except json.JSONDecodeError:
                raw = raw.replace('\\n', '\n').replace('\\"', '"').replace('\\\\', '\\')
                try:
                    return json.loads(raw)
                except:
                    return []
        return []

    def get_history(self):
        raw = self._get_string(self.dll.vortex_get_history)
        if raw:
            try:
                return json.loads(raw)
            except json.JSONDecodeError:
                raw = raw.replace('\\n', '\n').replace('\\"', '"').replace('\\\\', '\\')
                try:
                    return json.loads(raw)
                except:
                    return []
        return []

    # ---------- Отправка ----------
    def send_message(self, user_message):
        ptr = self.dll.vortex_send_message(user_message.encode('utf-8'))
        if not ptr:
            return None
        result = ctypes.string_at(ptr).decode('utf-8')
        self._free(ptr)
        return result

    def start_stream(self, user_message):
        return self.dll.vortex_start_stream(user_message.encode('utf-8'))

    def get_stream_chunk(self):
        ptr = self.dll.vortex_get_stream_chunk()
        if not ptr:
            return None
        result = ctypes.string_at(ptr).decode('utf-8')
        self._free(ptr)
        return result

    def is_generating(self):
        return bool(self.dll.vortex_is_generating())

    def warmup(self):
        return self.dll.vortex_warmup()