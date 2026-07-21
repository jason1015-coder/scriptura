#ifndef PLUGINFEATURE_H
#define PLUGINFEATURE_H

#include <Qt>
#include <QMetaType>

/**
 * @file pluginfeature.h
 * @brief 定義插件功能特徵枚舉，用於描述插件提供的功能類型
 * 
 * 這個枚舉定義了 Scriptura 插件系統中所有可能的功能特徵，
 * 插件可以通過 hasFeature() 方法聲明它支持哪些功能。
 */

/**
 * @enum PluginFeature
 * @brief 插件功能特徵類型
 * 
 * 每個值代表插件可能提供的特定功能類別，
 * 用於插件發現、功能查詢和依賴解析。
 */
enum class PluginFeature {
    // 編輯器功能
    EditorExtension,        ///< 編輯器擴展
    SyntaxHighlighting,     ///< 語法高亮
    CodeCompletion,         ///< 程式碼補全

    // UI 控制功能
    MenuAction,             ///< 選單動作
    ToolbarButton,          ///< 工具列按鈕
    StatusBarWidget,        ///< 狀態列元件
    SidePanel,              ///< 側邊面板
    BottomPanel,            ///< 底部面板
    SidebarButton,          ///< 側邊欄按鈕

    // 工具功能
    ToolPanel,              ///< 工具面板

    // 專案功能
    ProjectWizard,          ///< 專案精靈
    BuildSystem,            ///< 構建系統
    FileExplorer,           ///< 檔案瀏覽

    // 編輯器擴展功能
    EditorDecoration,       ///< 編輯器裝飾
    EditorAnnotation,       ///< 編輯器註釋
    InlineCompletion,       ///< 行內補全

    // 通知功能
    Notification,           ///< 通知顯示
    ProgressIndicator,      ///< 進度指示器

    // 分析功能
    LSPProvider,            ///< LSP 提供者
    DiagnosticsProvider,    ///< 診斷提供者
    Formatter,              ///< 格式化工具

    // 整合功能
    VCSIntegration,         ///< 版本控制
    TerminalEmulator,       ///< 終端機
    ExternalTool            ///< 外部工具
};

// 註冊為 QMetaType 以支援 QVariant 封裝
Q_DECLARE_METATYPE(PluginFeature)

#endif // PLUGINFEATURE_H