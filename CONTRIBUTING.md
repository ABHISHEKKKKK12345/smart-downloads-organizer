# Contributing to Smart Downloads Organizer

Thank you for your interest in contributing. This document explains how to
report bugs, propose features, and submit code changes.

---

## Table of Contents

- [Code of Conduct](#code-of-conduct)
- [Reporting Bugs](#reporting-bugs)
- [Requesting Features](#requesting-features)
- [Development Setup](#development-setup)
- [Branching Strategy](#branching-strategy)
- [Submitting a Pull Request](#submitting-a-pull-request)
- [Commit Messages](#commit-messages)
- [Code Style](#code-style)
- [Tests](#tests)
- [CI Requirements](#ci-requirements)

---

## Code of Conduct

All contributors are expected to follow the [Code of Conduct](CODE_OF_CONDUCT.md).
Maintainers reserve the right to remove contributions or ban contributors who
violate it.

---

## Reporting Bugs

Before filing a bug:

1. Search existing [issues](https://github.com/ABHISHEKKKKK12345/smart-downloads-organizer/issues) to avoid
   duplicates.
2. Reproduce the problem on the latest `main` branch.

When filing, include:

- **SDO version** — output of `sdo --version`
- **Linux distribution and version** — e.g. Ubuntu 24.04, Fedora 40
- **GTK version** — `pkg-config --modversion gtk+-3.0`
- **Steps to reproduce** — exact sequence of actions
- **Expected behaviour**
- **Actual behaviour**
- **Log file** — `~/.local/share/smart-downloads-organizer/sdo.log` (attach
  or paste relevant lines)

If the bug is a **security vulnerability**, do **not** open a public issue.
See [SECURITY.md](SECURITY.md) for the disclosure process.

---

## Requesting Features

Open an issue with the label `enhancement`. Describe:

- **The problem you are trying to solve** — not just the solution you want.
- **Proposed solution** — how you imagine it would work.
- **Alternatives considered** — other approaches you thought about.

Feature requests that require adding a new file-system API (e.g. Windows
`ReadDirectoryChangesW`) must include a portability plan.

---

## Development Setup

```bash
# Clone
git clone https://github.com/ABHISHEKKKKK12345/smart-downloads-organizer.git
cd organizer

# Install dependencies (Ubuntu/Debian)
sudo apt install cmake ninja-build libgtk-3-dev libsqlite3-dev \
                 libssl-dev uuid-dev pkg-config g++ clang-format clang-tidy

# Build (debug mode with sanitizers)
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DSDO_BUILD_TESTS=ON
cmake --build build --parallel

# Run tests
cd build && ctest --output-on-failure

# Run the app
./build/bin/sdo
```

For a detailed architecture walkthrough see [docs/developer-guide.md](docs/developer-guide.md).

---

## Branching Strategy

| Branch | Purpose |
|---|---|
| `main` | Always releasable. Protected — no direct pushes. |
| `develop` | Integration branch. PRs target here. |
| `feature/<name>` | New features. Branch from `develop`. |
| `fix/<name>` | Bug fixes. Branch from `develop` (or `main` for hotfixes). |
| `release/<version>` | Release preparation. Merges to `main` and back to `develop`. |

```
feature/xyz ──▶ develop ──▶ release/2.1.0 ──▶ main (tag v2.1.0)
                                          └──▶ develop
```

**Do not open PRs directly to `main`** unless it is an emergency hotfix for
a critical security issue.

---

## Submitting a Pull Request

1. Fork the repository and create your branch from `develop`:
   ```bash
   git checkout develop
   git pull origin develop
   git checkout -b feature/my-feature
   ```

2. Make your changes following the coding conventions in
   [docs/developer-guide.md](docs/developer-guide.md).

3. Add or update tests. Every new function must have corresponding tests in
   `tests/`. The CI will reject PRs that reduce test coverage.

4. Run the full test suite locally:
   ```bash
   cd build && ctest --output-on-failure
   ```

5. Format your code:
   ```bash
   clang-format -i src/*.cpp include/*.hpp
   ```

6. Commit and push:
   ```bash
   git push origin feature/my-feature
   ```

7. Open a pull request against `develop`. Fill in the PR template:
   - What does this change?
   - Why is it needed?
   - How was it tested?
   - Any breaking changes?

8. Address review comments. Maintainers may request changes before merging.

### PR checklist

- [ ] All existing tests pass (`ctest --output-on-failure`)
- [ ] New code has tests
- [ ] `clang-format` applied — zero formatting diff
- [ ] `clang-tidy` passes with no new errors
- [ ] Build is clean with `-Wall -Wextra` — zero new warnings
- [ ] `docs/` updated if behaviour changes
- [ ] `CHANGELOG.md` has an entry under `[Unreleased]`

---

## Commit Messages

Format: `type(scope): short description`

```
feat(rules): add 'endswith' condition operator
fix(watcher): handle IN_MOVED_FROM without matching IN_MOVED_TO
docs(developer-guide): add thread model diagram
test(database): add pruneOldRecords coverage
chore(cmake): remove deprecated -Wpedantic flag
```

**Types:** `feat`, `fix`, `docs`, `test`, `refactor`, `perf`, `chore`, `ci`

**Scope:** module name — `rules`, `watcher`, `database`, `analyzer`, `gui`,
`cmake`, `packaging`

Keep the subject line under 72 characters. Use the body for detail when needed.
Reference issues: `Closes #42`, `Fixes #17`.

---

## Code Style

All style rules are enforced by `.clang-format`. Run `clang-format -i` before
every commit. The key rules:

- 4-space indentation, no tabs
- 100-column limit
- LLVM base style
- Member variables prefixed `m_`
- Classes `CamelCase`, methods `camelBack`, constants `UPPER_CASE`

See [docs/developer-guide.md § Coding Conventions](docs/developer-guide.md#9-coding-conventions)
for the full guide including error handling and thread safety requirements.

---

## Tests

Tests live in `tests/`. Each module has one test file. Tests use the
self-contained framework in the test files — no external library.

When adding a feature:

1. Add tests **in the same PR** as the feature — not as a follow-up.
2. Test the happy path, edge cases, and failure cases.
3. For `FileWatcher` tests, use the `EventCollector` + `TempDir` helpers
   already defined in `tests/test_file_watcher.cpp`.
4. For database tests, open a temp database with a unique path and close/delete
   it in the test.
5. For engine tests, set `simulateMode = true` to avoid touching the real
   filesystem.

---

## CI Requirements

Every PR must pass the CI pipeline defined in `.github/workflows/build.yml`:

| Job | Requirement |
|---|---|
| `build-ubuntu` (22.04 + 24.04, Debug + Release) | Zero build errors, zero warnings |
| `build-fedora` | Builds and tests pass |
| `asan` | No AddressSanitizer or UBSan errors |
| `static-analysis` | No `clang-tidy` errors (warnings allowed) |

PRs that fail CI will not be merged regardless of review approval.
