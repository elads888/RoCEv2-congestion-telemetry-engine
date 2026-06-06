#pragma once

#include <cstddef>
#include <stdexcept>
#include <string>

#include <sys/mman.h>

// =============================================================================
// bridge.h — Zero-copy kernel-bypass bridge (RAII wrappers)
// =============================================================================

// Opens /dev/mock_nic and holds the file descriptor for the lifetime of the
// object. Throws std::runtime_error if the kernel module is not loaded.
// raw file descriptor has no automatic cleanup the destructor guarantees close() is called on every
// exit path with no manual cleanup required.
class DeviceHandle
{
public:
    explicit DeviceHandle(const char *path);
    ~DeviceHandle();

    int fd() const { return fd_; }

    DeviceHandle(const DeviceHandle &) = delete;
    DeviceHandle &operator=(const DeviceHandle &) = delete;

private:
    int fd_ = -1;
};

// Calls mmap() on the device fd, which triggers device_mmap() in mock_nic.c.
// Wires this virtual address range to the kernel's physical page frame — establishing the zero-copy bridge.
// RAII is used to ensure cleanup with munmap() on destruction.
class MappedBuffer
{
public:
    MappedBuffer(int fd, size_t size);
    ~MappedBuffer();

    void *ptr() const { return ptr_; }

    MappedBuffer(const MappedBuffer &) = delete;
    MappedBuffer &operator=(const MappedBuffer &) = delete;

private:
    void *ptr_ = MAP_FAILED;
    size_t size_ = 0;
};
