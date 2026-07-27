# AI-Assisted Development Workflow

## Branch Roles

```text
main
  Production-ready code only

dev-full-hardwareversion
  Verified current development/integration branch

feature/<issue>-<description>
fix/<issue>-<description>
refactor/<issue>-<description>
chore/<description>
```

The remote default branch is `main`. During the migration inspection,
`origin/dev-full-hardwareversion` was the integration line and
`origin/future/optimize_water_decress_ram_consumption` was a feature branch two
commits on top of it. Re-verify branch roles before starting future work; names
alone are not authority.

## Standard Change Flow

```bash
git fetch --all --prune
git switch dev-full-hardwareversion
git pull --ff-only
git switch -c feature/<issue>-<description>
```

Read `CLAUDE.md`, `AGENTS.md`, and relevant source. Use:

1. Codebase Explorer — read-only mapping.
2. Engineering Architect — root-cause analysis and implementation plan.
3. Implementation Agent — scoped modification.
4. Build Verifier — compile and deterministic validation.
5. Independent Reviewer — actual-diff and regression review.
6. Human Reviewer — pull-request approval or rejection.

The implementation agent must not approve its own work.

## Before Commit

```bash
./scripts/verify-dev-env.sh
git status --short
git diff --check
git diff --stat
```

Run the relevant build and tests. Stage exact paths:

```bash
git add path/to/file1 path/to/file2
git diff --cached --check
git diff --cached --stat
git commit -m "type(scope): description"
git push -u origin HEAD
```

Open a pull request and require independent review before merge.

## Parallel Agents and Worktrees

Agents must not edit the same working tree concurrently. Give each agent:

- its own branch;
- its own worktree;
- a precise file/scope boundary;
- a validation command;
- a required handoff format.

Example:

```bash
git worktree add ../iMissionDF-audio -b fix/audio-stutter dev-full-hardwareversion
git worktree add ../iMissionDF-ui -b feature/ui-improvement dev-full-hardwareversion
```

Do not share uncommitted changes between worktrees. Integrate through commits
and reviewed merges/cherry-picks.

## Evidence and Claims

Always separate:

- static inspection;
- qmake configuration;
- desktop compilation;
- automated tests;
- runtime/QML checks;
- target compilation;
- target execution;
- physical hardware validation.

One layer is not proof of another. The handoff must state what was not run.

## Safety Hooks

`.claude/settings.json` wires three `PreToolUse`/`PostToolUse` hooks. **Hooks
are guardrails, not a security boundary.** They classify command/file text
before a tool runs; they do not sandbox the shell, and a sufficiently indirect
command (e.g. hidden inside `$(...)` command substitution, a shell function,
or a script the agent invokes) can still fall outside what text inspection
catches. Treat them as a safety net that catches the common accidental and
scripted cases, not as authorization to skip review of risky changes.

### `block-destructive-git.sh` (PreToolUse: Bash)

Tokenizes the command (via `.claude/hooks/lib/destructive_git_policy.py`,
using Python's `shlex`), splits on raw newlines (honoring `\`-newline
continuation the way bash does) and shell separators (`&&`, `||`, `;`, `|`,
`&`) within each line, strips wrapper commands (`env`, `command`, `sudo`, `nice`, `nohup`,
`time`, `exec`) and leading `VAR=value` assignments, then walks past Git
global options (`-C`, `--git-dir`, `--work-tree`, `-c`, etc.) to find the real
subcommand — so `git -C /tmp/x reset --hard`, `/usr/bin/git reset --hard`, and
`env LANG=C git reset --hard` are all recognized as the same operation a bare
`git reset --hard` is.

Blocked:

- `git reset --hard|--merge|--keep` (can discard uncommitted work)
- `git clean` with any force flag (`-f`, `-fd`, `-ffdx`, `--force`, ...)
- `git checkout -- <pathspec>` (discards working-tree changes to those paths)
- `git restore` with any argument other than `-h`/`--help` (restore always
  overwrites working-tree/index content from another source)
- `git push --force`, `-f`, or `--force-with-lease[=...]`
- `git add -A`, `--all`, `.`, `./`, or `:/` (bulk staging)

Any of the above still blocks the whole command if it appears in *any*
segment of a chained command (`echo ok && git reset --hard HEAD` is blocked
because of the second segment).

**Explicit file staging remains allowed** — `git add CLAUDE.md`,
`git add .claude/hooks/block-destructive-git.sh`, etc. are never blocked; only
the bulk forms above are. Plain inspection/read commands
(`git status`, `git diff`, `git log`, `git show`, `git branch -vv`,
`git fetch`, `--help` variants, `--dry-run`) are also unaffected — the goal is
accurate policy enforcement, not blocking Git entirely.

### `check-generated-files.sh` (PreToolUse: Edit|Write)

Normalizes the target path with `os.path.abspath(os.path.normpath(path))`
(via `.claude/hooks/lib/generated_file_policy.py`) before matching, so
`Makefile`, `./Makefile`, and `/repo/Makefile` are all recognized the same
way — a bare relative name is not a bypass. Blocks generated/machine-local
paths: build output directories (`build/`, `out/`, `bin/`, `obj/`, `Build/`),
qmake/Qt Creator artifacts (`Makefile`, `.qmake.stash`, `.qtc_clangd/`,
`*.pro.user*`), and moc/rcc/uic output (`moc_*`, `qrc_*`, `ui_*`). This
project's policy protects *any* qmake-generated `Makefile` path, not just one
specific location, because a stray committed `Makefile` breaks reproducible
builds across machines.

### `check-secrets.sh` (PostToolUse: Edit|Write)

Scans the file just written for strong secret indicators (private-key
headers, GitHub/AWS/Google/Slack/OpenAI-shaped tokens, and
`password|passwd|secret ... = <64-hex-char>` assignments) and reports only
the file path and category — it never prints the matched value.

### Verifying hook behavior

```bash
./scripts/test-claude-hooks.sh          # adversarial regression suite (no destructive commands ever run)
./scripts/validate-claude-team-config.sh # syntax, inventory, executable bits, the test suite above, git diff --check, tracked-secret scan
```

Both are safe to run from a dirty working tree and modify nothing.

## Sensitive and Local Data

Never commit user Claude settings, credentials, sessions, histories, local MCP
configuration, `.env`, Qt Creator state, build caches, private keys, password
hashes, or tokens. Project settings belong in `.claude/settings.json`;
machine-only overrides belong in ignored `.claude/settings.local.json`.

The tracked `.claude/` directory (agents, skills, rules, hooks, settings) is
intentionally repository-owned and is meant to travel through Git to every
laptop and coworker machine that clones the repo — that is the point of the
migration. Machine-local Claude state (`~/.claude/settings.local.json`,
credentials, projects, history, transcripts, plugins) must never be copied
into the tracked directory; each developer authenticates and configures those
independently.
