#!/usr/bin/env bash
# Static validation for the shared .claude/ team configuration.
#
# Read-only by default: never modifies files, never changes Git state, never
# installs dependencies, never touches hardware, never contacts external
# services. Building the full desktop app is opt-in via --with-desktop-build,
# and even then only compiles/links against tools already on this machine.
set -Eeuo pipefail

with_desktop_build=0
for arg in "$@"; do
    case "$arg" in
        --with-desktop-build) with_desktop_build=1 ;;
        *)
            echo "ERROR: unknown option: $arg" >&2
            echo "Usage: $0 [--with-desktop-build]" >&2
            exit 1
            ;;
    esac
done

project_root=$(git rev-parse --show-toplevel 2>/dev/null) || {
    echo "ERROR: run this script inside the project repository." >&2
    exit 1
}
cd "$project_root"

if ! command -v python3 >/dev/null 2>&1; then
    echo "ERROR: python3 is required to run this validation script." >&2
    exit 1
fi

overall_status=0

section() {
    echo ""
    echo "== $1 =="
}

pass_section() {
    echo "RESULT=PASS: $1"
}

fail_section() {
    overall_status=1
    echo "RESULT=FAIL: $1"
}

shopt -s nullglob

# 1. Bash syntax for every hook and script, plus Python syntax for the hook
#    policy modules the hooks delegate to.
section "Bash and Python syntax"
syntax_fail=0
sh_files=(.claude/hooks/*.sh scripts/*.sh)
for f in "${sh_files[@]}"; do
    if out=$(bash -n "$f" 2>&1); then
        echo "PASS  $f"
    else
        echo "FAIL  $f"
        echo "$out" | sed 's/^/      /'
        syntax_fail=1
    fi
done
py_files=(.claude/hooks/lib/*.py)
for f in "${py_files[@]}"; do
    if out=$(python3 -m py_compile "$f" 2>&1); then
        echo "PASS  $f"
    else
        echo "FAIL  $f"
        echo "$out" | sed 's/^/      /'
        syntax_fail=1
    fi
done
find . -name '__pycache__' -path '*/.claude/hooks/*' -prune -exec rm -rf {} + 2>/dev/null || true
if [ "$syntax_fail" -eq 0 ]; then pass_section "Bash and Python syntax"; else fail_section "Bash and Python syntax"; fi

# 2. JSON syntax for settings.json
section "JSON syntax (.claude/settings.json)"
if python3 -m json.tool .claude/settings.json >/dev/null 2>&1; then
    pass_section "JSON syntax"
else
    fail_section "JSON syntax"
fi

# 3. Agent inventory
section "Agent inventory (.claude/agents)"
agent_files=(.claude/agents/*.md)
if [ "${#agent_files[@]}" -gt 0 ]; then
    for f in "${agent_files[@]}"; do echo "  $f"; done
    pass_section "Agent inventory (${#agent_files[@]} found)"
else
    fail_section "Agent inventory (none found)"
fi

# 4. Skill inventory
section "Skill inventory (.claude/skills)"
skill_files=(.claude/skills/*/SKILL.md)
if [ "${#skill_files[@]}" -gt 0 ]; then
    for f in "${skill_files[@]}"; do echo "  $f"; done
    pass_section "Skill inventory (${#skill_files[@]} found)"
else
    fail_section "Skill inventory (none found)"
fi

# 5. Executable-bit checks
section "Executable bits"
exec_fail=0
exec_targets=(.claude/hooks/*.sh .claude/hooks/lib/*.py scripts/*.sh)
for f in "${exec_targets[@]}"; do
    if [ -x "$f" ]; then
        echo "PASS  $f"
    else
        echo "FAIL  $f (not executable)"
        exec_fail=1
    fi
done
if [ "$exec_fail" -eq 0 ]; then pass_section "Executable bits"; else fail_section "Executable bits"; fi

# 6. Automated hook test suite
section "Hook test suite (scripts/test-claude-hooks.sh)"
if ./scripts/test-claude-hooks.sh; then
    pass_section "Hook test suite"
else
    fail_section "Hook test suite"
fi

# 7. git diff --check (whitespace/conflict-marker errors in the current diff)
section "git diff --check"
diff_check_out=$(git diff --check 2>&1) && diff_check_rc=0 || diff_check_rc=$?
if [ -n "$diff_check_out" ]; then
    echo "$diff_check_out"
fi
if [ "$diff_check_rc" -eq 0 ]; then
    pass_section "git diff --check"
else
    fail_section "git diff --check"
fi

# 8. Accidentally tracked sensitive Claude files
section "Sensitive tracked-file scan"
sensitive_hit=0
while IFS= read -r tracked; do
    case "$tracked" in
        .env.example|.env.sample|.env.template|.env.dist)
            # Safe placeholder files with no real values - not a leak.
            ;;
        .claude/settings.local.json|.claude/.credentials.json| \
        .claude/history/*|.claude/transcripts/*|.claude/projects/*| \
        *.pem|*.key|*id_rsa|*id_ed25519|.env|.env.*)
            echo "FAIL  tracked sensitive file: $tracked"
            sensitive_hit=1
            ;;
    esac
done < <(git ls-files)
if [ "$sensitive_hit" -eq 0 ]; then
    pass_section "Sensitive tracked-file scan"
else
    fail_section "Sensitive tracked-file scan"
fi

# 9. Machine-specific absolute paths in files meant to be shared as-is
section "Machine-specific absolute paths in shared config"
abs_path_pattern='/home/[A-Za-z0-9_.-]+|/Users/[A-Za-z0-9_.-]+'
abs_path_hit=0
shared_targets=()
while IFS= read -r -d '' f; do shared_targets+=("$f"); done \
    < <(find .claude/agents .claude/skills .claude/rules -type f -print0 2>/dev/null)
for f in CLAUDE.md CONTRIBUTING.md; do
    [ -f "$f" ] && shared_targets+=("$f")
done
while IFS= read -r -d '' f; do shared_targets+=("$f"); done \
    < <(find docs -maxdepth 1 -type f -name '*.md' -print0 2>/dev/null)
for f in "${shared_targets[@]}"; do
    hits=$(grep -nEI "$abs_path_pattern" "$f" 2>/dev/null || true)
    if [ -n "$hits" ]; then
        echo "FAIL  $f"
        echo "$hits" | sed 's/^/      /'
        abs_path_hit=1
    fi
done
if [ "$abs_path_hit" -eq 0 ]; then
    pass_section "Machine-specific absolute paths"
else
    fail_section "Machine-specific absolute paths"
fi

# 10. Required files/directories present
section "Required files present"
required_fail=0
required_files=(
    CLAUDE.md
    CONTRIBUTING.md
    .claude/settings.json
    docs/CLAUDE-TEAM-MIGRATION-REPORT.md
    docs/AI-WORKFLOW.md
    docs/DEVELOPMENT.md
    docs/HARDWARE-SAFETY.md
    .claude/hooks/block-destructive-git.sh
    .claude/hooks/check-generated-files.sh
    .claude/hooks/check-secrets.sh
    .claude/hooks/lib/destructive_git_policy.py
    .claude/hooks/lib/generated_file_policy.py
    scripts/test-claude-hooks.sh
    scripts/validate-claude-team-config.sh
)
for f in "${required_files[@]}"; do
    if [ -f "$f" ]; then
        echo "PASS  $f"
    else
        echo "FAIL  $f (missing)"
        required_fail=1
    fi
done
for d_desc in ".claude/agents:*.md" ".claude/rules:*.md" ".claude/skills:*/SKILL.md"; do
    d="${d_desc%%:*}"
    pattern="${d_desc#*:}"
    matches=("$d"/$pattern)
    if [ "${#matches[@]}" -gt 0 ] && [ -e "${matches[0]}" ]; then
        echo "PASS  $d ($pattern present)"
    else
        echo "FAIL  $d ($pattern missing)"
        required_fail=1
    fi
done
if [ "$required_fail" -eq 0 ]; then pass_section "Required files present"; else fail_section "Required files present"; fi

# 11. Optional desktop build validation (never installs dependencies)
if [ "$with_desktop_build" -eq 1 ]; then
    section "Desktop build (--with-desktop-build)"
    if command -v qmake >/dev/null 2>&1 && command -v g++ >/dev/null 2>&1 && command -v make >/dev/null 2>&1; then
        build_out=$(./scripts/build-desktop.sh 2>&1) && build_rc=0 || build_rc=$?
        echo "$build_out"
        if [ "$build_rc" -eq 0 ]; then
            echo "QMAKE_CONFIGURATION=PASS"
            echo "CPP_COMPILE=PASS"
            echo "LINK=PASS"
            pass_section "Desktop build"
        elif printf '%s' "$build_out" | grep -qE 'Unknown module|was not found in the pkg-config search path|Project ERROR:.*(module|package)'; then
            echo "QMAKE_CONFIGURATION=FAIL_DEPENDENCY"
            echo "CPP_COMPILE=NOT_RUN"
            echo "LINK=NOT_RUN"
            echo "Missing Qt module or pkg-config package (see output above). Not installing automatically."
            echo "Run ./scripts/verify-dev-env.sh for exact missing-dependency detail."
            fail_section "Desktop build (dependency unavailable)"
        else
            echo "QMAKE_CONFIGURATION=FAIL"
            echo "CPP_COMPILE=NOT_RUN"
            echo "LINK=NOT_RUN"
            fail_section "Desktop build"
        fi
    else
        echo "QMAKE_CONFIGURATION=FAIL_DEPENDENCY"
        echo "CPP_COMPILE=NOT_RUN"
        echo "LINK=NOT_RUN"
        echo "Missing one or more of: qmake, g++, make. Not installing automatically."
        echo "Run ./scripts/verify-dev-env.sh for exact missing-dependency detail."
        fail_section "Desktop build (dependency unavailable)"
    fi
else
    section "Desktop build"
    echo "Skipped (pass --with-desktop-build to attempt it)."
    echo "QMAKE_CONFIGURATION=NOT_RUN"
    echo "CPP_COMPILE=NOT_RUN"
    echo "LINK=NOT_RUN"
fi

echo ""
if [ "$overall_status" -eq 0 ]; then
    echo "CLAUDE_CONFIG_VALIDATION=PASS"
    exit 0
else
    echo "CLAUDE_CONFIG_VALIDATION=FAIL"
    exit 1
fi
