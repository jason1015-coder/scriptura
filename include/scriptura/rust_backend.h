#ifndef RUST_BACKEND_H
#define RUST_BACKEND_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Opaque handle types ──────────────────────────────────────── */
typedef struct RustEventBus               RustEventBus;
typedef struct RustLspClient              RustLspClient;
typedef struct RustDapClient              RustDapClient;
typedef struct RustDebugSession           RustDebugSession;
typedef struct RustTaskRunner             RustTaskRunner;
typedef struct RustUpdater                RustUpdater;
typedef struct RustVersionFetcher         RustVersionFetcher;
typedef struct RustWorkspace              RustWorkspace;
typedef struct RustConfigValidator        RustConfigValidator;
typedef struct RustLengthPrefixedFramer   RustLengthPrefixedFramer;
typedef struct RustLanguageRegistry       RustLanguageRegistry;
typedef struct RustLanguageServerManager  RustLanguageServerManager;
typedef struct RustDebugConfigurationManager RustDebugConfigurationManager;
typedef struct RustUiActionHandler        RustUiActionHandler;

/* ── C callback type aliases ───────────────────────────────────── */
typedef void (*OnStringMessage)(const char* data, void* user_data);
typedef void (*OnEvent)(const char* event, const char* json_data, void* user_data);
typedef void (*OnDapStopped)(const char* reason, int thread_id, void* user_data);
typedef void (*OnStackFrames)(int thread_id, const char* json_frames, void* user_data);
typedef void (*OnScopes)(int frame_id, const char* json_scopes, void* user_data);
typedef void (*OnVariables)(int var_ref, const char* json_vars, void* user_data);
typedef void (*OnDapBreakpoints)(const char* source, const char* json_breakpoints, void* user_data);

/* ── Global helpers ────────────────────────────────────────────── */
char* rust_last_error(void);
void  rust_free_string(char* s);
void  rust_free_strings(char** strs, size_t len);

/* ══════════════════════════════════════════════════════════════════
 *  EventBus
 * ══════════════════════════════════════════════════════════════════ */
RustEventBus* rust_eventbus_new(void);
void          rust_eventbus_free(RustEventBus* bus);

uint64_t rust_eventbus_subscribe(RustEventBus* bus, const char* event,
                                 OnEvent callback, void* user_data);
void     rust_eventbus_unsubscribe(RustEventBus* bus, const char* event,
                                   uint64_t sub_id);
void     rust_eventbus_publish(RustEventBus* bus, const char* event,
                               const char* json_data);
bool     rust_eventbus_has_subscribers(RustEventBus* bus, const char* event);

/* ══════════════════════════════════════════════════════════════════
 *  LengthPrefixedFramer
 * ══════════════════════════════════════════════════════════════════ */
RustLengthPrefixedFramer* rust_framer_new(void);
void                      rust_framer_free(RustLengthPrefixedFramer* framer);

/* ══════════════════════════════════════════════════════════════════
 *  LSP Client
 * ══════════════════════════════════════════════════════════════════ */
RustLspClient* rust_lsp_client_new(void);
void           rust_lsp_client_free(RustLspClient* client);

bool rust_lsp_start_server(RustLspClient* client,
                           const char* command, const char* const* args,
                           size_t args_len, const char* root_uri);
void rust_lsp_stop_server(RustLspClient* client);
bool rust_lsp_is_running(const RustLspClient* client);

void rust_lsp_initialize(RustLspClient* client,
                         const char* root_uri, const char* language_id);
void rust_lsp_initialized(RustLspClient* client);
void rust_lsp_did_open(RustLspClient* client,
                       const char* uri, const char* lang_id, const char* text);
void rust_lsp_did_change(RustLspClient* client,
                         const char* uri, const char* text);
void rust_lsp_did_close(RustLspClient* client, const char* uri);
void rust_lsp_shutdown(RustLspClient* client);
void rust_lsp_exit(RustLspClient* client);

int rust_lsp_completion(RustLspClient* client,
                        const char* uri, int line, int character);
int rust_lsp_definition(RustLspClient* client,
                        const char* uri, int line, int character);
int rust_lsp_hover(RustLspClient* client,
                   const char* uri, int line, int character);
int rust_lsp_references(RustLspClient* client,
                        const char* uri, int line, int character);
int rust_lsp_signature_help(RustLspClient* client,
                            const char* uri, int line, int character);
int rust_lsp_declaration(RustLspClient* client,
                         const char* uri, int line, int character);
int rust_lsp_type_definition(RustLspClient* client,
                             const char* uri, int line, int character);
int rust_lsp_implementation(RustLspClient* client,
                            const char* uri, int line, int character);

int rust_lsp_code_action(RustLspClient* client,
                         const char* uri,
                         int start_line, int start_char,
                         int end_line, int end_char);
int rust_lsp_range_formatting(RustLspClient* client,
                              const char* uri,
                              int start_line, int start_char,
                              int end_line, int end_char);
int rust_lsp_rename(RustLspClient* client,
                    const char* uri, int line, int character,
                    const char* new_name);
int rust_lsp_document_symbol(RustLspClient* client, const char* uri);
int rust_lsp_workspace_symbol(RustLspClient* client, const char* query);
int rust_lsp_formatting(RustLspClient* client,
                        const char* uri, const char* json_options);
void rust_lsp_feed_message(RustLspClient* client, const char* json_data);

/* ── LSP callback setters ──────────────────────────────────────── */
void rust_lsp_on_server_started(RustLspClient* client,
                                OnStringMessage cb, void* user_data);
void rust_lsp_on_server_failed(RustLspClient* client,
                               OnStringMessage cb, void* user_data);
void rust_lsp_on_diagnostics(RustLspClient* client,
                             void (*cb)(const char*, const char*, void*),
                             void* user_data);
void rust_lsp_on_completion(RustLspClient* client,
                            void (*cb)(int, const char*, void*),
                            void* user_data);
void rust_lsp_on_definition(RustLspClient* client,
                            void (*cb)(int, const char*, void*),
                            void* user_data);
void rust_lsp_on_hover(RustLspClient* client,
                       void (*cb)(int, const char*, void*),
                       void* user_data);
void rust_lsp_on_references(RustLspClient* client,
                            void (*cb)(int, const char*, void*),
                            void* user_data);
void rust_lsp_on_code_action(RustLspClient* client,
                             void (*cb)(int, const char*, void*),
                             void* user_data);

/* ══════════════════════════════════════════════════════════════════
 *  DAP Client
 * ══════════════════════════════════════════════════════════════════ */
RustDapClient* rust_dap_client_new(void);
void           rust_dap_client_free(RustDapClient* client);

bool rust_dap_start_server(RustDapClient* client,
                           const char* command, const char* const* args,
                           size_t args_len);
void rust_dap_stop_server(RustDapClient* client);
bool rust_dap_is_running(const RustDapClient* client);

void rust_dap_initialize(RustDapClient* client,
                         const char* program, const char* const* args,
                         size_t args_len, const char* cwd);
void rust_dap_launch(RustDapClient* client);
void rust_dap_configuration_done(RustDapClient* client);
void rust_dap_set_breakpoints(RustDapClient* client,
                              const char* source_path,
                              const int* lines, size_t lines_len);
void rust_dap_continue(RustDapClient* client);
void rust_dap_next(RustDapClient* client);
void rust_dap_step_in(RustDapClient* client);
void rust_dap_step_out(RustDapClient* client);
void rust_dap_pause(RustDapClient* client);
void rust_dap_disconnect(RustDapClient* client);
void rust_dap_stack_trace(RustDapClient* client, int thread_id);
void rust_dap_scopes(RustDapClient* client, int frame_id);
void rust_dap_variables(RustDapClient* client, int var_ref);
void rust_dap_evaluate(RustDapClient* client,
                       const char* expression, int frame_id,
                       const char* context);
void rust_dap_feed_message(RustDapClient* client, const char* json_data);

/* ── DAP callback setters ──────────────────────────────────────── */
void rust_dap_on_server_started(RustDapClient* c, OnStringMessage cb, void* u);
void rust_dap_on_server_failed(RustDapClient* c, OnStringMessage cb, void* u);
void rust_dap_on_initialized(RustDapClient* c, OnStringMessage cb, void* u);
void rust_dap_on_stopped(RustDapClient* c, OnDapStopped cb, void* u);
void rust_dap_on_continued(RustDapClient* c, OnStringMessage cb, void* u);
void rust_dap_on_breakpoints(RustDapClient* c, OnDapBreakpoints cb, void* u);
void rust_dap_on_stack_trace(RustDapClient* c, OnStackFrames cb, void* u);
void rust_dap_on_scopes(RustDapClient* c, OnScopes cb, void* u);
void rust_dap_on_variables(RustDapClient* c, OnVariables cb, void* u);
void rust_dap_on_evaluation(RustDapClient* c, OnStringMessage cb, void* u);

/* ══════════════════════════════════════════════════════════════════
 *  Task Runner
 * ══════════════════════════════════════════════════════════════════ */
RustTaskRunner* rust_task_runner_new(void);
void            rust_task_runner_free(RustTaskRunner* runner);

bool   rust_task_runner_load(RustTaskRunner* runner, const char* json_tasks);
void   rust_task_runner_run(RustTaskRunner* runner, const char* task_name);
void   rust_task_runner_stop(RustTaskRunner* runner);
char** rust_task_runner_available(RustTaskRunner* runner, size_t* out_len);

/* ── Task Runner callback setters ──────────────────────────────── */
void rust_task_runner_on_started(RustTaskRunner* r, OnStringMessage cb, void* u);
void rust_task_runner_on_finished(RustTaskRunner* r, OnStringMessage cb, void* u);
void rust_task_runner_on_output(RustTaskRunner* r, OnStringMessage cb, void* u);
void rust_task_runner_on_error(RustTaskRunner* r, OnStringMessage cb, void* u);

/* ══════════════════════════════════════════════════════════════════
 *  Updater
 * ══════════════════════════════════════════════════════════════════ */
RustUpdater* rust_updater_new(void);
void         rust_updater_free(RustUpdater* updater);

void  rust_updater_check(RustUpdater* updater,
                         const char* current_version, const char* update_url);
bool  rust_updater_is_update_available(const RustUpdater* updater);
char* rust_updater_latest_version(const RustUpdater* updater);
void  rust_updater_on_update_available(RustUpdater* u,
                                       OnStringMessage cb, void* user);

/* ══════════════════════════════════════════════════════════════════
 *  Version Fetcher
 * ══════════════════════════════════════════════════════════════════ */
RustVersionFetcher* rust_version_fetcher_new(void);
void                rust_version_fetcher_free(RustVersionFetcher* vf);

void  rust_version_fetcher_fetch(RustVersionFetcher* vf, const char* url);
char* rust_version_fetcher_latest(RustVersionFetcher* vf);
void  rust_version_fetcher_on_fetched(RustVersionFetcher* vf,
                                      OnStringMessage cb, void* user_data);

/* ══════════════════════════════════════════════════════════════════
 *  Workspace
 * ══════════════════════════════════════════════════════════════════ */
RustWorkspace* rust_workspace_new(void);
void           rust_workspace_free(RustWorkspace* ws);

bool   rust_workspace_load(RustWorkspace* ws, const char* path);
bool   rust_workspace_save(RustWorkspace* ws);
bool   rust_workspace_save_as(RustWorkspace* ws, const char* path);
char** rust_workspace_folders(RustWorkspace* ws, size_t* out_len);
void   rust_workspace_set_folders(RustWorkspace* ws,
                                  const char* const* folders, size_t count);
char*  rust_workspace_get_settings(RustWorkspace* ws);
void   rust_workspace_set_settings(RustWorkspace* ws, const char* json_settings);
char** rust_workspace_recent_files(RustWorkspace* ws, size_t* out_len);
void   rust_workspace_add_recent(RustWorkspace* ws, const char* path);
char*  rust_workspace_path(RustWorkspace* ws);
bool   rust_workspace_is_loaded(RustWorkspace* ws);

/* ══════════════════════════════════════════════════════════════════
 *  Config Validator
 * ══════════════════════════════════════════════════════════════════ */
RustConfigValidator* rust_config_validator_new(void);
void                 rust_config_validator_free(RustConfigValidator* cv);

char* rust_config_validator_validate(RustConfigValidator* cv,
                                     const char* json_config,
                                     const char* schema_json);
void  rust_config_validator_on_error(RustConfigValidator* cv,
                                     OnStringMessage cb, void* user_data);

/* ══════════════════════════════════════════════════════════════════
 *  Debug Session
 * ══════════════════════════════════════════════════════════════════ */
RustDebugSession* rust_debug_session_new(void);
void              rust_debug_session_free(RustDebugSession* session);

void rust_debug_session_start(RustDebugSession* session, const char* config_json);
void rust_debug_session_stop(RustDebugSession* session);
void rust_debug_session_on_state_change(RustDebugSession* session,
                                        OnStringMessage cb, void* user_data);

/* ══════════════════════════════════════════════════════════════════
 *  Debug Configuration Manager
 * ══════════════════════════════════════════════════════════════════ */
RustDebugConfigurationManager* rust_debug_config_manager_new(void);
void                           rust_debug_config_manager_free(
                                RustDebugConfigurationManager* m);

bool   rust_debug_config_manager_load(RustDebugConfigurationManager* m,
                                      const char* json_config);
char** rust_debug_config_manager_list(RustDebugConfigurationManager* m,
                                      size_t* out_len);
char*  rust_debug_config_manager_get(RustDebugConfigurationManager* m,
                                     const char* name);

/* ══════════════════════════════════════════════════════════════════
 *  Language Registry
 * ══════════════════════════════════════════════════════════════════ */
RustLanguageRegistry* rust_language_registry_new(void);
void                  rust_language_registry_free(RustLanguageRegistry* lr);

void  rust_language_registry_register(RustLanguageRegistry* lr,
                                      const char* lang_id, const char* name,
                                      const char* extensions,
                                      const char* server_command,
                                      const char* server_args);
void  rust_language_registry_unregister(RustLanguageRegistry* lr,
                                        const char* lang_id);
char* rust_language_registry_get(RustLanguageRegistry* lr, const char* lang_id);
char* rust_language_registry_detect(RustLanguageRegistry* lr,
                                    const char* filename);

/* ══════════════════════════════════════════════════════════════════
 *  Language Server Manager
 * ══════════════════════════════════════════════════════════════════ */
RustLanguageServerManager* rust_ls_manager_new(void);
void                       rust_ls_manager_free(RustLanguageServerManager* m);

void rust_ls_manager_start(RustLanguageServerManager* m,
                           const char* lang_id, const char* command,
                           const char* const* args, size_t args_len,
                           const char* root_uri);
void rust_ls_manager_stop(RustLanguageServerManager* m, const char* lang_id);
void rust_ls_manager_stop_all(RustLanguageServerManager* m);

/* ══════════════════════════════════════════════════════════════════
 *  Diff Engine
 * ══════════════════════════════════════════════════════════════════ */
char* rust_diff_compute(const char* left, const char* right);

/* ══════════════════════════════════════════════════════════════════
 *  Encoding Engine
 * ══════════════════════════════════════════════════════════════════ */
char* rust_encoding_detect(const char* file_path);
char* rust_encoding_detect_line_ending(const char* file_path);
bool  rust_encoding_has_bom(const char* file_path);
char* rust_encoding_convert_line_endings(const char* content, const char* from_style, const char* to_style);
char* rust_encoding_read(const char* file_path, const char* encoding);
bool  rust_encoding_write(const char* file_path, const char* content, const char* encoding);
char* rust_encoding_supported(void);

/* ══════════════════════════════════════════════════════════════════
 *  Blame Engine
 * ══════════════════════════════════════════════════════════════════ */
char* rust_blame_parse(const char* output);

/* ══════════════════════════════════════════════════════════════════
 *  Emmet Engine
 * ══════════════════════════════════════════════════════════════════ */
char* rust_emmet_expand(const char* abbreviation);
char* rust_emmet_expand_json(const char* abbreviation);
bool  rust_emmet_is_css_shorthand(const char* text);

/* ══════════════════════════════════════════════════════════════════
 *  Session Engine
 * ══════════════════════════════════════════════════════════════════ */
bool  rust_session_has_saved(void);
bool  rust_session_clear(void);
char* rust_session_load(void);
bool  rust_session_save(const char* json);
bool  rust_session_has_hot_exit(void);
char* rust_session_load_hot_exit(void);

/* ══════════════════════════════════════════════════════════════════
 *  Test Engine
 * ══════════════════════════════════════════════════════════════════ */
char* rust_test_detect_framework(const char* project_path);
char* rust_test_build_command(const char* framework, const char* project_path, const char* filter);
char* rust_test_parse_output(const char* framework, const char* output);

/* ══════════════════════════════════════════════════════════════════
 *  Settings Store (encrypted file + OS keychain)
 * ══════════════════════════════════════════════════════════════════ */
int   settings_initialize(void);
int   settings_initialize_with_names(const char* app, const char* org);
char* settings_migration_status(void);
char* settings_get(const char* key);
int   settings_set(const char* key, const char* value);
int   settings_remove(const char* key);
int   settings_contains(const char* key);
char* settings_get_all_json(void);
int   settings_set_all_json(const char* json);
int   settings_set_secret(const char* key, const char* value);
char* settings_get_secret(const char* key);
int   settings_delete_secret(const char* key);
int   settings_has_secret(const char* key);
int   settings_is_keychain_available(void);
char* settings_keychain_backend(void);
int   settings_keychain_is_persistent(void);
char* settings_key_storage(void);
char* settings_warnings_json(void);
int   settings_clear_all(void);
int   settings_factory_reset(void);
char* settings_get_path(void);
char* settings_get_dir(void);
void  settings_free_string(char* s);

/* ══════════════════════════════════════════════════════════════════════
 *  UI Action Handler
 * ══════════════════════════════════════════════════════════════════════
 * Every user interaction is routed through Rust: it validates the action +
 * payload, decides, and returns the minimal Qt commands for C++ to execute.
 * Action/command names live in ui_actions.h (mirrors ui_actions.rs).
 */
RustUiActionHandler* rust_ui_actions_new(void);
void                rust_ui_actions_free(RustUiActionHandler* h);

/* Route a user action. Returns JSON:
 *   { "commands": [ { "cmd": ... }, ... ], "error": null | "message" }
 * Free the result with rust_free_string(). */
char* rust_ui_actions_handle(RustUiActionHandler* h,
                             const char* action,
                             const char* json_payload);

/* Audit trail of handled actions (most recent last).
 * Free the array with rust_free_strings(). */
char** rust_ui_actions_log(RustUiActionHandler* h, size_t* out_len);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* RUST_BACKEND_H */
