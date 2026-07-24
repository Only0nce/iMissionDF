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

## Sensitive and Local Data

Never commit user Claude settings, credentials, sessions, histories, local MCP
configuration, `.env`, Qt Creator state, build caches, private keys, password
hashes, or tokens. Project settings belong in `.claude/settings.json`;
machine-only overrides belong in ignored `.claude/settings.local.json`.
