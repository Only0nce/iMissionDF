#!/usr/bin/env bash
set -u

project_root=$(git rev-parse --show-toplevel 2>/dev/null) || {
    echo "ERROR: run this script inside the project repository." >&2
    exit 1
}

output_path=${1:-"$project_root/claude-team-inventory.md"}
claude_root="${HOME}/.claude"
indicator='api[_-]?key|token|secret|password|passwd|authorization|bearer|private[_-]?key|BEGIN [A-Z ]*PRIVATE KEY|ssh-rsa|github_pat_|ghp_|sk-[A-Za-z0-9]'

umask 077

{
    echo "# Local Claude Inventory"
    echo
    echo "Generated: $(date -Iseconds)"
    echo
    echo "This report lists names and safe metadata only. Possible values are never printed."
    echo
    echo "## Tool Versions"
    echo
    printf -- "- OS: "
    if [ -r /etc/os-release ]; then
        ( . /etc/os-release; printf '%s %s\n' "$NAME" "$VERSION_ID" )
    else
        uname -s
    fi
    echo "- Architecture: $(uname -m)"
    echo "- Git: $(git --version 2>/dev/null || echo unavailable)"
    echo "- Claude: $(claude --version 2>/dev/null || echo unavailable)"
    echo "- qmake: $(qmake -v 2>&1 | tr '\n' ' ' || echo unavailable)"
    echo "- Compiler: $(g++ --version 2>/dev/null | sed -n '1p' || echo unavailable)"
    echo
    echo "## Repository"
    echo
    echo "- Root: \`$project_root\`"
    echo "- Status:"
    echo
    echo '```text'
    git -C "$project_root" status --short --branch 2>/dev/null || true
    echo '```'

    for resource_type in agents skills commands hooks; do
        echo
        echo "## Local ${resource_type^}"
        echo
        resource_dir="$claude_root/$resource_type"
        if [ ! -d "$resource_dir" ]; then
            echo
            echo "Not present."
            continue
        fi
        echo
        find "$resource_dir" -maxdepth 8 -type f -printf '%P\n' 2>/dev/null |
            sort |
            sed 's/^/- `~\/.claude\/'"$resource_type"'\//; s/$/`/'
    done

    echo
    echo "## Local Configuration Metadata"
    echo
    for local_path in \
        "$claude_root/settings.json" \
        "$claude_root/settings.local.json" \
        "${HOME}/.claude.json" \
        "$claude_root/.credentials.json" \
        "$claude_root/projects" \
        "$claude_root/history" \
        "$claude_root/transcripts" \
        "$claude_root/plugins"; do
        display=${local_path#"$HOME"/}
        if [ -e "$local_path" ]; then
            printf -- "- \`~/%s\`: exists (%s, mode %s)\n" \
                "$display" \
                "$(stat -c '%F' "$local_path" 2>/dev/null || echo unknown)" \
                "$(stat -c '%a' "$local_path" 2>/dev/null || echo unknown)"
        else
            printf -- "- \`~/%s\`: absent\n" "$display"
        fi
    done

    echo
    echo "## Possible Secret Indicators"
    echo
    echo "Indicator matches require manual review. Values are shown as \`[REDACTED]\`."
    echo
    found=0
    for resource_type in agents skills commands; do
        resource_dir="$claude_root/$resource_type"
        [ -d "$resource_dir" ] || continue
        while IFS= read -r resource_file; do
            if LC_ALL=C grep -IqiE "$indicator" "$resource_file" 2>/dev/null; then
                rel=${resource_file#"$claude_root"/}
                echo "- \`~/.claude/$rel\`: possible indicator; value \`[REDACTED]\`"
                found=1
            fi
        done < <(find "$resource_dir" -maxdepth 8 -type f -print 2>/dev/null | sort)
    done
    if [ "$found" -eq 0 ]; then
        echo "- No indicator matches in local agents, skills, or commands."
    fi
} >"$output_path" || {
    echo "ERROR: could not write inventory to $output_path" >&2
    exit 1
}

echo "Wrote redacted inventory: $output_path"
exit 0
