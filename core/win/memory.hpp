#pragma once

#include <cstddef>
#include <windows.h>

namespace core::win {

    enum class PageProtection : DWORD {
        NoAccess         = PAGE_NOACCESS,
        ReadOnly         = PAGE_READONLY,
        ReadWrite        = PAGE_READWRITE,
        ExecuteRead      = PAGE_EXECUTE_READ,
        ExecuteReadWrite = PAGE_EXECUTE_READWRITE
    };

    struct MemoryBlock {
        void* address{};
        std::size_t size{};
    };

    std::size_t page_size() noexcept;

    void* allocate_memory(
        std::size_t size,
        PageProtection protection
    ) noexcept;

    int protect_memory(
        void* address,
        std::size_t size,
        PageProtection protection
    ) noexcept;

    int release_memory(void* address) noexcept;


    class MemoryRegion {
    public:
        MemoryRegion() noexcept = default;
        explicit MemoryRegion(MemoryBlock block) noexcept;
        ~MemoryRegion();

        MemoryRegion(const MemoryRegion&) = delete;
        MemoryRegion& operator=(const MemoryRegion&) = delete;

        MemoryRegion(MemoryRegion&& other) noexcept;
        MemoryRegion& operator=(MemoryRegion&& other) noexcept;

        int allocate(
            std::size_t size,
            PageProtection protection
        ) noexcept;

        int protect(PageProtection protection) noexcept;
        int reset() noexcept;

        [[nodiscard]] bool valid() const noexcept;
        [[nodiscard]] void* data() const noexcept;
        [[nodiscard]] std::size_t size() const noexcept;

        MemoryBlock release() noexcept;

    private:
        MemoryBlock block_{};
    };

}
