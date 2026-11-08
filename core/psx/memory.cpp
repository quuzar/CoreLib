#include <psx/memory.hpp>
#include <cerrno>
#include <exception>

namespace core::psx {

    std::size_t page_size() noexcept {
        const long result = ::sysconf(_SC_PAGESIZE);

        if (result == -1) {
            return 0;
        }

        return static_cast<std::size_t>(result);
    }

    void* allocate_memory(
        std::size_t    size,
        PageProtection protection
    ) noexcept {
        return ::mmap(
            nullptr,
            size,
            static_cast<int>(protection),
            MAP_PRIVATE | MAP_ANONYMOUS,
            -1,
            0
        );
    }

    int protect_memory(
        void*           address,
        std::size_t     size,
        PageProtection  protection
    ) noexcept {
        return ::mprotect(
            address,
            size,
            static_cast<int>(protection)
        );
    }

    int release_memory(
        void*       address,
        std::size_t size
    ) noexcept {
        return ::munmap(
            address,
            size
        );
    }

}

namespace core::psx {

    MemoryRegion::MemoryRegion(MemoryBlock block) noexcept {
        block_ = block;
    }

    MemoryRegion::~MemoryRegion() {
        if (valid()) {
            release_memory(
                block_.address,
                block_.size
            );
        }
    }

    MemoryRegion::MemoryRegion(MemoryRegion&& other) noexcept {
        block_ = other.block_;
        other.block_ = {};
    }

    MemoryRegion& MemoryRegion::operator=(MemoryRegion&& other) noexcept {
        if (this == &other) {
            return *this;
        }

        if (valid()) {
            if (release_memory(block_.address, block_.size) == -1) {
                std::terminate();
            }
        }

        block_ = other.block_;
        other.block_ = {};

        return *this;
    }

    int MemoryRegion::allocate(
        std::size_t    size,
        PageProtection protection
    ) noexcept {
        if (valid()) {
            errno = EBUSY;
            return -1;
        }

        if (size == 0) {
            errno = EINVAL;
            return -1;
        }

        void* address = allocate_memory(
            size,
            protection
        );

        if (address == MAP_FAILED) {
            return -1;
        }

        block_.address = address;
        block_.size    = size;

        return 0;
    }

    int MemoryRegion::protect(
        PageProtection protection
    ) noexcept {
        if (!valid()) {
            errno = EINVAL;
            return -1;
        }

        return protect_memory(
            block_.address,
            block_.size,
            protection
        );
    }

    int MemoryRegion::reset() noexcept {
        if (!valid()) {
            return 0;
        }

        if (release_memory(block_.address, block_.size) == -1) {
            return -1;
        }

        block_ = {};
        return 0;
    }

    bool MemoryRegion::valid() const noexcept {
        return block_.address != nullptr &&
               block_.address != MAP_FAILED &&
               block_.size != 0;
    }

    void* MemoryRegion::data() const noexcept {
        return block_.address;
    }

    std::size_t MemoryRegion::size() const noexcept {
        return block_.size;
    }

    MemoryBlock MemoryRegion::release() noexcept {
        const MemoryBlock block = block_;
        block_ = {};
        return block;
    }

}