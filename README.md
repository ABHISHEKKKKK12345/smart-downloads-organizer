# Smart Downloads Organizer

<p align="center">
  <img src="resources/icons/sdo.svg" width="128" height="128" alt="SDO Logo"/>
</p>

<p align="center">
  <strong>Enterprise-grade automatic downloads folder organizer for Linux</strong>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/version-2.0.0-red" alt="Version"/>
  <img src="https://img.shields.io/badge/license-MIT-blue" alt="License"/>
  <img src="https://img.shields.io/badge/platform-Linux-lightgrey" alt="Platform"/>
  <img src="https://img.shields.io/badge/C%2B%2B-17-blue" alt="C++17"/>
  <img src="https://img.shields.io/badge/GTK-3.24%2B-green" alt="GTK3"/>
  <img src="https://img.shields.io/badge/tests-210%20passing-brightgreen" alt="Tests"/>
</p>

---

Smart Downloads Organizer (SDO) watches your Downloads folder in real time and
automatically sorts, categorizes, deduplicates, and cleans up incoming files
using a fully configurable rule engine — with a polished GTK3 dark-theme interface
that most users will actually want to run.

---

## Features

| Feature | Detail |
|---|---|
| **Real-time watching** | Linux inotify — zero polling, instant response |
| **Auto-categorization** | 200+ extensions → 12 categories (Images, Videos, Audio, Documents, Archives, Code, Executables, Fonts, Data, Ebooks, Torrents, Unknown) |
| **MIME detection** | Magic-byte sniffing independent of file extension |
| **Duplicate detection** | SHA-256 content hashing; groups exact duplicates regardless of filename |
| **Rule engine** | 8 operators (eq, ne, contains, startswith, endswith, regex, gt, lt) on 6 fields (extension, name, size, age, category, MIME) |
| **Rule actions** | Move, Copy, Delete, Rename with `{name}` `{date}` `{year}` `{month}` `{ext}` `{category}` pattern variables |
| **Condition logic** | AND (all must match) or OR (any match) per rule |
| **Undo history** | Full undo for every Move/Copy/Rename stored in SQLite |
| **Cleanup suggestions** | Surface large files, old files, and duplicates on one page |
| **Simulate mode** | Dry-run: preview every action without touching the filesystem |
| **Headless mode** | `--headless` flag for daemon/server/systemd use |
| **Recursive watching** | Optional recursive subdirectory monitoring |
| **Trash integration** | Delete sends to `~/.local/share/Trash` by default |
| **Persistent storage** | SQLite WAL-mode database with full-text file search |
| **Dark theme** | GTK3 CSS dark theme, red accent, navy background |

---

## Screenshots

> **Dashboard** shows live file statistics, category breakdowns, and quick-action buttons.
> **Files** provides a searchable, filterable table of every detected file with status colour coding.
> **Duplicates** lists SHA-256 matched files grouped by content hash.
> **Cleanup** surfaces large, old, and duplicate files with their sizes and reasons.
> **Rules** displays all organiser rules with enable/disable toggles, priorities, and match counts.
> **Logs** shows a live, colour-coded activity feed of every action SDO has taken.
> **Settings** exposes all configuration: watch paths, thresholds, and behaviour toggles.

Screenshots will be available in the [project wiki](https://github.com/ABHISHEKKKKK12345/smart-downloads-organizer/wiki) once the repository is published.

---

## Requirements

| Dependency | Minimum | Purpose |
|---|---|---|
| Linux kernel | 2.6.36+ | inotify file watching |
| GCC / Clang | 9+ / 10+ | C++17 `std::filesystem` |
| CMake | 3.16+ | Build system |
| GTK3 | 3.24 | GUI |
| SQLite | 3.31 | Persistent storage |
| OpenSSL | 1.1 | SHA-256 hashing |
| libuuid | 2.34 | Unique ID generation |

---

## Quick Start

### One-command install (auto-detects distribution)

```bash
git clone https://github.com/ABHISHEKKKKK12345/smart-downloads-organizer.git
cd organizer
sudo ./install.sh
sdo
```

### User-local install (no root required)

```bash
./install.sh --user
export PATH="$HOME/.local/bin:$PATH"
sdo
```

### Manual build

```bash
# Install dependencies (Ubuntu/Debian)
sudo apt install cmake ninja-build libgtk-3-dev libsqlite3-dev libssl-dev uuid-dev

# Configure & build
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel

# Run
./build/bin/sdo

# Install system-wide
sudo cmake --install build
```

### Fedora / RHEL

```bash
sudo dnf install cmake ninja-build gtk3-devel sqlite-devel openssl-devel libuuid-devel
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
sudo cmake --install build
```

### Arch Linux (AUR)

```bash
# From source
makepkg -si

# Or via AUR helper once the package is published
# yay -S smart-downloads-organizer
```

---

## Usage

```
Usage: sdo [OPTIONS]

Options:
  -h, --help            Show this help message
  -v, --version         Show version and exit
  -H, --headless        Run without GUI (watch + organize only)
  -c, --config PATH     Use alternate config file
```

### Run as a systemd service

```ini
# ~/.config/systemd/user/sdo.service
[Unit]
Description=Smart Downloads Organizer
After=graphical-session.target

[Service]
ExecStart=/usr/bin/sdo --headless
Restart=on-failure
RestartSec=5s

[Install]
WantedBy=default.target
```

```bash
systemctl --user enable --now sdo
```

---

## Configuration

Config is stored at `~/.config/smart-downloads-organizer/config.json`.
All settings are also editable from the Settings page in the GUI.

Key settings:

| Key | Default | Description |
|---|---|---|
| `autoOrganize` | `false` | Apply rules automatically on file detection |
| `watchRecursive` | `false` | Watch subdirectories recursively |
| `enableDuplicateDetect` | `true` | SHA-256 hash every incoming file |
| `simulateMode` | `false` | Dry-run: log actions without executing |
| `moveToTrash` | `true` | Send deleted files to Trash instead of permanent delete |
| `largeSizeThreshold` | `524288000` | Large-file threshold in bytes (500 MB) |
| `oldFileAgeDays` | `30` | Age in days before a file is flagged as old |

---

## Rule Engine

Rules are evaluated in descending priority order. The first matching rule wins.

### Example rule (move all PDFs to Documents)

```json
{
  "id": "pdf-to-docs",
  "name": "PDFs → Documents",
  "enabled": true,
  "priority": 10,
  "conditionLogic": "OR",
  "conditions": [
    { "field": "extension", "op": "eq", "value": "pdf" }
  ],
  "action": {
    "type": "Move",
    "targetDirectory": "/home/user/Documents/PDFs",
    "createSubfolders": true
  }
}
```

### Rename pattern variables

| Variable | Expands to |
|---|---|
| `{name}` | Filename without extension |
| `{ext}` | File extension |
| `{date}` | `YYYY-MM-DD` of file modification date |
| `{year}` | `YYYY` |
| `{month}` | `MM` |
| `{category}` | Category name (e.g. `Documents`) |
| `{size}` | Human-readable size (e.g. `2.4 MB`) |

### Condition fields and operators

| Field | Operators |
|---|---|
| `extension` | `eq` `ne` `contains` `startswith` `endswith` `regex` |
| `name` | `eq` `ne` `contains` `startswith` `endswith` `regex` |
| `category` | `eq` `ne` |
| `mime` | `eq` `ne` `contains` `regex` |
| `size` | `gt` `lt` `eq` (bytes) |
| `age` | `gt` `lt` `eq` (days since last modified) |

---

## Documentation

| Document | Description |
|---|---|
| [User Guide](docs/user-guide.md) | Installing, launching, all 7 GUI pages, settings, headless mode, undo, troubleshooting |
| [Rule Reference](docs/rule-reference.md) | All condition fields, operators, action types, pattern variables, priority system, and built-in rules |
| [Developer Guide](docs/developer-guide.md) | Architecture, module responsibilities, thread model, data flow, build system, test suite, contributing |

---

## Architecture

```
smart-downloads-organizer/
├── include/
│   ├── types.hpp            # All shared data structures and enums
│   ├── logger.hpp           # Thread-safe ring-buffer logger
│   ├── config_manager.hpp   # JSON config persistence (singleton)
│   ├── file_analyzer.hpp    # Extension map, MIME detection, SHA-256
│   ├── file_watcher.hpp     # inotify wrapper with recursive support
│   ├── database.hpp         # SQLite3 persistence layer
│   ├── organizer_engine.hpp # Rule evaluator + file queue processor
│   └── app_window.hpp       # GTK3 main window (7 pages)
├── src/
│   ├── main.cpp             # Entry point, CLI flags, subsystem init
│   ├── logger.cpp
│   ├── config_manager.cpp
│   ├── file_analyzer.cpp
│   ├── file_watcher.cpp
│   ├── database.cpp
│   ├── organizer_engine.cpp
│   └── app_window.cpp
├── tests/
│   ├── test_file_analyzer.cpp   # 17 tests, 55 assertions
│   ├── test_database.cpp        # 20 tests, 43 assertions
│   ├── test_organizer_engine.cpp# 12 tests, 21 assertions
│   ├── test_config_manager.cpp  # 21 tests, 70 assertions
│   └── test_file_watcher.cpp    # 14 tests, 21 assertions  (210 total)
├── resources/
│   ├── icons/               # SVG + PNG at 48/128/256px
│   ├── com.sdo.organizer.desktop
│   └── com.sdo.organizer.appdata.xml
├── packaging/
│   ├── debian/              # .deb packaging
│   ├── sdo.spec             # RPM spec
│   └── PKGBUILD             # Arch AUR
├── cmake/
│   └── FindSDODeps.cmake    # Dependency finder with version checks
├── .github/workflows/
│   ├── build.yml            # CI: Ubuntu 22.04/24.04 + Fedora + ASAN
│   └── release.yml          # Release: build packages on tag push
├── CMakeLists.txt
├── install.sh               # Multi-distro installer
├── .clang-format
├── .clang-tidy
├── .gitignore
├── LICENSE                  # MIT
└── CHANGELOG.md
```

### Thread model

- **GTK main thread** — all UI mutations, event loop
- **Worker thread** — file queue processing, hashing, rule application
- **inotify thread** — filesystem event polling
- **Thread-safe bridge** — `g_idle_add()` posts `UIUpdate` structs from any thread to the GTK main thread

---

## Development

### Build with tests and ASAN

```bash
cmake -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DSDO_BUILD_TESTS=ON
cmake --build build --parallel
cd build && ctest --output-on-failure
```

### Code style

```bash
# Format all source
clang-format -i src/*.cpp include/*.hpp

# Static analysis
clang-tidy -p build src/*.cpp
```

### Running tests individually

```bash
./build/bin/test_file_analyzer
./build/bin/test_database
./build/bin/test_organizer_engine
./build/bin/test_config_manager
./build/bin/test_file_watcher
```

---

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md).  In brief:

1. Fork → feature branch → PR against `develop`
2. All PRs must pass CI (zero errors, zero warnings, all tests green)
3. Follow `.clang-format` style
4. New features need corresponding tests in `tests/`

---

## License

MIT — see [LICENSE](LICENSE).

---

## Changelog

See [CHANGELOG.md](CHANGELOG.md).
