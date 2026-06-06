#!/usr/bin/env bash
# =============================================================================
# post_session_check.sh — Post-session kernel state audit
# =============================================================================
#
# This hook runs asynchronously at the Stop event -
# meaning Claude finishes responding and then this runs in the background
# without blocking the next prompt.
#
# The specific problem it helps against is the "orphaned kernel module"
# scenario. When we call insmod mock_nic.ko, the kernel module:
#
#   1. Calls kmalloc() and claims a physical memory page.
#   2. Registers /dev/mock_nic as a char device.
#
# If the userspace demo process crashes or is killed (Ctrl+C, SIGKILL,
# whatever), the kernel module stays loaded. The physical page it owns
# stays pinned - it cannot be reclaimed by the kernel's page allocator.
# This is a memory leak at the OS level.
#
# The second thing this hook does is emit a summary of the session's
# audit log -the running trace written by pre_bash_guard.sh.
#
# =============================================================================

set -uo pipefail

AUDIT_LOG="/tmp/rocev2_claude_audit.log"
SEPARATOR="═══════════════════════════════════════════"

echo ""
echo "$SEPARATOR"
echo "  RoCEv2 Claude Code — Session End Report"
echo "$SEPARATOR"

# ─── Kernel module state check ─────────────────────────────────────────────

echo ""
echo "▶ Kernel module status:"

if lsmod 2>/dev/null | grep -q "mock_nic"; then
    echo "  ⚠️  WARNING: mock_nic module is still loaded."
    echo "  The physical memory page it owns is pinned and cannot be"
    echo "  reclaimed. Run: sudo rmmod mock_nic"
    echo ""

    # Check if any process has /dev/mock_nic open. If so, rmmod will fail
    # anyway - give more specific guidance.
    if command -v fuser &>/dev/null; then
        USERS=$(fuser /dev/mock_nic 2>/dev/null || true)
        if [[ -n "$USERS" ]]; then
            echo "  Additionally: /dev/mock_nic is held open by PID(s): $USERS"
            echo "  Kill the demo process first, then rmmod."
        fi
    fi
else
    echo "  ✓ mock_nic module is not loaded. Clean state."
fi

# ─── Device file check ─────────────────────────────────────────────────────

echo ""
echo "▶ Device file status:"

if [[ -e /dev/mock_nic ]]; then
    echo "  ⚠️  /dev/mock_nic exists. If mock_nic module is not loaded, this"
    echo "  is a stale device node. Remove with: sudo rm /dev/mock_nic"
else
    echo "  ✓ /dev/mock_nic does not exist. Clean state."
fi

# ─── Physical memory summary ───────────────────────────────────────────────

echo ""
echo "▶ Physical memory (quick view):"
if [[ -r /proc/meminfo ]]; then
    # Show just the three lines that matter for this project.
    grep -E "^(MemAvailable|MemFree|Cached)" /proc/meminfo | \
        awk '{printf "  %-20s %s %s\n", $1, $2, $3}'
fi

# ─── Audit log summary ─────────────────────────────────────────────────────

echo ""
echo "▶ Session audit log:"

if [[ -f "$AUDIT_LOG" ]]; then
    LINE_COUNT=$(wc -l < "$AUDIT_LOG")
    if (( LINE_COUNT > 0 )); then
        echo "  Last $((LINE_COUNT > 20 ? 20 : LINE_COUNT)) audit entries:"
        tail -20 "$AUDIT_LOG" | sed 's/^/  /'
    else
        echo "  No audit entries this session."
    fi
    # Rotate if log exceeds 500 lines - unbounded log growth is a trap.
    if (( LINE_COUNT > 500 )); then
        tail -200 "$AUDIT_LOG" > "${AUDIT_LOG}.tmp" && mv "${AUDIT_LOG}.tmp" "$AUDIT_LOG"
        echo "  (Log rotated - kept last 200 entries)"
    fi
else
    echo "  No audit log found at $AUDIT_LOG."
fi

echo ""
echo "$SEPARATOR"
echo ""
