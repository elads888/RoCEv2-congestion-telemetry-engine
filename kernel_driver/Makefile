# =============================================================================
# Makefile — Kernel Module Build
# =============================================================================
# The kernel build system (kbuild) compiles mock_nic.c against the headers
# of the currently running kernel. The kernel version must match exactly —
# this is why linux-headers-$(uname -r) must be installed before building.
#
# Usage:
#   make              — build mock_nic.ko
#   make clean        — remove build artefacts
#   sudo insmod mock_nic.ko   — load into the running kernel
#   sudo rmmod mock_nic       — unload
#   dmesg | tail -10          — view kernel log output
# =============================================================================

obj-m += mock_nic.o

KDIR := /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
