#include "win/memory.hpp"
#include <exception>

namespace core::win {

    std::size_t page_size() noexcept {
        SYSTEM_INFO info{};
        ::GetSystemInfo(&info);

        return info.dwPageSize;
    }

    void* allocate_memory(
        std::size_t size,
        PageProtection protection
    ) noexcept {
        if (size == 0) {
            ::SetLastError(ERROR_INVALID_PARAMETER);
            return nullptr;
        }

        return ::VirtualAlloc(
            nullptr,
            size,
            MEM_RESERVE | MEM_COMMIT,
            static_cast<DWORD>(protection)
        );
    }

    int protect_memory(
        void* address,
        std::size_t size,
        PageProtection protection
    ) noexcept {
        if (address == nullptr || size == 0) {
            ::SetLastError(ERROR_INVALID_PARAMETER);
            return -1;
        }

        DWORD previous{};

        return ::VirtualProtect(
            address,
            size,
            static_cast<DWORD>(protection),
            &previous
        ) ? 0 : -1;
    }

    int release_memory(void* address) noexcept {
        if (address == nullptr) {
            ::SetLastError(ERROR_INVALID_PARAMETER);
            return -1;
        }

        return ::VirtualFree(
            address,
            0,
            MEM_RELEASE
        ) ? 0 : -1;
    }


    MemoryRegion::MemoryRegion(MemoryBlock block) noexcept
        : block_(block) {}

    MemoryRegion::~MemoryRegion() {
        if (valid()) {
            release_memory(block_.address);
        }
    }

    MemoryRegion::MemoryRegion(MemoryRegion&& other) noexcept
        : block_(other.block_) {
        other.block_ = {};
    }

    MemoryRegion& MemoryRegion::operator=(MemoryRegion&& other) noexcept {
        if (this == &other) {
            return *this;
        }

        if (valid() && block_.address == other.block_.address) {
            std::terminate();
        }

        if (valid() && release_memory(block_.address) == -1) {
            std::terminate();
        }

        block_ = other.block_;
        other.block_ = {};

        return *this;
    }

    int MemoryRegion::allocate(
        std::size_t size,
        PageProtection protection
    ) noexcept {
        if (valid()) {
            ::SetLastError(ERROR_BUSY);
            return -1;
        }

        void* address = allocate_memory(size, protection);

        if (address == nullptr) {
            return -1;
        }

        block_ = {
            .address = address,
            .size = size
        };

        return 0;
    }

    int MemoryRegion::protect(PageProtection protection) noexcept {
        if (!valid()) {
            ::SetLastError(ERROR_INVALID_ADDRESS);
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

        if (release_memory(block_.address) == -1) {
            return -1;
        }

        block_ = {};
        return 0;
    }

    bool MemoryRegion::valid() const noexcept {
        return block_.address != nullptr && block_.size != 0;
    }

    void* MemoryRegion::data() const noexcept {
        return valid() ? block_.address : nullptr;
    }

    std::size_t MemoryRegion::size() const noexcept {
        return block_.size;
    }

    MemoryBlock MemoryRegion::release() noexcept {
        MemoryBlock block = block_;
        block_ = {};

        return block;
    }

}
