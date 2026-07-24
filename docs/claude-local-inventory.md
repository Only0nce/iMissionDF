# Local Claude Inventory

## System

- OS: Ubuntu 20.04
- Architecture: x86_64
- Host type: physical or not detected as a virtual machine
- Claude Code: 2.1.218
- Git: 2.25.1
- Qt: 5.12.8
- qmake: 3.1 at `/usr/bin/qmake`, default mkspec `linux-g++`
- Compiler: GCC/G++ 9.4.0

## Repository

- Inspected root: `[LOCAL_PATH]/iMissionDF` (user-specific prefix redacted)
- Remote: `origin` (SSH; no credential embedded in the URL)
- Current inspected branch: `main`, one commit behind `origin/main`
- Remote default/production branch: `origin/main`
- Authoritative development branch: `origin/dev-full-hardwareversion`
- Feature branch on development: `origin/future/optimize_water_decress_ram_consumption`
- Original worktree: 1 modified tracked file, 0 deleted, 0 staged, 4 untracked
- Original untracked items: a validation report, an unrelated CMake reproduction,
  a full-repository archive, and the migration instructions
- Implementation worktree: separate branch `chore/claude-team-migration`

The original worktree was not safe for branch switching or broad modification.
Migration was therefore implemented in a separate worktree.

## Local Claude Resources

| Type | Local path | Classification | Recommended project path | Action |
| --- | --- | --- | --- | --- |
| Agent | `~/.claude/agents/fpga-rtl-engineer.md` | General reusable; safe after sanitization | `.claude/agents/hardware-safety-reviewer.md` | Distill hardware/contract checks |
| Agent | `~/.claude/agents/fullstack-system-engineer.md` | General reusable; possible-secret wording only | None | Do not migrate; scope is not project-specific |
| Agent | `~/.claude/agents/gpt-architect.md` | Machine-specific wrapper/skill dependency | None | Do not migrate |
| Agent | `~/.claude/agents/gpt-reviewer.md` | Machine-specific wrapper/skill dependency | None | Do not migrate |
| Agent | `~/.claude/agents/integration-reviewer.md` | General reusable; external skill dependencies | `.claude/agents/independent-reviewer.md` | Distill after removing dependencies |
| Agent | `~/.claude/agents/low-level-engineer.md` | General reusable; project-relevant | `.claude/agents/linux-reviewer.md` | Distill Linux and device review |
| Agent | `~/.claude/agents/system-architect.md` | General reusable; external skill dependencies | `.claude/agents/engineering-architect.md` | Distill read-only architecture role |
| Agent | `~/.claude/agents/qt5-levels/qt5-codex-reviewer.md` | Machine-specific wrapper; possible-secret wording only | None | Do not migrate wrapper |
| Agent | `~/.claude/agents/qt5-levels/qt5-explorer-haiku.md` | Project-relevant; model-specific | `.claude/agents/codebase-explorer.md` | Distill without model tier |
| Agent | `~/.claude/agents/qt5-levels/qt5-l1-economy.md` | Duplicate tiered implementer | None | Do not migrate |
| Agent | `~/.claude/agents/qt5-levels/qt5-l2-balanced.md` | Duplicate broad implementer | None | Do not migrate |
| Agent | `~/.claude/agents/qt5-levels/qt5-l3-deep.md` | Duplicate/model-specific implementer | None | Do not migrate |
| Agent | `~/.claude/agents/qt5-levels/qt5-l4-claude-only-opus.md` | Provider/model-specific fallback | None | Do not migrate |
| Agent | `~/.claude/agents/qt5-levels/qt5-l4-claude-only-sonnet.md` | Provider/model-specific fallback | None | Do not migrate |
| Agent | `~/.claude/agents/qt5-levels/qt5-l5-tool-first.md` | Reusable validation expertise | `.claude/agents/build-verifier.md` | Distill deterministic checks |
| Agent | `~/.claude/agents/qt5-levels/qt5-opus-reviewer.md` | Reusable Qt review; model-specific | `.claude/agents/qt5-cpp-reviewer.md` | Distill without model tier |
| Agent | `~/.claude/agents/qt5-levels/qt5-sonnet-verifier.md` | Duplicate/model-specific verifier | `.claude/agents/independent-reviewer.md` | Distill finding verification |
| Skill | `~/.claude/skills/develop-system/SKILL.md` | General reusable; external agents/skills | `.claude/skills/plan-change/SKILL.md` and related skills | Split into project workflows |
| Command | `~/.claude/commands/` | Absent | None | Nothing to migrate |
| Setting | `~/.claude/settings.json` | Personal plugins, marketplace, effort, theme | `.claude/settings.json` | Do not copy; create project-safe hooks only |

## Local-Only Data Not Migrated

- `~/.claude.json`
- `~/.claude/.credentials.json`
- `~/.claude/projects/`
- `~/.claude/plugins/`
- user plugin marketplace and theme/effort settings
- local `claude-mem` MCP process
- failed local `gpt-architect` MCP wrapper and its absolute path
- environment authentication values
- Git credential store and SSH private keys

## Secret and Privacy Indicators

- An Anthropic authentication environment variable is set; value `[REDACTED]`.
- Claude credential/account files exist; contents were not printed or copied.
- Git credential storage and SSH private keys exist; contents were not inspected.
- The untracked 119 MB archive contains `.git`, Qt Creator state, build/cache
  data, and machine paths. It was listed but not extracted or executed.
- The development source contains an embedded legacy network administrator
  password hash fallback; value `[REDACTED]`.
- Generic scans found password/token terminology in application code, mostly
  database fields and network handling. No strong token/private-key signature
  was found in the inspected development tree.

This inventory is sanitized for Git. It contains no credential values.
