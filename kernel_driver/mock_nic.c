// =============================================================================
// mock_nic.c — Linux Kernel Module: Physical RAM Allocator + mmap Gateway
// 
// This module simulates a NIC driver by doing two things:
//     1. Claiming a physically contiguous, non-swappable 4 KiB page of RAM
//        using kmalloc(). This represents the DMA ring buffer where a real NIC
//        would put incoming packets via PCIe write transactions.
//     2. Registering /dev/mock_nic as a character device. When user-space
//        calls mmap() on this device, the handler calls remap_pfn_range() internally
//        to permanently wire user virtual addresses to that physical page.
//
// =============================================================================

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("RoCEv2 Telemetry Project");
MODULE_DESCRIPTION("Mock NIC: kernel RAM allocator + zero-copy mmap bridge");
MODULE_VERSION("1.0");

#define DEVICE_NAME  "mock_nic"
#define BUFFER_SIZE  (4096)

static int    major_number;
static char  *kernel_buffer = NULL;

static int device_open(struct inode *inode, struct file *file)
{
    printk(KERN_INFO "mock_nic: device opened\n");
    return 0;
}

static int device_release(struct inode *inode, struct file *file)
{
    printk(KERN_INFO "mock_nic: device closed\n");
    return 0;
}

// device_mmap — THE ZERO-COPY BRIDGE
static int device_mmap(struct file *file, struct vm_area_struct *vma)
{
    unsigned long size = vma->vm_end - vma->vm_start;

    if (size > BUFFER_SIZE) {
        printk(KERN_ERR "mock_nic: mmap request (%lu B) exceeds buffer (%d B)\n",
               size, BUFFER_SIZE);
        return -EINVAL;
    }

    unsigned long pfn = virt_to_phys(kernel_buffer) >> PAGE_SHIFT;

    if (remap_pfn_range(vma, vma->vm_start, pfn, size, vma->vm_page_prot)) {
        printk(KERN_ERR "mock_nic: remap_pfn_range failed\n");
        return -EAGAIN;
    }

    printk(KERN_INFO "mock_nic: bridge live. user_va=0x%lx -> pfn=0x%lx (phys=0x%lx)\n",
           vma->vm_start, pfn, virt_to_phys(kernel_buffer));
    return 0;
}

static const struct file_operations fops = {
    .owner   = THIS_MODULE,
    .open    = device_open,
    .release = device_release,
    .mmap    = device_mmap,
};

static int __init mock_nic_init(void)
{
    kernel_buffer = kmalloc(BUFFER_SIZE, GFP_KERNEL);
    if (!kernel_buffer) {
        printk(KERN_ERR "mock_nic: kmalloc failed\n");
        return -ENOMEM;
    }
    memset(kernel_buffer, 0, BUFFER_SIZE);
    printk(KERN_INFO "mock_nic: allocated %d B at virt=0x%px phys=0x%lx\n",
           BUFFER_SIZE, kernel_buffer, virt_to_phys(kernel_buffer));

    major_number = register_chrdev(0, DEVICE_NAME, &fops);
    if (major_number < 0) {
        printk(KERN_ERR "mock_nic: register_chrdev failed: %d\n", major_number);
        kfree(kernel_buffer);
        return major_number;
    }
    printk(KERN_INFO "mock_nic: ready on major=%d\n", major_number);
    return 0;
}

static void __exit mock_nic_exit(void)
{
    unregister_chrdev(major_number, DEVICE_NAME);
    kfree(kernel_buffer);
    printk(KERN_INFO "mock_nic: unloaded, RAM freed\n");
}

module_init(mock_nic_init);
module_exit(mock_nic_exit);
