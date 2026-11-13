#include "win/mapping.hpp"
#include <cstddef>
#include <cstdint>
#include <exception>
#include <limits>

namespace core::win {
    namespace {
        struct MappingAccess {
            DWORD protection{};
            DWORD access{};
        };

        std::size_t allocation_granularity() noexcept {
            SYSTEM_INFO info{};
            ::GetSystemInfo(&info);

            return info.dwAllocationGranularity;
        }

        bool mapping_access(
            PageProtection protection,
            MappingMode mode,
            MappingAccess& result
        ) noexcept {
            switch (mode) {
                case MappingMode::Shared:
                    switch (protection) {
                        case PageProtection::ReadOnly:
                            result = {PAGE_READONLY, FILE_MAP_READ};
                            return true;

                        case PageProtection::ReadWrite:
                            result = {PAGE_READWRITE, FILE_MAP_WRITE};
                            return true;

                        case PageProtection::ExecuteRead:
                            result = {
                                PAGE_EXECUTE_READ,
                                FILE_MAP_READ | FILE_MAP_EXECUTE
                            };
                            return true;

                        case PageProtection::ExecuteReadWrite:
                            result = {
                                PAGE_EXECUTE_READWRITE,
                                FILE_MAP_WRITE | FILE_MAP_EXECUTE
                            };
                            return true;

                        default:
                            break;
                    }
                    break;

                case MappingMode::Private:
                    switch (protection) {
                        case PageProtection::ReadOnly:
                            result = {PAGE_READONLY, FILE_MAP_READ};
                            return true;

                        case PageProtection::ReadWrite:
                            result = {PAGE_WRITECOPY, FILE_MAP_COPY};
                            return true;

                        case PageProtection::ExecuteRead:
                            result = {
                                PAGE_EXECUTE_READ,
                                FILE_MAP_READ | FILE_MAP_EXECUTE
                            };
                            return true;

                        case PageProtection::ExecuteReadWrite:
                            result = {
                                PAGE_EXECUTE_WRITECOPY,
                                FILE_MAP_COPY | FILE_MAP_EXECUTE
                            };
                            return true;

                        default:
                            break;
                    }
                    break;
            }

            ::SetLastError(ERROR_INVALID_PARAMETER);
            return false;
        }
    }

    void* map_file(
        HANDLE file,
        std::size_t size,
        std::int64_t offset,
        PageProtection protection,
        MappingMode mode
    ) noexcept {
        if (file == nullptr || file == INVALID_HANDLE_VALUE) {
            ::SetLastError(ERROR_INVALID_HANDLE);
            return nullptr;
        }

        if (size == 0 || offset < 0) {
            ::SetLastError(ERROR_INVALID_PARAMETER);
            return nullptr;
        }

        const std::size_t granularity = allocation_granularity();

        if (granularity == 0) {
            ::SetLastError(ERROR_INVALID_PARAMETER);
            return nullptr;
        }

        if (static_cast<std::uint64_t>(offset) % granularity != 0) {
            ::SetLastError(ERROR_MAPPED_ALIGNMENT);
            return nullptr;
        }

        MappingAccess flags{};

        if (!mapping_access(protection, mode, flags)) {
            return nullptr;
        }

        HANDLE mapping = ::CreateFileMappingW(
            file,
            nullptr,
            flags.protection,
            0,
            0,
            nullptr
        );

        if (mapping == nullptr) {
            return nullptr;
        }

        const auto position = static_cast<std::uint64_t>(offset);

        void* address = ::MapViewOfFile(
            mapping,
            flags.access,
            static_cast<DWORD>(position >> 32),
            static_cast<DWORD>(position),
            size
        );

        const DWORD error = address == nullptr
            ? ::GetLastError()
            : ERROR_SUCCESS;

        ::CloseHandle(mapping);

        if (address == nullptr) {
            ::SetLastError(error);
        }

        return address;
    }

    int unmap_file(void* address) noexcept {
        if (address == nullptr) {
            ::SetLastError(ERROR_INVALID_PARAMETER);
            return -1;
        }

        return ::UnmapViewOfFile(address) ? 0 : -1;
    }

    int flush_mapping(
        void* address,
        std::size_t size
    ) noexcept {
        if (address == nullptr || size == 0) {
            ::SetLastError(ERROR_INVALID_PARAMETER);
            return -1;
        }

        return ::FlushViewOfFile(address, size) ? 0 : -1;
    }

    MappedRegion::MappedRegion(MappingBlock block) noexcept
        : block_(block) {}

    MappedRegion::~MappedRegion() {
        if (valid()) {
            unmap_file(block_.address);
        }
    }

    MappedRegion::MappedRegion(MappedRegion&& other) noexcept
        : block_(other.block_) {
        other.block_ = {};
    }

    MappedRegion& MappedRegion::operator=(MappedRegion&& other) noexcept {
        if (this == &other) {
            return *this;
        }

        if (valid() && other.valid() &&
            block_.address == other.block_.address) {
            std::terminate();
        }

        if (valid() && unmap_file(block_.address) == -1) {
            std::terminate();
        }

        block_ = other.block_;
        other.block_ = {};

        return *this;
    }

    int MappedRegion::map(
        HANDLE file,
        std::size_t size,
        std::int64_t offset,
        PageProtection protection,
        MappingMode mode
    ) noexcept {
        if (valid()) {
            ::SetLastError(ERROR_BUSY);
            return -1;
        }

        if (size == 0 || offset < 0) {
            ::SetLastError(ERROR_INVALID_PARAMETER);
            return -1;
        }

        const std::size_t granularity = allocation_granularity();

        if (granularity == 0) {
            ::SetLastError(ERROR_INVALID_PARAMETER);
            return -1;
        }

        const auto position = static_cast<std::uint64_t>(offset);
        const auto remainder = position % granularity;

        const auto aligned_offset = position - remainder;
        const auto delta = static_cast<std::size_t>(remainder);

        if (size > (std::numeric_limits<std::size_t>::max)() - delta) {
            ::SetLastError(ERROR_ARITHMETIC_OVERFLOW);
            return -1;
        }

        const std::size_t mapped_size = size + delta;

        void* address = map_file(
            file,
            mapped_size,
            static_cast<std::int64_t>(aligned_offset),
            protection,
            mode
        );

        if (address == nullptr) {
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

    int MappedRegion::flush() noexcept {
        if (!valid()) {
            ::SetLastError(ERROR_INVALID_ADDRESS);
            return -1;
        }

        return flush_mapping(
            block_.address,
            block_.mapped_size
        );
    }

    int MappedRegion::reset() noexcept {
        if (!valid()) {
            return 0;
        }

        if (unmap_file(block_.address) == -1) {
            return -1;
        }

        block_ = {};
        return 0;
    }

    bool MappedRegion::valid() const noexcept {
        return block_.address != nullptr &&
               block_.mapped_size != 0 &&
               block_.size != 0;
    }

    void* MappedRegion::data() const noexcept {
        if (!valid()) {
            return nullptr;
        }

        return static_cast<std::byte*>(block_.address)
            + block_.offset;
    }

    std::size_t MappedRegion::size() const noexcept {
        return block_.size;
    }

    MappingBlock MappedRegion::release() noexcept {
        MappingBlock block = block_;
        block_ = {};

        return block;
    }
}