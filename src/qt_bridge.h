#pragma once
#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

void* cpp_qt_create_app(int argc, char** argv);
void cpp_qt_destroy_app(void* app);
void cpp_qt_set_org_name(void* app, const char* name);
void cpp_qt_set_app_name(void* app, const char* name);
void cpp_qt_set_style(void* app, const char* style);
void cpp_qt_set_window_icon(void* app);
void cpp_qt_load_translations(void* app);
void cpp_qt_load_theme(void* app, int theme_id, int mode);
void* cpp_qt_create_main_window(void* app, void* rust_app, const char* project, const char** files, int file_count);
void cpp_qt_show_window(void* app, void* window);
int cpp_qt_run_event_loop(void* app);

#ifdef __cplusplus
}
#endif
