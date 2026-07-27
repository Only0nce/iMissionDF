# Claude Team Migration Report

## Hardening Follow-Up

A second-pass audit of the migration commit (`9fb3392`, `chore/claude-team-migration`)
found the two Bash-regex/`case`-glob hooks were bypassable, and no automated
test suite backed the claimed protection. This was addressed on branch
`fix/claude-team-migration-hardening` (based directly on `9fb3392`), in an
isolated worktree so the dirty `main` worktree was never touched.

**Root cause:** `block-destructive-git.sh` matched a fixed regex directly
against the raw command string, so anything that changed the surface form
without changing Git's behavior — a full path (`/usr/bin/git`), a wrapper
(`env`, `command`), or a Git global option before the subcommand
(`git -C <path> ...`, `git --git-dir=... ...`) — slipped past it uncaught.
`check-generated-files.sh` matched a `case` glob that required a leading `/`
(`*/Makefile`, `*/build/*`, `*/moc_*.cpp`), so a bare relative path
(`Makefile`, `build/generated.cpp`, `moc_example.cpp`) never matched.

**Confirmed bypasses** (reproduced against the pre-fix hooks by feeding
synthetic JSON payloads — no destructive command was ever executed):

```text
git add --all
git -C /tmp/example reset --hard HEAD
git -C /tmp/example clean -fd
git restore --source=HEAD .
git --git-dir=.git reset --hard HEAD
git add .
git add :/
false || git add --all
Makefile / build/generated.cpp / moc_example.cpp / qrc_qml.cpp / ui_MainWindow.h
  (as relative paths, without a leading "/")
```

An independent second-pass review of the *fixed* hook (Step 13 of the
hardening task) found one more gap the initial fix introduced: a raw newline
between two commands (`git status\ngit reset --hard HEAD`) was swallowed by
`shlex`'s whitespace splitting and merged into a single token stream, so only
the first subcommand (`status`) was ever inspected — the second line's
`reset --hard` was never examined. This is now fixed by splitting on raw
newlines (honoring `\`-newline continuation the way bash does) before
tokenizing each line, and is covered by dedicated regression tests.

**Fix:** both hooks now delegate to Python modules
(`.claude/hooks/lib/destructive_git_policy.py`,
`.claude/hooks/lib/generated_file_policy.py`) instead of Bash regex/glob:

- The Git-command policy tokenizes with `shlex`, splits on raw newlines and
  shell separators (`&&`, `||`, `;`, `|`, `&`), strips wrapper commands and
  `VAR=value` assignments, and walks past Git global options to find the real
  subcommand — see `docs/AI-WORKFLOW.md` for the exact blocked operations.
- The generated-file policy normalizes every path with
  `os.path.abspath(os.path.normpath(path))` before matching, so relative,
  absolute, `./`-prefixed, and `..`-containing paths are treated identically.

`check-secrets.sh` was reviewed and found to have no equivalent bug; it was
left unchanged.

A new adversarial test suite, `scripts/test-claude-hooks.sh`, replaces
narrative validation with executable evidence: 61 assertions covering the
allow-list, every bypass above (now blocked, including the newline gap found
in review), the generated-file matrix, and the secret-detection matrix
(including a check that no matched secret value is ever printed).
`scripts/validate-claude-team-config.sh` wraps that suite together with
syntax checks, inventory, executable-bit checks, `git diff --check`, a
tracked-sensitive-file scan, and a machine-specific absolute-path scan, and
prints a final `CLAUDE_CONFIG_VALIDATION=PASS|FAIL`.

Known residual gaps this text-based approach cannot close (documented, not
fixed, consistent with "hooks are guardrails, not a sandbox"): a destructive
command hidden inside `$(...)`/backtick command substitution, a Git alias
configured to do the same thing, or indirect execution through a wrapper
script/`xargs` are not detected. These require executing or fully
interpreting the shell to catch, which is out of scope for a lightweight
pre-execution text check.

See the hardening commit on `fix/claude-team-migration-hardening` (`git log`
on that branch) for the exact diff; it touches only `.claude/hooks/`,
`scripts/`, and documentation — no application C++, headers, QML, resources,
DSP, device-control, network, RF, GPIO, SPI/I2C, DMA, audio, modem, database,
or systemd file was changed. That commit had not been pushed to any remote
branch at the time this section was written — confirm with
`git branch -r --contains <hash>` before assuming otherwise.

## Executive Summary

Local Claude expertise was converted into a repository-owned, Git-shareable
configuration on `chore/claude-team-migration`, based on the verified
development branch `origin/dev-full-hardwareversion`. The dirty `main`
worktree was preserved by using a separate Git worktree.

The migration adds focused agents, repeatable skills, engineering rules,
project-safe hooks/settings, environment and build scripts, and team
documentation. No application source behavior was intentionally changed.

## Verified Repository State

- Remote default/production branch: `origin/main`
- Authoritative development/integration branch:
  `origin/dev-full-hardwareversion`
- `origin/future/optimize_water_decress_ram_consumption` is two commits on top
  of the development branch and is feature work, not the integration base.
- Original `main`: one commit behind `origin/main`
- Original worktree: one modified tracked `iScanMR10.pro.user`, no staged or
  deleted files, and four untracked files
- Authoritative build: qmake with `iScanMR10.pro`
- Untracked `CMakeLists.txt`: a 12-line unrelated reproduction project
- Development configuration: active `CONFIG+=HW_5G`

## Existing Local Agents

Seventeen agents were found:

- `fpga-rtl-engineer`
- `fullstack-system-engineer`
- `gpt-architect`
- `gpt-reviewer`
- `integration-reviewer`
- `low-level-engineer`
- `system-architect`
- `qt5-levels/qt5-codex-reviewer`
- `qt5-levels/qt5-explorer-haiku`
- `qt5-levels/qt5-l1-economy`
- `qt5-levels/qt5-l2-balanced`
- `qt5-levels/qt5-l3-deep`
- `qt5-levels/qt5-l4-claude-only-opus`
- `qt5-levels/qt5-l4-claude-only-sonnet`
- `qt5-levels/qt5-l5-tool-first`
- `qt5-levels/qt5-opus-reviewer`
- `qt5-levels/qt5-sonnet-verifier`

## Existing Local Skills

One local skill was found: `develop-system`.

## Existing Local Commands

No files existed under `~/.claude/commands/`.

## Secret and Privacy Findings

- Claude credential/account/project/plugin data exists locally and was not
  printed or copied.
- An Anthropic authentication environment variable is present; value
  `[REDACTED]`.
- Git credential storage and SSH private keys exist; values were not inspected.
- The original untracked 119 MB archive contains `.git`, Qt Creator state,
  caches, and machine-specific data. It was not extracted or executed.
- The development source contains an embedded legacy network-administrator
  password hash fallback; value `[REDACTED]`.
- The local MCP list contains a user-specific memory plugin and an absolute-path
  GPT wrapper that fails to connect. Neither was migrated.
- Two local agent files matched secret-related policy words. Manual inspection
  found no embedded credential value.
- No strong private-key/API-token signature was found in the intended migration
  files. Generic terms in security rules and scanner patterns are intentional.

## Selected Items for Migration

Local resources were distilled rather than copied verbatim:

| Local expertise | Repository result |
| --- | --- |
| Qt explorer | `codebase-explorer` |
| System architect | `engineering-architect` |
| Qt Opus reviewer | `qt5-cpp-reviewer` |
| Qt/QML guidance | `qml-ui-reviewer` |
| Low-level engineer | `linux-reviewer` |
| FPGA/hardware safety | `hardware-safety-reviewer` |
| Tool-first Qt verifier | `build-verifier` |
| Integration/finding reviewers | `independent-reviewer` |
| `develop-system` workflow | Eight scoped project skills |

Personal plugin references, model tiers, provider fallbacks, and local wrapper
calls were removed.

## Rejected Items and Reasons

- `fullstack-system-engineer`: broad web/backend role not specific to this Qt
  embedded repository.
- `gpt-architect`, `gpt-reviewer`, and `qt5-codex-reviewer`: require a
  machine-specific local wrapper and skills.
- Tiered Qt implementers: duplicate responsibilities and hard-code model/provider
  choices.
- User `settings.json`: contains personal plugins, marketplace, effort, and theme.
- User credentials, account state, projects, sessions, caches, and plugins:
  private/machine-specific.
- Local MCP servers: machine-specific and one is currently unhealthy.
- Original untracked archive, CMake reproduction, migration brief, and AstraRX
  validation report: unrelated local artifacts.

## Files Created

- `CLAUDE.md`
- `CONTRIBUTING.md`
- `.env.example`
- `.claude/settings.json`
- `.claude/agents/codebase-explorer.md`
- `.claude/agents/engineering-architect.md`
- `.claude/agents/qt5-cpp-reviewer.md`
- `.claude/agents/qml-ui-reviewer.md`
- `.claude/agents/linux-reviewer.md`
- `.claude/agents/hardware-safety-reviewer.md`
- `.claude/agents/build-verifier.md`
- `.claude/agents/independent-reviewer.md`
- `.claude/skills/learn-codebase/SKILL.md`
- `.claude/skills/check-project-status/SKILL.md`
- `.claude/skills/plan-change/SKILL.md`
- `.claude/skills/debug-build/SKILL.md`
- `.claude/skills/review-diff/SKILL.md`
- `.claude/skills/verify-qt5/SKILL.md`
- `.claude/skills/hardware-change-review/SKILL.md`
- `.claude/skills/create-handoff/SKILL.md`
- `.claude/rules/general-engineering.md`
- `.claude/rules/cpp-qt5.md`
- `.claude/rules/qml-ui.md`
- `.claude/rules/embedded-linux.md`
- `.claude/rules/hardware-safety.md`
- `.claude/rules/git-workflow.md`
- `.claude/hooks/block-destructive-git.sh`
- `.claude/hooks/check-generated-files.sh`
- `.claude/hooks/check-secrets.sh`
- `scripts/inventory-claude-local.sh`
- `scripts/bootstrap-dev.sh`
- `scripts/verify-dev-env.sh`
- `scripts/build-desktop.sh`
- `scripts/build-target.sh`
- `docs/AI-WORKFLOW.md`
- `docs/DEVELOPMENT.md`
- `docs/ARCHITECTURE.md`
- `docs/HARDWARE-SAFETY.md`
- `docs/claude-local-inventory.md`
- `docs/CLAUDE-TEAM-MIGRATION-REPORT.md`

## Files Modified

- `AGENTS.md`: corrected the active hardware selection and expanded verified
  architecture, contracts, safety, validation, and known risks.
- `.gitignore`: removed the unsafe global `*.txt` rule and added local Claude,
  Qt Creator/qmake, generated build, secret, and inventory patterns.

## Files Not Modified

- All C++, headers, QML, qmake project source, resources, databases, and hardware code
- Existing tracked `Makefile`
- Existing tracked `.qmake.stash`
- Existing tracked `iScanMR10.pro.user`
- Existing tracked `.qtc_clangd/` cache files
- User-level Claude and Git/SSH configuration
- Original dirty `main` worktree files

## Git Ignore Changes

New local artifacts are ignored:

- `.claude/settings.local.json`
- `CLAUDE.local.md`
- `claude-team-inventory.md` and `.txt`
- `.env` and `.env.*`, except `.env.example`
- `*.pem` and `*.key`
- `*.pro.user*`, `.qtc_clangd/`, `.qmake.stash`
- generated `Makefile*` and editor backups

Ignore rules do not untrack historical artifacts. The 1,641 tracked local/cache
files require a separate reviewed cleanup.

## Shared Configuration

`.claude/settings.json` contains only repository-relative hooks:

- block destructive Git and bulk staging commands;
- block edits/writes to generated or machine-local files;
- scan edited/written files for strong secret indicators.

No MCP server was added because the discovered servers were not portable
team dependencies.

## Machine-Specific Configuration

Keep machine-only settings in ignored `.claude/settings.local.json`. Keep
credentials, account state, authentication, sessions, projects, histories,
plugins, private MCP configuration, target sysroots, target qmake paths, and
secret environment values outside Git.

Each user must authenticate Claude Code independently.

## Laptop Setup Procedure

After this branch is reviewed, pushed, and merged:

```bash
git clone git@github.com:Only0nce/iMissionDF.git
cd iMissionDF
git switch dev-full-hardwareversion
./scripts/verify-dev-env.sh
./scripts/bootstrap-dev.sh
claude
```

If the migration branch must be tested before merge, first push it, then use:

```bash
git fetch origin
git switch --track origin/chore/claude-team-migration
./scripts/verify-dev-env.sh
```

## Team Member Setup Procedure

```bash
git clone git@github.com:Only0nce/iMissionDF.git
cd iMissionDF
git switch dev-full-hardwareversion
./scripts/verify-dev-env.sh
./scripts/bootstrap-dev.sh
./scripts/build-desktop.sh
claude
```

Install only reviewed missing dependencies and rerun verification. Target users
must configure their own target qmake/sysroot outside Git.

## Build Validation

Desktop qmake configuration was attempted with:

```bash
./scripts/build-desktop.sh
```

Result: **FAIL (environment dependencies)**.

- Qt/qmake detected: Qt 5.12.8 / qmake 3.1 / `linux-g++`
- Active source selector detected: `CONFIG+=HW_5G`
- Missing Qt module: `Qt5Multimedia`
- Missing pkg-config modules: `libgps`, `geographiclib`
- qmake stopped at `Unknown module(s) in QT: multimedia`

No package installation was authorized or performed. No compiler stage was
reached, so source compilation success is not claimed.

Target build: **NOT RUN**. `TARGET_QMAKE` and the target toolchain were not
configured. Hardware validation: **NOT RUN**.

## Tests Performed

- shell syntax (`bash -n`) for all hooks and scripts
- JSON syntax for `.claude/settings.json`
- Claude Code `doctor`
- safe/destructive Git hook behavior
- generated-file hook allow/block behavior
- secret hook against `.env.example`
- `scripts/bootstrap-dev.sh`
- `scripts/verify-dev-env.sh`
- `scripts/inventory-claude-local.sh` and ignore verification
- `git diff --check`
- strong and generic secret-indicator scans with values withheld
- real desktop qmake configure attempt

## Tests Not Performed

- full C++ link/build (blocked during qmake configuration)
- automated application tests (none were found)
- QML runtime/UI interaction
- target toolchain build
- target deployment or service checks
- GPIO, RF, SPI/I2C, DMA, DSP, audio, modem, network-state, or physical hardware tests

## Remaining Risks

- Required desktop development packages are missing.
- The legacy embedded password hash remains in application source.
- Historical generated/IDE/cache files remain tracked and contain machine-specific data.
- The original 119 MB full-repository archive remains untracked in the original
  worktree and should not be committed.
- The original `main` worktree remains dirty and behind its upstream by design.
- Runtime, target, and physical hardware behavior remain unvalidated.
- Branch roles must be re-verified if repository governance changes.

## Recommended Next Actions

1. Review this migration diff and local commit.
2. Install the reported desktop dependencies only with user/admin approval,
   then rerun `./scripts/verify-dev-env.sh` and `./scripts/build-desktop.sh`.
3. Rotate and remove the embedded legacy password-hash fallback in a separate
   security-reviewed change.
4. Review removal of the 1,641 tracked generated/IDE/cache artifacts in a
   separate cleanup pull request.
5. Keep the original archive outside the repository or move it to approved
   encrypted backup storage.
6. Push the migration branch and open a pull request into the verified
   development branch.
