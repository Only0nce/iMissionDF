# Codex Task: Convert Local Claude Agents and Skills into a Shared Git-Based Project Setup

## Role

Act as a Senior Software Architect, DevOps Engineer, Git Administrator, Claude Code Configuration Specialist, and Security Reviewer.

Your task is to inspect the current repository and the local Claude Code configuration, then design and implement a safe project-owned Agent and Skill system that can be shared through Git and used consistently on:

* The current PC
* A laptop
* Other developers' computers
* Multiple Git branches and Git worktrees

The repository must remain functional throughout the migration.

---

# Primary Goal

Convert the current machine-specific Claude Code setup into a repository-managed setup using:

```text
CLAUDE.md
AGENTS.md
.claude/agents/
.claude/skills/
.claude/rules/
.claude/hooks/
.claude/settings.json
```

The shared configuration must be committed to Git.

Machine-specific credentials, sessions, caches, histories, private configuration, and secrets must remain outside Git.

---

# Important Safety Rules

1. Do not modify files before completing the inspection.
2. Do not run scripts discovered inside archives or repositories during the inspection.
3. Do not copy the entire `~/.claude` directory into the repository.
4. Do not expose or print secrets.
5. Do not commit API keys, tokens, passwords, cookies, SSH keys, credentials, or session data.
6. Do not use:

```bash
git reset --hard
git clean -fd
git checkout -- .
git restore .
git push --force
git add -A
```

unless explicitly approved after presenting the exact impact.

7. Do not delete tracked files without verifying why they appear deleted.
8. Do not change the current application architecture unnecessarily.
9. Preserve all existing source code behavior.
10. Preserve existing Qt, QML, C++, hardware, network, database, and protocol contracts.
11. Do not convert the project from qmake to CMake unless explicitly requested.
12. Do not assume the newest branch is the correct production branch.
13. Verify the actual repository state before making recommendations.
14. Clearly separate:

```text
Verified facts
Inferences
Risks
Unknowns
Recommended actions
Implemented changes
Unimplemented changes
```

---

# Project Location

Start from the current working directory.

Determine the repository root using:

```bash
git rev-parse --show-toplevel
```

Store it as:

```bash
PROJECT_ROOT="$(git rev-parse --show-toplevel)"
```

Do not assume a hard-coded path.

---

# Phase 1: Inspect the Repository

Collect the following information without changing files.

## Repository identity

Run:

```bash
git rev-parse --show-toplevel
git remote -v
git status --short --branch
git branch -vv
git branch -a
git tag --list
git log --graph --decorate --oneline --all -50
```

## Working tree state

Run:

```bash
git status --porcelain=v1
git diff --stat
git diff --name-status
git diff --cached --stat
git diff --cached --name-status
git ls-files -d
git ls-files --others --exclude-standard
```

Report:

* Current branch
* Upstream branch
* Ahead/behind state
* Modified files
* Deleted tracked files
* Staged files
* Untracked files
* Ignored files that may matter
* Whether the repository is safe to modify

Do not stage or restore anything during this phase.

---

# Phase 2: Determine the Real Build System

Inspect the project for:

```text
*.pro
*.pri
CMakeLists.txt
meson.build
Makefile
package.json
Cargo.toml
pyproject.toml
Dockerfile
docker-compose.yml
```

Determine which build system is actually authoritative.

For a Qt5 project, inspect:

```bash
find "$PROJECT_ROOT" \
    -maxdepth 4 \
    \( -name '*.pro' -o -name '*.pri' -o -name 'CMakeLists.txt' \) \
    -print
```

Verify whether any root-level `CMakeLists.txt` is:

* The real build system
* A reproduction project
* An experiment
* An accidentally copied file

Do not treat an untracked experimental file as the main build system.

---

# Phase 3: Inspect Existing Project Instructions

Search for existing AI and development instructions:

```bash
find "$PROJECT_ROOT" \
    -maxdepth 6 \
    \( \
        -name 'CLAUDE.md' \
        -o -name 'CLAUDE.local.md' \
        -o -name 'AGENTS.md' \
        -o -name '.cursorrules' \
        -o -name 'copilot-instructions.md' \
        -o -path '*/.claude/*' \
        -o -path '*/.github/instructions/*' \
    \) \
    -print
```

Search all branches for important instruction files:

```bash
git log --all --name-only --pretty=format: -- \
    CLAUDE.md \
    AGENTS.md \
    '.claude/**' |
    sed '/^$/d' |
    sort -u
```

Where an instruction file exists only on another branch, inspect it using:

```bash
git show <branch>:<path>
```

Do not switch branches just to read a file.

Evaluate whether the instructions still match the actual source code.

---

# Phase 4: Inspect Local Claude Code Configuration

Inspect only file names and safe metadata first.

## Claude version

Run:

```bash
command -v claude || true
claude --version 2>/dev/null || true
```

## Local directories

Inspect:

```bash
find ~/.claude/agents \
    -type f \
    -maxdepth 6 \
    -printf '%P\n' 2>/dev/null |
    sort

find ~/.claude/skills \
    -type f \
    -maxdepth 8 \
    -printf '%P\n' 2>/dev/null |
    sort

find ~/.claude/commands \
    -type f \
    -maxdepth 6 \
    -printf '%P\n' 2>/dev/null |
    sort

find ~/.claude/hooks \
    -type f \
    -maxdepth 6 \
    -printf '%P\n' 2>/dev/null |
    sort
```

Also check whether these exist:

```text
~/.claude/settings.json
~/.claude/settings.local.json
~/.claude.json
~/.claude/.credentials.json
~/.claude/projects/
~/.claude/history/
~/.claude/transcripts/
~/.claude/plugins/
```

Do not print the contents of credential, session, account, history, transcript, or cache files.

---

# Phase 5: Inspect Agent and Skill Contents Safely

For each file under:

```text
~/.claude/agents/
~/.claude/skills/
~/.claude/commands/
```

classify it as one of:

```text
Safe to share
Safe after sanitization
Machine-specific
Project-specific
General reusable
Contains possible secret
Deprecated
Duplicate
Unknown
```

Before reading full content, scan for possible secret patterns.

Use a safe scanner such as:

```bash
grep -RInE \
    '(api[_-]?key|token|secret|password|passwd|authorization|bearer|private[_-]?key|BEGIN [A-Z ]*PRIVATE KEY|ssh-rsa|github_pat_|ghp_|sk-[A-Za-z0-9])' \
    ~/.claude/agents \
    ~/.claude/skills \
    ~/.claude/commands \
    2>/dev/null || true
```

This scanner is only an indicator.

Do not copy files automatically if a possible secret is detected.

Do not include actual secret values in the report.

Redact findings using:

```text
[REDACTED]
```

---

# Phase 6: Create a Local Inventory Report

Create:

```text
docs/claude-local-inventory.md
```

The report must contain:

## System

* OS
* Architecture
* Host type
* Claude version
* Git version
* Qt version where applicable
* qmake version where applicable
* Compiler version

## Repository

* Repository root
* Remote names
* Current branch
* Candidate development branches
* Working-tree state
* Deleted tracked file count
* Modified file count
* Untracked file count

## Local Claude resources

Create a table:

| Type    | Local path                | Classification         | Recommended project path | Action              |
| ------- | ------------------------- | ---------------------- | ------------------------ | ------------------- |
| Agent   | `~/.claude/agents/...`    | General reusable       | `.claude/agents/...`     | Copy after review   |
| Skill   | `~/.claude/skills/...`    | Project-specific       | `.claude/skills/...`     | Copy                |
| Command | `~/.claude/commands/...`  | Deprecated or reusable | `.claude/skills/...`     | Convert             |
| Setting | `~/.claude/settings.json` | Mixed                  | `.claude/settings.json`  | Extract safe subset |

Never include secret values.

---

# Phase 7: Design the Repository-Owned Structure

Prepare the following structure:

```text
PROJECT_ROOT/
├── CLAUDE.md
├── AGENTS.md
├── CONTRIBUTING.md
│
├── .claude/
│   ├── settings.json
│   │
│   ├── agents/
│   │   ├── codebase-explorer.md
│   │   ├── engineering-architect.md
│   │   ├── qt5-cpp-reviewer.md
│   │   ├── qml-ui-reviewer.md
│   │   ├── linux-reviewer.md
│   │   ├── hardware-safety-reviewer.md
│   │   ├── build-verifier.md
│   │   └── independent-reviewer.md
│   │
│   ├── skills/
│   │   ├── learn-codebase/
│   │   │   └── SKILL.md
│   │   ├── check-project-status/
│   │   │   └── SKILL.md
│   │   ├── plan-change/
│   │   │   └── SKILL.md
│   │   ├── debug-build/
│   │   │   └── SKILL.md
│   │   ├── review-diff/
│   │   │   └── SKILL.md
│   │   ├── verify-qt5/
│   │   │   └── SKILL.md
│   │   ├── hardware-change-review/
│   │   │   └── SKILL.md
│   │   └── create-handoff/
│   │       └── SKILL.md
│   │
│   ├── rules/
│   │   ├── general-engineering.md
│   │   ├── cpp-qt5.md
│   │   ├── qml-ui.md
│   │   ├── embedded-linux.md
│   │   ├── hardware-safety.md
│   │   └── git-workflow.md
│   │
│   └── hooks/
│       ├── block-destructive-git.sh
│       ├── check-generated-files.sh
│       └── check-secrets.sh
│
├── scripts/
│   ├── bootstrap-dev.sh
│   ├── verify-dev-env.sh
│   ├── inventory-claude-local.sh
│   ├── build-desktop.sh
│   └── build-target.sh
│
└── docs/
    ├── AI-WORKFLOW.md
    ├── DEVELOPMENT.md
    ├── ARCHITECTURE.md
    ├── HARDWARE-SAFETY.md
    └── claude-local-inventory.md
```

Adapt the structure to the actual project.

Do not create meaningless empty files.

---

# Phase 8: Create `CLAUDE.md`

Create a concise root-level `CLAUDE.md`.

It must not contain every detailed procedure.

It should provide:

```markdown
# Project Instructions

@AGENTS.md

## Build System

- State the verified build system.
- State the main project file.
- State desktop and target build differences.

## Engineering Rules

- Inspect before editing.
- Preserve behavior and interfaces.
- Do not modify generated files.
- Do not change hardware state without approval.
- Do not claim build or test success unless actually performed.
- Report exact files and validation results.

## Git Rules

- Do not commit directly to protected branches.
- Do not use destructive Git commands.
- Stage only intended files.
- Use feature branches.
- Require review before merge.

## Project Skills

- `/learn-codebase`
- `/check-project-status`
- `/plan-change`
- `/debug-build`
- `/review-diff`
- `/verify-qt5`
- `/hardware-change-review`
- `/create-handoff`
```

Use imports where useful instead of duplicating large documents.

---

# Phase 9: Create or Update `AGENTS.md`

The file must explain the actual project architecture.

Include:

* Main application purpose
* Important modules
* Source tree
* Build system
* Qt and QML architecture
* Threading rules
* Network and protocol contracts
* Database contracts
* Hardware interfaces
* Generated files
* Validation commands
* Known risks
* Unsupported assumptions

Verify every important statement against the source.

Mark unresolved items explicitly.

---

# Phase 10: Convert Local Agents

For each safe local Agent:

1. Preserve its useful expertise.
2. Remove personal paths.
3. Remove machine-specific paths.
4. Remove credentials.
5. Remove instructions that conflict with repository policy.
6. Remove duplicate content already covered by `CLAUDE.md`.
7. Give it a clear single responsibility.
8. Specify whether it may edit files.
9. Specify required validation.
10. Specify expected output.

Example project Agent:

```markdown
---
name: codebase-explorer
description: Read-only exploration of the repository, architecture, dependencies, and affected code paths.
tools:
  - Read
  - Grep
  - Glob
  - Bash
---

# Responsibility

Inspect and explain the codebase without modifying files.

# Rules

- Do not create, edit, rename, or delete files.
- Do not run destructive commands.
- Verify documentation against source.
- Distinguish facts, inferences, and unknowns.
- Identify exact files, classes, functions, signals, slots, and dependencies.

# Output

1. Scope inspected
2. Verified architecture
3. Relevant code paths
4. Risks
5. Unknowns
6. Recommended next step
```

Do not make every Agent a general-purpose Agent.

---

# Phase 11: Convert Local Commands into Skills

If local reusable commands exist under:

```text
~/.claude/commands/
```

evaluate whether they should become project Skills.

A Skill must live at:

```text
.claude/skills/<skill-name>/SKILL.md
```

Example:

```markdown
---
name: check-project-status
description: Inspect repository, branch, local changes, build system, and recent work before starting a task.
---

# Purpose

Establish a verified current project state before modification.

# Procedure

1. Determine repository root.
2. Read `CLAUDE.md` and `AGENTS.md`.
3. Inspect branch and upstream.
4. Inspect staged, unstaged, deleted, and untracked files.
5. Inspect recent commits.
6. Determine the authoritative build system.
7. Identify unfinished work.
8. Do not modify files.

# Required Output

## Repository State

## Current Branch

## Local Changes

## Recent Relevant Commits

## Build Status

## Risks

## Recommended Next Action
```

---

# Phase 12: Create Shared Rules

## General engineering

Create:

```text
.claude/rules/general-engineering.md
```

Include:

* Root-cause analysis before modification
* Preserve functionality
* Avoid unnecessary rewrites
* Verify assumptions
* Explain trade-offs
* Separate static review, build, test, simulation, and hardware validation

## Qt5 and C++

Create:

```text
.claude/rules/cpp-qt5.md
```

Include:

* QObject ownership
* Thread affinity
* QThread worker pattern
* QTimer creation in owner thread
* Signal and slot compatibility
* Lifetime and destruction
* Locking
* Blocking I/O
* QProcess cleanup
* Memory and file descriptor management
* Qt5 compatibility

## QML

Create:

```text
.claude/rules/qml-ui.md
```

Include:

* Preserve user interaction
* Avoid binding loops
* Avoid unnecessary redraw
* Avoid heavy JavaScript in render-critical paths
* Preserve signal contracts
* Consider touch targets
* Consider readability and accessibility

## Hardware safety

Create:

```text
.claude/rules/hardware-safety.md
```

Include:

* Identify whether commands change hardware state
* Separate read-only inspection from active testing
* Warn before GPIO, RF, TX, DMA, SPI write, reset, power, or network interface changes
* Require a validation plan
* Never claim hardware success without hardware evidence

---

# Phase 13: Shared Settings

Create:

```text
.claude/settings.json
```

Only place project-safe settings inside it.

Do not copy the complete user-level settings file.

Do not include:

```text
Account data
Authentication data
Private MCP tokens
User-specific paths
Session configuration
Machine-specific allowlists
Personal preferences unrelated to the project
```

Keep machine-specific settings in:

```text
.claude/settings.local.json
```

Ensure it is ignored by Git.

---

# Phase 14: MCP Configuration

If MCP servers are used, inspect:

```bash
claude mcp list 2>/dev/null || true
```

Create `.mcp.json` only where it benefits the whole team.

Replace secret values with environment variables.

Example:

```json
{
  "mcpServers": {
    "project-tools": {
      "type": "http",
      "url": "${PROJECT_MCP_URL}",
      "headers": {
        "Authorization": "Bearer ${PROJECT_MCP_TOKEN}"
      }
    }
  }
}
```

Create:

```text
.env.example
```

Example:

```dotenv
PROJECT_MCP_URL=
PROJECT_MCP_TOKEN=
```

Never create a real `.env` file containing copied secrets.

---

# Phase 15: Fix `.gitignore`

Inspect the current `.gitignore`.

Add appropriate patterns for:

```gitignore
# Claude local-only
.claude/settings.local.json
CLAUDE.local.md

# Qt Creator
*.pro.user
*.pro.user.*
.qtc_clangd/
.qmake.stash

# Generated qmake files
Makefile
Makefile.*
moc_*.cpp
moc_*.h
qrc_*.cpp
qrc_*.h
ui_*.h

# Build outputs
build/
build-*/
Build/
out/
bin/
obj/
cmake-build-*/
*.o
*.a
*.so
*.so.*
*.exe

# Local environment and secrets
.env
.env.*
!.env.example
*.pem
*.key

# Logs and temporary files
*.log
*.tmp
*.swp
*~
```

Do not ignore all `.txt`, `.md`, `.json`, or `.yaml` files globally.

If generated files are already tracked, report them first.

Do not remove them from Git until the impact is reviewed.

---

# Phase 16: Create Environment Inventory Script

Create:

```text
scripts/inventory-claude-local.sh
```

Requirements:

* Read-only
* Never print file contents that may contain secrets
* Show relative Agent and Skill paths
* Show Claude version
* Show Git status
* Show build tool versions
* Scan for possible secret indicators
* Redact sensitive output
* Exit nonzero only for real script errors

The script should produce:

```text
claude-team-inventory.md
```

or:

```text
claude-team-inventory.txt
```

The generated inventory file must be ignored by Git unless it has been manually sanitized.

---

# Phase 17: Create Bootstrap Script

Create:

```text
scripts/bootstrap-dev.sh
```

The script must:

1. Detect Linux distribution.
2. Detect architecture.
3. Verify Git.
4. Verify Claude Code.
5. Verify project build dependencies.
6. Verify Qt and qmake.
7. Verify compiler.
8. Verify optional target-specific dependencies.
9. Verify required environment variables without printing their values.
10. Verify Git hooks or project settings.
11. Never install packages without explicit confirmation.
12. Never modify global Claude configuration automatically.
13. Print actionable commands for missing dependencies.

Do not assume PC and Laptop use identical package paths.

---

# Phase 18: Create Environment Verification Script

Create:

```text
scripts/verify-dev-env.sh
```

Output a table similar to:

```text
Component              Status      Detected
--------------------------------------------------------
Git                    PASS        2.x
Claude Code            PASS        x.x.x
Architecture           PASS        x86_64
Qt                     PASS        5.15.x
qmake                  PASS        /usr/bin/qmake
Compiler               PASS        GCC x.x
Desktop build profile  PASS        linux-g++
Target build profile   SKIP        Not on target machine
Repository state       WARN        Local modifications exist
Secrets scan           PASS        No obvious tracked secrets
```

The script must not expose secret values.

---

# Phase 19: Git Collaboration Workflow

Create:

```text
docs/AI-WORKFLOW.md
```

Document this workflow:

```text
main
  Production-ready code only

integration or develop
  Reviewed integration work

feature/<issue>-<description>
fix/<issue>-<description>
refactor/<issue>-<description>
chore/<description>
```

Recommended process:

```bash
git fetch --all --prune
git switch integration
git pull --ff-only
git switch -c feature/<name>
```

Before commit:

```bash
git status --short
git diff --check
git diff --stat
```

Stage specific files:

```bash
git add path/to/file1 path/to/file2
```

Do not use `git add -A` by default.

Commit:

```bash
git commit -m "type(scope): description"
```

Push:

```bash
git push -u origin feature/<name>
```

Require Pull Request review before merge.

---

# Phase 20: Multi-Agent and Git Worktree Workflow

Document that Agents must not modify the same working tree concurrently.

For parallel work, use Git worktrees:

```bash
git worktree add \
    ../project-audio \
    -b fix/audio-stutter \
    integration

git worktree add \
    ../project-ui \
    -b feature/ui-improvement \
    integration
```

Each Agent must receive:

* Its own worktree
* Its own branch
* A clearly defined scope
* A validation command
* A required output format

Do not share uncommitted changes between Agents.

---

# Phase 21: Recommended Agent Workflow

Document the standard pipeline:

```text
1. Codebase Explorer
   Read-only inspection

2. Engineering Architect
   Root-cause analysis and implementation plan

3. Implementation Agent
   Scoped modification

4. Build Verifier
   Compile and static validation

5. Independent Reviewer
   Review actual diff and regression risks

6. Human Reviewer
   Approve or reject Pull Request
```

The implementation Agent must not approve its own work.

---

# Phase 22: Secrets and Security Review

Before staging files, scan the intended changes.

Use:

```bash
git diff --check
git diff --cached --check
git grep -nEI \
    '(api[_-]?key|token|secret|password|authorization|bearer|private[_-]?key|BEGIN [A-Z ]*PRIVATE KEY|github_pat_|ghp_|sk-[A-Za-z0-9])' \
    -- \
    ':!*.lock' \
    ':!docs/claude-local-inventory.md'
```

Also inspect filenames:

```bash
find "$PROJECT_ROOT" \
    -type f \
    \( \
        -name '*.pem' \
        -o -name '*.key' \
        -o -name '.env' \
        -o -name '.env.*' \
    \) \
    -print
```

Do not delete suspicious files automatically.

Report them and recommend remediation.

---

# Phase 23: Validation

After implementation, validate:

## Structure

```bash
find .claude -maxdepth 4 -type f | sort
```

## Git state

```bash
git status --short
git diff --check
git diff --stat
git diff --name-status
```

## No tracked local settings

```bash
git ls-files |
    grep -E \
    '(^|/)(settings\.local\.json|CLAUDE\.local\.md|\.env|.*\.pro\.user|\.qtc_clangd/)' &&
    echo "ERROR: local files are tracked" ||
    echo "PASS: local files are not tracked"
```

## Secret indicators

Run the secret checks again.

## Build

Use the verified build command only.

For a qmake Qt5 project, a typical desktop validation may be:

```bash
mkdir -p build-codex-desktop
cd build-codex-desktop
qmake ../<actual-project-file>.pro -spec linux-g++
make -j"$(nproc)"
```

Do not hard-code the project file until it has been verified.

For target builds, do not claim validation unless run on the correct target or toolchain.

---

# Phase 24: Required Final Report

At the end, produce:

```text
docs/CLAUDE-TEAM-MIGRATION-REPORT.md
```

Use the following structure:

# Claude Team Migration Report

## Executive Summary

## Verified Repository State

## Existing Local Agents

## Existing Local Skills

## Existing Local Commands

## Secret and Privacy Findings

## Selected Items for Migration

## Rejected Items and Reasons

## Files Created

## Files Modified

## Files Not Modified

## Git Ignore Changes

## Shared Configuration

## Machine-Specific Configuration

## Laptop Setup Procedure

## Team Member Setup Procedure

## Build Validation

## Tests Performed

## Tests Not Performed

## Remaining Risks

## Recommended Next Actions

---

# Final Response Requirements

After completing the work, respond with:

1. A concise summary.
2. Exact files created.
3. Exact files modified.
4. Local Agents discovered.
5. Local Skills discovered.
6. Which resources were migrated.
7. Which resources were intentionally not migrated.
8. Secret or privacy risks found, with values redacted.
9. Git status after changes.
10. Build and validation results.
11. Remaining unresolved decisions.
12. Commands for setting up the Laptop.
13. Commands for another team member to clone and verify the project.

Do not claim success for any step that was not actually executed.

---

# Expected Laptop Setup

After the changes are committed and pushed, another machine should be able to use:

```bash
git clone <repository-url>
cd <repository-directory>

./scripts/verify-dev-env.sh
./scripts/bootstrap-dev.sh
```

Then start Claude Code from the repository root:

```bash
claude
```

Claude should discover:

```text
CLAUDE.md
AGENTS.md
.claude/agents/
.claude/skills/
.claude/rules/
.claude/settings.json
```

Each user must authenticate Claude Code using their own account.

No account credentials or user-level Claude session files may be shared through Git.

---

# Implementation Approval Rule

First complete the inspection and write the proposed migration plan.

If the repository has:

* Large unexpected deletions
* Unknown untracked build files
* Conflicting branches
* Possible secrets
* A dirty working tree that could cause data loss
* An unclear authoritative branch

do not perform destructive cleanup.

Create the safe project-owned Claude files only where they do not risk overwriting existing work, and clearly report what still requires human approval.
