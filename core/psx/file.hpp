#pragma once

#include <cstddef>
#include <cstdint>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>

namespace core::psx {
    enum class OpenMode : int {
        ReadOnly  = O_RDONLY,
        WriteOnly = O_WRONLY,
        ReadWrite = O_RDWR
    };

    enum class OpenFlag : int {
        None        = 0,
        Create      = O_CREAT,
        Exclusive   = O_EXCL,
        Truncate    = O_TRUNC,
        Append      = O_APPEND,
        NonBlocking = O_NONBLOCK,
        CloseOnExec = O_CLOEXEC
    };

    enum class SeekOrigin : int {
        Begin   = SEEK_SET,
        Current = SEEK_CUR,
        End     = SEEK_END
    };

    enum class Permission : mode_t {
        OwnerRead    = 0b100'000'000,
        OwnerWrite   = 0b010'000'000,
        OwnerExecute = 0b001'000'000,
        GroupRead    = 0b000'100'000,
        GroupWrite   = 0b000'010'000,
        GroupExecute = 0b000'001'000,
        OtherRead    = 0b000'000'100,
        OtherWrite   = 0b000'000'010,
        OtherExecute = 0b000'000'001
    };

    inline OpenFlag operator|(
        const OpenFlag lhs,
        const OpenFlag rhs
    ) noexcept {
        return static_cast<OpenFlag>(static_cast<int>(lhs) | static_cast<int>(rhs));
    }

    inline OpenFlag& operator|=(
        OpenFlag& lhs,
        const OpenFlag  rhs
    ) noexcept {
        lhs = lhs | rhs;
        return lhs;
    }

    inline OpenFlag operator&(
        const OpenFlag lhs,
        const OpenFlag rhs
    ) noexcept {
        return static_cast<OpenFlag>(static_cast<int>(lhs) & static_cast<int>(rhs));
    };

    inline Permission operator|(
        const Permission lhs,
        const Permission rhs
    ) noexcept {
        return static_cast<Permission>(static_cast<mode_t>(lhs) | static_cast<mode_t>(rhs));
    }

    inline Permission& operator|=(
        Permission& lhs,
        const Permission  rhs
    ) noexcept {
        lhs = lhs | rhs;
        return lhs;
    }

    inline Permission operator&(
        const Permission lhs,
        const Permission rhs
    ) noexcept {
        return static_cast<Permission>(static_cast<mode_t>(lhs) & static_cast<mode_t>(rhs));
    }
}



namespace core::psx {
    int open_file(
        const char* path,
        OpenMode    mode,
        OpenFlag    flags,
        mode_t      permissions = 0644
    ) noexcept;

    int close_file(
        int fd
    ) noexcept;

    ssize_t read_file(
        int         fd,
        void*       buffer,
        std::size_t count
    ) noexcept;

    ssize_t write_file(
        int         fd,
        const void* buffer,
        std::size_t count
    ) noexcept;

    ssize_t read_at(
        int         fd,
        void*       buffer,
        std::size_t count,
        off_t       offset
    ) noexcept;

    ssize_t write_at(
        int         fd,
        const void* buffer,
        std::size_t count,
        off_t       offset
    ) noexcept;

    off_t seek_file(
        int         fd,
        off_t       offset,
        SeekOrigin  origin
    ) noexcept;

    off_t file_size(
        int fd
    ) noexcept;

    int truncate_file(
        int   fd,
        off_t size
    ) noexcept;

    int sync_file(
        int fd
    ) noexcept;

}


namespace core::psx {

    class File {
    public:
        File() noexcept = default;
        explicit File(int fd) noexcept;
        ~File();

        File(const File&) = delete;
        File& operator=(const File&) = delete;

        File(File&& other) noexcept;
        File& operator=(File&& other) noexcept;

        int open(
            const char* path,
            OpenMode    mode,
            OpenFlag    flags = OpenFlag::None,
            mode_t      permissions = 0644
        ) noexcept;

        int close() noexcept;

        ssize_t read(
            void*       buffer,
            std::size_t count
        ) noexcept;

        ssize_t write(
            const void* buffer,
            std::size_t count
        ) noexcept;

        ssize_t read_at(
            void*       buffer,
            std::size_t count,
            off_t       offset
        ) noexcept;

        ssize_t write_at(
            const void* buffer,
            std::size_t count,
            off_t       offset
        ) noexcept;

        off_t seek(
            off_t       offset,
            SeekOrigin  origin
        ) noexcept;

        off_t size() const noexcept;

        int truncate(off_t size) noexcept;
        int sync() noexcept;

        [[nodiscard]] bool valid() const noexcept;
        [[nodiscard]] int native_handle() const noexcept;

        int release() noexcept;
        int reset(int fd = -1) noexcept;

    private:
        int fd_{-1};
    };

}
