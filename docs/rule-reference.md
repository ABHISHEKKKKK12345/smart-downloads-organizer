# Smart Downloads Organizer — Rule Reference

**Version 2.0.0**

This document is the complete technical reference for the SDO rule engine:
every condition field, every operator, every action type, every pattern
variable, and how evaluation works end-to-end.

---

## Table of Contents

1. [Rule Structure](#1-rule-structure)
2. [Condition Fields](#2-condition-fields)
3. [Condition Operators](#3-condition-operators)
4. [Condition Logic (AND / OR)](#4-condition-logic-and--or)
5. [Action Types](#5-action-types)
6. [Rename Pattern Variables](#6-rename-pattern-variables)
7. [Rule Priority and Evaluation Order](#7-rule-priority-and-evaluation-order)
8. [File Categories](#8-file-categories)
9. [File Status Values](#9-file-status-values)
10. [Complete Examples](#10-complete-examples)
11. [Built-in Rules Reference](#11-built-in-rules-reference)

---

## 1. Rule Structure

Every rule has the following fields:

| Field | Type | Required | Description |
|---|---|---|---|
| `id` | string | auto-generated | Unique identifier — set automatically on creation |
| `name` | string | yes | Human-readable name shown in the Rules page |
| `description` | string | no | Optional notes about what the rule does |
| `enabled` | bool | yes | Whether the rule participates in evaluation |
| `priority` | int (0–100) | yes | Higher number = evaluated first |
| `conditionLogic` | `"AND"` or `"OR"` | yes | How multiple conditions are combined |
| `conditions` | list | yes | One or more condition objects |
| `action` | object | yes | What to do when the rule matches |

A rule with **zero conditions** never matches anything and is silently skipped.

---

## 2. Condition Fields

Each condition tests one attribute of the incoming file.

### `extension`

The file extension in lowercase, without the leading dot.

```
field: "extension"
value: "pdf"          # matches file.pdf, Report.PDF, DOCUMENT.pdf
value: "jpg"          # matches photo.jpg, IMG.JPG
```

Extensions are always compared case-insensitively, regardless of the
`caseSensitive` flag.

---

### `name`

The full filename including the extension. Compared as a string.

```
field: "name"
value: "invoice"      # with op=contains: matches invoice_2024.pdf, my_invoice.xlsx
value: "^report"      # with op=regex:    matches report_Q1.pdf but not Q1_report.pdf
```

---

### `size`

The file size in **bytes**. Used with numeric operators (`gt`, `lt`, `eq`).

```
field: "size"
op:    "gt"
value: "524288000"    # files larger than 500 MB (500 * 1024 * 1024)
```

Common size values:

| Human size | Bytes |
|---|---|
| 1 KB | 1024 |
| 1 MB | 1048576 |
| 10 MB | 10485760 |
| 100 MB | 104857600 |
| 500 MB | 524288000 |
| 1 GB | 1073741824 |
| 4 GB | 4294967296 |

---

### `age`

Number of **days** since the file was last modified. Used with numeric operators.

```
field: "age"
op:    "gt"
value: "30"           # files not modified in the last 30 days
```

Age is calculated at the moment the rule is evaluated, not when the file was
first detected. A file detected today that was last modified 45 days ago has
age 45.

---

### `category`

The category SDO assigned based on extension and MIME type. Compared as a
string using the category names below.

```
field: "category"
op:    "eq"
value: "Images"       # exact match — capital I required
```

Valid category values: `Images`, `Videos`, `Audio`, `Documents`, `Archives`,
`Code`, `Executables`, `Fonts`, `Data`, `Ebooks`, `Torrents`, `Unknown`.

See [Section 8](#8-file-categories) for the full extension list per category.

---

### `mime`

The detected MIME type string, determined by reading the first 16 bytes of the
file (magic-byte detection), not just the extension.

```
field: "mime"
op:    "eq"
value: "application/pdf"

field: "mime"
op:    "contains"
value: "image/"       # matches image/jpeg, image/png, image/webp, etc.
```

Common MIME types detected:

| MIME type | Files |
|---|---|
| `image/jpeg` | .jpg, .jpeg |
| `image/png` | .png |
| `image/gif` | .gif |
| `image/webp` | .webp |
| `image/bmp` | .bmp |
| `image/tiff` | .tiff, .tif |
| `video/mp4` | .mp4, .m4v |
| `video/x-matroska` | .mkv |
| `audio/mpeg` | .mp3 |
| `audio/flac` | .flac |
| `audio/ogg` | .ogg |
| `application/pdf` | .pdf |
| `application/zip` | .zip |
| `application/x-7z-compressed` | .7z |
| `application/x-rar-compressed` | .rar |
| `application/x-elf` | Linux executables |
| `text/plain` | .txt, .md, .csv, .py, .js, and other text files |
| `application/octet-stream` | Unknown binary |

---

## 3. Condition Operators

### String operators

Used with `extension`, `name`, `category`, `mime`.

| Operator | Matches when | Case-sensitive? |
|---|---|---|
| `eq` | field exactly equals value | Follows `caseSensitive` flag |
| `ne` | field does not equal value | Follows `caseSensitive` flag |
| `contains` | value appears anywhere in field | Follows `caseSensitive` flag |
| `startswith` | field begins with value | Follows `caseSensitive` flag |
| `endswith` | field ends with value | Follows `caseSensitive` flag |
| `regex` | field matches ECMAScript regex pattern | Always case-insensitive |

#### `caseSensitive` flag

All string operators default to **case-insensitive** (`caseSensitive: false`).
Set `caseSensitive: true` to make `eq`, `ne`, `contains`, `startswith`,
`endswith` respect case. The `regex` operator is always case-insensitive
regardless of this flag.

#### Regex syntax

Uses C++ `std::regex` with ECMAScript syntax (same as JavaScript regex, without
the `/` delimiters).

```
# Match filenames starting with "invoice" followed by digits
field: "name"
op:    "regex"
value: "^invoice_\\d{4}"

# Match any image extension
field: "extension"
op:    "regex"
value: "^(jpg|jpeg|png|gif|webp|bmp|tiff)$"

# Match filenames that look like dates (YYYY-MM-DD anywhere in name)
field: "name"
op:    "regex"
value: "\\d{4}-\\d{2}-\\d{2}"
```

If the regex pattern is invalid, the condition evaluates to `false` (no match)
and a warning is written to the log.

---

### Numeric operators

Used with `size` (bytes) and `age` (days).

| Operator | Meaning |
|---|---|
| `gt` | field > value |
| `lt` | field < value |
| `eq` | field == value |

`ne`, `contains`, `startswith`, `endswith`, `regex` are not valid for numeric
fields and will silently evaluate to `false`.

---

## 4. Condition Logic (AND / OR)

The `conditionLogic` field controls how multiple conditions within a single
rule are combined.

### `"OR"` (default)

The rule matches if **any one** condition is true.

```
conditionLogic: "OR"
conditions:
  - field: extension, op: eq, value: jpg
  - field: extension, op: eq, value: jpeg
  - field: extension, op: eq, value: png
  - field: extension, op: eq, value: gif
```

This rule matches any image file with any of those four extensions.

### `"AND"`

The rule matches only if **all** conditions are true simultaneously.

```
conditionLogic: "AND"
conditions:
  - field: extension, op: eq,  value: mp4
  - field: size,      op: gt,  value: 1073741824
  - field: age,       op: gt,  value: 7
```

This rule matches MP4 files that are both larger than 1 GB AND older than 7
days — but not MP4 files that are small, and not large MP4 files that arrived
today.

### Mixing AND / OR logic

A single rule supports only one logic mode for all its conditions. To express
more complex logic (e.g. "PDF AND (large OR old)"), create two separate rules
with the same priority and destination:

- Rule A: `AND [ extension=pdf, size > 500MB ]` priority 20
- Rule B: `AND [ extension=pdf, age > 30 ]` priority 20

Since both have the same priority, whichever is listed first in the rules list
will be tested first.

---

## 5. Action Types

### `Move`

Moves the file from its current location to `targetDirectory`.

```
action:
  type: Move
  targetDirectory: /home/user/Documents/PDFs
  createSubfolders: true
  renamePattern: ""
```

- If `createSubfolders` is true, the target directory and any year/month
  subdirectories are created automatically.
- If a file with the same name already exists at the destination, SDO
  appends `_1`, `_2`, etc. to avoid overwriting.
- Move is atomic on the same filesystem. Across filesystems it becomes a
  copy-then-delete.
- The original database record is deleted; a new scan will re-catalogue the
  file at its new location.
- The move is recorded in the undo log and can be reversed with ↩ Undo.

---

### `Copy`

Copies the file to `targetDirectory`, leaving the original in place.

```
action:
  type: Copy
  targetDirectory: /mnt/backup/Downloads
  createSubfolders: false
```

- The original file stays in the watched folder and remains in the database.
- The copy is **not** automatically catalogued (it will be catalogued when the
  watcher detects it in the target directory, if that directory is also watched).
- Copy is recorded in the undo log. Undoing a copy deletes the copy.

---

### `Delete`

Removes the file.

```
action:
  type: Delete
```

- If **Move to Trash** is ON in Settings, the file is moved to
  `~/.local/share/Trash/files/` (standard XDG Trash location).
- If **Move to Trash** is OFF, the file is permanently deleted with no
  recovery path.
- Delete operations **cannot** be undone from SDO's undo log (though files
  in Trash can be recovered manually).
- The database record is deleted.

---

### `Rename`

Renames the file in place (does not change its directory).

```
action:
  type: Rename
  renamePattern: "{date}_{name}"
```

- `renamePattern` is required. See [Section 6](#6-rename-pattern-variables)
  for all available variables.
- The extension is always preserved and appended automatically.
- The rename is recorded in the undo log.

---

### `Skip`

Does nothing. Used as a placeholder to mark a rule as "catalogued but
intentionally not processed."

```
action:
  type: Skip
```

A Skip rule still wins the priority contest — no later rule will be tested
for files that match a Skip rule. This lets you exempt specific file types
from being caught by a broader rule further down the list.

---

## 6. Rename Pattern Variables

Used in the `renamePattern` field for `Move` and `Rename` actions.
Variables are enclosed in `{` and `}`.

| Variable | Expands to | Example |
|---|---|---|
| `{name}` | Filename without extension | `my document` |
| `{ext}` | File extension (no dot) | `pdf` |
| `{date}` | File modification date as `YYYY-MM-DD` | `2024-03-15` |
| `{year}` | Four-digit year of modification date | `2024` |
| `{month}` | Two-digit month of modification date | `03` |
| `{category}` | SDO category name | `Documents` |
| `{size}` | Human-readable file size | `2.4 MB` |

Variables can be combined freely:

| Pattern | Result (example) |
|---|---|
| `{name}` | `my document.pdf` (unchanged) |
| `{date}_{name}` | `2024-03-15_my document.pdf` |
| `{year}/{month}/{name}` | Used as a subfolder path in Move destination |
| `{category}_{date}_{name}` | `Documents_2024-03-15_my document.pdf` |
| `invoice_{date}` | `invoice_2024-03-15.pdf` |

The extension is always appended after the pattern. You do not include `{ext}`
at the end unless you want it to appear somewhere in the middle of the name.

If `renamePattern` is empty for a `Move` action, the original filename is
preserved unchanged at the destination.

---

## 7. Rule Priority and Evaluation Order

Rules are sorted **descending by priority** before evaluation.

```
Priority 100  ─── evaluated first
Priority  50
Priority  10  ─── built-in rules default
Priority   0  ─── evaluated last
```

### First-match wins

The moment a rule matches, evaluation stops. No other rules are tested for
that file. This means:

- Put **specific** rules at **high priority**.
- Put **broad/catch-all** rules at **low priority**.

### Same priority

If two rules have the same priority, they are tested in the order they appear
in the rules list (top to bottom in the Rules page). The first one that
matches wins.

### Disabled rules

Disabled rules are completely skipped — they do not consume a priority slot and
do not block lower-priority rules.

### Example priority arrangement

```
Priority 80  — "PDFs named 'invoice*' → Invoices folder"          (specific)
Priority 60  — "PDFs larger than 50 MB → Large Documents folder"  (specific)
Priority 10  — "All PDFs → Documents folder"                       (broad)
Priority  5  — "All files older than 90 days → Archive folder"     (catch-all)
```

An invoice PDF hits rule 1 (priority 80) and goes to Invoices. A large PDF
that is not named "invoice" hits rule 2 (priority 60). A small recent PDF
falls through to rule 3 (priority 10). An old file of any type that wasn't
caught by an earlier rule hits rule 4.

---

## 8. File Categories

SDO assigns every file to exactly one category based on its extension (and
MIME type as a tiebreaker).

| Category | Extensions (partial list) |
|---|---|
| **Images** | jpg, jpeg, png, gif, bmp, tiff, tif, webp, heic, heif, svg, raw, cr2, cr3, nef, arw, dng, avif, psd, xcf, ico |
| **Videos** | mp4, mkv, avi, mov, wmv, flv, webm, m4v, ts, mpg, mpeg, vob, 3gp, mxf, divx, ogv |
| **Audio** | mp3, flac, wav, aac, ogg, wma, m4a, opus, alac, aiff, ape, mid, midi, amr |
| **Documents** | pdf, doc, docx, xls, xlsx, ppt, pptx, odt, ods, odp, txt, rtf, md, markdown, tex, pages, numbers, key |
| **Archives** | zip, tar, gz, bz2, xz, 7z, rar, tgz, zst, lz4, cab, deb, rpm, iso, img, jar, whl |
| **Code** | py, js, ts, cpp, c, h, hpp, java, go, rs, rb, php, cs, swift, kt, sh, bash, html, css, json, xml, yaml, toml, sql |
| **Executables** | exe, msi, appimage, flatpak, run, elf, so, dll |
| **Fonts** | ttf, otf, woff, woff2, eot, fon |
| **Data** | sqlite, sqlite3, db, parquet, csv, tsv, ndjson, jsonl, avro, h5 |
| **Ebooks** | epub, mobi, azw, azw3, fb2, cbz, cbr |
| **Torrents** | torrent |
| **Unknown** | Any extension not in the above lists |

If an extension appears in the `Data` category but the file also has JSON
content, the extension map takes precedence. MIME detection is used only when
the extension is absent or in the `Unknown` category.

---

## 9. File Status Values

Each tracked file carries one of these status values:

| Status | Meaning |
|---|---|
| `Normal` | No special flags |
| `Duplicate` | SHA-256 hash matches another file in the database |
| `Large` | File size exceeds the large-file threshold |
| `Old` | File has not been modified in longer than the old-file age threshold |
| `Orphaned` | File was in the database but no longer exists on disk |
| `Temporary` | Reserved for future use |

A file can only have one status at a time. Priority: `Duplicate` >
`Large` > `Old` > `Normal`.

---

## 10. Complete Examples

### Example 1 — Move all images to dated subfolders

```
Name:           Images to Pictures
Priority:       10
Condition logic: OR
Conditions:
  extension eq jpg
  extension eq jpeg
  extension eq png
  extension eq gif
  extension eq webp
  extension eq heic
Action:         Move
Destination:    /home/user/Pictures/Downloads
Create subfolders: true
Rename pattern: (empty — preserve original filename)
```

Files land at `/home/user/Pictures/Downloads/2024/03/my_photo.jpg`.

---

### Example 2 — Archive large video files older than 2 weeks

```
Name:           Archive old large videos
Priority:       50
Condition logic: AND
Conditions:
  category eq Videos
  size     gt 524288000    (> 500 MB)
  age      gt 14
Action:         Move
Destination:    /mnt/nas/VideoArchive
Create subfolders: true
```

Only videos that are both large AND old are moved. A new large video is left
alone. An old small video is left alone.

---

### Example 3 — Rename invoice PDFs with date prefix

```
Name:           Date-prefix invoices
Priority:       80
Condition logic: AND
Conditions:
  extension eq  pdf
  name      contains  invoice
Action:         Rename
Rename pattern: {date}_{name}
```

`invoice_acme_q1.pdf` becomes `2024-03-15_invoice_acme_q1.pdf` in place.

---

### Example 4 — Delete torrent files automatically

```
Name:           Clean up torrent files
Priority:       10
Condition logic: OR
Conditions:
  extension eq torrent
Action:         Delete
```

With "Move to Trash" ON, torrent files go to Trash and can be recovered.
With "Move to Trash" OFF, they are permanently deleted.

---

### Example 5 — Exempt .txt files from the Documents rule

```
Name:           Skip plain text (keep in Downloads)
Priority:       15          ← higher than the Documents rule at priority 10
Condition logic: OR
Conditions:
  extension eq txt
  extension eq md
Action:         Skip
```

Text and Markdown files match the Skip rule first and are left in Downloads,
even though the Documents rule would also have matched them.

---

### Example 6 — Catch all unknown files after 60 days

```
Name:           Archive unknowns after 60 days
Priority:       1           ← lowest priority, runs last
Condition logic: AND
Conditions:
  category eq Unknown
  age      gt 60
Action:         Move
Destination:    /home/user/Downloads/Old-Misc
```

---

## 11. Built-in Rules Reference

These four rules are created automatically on first launch.
All are enabled but Auto-organize is OFF by default, so they do not move
anything until you turn Auto-organize ON.

### Images → Pictures

| Field | Value |
|---|---|
| ID | `builtin-images` |
| Priority | 10 |
| Logic | OR |
| Conditions | extension eq: jpg, jpeg, png, gif, bmp, tiff, webp, heic, svg, raw, cr2, nef |
| Action | Move |
| Destination | `~/Pictures/Downloaded` |
| Create subfolders | Yes (YYYY/MM) |

### Videos → Movies

| Field | Value |
|---|---|
| ID | `builtin-videos` |
| Priority | 10 |
| Logic | OR |
| Conditions | extension eq: mp4, mkv, avi, mov, wmv, flv, webm, m4v, ts, mpg, mpeg |
| Action | Move |
| Destination | `~/Videos/Downloaded` |
| Create subfolders | Yes (YYYY/MM) |

### Documents → Documents

| Field | Value |
|---|---|
| ID | `builtin-documents` |
| Priority | 10 |
| Logic | OR |
| Conditions | extension eq: pdf, doc, docx, xls, xlsx, ppt, pptx, odt, ods, txt, rtf, csv |
| Action | Move |
| Destination | `~/Documents/Downloaded` |
| Create subfolders | Yes (YYYY/MM) |

### Archives → Archives

| Field | Value |
|---|---|
| ID | `builtin-archives` |
| Priority | 10 |
| Logic | OR |
| Conditions | extension eq: zip, tar, gz, bz2, xz, 7z, rar, tgz, zst |
| Action | Move |
| Destination | `~/Downloads/Archives` |
| Create subfolders | Yes (YYYY/MM) |
