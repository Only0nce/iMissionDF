# Claude Team Migration Report

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
