#!/usr/bin/env python3
"""Detect destructive Git operations inside a Claude Bash tool_input.command string.

Reads a Claude PreToolUse hook JSON payload from stdin. Never executes the
inspected command - this module only tokenizes and inspects the command text.

Exit codes:
  0 - command is allowed
  2 - command is blocked by policy (or the payload could not be parsed)
"""
import json
import os
import re
import shlex
import sys

ENV_ASSIGN_RE = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*=")

# Commands that wrap or precede the real invocation without changing its
# effect (env var assignments handled separately via ENV_ASSIGN_RE).
WRAPPER_COMMANDS = {"env", "command", "nice", "nohup", "time", "exec", "sudo"}

SEPARATORS = {"&&", "||", ";", "|", "&"}

# Git global options that consume the following token as a separate value,
# e.g. `git -C /path reset --hard`. Options using an attached `=form` are
# handled generically (any `--opt=value` token is skipped as a whole).
GLOBAL_OPTS_WITH_VALUE = {
    "-C", "--git-dir", "--work-tree", "-c",
    "--namespace", "--super-prefix", "--exec-path", "--attr-source",
}

RESET_BLOCKED_MODES = {"--hard", "--merge", "--keep"}


def read_command_text():
    try:
        payload = json.load(sys.stdin)
    except (json.JSONDecodeError, ValueError):
        print("Blocked: could not parse the Claude Bash hook payload.", file=sys.stderr)
        sys.exit(2)
    return payload.get("tool_input", {}).get("command", "") or ""


def tokenize(command_text):
    lexer = shlex.shlex(command_text, posix=True, punctuation_chars=True)
    lexer.whitespace_split = True
    try:
        return list(lexer)
    except ValueError:
        # Unbalanced quotes etc: fail closed by treating the raw text as a
        # single opaque segment rather than raising past the hook boundary.
        return command_text.split()


def split_segments(tokens):
    segments = []
    current = []
    for tok in tokens:
        if tok in SEPARATORS:
            if current:
                segments.append(current)
            current = []
        else:
            current.append(tok)
    if current:
        segments.append(current)
    return segments


def strip_wrappers(tokens):
    """Skip leading env-var assignments and passthrough wrapper commands,
    e.g. `env LANG=C command sudo git ...` -> `git ...`."""
    i, n = 0, len(tokens)
    while i < n:
        tok = tokens[i]
        if ENV_ASSIGN_RE.match(tok):
            i += 1
            continue
        if tok in WRAPPER_COMMANDS:
            i += 1
            while i < n and tokens[i].startswith("-"):
                i += 1
            continue
        break
    return tokens[i:]


def find_subcommand(args):
    """Walk past Git global options to find the actual subcommand token."""
    i, n = 0, len(args)
    while i < n:
        tok = args[i]
        if tok == "--":
            i += 1
            continue
        if tok.startswith("-"):
            if "=" in tok:
                i += 1
                continue
            if tok in GLOBAL_OPTS_WITH_VALUE:
                i += 2
                continue
            i += 1
            continue
        return tok, args[i + 1:]
    return None, []


def check_policy(subcmd, rest):
    if subcmd == "reset":
        for tok in rest:
            if tok in RESET_BLOCKED_MODES:
                return f"'git reset {tok}' can discard uncommitted work"
        return None

    if subcmd == "clean":
        for tok in rest:
            if tok == "--force":
                return "'git clean --force' permanently deletes untracked files"
            if tok.startswith("-") and not tok.startswith("--") and "f" in tok[1:]:
                return f"'git clean {tok}' permanently deletes untracked files"
        return None

    if subcmd == "checkout":
        if "--" in rest:
            return "'git checkout -- <pathspec>' discards working-tree changes"
        return None

    if subcmd == "restore":
        non_help = [t for t in rest if t not in ("-h", "--help")]
        if non_help:
            return "'git restore' overwrites working-tree/index contents"
        return None

    if subcmd == "push":
        for tok in rest:
            if tok in ("--force", "-f") or tok.startswith("--force-with-lease"):
                return f"'git push {tok}' rewrites remote history"
        return None

    if subcmd == "add":
        for tok in rest:
            if tok in ("-A", "--all"):
                return f"'git add {tok}' bulk-stages the entire working tree"
            if tok in (".", "./", ":/"):
                return f"'git add {tok}' bulk-stages the entire working tree"
        return None

    return None


def classify_segment(tokens):
    tokens = strip_wrappers(tokens)
    if not tokens:
        return None
    if os.path.basename(tokens[0]) != "git":
        return None
    subcmd, rest = find_subcommand(tokens[1:])
    if subcmd is None:
        return None
    return check_policy(subcmd, rest)


def split_lines(command_text):
    """Split into logical shell lines, honoring backslash-newline
    continuation (bash joins those into a single line)."""
    joined = command_text.replace("\\\n", " ")
    return joined.split("\n")


def main():
    command_text = read_command_text()
    if not command_text.strip():
        sys.exit(0)

    # A raw newline separates statements exactly like `;` does, but shlex's
    # whitespace_split mode swallows it silently as ordinary whitespace, so
    # lines are split explicitly before tokenizing each one.
    for line in split_lines(command_text):
        if not line.strip():
            continue
        tokens = tokenize(line)
        for segment in split_segments(tokens):
            reason = classify_segment(segment)
            if reason:
                print(f"Blocked by project policy: {reason}.", file=sys.stderr)
                sys.exit(2)

    sys.exit(0)


if __name__ == "__main__":
    main()
