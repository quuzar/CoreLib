#include <psx/mapping.hpp>

namespace core::psx {

    void* map_file(
        const int            fd,
        const std::size_t    size,
        const off_t          offset,
        const PageProtection protection,
        const MappingMode    mode
    ) noexcept {
        return ::mmap(
            nullptr,
            size,
            static_cast<int>(protection),
            static_cast<int>(mode),
            fd,
            offset
        );
    }

    int unmap_file(
        void*             address,
        const std::size_t size
    ) noexcept {
        return ::munmap(
            address,
            size
        );
    }

    int flush_mapping(
        void*             address,
        const std::size_t size,
        const SyncFlag    flags
    ) noexcept {
        return ::msync(
            address,
            size,
            static_cast<int>(flags)
        );
    }

}

namespace core::psx {

    MappedRegion::MappedRegion(MappingBlock block) noexcept {
        block_ = block;
    }

    MappedRegion::~MappedRegion() {
        if (valid()) {
            unmap_file(block_.address, block_.mapped_size);
        }
    }

    MappedRegion::MappedRegion(MappedRegion&& other) noexcept {
        block_ = other.block_;
        other.block_ = {};
    }

    MappedRegion& MappedRegion::operator=(MappedRegion&& other) noexcept {
        if (this == &other) {
            return *this;
        }

        if (valid() && block_.address == other.block_.address) {
            std::terminate();
        }

        if (valid()) {
            if (unmap_file(block_.address, block_.mapped_size) == -1) {
                std::terminate();
            }
        }

        block_ = other.block_;
        other.block_ = {};

        return *this;
    }


    int MappedRegion::map(
        const int            fd,
        const std::size_t    size,
        const off_t          offset,
        const PageProtection protection,
        const MappingMode    mode
    ) noexcept {
        if (valid()) {
            errno = EBUSY;
            return -1;
        }

        if (size == 0 || offset < 0) {
            errno = EINVAL;
            return -1;
        }

        const std::size_t page = page_size();

        if (page == 0) {
            return -1;
        }

        const off_t remainder = offset % static_cast<off_t>(page);
        const off_t aligned_offset = offset - remainder;
        const std::size_t delta = static_cast<std::size_t>(remainder);

        if (size > SIZE_MAX - delta) {
            errno = EOVERFLOW;
            return -1;
        }

        const std::size_t mapped_size = size + delta;

        void* address = map_file(
            fd,
            mapped_size,
            aligned_offset,
            protection,
            mode
        );

        if (address == MAP_FAILED) {
            return -1;
        }

        block_ = {
            .address = address,
            .mapped_size = mapped_size,
            .offset = delta,
            .size = size
        };

        return 0;
    }



    int MappedRegion::flush(const SyncFlag flags) noexcept {
        if (!valid()) {
            errno = EINVAL;
            return -1;
        }

        return flush_mapping(
            block_.address,
            block_.mapped_size,
            flags
        );
    }

    int MappedRegion::reset() noexcept {
        if (!valid()) {
            return 0;
        }

        if (unmap_file(block_.address, block_.mapped_size) == -1) {
            return -1;
        }

        block_ = {};
        return 0;
    }


    bool MappedRegion::valid() const noexcept {
        return block_.address != nullptr &&
               block_.address != MAP_FAILED &&
               block_.mapped_size != 0 &&
               block_.size != 0;
    }

    void* MappedRegion::data() const noexcept {
        if (!valid()) {
            return nullptr;
        }

        return static_cast<std::byte*>(block_.address) + block_.offset;
    }

    std::size_t MappedRegion::size() const noexcept {
        return block_.size;
    }

    MappingBlock MappedRegion::release() noexcept {
        const MappingBlock block = block_;
        block_ = {};
        return block;
    }

}
