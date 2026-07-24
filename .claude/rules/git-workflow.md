# Git Workflow Rules

- Inspect status, branch, upstream, ahead/behind state, and worktrees before editing.
- Preserve staged, unstaged, deleted, untracked, and ignored user work.
- Use a scoped branch from the verified integration/development base.
- Use separate worktrees for parallel agents; never modify one worktree concurrently.
- Do not use `git reset --hard`, `git clean -fd`, `git checkout -- .`,
  `git restore .`, force push, or bulk staging without explicit approval.
- Stage only intended paths and review staged content before committing.
- Do not commit credentials, sessions, machine paths, IDE state, build outputs, or generated caches.
- Require independent review before merge; the implementer does not self-approve.
