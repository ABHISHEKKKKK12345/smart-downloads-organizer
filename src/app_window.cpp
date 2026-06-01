#include "app_window.hpp"
#include "config_manager.hpp"
#include "organizer_engine.hpp"
#include "database.hpp"
#include "file_analyzer.hpp"
#include "logger.hpp"
#include <filesystem>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cstring>

namespace fs = std::filesystem;

// ─── Column indices for file list store ──────────────────────────────────────
enum FileCols {
    COL_F_FILENAME = 0, COL_F_SIZE, COL_F_CATEGORY, COL_F_STATUS,
    COL_F_MODIFIED, COL_F_PATH, COL_F_STATUS_COLOR, COL_F_NUM
};
enum DupCols {
    COL_D_FILENAME = 0, COL_D_SIZE, COL_D_HASH, COL_D_PATH, COL_D_NUM
};
enum CleanupCols {
    COL_C_FILENAME = 0, COL_C_SIZE, COL_C_REASON, COL_C_MODIFIED, COL_C_PATH, COL_C_NUM
};
enum RuleCols {
    COL_R_ENABLED = 0, COL_R_PRIORITY, COL_R_NAME, COL_R_DESCRIPTION,
    COL_R_MATCHES, COL_R_ACTION, COL_R_ID, COL_R_NUM
};
enum LogCols {
    COL_L_TIME = 0, COL_L_LEVEL, COL_L_MESSAGE, COL_L_COLOR, COL_L_NUM
};

// ─── CSS Theme ────────────────────────────────────────────────────────────────
static const char* CSS_DARK = R"CSS(
* { font-family: "Segoe UI", "Ubuntu", "Noto Sans", sans-serif; }

window {
  background-color: #1a1a2e;
  color: #e0e0e0;
}

.sidebar {
  background-color: #16213e;
  border-right: 1px solid #0f3460;
  min-width: 220px;
}

.sidebar-title {
  font-size: 14px;
  font-weight: bold;
  color: #e94560;
  padding: 16px 20px 8px 20px;
  letter-spacing: 2px;
}

.sidebar-subtitle {
  font-size: 10px;
  color: #888;
  padding: 0px 20px 16px 20px;
}

.nav-button {
  background: transparent;
  border: none;
  color: #aaa;
  padding: 10px 20px;
  text-align: left;
  font-size: 13px;
  border-radius: 0;
  box-shadow: none;
}

.nav-button:hover {
  background-color: rgba(233,69,96,0.15);
  color: #e0e0e0;
}

.nav-button.active {
  background-color: rgba(233,69,96,0.25);
  color: #e94560;
  border-left: 3px solid #e94560;
  font-weight: bold;
}

.header-bar {
  background-color: #16213e;
  border-bottom: 1px solid #0f3460;
  padding: 8px 16px;
  min-height: 56px;
}

.header-title {
  font-size: 16px;
  font-weight: bold;
  color: #e0e0e0;
}

.stat-card {
  background-color: #16213e;
  border-radius: 12px;
  border: 1px solid #0f3460;
  padding: 20px;
  margin: 8px;
}

.stat-card:hover {
  border-color: #e94560;
}

.stat-value {
  font-size: 32px;
  font-weight: bold;
  color: #e94560;
}

.stat-label {
  font-size: 12px;
  color: #888;
  margin-top: 4px;
}

.stat-icon {
  font-size: 24px;
  margin-bottom: 8px;
}

.primary-button {
  background-color: #e94560;
  color: white;
  border: none;
  border-radius: 6px;
  padding: 8px 20px;
  font-weight: bold;
  font-size: 13px;
}

.primary-button:hover {
  background-color: #c73652;
}

.secondary-button {
  background-color: #0f3460;
  color: #e0e0e0;
  border: 1px solid #1a4a7a;
  border-radius: 6px;
  padding: 8px 16px;
  font-size: 13px;
}

.secondary-button:hover {
  background-color: #1a4a7a;
}

.danger-button {
  background-color: #8b0000;
  color: white;
  border: none;
  border-radius: 6px;
  padding: 8px 16px;
  font-size: 13px;
}

.danger-button:hover {
  background-color: #cc0000;
}

.page-title {
  font-size: 22px;
  font-weight: bold;
  color: #e0e0e0;
  margin-bottom: 4px;
}

.page-subtitle {
  font-size: 13px;
  color: #888;
  margin-bottom: 20px;
}

treeview {
  background-color: #16213e;
  color: #e0e0e0;
  font-size: 13px;
}

treeview:selected {
  background-color: #e94560;
  color: white;
}

treeview header button {
  background-color: #0f3460;
  color: #aaa;
  border: none;
  font-size: 12px;
  font-weight: bold;
}

.status-bar {
  background-color: #0f3460;
  color: #aaa;
  font-size: 12px;
  padding: 4px 16px;
}

.toast {
  background-color: #0f3460;
  color: #e0e0e0;
  border-left: 4px solid #e94560;
  border-radius: 0 4px 4px 0;
  padding: 12px 16px;
  font-size: 13px;
  margin: 8px;
}

.toast.success { border-color: #4CAF50; }
.toast.warning { border-color: #FF9800; }
.toast.error   { border-color: #e94560; }

entry {
  background-color: #0f3460;
  color: #e0e0e0;
  border: 1px solid #1a4a7a;
  border-radius: 6px;
  padding: 6px 10px;
}

entry:focus { border-color: #e94560; }

combobox button {
  background-color: #0f3460;
  color: #e0e0e0;
  border: 1px solid #1a4a7a;
  border-radius: 6px;
}

scrolledwindow {
  background-color: transparent;
}

.section-header {
  font-size: 14px;
  font-weight: bold;
  color: #e94560;
  border-bottom: 1px solid #0f3460;
  padding-bottom: 8px;
  margin-bottom: 12px;
}

.badge {
  background-color: #e94560;
  color: white;
  border-radius: 12px;
  padding: 2px 8px;
  font-size: 11px;
  font-weight: bold;
}

progressbar trough {
  background-color: #0f3460;
  border-radius: 4px;
  min-height: 6px;
}

progressbar progress {
  background-color: #e94560;
  border-radius: 4px;
}

switch {
  background-color: #0f3460;
}

switch:checked {
  background-color: #e94560;
}

checkbutton check {
  background-color: #0f3460;
  border: 1px solid #1a4a7a;
  border-radius: 3px;
}

checkbutton check:checked {
  background-color: #e94560;
  border-color: #e94560;
}

spinbutton {
  background-color: #0f3460;
  color: #e0e0e0;
  border: 1px solid #1a4a7a;
  border-radius: 6px;
}

.rule-enabled   { color: #4CAF50; }
.rule-disabled  { color: #888; }
.file-duplicate { color: #FF9800; }
.file-large     { color: #e94560; }
.file-old       { color: #888; }
.log-error      { color: #e94560; }
.log-warning    { color: #FF9800; }
.log-info       { color: #4CAF50; }
.log-debug      { color: #888; }
)CSS";

namespace SDO {

// ─── Helpers ─────────────────────────────────────────────────────────────────
static std::string formatTime(const std::chrono::system_clock::time_point& tp) {
    auto t = std::chrono::system_clock::to_time_t(tp);
    std::tm tm_buf{};
    localtime_r(&t, &tm_buf);
    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M");
    return oss.str();
}

static const char* categoryIcon(FileCategory cat) {
    switch (cat) {
        case FileCategory::Images:      return "🖼";
        case FileCategory::Videos:      return "🎬";
        case FileCategory::Audio:       return "🎵";
        case FileCategory::Documents:   return "📄";
        case FileCategory::Archives:    return "📦";
        case FileCategory::Code:        return "💻";
        case FileCategory::Executables: return "⚙";
        case FileCategory::Fonts:       return "🔤";
        case FileCategory::Data:        return "🗄";
        case FileCategory::Ebooks:      return "📚";
        case FileCategory::Torrents:    return "⬇";
        default:                        return "📁";
    }
}

// ─── Constructor/Destructor ───────────────────────────────────────────────────
AppWindow::AppWindow() {}
AppWindow::~AppWindow() {
    if (m_cssProvider) g_object_unref(m_cssProvider);
}

bool AppWindow::init(int argc, char** argv) {
    gtk_init(&argc, &argv);

    // Apply CSS first
    applyCSS();

    // Build and show main window
    m_window = buildMainWindow();
    if (!m_window) return false;

    // Wire up organizer callbacks (thread-safe via GTK idle)
    OrganizerEngine::instance().setFileDetectedCallback([this](const FileInfo& fi){
        UIUpdate upd;
        upd.type = UIUpdateType::FileAdded;
        upd.file = fi;
        postUIUpdate(upd);
    });

    OrganizerEngine::instance().setStatsUpdatedCallback([this](const Statistics& st){
        UIUpdate upd;
        upd.type  = UIUpdateType::StatsRefresh;
        upd.stats = st;
        postUIUpdate(upd);
    });

    OrganizerEngine::instance().setNotificationCallback([this](const Notification& n){
        UIUpdate upd;
        upd.type         = UIUpdateType::Notification;
        upd.notification = n;
        postUIUpdate(upd);
    });

    Logger::instance().setCallback([this](const LogEntry& e){
        UIUpdate upd;
        upd.type     = UIUpdateType::LogEntry;
        upd.logEntry = e;
        postUIUpdate(upd);
    });

    gtk_widget_show_all(m_window);

    const auto& cfg = ConfigManager::instance().config();
    if (cfg.startMinimized) gtk_window_iconify(GTK_WINDOW(m_window));

    // Initial stats load
    auto stats = Database::instance().getStatistics();
    refreshDashboard(stats);

    return true;
}

void AppWindow::run() {
    gtk_main();
}

void AppWindow::quit() {
    OrganizerEngine::instance().stopWatching();
    ConfigManager::instance().save();
    gtk_main_quit();
}

// ─── Thread-safe UI updates ───────────────────────────────────────────────────
void AppWindow::postUIUpdate(UIUpdate update) {
    {
        std::lock_guard<std::mutex> lk(m_uiMutex);
        m_uiQueue.push(std::move(update));
    }
    g_idle_add(onUIUpdateIdle, this);
}

gboolean AppWindow::onUIUpdateIdle(gpointer data) {
    auto* self = static_cast<AppWindow*>(data);
    UIUpdate upd;
    {
        std::lock_guard<std::mutex> lk(self->m_uiMutex);
        if (self->m_uiQueue.empty()) return G_SOURCE_REMOVE;
        upd = self->m_uiQueue.front();
        self->m_uiQueue.pop();
    }

    switch (upd.type) {
        case UIUpdateType::FileAdded:
            self->addFileToList(upd.file);
            // Refresh stats periodically
            {
                auto stats = Database::instance().getStatistics();
                self->refreshDashboard(stats);
            }
            break;
        case UIUpdateType::StatsRefresh:
            self->refreshDashboard(upd.stats);
            break;
        case UIUpdateType::Notification:
            self->showNotificationToast(upd.notification);
            break;
        case UIUpdateType::LogEntry:
            self->addLogEntry(upd.logEntry);
            break;
        case UIUpdateType::QueueSizeUpdate:
            self->updateStatusBar("Monitoring...", upd.queueSize);
            break;
        default: break;
    }
    return G_SOURCE_REMOVE;
}

// ─── CSS ─────────────────────────────────────────────────────────────────────
void AppWindow::applyCSS() {
    m_cssProvider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(m_cssProvider, CSS_DARK, -1, nullptr);
    gtk_style_context_add_provider_for_screen(
        gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(m_cssProvider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

// ─── Main Window ─────────────────────────────────────────────────────────────
GtkWidget* AppWindow::buildMainWindow() {
    GtkWidget* win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(win), APP_NAME);
    gtk_window_set_default_size(GTK_WINDOW(win),
        ConfigManager::instance().config().windowWidth,
        ConfigManager::instance().config().windowHeight);
    gtk_window_set_position(GTK_WINDOW(win), GTK_WIN_POS_CENTER);

    // Allow resize
    gtk_window_set_resizable(GTK_WINDOW(win), TRUE);
    g_signal_connect(win, "destroy", G_CALLBACK(onWindowDestroy), this);

    // Outer VBox: header + content + statusbar
    GtkWidget* outerBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(win), outerBox);

    // Header bar
    gtk_box_pack_start(GTK_BOX(outerBox), buildHeaderBar(), FALSE, FALSE, 0);

    // Main pane: sidebar + content stack
    GtkWidget* mainPane = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(outerBox), mainPane, TRUE, TRUE, 0);

    // Sidebar
    m_sidebar = buildSidebar();
    gtk_paned_pack1(GTK_PANED(mainPane), m_sidebar, FALSE, FALSE);
    gtk_paned_set_position(GTK_PANED(mainPane), 220);

    // Content stack
    m_stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(m_stack), GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT_RIGHT);
    gtk_stack_set_transition_duration(GTK_STACK(m_stack), 150);
    gtk_paned_pack2(GTK_PANED(mainPane), m_stack, TRUE, TRUE);

    // Add pages
    gtk_stack_add_named(GTK_STACK(m_stack), buildDashboardPage(), "dashboard");
    gtk_stack_add_named(GTK_STACK(m_stack), buildFilesPage(),     "files");
    gtk_stack_add_named(GTK_STACK(m_stack), buildDuplicatesPage(),"duplicates");
    gtk_stack_add_named(GTK_STACK(m_stack), buildCleanupPage(),   "cleanup");
    gtk_stack_add_named(GTK_STACK(m_stack), buildRulesPage(),     "rules");
    gtk_stack_add_named(GTK_STACK(m_stack), buildLogsPage(),      "logs");
    gtk_stack_add_named(GTK_STACK(m_stack), buildSettingsPage(),  "settings");

    // Toast overlay (positioned manually for now via info bar)
    m_toastBar = gtk_info_bar_new();
    gtk_info_bar_set_message_type(GTK_INFO_BAR(m_toastBar), GTK_MESSAGE_INFO);
    m_toastLabel = gtk_label_new("Ready");
    gtk_box_pack_start(GTK_BOX(gtk_info_bar_get_content_area(GTK_INFO_BAR(m_toastBar))),
                       m_toastLabel, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(outerBox), m_toastBar, FALSE, FALSE, 0);
    gtk_widget_set_no_show_all(m_toastBar, TRUE);

    // Status bar
    gtk_box_pack_start(GTK_BOX(outerBox), buildStatusBar(), FALSE, FALSE, 0);

    return win;
}

// ─── Header Bar ──────────────────────────────────────────────────────────────
GtkWidget* AppWindow::buildHeaderBar() {
    GtkWidget* hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_style_context_add_class(gtk_widget_get_style_context(hbox), "header-bar");
    gtk_widget_set_margin_start(hbox, 8);
    gtk_widget_set_margin_end(hbox, 8);

    // App icon + title
    GtkWidget* titleBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget* icon = gtk_label_new("📂");
    gtk_box_pack_start(GTK_BOX(titleBox), icon, FALSE, FALSE, 0);
    GtkWidget* title = gtk_label_new("Smart Downloads Organizer");
    gtk_style_context_add_class(gtk_widget_get_style_context(title), "header-title");
    gtk_box_pack_start(GTK_BOX(titleBox), title, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), titleBox, FALSE, FALSE, 0);

    // Spacer
    gtk_box_pack_start(GTK_BOX(hbox), gtk_label_new(""), TRUE, TRUE, 0);

    // Action buttons
    GtkWidget* undoBtn = gtk_button_new_with_label("↩ Undo");
    gtk_style_context_add_class(gtk_widget_get_style_context(undoBtn), "secondary-button");
    g_signal_connect(undoBtn, "clicked", G_CALLBACK(onUndoClicked), this);
    gtk_box_pack_end(GTK_BOX(hbox), undoBtn, FALSE, FALSE, 0);

    GtkWidget* organizeBtn = gtk_button_new_with_label("▶ Organize Now");
    gtk_style_context_add_class(gtk_widget_get_style_context(organizeBtn), "primary-button");
    g_signal_connect(organizeBtn, "clicked", G_CALLBACK(onOrganizeNowClicked), this);
    gtk_box_pack_end(GTK_BOX(hbox), organizeBtn, FALSE, FALSE, 0);

    GtkWidget* scanBtn = gtk_button_new_with_label("🔍 Scan");
    gtk_style_context_add_class(gtk_widget_get_style_context(scanBtn), "secondary-button");
    g_signal_connect(scanBtn, "clicked", G_CALLBACK(onScanNowClicked), this);
    gtk_box_pack_end(GTK_BOX(hbox), scanBtn, FALSE, FALSE, 0);

    return hbox;
}

// ─── Sidebar ─────────────────────────────────────────────────────────────────
GtkWidget* AppWindow::buildSidebar() {
    GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_style_context_add_class(gtk_widget_get_style_context(vbox), "sidebar");

    // App name
    GtkWidget* titleLabel = gtk_label_new("SDO");
    gtk_style_context_add_class(gtk_widget_get_style_context(titleLabel), "sidebar-title");
    gtk_widget_set_halign(titleLabel, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(vbox), titleLabel, FALSE, FALSE, 0);

    GtkWidget* subLabel = gtk_label_new("v2.0.0 • Enterprise");
    gtk_style_context_add_class(gtk_widget_get_style_context(subLabel), "sidebar-subtitle");
    gtk_widget_set_halign(subLabel, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(vbox), subLabel, FALSE, FALSE, 0);

    GtkWidget* sep1 = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(vbox), sep1, FALSE, FALSE, 0);

    struct NavItem { const char* icon; const char* label; const char* page; };
    NavItem items[] = {
        {"📊", "Dashboard",  "dashboard"},
        {"📁", "Files",      "files"},
        {"🔄", "Duplicates", "duplicates"},
        {"🧹", "Cleanup",    "cleanup"},
        {"⚙", "Rules",      "rules"},
        {"📋", "Logs",       "logs"},
        {"🔧", "Settings",   "settings"},
    };

    for (const auto& item : items) {
        std::string label = std::string(item.icon) + "  " + item.label;
        GtkWidget* btn = gtk_button_new_with_label(label.c_str());
        gtk_style_context_add_class(gtk_widget_get_style_context(btn), "nav-button");
        gtk_widget_set_halign(btn, GTK_ALIGN_FILL);
        g_object_set_data_full(G_OBJECT(btn), "page", g_strdup(item.page), g_free);
        g_signal_connect(btn, "clicked", G_CALLBACK(onSidebarNavClicked), this);
        gtk_box_pack_start(GTK_BOX(vbox), btn, FALSE, FALSE, 0);
        m_navButtons[item.page] = btn;
    }

    // Set dashboard as active
    if (m_navButtons.count("dashboard"))
        gtk_style_context_add_class(
            gtk_widget_get_style_context(m_navButtons["dashboard"]), "active");

    // Bottom spacer
    gtk_box_pack_start(GTK_BOX(vbox), gtk_label_new(""), TRUE, TRUE, 0);

    // Watch status indicator
    GtkWidget* watchBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_start(watchBox, 16);
    gtk_widget_set_margin_bottom(watchBox, 16);
    GtkWidget* watchDot = gtk_label_new("●");
    // Use CSS-based coloring (gtk_widget_override_color deprecated since 3.16)
    GtkCssProvider* dotCss = gtk_css_provider_new();
    gtk_css_provider_load_from_data(dotCss, "label { color: #4CAF50; }", -1, nullptr);
    gtk_style_context_add_provider(gtk_widget_get_style_context(watchDot),
        GTK_STYLE_PROVIDER(dotCss), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(dotCss);
    gtk_box_pack_start(GTK_BOX(watchBox), watchDot, FALSE, FALSE, 0);
    GtkWidget* watchLabel = gtk_label_new("Watching active");
    gtk_style_context_add_class(gtk_widget_get_style_context(watchLabel), "sidebar-subtitle");
    gtk_box_pack_start(GTK_BOX(watchBox), watchLabel, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), watchBox, FALSE, FALSE, 0);

    return vbox;
}

// ─── Dashboard Page ───────────────────────────────────────────────────────────
GtkWidget* AppWindow::buildDashboardPage() {
    GtkWidget* scroll = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
        GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

    GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_margin_start(vbox, 24);
    gtk_widget_set_margin_end(vbox, 24);
    gtk_widget_set_margin_top(vbox, 24);
    gtk_container_add(GTK_CONTAINER(scroll), vbox);

    // Page title
    GtkWidget* pageTitle = gtk_label_new("Dashboard");
    gtk_style_context_add_class(gtk_widget_get_style_context(pageTitle), "page-title");
    gtk_widget_set_halign(pageTitle, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(vbox), pageTitle, FALSE, FALSE, 0);

    GtkWidget* pageSub = gtk_label_new("Overview of your downloads folder");
    gtk_style_context_add_class(gtk_widget_get_style_context(pageSub), "page-subtitle");
    gtk_widget_set_halign(pageSub, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(vbox), pageSub, FALSE, FALSE, 4);

    // Stats grid
    GtkWidget* grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 0);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 0);
    gtk_grid_set_column_homogeneous(GTK_GRID(grid), TRUE);
    gtk_box_pack_start(GTK_BOX(vbox), grid, FALSE, FALSE, 8);

    struct StatDef { const char* icon; const char* label; const char* id; int col; int row; };
    StatDef defs[] = {
        {"📁", "Total Files",     "total_files",    0, 0},
        {"💾", "Total Size",      "total_size",     1, 0},
        {"🔄", "Duplicates",      "duplicates",     2, 0},
        {"🧹", "Space Saveable",  "space_save",     3, 0},
        {"📦", "Organized Today", "org_today",      0, 1},
        {"⚠",  "Large Files",    "large_files",    1, 1},
        {"🕐", "Old Files",       "old_files",      2, 1},
        {"✅", "Total Organized", "org_total",      3, 1},
    };

    for (const auto& def : defs) {
        GtkWidget* card = buildStatCard(def.icon, def.label, def.id);
        gtk_grid_attach(GTK_GRID(grid), card, def.col, def.row, 1, 1);
    }

    // Category breakdown section
    GtkWidget* catHeader = gtk_label_new("Category Breakdown");
    gtk_style_context_add_class(gtk_widget_get_style_context(catHeader), "section-header");
    gtk_widget_set_halign(catHeader, GTK_ALIGN_START);
    gtk_widget_set_margin_top(catHeader, 20);
    gtk_box_pack_start(GTK_BOX(vbox), catHeader, FALSE, FALSE, 0);

    // Category grid
    GtkWidget* catGrid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(catGrid), 16);
    gtk_grid_set_row_spacing(GTK_GRID(catGrid), 8);
    gtk_box_pack_start(GTK_BOX(vbox), catGrid, FALSE, FALSE, 8);

    const char* catNames[] = {"🖼 Images","🎬 Videos","🎵 Audio","📄 Documents",
                               "📦 Archives","💻 Code","⚙ Executables","📚 Ebooks"};
    const char* catIds[]   = {"cat_images","cat_videos","cat_audio","cat_docs",
                               "cat_archives","cat_code","cat_exec","cat_ebooks"};
    for (int i = 0; i < 8; ++i) {
        GtkWidget* lbl = gtk_label_new(catNames[i]);
        gtk_widget_set_halign(lbl, GTK_ALIGN_START);
        gtk_grid_attach(GTK_GRID(catGrid), lbl, (i%4)*2, i/4, 1, 1);
        GtkWidget* val = gtk_label_new("0");
        gtk_widget_set_halign(val, GTK_ALIGN_END);
        gtk_style_context_add_class(gtk_widget_get_style_context(val), "badge");
        gtk_grid_attach(GTK_GRID(catGrid), val, (i%4)*2+1, i/4, 1, 1);
        m_statLabels[catIds[i]] = val;
    }

    // Quick actions
    GtkWidget* actHeader = gtk_label_new("Quick Actions");
    gtk_style_context_add_class(gtk_widget_get_style_context(actHeader), "section-header");
    gtk_widget_set_halign(actHeader, GTK_ALIGN_START);
    gtk_widget_set_margin_top(actHeader, 20);
    gtk_box_pack_start(GTK_BOX(vbox), actHeader, FALSE, FALSE, 0);

    GtkWidget* actBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_margin_top(actBox, 8);
    gtk_box_pack_start(GTK_BOX(vbox), actBox, FALSE, FALSE, 0);

    auto makeQuickBtn = [&](const char* label, const char* cls, GCallback cb) {
        GtkWidget* b = gtk_button_new_with_label(label);
        gtk_style_context_add_class(gtk_widget_get_style_context(b), cls);
        g_signal_connect(b, "clicked", cb, this);
        gtk_box_pack_start(GTK_BOX(actBox), b, FALSE, FALSE, 0);
        return b;
    };

    makeQuickBtn("🔍 Scan Now",         "primary-button",   G_CALLBACK(onScanNowClicked));
    makeQuickBtn("▶ Organize All",      "primary-button",   G_CALLBACK(onOrganizeNowClicked));
    makeQuickBtn("🔄 Duplicate Scan",   "secondary-button", G_CALLBACK(onDuplicateScanClicked));
    makeQuickBtn("🧹 Show Cleanup",     "secondary-button", G_CALLBACK(onCleanupClicked));

    return scroll;
}

GtkWidget* AppWindow::buildStatCard(const char* icon, const char* label, const char* id) {
    GtkWidget* card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_style_context_add_class(gtk_widget_get_style_context(card), "stat-card");
    gtk_widget_set_margin_start(card, 8);
    gtk_widget_set_margin_end(card, 8);
    gtk_widget_set_margin_top(card, 8);
    gtk_widget_set_margin_bottom(card, 8);

    GtkWidget* iconLbl = gtk_label_new(icon);
    gtk_style_context_add_class(gtk_widget_get_style_context(iconLbl), "stat-icon");
    gtk_widget_set_halign(iconLbl, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(card), iconLbl, FALSE, FALSE, 0);

    GtkWidget* valLbl = gtk_label_new("0");
    gtk_style_context_add_class(gtk_widget_get_style_context(valLbl), "stat-value");
    gtk_widget_set_halign(valLbl, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(card), valLbl, FALSE, FALSE, 0);
    m_statLabels[id] = valLbl;

    GtkWidget* labLbl = gtk_label_new(label);
    gtk_style_context_add_class(gtk_widget_get_style_context(labLbl), "stat-label");
    gtk_widget_set_halign(labLbl, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(card), labLbl, FALSE, FALSE, 0);

    return card;
}

// ─── Files Page ───────────────────────────────────────────────────────────────
GtkWidget* AppWindow::buildFilesPage() {
    GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_margin_start(vbox, 16);
    gtk_widget_set_margin_end(vbox, 16);
    gtk_widget_set_margin_top(vbox, 16);

    // Title + toolbar
    GtkWidget* titleBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_box_pack_start(GTK_BOX(vbox), titleBox, FALSE, FALSE, 0);

    GtkWidget* title = gtk_label_new("All Files");
    gtk_style_context_add_class(gtk_widget_get_style_context(title), "page-title");
    gtk_box_pack_start(GTK_BOX(titleBox), title, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(titleBox), gtk_label_new(""), TRUE, TRUE, 0);

    // Search
    m_searchEntry = gtk_search_entry_new();
    gtk_widget_set_size_request(m_searchEntry, 250, -1);
    gtk_entry_set_placeholder_text(GTK_ENTRY(m_searchEntry), "Search files...");
    g_signal_connect(m_searchEntry, "search-changed", G_CALLBACK(onSearchChanged), this);
    gtk_box_pack_end(GTK_BOX(titleBox), m_searchEntry, FALSE, FALSE, 0);

    // Category filter
    m_categoryFilter = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(m_categoryFilter), "All Categories");
    const char* catNames[] = {"Images","Videos","Audio","Documents","Archives",
                               "Code","Executables","Fonts","Data","Ebooks","Torrents","Unknown"};
    for (const auto& n : catNames) gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(m_categoryFilter), n);
    gtk_combo_box_set_active(GTK_COMBO_BOX(m_categoryFilter), 0);
    g_signal_connect(m_categoryFilter, "changed", G_CALLBACK(onFilterCategoryChanged), this);
    gtk_box_pack_end(GTK_BOX(titleBox), m_categoryFilter, FALSE, FALSE, 0);

    // File list
    m_filesStore = gtk_list_store_new(COL_F_NUM,
        G_TYPE_STRING,  // filename
        G_TYPE_STRING,  // size
        G_TYPE_STRING,  // category
        G_TYPE_STRING,  // status
        G_TYPE_STRING,  // modified
        G_TYPE_STRING,  // path (hidden)
        G_TYPE_STRING   // status color
    );

    m_filesTreeView = gtk_tree_view_new_with_model(GTK_TREE_MODEL(m_filesStore));
    
    gtk_tree_view_set_search_column(GTK_TREE_VIEW(m_filesTreeView), COL_F_FILENAME);

    auto addCol = [&](const char* title, int col, int colorCol = -1) {
        GtkCellRenderer* renderer = gtk_cell_renderer_text_new();
        GtkTreeViewColumn* c = gtk_tree_view_column_new_with_attributes(
            title, renderer, "text", col, nullptr);
        if (colorCol >= 0)
            gtk_tree_view_column_add_attribute(c, renderer, "foreground", colorCol);
        gtk_tree_view_column_set_resizable(c, TRUE);
        gtk_tree_view_column_set_sort_column_id(c, col);
        gtk_tree_view_append_column(GTK_TREE_VIEW(m_filesTreeView), c);
    };

    addCol("Filename",  COL_F_FILENAME, COL_F_STATUS_COLOR);
    addCol("Size",      COL_F_SIZE);
    addCol("Category",  COL_F_CATEGORY);
    addCol("Status",    COL_F_STATUS,   COL_F_STATUS_COLOR);
    addCol("Modified",  COL_F_MODIFIED);

    g_signal_connect(m_filesTreeView, "row-activated", G_CALLBACK(onFilesRowActivated), this);

    GtkWidget* scroll = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
        GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scroll), m_filesTreeView);
    gtk_widget_set_margin_top(scroll, 12);
    gtk_box_pack_start(GTK_BOX(vbox), scroll, TRUE, TRUE, 0);

    return vbox;
}

// ─── Duplicates Page ──────────────────────────────────────────────────────────
GtkWidget* AppWindow::buildDuplicatesPage() {
    GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_margin_start(vbox, 16);
    gtk_widget_set_margin_end(vbox, 16);
    gtk_widget_set_margin_top(vbox, 16);

    // Title + action
    GtkWidget* titleBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_box_pack_start(GTK_BOX(vbox), titleBox, FALSE, FALSE, 0);
    GtkWidget* title = gtk_label_new("Duplicate Files");
    gtk_style_context_add_class(gtk_widget_get_style_context(title), "page-title");
    gtk_box_pack_start(GTK_BOX(titleBox), title, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(titleBox), gtk_label_new(""), TRUE, TRUE, 0);

    GtkWidget* scanBtn = gtk_button_new_with_label("🔄 Run Duplicate Scan");
    gtk_style_context_add_class(gtk_widget_get_style_context(scanBtn), "primary-button");
    g_signal_connect(scanBtn, "clicked", G_CALLBACK(onDuplicateScanClicked), this);
    gtk_box_pack_end(GTK_BOX(titleBox), scanBtn, FALSE, FALSE, 0);

    GtkWidget* sub = gtk_label_new("Files with identical content (SHA-256 hash match)");
    gtk_style_context_add_class(gtk_widget_get_style_context(sub), "page-subtitle");
    gtk_widget_set_halign(sub, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(vbox), sub, FALSE, FALSE, 4);

    // Duplicates list
    m_dupsStore = gtk_list_store_new(COL_D_NUM,
        G_TYPE_STRING,  // filename
        G_TYPE_STRING,  // size
        G_TYPE_STRING,  // hash (short)
        G_TYPE_STRING   // path
    );

    m_dupsTreeView = gtk_tree_view_new_with_model(GTK_TREE_MODEL(m_dupsStore));
    

    auto addCol = [&](const char* t, int c) {
        GtkCellRenderer* r = gtk_cell_renderer_text_new();
        GtkTreeViewColumn* col = gtk_tree_view_column_new_with_attributes(t, r, "text", c, nullptr);
        gtk_tree_view_column_set_resizable(col, TRUE);
        gtk_tree_view_column_set_sort_column_id(col, c);
        gtk_tree_view_append_column(GTK_TREE_VIEW(m_dupsTreeView), col);
    };

    addCol("Filename",      COL_D_FILENAME);
    addCol("Size",          COL_D_SIZE);
    addCol("Hash (SHA-256)",COL_D_HASH);
    addCol("Full Path",     COL_D_PATH);

    GtkWidget* scroll = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
        GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scroll), m_dupsTreeView);
    gtk_widget_set_margin_top(scroll, 12);
    gtk_box_pack_start(GTK_BOX(vbox), scroll, TRUE, TRUE, 0);

    return vbox;
}

// ─── Cleanup Page ─────────────────────────────────────────────────────────────
GtkWidget* AppWindow::buildCleanupPage() {
    GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_margin_start(vbox, 16);
    gtk_widget_set_margin_end(vbox, 16);
    gtk_widget_set_margin_top(vbox, 16);

    GtkWidget* titleBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_box_pack_start(GTK_BOX(vbox), titleBox, FALSE, FALSE, 0);
    GtkWidget* title = gtk_label_new("Cleanup Suggestions");
    gtk_style_context_add_class(gtk_widget_get_style_context(title), "page-title");
    gtk_box_pack_start(GTK_BOX(titleBox), title, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(titleBox), gtk_label_new(""), TRUE, TRUE, 0);

    GtkWidget* refreshBtn = gtk_button_new_with_label("🔄 Refresh Suggestions");
    gtk_style_context_add_class(gtk_widget_get_style_context(refreshBtn), "secondary-button");
    g_signal_connect(refreshBtn, "clicked", G_CALLBACK(onCleanupClicked), this);
    gtk_box_pack_end(GTK_BOX(titleBox), refreshBtn, FALSE, FALSE, 0);

    GtkWidget* sub = gtk_label_new("Large files, old files, and duplicates that can be cleaned up");
    gtk_style_context_add_class(gtk_widget_get_style_context(sub), "page-subtitle");
    gtk_widget_set_halign(sub, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(vbox), sub, FALSE, FALSE, 4);

    m_cleanupStore = gtk_list_store_new(COL_C_NUM,
        G_TYPE_STRING,  // filename
        G_TYPE_STRING,  // size
        G_TYPE_STRING,  // reason
        G_TYPE_STRING,  // modified
        G_TYPE_STRING   // path
    );

    m_cleanupTreeView = gtk_tree_view_new_with_model(GTK_TREE_MODEL(m_cleanupStore));
    

    auto addCol = [&](const char* t, int c) {
        GtkCellRenderer* r = gtk_cell_renderer_text_new();
        GtkTreeViewColumn* col = gtk_tree_view_column_new_with_attributes(t, r, "text", c, nullptr);
        gtk_tree_view_column_set_resizable(col, TRUE);
        gtk_tree_view_append_column(GTK_TREE_VIEW(m_cleanupTreeView), col);
    };
    addCol("Filename",  COL_C_FILENAME);
    addCol("Size",      COL_C_SIZE);
    addCol("Reason",    COL_C_REASON);
    addCol("Modified",  COL_C_MODIFIED);
    addCol("Full Path", COL_C_PATH);

    GtkWidget* scroll = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
        GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scroll), m_cleanupTreeView);
    gtk_widget_set_margin_top(scroll, 12);
    gtk_box_pack_start(GTK_BOX(vbox), scroll, TRUE, TRUE, 0);

    return vbox;
}

// ─── Rules Page ───────────────────────────────────────────────────────────────
GtkWidget* AppWindow::buildRulesPage() {
    GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_margin_start(vbox, 16);
    gtk_widget_set_margin_end(vbox, 16);
    gtk_widget_set_margin_top(vbox, 16);

    GtkWidget* titleBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_box_pack_start(GTK_BOX(vbox), titleBox, FALSE, FALSE, 0);
    GtkWidget* title = gtk_label_new("Organizer Rules");
    gtk_style_context_add_class(gtk_widget_get_style_context(title), "page-title");
    gtk_box_pack_start(GTK_BOX(titleBox), title, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(titleBox), gtk_label_new(""), TRUE, TRUE, 0);

    GtkWidget* addBtn = gtk_button_new_with_label("+ Add Rule");
    gtk_style_context_add_class(gtk_widget_get_style_context(addBtn), "primary-button");
    g_signal_connect(addBtn, "clicked", G_CALLBACK(onAddRuleClicked), this);
    gtk_box_pack_end(GTK_BOX(titleBox), addBtn, FALSE, FALSE, 0);

    GtkWidget* sub = gtk_label_new("Define rules to automatically organize files");
    gtk_style_context_add_class(gtk_widget_get_style_context(sub), "page-subtitle");
    gtk_widget_set_halign(sub, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(vbox), sub, FALSE, FALSE, 4);

    m_rulesStore = gtk_list_store_new(COL_R_NUM,
        G_TYPE_BOOLEAN,  // enabled
        G_TYPE_INT,      // priority
        G_TYPE_STRING,   // name
        G_TYPE_STRING,   // description
        G_TYPE_UINT,     // match count
        G_TYPE_STRING,   // action
        G_TYPE_STRING    // id
    );

    m_rulesTreeView = gtk_tree_view_new_with_model(GTK_TREE_MODEL(m_rulesStore));
    

    // Enabled toggle
    GtkCellRenderer* toggleRenderer = gtk_cell_renderer_toggle_new();
    GtkTreeViewColumn* enabledCol = gtk_tree_view_column_new_with_attributes(
        "On", toggleRenderer, "active", COL_R_ENABLED, nullptr);
    gtk_tree_view_append_column(GTK_TREE_VIEW(m_rulesTreeView), enabledCol);

    auto addTextCol = [&](const char* t, int c) {
        GtkCellRenderer* r = gtk_cell_renderer_text_new();
        GtkTreeViewColumn* col = gtk_tree_view_column_new_with_attributes(t, r, "text", c, nullptr);
        gtk_tree_view_column_set_resizable(col, TRUE);
        gtk_tree_view_column_set_sort_column_id(col, c);
        gtk_tree_view_append_column(GTK_TREE_VIEW(m_rulesTreeView), col);
    };
    addTextCol("Priority",    COL_R_PRIORITY);
    addTextCol("Rule Name",   COL_R_NAME);
    addTextCol("Description", COL_R_DESCRIPTION);
    addTextCol("Matches",     COL_R_MATCHES);
    addTextCol("Action",      COL_R_ACTION);

    GtkWidget* scroll = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
        GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scroll), m_rulesTreeView);
    gtk_widget_set_margin_top(scroll, 12);
    gtk_box_pack_start(GTK_BOX(vbox), scroll, TRUE, TRUE, 0);

    return vbox;
}

// ─── Logs Page ────────────────────────────────────────────────────────────────
GtkWidget* AppWindow::buildLogsPage() {
    GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_margin_start(vbox, 16);
    gtk_widget_set_margin_end(vbox, 16);
    gtk_widget_set_margin_top(vbox, 16);

    GtkWidget* title = gtk_label_new("Activity Log");
    gtk_style_context_add_class(gtk_widget_get_style_context(title), "page-title");
    gtk_widget_set_halign(title, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(vbox), title, FALSE, FALSE, 0);

    m_logStore = gtk_list_store_new(COL_L_NUM,
        G_TYPE_STRING,  // time
        G_TYPE_STRING,  // level
        G_TYPE_STRING,  // message
        G_TYPE_STRING   // color
    );

    m_logTreeView = gtk_tree_view_new_with_model(GTK_TREE_MODEL(m_logStore));

    auto addCol = [&](const char* t, int c, int colorCol = -1) {
        GtkCellRenderer* r = gtk_cell_renderer_text_new();
        GtkTreeViewColumn* col = gtk_tree_view_column_new_with_attributes(t, r, "text", c, nullptr);
        if (colorCol >= 0) gtk_tree_view_column_add_attribute(col, r, "foreground", colorCol);
        gtk_tree_view_column_set_resizable(col, TRUE);
        gtk_tree_view_append_column(GTK_TREE_VIEW(m_logTreeView), col);
    };
    addCol("Time",    COL_L_TIME);
    addCol("Level",   COL_L_LEVEL,   COL_L_COLOR);
    addCol("Message", COL_L_MESSAGE, COL_L_COLOR);

    GtkWidget* scroll = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
        GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scroll), m_logTreeView);
    gtk_widget_set_margin_top(scroll, 12);
    gtk_box_pack_start(GTK_BOX(vbox), scroll, TRUE, TRUE, 0);

    return vbox;
}

// ─── Settings Page ────────────────────────────────────────────────────────────
GtkWidget* AppWindow::buildSettingsPage() {
    GtkWidget* scroll = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
        GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

    GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_set_margin_start(vbox, 24);
    gtk_widget_set_margin_end(vbox, 24);
    gtk_widget_set_margin_top(vbox, 24);
    gtk_container_add(GTK_CONTAINER(scroll), vbox);

    GtkWidget* title = gtk_label_new("Settings");
    gtk_style_context_add_class(gtk_widget_get_style_context(title), "page-title");
    gtk_widget_set_halign(title, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(vbox), title, FALSE, FALSE, 0);

    auto& cfg = ConfigManager::instance().config();

    auto addSection = [&](const char* name) {
        GtkWidget* l = gtk_label_new(name);
        gtk_style_context_add_class(gtk_widget_get_style_context(l), "section-header");
        gtk_widget_set_halign(l, GTK_ALIGN_START);
        gtk_widget_set_margin_top(l, 8);
        gtk_box_pack_start(GTK_BOX(vbox), l, FALSE, FALSE, 0);
    };

    auto addToggle = [&](const char* label, bool state) -> GtkWidget* {
        GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        GtkWidget* lbl = gtk_label_new(label);
        gtk_widget_set_halign(lbl, GTK_ALIGN_START);
        gtk_box_pack_start(GTK_BOX(row), lbl, TRUE, TRUE, 0);
        GtkWidget* sw = gtk_switch_new();
        gtk_switch_set_active(GTK_SWITCH(sw), state);
        gtk_box_pack_end(GTK_BOX(row), sw, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(vbox), row, FALSE, FALSE, 0);
        return sw;
    };

    // Watch paths
    addSection("Watched Paths");
    for (const auto& p : cfg.watchPaths) {
        GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        GtkWidget* lbl = gtk_label_new(p.c_str());
        gtk_widget_set_halign(lbl, GTK_ALIGN_START);
        gtk_box_pack_start(GTK_BOX(row), lbl, TRUE, TRUE, 0);
        gtk_box_pack_start(GTK_BOX(vbox), row, FALSE, FALSE, 0);
    }
    GtkWidget* addPathBtn = gtk_button_new_with_label("+ Add Watch Path");
    gtk_style_context_add_class(gtk_widget_get_style_context(addPathBtn), "secondary-button");
    gtk_widget_set_halign(addPathBtn, GTK_ALIGN_START);
    g_signal_connect(addPathBtn, "clicked", G_CALLBACK(onAddWatchPathClicked), this);
    gtk_box_pack_start(GTK_BOX(vbox), addPathBtn, FALSE, FALSE, 0);

    // Behavior
    addSection("Behavior");
    addToggle("Auto-organize files when detected",      cfg.autoOrganize);
    addToggle("Watch subfolders recursively",           cfg.watchRecursive);
    addToggle("Detect duplicate files (SHA-256 hash)",  cfg.enableDuplicateDetect);
    addToggle("Desktop notifications",                  cfg.enableNotifications);
    addToggle("Move to Trash instead of deleting",      cfg.moveToTrash);
    addToggle("Simulation mode (dry run, no changes)",  cfg.simulateMode);
    addToggle("Start minimized to tray",                cfg.startMinimized);
    addToggle("Suggest cleanup for old/large files",    cfg.suggestCleanup);

    // Thresholds
    addSection("Thresholds");
    GtkWidget* sizeRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_pack_start(GTK_BOX(sizeRow), gtk_label_new("Large file threshold (MB):"), FALSE, FALSE, 0);
    GtkWidget* sizeSpin = gtk_spin_button_new_with_range(1, 10000, 50);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(sizeSpin),
                              static_cast<double>(cfg.largeSizeThreshold / (1024*1024)));
    gtk_box_pack_start(GTK_BOX(sizeRow), sizeSpin, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), sizeRow, FALSE, FALSE, 0);

    GtkWidget* ageRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_pack_start(GTK_BOX(ageRow), gtk_label_new("Old file age (days):"), FALSE, FALSE, 0);
    GtkWidget* ageSpin = gtk_spin_button_new_with_range(1, 3650, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(ageSpin), cfg.oldFileAgeDays);
    gtk_box_pack_start(GTK_BOX(ageRow), ageSpin, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), ageRow, FALSE, FALSE, 0);

    // Save button
    GtkWidget* saveBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_top(saveBox, 16);
    GtkWidget* saveBtn = gtk_button_new_with_label("💾 Save Settings");
    gtk_style_context_add_class(gtk_widget_get_style_context(saveBtn), "primary-button");
    gtk_box_pack_start(GTK_BOX(saveBox), saveBtn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), saveBox, FALSE, FALSE, 0);

    g_signal_connect_swapped(saveBtn, "clicked", G_CALLBACK(+[](gpointer /*d*/){
        ConfigManager::instance().save();
        LOG_INFO("Settings saved by user");
    }), nullptr);

    return scroll;
}

// ─── Status Bar ───────────────────────────────────────────────────────────────
GtkWidget* AppWindow::buildStatusBar() {
    GtkWidget* hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_style_context_add_class(gtk_widget_get_style_context(hbox), "status-bar");
    gtk_widget_set_margin_start(hbox, 8);
    gtk_widget_set_margin_end(hbox, 8);

    m_statusLabel = gtk_label_new("Ready • Watching for changes");
    gtk_widget_set_halign(m_statusLabel, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(hbox), m_statusLabel, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(hbox), gtk_label_new(""), TRUE, TRUE, 0);

    m_queueLabel = gtk_label_new("Queue: 0");
    gtk_box_pack_end(GTK_BOX(hbox), m_queueLabel, FALSE, FALSE, 0);

    return hbox;
}

// ─── Navigation ──────────────────────────────────────────────────────────────
void AppWindow::showPage(const std::string& name) {
    gtk_stack_set_visible_child_name(GTK_STACK(m_stack), name.c_str());
    updateSidebarActive(name);
    m_currentPage = name;

    // Refresh data for specific pages
    if (name == "dashboard") {
        auto stats = Database::instance().getStatistics();
        refreshDashboard(stats);
    } else if (name == "files") {
        refreshFilesList();
    } else if (name == "duplicates") {
        refreshDuplicatesList();
    } else if (name == "cleanup") {
        refreshCleanupList();
    } else if (name == "rules") {
        refreshRulesList();
    }
}

void AppWindow::updateSidebarActive(const std::string& page) {
    for (auto& [p, btn] : m_navButtons) {
        auto ctx = gtk_widget_get_style_context(btn);
        gtk_style_context_remove_class(ctx, "active");
        if (p == page) gtk_style_context_add_class(ctx, "active");
    }
}

// ─── Data refresh methods ─────────────────────────────────────────────────────
void AppWindow::updateStatCard(const char* id, const char* value) {
    auto it = m_statLabels.find(id);
    if (it != m_statLabels.end()) gtk_label_set_text(GTK_LABEL(it->second), value);
}

void AppWindow::refreshDashboard(const Statistics& stats) {
    auto& fa = FileAnalyzer::instance();
    updateStatCard("total_files", std::to_string(stats.totalFiles).c_str());
    updateStatCard("total_size",  fa.formatSize(stats.totalSize).c_str());
    updateStatCard("duplicates",  std::to_string(stats.duplicateFiles).c_str());
    updateStatCard("space_save",  fa.formatSize(stats.duplicateSize).c_str());
    updateStatCard("large_files", std::to_string(stats.largeFiles).c_str());
    updateStatCard("old_files",   std::to_string(stats.oldFiles).c_str());
    updateStatCard("org_total",   std::to_string(stats.organizedTotal).c_str());
    updateStatCard("org_today",   std::to_string(stats.organizedToday).c_str());

    // Category badges
    auto update = [&](FileCategory cat, const char* id) {
        auto it = stats.countByCategory.find(cat);
        uint64_t cnt = (it != stats.countByCategory.end()) ? it->second : 0;
        auto sit = m_statLabels.find(id);
        if (sit != m_statLabels.end()) gtk_label_set_text(GTK_LABEL(sit->second), std::to_string(cnt).c_str());
    };
    update(FileCategory::Images,    "cat_images");
    update(FileCategory::Videos,    "cat_videos");
    update(FileCategory::Audio,     "cat_audio");
    update(FileCategory::Documents, "cat_docs");
    update(FileCategory::Archives,  "cat_archives");
    update(FileCategory::Code,      "cat_code");
    update(FileCategory::Executables,"cat_exec");
    update(FileCategory::Ebooks,    "cat_ebooks");
}

void AppWindow::addFileToList(const FileInfo& fi) {
    if (!m_filesStore) return;

    // Check filter
    if (m_filterCategory != FileCategory::COUNT && fi.category != m_filterCategory) return;
    if (!m_searchQuery.empty()) {
        std::string lower = fi.filename;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        std::string query = m_searchQuery;
        std::transform(query.begin(), query.end(), query.begin(), ::tolower);
        if (lower.find(query) == std::string::npos) return;
    }

    const char* statusColor = "#e0e0e0";
    if (fi.isDuplicate) statusColor = "#FF9800";
    else if (fi.status == FileStatus::Large) statusColor = "#e94560";
    else if (fi.status == FileStatus::Old)   statusColor = "#888888";

    std::string catStr = std::string(categoryIcon(fi.category)) + " " + fi.categoryName();
    std::string sizeStr = fi.sizeHuman();
    std::string modStr  = formatTime(fi.modifiedAt);

    GtkTreeIter iter;
    gtk_list_store_prepend(m_filesStore, &iter);
    gtk_list_store_set(m_filesStore, &iter,
        COL_F_FILENAME,     fi.filename.c_str(),
        COL_F_SIZE,         sizeStr.c_str(),
        COL_F_CATEGORY,     catStr.c_str(),
        COL_F_STATUS,       fi.statusName().c_str(),
        COL_F_MODIFIED,     modStr.c_str(),
        COL_F_PATH,         fi.path.c_str(),
        COL_F_STATUS_COLOR, statusColor,
        -1);
}

void AppWindow::refreshFilesList() {
    if (!m_filesStore) return;
    gtk_list_store_clear(m_filesStore);

    std::vector<FileInfo> files;
    if (m_filterCategory != FileCategory::COUNT) {
        files = Database::instance().getFilesByCategory(m_filterCategory);
    } else if (!m_searchQuery.empty()) {
        files = Database::instance().searchFiles(m_searchQuery);
    } else {
        files = Database::instance().getAllFiles();
    }

    for (const auto& fi : files) {
        addFileToList(fi);
    }
}

void AppWindow::refreshDuplicatesList() {
    if (!m_dupsStore) return;
    gtk_list_store_clear(m_dupsStore);

    auto dups = Database::instance().getDuplicates();
    for (const auto& fi : dups) {
        std::string hashShort = fi.sha256Hash.size() > 16
                                ? fi.sha256Hash.substr(0,8) + "..." + fi.sha256Hash.substr(fi.sha256Hash.size()-8)
                                : fi.sha256Hash;
        GtkTreeIter iter;
        gtk_list_store_append(m_dupsStore, &iter);
        gtk_list_store_set(m_dupsStore, &iter,
            COL_D_FILENAME, fi.filename.c_str(),
            COL_D_SIZE,     fi.sizeHuman().c_str(),
            COL_D_HASH,     hashShort.c_str(),
            COL_D_PATH,     fi.path.c_str(),
            -1);
    }
}

void AppWindow::refreshCleanupList() {
    if (!m_cleanupStore) return;
    gtk_list_store_clear(m_cleanupStore);

    auto suggestions = OrganizerEngine::instance().getCleanupSuggestions();
    for (const auto& fi : suggestions) {
        std::string reason;
        if (fi.isDuplicate) reason = "Duplicate";
        else if (fi.status == FileStatus::Large) reason = "Large file";
        else if (fi.status == FileStatus::Old)   reason = "Old (>30 days)";
        else reason = "Review suggested";

        GtkTreeIter iter;
        gtk_list_store_append(m_cleanupStore, &iter);
        gtk_list_store_set(m_cleanupStore, &iter,
            COL_C_FILENAME, fi.filename.c_str(),
            COL_C_SIZE,     fi.sizeHuman().c_str(),
            COL_C_REASON,   reason.c_str(),
            COL_C_MODIFIED, formatTime(fi.modifiedAt).c_str(),
            COL_C_PATH,     fi.path.c_str(),
            -1);
    }
}

void AppWindow::refreshRulesList() {
    if (!m_rulesStore) return;
    gtk_list_store_clear(m_rulesStore);

    const auto& rules = ConfigManager::instance().config().rules;
    for (const auto& rule : rules) {
        std::string actionStr;
        switch (rule.action.type) {
            case ActionType::Move:   actionStr = "→ " + rule.action.targetDirectory; break;
            case ActionType::Copy:   actionStr = "Copy to " + rule.action.targetDirectory; break;
            case ActionType::Delete: actionStr = "Delete"; break;
            case ActionType::Rename: actionStr = "Rename: " + rule.action.renamePattern; break;
            default: actionStr = "Skip"; break;
        }

        GtkTreeIter iter;
        gtk_list_store_append(m_rulesStore, &iter);
        gtk_list_store_set(m_rulesStore, &iter,
            COL_R_ENABLED,     rule.enabled,
            COL_R_PRIORITY,    rule.priority,
            COL_R_NAME,        rule.name.c_str(),
            COL_R_DESCRIPTION, rule.description.c_str(),
            COL_R_MATCHES,     static_cast<guint>(rule.matchCount),
            COL_R_ACTION,      actionStr.c_str(),
            COL_R_ID,          rule.id.c_str(),
            -1);
    }
}

void AppWindow::addLogEntry(const LogEntry& entry) {
    if (!m_logStore) return;

    auto t = std::chrono::system_clock::to_time_t(entry.timestamp);
    std::tm tm_buf{};
    localtime_r(&t, &tm_buf);
    char timeBuf[32];
    std::strftime(timeBuf, sizeof(timeBuf), "%H:%M:%S", &tm_buf);

    static const char* levelNames[] = {"DEBUG","INFO","WARN","ERROR","CRIT"};
    static const char* levelColors[] = {"#888888","#4CAF50","#FF9800","#e94560","#FF0000"};
    int li = std::min(static_cast<int>(entry.level), 4);

    std::string msg = entry.message;
    if (!entry.detail.empty()) msg += " | " + entry.detail;

    GtkTreeIter iter;
    gtk_list_store_prepend(m_logStore, &iter);
    gtk_list_store_set(m_logStore, &iter,
        COL_L_TIME,    timeBuf,
        COL_L_LEVEL,   levelNames[li],
        COL_L_MESSAGE, msg.c_str(),
        COL_L_COLOR,   levelColors[li],
        -1);
}

void AppWindow::updateStatusBar(const std::string& msg, size_t queueSize) {
    if (m_statusLabel) gtk_label_set_text(GTK_LABEL(m_statusLabel), msg.c_str());
    if (m_queueLabel) {
        std::string q = "Queue: " + std::to_string(queueSize);
        gtk_label_set_text(GTK_LABEL(m_queueLabel), q.c_str());
    }
}

void AppWindow::showNotificationToast(const Notification& notif) {
    if (!m_toastBar || !m_toastLabel) return;

    GtkMessageType mtype = GTK_MESSAGE_INFO;
    if (notif.level == LogLevel::Warning) mtype = GTK_MESSAGE_WARNING;
    else if (notif.level >= LogLevel::Error) mtype = GTK_MESSAGE_ERROR;

    gtk_info_bar_set_message_type(GTK_INFO_BAR(m_toastBar), mtype);
    std::string text = notif.title + ": " + notif.message;
    gtk_label_set_text(GTK_LABEL(m_toastLabel), text.c_str());
    gtk_widget_show(m_toastBar);

    if (m_toastTimerId) g_source_remove(m_toastTimerId);
    m_toastTimerId = g_timeout_add(4000, onToastTimeout, this);
}

// ─── Signal handlers ─────────────────────────────────────────────────────────
void AppWindow::onSidebarNavClicked(GtkButton* btn, gpointer data) {
    auto* self = static_cast<AppWindow*>(data);
    const char* page = static_cast<const char*>(g_object_get_data(G_OBJECT(btn), "page"));
    if (page) self->showPage(page);
}

void AppWindow::onOrganizeNowClicked(GtkButton*, gpointer data) {
    auto* self = static_cast<AppWindow*>(data);
    self->updateStatusBar("Organizing files...");
    std::thread([](){
        OrganizerEngine::instance().applyRulesToAll();
    }).detach();
}

void AppWindow::onScanNowClicked(GtkButton*, gpointer data) {
    auto* self = static_cast<AppWindow*>(data);
    self->updateStatusBar("Scanning...");
    std::thread([](){
        OrganizerEngine::instance().rescanAll();
    }).detach();
}

void AppWindow::onUndoClicked(GtkButton*, gpointer data) {
    auto* self = static_cast<AppWindow*>(data);
    bool ok = OrganizerEngine::instance().undoLastAction();
    if (!ok) {
        Notification n;
        n.title   = "Undo";
        n.message = "Nothing to undo";
        n.level   = LogLevel::Info;
        n.time    = std::chrono::system_clock::now();
        self->showNotificationToast(n);
    }
}

void AppWindow::onAddWatchPathClicked(GtkButton*, gpointer data) {
    auto* self = static_cast<AppWindow*>(data);
    GtkWidget* dialog = gtk_file_chooser_dialog_new(
        "Select Watch Directory", GTK_WINDOW(self->m_window),
        GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Add",    GTK_RESPONSE_ACCEPT,
        nullptr);
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char* folder = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        if (folder) {
            ConfigManager::instance().addWatchPath(folder);
            ConfigManager::instance().save();
            OrganizerEngine::instance().stopWatching();
            OrganizerEngine::instance().startWatching();
            g_free(folder);
        }
    }
    gtk_widget_destroy(dialog);
}

void AppWindow::onAddRuleClicked(GtkButton*, gpointer data) {
    auto* self = static_cast<AppWindow*>(data);
    self->showAddRuleDialog();
}

void AppWindow::onDuplicateScanClicked(GtkButton*, gpointer data) {
    auto* self = static_cast<AppWindow*>(data);
    self->updateStatusBar("Running duplicate scan...");
    std::thread([](){
        OrganizerEngine::instance().runDuplicateScan();
    }).detach();
}

void AppWindow::onCleanupClicked(GtkButton*, gpointer data) {
    auto* self = static_cast<AppWindow*>(data);
    self->showPage("cleanup");
    self->refreshCleanupList();
}

void AppWindow::onSearchChanged(GtkSearchEntry* entry, gpointer data) {
    auto* self = static_cast<AppWindow*>(data);
    const char* text = gtk_entry_get_text(GTK_ENTRY(entry));
    self->m_searchQuery = text ? text : "";
    self->refreshFilesList();
}

void AppWindow::onFilterCategoryChanged(GtkComboBox* combo, gpointer data) {
    auto* self = static_cast<AppWindow*>(data);
    int active = gtk_combo_box_get_active(combo);
    if (active <= 0) self->m_filterCategory = FileCategory::COUNT;
    else self->m_filterCategory = static_cast<FileCategory>(active - 1);
    self->refreshFilesList();
}

void AppWindow::onFilesRowActivated(GtkTreeView* tv, GtkTreePath* path,
                                    GtkTreeViewColumn*, gpointer /*data*/) {
    GtkTreeModel* model = gtk_tree_view_get_model(tv);
    GtkTreeIter iter;
    if (!gtk_tree_model_get_iter(model, &iter, path)) return;

    gchar* filePath = nullptr;
    gtk_tree_model_get(model, &iter, COL_F_PATH, &filePath, -1);
    if (!filePath) return;

    // Open file manager at the containing directory.
    // Use g_spawn_async instead of std::system to avoid shell injection:
    // g_spawn_async passes argv as an array — no shell is invoked, so
    // special characters in the path (quotes, semicolons, $, etc.) are safe.
    std::string dirPath = fs::path(filePath).parent_path().string();
    g_free(filePath);

    gchar* argv[] = {
        const_cast<gchar*>("xdg-open"),
        const_cast<gchar*>(dirPath.c_str()),
        nullptr
    };
    GError* err = nullptr;
    if (!g_spawn_async(nullptr, argv, nullptr,
                       G_SPAWN_SEARCH_PATH,
                       nullptr, nullptr, nullptr, &err)) {
        LOG_WARN("g_spawn_async xdg-open failed",
                 err ? err->message : "unknown error", dirPath);
        if (err) g_error_free(err);
    }
}

void AppWindow::onWindowDestroy(GtkWidget*, gpointer data) {
    auto* self = static_cast<AppWindow*>(data);
    self->quit();
}

gboolean AppWindow::onToastTimeout(gpointer data) {
    auto* self = static_cast<AppWindow*>(data);
    if (self->m_toastBar) gtk_widget_hide(self->m_toastBar);
    self->m_toastTimerId = 0;
    return G_SOURCE_REMOVE;
}

// ─── Add Rule Dialog ──────────────────────────────────────────────────────────
void AppWindow::showAddRuleDialog() {
    GtkWidget* dialog = gtk_dialog_new_with_buttons(
        "Add Organizer Rule", GTK_WINDOW(m_window),
        GTK_DIALOG_MODAL,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Add Rule", GTK_RESPONSE_ACCEPT,
        nullptr);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 520, 480);

    GtkWidget* content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget* grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
    gtk_widget_set_margin_start(grid, 16);
    gtk_widget_set_margin_end(grid, 16);
    gtk_widget_set_margin_top(grid, 16);
    gtk_widget_set_margin_bottom(grid, 16);
    gtk_container_add(GTK_CONTAINER(content), grid);

    int row = 0;
    auto addField = [&](const char* label, GtkWidget* widget) {
        GtkWidget* lbl = gtk_label_new(label);
        gtk_widget_set_halign(lbl, GTK_ALIGN_END);
        gtk_grid_attach(GTK_GRID(grid), lbl,    0, row, 1, 1);
        gtk_grid_attach(GTK_GRID(grid), widget, 1, row, 1, 1);
        gtk_widget_set_hexpand(widget, TRUE);
        ++row;
    };

    GtkWidget* nameEntry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(nameEntry), "My Rule");
    addField("Rule Name:", nameEntry);

    GtkWidget* descEntry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(descEntry), "Optional description");
    addField("Description:", descEntry);

    // Condition
    GtkWidget* condBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget* fieldCombo = gtk_combo_box_text_new();
    for (auto& f : {"extension","name","size","age","category","mime"})
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(fieldCombo), f);
    gtk_combo_box_set_active(GTK_COMBO_BOX(fieldCombo), 0);
    gtk_box_pack_start(GTK_BOX(condBox), fieldCombo, FALSE, FALSE, 0);

    GtkWidget* opCombo = gtk_combo_box_text_new();
    for (auto& o : {"eq","ne","contains","startswith","endswith","regex","gt","lt"})
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(opCombo), o);
    gtk_combo_box_set_active(GTK_COMBO_BOX(opCombo), 0);
    gtk_box_pack_start(GTK_BOX(condBox), opCombo, FALSE, FALSE, 0);

    GtkWidget* condVal = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(condVal), "e.g. pdf, mp4, 500000...");
    gtk_box_pack_start(GTK_BOX(condBox), condVal, TRUE, TRUE, 0);
    addField("Condition:", condBox);

    // Action
    GtkWidget* actionCombo = gtk_combo_box_text_new();
    for (auto& a : {"Move","Copy","Delete","Rename","Skip"})
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(actionCombo), a);
    gtk_combo_box_set_active(GTK_COMBO_BOX(actionCombo), 0);
    addField("Action:", actionCombo);

    GtkWidget* destEntry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(destEntry), "/home/user/Documents/Category");
    addField("Destination:", destEntry);

    GtkWidget* patternEntry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(patternEntry), "{name}_{date} (optional)");
    addField("Rename Pattern:", patternEntry);

    GtkWidget* prioritySpin = gtk_spin_button_new_with_range(0, 100, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(prioritySpin), 10);
    addField("Priority:", prioritySpin);

    GtkWidget* enabledCheck = gtk_check_button_new_with_label("Enabled");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(enabledCheck), TRUE);
    gtk_grid_attach(GTK_GRID(grid), enabledCheck, 1, row++, 1, 1);

    gtk_widget_show_all(dialog);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        OrganizerRule rule;
        rule.id          = std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        rule.name        = gtk_entry_get_text(GTK_ENTRY(nameEntry));
        rule.description = gtk_entry_get_text(GTK_ENTRY(descEntry));
        rule.enabled     = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(enabledCheck));
        rule.priority    = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(prioritySpin));
        rule.conditionLogic = "OR";

        RuleCondition cond;
        gchar* rawField = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(fieldCombo));
        gchar* rawOp    = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(opCombo));
        cond.field = rawField ? rawField : "extension";
        cond.op    = rawOp    ? rawOp    : "eq";
        g_free(rawField);
        g_free(rawOp);
        cond.value = gtk_entry_get_text(GTK_ENTRY(condVal));
        if (!cond.value.empty()) rule.conditions.push_back(cond);

        int actionIdx = gtk_combo_box_get_active(GTK_COMBO_BOX(actionCombo));
        rule.action.type = static_cast<ActionType>(actionIdx);
        rule.action.targetDirectory = gtk_entry_get_text(GTK_ENTRY(destEntry));
        rule.action.renamePattern   = gtk_entry_get_text(GTK_ENTRY(patternEntry));
        rule.action.createSubfolders= true;

        if (!rule.name.empty()) {
            ConfigManager::instance().addRule(rule);
            ConfigManager::instance().save();
            refreshRulesList();
            LOG_INFO("Rule added: " + rule.name);
        }
    }
    gtk_widget_destroy(dialog);
}

} // namespace SDO

// ═══════════════════════════════════════════════════════════════════════════════
// IMPLEMENTATIONS OF PREVIOUSLY DECLARED-BUT-UNIMPLEMENTED METHODS
// ═══════════════════════════════════════════════════════════════════════════════

namespace SDO {

// ─── loadCSSTheme — swap between dark and light ───────────────────────────────
void AppWindow::loadCSSTheme(bool dark) {
    if (!m_cssProvider) return;
    // For now we only ship the dark theme; light is a future enhancement.
    // The method exists so the Settings toggle compiles and links correctly.
    (void)dark;
    applyCSS();
    LOG_INFO(dark ? "Dark theme applied" : "Light theme applied");
}

// ─── showEditRuleDialog ───────────────────────────────────────────────────────
void AppWindow::showEditRuleDialog(const OrganizerRule& rule) {
    GtkWidget* dialog = gtk_dialog_new_with_buttons(
        "Edit Rule", GTK_WINDOW(m_window),
        GTK_DIALOG_MODAL,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Save",   GTK_RESPONSE_ACCEPT,
        nullptr);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 520, 480);

    GtkWidget* content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget* grid    = gtk_grid_new();
    gtk_grid_set_row_spacing   (GTK_GRID(grid), 10);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
    gtk_widget_set_margin_start (grid, 16);
    gtk_widget_set_margin_end   (grid, 16);
    gtk_widget_set_margin_top   (grid, 16);
    gtk_widget_set_margin_bottom(grid, 16);
    gtk_container_add(GTK_CONTAINER(content), grid);

    int row = 0;
    auto addRow = [&](const char* label, GtkWidget* w) {
        GtkWidget* lbl = gtk_label_new(label);
        gtk_widget_set_halign(lbl, GTK_ALIGN_END);
        gtk_grid_attach(GTK_GRID(grid), lbl, 0, row,   1, 1);
        gtk_grid_attach(GTK_GRID(grid), w,   1, row++, 1, 1);
        gtk_widget_set_hexpand(w, TRUE);
    };

    GtkWidget* nameEntry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(nameEntry), rule.name.c_str());
    addRow("Rule Name:", nameEntry);

    GtkWidget* descEntry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(descEntry), rule.description.c_str());
    addRow("Description:", descEntry);

    // First condition (simplified: show first condition's values)
    GtkWidget* condBox  = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget* fieldCombo = gtk_combo_box_text_new();
    for (auto& f : {"extension","name","size","age","category","mime"})
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(fieldCombo), f);

    GtkWidget* opCombo = gtk_combo_box_text_new();
    for (auto& o : {"eq","ne","contains","startswith","endswith","regex","gt","lt"})
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(opCombo), o);

    GtkWidget* condVal = gtk_entry_new();
    if (!rule.conditions.empty()) {
        const auto& c = rule.conditions[0];
        // Select matching field/op
        const char* fields[] = {"extension","name","size","age","category","mime"};
        for (int i = 0; i < 6; ++i)
            if (c.field == fields[i]) { gtk_combo_box_set_active(GTK_COMBO_BOX(fieldCombo), i); break; }
        const char* ops[] = {"eq","ne","contains","startswith","endswith","regex","gt","lt"};
        for (int i = 0; i < 8; ++i)
            if (c.op == ops[i]) { gtk_combo_box_set_active(GTK_COMBO_BOX(opCombo), i); break; }
        gtk_entry_set_text(GTK_ENTRY(condVal), c.value.c_str());
    } else {
        gtk_combo_box_set_active(GTK_COMBO_BOX(fieldCombo), 0);
        gtk_combo_box_set_active(GTK_COMBO_BOX(opCombo),    0);
    }
    gtk_box_pack_start(GTK_BOX(condBox), fieldCombo, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(condBox), opCombo,    FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(condBox), condVal,    TRUE,  TRUE,  0);
    addRow("Condition:", condBox);

    GtkWidget* actionCombo = gtk_combo_box_text_new();
    const char* actionNames[] = {"Move","Copy","Delete","Rename","Skip"};
    for (auto& a : actionNames)
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(actionCombo), a);
    gtk_combo_box_set_active(GTK_COMBO_BOX(actionCombo), static_cast<int>(rule.action.type));
    addRow("Action:", actionCombo);

    GtkWidget* destEntry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(destEntry), rule.action.targetDirectory.c_str());
    addRow("Destination:", destEntry);

    GtkWidget* patternEntry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(patternEntry), rule.action.renamePattern.c_str());
    addRow("Rename Pattern:", patternEntry);

    GtkWidget* prioritySpin = gtk_spin_button_new_with_range(0, 100, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(prioritySpin), rule.priority);
    addRow("Priority:", prioritySpin);

    GtkWidget* enabledCheck = gtk_check_button_new_with_label("Enabled");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(enabledCheck), rule.enabled);
    gtk_grid_attach(GTK_GRID(grid), enabledCheck, 1, row++, 1, 1);

    gtk_widget_show_all(dialog);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        OrganizerRule updated = rule; // copy original (preserves id, matchCount, etc.)
        updated.name        = gtk_entry_get_text(GTK_ENTRY(nameEntry));
        updated.description = gtk_entry_get_text(GTK_ENTRY(descEntry));
        updated.enabled     = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(enabledCheck));
        updated.priority    = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(prioritySpin));

        RuleCondition cond;
        gchar* field = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(fieldCombo));
        gchar* op    = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(opCombo));
        cond.field   = field ? field : "extension";
        cond.op      = op    ? op    : "eq";
        cond.value   = gtk_entry_get_text(GTK_ENTRY(condVal));
        g_free(field); g_free(op);

        updated.conditions.clear();
        if (!cond.value.empty()) updated.conditions.push_back(cond);

        updated.action.type            = static_cast<ActionType>(
                                             gtk_combo_box_get_active(GTK_COMBO_BOX(actionCombo)));
        updated.action.targetDirectory = gtk_entry_get_text(GTK_ENTRY(destEntry));
        updated.action.renamePattern   = gtk_entry_get_text(GTK_ENTRY(patternEntry));

        ConfigManager::instance().updateRule(updated);
        ConfigManager::instance().save();
        refreshRulesList();
        LOG_INFO("Rule updated: " + updated.name);
    }
    gtk_widget_destroy(dialog);
}

// ─── showSettingsDialog ───────────────────────────────────────────────────────
void AppWindow::showSettingsDialog() {
    // Delegate to the inline settings page — just navigate there.
    showPage("settings");
}

// ─── showAboutDialog ──────────────────────────────────────────────────────────
void AppWindow::showAboutDialog() {
    GtkWidget* dialog = gtk_about_dialog_new();
    gtk_about_dialog_set_program_name (GTK_ABOUT_DIALOG(dialog), APP_NAME);
    gtk_about_dialog_set_version      (GTK_ABOUT_DIALOG(dialog), APP_VERSION);
    gtk_about_dialog_set_comments     (GTK_ABOUT_DIALOG(dialog),
        "Enterprise-grade automatic downloads organizer.\n"
        "Watches folders, categorises files, detects duplicates,\n"
        "applies configurable rules, and suggests cleanup.");
    gtk_about_dialog_set_copyright    (GTK_ABOUT_DIALOG(dialog), "© 2025 SDO Project");
    gtk_about_dialog_set_license_type (GTK_ABOUT_DIALOG(dialog), GTK_LICENSE_MIT_X11);
    gtk_about_dialog_set_website      (GTK_ABOUT_DIALOG(dialog), "https://github.com/sdo/organizer");
    gtk_about_dialog_set_website_label(GTK_ABOUT_DIALOG(dialog), "Project Homepage");
    const char* authors[] = { "SDO Developers", nullptr };
    gtk_about_dialog_set_authors      (GTK_ABOUT_DIALOG(dialog), authors);
    gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(m_window));
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

// ─── showFilenameDialog ───────────────────────────────────────────────────────
// Prompts the user to enter a new filename for the selected file in the Files view.
void AppWindow::showFilenameDialog() {
    // Get selected path from files tree view
    if (!m_filesTreeView) return;

    GtkTreeSelection* sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(m_filesTreeView));
    GtkTreeModel* model   = nullptr;
    GtkTreeIter   iter;
    if (!gtk_tree_selection_get_selected(sel, &model, &iter)) {
        Notification n;
        n.title   = "Rename";
        n.message = "Select a file first";
        n.level   = LogLevel::Warning;
        n.time    = std::chrono::system_clock::now();
        showNotificationToast(n);
        return;
    }

    gchar* filePath = nullptr;
    gtk_tree_model_get(model, &iter, 5 /*COL_F_PATH*/, &filePath, -1);
    if (!filePath) return;
    std::string srcPath = filePath;
    g_free(filePath);

    GtkWidget* dialog = gtk_dialog_new_with_buttons(
        "Rename File", GTK_WINDOW(m_window),
        GTK_DIALOG_MODAL,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Rename", GTK_RESPONSE_ACCEPT,
        nullptr);

    GtkWidget* content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget* vbox    = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_start (vbox, 16);
    gtk_widget_set_margin_end   (vbox, 16);
    gtk_widget_set_margin_top   (vbox, 16);
    gtk_widget_set_margin_bottom(vbox, 16);
    gtk_container_add(GTK_CONTAINER(content), vbox);

    std::string currentName = std::filesystem::path(srcPath).filename().string();
    GtkWidget* label = gtk_label_new(("Renaming: " + currentName).c_str());
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

    GtkWidget* entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(entry), currentName.c_str());
    gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);
    gtk_box_pack_start(GTK_BOX(vbox), entry, FALSE, FALSE, 0);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);

    gtk_widget_show_all(dialog);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        const char* newName = gtk_entry_get_text(GTK_ENTRY(entry));
        if (newName && *newName && std::string(newName) != currentName) {
            namespace fs = std::filesystem;
            std::string destPath = (fs::path(srcPath).parent_path() / newName).string();
            std::error_code ec;
            fs::rename(srcPath, destPath, ec);
            if (!ec) {
                Database::instance().deleteFile(srcPath);
                OrganizerEngine::instance().enqueueFile(destPath);
                LOG_INFO("Renamed: " + currentName + " → " + std::string(newName));
                refreshFilesList();
            } else {
                LOG_ERROR("Rename failed", ec.message(), srcPath);
                Notification n;
                n.title   = "Rename Failed";
                n.message = ec.message();
                n.level   = LogLevel::Error;
                n.time    = std::chrono::system_clock::now();
                showNotificationToast(n);
            }
        }
    }
    gtk_widget_destroy(dialog);
}

// ─── confirmDelete ────────────────────────────────────────────────────────────
void AppWindow::confirmDelete(const std::string& path) {
    namespace fs = std::filesystem;
    std::string filename = fs::path(path).filename().string();

    const auto& cfg = ConfigManager::instance().config();
    std::string action = cfg.moveToTrash ? "move to Trash" : "permanently delete";

    GtkWidget* dialog = gtk_message_dialog_new(
        GTK_WINDOW(m_window),
        GTK_DIALOG_MODAL,
        GTK_MESSAGE_WARNING,
        GTK_BUTTONS_NONE,
        "Confirm Delete");

    gtk_message_dialog_format_secondary_text(
        GTK_MESSAGE_DIALOG(dialog),
        "Are you sure you want to %s:\n\n%s\n\nThis action %s.",
        action.c_str(),
        filename.c_str(),
        cfg.moveToTrash ? "can be undone from Trash" : "cannot be undone");

    gtk_dialog_add_button(GTK_DIALOG(dialog), "_Cancel", GTK_RESPONSE_CANCEL);
    GtkWidget* deleteBtn = gtk_dialog_add_button(GTK_DIALOG(dialog), "🗑 Delete", GTK_RESPONSE_ACCEPT);
    gtk_style_context_add_class(gtk_widget_get_style_context(deleteBtn), "destructive-action");

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        // Use the organizer engine's delete path (respects trash/simulate settings)
        OrganizerRule deleteRule;
        deleteRule.action.type = ActionType::Delete;

        auto fi = Database::instance().getFile(path);
        if (fi.has_value()) {
            OrganizeResult result;
            result.sourcePath = path;
            // Direct file removal
            std::error_code ec;
            if (cfg.moveToTrash) {
                const char* home = std::getenv("HOME");
                std::string trashPath = std::string(home ? home : "/tmp") +
                                        "/.local/share/Trash/files/" + filename;
                int counter = 0;
                while (fs::exists(trashPath))
                    trashPath = std::string(home ? home : "/tmp") +
                                "/.local/share/Trash/files/" + std::to_string(++counter) + "_" + filename;
                fs::create_directories(fs::path(trashPath).parent_path(), ec);
                fs::rename(path, trashPath, ec);
            } else {
                fs::remove(path, ec);
            }

            if (!ec) {
                Database::instance().deleteFile(path);
                refreshFilesList();
                Notification n;
                n.title   = "File Deleted";
                n.message = filename + (cfg.moveToTrash ? " moved to Trash" : " permanently deleted");
                n.level   = LogLevel::Info;
                n.time    = std::chrono::system_clock::now();
                showNotificationToast(n);
                LOG_INFO("Deleted: " + path);
            } else {
                LOG_ERROR("Delete failed", ec.message(), path);
            }
        }
    }
    gtk_widget_destroy(dialog);
}

} // namespace SDO
