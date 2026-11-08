
#pragma once

#include <cstddef>
#include <cstdint>
#include <sys/mman.h>
#include <sys/types.h>
#include <stdexcept>

#include "psx/memory.hpp"

namespace core::psx {

    enum class MappingMode : int {
        Shared  = MAP_SHARED,
        Private = MAP_PRIVATE
    };

    enum class SyncFlag : int {
        Async      = MS_ASYNC,
        Sync       = MS_SYNC,
        Invalidate = MS_INVALIDATE
    };

    inline SyncFlag operator|(
        const SyncFlag lhs,
        const SyncFlag rhs
    ) noexcept {
        return static_cast<SyncFlag>( static_cast<int>(lhs) | static_cast<int>(rhs) );
    }

    inline SyncFlag& operator|=(
        SyncFlag& lhs,
        const SyncFlag rhs
    ) noexcept {
        lhs = lhs | rhs;
        return lhs;
    }

    inline SyncFlag operator&(
        const SyncFlag lhs,
        const SyncFlag rhs
    ) noexcept {
        return static_cast<SyncFlag>( static_cast<int>(lhs) & static_cast<int>(rhs) );
    }

    struct MappingBlock {
        void*       address{nullptr};
        std::size_t mapped_size{0};
        std::size_t offset{0};
        std::size_t size{0};
    };

    void* map_file(
        int            fd,
        std::size_t    size,
        off_t          offset,
        PageProtection protection,
        MappingMode    mode
    ) noexcept;

    int unmap_file(
        void*       address,
        std::size_t size
    ) noexcept;

    int flush_mapping(
        void*       address,
        std::size_t size,
        SyncFlag    flags = SyncFlag::Sync
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
            int            fd,
            std::size_t    size,
            off_t          offset = 0,
            PageProtection protection = PageProtection::Read,
            MappingMode    mode = MappingMode::Shared
        ) noexcept;

        int flush(
            SyncFlag flags = SyncFlag::Sync
        ) noexcept;

        int reset() noexcept;

        [[nodiscard]] bool valid() const noexcept;
        [[nodiscard]] void* data() const noexcept;
        [[nodiscard]] std::size_t size() const noexcept;

        MappingBlock release() noexcept;

    private:
        MappingBlock block_{};
    };

}
