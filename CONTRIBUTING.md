# Contributing

Start every change by reading `CLAUDE.md`, `AGENTS.md`, and the relevant source.
Use the verified development branch as the base and create a scoped branch:

```bash
git fetch --all --prune
git switch dev-full-hardwareversion
git pull --ff-only
git switch -c feature/<issue>-<description>
```

Use `fix/`, `refactor/`, or `chore/` when those better describe the change.
Do not work directly on production or integration branches.

Before committing:

```bash
./scripts/verify-dev-env.sh
git status --short
git diff --check
git diff --stat
```

Run the relevant build and tests. Hardware-facing work also requires the review
procedure in `docs/HARDWARE-SAFETY.md`.

Stage only intended files:

```bash
git add path/to/file1 path/to/file2
git commit -m "type(scope): description"
git push -u origin HEAD
```

Open a pull request and obtain independent review. The implementation author
must not be the only reviewer. Parallel agents or developers must use separate
branches and worktrees; never share uncommitted changes between them.
