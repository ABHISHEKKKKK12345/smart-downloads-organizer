# Smart Downloads Organizer — User Guide

**Version 2.0.0 · Linux · GTK3**

This guide covers everything you need to use Smart Downloads Organizer (SDO)
day-to-day: installation, first launch, understanding the interface, configuring
rules, managing duplicates, cleaning up, and running SDO in the background.

---

## Table of Contents

1. [Installation](#1-installation)
2. [First Launch](#2-first-launch)
3. [The Interface](#3-the-interface)
4. [Dashboard](#4-dashboard)
5. [Files View](#5-files-view)
6. [Duplicates](#6-duplicates)
7. [Cleanup Suggestions](#7-cleanup-suggestions)
8. [Rules](#8-rules)
9. [Activity Log](#9-activity-log)
10. [Settings](#10-settings)
11. [Headless / Background Mode](#11-headless--background-mode)
12. [Undo](#12-undo)
13. [Keyboard & Tips](#13-keyboard--tips)
14. [Troubleshooting](#14-troubleshooting)

---

## 1. Installation

### Automatic (recommended)

```bash
tar -xzf smart-downloads-organizer-2.0.0.tar.gz
cd smart-downloads-organizer
sudo ./install.sh
```

`install.sh` detects your Linux distribution, installs build dependencies
automatically (apt / dnf / pacman / zypper / xbps), compiles, and installs to
`/usr/local/bin/sdo`.

### User-local install (no root required)

```bash
./install.sh --user
export PATH="$HOME/.local/bin:$PATH"
```

### Manual build

```bash
# Ubuntu / Debian
sudo apt install cmake ninja-build libgtk-3-dev libsqlite3-dev libssl-dev uuid-dev

cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
sudo cmake --install build
```

### Verify installation

```bash
sdo --version
# Smart Downloads Organizer 2.0.0
```

---

## 2. First Launch

```bash
sdo
```

On first launch SDO:

1. Creates `~/.config/smart-downloads-organizer/config.json` with default settings.
2. Creates `~/.local/share/smart-downloads-organizer/sdo.db` (SQLite database).
3. Adds four built-in rules: **Images → Pictures**, **Videos → Videos**,
   **Documents → Documents**, **Archives → Downloads/Archives**.
4. Starts watching `~/Downloads` in real time.
5. Opens the GUI on the Dashboard page.

> **Auto-organize is OFF by default.** SDO will detect and catalogue files but
> will not move anything until you either click **Organize Now** or enable
> Auto-organize in Settings. This lets you review the rules first.

---

## 3. The Interface

The window has three zones:

```
┌─────────────┬──────────────────────────────────────────┐
│             │                                          │
│  Sidebar    │         Content Area                     │
│  (nav)      │         (active page)                    │
│             │                                          │
├─────────────┴──────────────────────────────────────────┤
│  Status bar                            Queue: 0        │
└────────────────────────────────────────────────────────┘
```

**Header bar** — App title, Scan button, Organize Now button, Undo button.

**Sidebar** — Seven navigation buttons: Dashboard, Files, Duplicates, Cleanup,
Rules, Logs, Settings. The active page is highlighted in red.

**Status bar** — Current activity message on the left; processing queue size
on the right. A queue size above zero means files are being hashed and analysed
in the background.

**Toast notifications** — Short messages appear as a coloured bar below the
header when files are organized, duplicates found, or errors occur.

---

## 4. Dashboard

The Dashboard gives you a live overview of everything SDO knows about your
watched folders.

### Stat cards (top row)

| Card | What it shows |
|---|---|
| 📁 Total Files | Files currently tracked in the database |
| 💾 Total Size | Combined size of all tracked files |
| 🔄 Duplicates | Files whose SHA-256 hash matches another file |
| 🧹 Space Saveable | Total size of duplicate files (potential recovery) |
| 📦 Organized Today | Files moved/copied/renamed today |
| ⚠ Large Files | Files above the large-file threshold (default 500 MB) |
| 🕐 Old Files | Files not modified in longer than the old-file threshold (default 30 days) |
| ✅ Total Organized | All-time count of files processed by rules |

### Category breakdown

Badges showing the count of files in each of the 12 categories. Updates live as
new files are detected.

### Quick actions

| Button | What it does |
|---|---|
| 🔍 Scan Now | Re-scans all watched directories immediately |
| ▶ Organize All | Applies all enabled rules to every unprocessed file |
| 🔄 Duplicate Scan | Runs SHA-256 hashing across all files to find duplicates |
| 🧹 Show Cleanup | Jumps to the Cleanup Suggestions page |

---

## 5. Files View

A searchable, filterable table of every file SDO has detected.

### Columns

| Column | Description |
|---|---|
| Filename | File name with category emoji prefix |
| Size | Human-readable file size |
| Category | Detected category (Images, Videos, etc.) |
| Status | Normal / Duplicate / Large / Old |
| Modified | Last-modified date and time |

Status column colour coding:
- **White** — Normal file
- **Orange** — Duplicate
- **Red** — Large file
- **Grey** — Old file

### Search

Type in the search box (top-right) to filter by filename. The filter applies
instantly as you type.

### Category filter

Use the dropdown to show only one category. Combine with the search box to
narrow results further.

### Opening a file's folder

Double-click any row to open that file's containing folder in your file manager
(via `xdg-open`).

---

## 6. Duplicates

Shows every file whose SHA-256 content hash matches at least one other file,
regardless of filename or location.

### How duplicate detection works

When a file is detected, SDO computes its SHA-256 hash (a 64-character
fingerprint of the entire file content). If any existing file in the database
shares the same hash, both files are marked as duplicates. The first file seen
is the "original"; all later files with the same hash are "duplicates".

This means:
- Renaming a file does **not** hide it from duplicate detection.
- Copying a file to a different folder creates a detected duplicate.
- Two different files that happen to have the same name are **not** duplicates
  unless their content is also identical.

### Running a duplicate scan

Click **🔄 Run Duplicate Scan** to hash all files currently in the database.
This can take a few minutes for large collections. Progress is shown in the
status bar.

### Columns

| Column | Description |
|---|---|
| Filename | Name of the duplicate file |
| Size | File size (each duplicate wastes this much space) |
| Hash (SHA-256) | Abbreviated hash (first 8 + last 8 characters) |
| Full Path | Absolute path to the file |

### Acting on duplicates

Select a row and use your file manager (opened by double-click) to review and
delete duplicates manually, or create a rule on the Rules page to automatically
delete new duplicates as they appear.

---

## 7. Cleanup Suggestions

Three categories of files that are candidates for removal, surfaced on one page:

| Reason | Default threshold | Configurable in |
|---|---|---|
| **Duplicate** | Any exact-content match | Duplicate detection toggle in Settings |
| **Large file** | > 500 MB | "Large file threshold" in Settings |
| **Old file** | Not modified in > 30 days | "Old file age" in Settings |

### Columns

| Column | Description |
|---|---|
| Filename | File name |
| Size | File size |
| Reason | Why it was suggested (Duplicate / Large file / Old) |
| Modified | Last-modified date |
| Full Path | Absolute path |

Click **🔄 Refresh Suggestions** to recompute the list against the current
database and thresholds.

> SDO does **not** delete anything from this page automatically. It is a
> read-only suggestion list. To act on suggestions, either delete files
> manually or create a rule that targets large/old files.

---

## 8. Rules

Rules are the heart of SDO. Each rule watches for incoming files that match
one or more conditions and performs an action when they match.

### How rules are evaluated

1. Rules are sorted by **priority** (highest number first).
2. For each new file, SDO tests rules in priority order.
3. The **first matching rule wins** — no further rules are tested.
4. If no rule matches, the file is catalogued but left in place.

### Built-in rules

SDO ships with four built-in rules (all disabled for auto-organize by default):

| Rule | Matches | Action |
|---|---|---|
| Images → Pictures | jpg, jpeg, png, gif, bmp, tiff, webp, heic, svg, raw, cr2, nef | Move to `~/Pictures/Downloaded/YYYY/MM/` |
| Videos → Movies | mp4, mkv, avi, mov, wmv, flv, webm, m4v, ts, mpg, mpeg | Move to `~/Videos/Downloaded/YYYY/MM/` |
| Documents → Documents | pdf, doc, docx, xls, xlsx, ppt, pptx, odt, ods, txt, rtf, csv | Move to `~/Documents/Downloaded/YYYY/MM/` |
| Archives → Archives | zip, tar, gz, bz2, xz, 7z, rar, tgz, zst | Move to `~/Downloads/Archives/YYYY/MM/` |

### Adding a rule

1. Click **+ Add Rule** (top-right of the Rules page).
2. Fill in the dialog:
   - **Rule Name** — a short descriptive name.
   - **Description** — optional notes.
   - **Condition** — field, operator, value (see the Rule Reference document for full details).
   - **Action** — what to do when the rule matches.
   - **Destination** — target directory (for Move/Copy actions).
   - **Rename Pattern** — optional filename pattern (for Rename/Move actions).
   - **Priority** — higher numbers run first (0–100).
   - **Enabled** — toggle the rule on or off without deleting it.
3. Click **Add Rule**.

### Editing a rule

Double-click a rule row to open the Edit Rule dialog. All fields are the same
as the Add Rule dialog. Changes take effect on the next file that arrives.

### Enabling / disabling rules

Toggle the checkbox in the **On** column. Disabled rules are shown greyed out
and are completely skipped during evaluation.

### Rule priority

If two rules could both match the same file, the one with the higher priority
number wins. Give more specific rules a higher priority than broad ones.

**Example:** A rule matching `extension = pdf AND name contains invoice` with
priority 50 should have a higher priority than a general `extension = pdf`
rule with priority 10.

---

## 9. Activity Log

A real-time scrolling log of everything SDO has done: files detected, rules
applied, errors, warnings, and system events.

### Log levels

| Colour | Level | Meaning |
|---|---|---|
| Green | INFO | Normal events (file detected, organized, etc.) |
| Orange | WARNING | Non-fatal issues (watch path missing, hash failed) |
| Red | ERROR | Failures (move failed, database error) |
| Grey | DEBUG | Verbose events (shown in headless mode only) |

The log is stored on disk at `~/.local/share/smart-downloads-organizer/sdo.log`
and persists across sessions. The in-app log shows the most recent entries.

---

## 10. Settings

All settings are saved immediately to
`~/.config/smart-downloads-organizer/config.json` when you click **💾 Save Settings**.

### Watched Paths

The folders SDO monitors. `~/Downloads` is added by default. Click
**+ Add Watch Path** to add more folders (any folder on any locally mounted
filesystem). Removing a path stops watching it but does not delete existing
database records for files in that path.

### Behavior toggles

| Setting | Default | Description |
|---|---|---|
| Auto-organize files | OFF | Automatically apply rules when a file is detected |
| Watch subfolders recursively | OFF | Also watch all subdirectories of watched paths |
| Detect duplicate files | ON | Compute SHA-256 hash for every file |
| Desktop notifications | ON | Show system tray notifications for key events |
| Move to Trash instead of deleting | ON | Deleted files go to `~/.local/share/Trash` |
| Simulation mode (dry run) | OFF | Log what would happen but don't touch any files |
| Start minimized | OFF | Launch without showing the window |
| Suggest cleanup | ON | Show suggestions on the Cleanup page |

### Thresholds

| Setting | Default | Description |
|---|---|---|
| Large file threshold | 500 MB | Files larger than this are flagged as "Large" |
| Old file age | 30 days | Files not modified for longer than this are flagged as "Old" |

---

## 11. Headless / Background Mode

Run SDO without any GUI — just file watching and organizing in the background.

```bash
sdo --headless
```

In headless mode:
- All rules still apply (if Auto-organize is ON in config).
- Duplicate detection still runs.
- Everything is logged to `~/.local/share/smart-downloads-organizer/sdo.log`.
- The process runs until you press Ctrl+C or send SIGTERM.

### Run at login with systemd

```bash
mkdir -p ~/.config/systemd/user

cat > ~/.config/systemd/user/sdo.service << 'EOF'
[Unit]
Description=Smart Downloads Organizer
After=graphical-session.target

[Service]
ExecStart=/usr/local/bin/sdo --headless
Restart=on-failure
RestartSec=5s

[Install]
WantedBy=default.target
EOF

# Enable and start
systemctl --user enable --now sdo

# Check status
systemctl --user status sdo

# View logs
journalctl --user -u sdo -f
```

To stop: `systemctl --user stop sdo`
To disable autostart: `systemctl --user disable sdo`

---

## 12. Undo

Every Move, Copy, and Rename performed by SDO is recorded in the undo log
(stored in the SQLite database). Click **↩ Undo** in the header bar to reverse
the most recent file operation.

- Undo restores the file to its original location.
- Delete operations **cannot** be undone from SDO (though files moved to Trash
  can be recovered from the Trash manually).
- The undo history is kept for 90 days, after which old entries are pruned
  automatically at shutdown.

---

## 13. Keyboard & Tips

| Action | How |
|---|---|
| Search files | Click the search box on the Files page and type |
| Open file's folder | Double-click any row in the Files view |
| Undo last action | Click ↩ Undo in the header bar |
| Force rescan | Click 🔍 Scan in the header bar |
| Organize all now | Click ▶ Organize Now in the header bar |

**Tip — Test before enabling auto-organize:**
Enable Simulation mode in Settings first. SDO will log every action it would
take without moving anything. Review the Activity Log, then turn off simulation
mode when you are satisfied with the rules.

**Tip — Large collections:**
For folders with tens of thousands of files, the initial SHA-256 scan can take
several minutes. The status bar shows "Queue: N" while files are being
processed. You can use the app normally during this time.

**Tip — Multiple watch paths:**
You can watch any number of folders. Add your Desktop, a secondary Downloads
location, or a mounted drive in Settings → Watched Paths.

---

## 14. Troubleshooting

**SDO opens but shows no files**
The initial scan runs in the background. Wait a few seconds and check the
status bar queue counter. If it stays at zero, click 🔍 Scan Now.

**Files are not being moved**
Check that Auto-organize is ON in Settings. Also verify your rules are enabled
(the On column in the Rules page) and that the destination directory exists or
that "Create subfolders" is enabled in the rule.

**"Watch path does not exist" in the log**
A path in your watched paths list no longer exists (e.g. a USB drive was
unplugged). Remove the path in Settings or reconnect the drive and restart SDO.

**Duplicate scan is slow**
SHA-256 hashing reads every byte of every file. For large video files this
takes time. The scan runs on a background thread and does not block the UI.

**Files moved to wrong location**
Check rule priorities. A lower-priority rule may be matching before a more
specific one. Increase the priority of the specific rule.

**Config file location**
`~/.config/smart-downloads-organizer/config.json` — edit with any text editor
if needed, or delete it to reset to defaults.

**Log file location**
`~/.local/share/smart-downloads-organizer/sdo.log` — open with any text editor
or `tail -f ~/.local/share/smart-downloads-organizer/sdo.log` to watch live.
