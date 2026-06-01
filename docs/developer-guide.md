# Smart Downloads Organizer — Developer Guide

**Version 2.0.0 · C++17 · GTK3 · Linux**

This guide covers the internal architecture, every module's responsibilities,
how to build and test from source, coding conventions, and how to extend or
contribute to the project.

---

## Table of Contents

1. [Repository Layout](#1-repository-layout)
2. [Architecture Overview](#2-architecture-overview)
3. [Module Reference](#3-module-reference)
4. [Thread Model](#4-thread-model)
5. [Data Flow](#5-data-flow)
6. [Build System](#6-build-system)
7. [Test Suite](#7-test-suite)
8. [Adding a New Feature](#8-adding-a-new-feature)
9. [Coding Conventions](#9-coding-conventions)
10. [Debugging](#10-debugging)
11. [Dependency Versions](#11-dependency-versions)

---

## 1. Repository Layout

```
smart-downloads-organizer/
│
├── include/                  # All public headers — one per module
│   ├── types.hpp             # Shared data structures and enums (no .cpp)
│   ├── logger.hpp
│   ├── config_manager.hpp
│   ├── file_analyzer.hpp
│   ├── file_watcher.hpp
│   ├── database.hpp
│   ├── organizer_engine.hpp
│   └── app_window.hpp
│
├── src/                      # Implementations — one per header
│   ├── main.cpp              # Entry point only; no logic
│   ├── logger.cpp
│   ├── config_manager.cpp
│   ├── file_analyzer.cpp
│   ├── file_watcher.cpp
│   ├── database.cpp
│   ├── organizer_engine.cpp
│   └── app_window.cpp
│
├── tests/                    # One test file per module (no framework dependency)
│   ├── test_file_analyzer.cpp
│   ├── test_database.cpp
│   ├── test_organizer_engine.cpp
│   ├── test_config_manager.cpp
│   └── test_file_watcher.cpp
│
├── docs/                     # Documentation
│   ├── user-guide.md
│   ├── rule-reference.md
│   └── developer-guide.md   ← this file
│
├── resources/                # App assets
│   ├── icons/sdo.svg         # Source vector icon
│   ├── icons/sdo_48.png
│   ├── icons/sdo_128.png
│   ├── icons/sdo_256.png
│   ├── com.sdo.organizer.desktop
│   └── com.sdo.organizer.appdata.xml
│
├── cmake/
│   └── FindSDODeps.cmake     # Dependency detection with version checks
│
├── packaging/
│   ├── debian/               # dpkg-buildpackage support
│   │   ├── control
│   │   ├── rules
│   │   ├── changelog
│   │   ├── compat
│   │   └── copyright
│   ├── rpm/
│   │   └── sdo.spec          # rpmbuild support
│   ├── sdo.spec              # CPack RPM spec
│   └── PKGBUILD              # Arch Linux AUR
│
├── .github/workflows/
│   ├── build.yml             # CI: Ubuntu + Fedora + ASAN + clang-tidy
│   └── release.yml           # Automated release on git tag push
│
├── CMakeLists.txt            # Primary build definition
├── install.sh                # Multi-distro installer
├── .clang-format             # Code style
├── .clang-tidy               # Static analysis
├── .gitignore
├── README.md
├── CHANGELOG.md
├── CONTRIBUTING.md
├── SECURITY.md
├── CODE_OF_CONDUCT.md
└── LICENSE                   # MIT
```

---

## 2. Architecture Overview

SDO is a **multi-threaded, event-driven** application. The core design
principle is that **all UI mutations happen on the GTK main thread**, while
all blocking work (file hashing, database writes, rule application) happens on
background threads. The bridge between the two is `g_idle_add()`.

```
┌─────────────────────────────────────────────────────────────────┐
│  GTK Main Thread                                                │
│                                                                 │
│  AppWindow ──── g_idle_add callbacks ──── UIUpdate queue        │
│       ↑                                        ↑               │
│  gtk_main()                            postUIUpdate()           │
└─────────────────────────────────────────────────────────────────┘
         ↑                                        ↑
         │                                        │
┌────────┴──────────┐                  ┌──────────┴──────────────┐
│  inotify Thread   │                  │  Worker Thread           │
│                   │                  │                          │
│  FileWatcher      │                  │  OrganizerEngine         │
│  (select loop)    │──enqueueFile()──▶│  processLoop()           │
│                   │                  │  - FileAnalyzer::analyze │
└───────────────────┘                  │  - Database::upsertFile  │
                                       │  - applyRules()          │
                                       └──────────────────────────┘
```

**Singleton pattern** — ConfigManager, Database, Logger, FileAnalyzer, and
OrganizerEngine are all singletons accessed via `::instance()`. This avoids
passing references through every call stack and is appropriate because each
has exactly one instance per process.

**No global mutable state outside singletons** — every singleton is protected
by its own `std::mutex`.

---

## 3. Module Reference

### `types.hpp` — Shared data structures

No implementation file. Contains:

- **Enums:** `FileCategory` (12 values), `FileStatus` (6), `ActionType` (6), `LogLevel` (5)
- **Structs:** `FileInfo`, `RuleAction`, `RuleCondition`, `OrganizerRule`, `AppConfig`, `Statistics`, `LogEntry`, `UndoEntry`, `Notification`, `CategoryMeta`
- **Type aliases:** `FileDetectedCb`, `FileChangedCb`, `StatsUpdatedCb`, `LogCb`, `NotificationCb` (all `std::function`)
- **Constants:** `APP_NAME`, `APP_VERSION`, `APP_ID`

`FileInfo` has three methods implemented in `file_analyzer.cpp`:
`sizeHuman()`, `categoryName()`, `statusName()`, `ageInDays()`.

---

### `Logger` — Thread-safe structured logger

**Singleton.** Call `Logger::instance().init(path, minLevel)` once at startup.

Internals:
- `std::ofstream` for persistent file logging.
- `std::deque<LogEntry>` ring buffer (max 5000 entries) for the UI log page.
- `std::mutex` protecting all state.
- UI callback (`LogCb`) fired on every log entry after the mutex is released
  to prevent deadlock.

Convenience macros: `LOG_DEBUG`, `LOG_INFO`, `LOG_WARN`, `LOG_ERROR`,
`LOG_CRITICAL` — all expand to `Logger::instance().level(msg, detail, file)`.

---

### `ConfigManager` — JSON configuration persistence

**Singleton.** Owns the `AppConfig` struct which holds all user preferences,
watch paths, and the full rules list.

Key design decisions:
- **Hand-rolled JSON serializer/parser** — avoids any external JSON library
  dependency. The parser uses simple string search; it handles the config
  schema exactly and nothing more.
- `save()` writes atomically to the config path (creates parent directories
  automatically).
- `load()` on a missing file returns `true` and uses built-in defaults.
- `defaultConfig()` and `defaultCategoryMeta()` are static and safe to call
  before `load()`.

Rule management methods (`addRule`, `updateRule`, `deleteRule`, `reorderRules`)
all acquire the mutex and modify `m_config.rules` in-place.

---

### `FileAnalyzer` — File analysis and hashing

**Singleton.** Stateless after construction (the extension map is built once
in the constructor and never modified).

Key functions:
- `analyzeQuick(path)` — `stat()` only; no hash; fast for initial scans.
- `analyze(path)` — full analysis including SHA-256 hash via OpenSSL EVP API.
- `categorize(extension)` — O(1) lookup in `std::map<string, FileCategory>`.
- `detectMimeType(path)` — reads first 16 bytes; matches against 16 magic
  byte patterns.
- `computeSHA256(path)` — uses `EVP_DigestInit_ex` / `EVP_DigestUpdate` /
  `EVP_DigestFinal_ex`; reads in 64 KB chunks; compatible with OpenSSL 3.x.

**OpenSSL note:** The low-level `SHA256_Init` / `SHA256_Final` API is
deprecated in OpenSSL 3.0. This project uses the EVP API exclusively.

---

### `FileWatcher` — inotify filesystem event monitor

**Not a singleton** — `OrganizerEngine` owns one instance.

inotify watch flags: `IN_CREATE | IN_CLOSE_WRITE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO | IN_MODIFY`

The watch loop runs on a dedicated thread (`m_thread`). It calls `select()`
with a 200 ms timeout so `m_running.load()` is checked regularly for clean
shutdown.

Event buffer is sized `256 * (sizeof(inotify_event) + NAME_MAX + 1)` bytes.

**Rename tracking:** inotify pairs `IN_MOVED_FROM` and `IN_MOVED_TO` events
using a cookie. The watcher stores `IN_MOVED_FROM` events in `m_renames`
(keyed by cookie) and matches them when `IN_MOVED_TO` arrives. Unmatched
`IN_MOVED_FROM` events (file moved out of watched tree) are discarded.

**Recursive watching:** When `addWatch(path, recursive=true)` is called, the
watcher adds inotify watches for all existing subdirectories. When a new
directory is created inside a recursive root, the `IN_CREATE | IN_ISDIR`
event triggers `addInotifyWatch()` for the new directory automatically.

---

### `Database` — SQLite3 persistence layer

**Singleton.** Opens in WAL mode with `synchronous=NORMAL` for a good balance
of durability and performance.

Schema:
```sql
files (
    path TEXT PRIMARY KEY,
    filename TEXT, extension TEXT, mime_type TEXT,
    size_bytes INTEGER, sha256_hash TEXT, md5_hash TEXT,
    category INTEGER, status INTEGER,
    is_processed INTEGER, is_duplicate INTEGER, duplicate_of TEXT,
    created_at INTEGER, modified_at INTEGER, accessed_at INTEGER,
    detected_at INTEGER
)

undo_log (
    id TEXT PRIMARY KEY,
    action INTEGER, source_path TEXT, dest_path TEXT,
    description TEXT, performed_at INTEGER, can_undo INTEGER
)

statistics (key TEXT PRIMARY KEY, value TEXT)
```

All queries use **prepared statements** (`sqlite3_prepare_v2` /
`sqlite3_bind_*` / `sqlite3_step`). No string interpolation is used for
user-supplied values.

Timestamps are stored as `INTEGER` milliseconds since Unix epoch and
round-tripped via `toUnixMs()` / `fromUnixMs()`.

**`getDuplicateGroups()`** runs two queries: first fetches all SHA-256 hashes
that appear more than once, then fetches all files for each such hash.

---

### `OrganizerEngine` — Rule evaluation and file queue

**Singleton.** Owns the `FileWatcher` instance and the background worker
thread.

**File queue:** `std::queue<std::string>` protected by `std::mutex` +
`std::condition_variable`. Files are enqueued from the inotify thread or
from `scanDirectory()`. A `std::set<std::string>` (`m_processingSet`)
prevents the same path from being enqueued twice concurrently.

**Rule matching (`matchesCondition`):** Dispatches on `cond.field` to the
appropriate comparison. Numeric fields (`size`, `age`) bypass string operators.
The `regex` operator uses `std::regex` with `std::regex::icase`. Invalid regex
patterns evaluate to `false` without crashing.

**Action execution (`executeAction`):** All filesystem operations go through
`moveFile()`, `copyFile()`, or `deleteFile()`. Each checks
`ConfigManager::instance().simulateMode()` and logs instead of acting when
enabled. Cross-device moves fall back to copy-then-delete.

**Undo:** Every successful non-Skip action calls `saveUndoEntry()` which
writes to the `undo_log` table. `undoAction()` reverses Move/Rename by calling
`moveFile(dest, source)` and Copy by calling `deleteFile(dest)`.

---

### `AppWindow` — GTK3 GUI

**Not a singleton** — created and owned by `main()`.

**Page system:** Uses `GtkStack` with seven named children. `showPage(name)`
calls `gtk_stack_set_visible_child_name()` and refreshes the target page's
data from the database.

**Thread-safe updates:** Background threads call `postUIUpdate(UIUpdate)`,
which pushes to `m_uiQueue` (protected by `m_uiMutex`) and schedules
`g_idle_add(onUIUpdateIdle, this)`. The idle callback runs on the GTK main
thread, dequeues one update, and calls the appropriate refresh method.

**List stores:** Each page uses a `GtkListStore` backed by a
`GtkTreeViewColumn` set. All list stores are cleared and repopulated on each
`refresh*()` call (no incremental updates).

**CSS theming:** A single `GtkCssProvider` loaded with the dark theme CSS
string is added to the default screen at priority
`GTK_STYLE_PROVIDER_PRIORITY_APPLICATION`.

---

## 4. Thread Model

| Thread | Owner | Work done | Communicates via |
|---|---|---|---|
| GTK main thread | `gtk_main()` | All UI rendering and event handling | Owns all GTK objects |
| inotify thread | `FileWatcher::m_thread` | `select()` poll, reads inotify events | Calls `OrganizerEngine::enqueueFile()` |
| Worker thread | `OrganizerEngine::m_workerThread` | File analysis, hashing, DB writes, rule application | `g_idle_add()` → UI callbacks |

**Locking discipline:**
- `Logger::m_mutex` — protects log stream and entry deque
- `ConfigManager::m_mutex` — protects `AppConfig` struct
- `Database::m_mutex` — protects SQLite handle (SQLite opened in `FULLMUTEX` mode as an additional safety layer)
- `FileWatcher::m_mutex` — protects inotify watch maps and rename cookie map
- `OrganizerEngine::m_mutex` — protects file queue and processing set
- `AppWindow::m_uiMutex` — protects `m_uiQueue`

**No lock is held while calling across module boundaries.** This prevents
deadlock. For example, `Logger::log()` unlocks `m_mutex` before calling
`m_callback`.

---

## 5. Data Flow

### New file detected (auto-organize ON)

```
Filesystem event
      │
      ▼
FileWatcher::processEvent()           [inotify thread]
      │ IN_CLOSE_WRITE or IN_CREATE
      ▼
OrganizerEngine::onFileEvent()        [inotify thread]
      │ enqueueFile(path)
      ▼
OrganizerEngine::processLoop()        [worker thread]
      │ dequeue path
      ▼
FileAnalyzer::analyze(path)           [worker thread]
      │ stat + SHA-256 hash
      ▼
Duplicate check vs Database           [worker thread]
      │
      ▼
Database::upsertFile(fi)              [worker thread]
      │
      ▼
OrganizerEngine::applyRules(fi)       [worker thread]
      │ match conditions → pick action
      ▼
executeAction() → moveFile()          [worker thread]
      │
      ▼
Database::markProcessed(path)         [worker thread]
      │
      ▼
saveUndoEntry()                       [worker thread]
      │
      ▼
postUIUpdate(FileAdded / Notification) [worker thread]
      │ g_idle_add
      ▼
AppWindow::onUIUpdateIdle()           [GTK main thread]
      │
      ▼
refreshDashboard() / addFileToList()  [GTK main thread]
```

---

## 6. Build System

### CMake targets

| Target | Type | Description |
|---|---|---|
| `sdo` | Executable | Main binary |
| `sdo_lib` | Static library | All sources except `main.cpp`; linked by test targets |
| `test_file_analyzer` | Executable | FileAnalyzer test suite |
| `test_database` | Executable | Database test suite |
| `test_organizer_engine` | Executable | OrganizerEngine test suite |
| `test_config_manager` | Executable | ConfigManager test suite |
| `test_file_watcher` | Executable | FileWatcher integration tests |

### Build configurations

```bash
# Release (optimised, no debug info)
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel

# Debug (no optimisation, address sanitizer + UBSan)
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel

# RelWithDebInfo (optimised with debug symbols)
cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo

# With tests
cmake -B build -DSDO_BUILD_TESTS=ON

# Without tests (faster packaging builds)
cmake -B build -DSDO_BUILD_TESTS=OFF
```

### Direct g++ build (no CMake)

```bash
CFLAGS="-std=c++17 -Iinclude -Wall -Wextra $(pkg-config --cflags gtk+-3.0 sqlite3 openssl)"
LIBS="$(pkg-config --libs gtk+-3.0 sqlite3 openssl) -luuid -lpthread -lstdc++fs"

g++ $CFLAGS \
    src/logger.cpp src/config_manager.cpp src/file_analyzer.cpp \
    src/file_watcher.cpp src/database.cpp src/organizer_engine.cpp \
    src/app_window.cpp src/main.cpp \
    $LIBS -o sdo
```

### Install

```bash
sudo cmake --install build --prefix /usr
```

Installs: binary to `/usr/bin/sdo`, `.desktop` to
`/usr/share/applications/`, icons to `/usr/share/icons/hicolor/*/apps/`,
AppStream XML to `/usr/share/metainfo/`.

### Packaging

```bash
cd build
cpack -G DEB    # → smart-downloads-organizer-2.0.0-Linux.deb
cpack -G TGZ    # → smart-downloads-organizer-2.0.0-Linux.tar.gz
cpack -G RPM    # → smart-downloads-organizer-2.0.0-Linux.rpm  (requires rpm-build)
```

---

## 7. Test Suite

### Test framework

Tests use a minimal self-contained framework defined in each test file.
No external test library (Google Test, Catch2, etc.) is required. The
framework provides:

- `EXPECT_EQ(a, b)` — type-safe equality via `printVal<T>()` template
- `EXPECT_TRUE(x)`, `EXPECT_FALSE(x)`, `EXPECT_NE(a, b)`, `EXPECT_GE(a, b)`
- `TEST(name)` — defines a `static void name()` function
- `RUN(name)` — calls `name()` and prints pass/fail

### Running tests

```bash
# Via CTest
cmake -B build -DSDO_BUILD_TESTS=ON
cmake --build build --parallel
cd build && ctest --output-on-failure

# Directly
./build/bin/test_file_analyzer
./build/bin/test_database
./build/bin/test_organizer_engine
./build/bin/test_config_manager
./build/bin/test_file_watcher
```

### Test coverage summary

| Suite | Tests | What is covered |
|---|---|---|
| `test_file_analyzer` | 18 | Extension mapping, MIME detection, SHA-256, MD5, size formatting, FileInfo methods |
| `test_database` | 19 | Upsert, get, delete, mark-processed, category query, duplicate groups, undo log, statistics, vacuum, prune |
| `test_organizer_engine` | 13 | Rule matching (OR/AND), priority order, disabled rules, regex, age/size conditions, simulate mode, cleanup suggestions |
| `test_config_manager` | 19 | Load/save, defaults, round-trip, add/update/delete/reorder rules, watch paths, callback, JSON validity |
| `test_file_watcher` | 15 | inotify create/modify/delete/rename events, recursive subdirectory detection, add/remove watches, post-stop silence |

### Adding tests

Create a new test file in `tests/` following the existing pattern.
Add it to `CMakeLists.txt` in the `foreach(TEST_NAME ...)` block.

---

## 8. Adding a New Feature

### Adding a new file category

1. Add a value to `FileCategory` enum in `include/types.hpp`.
   Update `COUNT` to be the last enumerator.
2. Add extensions to the `add(FileCategory::YourCategory, {...})` block
   in `FileAnalyzer::buildExtensionMap()` in `src/file_analyzer.cpp`.
3. Add the category name string to `FileInfo::categoryName()` in
   `src/file_analyzer.cpp`.
4. Add a `CategoryMeta` entry in
   `ConfigManager::defaultCategoryMeta()` in `src/config_manager.cpp`.
5. Update the Dashboard category badges in `AppWindow::refreshDashboard()`
   in `src/app_window.cpp`.
6. Add test cases to `tests/test_file_analyzer.cpp`.

### Adding a new condition field

1. Add a branch in `OrganizerEngine::matchesCondition()` in
   `src/organizer_engine.cpp`.
2. Update the condition field dropdown in `AppWindow::showAddRuleDialog()`
   and `AppWindow::showEditRuleDialog()`.
3. Document the new field in `docs/rule-reference.md`.
4. Add test cases to `tests/test_organizer_engine.cpp`.

### Adding a new action type

1. Add a value to `ActionType` enum in `include/types.hpp`.
2. Add a branch in `OrganizerEngine::executeAction()`.
3. Handle undo in `OrganizerEngine::undoAction()`.
4. Add the action name to the action dropdown in both rule dialogs.
5. Document in `docs/rule-reference.md`.
6. Add tests.

### Adding a new settings field

1. Add the field to `AppConfig` struct in `include/types.hpp`.
2. Set the default value in `ConfigManager::defaultConfig()`.
3. Add serialization in `ConfigManager::serializeConfig()`.
4. Add deserialization in `ConfigManager::parseConfig()`.
5. Add the UI control in `AppWindow::buildSettingsPage()`.
6. Add a test in `tests/test_config_manager.cpp`.

---

## 9. Coding Conventions

### Formatting

Run `clang-format -i src/*.cpp include/*.hpp` before committing.
The `.clang-format` file uses LLVM base style with 4-space indentation
and 100-column limit.

### Naming

| Item | Convention | Example |
|---|---|---|
| Classes / structs | `CamelCase` | `FileAnalyzer`, `OrganizerRule` |
| Methods / functions | `camelBack` | `analyzeQuick()`, `buildExtensionMap()` |
| Member variables | `m_` prefix + `camelBack` | `m_inotifyFd`, `m_running` |
| Constants | `UPPER_CASE` | `APP_NAME`, `APP_VERSION` |
| Local variables | `camelBack` | `filePath`, `hashKey` |
| Namespaces | `CamelCase` | `SDO` |

### Error handling

- Never use exceptions for expected failure paths (file not found, hash
  failed). Return `bool` or `std::optional<T>`.
- Use exceptions only for truly unrecoverable situations.
- Always log errors with `LOG_ERROR(msg, detail, filepath)` before returning
  `false`.
- Check all system call return values (`inotify_init1`, `open`, `stat`, etc.).

### Thread safety

- Any data accessed from more than one thread must be protected by a mutex.
- Always use `std::lock_guard<std::mutex>` (not manual lock/unlock).
- Never hold a lock while calling into another module.
- All GTK API calls must happen on the GTK main thread. Use `g_idle_add()`
  to marshal calls from background threads.

### Memory

- Use RAII for all resources. No naked `new`/`delete`.
- GTK objects are owned by GTK and managed via `g_object_ref`/`g_object_unref`
  or by the widget hierarchy. Do not `delete` GTK objects.
- SQLite `sqlite3_stmt*` must always be finalized with `sqlite3_finalize()`.

---

## 10. Debugging

### Verbose logging

```bash
# Set log level to DEBUG before init in main.cpp (for development builds)
Logger::instance().init(cfg.logPath, SDO::LogLevel::Debug);
```

Or watch the log file live:
```bash
tail -f ~/.local/share/smart-downloads-organizer/sdo.log
```

### AddressSanitizer + UBSan

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ASAN_OPTIONS=halt_on_error=1:detect_leaks=1 ./build/bin/sdo --headless
```

### Valgrind

```bash
valgrind --leak-check=full --track-origins=yes \
         --suppressions=/usr/share/gtk-3.0/valgrind/gtk.suppressions \
         ./build/bin/sdo --headless
```

### GDB

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
gdb ./build/bin/sdo
(gdb) run --headless
```

### SQLite database inspection

```bash
sqlite3 ~/.local/share/smart-downloads-organizer/sdo.db
sqlite> .tables
sqlite> SELECT path, category, is_duplicate FROM files LIMIT 10;
sqlite> SELECT * FROM undo_log ORDER BY performed_at DESC LIMIT 5;
```

---

## 11. Dependency Versions

| Dependency | Minimum | Tested with | Notes |
|---|---|---|---|
| GCC | 9.0 | 13.3.0 | C++17 `std::filesystem` requires GCC ≥ 9 |
| Clang | 10.0 | — | Alternative compiler; same C++17 requirement |
| CMake | 3.16 | 3.28 | `cmake --install` requires ≥ 3.15 |
| GTK3 | 3.24 | 3.24.41 | `GtkStack` transition effects require ≥ 3.12 |
| SQLite | 3.31 | 3.45.1 | WAL mode and `INSERT OR REPLACE` require ≥ 3.24 |
| OpenSSL | 1.1 | 3.0.13 | EVP API used; low-level SHA/MD5 API avoided |
| libuuid | 2.34 | 2.39.3 | `uuid_generate_random()` |
| Linux kernel | 2.6.36 | 6.18.5 | `inotify_init1()` requires ≥ 2.6.27; `IN_CLOEXEC` requires ≥ 2.6.27 |
| Ninja | 1.10 | 1.11.1 | Optional; `make` also works |
