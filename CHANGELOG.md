# Changelog

All notable changes to Smart Downloads Organizer are documented in this file.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [Unreleased]

### Planned
- Light theme support
- Tray icon / system tray integration  
- Network-attached storage (NFS/Samba) path support
- Rule import/export as JSON
- Scheduled cleanup (cron-like rules)
- File tagging system
- Plugin API for custom categorizers

---

## [2.0.0] — 2025-01-01

### Added
- **Full GTK3 GUI** with 7 pages: Dashboard, Files, Duplicates, Cleanup, Rules,
  Activity Log, and Settings
- **Real-time file watching** via Linux inotify with zero polling overhead
- **Automatic categorization** of 200+ file extensions into 12 categories:
  Images, Videos, Audio, Documents, Archives, Code, Executables, Fonts,
  Data, Ebooks, Torrents, Unknown
- **SHA-256 duplicate detection** engine using OpenSSL EVP API (OpenSSL 3.x
  compatible); groups duplicates by content hash rather than filename
- **Rule engine** supporting 8 condition operators (eq, ne, contains,
  startswith, endswith, regex, gt, lt) across 6 fields (extension, name,
  size, age, category, MIME type)
- **Rule condition logic**: AND (all conditions must match) or OR (any match)
- **Rule actions**: Move, Copy, Delete, Rename with `{name}`, `{date}`,
  `{year}`, `{month}`, `{ext}`, `{category}` pattern variables
- **Full undo history** for every Move, Copy, and Rename operation stored in
  SQLite; accessible from the header bar Undo button
- **Cleanup suggestions** surfacing large files (configurable threshold),
  old files (configurable age), and duplicates on a single page
- **Simulation (dry-run) mode** to preview every rule action without
  touching the filesystem
- **Headless/daemon mode** (`--headless`) for background operation on
  servers or via systemd
- **Recursive directory watching** for nested folder structures
- **Magic-byte MIME detection** for 16 common formats independent of
  file extension
- **Persistent SQLite database** (WAL mode, full-text search, per-category
  statistics)
- **Thread-safe UI update queue** using GTK idle callbacks — all background
  work runs on worker threads, all UI mutations on the GTK main thread
- **Configurable thresholds**: large-file size, old-file age in days,
  scan interval
- **Trash integration**: deleted files go to `~/.local/share/Trash` by default
  rather than permanent deletion
- **About dialog**, **Edit Rule dialog**, **Rename File dialog**,
  **Confirm Delete dialog** — all fully implemented
- Dark-themed UI with custom CSS (red/navy color scheme)
- Auto-detect distribution and install dependencies via `install.sh`
- Debian `.deb` packaging via CPack and `packaging/debian/`
- RPM `.spec` for Fedora/RHEL/openSUSE
- Arch Linux PKGBUILD for AUR
- GitHub Actions CI: build on Ubuntu 22.04/24.04, Fedora; ASAN/UBSan build;
  clang-tidy static analysis; automated release asset creation
- 5 test suites, 175+ tests covering FileAnalyzer, Database, OrganizerEngine,
  ConfigManager, and FileWatcher
- `.desktop` file and AppStream metadata for software center integration
- SVG application icon with PNG renders at 48×128×256px

### Architecture
- `types.hpp` — all shared data structures and enums
- `Logger` — thread-safe ring-buffer logger with file output and UI callback
- `ConfigManager` — JSON config persistence with hand-rolled serializer
- `FileAnalyzer` — extension map, magic-byte MIME, OpenSSL EVP hashing
- `FileWatcher` — inotify wrapper with recursive auto-watching
- `Database` — SQLite3 layer with prepared statements throughout
- `OrganizerEngine` — rule evaluator, file queue processor, undo system
- `AppWindow` — GTK3 GUI; thread-safe via `g_idle_add` update queue

---

## [1.x] — Internal

Pre-release development versions. Not publicly available.

---

[Unreleased]: https://github.com/ABHISHEKKKKK12345/smart-downloads-organizer/compare/v2.0.0...HEAD
[2.0.0]: https://github.com/ABHISHEKKKKK12345/smart-downloads-organizer/releases/tag/v2.0.0
