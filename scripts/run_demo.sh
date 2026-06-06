#!/usr/bin/env bash
# =============================================================================
# run_demo.sh — RoCEv2 Congestion Telemetry: One-Shot Demo Launcher
# =============================================================================
#
# This script is the single entry point for the demo. It:
#   1. Checks that it is running on Linux with sudo privileges
#   2. Installs all build dependencies if they are missing
#   3. Builds the kernel module (mock_nic.ko) against the running kernel
#   4. Builds the userspace demo binary (integrated_demo) with CMake
#   5. Loads the kernel module into the running kernel
#   6. Creates /dev/mock_nic and sets permissions
#   7. Prints the browser URL and launches the demo
#
# Usage (from inside the cloned repository):
#   sudo bash scripts/run_demo.sh
#
# To stop the demo:
#   Press Ctrl+C — the demo shuts down cleanly within one second.
#
# To clean up the kernel module after stopping:
#   sudo rmmod mock_nic
#   sudo rm -f /dev/mock_nic
# =============================================================================

# exit on any error, undefined variable, or failed pipeline command
set -euo pipefail

# Color helpers
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'
CYAN='\033[0;36m'; BOLD='\033[1m'; NC='\033[0m'

# functions for formatted output
step()  { echo -e "\n${CYAN}▶ [${1}/${TOTAL}]${NC} ${BOLD}${2}${NC}"; }
ok()    { echo -e "    ${GREEN}✓${NC}  $1"; }
warn()  { echo -e "    ${YELLOW}!${NC}  $1"; }
info()  { echo -e "    ${CYAN}·${NC}  $1"; }
die()   {
    echo -e "\n${RED}[ERROR]${NC} $1"
    echo -e "${RED}        $2${NC}\n"
    exit 1
}

TOTAL=7

# Resolve paths relative to this script's location so the script works
# regardless of where the user's current directory is when they run it.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
KERNEL_DIR="$PROJECT_ROOT/kernel_driver"
DEMO_DIR="$PROJECT_ROOT"
DEMO_BUILD="$DEMO_DIR/build"
DEMO_BIN="$DEMO_BUILD/integrated_demo"

# ---------------------------------------------------------------------------
# STEP 0: Pre-flight checks
# ---------------------------------------------------------------------------

echo -e "\n${BOLD}╔══════════════════════════════════════════════════════════╗"
echo -e "║    RoCEv2 Congestion Telemetry — Interactive Demo         ║"
echo -e "╚══════════════════════════════════════════════════════════╝${NC}"
echo -e "    Project root: ${CYAN}$PROJECT_ROOT${NC}\n"

# run as root because:
#   - insmod (loading a kernel module) requires root
#   - mknod (creating a device node in /dev) requires root
if [ "$EUID" -ne 0 ]; then
    die "This script must run with sudo." \
        "Run: sudo bash scripts/run_demo.sh"
fi

# Must be Linux — the kernel module uses Linux-specific kernel APIs
# (kmalloc, remap_pfn_range, register_chrdev)
if [ "$(uname -s)" != "Linux" ]; then
    die "This project requires a Linux kernel." \
        "Run it inside a Linux VM (VirtualBox, VMware, or a cloud instance)."
fi

# ---------------------------------------------------------------------------
# STEP 1: Install build dependencies if missing
# ---------------------------------------------------------------------------
step 1 "Checking build dependencies"

MISSING_PKGS=()
KERNEL_VER=$(uname -r)
HEADERS_PATH="/lib/modules/$KERNEL_VER/build"

# Check for each dependency individually so the error message is precise.
command -v gcc   &>/dev/null || MISSING_PKGS+=("build-essential")
command -v cmake &>/dev/null || MISSING_PKGS+=("cmake")
command -v git   &>/dev/null || MISSING_PKGS+=("git")

# The kernel headers MUST match the running kernel exactly.
# If the path does not exist, the module build will fail.
if [ ! -d "$HEADERS_PATH" ]; then
    MISSING_PKGS+=("linux-headers-$KERNEL_VER")
fi

if [ ${#MISSING_PKGS[@]} -gt 0 ]; then
    warn "Missing packages: ${MISSING_PKGS[*]}"
    info "Installing..."
    apt-get update -qq
    apt-get install -y -qq "${MISSING_PKGS[@]}" \
        || die "apt-get install failed." \
               "Check your internet connection and try again."
    ok "Dependencies installed"
else
    ok "All dependencies present (gcc, cmake, git, kernel headers for $KERNEL_VER)"
fi

# ---------------------------------------------------------------------------
# STEP 2: Build the kernel module
# ---------------------------------------------------------------------------
step 2 "Building kernel module (mock_nic.ko)"

# The kernel build system (kbuild) compiles mock_nic.c against the headers
# of the CURRENTLY RUNNING kernel. This is why the kernel version in
# HEADERS_PATH must match `uname -r` exactly.
cd "$KERNEL_DIR"
make -s 2>&1 | grep -v "^$" | grep -v "^make" \
    | grep -v "Skipping BTF" || true   # suppress cosmetic warnings

[ -f "$KERNEL_DIR/mock_nic.ko" ] \
    || die "mock_nic.ko was not produced." \
           "Check the output above for compiler errors."

ok "mock_nic.ko built against kernel $KERNEL_VER"

# ---------------------------------------------------------------------------
# STEP 3: Build the userspace demo binary
# ---------------------------------------------------------------------------
step 3 "Building integrated demo binary"

# Clean a stale build directory if it was previously created by root
# (running via sudo) and now has wrong ownership. This prevents the
# "Permission denied on cmake_install.cmake" error.
if [ -d "$DEMO_BUILD" ]; then
    rm -rf "$DEMO_BUILD"
fi
mkdir -p "$DEMO_BUILD"

cd "$DEMO_BUILD"
cmake "$DEMO_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    -Wno-dev \
    --no-warn-unused-cli \
    > /dev/null 2>&1

make -s -j"$(nproc)" 2>&1 | grep -v "^$" || true

[ -f "$DEMO_BIN" ] \
    || die "integrated_demo binary was not produced." \
           "Check the output above for compiler errors."

ok "integrated_demo built at $DEMO_BIN"

# ---------------------------------------------------------------------------
# STEP 4: Load the kernel module
# ---------------------------------------------------------------------------
step 4 "Loading kernel module"

# Unload any previous instance first — makes this script safely re-runnable.
if lsmod | grep -q "^mock_nic"; then
    warn "mock_nic already loaded — unloading previous instance"
    rmmod mock_nic 2>/dev/null || true
    sleep 0.2
fi

cd "$KERNEL_DIR"
insmod mock_nic.ko \
    || die "insmod failed." \
           "Check dmesg for kernel errors: dmesg | tail -20"

ok "mock_nic.ko loaded into the running kernel"

# Give the kernel a moment to write the log entry before we read it.
sleep 0.4

# Read the major device number the kernel dynamically assigned.
# We need this to create the device node in the next step.
MAJOR=$(dmesg | grep "mock_nic: ready on major=" | tail -1 \
       | grep -oP 'major=\K[0-9]+' || true)

if [ -z "$MAJOR" ]; then
    # Fallback: try broader dmesg search
    MAJOR=$(dmesg | tail -30 | grep -oP 'major=\K[0-9]+' | head -1 || true)
fi

[ -n "$MAJOR" ] \
    || die "Could not read major number from dmesg." \
           "Run: dmesg | tail -20 and look for 'mock_nic: ready on major='"

ok "Kernel assigned major device number: $MAJOR"

# ---------------------------------------------------------------------------
# STEP 5: Create the device node
# ---------------------------------------------------------------------------
step 5 "Creating /dev/mock_nic"

# /dev/mock_nic is the gateway between user-space and the kernel module.
# The character device (c) with major=$MAJOR and minor=0 matches what
# register_chrdev() created inside mock_nic_init().
rm -f /dev/mock_nic
mknod /dev/mock_nic c "$MAJOR" 0

# 666 permissions let the demo binary open the device without sudo.
# In production you would use a udev rule to set ownership instead.
chmod 666 /dev/mock_nic

ok "/dev/mock_nic created (major=$MAJOR, mode=666)"

# ---------------------------------------------------------------------------
# STEP 6: Verify the bridge can be established
# ---------------------------------------------------------------------------
step 6 "Verifying device node"

[ -r /dev/mock_nic ] \
    || die "/dev/mock_nic is not readable." \
           "Check: ls -la /dev/mock_nic"

ok "Device node is accessible — mmap bridge ready"

# ---------------------------------------------------------------------------
# STEP 7: Launch the demo
# ---------------------------------------------------------------------------
step 7 "Launching integrated demo"

# Find the VM's non-loopback IP address to print the correct browser URL.
# This works on both bridged and NAT-with-port-forwarding setups.
VM_IP=$(ip addr show \
       | grep 'inet ' \
       | grep -v '127.0.0.1' \
       | awk '{print $2}' \
       | cut -d/ -f1 \
       | head -1 2>/dev/null || echo "YOUR_VM_IP")

echo ""
echo -e "  ┌─────────────────────────────────────────────────────────┐"
echo -e "  │  ${BOLD}Open this URL in your browser:${NC}                        │"
echo -e "  │                                                         │"
echo -e "  │  ${GREEN}http://${VM_IP}:8080${NC}                              │"
echo -e "  │                                                         │"
echo -e "  │  If that does not work, also try:                       │"
echo -e "  │  ${CYAN}http://localhost:8080${NC}  (if using port forwarding)     │"
echo -e "  └─────────────────────────────────────────────────────────┘"
echo ""
echo -e "  ${BOLD}What to do:${NC}"
echo    "  1. Open the URL above in your browser"
echo    "  2. Drag the slider RIGHT past 5,000 pps → watch the buffer fill"
echo    "  3. At 70% buffer fill → ECN activates, packets turn red"
echo    "  4. Hold above 7,000 pps → congestion rate climbs, ALERT fires"
echo    "  5. Drag slider LEFT below 5,000 pps → buffer drains, packets green"
echo    "  6. This is the DCQCN story: raise rate to congest, lower to recover"
echo ""
echo    "  Press Ctrl+C to stop the demo cleanly."
echo ""

# Register a cleanup trap so Ctrl+C also removes the device node cleanly.
# This means the user does not need to run any manual cleanup commands.
cleanup() {
    echo -e "\n${CYAN}[Cleanup]${NC} Removing /dev/mock_nic and unloading module..."
    rm -f /dev/mock_nic
    rmmod mock_nic 2>/dev/null || true
    echo -e "${GREEN}[Cleanup]${NC} Done. Goodbye."
}
trap cleanup EXIT

# Run the demo binary directly (exec replaces this shell process).
# The binary runs as root because this whole script runs as root,
# but /dev/mock_nic has 666 permissions so it would also work as a
# regular user once the module is loaded.
exec "$DEMO_BIN"
