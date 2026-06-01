# Security Policy

## Supported Versions

Security fixes are backported to the most recent minor release.
Older versions do not receive security updates.

| Version | Supported |
|---|---|
| 2.0.x | ✅ Yes — current release |
| < 2.0 | ❌ No — please upgrade |

---

## Reporting a Vulnerability

**Do not open a public GitHub issue for security vulnerabilities.**

Security issues in SDO can have real impact: the application reads, moves, and
deletes files on behalf of the user, and runs with the user's full filesystem
permissions. A vulnerability could lead to privilege escalation, data loss, or
arbitrary file operations.

### How to report

Email: **183800969+ABHISHEKKKKK12345@users.noreply.github.com**

Include:

- **Description** — what the vulnerability is and what an attacker could achieve
- **Affected version(s)** — output of `sdo --version`
- **Steps to reproduce** — exact sequence to trigger the issue
- **Proof of concept** — a minimal reproducer if possible
- **Impact assessment** — your assessment of severity

Encrypt sensitive reports with our PGP key (available on
[keys.openpgp.org](https://keys.openpgp.org) — search for
`183800969+ABHISHEKKKKK12345@users.noreply.github.com`).

### What to expect

| Timeline | Action |
|---|---|
| Within 48 hours | Acknowledgement of your report |
| Within 7 days | Initial assessment and severity classification |
| Within 30 days | Patch developed and tested (complex issues may take longer) |
| Within 45 days | Coordinated public disclosure |

We will keep you informed at each stage. If you have a deadline or coordinated
disclosure requirement, please mention it in your initial report.

---

## Disclosure Policy

We follow **coordinated disclosure**:

1. Reporter notifies us privately.
2. We develop and test a fix.
3. We release a patched version.
4. We publish a security advisory (GitHub Security Advisory) simultaneously
   with or immediately after the release.
5. Reporter is credited in the advisory unless they request anonymity.

We will not pursue legal action against researchers who report vulnerabilities
in good faith and follow this policy.

---

## Scope

### In scope

- **Path traversal** — a rule or watch path leading outside intended
  directories
- **Symlink following** — operations following symlinks to unintended locations
- **Privilege escalation** — gaining filesystem permissions beyond the running
  user
- **Command injection** — any shell injection via filenames or config values
- **SQLite injection** — database corruption or data leakage via crafted
  filenames
- **Denial of service** — crashing or hanging the process via crafted files or
  directory names
- **Insecure file operations** — TOCTOU (time-of-check/time-of-use) races in
  move/copy/delete operations

### Out of scope

- Issues in dependencies (GTK3, SQLite, OpenSSL) — report those upstream
- Issues requiring physical access to the machine
- Social engineering attacks
- UI annoyances or cosmetic bugs (use the regular bug tracker)

---

## Security Considerations for Users

SDO runs entirely as your user account. It does not use `sudo`, `setuid`, or
any elevated privilege. It can only access files you can access.

**Simulation mode** (`simulateMode: true` in Settings) logs all actions
without performing them. Enable this before deploying new rules to verify they
behave as expected.

**Watch path selection** — only add watch paths you trust. Adding `/` or `/home`
as a watch path with aggressive rules could affect files you did not intend.

**Rule destination directories** — verify destination paths before enabling
auto-organize. SDO creates destination directories automatically if they do
not exist.
