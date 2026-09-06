#pragma once

#ifdef VORTEX_CORE_EXPORTS
#define VORTEX_API __declspec(dllexport)
#else
#define VORTEX_API __declspec(dllimport)
#endif

#ifdef __cplusplus
extern "C" {
#endif

VORTEX_API int vortex_init(const char* ollama_host);
VORTEX_API void vortex_shutdown();

VORTEX_API int vortex_clear_chat(int chat_id);
VORTEX_API char* vortex_get_models();
VORTEX_API int vortex_pull_model(const char* model_name);

VORTEX_API int vortex_set_model(const char* model_name);
VORTEX_API int vortex_set_mode(const char* mode_name);
VORTEX_API int vortex_set_generation_mode(const char* mode_name);
VORTEX_API char* vortex_get_current_model();
VORTEX_API char* vortex_get_current_mode();
VORTEX_API char* vortex_get_current_generation_mode();
VORTEX_API int vortex_warmup();

VORTEX_API int vortex_create_chat(const char* chat_name);
VORTEX_API int vortex_delete_chat(int chat_id);
VORTEX_API int vortex_select_chat(int chat_id);
VORTEX_API char* vortex_get_chats();
VORTEX_API char* vortex_get_history();

VORTEX_API char* vortex_send_message(const char* user_message);
VORTEX_API char* vortex_send_message_with_images(const char* user_message, const char** images_base64, int image_count);

VORTEX_API int vortex_start_stream(const char* user_message);
VORTEX_API char* vortex_get_stream_chunk();
VORTEX_API int vortex_is_generating();

// Новые функции для провайдеров
VORTEX_API int vortex_set_active_provider(int provider_type);
VORTEX_API int vortex_get_active_provider();
VORTEX_API int vortex_set_provider_config(int provider_type,
                                          const char* name,
                                          const char* base_url,
                                          const char* api_key,
                                          const char* model);
VORTEX_API char* vortex_get_provider_list();
VORTEX_API char* vortex_get_active_provider_info();

VORTEX_API void vortex_free_string(char* str);

#ifdef __cplusplus
}
#endif