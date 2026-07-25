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
typedef struct RustPluginManager          RustPluginManager;
typedef struct RustPluginRegistry         RustPluginRegistry;
typedef struct RustTaskRunner             RustTaskRunner;
typedef struct RustUpdater                RustUpdater;
typedef struct RustPluginUpdater          RustPluginUpdater;
typedef struct RustVersionFetcher         RustVersionFetcher;
typedef struct RustWorkspace              RustWorkspace;
typedef struct RustConfigValidator        RustConfigValidator;
typedef struct RustArchiveExtractor       RustArchiveExtractor;
typedef struct RustPermissionManager      RustPermissionManager;
typedef struct RustLengthPrefixedFramer   RustLengthPrefixedFramer;
typedef struct RustDependencyResolver     RustDependencyResolver;
typedef struct RustServiceLocator         RustServiceLocator;
typedef struct RustLanguageRegistry       RustLanguageRegistry;
typedef struct RustLanguageServerManager  RustLanguageServerManager;
typedef struct RustDebugConfigurationManager RustDebugConfigurationManager;
typedef struct RustPluginCrashHandler     RustPluginCrashHandler;

/* ── C callback type aliases ───────────────────────────────────── */
typedef void (*OnStringMessage)(const char* data, void* user_data);
typedef void (*OnEvent)(const char* event, const char* json_data, void* user_data);
typedef void (*OnDapStopped)(const char* reason, int thread_id, void* user_data);
typedef void (*OnStackFrames)(int thread_id, const char* json_frames, void* user_data);
typedef void (*OnScopes)(int frame_id, const char* json_scopes, void* user_data);
typedef void (*OnVariables)(int var_ref, const char* json_vars, void* user_data);
typedef void (*OnDapBreakpoints)(const char* source, const char* json_breakpoints, void* user_data);
typedef void (*OnPluginEvent)(const char* plugin_id, const char* json_data, void* user_data);
typedef void (*OnProgress)(const char* task_id, int current, int total, void* user_data);

/* ── Global helpers ────────────────────────────────────────────── */
const char* rust_last_error(void);
void        rust_free_string(char* s);

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
 *  Plugin Manager
 * ══════════════════════════════════════════════════════════════════ */
RustPluginManager* rust_plugin_manager_new(void);
void               rust_plugin_manager_free(RustPluginManager* pm);

bool rust_pm_load_plugins(RustPluginManager* pm, const char* path);
bool rust_pm_load_plugin(RustPluginManager* pm, const char* file_path);
void rust_pm_unload_plugin(RustPluginManager* pm, const char* id);
void rust_pm_unload_all(RustPluginManager* pm);
bool rust_pm_is_loaded(const RustPluginManager* pm, const char* id);
char* rust_pm_plugin_version(const RustPluginManager* pm, const char* id);
char** rust_pm_list_loaded(const RustPluginManager* pm, size_t* out_len);
void  rust_pm_free_strings(char** strs, size_t len);
bool  rust_pm_build_dep_graph(RustPluginManager* pm,
                              const char* const* metadata_jsons, size_t count);
char** rust_pm_topological_sort(const RustPluginManager* pm, size_t* out_len);

/* ── Plugin Manager callback setters ───────────────────────────── */
void rust_pm_on_plugin_loaded(RustPluginManager* pm,
                              OnPluginEvent cb, void* user_data);
void rust_pm_on_plugin_unloaded(RustPluginManager* pm,
                                OnPluginEvent cb, void* user_data);
void rust_pm_on_plugin_error(RustPluginManager* pm,
                             OnPluginEvent cb, void* user_data);

/* ══════════════════════════════════════════════════════════════════
 *  Plugin Crash Handler
 * ══════════════════════════════════════════════════════════════════ */
RustPluginCrashHandler* rust_crash_handler_new(void);
void                    rust_crash_handler_free(RustPluginCrashHandler* h);

void rust_crash_handler_on_crash(RustPluginCrashHandler* h,
                                 OnPluginEvent cb, void* user_data);

void rust_crash_handler_report_crash(RustPluginCrashHandler* h,
                                     const char* plugin_id,
                                     const char* error);

/* ══════════════════════════════════════════════════════════════════
 *  Plugin Registry
 * ══════════════════════════════════════════════════════════════════ */
RustPluginRegistry* rust_plugin_registry_new(void);
void                rust_plugin_registry_free(RustPluginRegistry* reg);

void   rust_plugin_registry_set_url(RustPluginRegistry* reg, const char* url);
char*  rust_plugin_registry_get_url(RustPluginRegistry* reg);
void   rust_plugin_registry_check_updates(RustPluginRegistry* reg);
bool   rust_plugin_registry_upgrade_available(RustPluginRegistry* reg,
                                              const char* id,
                                              const char* current_ver);
void   rust_plugin_registry_on_update(RustPluginRegistry* reg,
                                      OnStringMessage cb, void* user_data);
void   rust_plugin_registry_on_install_failed(RustPluginRegistry* reg,
                                              OnPluginEvent cb, void* user_data);

/* ══════════════════════════════════════════════════════════════════
 *  Service Locator
 * ══════════════════════════════════════════════════════════════════ */
RustServiceLocator* rust_service_locator_new(void);
void                rust_service_locator_free(RustServiceLocator* sl);

void   rust_service_locator_register(RustServiceLocator* sl,
                                     const char* id, void* service);
void*  rust_service_locator_get(RustServiceLocator* sl, const char* id);
void   rust_service_locator_unregister(RustServiceLocator* sl, const char* id);
bool   rust_service_locator_has(RustServiceLocator* sl, const char* id);
char** rust_service_locator_list(RustServiceLocator* sl, size_t* out_len);
void   rust_service_locator_free_list(char** strs, size_t len);

/* ══════════════════════════════════════════════════════════════════
 *  Dependency Resolver
 * ══════════════════════════════════════════════════════════════════ */
RustDependencyResolver* rust_dep_resolver_new(void);
void                    rust_dep_resolver_free(RustDependencyResolver* r);

bool   rust_dep_resolver_resolve(RustDependencyResolver* r,
                                 const char* json_metadata);
char** rust_dep_resolver_order(RustDependencyResolver* r, size_t* out_len);
void   rust_dep_resolver_free_order(char** strs, size_t len);

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
 *  Plugin Updater
 * ══════════════════════════════════════════════════════════════════ */
RustPluginUpdater* rust_plugin_updater_new(void);
void               rust_plugin_updater_free(RustPluginUpdater* pu);

void rust_plugin_updater_check(RustPluginUpdater* pu,
                               const char* plugin_id,
                               const char* current_version);
void rust_plugin_updater_on_update(RustPluginUpdater* pu,
                                   OnPluginEvent cb, void* user_data);
void rust_plugin_updater_on_progress(RustPluginUpdater* pu,
                                     OnProgress cb, void* user_data);

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
 *  Archive Extractor
 * ══════════════════════════════════════════════════════════════════ */
RustArchiveExtractor* rust_archive_extractor_new(void);
void                  rust_archive_extractor_free(RustArchiveExtractor* ae);

bool rust_archive_extractor_extract(RustArchiveExtractor* ae,
                                    const uint8_t* archive_data,
                                    size_t data_len, const char* dest_dir);
void rust_archive_extractor_on_progress(RustArchiveExtractor* ae,
                                        OnProgress cb, void* user_data);

/* ══════════════════════════════════════════════════════════════════
 *  Permission Manager
 * ══════════════════════════════════════════════════════════════════ */
RustPermissionManager* rust_permission_manager_new(void);
void                   rust_permission_manager_free(RustPermissionManager* pm);

bool rust_permission_manager_check(RustPermissionManager* pm,
                                   const char* plugin_id, int perm);
void rust_permission_manager_request(RustPermissionManager* pm,
                                     const char* plugin_id, int perm);
void rust_permission_manager_grant(RustPermissionManager* pm,
                                   const char* plugin_id, int perm);
void rust_permission_manager_revoke(RustPermissionManager* pm,
                                    const char* plugin_id, int perm);

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

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* RUST_BACKEND_H */
