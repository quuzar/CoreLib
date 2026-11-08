
#pragma once

#include <cstddef>
#include <sys/mman.h>
#include <unistd.h>

namespace core::psx {

    enum class PageProtection : int {
        None    = PROT_NONE,
        Read    = PROT_READ,
        Write   = PROT_WRITE,
        Execute = PROT_EXEC
    };

    inline PageProtection operator|(
        const PageProtection lhs,
        const PageProtection rhs
    ) noexcept {
        return static_cast<PageProtection>( static_cast<int>(lhs) | static_cast<int>(rhs) );
    }

    inline PageProtection& operator|=(
        PageProtection&      lhs,
        const PageProtection rhs
    ) noexcept {
        lhs = lhs | rhs;
        return lhs;
    }

    inline PageProtection operator&(
        const PageProtection lhs,
        const PageProtection rhs
    ) noexcept {
        return static_cast<PageProtection>( static_cast<int>(lhs) & static_cast<int>(rhs) );
    }

    struct MemoryBlock {
        void* address{nullptr};
        std::size_t size{0};
    };

    std::size_t page_size() noexcept;

    void* allocate_memory(
        std::size_t size,
        PageProtection protection = PageProtection::Read | PageProtection::Write
    ) noexcept;

    int protect_memory(
        void* address,
        std::size_t size,
        PageProtection protection
    ) noexcept;

    int release_memory(
        void* address,
        std::size_t size
    ) noexcept;


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
            PageProtection protection = PageProtection::Read | PageProtection::Write
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
