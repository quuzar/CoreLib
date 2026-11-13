#pragma once

#include <cstddef>
#include <cstdint>
#include <windows.h>

namespace core::win {

    enum class FileAccess : DWORD {
        Read      = GENERIC_READ,
        Write     = GENERIC_WRITE,
        ReadWrite = GENERIC_READ | GENERIC_WRITE
    };

    enum class FileShare : DWORD {
        None   = 0,
        Read   = FILE_SHARE_READ,
        Write  = FILE_SHARE_WRITE,
        Delete = FILE_SHARE_DELETE
    };

    inline FileShare operator|(FileShare lhs, FileShare rhs) noexcept {
        return static_cast<FileShare>(
            static_cast<DWORD>(lhs) | static_cast<DWORD>(rhs)
        );
    }

    inline FileShare& operator|=(FileShare& lhs, FileShare rhs) noexcept {
        lhs = lhs | rhs;
        return lhs;
    }

    enum class FileCreation : DWORD {
        CreateNew        = CREATE_NEW,
        CreateAlways     = CREATE_ALWAYS,
        OpenExisting     = OPEN_EXISTING,
        OpenAlways       = OPEN_ALWAYS,
        TruncateExisting = TRUNCATE_EXISTING
    };

    enum class FileOptions : DWORD {
        Normal         = FILE_ATTRIBUTE_NORMAL,
        ReadOnly       = FILE_ATTRIBUTE_READONLY,
        Hidden         = FILE_ATTRIBUTE_HIDDEN,
        Temporary      = FILE_ATTRIBUTE_TEMPORARY,
        SequentialScan = FILE_FLAG_SEQUENTIAL_SCAN,
        RandomAccess   = FILE_FLAG_RANDOM_ACCESS,
        WriteThrough   = FILE_FLAG_WRITE_THROUGH
    };

    inline FileOptions operator|(FileOptions lhs, FileOptions rhs) noexcept {
        return static_cast<FileOptions>(
            static_cast<DWORD>(lhs) | static_cast<DWORD>(rhs)
        );
    }

    inline FileOptions& operator|=(FileOptions& lhs, FileOptions rhs) noexcept {
        lhs = lhs | rhs;
        return lhs;
    }

    enum class SeekOrigin : DWORD {
        Begin   = FILE_BEGIN,
        Current = FILE_CURRENT,
        End     = FILE_END
    };

    HANDLE open_file(
        const wchar_t* path,
        FileAccess access,
        FileShare share = FileShare::Read,
        FileCreation creation = FileCreation::OpenExisting,
        FileOptions options = FileOptions::Normal
    ) noexcept;

    int close_file(HANDLE handle) noexcept;

    std::int64_t read_file(
        HANDLE handle,
        void* buffer,
        std::size_t count
    ) noexcept;

    std::int64_t write_file(
        HANDLE handle,
        const void* buffer,
        std::size_t count
    ) noexcept;

    std::int64_t read_at(
        HANDLE handle,
        void* buffer,
        std::size_t count,
        std::int64_t offset
    ) noexcept;

    std::int64_t write_at(
        HANDLE handle,
        const void* buffer,
        std::size_t count,
        std::int64_t offset
    ) noexcept;

    std::int64_t seek_file(
        HANDLE handle,
        std::int64_t offset,
        SeekOrigin origin
    ) noexcept;

    std::int64_t file_size(HANDLE handle) noexcept;

    int truncate_file(
        HANDLE handle,
        std::int64_t size
    ) noexcept;

    int sync_file(HANDLE handle) noexcept;

    class File {
    public:
        File() noexcept = default;
        explicit File(HANDLE handle) noexcept;
        ~File();

        File(const File&) = delete;
        File& operator=(const File&) = delete;

        File(File&& other) noexcept;
        File& operator=(File&& other) noexcept;

        int open(
            const wchar_t* path,
            FileAccess access,
            FileShare share = FileShare::Read,
            FileCreation creation = FileCreation::OpenExisting,
            FileOptions options = FileOptions::Normal
        ) noexcept;

        int close() noexcept;

        std::int64_t read(
            void* buffer,
            std::size_t count
        ) noexcept;

        std::int64_t write(
            const void* buffer,
            std::size_t count
        ) noexcept;

        std::int64_t read_at(
            void* buffer,
            std::size_t count,
            std::int64_t offset
        ) noexcept;

        std::int64_t write_at(
            const void* buffer,
            std::size_t count,
            std::int64_t offset
        ) noexcept;

        std::int64_t seek(
            std::int64_t offset,
            SeekOrigin origin
        ) noexcept;

        [[nodiscard]] std::int64_t size() const noexcept;

        int truncate(std::int64_t size) noexcept;
        int sync() noexcept;

        [[nodiscard]] bool valid() const noexcept;
        [[nodiscard]] HANDLE native_handle() const noexcept;

        HANDLE release() noexcept;
        int reset(HANDLE handle = INVALID_HANDLE_VALUE) noexcept;

    private:
        HANDLE handle_{INVALID_HANDLE_VALUE};
    };

}
