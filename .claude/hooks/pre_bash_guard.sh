#!/usr/bin/env bash
# =============================================================================
# pre_bash_guard.sh — PreToolUse safety gate for kernel development context
# =============================================================================

set -euo pipefail

# Read the full JSON payload from stdin.
INPUT=$(cat)

COMMAND=$(echo "$INPUT" | python3 -c "
import sys, json
try:
    data = json.load(sys.stdin)
    print(data.get('tool_input', {}).get('command', ''))
except Exception:
    print('')
" 2>/dev/null || echo "")

# If we couldn't extract a command, deny by default.
if [[ -z "$COMMAND" ]]; then
    exit 2
fi

# ─── BLOCK PATTERNS ────────────────────────────────────────────────────────
# Each pattern is a regex tested against the full command string.
declare -A BLOCKED_PATTERNS=(
    ["rm -rf /"]="STOP: attempted to delete root filesystem."
    ["rm -rf ~"]="STOP: attempted to delete home directory."
    ["dd if=.*of=/dev/[a-z]"]="STOP: dd write to block device detected. This would corrupt disk."
    ["sudo chmod 777 /"]="STOP: attempted to chmod root filesystem world-writable."
    ["rmmod mock_nic.*-f"]="WARNING: force-unloading mock_nic while device may be open can kernel-panic. Use rmmod without -f."
    ["write.*\/lib\/modules"]="STOP: attempted to write directly to /lib/modules. Use kbuild only."
    ["curl.*\|.*bash"]="STOP: pipe-to-bash download pattern detected. Explicitly review before running."
    ["wget.*\|.*sh"]="STOP: pipe-to-sh download pattern detected. Explicitly review before running."
)

for PATTERN in "${!BLOCKED_PATTERNS[@]}"; do
    MESSAGE="${BLOCKED_PATTERNS[$PATTERN]}"
    if echo "$COMMAND" | grep -qE "$PATTERN"; then
        # Exit code 2 tells Claude Code to block the tool call and surface
        # the reason to the developer.
        echo "$MESSAGE" >&2
        echo "Blocked command was: $COMMAND" >&2
        exit 2
    fi
done


# All checks passed — allow the command.
exit 0
