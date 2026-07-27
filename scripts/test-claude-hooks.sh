#!/usr/bin/env bash
# Adversarial regression suite for .claude/hooks/*.sh.
#
# This never executes a real destructive Git command: it only feeds
# synthetic Claude PreToolUse/PostToolUse JSON payloads to the hook scripts
# and checks their exit code. Safe to run from any directory, on any branch,
# with a dirty working tree.
set -Eeuo pipefail

if ! command -v python3 >/dev/null 2>&1; then
    echo "ERROR: python3 is required to run this test suite." >&2
    exit 1
fi

project_root=$(git rev-parse --show-toplevel 2>/dev/null) || {
    echo "ERROR: run this script inside the project repository." >&2
    exit 1
}

hooks_dir="$project_root/.claude/hooks"
destructive_git_hook="$hooks_dir/block-destructive-git.sh"
generated_file_hook="$hooks_dir/check-generated-files.sh"
secrets_hook="$hooks_dir/check-secrets.sh"

for hook in "$destructive_git_hook" "$generated_file_hook" "$secrets_hook"; do
    [ -x "$hook" ] || {
        echo "ERROR: expected hook not found or not executable: $hook" >&2
        exit 1
    }
done

work_dir=$(mktemp -d "${TMPDIR:-/tmp}/claude-hook-tests.XXXXXX")
cleanup() {
    if [ -n "${work_dir:-}" ] && [ -d "$work_dir" ]; then
        rm -rf -- "$work_dir"
    fi
}
trap cleanup EXIT

total=0
pass=0
fail=0

report() {
    local status="$1" label="$2"
    total=$((total + 1))
    if [ "$status" = "PASS" ]; then
        pass=$((pass + 1))
    else
        fail=$((fail + 1))
    fi
    printf '[%s] %s\n' "$status" "$label"
}

run_command_hook() {
    # args: hook command_text
    python3 -c 'import json,sys; print(json.dumps({"tool_input": {"command": sys.argv[1]}}))' "$2" \
        | "$1" >/dev/null 2>&1
}

run_path_hook() {
    # args: hook file_path
    python3 -c 'import json,sys; print(json.dumps({"tool_input": {"file_path": sys.argv[1]}}))' "$2" \
        | "$1" >/dev/null 2>&1
}

expect_allowed_command() {
    local cmd="$1"
    if run_command_hook "$destructive_git_hook" "$cmd"; then
        report "PASS" "destructive-git allow: $cmd"
    else
        report "FAIL" "destructive-git allow: $cmd (was blocked, expected allowed)"
    fi
}

expect_blocked_command() {
    local cmd="$1"
    if run_command_hook "$destructive_git_hook" "$cmd"; then
        report "FAIL" "destructive-git block: $cmd (was allowed, expected blocked)"
    else
        report "PASS" "destructive-git block: $cmd"
    fi
}

expect_allowed_path() {
    local p="$1"
    if run_path_hook "$generated_file_hook" "$p"; then
        report "PASS" "generated-file allow: $p"
    else
        report "FAIL" "generated-file allow: $p (was blocked, expected allowed)"
    fi
}

expect_blocked_path() {
    local p="$1"
    if run_path_hook "$generated_file_hook" "$p"; then
        report "FAIL" "generated-file block: $p (was allowed, expected blocked)"
    else
        report "PASS" "generated-file block: $p"
    fi
}

expect_allowed_file_content() {
    local label="$1" file="$2"
    if run_path_hook "$secrets_hook" "$file"; then
        report "PASS" "secrets allow: $label"
    else
        report "FAIL" "secrets allow: $label (was blocked, expected allowed)"
    fi
}

expect_blocked_file_content() {
    local label="$1" file="$2"
    if run_path_hook "$secrets_hook" "$file"; then
        report "FAIL" "secrets block: $label (was allowed, expected blocked)"
    else
        report "PASS" "secrets block: $label"
    fi
}

echo "== destructive-git hook: expected ALLOWED =="
expect_allowed_command 'git status --short'
expect_allowed_command 'git diff --stat'
expect_allowed_command 'git log --oneline'
expect_allowed_command 'git add CLAUDE.md'
expect_allowed_command 'git add .claude/hooks/block-destructive-git.sh'
expect_allowed_command 'git push --dry-run'
expect_allowed_command 'git checkout --help'
expect_allowed_command 'git restore --help'
expect_allowed_command 'git commit -m "line one" \
  --no-verify'

echo ""
echo "== destructive-git hook: expected BLOCKED =="
expect_blocked_command 'git reset --hard HEAD'
expect_blocked_command 'git status
git reset --hard HEAD'
expect_blocked_command 'git reset \
  --hard HEAD'
expect_blocked_command 'git checkout -- \
  .'
expect_blocked_command '/usr/bin/git reset --hard HEAD'
expect_blocked_command 'command git reset --hard HEAD'
expect_blocked_command 'env git reset --hard HEAD'
expect_blocked_command 'env LANG=C git reset --hard HEAD'
expect_blocked_command 'git -C /tmp/example reset --hard HEAD'
expect_blocked_command 'git --git-dir=.git reset --hard HEAD'
expect_blocked_command 'git clean -fd'
expect_blocked_command 'git -C /tmp/example clean -fd'
expect_blocked_command 'git checkout -- .'
expect_blocked_command 'git restore .'
expect_blocked_command 'git restore --source=HEAD .'
expect_blocked_command 'git restore --worktree .'
expect_blocked_command 'git push --force'
expect_blocked_command 'git push -f'
expect_blocked_command 'git push --force-with-lease'
expect_blocked_command 'git add -A'
expect_blocked_command 'git add --all'
expect_blocked_command 'git add .'
expect_blocked_command 'git add :/'
expect_blocked_command 'echo safe && git reset --hard HEAD'
expect_blocked_command 'echo safe ; git clean -fd'
expect_blocked_command 'false || git add --all'

echo ""
echo "== generated-file hook: expected BLOCKED =="
expect_blocked_path 'Makefile'
expect_blocked_path './Makefile'
expect_blocked_path '/path/to/project/Makefile'
expect_blocked_path '.qmake.stash'
expect_blocked_path './.qmake.stash'
expect_blocked_path 'test.pro.user'
expect_blocked_path '/path/test.pro.user'
expect_blocked_path '.qtc_clangd/cache/file'
expect_blocked_path './.qtc_clangd/cache/file'
expect_blocked_path 'build/generated.cpp'
expect_blocked_path './build/generated.cpp'
expect_blocked_path '/path/to/project/build/generated.cpp'
expect_blocked_path 'moc_example.cpp'
expect_blocked_path 'qrc_qml.cpp'
expect_blocked_path 'ui_MainWindow.h'

echo ""
echo "== generated-file hook: expected ALLOWED =="
expect_allowed_path 'Mainwindows.cpp'
expect_allowed_path 'qml/main.qml'
expect_allowed_path 'iScanMR10.pro'
expect_allowed_path 'docs/DEVELOPMENT.md'

echo ""
echo "== secret hook: synthetic fixtures =="
normal_cpp="$work_dir/normal.cpp"
printf 'int main() { return 0; }\n' > "$normal_cpp"

normal_doc="$work_dir/normal.md"
printf 'This document describes the password reset flow.\n' > "$normal_doc"

fake_key="$work_dir/fake_key.pem"
printf -- '-----BEGIN RSA PRIVATE KEY-----\nFAKEFAKEFAKEFAKEFAKEFAKEFAKEFAKEFAKEFAKE\n-----END RSA PRIVATE KEY-----\n' > "$fake_key"

fake_token="$work_dir/fake_token.txt"
printf 'token = "ghp_FAKEFAKEFAKEFAKEFAKEFAKEFAKEFAKEFAKE"\n' > "$fake_token"

fake_api_key="$work_dir/fake_api.txt"
printf 'apiKey: "AIzaSyFAKEFAKEFAKEFAKEFAKEFAKEFAKEFAKEFA"\n' > "$fake_api_key"

fake_hash="$work_dir/fake_hash.txt"
hex64=$(printf '0123456789abcdef%.0s' 1 2 3 4)
printf 'password_hash = "%s"\n' "$hex64" > "$fake_hash"

expect_allowed_file_content "normal C++ source" "$normal_cpp"
expect_allowed_file_content "normal documentation" "$normal_doc"
expect_blocked_file_content "fake private-key header" "$fake_key"
expect_blocked_file_content "fake GitHub-token-shaped string" "$fake_token"
expect_blocked_file_content "fake API-key-shaped string" "$fake_api_key"
expect_blocked_file_content "fake 64-char password-hash assignment" "$fake_hash"

echo ""
echo "== secret hook: no value leakage =="
leak_found="no"
for f in "$fake_key" "$fake_token" "$fake_api_key" "$fake_hash"; do
    out=$(python3 -c 'import json,sys; print(json.dumps({"tool_input": {"file_path": sys.argv[1]}}))' "$f" \
        | "$secrets_hook" 2>&1 || true)
    case "$out" in
        *FAKEFAKE*|*AIzaSy*|*"$hex64"*)
            leak_found="yes"
            ;;
    esac
done
if [ "$leak_found" = "no" ]; then
    report "PASS" "secrets hook never echoes matched secret value"
else
    report "FAIL" "secrets hook printed a matched secret value"
fi

echo ""
echo "TOTAL=$total"
echo "PASS=$pass"
echo "FAIL=$fail"
if [ "$fail" -eq 0 ]; then
    echo "VALIDATION_RESULT=PASS"
    exit 0
else
    echo "VALIDATION_RESULT=FAIL"
    exit 1
fi
