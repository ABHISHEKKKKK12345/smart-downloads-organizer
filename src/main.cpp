/**
 * Smart Downloads Organizer — main.cpp
 *
 * Entry point: bootstraps config, database, logger, organizer engine, and
 * the GTK3 GUI. All subsystems are initialised in dependency order and torn
 * down cleanly on exit.
 */

#include "types.hpp"
#include "logger.hpp"
#include "config_manager.hpp"
#include "database.hpp"
#include "organizer_engine.hpp"
#include "app_window.hpp"

#include <iostream>
#include <filesystem>
#include <csignal>
#include <cstdlib>

namespace fs = std::filesystem;

// ─── Global shutdown flag (signal handler) ────────────────────────────────────
static volatile bool g_shutdown = false;

static void signalHandler(int sig) {
    (void)sig;
    g_shutdown = true;
}

// ─── Ensure required directories exist ───────────────────────────────────────
static void ensureDirectories(const SDO::AppConfig& cfg) {
    for (const std::string& dir : {
            fs::path(cfg.configPath).parent_path().string(),
            fs::path(cfg.databasePath).parent_path().string(),
            fs::path(cfg.logPath).parent_path().string()
        })
    {
        std::error_code ec;
        fs::create_directories(dir, ec);
        if (ec) {
            std::cerr << "[startup] Cannot create directory: " << dir
                      << " — " << ec.message() << "\n";
        }
    }
}

// ─── main ─────────────────────────────────────────────────────────────────────
int main(int argc, char* argv[]) {
    // ── Signal handling ──────────────────────────────────────────────────────
    std::signal(SIGINT,  signalHandler);
    std::signal(SIGTERM, signalHandler);

    // ── Parse simple CLI flags ───────────────────────────────────────────────
    bool headless   = false;
    bool showVersion= false;
    std::string configOverride;

    for (int i = 1; i < argc; ++i) {
        std::string arg(argv[i]);
        if (arg == "--headless" || arg == "-H") {
            headless = true;
        } else if (arg == "--version" || arg == "-v") {
            showVersion = true;
        } else if ((arg == "--config" || arg == "-c") && i + 1 < argc) {
            configOverride = argv[++i];
        } else if (arg == "--help" || arg == "-h") {
            std::cout <<
                "Usage: sdo [OPTIONS]\n"
                "\n"
                "Options:\n"
                "  -h, --help            Show this help message\n"
                "  -v, --version         Show version and exit\n"
                "  -H, --headless        Run without GUI (watch + organize only)\n"
                "  -c, --config PATH     Use alternate config file\n"
                "\n"
                "Smart Downloads Organizer " << SDO::APP_VERSION << "\n";
            return 0;
        }
    }

    if (showVersion) {
        std::cout << SDO::APP_NAME << " " << SDO::APP_VERSION << "\n";
        return 0;
    }

    // ── Config ───────────────────────────────────────────────────────────────
    auto& cfgMgr = SDO::ConfigManager::instance();
    std::string cfgPath = configOverride.empty()
                        ? SDO::ConfigManager::defaultConfigPath()
                        : configOverride;

    if (!cfgMgr.load(cfgPath)) {
        std::cerr << "[startup] Config load failed; using defaults\n";
    }

    const SDO::AppConfig& cfg = cfgMgr.config();
    ensureDirectories(cfg);

    // ── Logger ───────────────────────────────────────────────────────────────
    SDO::Logger::instance().init(cfg.logPath,
        headless ? SDO::LogLevel::Debug : SDO::LogLevel::Info);
    LOG_INFO("=== Smart Downloads Organizer " + std::string(SDO::APP_VERSION) + " starting ===");
    LOG_INFO("Config", cfg.configPath);
    LOG_INFO("Database", cfg.databasePath);
    LOG_INFO("Log", cfg.logPath);

    // ── Database ─────────────────────────────────────────────────────────────
    auto& db = SDO::Database::instance();
    if (!db.open(cfg.databasePath)) {
        std::cerr << "[startup] FATAL: Cannot open database: " << cfg.databasePath << "\n";
        LOG_CRITICAL("Database open failed", cfg.databasePath);
        return 1;
    }
    LOG_INFO("Database opened successfully");

    // ── Organizer Engine ─────────────────────────────────────────────────────
    auto& engine = SDO::OrganizerEngine::instance();

    if (!engine.startWatching()) {
        LOG_ERROR("Failed to start organizer engine — watch paths may be missing");
        // Non-fatal: continue without watching
    }

    // ── Headless mode ────────────────────────────────────────────────────────
    if (headless) {
        LOG_INFO("Running in headless mode — press Ctrl+C to stop");
        std::cout << "Smart Downloads Organizer running headless. Ctrl+C to stop.\n";

        while (!g_shutdown) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        LOG_INFO("Shutdown signal received");
        engine.stopWatching();
        db.close();
        cfgMgr.save();
        LOG_INFO("Shutdown complete");
        return 0;
    }

    // ── GUI mode ─────────────────────────────────────────────────────────────
    SDO::AppWindow window;
    if (!window.init(argc, argv)) {
        LOG_CRITICAL("GUI initialisation failed");
        engine.stopWatching();
        db.close();
        return 1;
    }

    LOG_INFO("GUI ready");
    window.run(); // blocks until window is closed

    // ── Teardown ─────────────────────────────────────────────────────────────
    LOG_INFO("Shutting down...");
    engine.stopWatching();

    // Persist config (window size etc. may have changed)
    cfgMgr.save();

    // Maintenance: prune stale undo entries older than 90 days
    db.pruneOldRecords(90);
    db.close();

    LOG_INFO("Shutdown complete");
    return 0;
}
