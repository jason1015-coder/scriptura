use crate::archive_extractor::ArchiveExtractor;
use crate::dap::DapClient;
use crate::eventbus::EventBus;
use crate::lsp::LspClient;
use crate::permission::PermissionManager;
use crate::plugin::PluginManager;
use crate::registry::PluginRegistry;
use crate::service_locator::ServiceLocator;
use crate::task_runner::TaskRunner;
use crate::ui_actions::UiActionHandler;
use crate::updater::Updater;
use crate::workspace::Workspace;
use crate::dependency_resolver::DependencyResolver;
use crate::config_validator::ConfigValidator;
use crate::app_crash::AppCrashHandler;

pub struct ScripturaApp {
    workspace: Workspace,
    lsp_client: LspClient,
    dap_client: DapClient,
    event_bus: EventBus,
    plugin_manager: PluginManager,
    task_runner: TaskRunner,
    updater: Updater,
    config_validator: ConfigValidator,
    plugin_registry: PluginRegistry,
    permission_manager: PermissionManager,
    service_locator: ServiceLocator,
    dependency_resolver: DependencyResolver,
    archive_extractor: ArchiveExtractor,
    crash_handler: AppCrashHandler,
    ui_actions: UiActionHandler,
}

impl ScripturaApp {
    pub fn new() -> Self {
        Self {
            workspace: Workspace::new(),
            lsp_client: LspClient::new(),
            dap_client: DapClient::new(),
            event_bus: EventBus::new(),
            plugin_manager: PluginManager::new(),
            task_runner: TaskRunner::new(),
            updater: Updater::new(),
            config_validator: ConfigValidator::new(),
            plugin_registry: PluginRegistry::new(),
            permission_manager: PermissionManager::new(),
            service_locator: ServiceLocator::new(),
            dependency_resolver: DependencyResolver::new(),
            archive_extractor: ArchiveExtractor::new(),
            crash_handler: AppCrashHandler::new(),
            ui_actions: UiActionHandler::new(),
        }
    }

    pub fn initialize(&mut self) {
        self.crash_handler.install();
    }

    pub fn shutdown(&mut self) {
        self.workspace.save();
    }

    pub fn workspace(&mut self) -> &mut Workspace { &mut self.workspace }
    pub fn lsp_client(&mut self) -> &mut LspClient { &mut self.lsp_client }
    pub fn dap_client(&mut self) -> &mut DapClient { &mut self.dap_client }
    pub fn event_bus(&mut self) -> &mut EventBus { &mut self.event_bus }
    pub fn plugin_manager(&mut self) -> &mut PluginManager { &mut self.plugin_manager }
    pub fn task_runner(&mut self) -> &mut TaskRunner { &mut self.task_runner }
    pub fn updater(&mut self) -> &mut Updater { &mut self.updater }
    pub fn config_validator(&mut self) -> &mut ConfigValidator { &mut self.config_validator }
    pub fn plugin_registry(&mut self) -> &mut PluginRegistry { &mut self.plugin_registry }
    pub fn permission_manager(&mut self) -> &mut PermissionManager { &mut self.permission_manager }
    pub fn service_locator(&mut self) -> &mut ServiceLocator { &mut self.service_locator }
    pub fn dependency_resolver(&mut self) -> &mut DependencyResolver { &mut self.dependency_resolver }
    pub fn archive_extractor(&mut self) -> &mut ArchiveExtractor { &mut self.archive_extractor }
    pub fn crash_handler(&mut self) -> &mut AppCrashHandler { &mut self.crash_handler }
    pub fn ui_actions(&mut self) -> &mut UiActionHandler { &mut self.ui_actions }
}
