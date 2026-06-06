#include "bridge.h"

#include <iostream>
#include <cstring>

#include <fcntl.h>
#include <unistd.h>

// =============================================================================
// bridge.cpp — Zero-copy kernel-bypass bridge implementations
// =============================================================================

DeviceHandle::DeviceHandle(const char *path)
{
    fd_ = open(path, O_RDWR);
    if (fd_ < 0)
    {
        throw std::runtime_error(
            std::string("open(") + path + ") failed: " + strerror(errno) +
            "\n  → Is the kernel module loaded?\n"
            "    sudo insmod kernel_driver/mock_nic.ko\n"
            "    sudo mknod /dev/mock_nic c "
            "$(dmesg | grep 'major=' | tail -1 | grep -oP 'major=\\K[0-9]+') 0\n"
            "    sudo chmod 666 /dev/mock_nic");
    }
    std::cout << "[Bridge] Opened " << path << " (fd=" << fd_ << ")\n";
}

DeviceHandle::~DeviceHandle()
{
    if (fd_ >= 0)
        close(fd_);
}

// -----------------------------------------------------------------------------

MappedBuffer::MappedBuffer(int fd, size_t size) : size_(size)
{
    // mmap() triggers device_mmap() in mock_nic.c, which calls
    // remap_pfn_range() to permanently wire our virtual address range
    // to the kernel's physical page frame. After this call returns,
    // every read/write through ptr_ is a direct DRAM access. This means no syscall, no kernel code, no copy on the data path.
    ptr_ = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (ptr_ == MAP_FAILED)
        throw std::runtime_error(
            std::string("mmap() failed: ") + strerror(errno));
    std::cout << "[Bridge] mmap bridge live — virtual=" << ptr_
              << " -> kernel physical RAM (zero-copy, kernel absent from data path)" << std::endl;
}

MappedBuffer::~MappedBuffer()
{
    if (ptr_ != MAP_FAILED)
        munmap(ptr_, size_);
}
