#pragma once

#include "types.hpp"
#include <gtk/gtk.h>
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <queue>

namespace SDO {

// ─── UI Update message (thread-safe GTK updates) ─────────────────────────────
enum class UIUpdateType {
    FileAdded,
    StatsRefresh,
    Notification,
    LogEntry,
    ScanProgress,
    QueueSizeUpdate
};

struct UIUpdate {
    UIUpdateType   type;
    FileInfo       file;
    Statistics     stats;
    Notification   notification;
    LogEntry       logEntry;
    std::string    message;
    int            progress  = 0;
    size_t         queueSize = 0;
};

// ─── Main App Window ──────────────────────────────────────────────────────────
class AppWindow {
public:
    AppWindow();
    ~AppWindow();

    bool init(int argc, char** argv);
    void run();
    void quit();

    // Thread-safe UI updates
    void postUIUpdate(UIUpdate update);

private:
    // ── Widget builders ──────────────────────────────────────────────────────
    GtkWidget* buildMainWindow();
    GtkWidget* buildHeaderBar();
    GtkWidget* buildSidebar();
    GtkWidget* buildDashboardPage();
    GtkWidget* buildFilesPage();
    GtkWidget* buildDuplicatesPage();
    GtkWidget* buildRulesPage();
    GtkWidget* buildCleanupPage();
    GtkWidget* buildLogsPage();
    GtkWidget* buildSettingsPage();
    GtkWidget* buildStatusBar();
    GtkWidget* buildStatCard(const char* icon, const char* label, const char* id);

    // ── Page switching ───────────────────────────────────────────────────────
    void showPage(const std::string& name);
    void updateSidebarActive(const std::string& page);

    // ── Data refresh ─────────────────────────────────────────────────────────
    void refreshDashboard(const Statistics& stats);
    void addFileToList(const FileInfo& fi);
    void refreshFilesList();
    void refreshDuplicatesList();
    void refreshRulesList();
    void refreshCleanupList();
    void addLogEntry(const LogEntry& entry);
    void updateStatusBar(const std::string& msg, size_t queueSize = 0);
    void showNotificationToast(const Notification& notif);
    void updateStatCard(const char* id, const char* value);

    // ── Dialogs ───────────────────────────────────────────────────────────────
    void showAddRuleDialog();
    void showEditRuleDialog(const OrganizerRule& rule);
    void showSettingsDialog();
    void showAboutDialog();
    void showFilenameDialog();
    void confirmDelete(const std::string& path);

    // ── Signal handlers (static → instance forwarding) ────────────────────────
    static void onSidebarNavClicked (GtkButton* btn, gpointer data);
    static void onOrganizeNowClicked(GtkButton* btn, gpointer data);
    static void onScanNowClicked    (GtkButton* btn, gpointer data);
    static void onUndoClicked       (GtkButton* btn, gpointer data);
    static void onAddWatchPathClicked(GtkButton* btn, gpointer data);
    static void onAddRuleClicked    (GtkButton* btn, gpointer data);
    static void onDuplicateScanClicked(GtkButton* btn, gpointer data);
    static void onCleanupClicked    (GtkButton* btn, gpointer data);
    static void onSearchChanged     (GtkSearchEntry* entry, gpointer data);
    static void onFilterCategoryChanged(GtkComboBox* combo, gpointer data);
    static void onFilesRowActivated (GtkTreeView* tv, GtkTreePath* path, GtkTreeViewColumn* col, gpointer data);
    static void onWindowDestroy     (GtkWidget* widget, gpointer data);
    static gboolean onUIUpdateIdle  (gpointer data);
    static gboolean onToastTimeout  (gpointer data);

    // ── CSS / theming ─────────────────────────────────────────────────────────
    void applyCSS();
    void loadCSSTheme(bool dark);

    // ── Widgets (owned by GTK) ────────────────────────────────────────────────
    GtkWidget*    m_window          = nullptr;
    GtkWidget*    m_stack           = nullptr;
    GtkWidget*    m_sidebar         = nullptr;
    GtkWidget*    m_statusLabel     = nullptr;
    GtkWidget*    m_queueLabel      = nullptr;
    GtkWidget*    m_toastLabel      = nullptr;
    GtkWidget*    m_toastBar        = nullptr;
    GtkWidget*    m_searchEntry     = nullptr;
    GtkWidget*    m_categoryFilter  = nullptr;
    GtkWidget*    m_progressBar     = nullptr;

    // File list
    GtkListStore* m_filesStore      = nullptr;
    GtkWidget*    m_filesTreeView   = nullptr;

    // Duplicates list
    GtkListStore* m_dupsStore       = nullptr;
    GtkWidget*    m_dupsTreeView    = nullptr;

    // Cleanup list
    GtkListStore* m_cleanupStore    = nullptr;
    GtkWidget*    m_cleanupTreeView = nullptr;

    // Rules list
    GtkListStore* m_rulesStore      = nullptr;
    GtkWidget*    m_rulesTreeView   = nullptr;

    // Log list
    GtkListStore* m_logStore        = nullptr;
    GtkWidget*    m_logTreeView     = nullptr;

    // Dashboard stat labels (keyed by id)
    std::map<std::string, GtkWidget*> m_statLabels;
    std::map<std::string, GtkWidget*> m_navButtons;

    // Thread-safe update queue
    mutable std::mutex           m_uiMutex;
    std::queue<UIUpdate>         m_uiQueue;

    std::string                  m_currentPage = "dashboard";
    GtkCssProvider*              m_cssProvider = nullptr;
    guint                        m_toastTimerId = 0;
    std::string                  m_searchQuery;
    FileCategory                 m_filterCategory = FileCategory::COUNT; // COUNT = all
};

} // namespace SDO
