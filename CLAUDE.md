# Project Instructions

@AGENTS.md

## Build System

- The authoritative build system is qmake.
- The main project file is `iScanMR10.pro`; do not treat ad-hoc CMake files as authoritative.
- Desktop builds use `linux-g++`. Jetson/Orin builds require the project target mkspec and target toolchain.
- The active hardware selector is independent of the build platform; inspect `iScanMR10.pro` before building.

## Engineering Rules

- Inspect the repository, current branch, local changes, and affected code paths before editing.
- Preserve existing C++, QML, JSON, network, database, and hardware interfaces unless the task explicitly changes them.
- Do not edit generated files, build outputs, Qt Creator state, or SigmaStudio exports unless the task explicitly owns them.
- Do not run commands that change GPIO, RF, TX, DMA, SPI/I2C state, power, modem state, or network configuration without approval.
- Separate static review, compilation, tests, simulation, target execution, and hardware validation.
- Never claim a build or test passed unless the exact command was run successfully.
- Report exact files changed, commands run, results, and validation that remains.

## Git Rules

- Do not commit directly to protected production or integration branches.
- Do not use destructive Git commands or overwrite another developer's work.
- Use a feature, fix, refactor, or chore branch and a separate worktree for parallel agents.
- Stage only intended paths; do not use `git add -A` by default.
- Require independent review before merge.

## Project Skills

- `/learn-codebase`
- `/check-project-status`
- `/plan-change`
- `/debug-build`
- `/review-diff`
- `/verify-qt5`
- `/hardware-change-review`
- `/create-handoff`

Detailed workflow and safety guidance lives in `docs/AI-WORKFLOW.md`,
`docs/DEVELOPMENT.md`, and `docs/HARDWARE-SAFETY.md`.
