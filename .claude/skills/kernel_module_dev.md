# Skill: Linux Kernel Module (mock_nic context)

## What this skill covers

This skill encodes the conventions specific to developing
"mock_nic.c" - the kernel module. Read this before making any edit to anything in "kernel_driver/".

## The module's contract with the rest of the system

"mock_nic.c" makes exactly three promises to the userspace layer:

1. After "insmod", "/dev/mock_nic" exists as a char device that can be "open()"d by root.
2. A single "mmap()" call on that fd maps a 4 KB physically contiguous page into the caller's virtual address space via "remap_pfn_range".
3. After "rmmod", the physical page is freed via "kfree()" and "/dev/mock_nic" is gone.

Any edit to "mock_nic.c" must preserve all three promises. If promise #2 breaks (e.g., wrong PFN calculation, wrong size argument to "remap_pfn_range"), the userspace demo will appear to work (mmap returns successfully) but will map the wrong physical memory.

## Memory allocation rules

Use "kmalloc(PAGE_SIZE, GFP_KERNEL)" for the DMA buffer. Do not use any other mallocs.

The PFN is obtained with: "virt_to_phys(kernel_buf_ptr) >> PAGE_SHIFT"

Never hardcode PFNs. Never compute them by arithmetic on the kernel virtual address directly. Always go through "virt_to_phys()".

## remap_pfn_range

Do not use hardcoded values or kernel virtual address.


## Cleanup order in module_exit

Order matters. The correct sequence is:

"""
1. device_destroy()         — remove /dev/mock_nic
2. class_destroy()          — destroy the device class
3. cdev_del()               — remove the char device from the VFS
4. unregister_chrdev_region() — release the major:minor numbers
5. kfree(kernel_buf)        — release the physical memory
"""

## printk discipline

Every "printk" call must have a log level:

"""c
printk(KERN_INFO  "mock_nic: module loaded, pfn=%lu\n", pfn);
printk(KERN_ERR   "mock_nic: kmalloc failed\n");
printk(KERN_DEBUG "mock_nic: mmap called, vm_start=%lx\n", vma->vm_start);
"""

## Common debugging workflow

When the module misbehaves:

"""bash
# Step 1: check if it loaded at all
lsmod | grep mock_nic

# Step 2: look at kernel messages since load
dmesg | grep -i mock

# Step 3: verify the device exists and has correct permissions
ls -la /dev/mock_nic
stat /dev/mock_nic

# Step 4: verify the PFN is valid (non-zero)
dmesg | grep pfn
"""
