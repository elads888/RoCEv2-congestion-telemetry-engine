# =============================================================================
# Makefile — Top-level dispatcher
# =============================================================================
# Delegates to the kernel build (Makefile.kernel) and the CMake-based
# userspace simulator build.
#
# Usage:
#   make              — build everything
#   make kernel       — build mock_nic.ko kernel module only
#   make sim          — configure & build the userspace simulator only
#   make clean        — remove all build artefacts
# =============================================================================

.PHONY: all kernel sim clean

all: kernel sim

kernel:
	$(MAKE) -C kernel_driver

sim:
	cmake -S . -B build
	$(MAKE) -C build

clean:
	$(MAKE) -C kernel_driver clean
	$(MAKE) -C build clean
