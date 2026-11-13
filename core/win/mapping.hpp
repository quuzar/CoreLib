#pragma once

#include "win/memory.hpp"
#include <cstddef>
#include <cstdint>
#include <windows.h>

namespace core::win {

    enum class MappingMode {
        Shared,
        Private
    };

    struct MappingBlock {
        void* address{};
        std::size_t mapped_size{};
        std::size_t offset{};
        std::size_t size{};
    };

    void* map_file(
        HANDLE file,
        std::size_t size,
        std::int64_t offset,
        PageProtection protection,
        MappingMode mode
    ) noexcept;

    int unmap_file(void* address) noexcept;

    int flush_mapping(
        void* address,
        std::size_t size
    ) noexcept;


    class MappedRegion {
    public:
        MappedRegion() noexcept = default;
        explicit MappedRegion(MappingBlock block) noexcept;
        ~MappedRegion();

        MappedRegion(const MappedRegion&) = delete;
        MappedRegion& operator=(const MappedRegion&) = delete;

        MappedRegion(MappedRegion&& other) noexcept;
        MappedRegion& operator=(MappedRegion&& other) noexcept;

        int map(
            HANDLE file,
            std::size_t size,
            std::int64_t offset,
            PageProtection protection,
            MappingMode mode
        ) noexcept;

        int flush() noexcept;
        int reset() noexcept;

        [[nodiscard]] bool valid() const noexcept;
        [[nodiscard]] void* data() const noexcept;
        [[nodiscard]] std::size_t size() const noexcept;

        MappingBlock release() noexcept;

    private:
        MappingBlock block_{};
    };

}
