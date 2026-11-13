#include "win/file.hpp"
#include <exception>
#include <limits>

namespace core::win {
    namespace {

        bool valid_handle(HANDLE handle) noexcept {
            return handle != nullptr &&
                   handle != INVALID_HANDLE_VALUE;
        }

        bool valid_io(
            HANDLE handle,
            const void* buffer,
            std::size_t count
        ) noexcept {
            if (!valid_handle(handle)) {
                ::SetLastError(ERROR_INVALID_HANDLE);
                return false;
            }

            if ((buffer == nullptr && count != 0) || count > std::numeric_limits<DWORD>::max() ) {
                ::SetLastError(ERROR_INVALID_PARAMETER);
                return false;
            }

            return true;
        }

    }


    HANDLE open_file(
        const wchar_t* path,
        FileAccess      access,
        FileShare       share,
        FileCreation    creation,
        FileOptions     options
    ) noexcept {
        if (path == nullptr) {
            ::SetLastError(ERROR_INVALID_PARAMETER);
            return INVALID_HANDLE_VALUE;
        }

        return ::CreateFileW(
            path,
            static_cast<DWORD>(access),
            static_cast<DWORD>(share),
            nullptr,
            static_cast<DWORD>(creation),
            static_cast<DWORD>(options),
            nullptr
        );
    }

    int close_file(HANDLE handle) noexcept {
        if (!valid_handle(handle)) {
            ::SetLastError(ERROR_INVALID_HANDLE);
            return -1;
        }

        return ::CloseHandle(handle) ? 0 : -1;
    }

    std::int64_t read_file(
        HANDLE      handle,
        void*       buffer,
        std::size_t count
    ) noexcept {
        if (!valid_io(handle, buffer, count)) {
            return -1;
        }

        if (count == 0) {
            return 0;
        }

        DWORD bytes_read = 0;

        if (!::ReadFile(
            handle,
            buffer,
            static_cast<DWORD>(count),
            &bytes_read,
            nullptr
        )) {
            return -1;
        }

        return static_cast<std::int64_t>(bytes_read);
    }

    std::int64_t write_file(
        HANDLE      handle,
        const void* buffer,
        std::size_t count
    ) noexcept {
        if (!valid_io(handle, buffer, count)) {
            return -1;
        }

        if (count == 0) {
            return 0;
        }

        DWORD bytes_written = 0;

        if (!::WriteFile(
            handle,
            buffer,
            static_cast<DWORD>(count),
            &bytes_written,
            nullptr
        )) {
            return -1;
        }

        return static_cast<std::int64_t>(bytes_written);
    }

    std::int64_t read_at(
        HANDLE       handle,
        void*        buffer,
        std::size_t  count,
        std::int64_t offset
    ) noexcept {
        if (!valid_io(handle, buffer, count)) {
            return -1;
        }

        if (offset < 0) {
            ::SetLastError(ERROR_INVALID_PARAMETER);
            return -1;
        }

        if (count == 0) {
            return 0;
        }

        LARGE_INTEGER zero{};
        LARGE_INTEGER previous{};

        if (!::SetFilePointerEx(
            handle,
            zero,
            &previous,
            FILE_CURRENT
        )) {
            return -1;
        }

        OVERLAPPED overlapped{};

        const auto position = static_cast<std::uint64_t>(offset);

        overlapped.Offset = static_cast<DWORD>(position);
        overlapped.OffsetHigh = static_cast<DWORD>(position >> 32);

        DWORD bytes_read = 0;

        const BOOL success = ::ReadFile(
            handle,
            buffer,
            static_cast<DWORD>(count),
            &bytes_read,
            &overlapped
        );

        const DWORD error = success ? ERROR_SUCCESS : ::GetLastError();

        if (!::SetFilePointerEx(
            handle,
            previous,
            nullptr,
            FILE_BEGIN
        )) {
            return -1;
        }

        if (!success) {
            if (error == ERROR_HANDLE_EOF) {
                return 0;
            }

            ::SetLastError(error);
            return -1;
        }

        return static_cast<std::int64_t>(bytes_read);
    }

    std::int64_t write_at(
        HANDLE       handle,
        const void*  buffer,
        std::size_t  count,
        std::int64_t offset
    ) noexcept {
        if (!valid_io(handle, buffer, count)) {
            return -1;
        }

        if (offset < 0) {
            ::SetLastError(ERROR_INVALID_PARAMETER);
            return -1;
        }

        if (count == 0) {
            return 0;
        }

        LARGE_INTEGER zero{};
        LARGE_INTEGER previous{};

        if (!::SetFilePointerEx(
            handle,
            zero,
            &previous,
            FILE_CURRENT
        )) {
            return -1;
        }

        OVERLAPPED overlapped{};

        const auto position = static_cast<std::uint64_t>(offset);

        overlapped.Offset = static_cast<DWORD>(position);
        overlapped.OffsetHigh = static_cast<DWORD>(position >> 32);

        DWORD bytes_written = 0;

        const BOOL success = ::WriteFile(
            handle,
            buffer,
            static_cast<DWORD>(count),
            &bytes_written,
            &overlapped
        );

        const DWORD error = success ? ERROR_SUCCESS : ::GetLastError();

        if (!::SetFilePointerEx(
            handle,
            previous,
            nullptr,
            FILE_BEGIN
        )) {
            return -1;
        }

        if (!success) {
            ::SetLastError(error);
            return -1;
        }

        return static_cast<std::int64_t>(bytes_written);
    }

    std::int64_t seek_file(
        HANDLE       handle,
        std::int64_t offset,
        SeekOrigin   origin
    ) noexcept {
        if (!valid_handle(handle)) {
            ::SetLastError(ERROR_INVALID_HANDLE);
            return -1;
        }

        LARGE_INTEGER distance{};
        LARGE_INTEGER position{};

        distance.QuadPart = offset;

        if (!::SetFilePointerEx(
            handle,
            distance,
            &position,
            static_cast<DWORD>(origin)
        )) {
            return -1;
        }

        return position.QuadPart;
    }

    std::int64_t file_size(HANDLE handle) noexcept {
        if (!valid_handle(handle)) {
            ::SetLastError(ERROR_INVALID_HANDLE);
            return -1;
        }

        LARGE_INTEGER size{};

        if (!::GetFileSizeEx(handle, &size)) {
            return -1;
        }

        return size.QuadPart;
    }

    int truncate_file(
        HANDLE       handle,
        std::int64_t size
    ) noexcept {
        if (!valid_handle(handle)) {
            ::SetLastError(ERROR_INVALID_HANDLE);
            return -1;
        }

        if (size < 0) {
            ::SetLastError(ERROR_INVALID_PARAMETER);
            return -1;
        }

        LARGE_INTEGER zero{};
        LARGE_INTEGER previous{};

        if (!::SetFilePointerEx(
            handle,
            zero,
            &previous,
            FILE_CURRENT
        )) {
            return -1;
        }

        LARGE_INTEGER position{};
        position.QuadPart = size;

        if (!::SetFilePointerEx(
            handle,
            position,
            nullptr,
            FILE_BEGIN
        )) {
            return -1;
        }

        const BOOL success = ::SetEndOfFile(handle);
        const DWORD error = success ? ERROR_SUCCESS : ::GetLastError();

        if (!::SetFilePointerEx(
            handle,
            previous,
            nullptr,
            FILE_BEGIN
        )) {
            return -1;
        }

        if (!success) {
            ::SetLastError(error);
            return -1;
        }

        return 0;
    }

    int sync_file(HANDLE handle) noexcept {
        if (!valid_handle(handle)) {
            ::SetLastError(ERROR_INVALID_HANDLE);
            return -1;
        }

        return ::FlushFileBuffers(handle) ? 0 : -1;
    }
}

namespace core::win {

    File::File(HANDLE handle) noexcept {
        handle_ = valid_handle(handle)
            ? handle
            : INVALID_HANDLE_VALUE;
    }

    File::~File() {
        if (valid()) {
            close_file(handle_);
        }
    }

    File::File(File&& other) noexcept {
        handle_ = other.handle_;
        other.handle_ = INVALID_HANDLE_VALUE;
    }

    File& File::operator=(File&& other) noexcept {
        if (this == &other) {
            return *this;
        }

        if (valid() && other.valid() && handle_ == other.handle_) {
            std::terminate();
        }

        if (valid()) {
            if (close_file(handle_) == -1) {
                std::terminate();
            }
        }

        handle_ = other.handle_;
        other.handle_ = INVALID_HANDLE_VALUE;

        return *this;
    }

    int File::open(
        const wchar_t* path,
        FileAccess      access,
        FileShare       share,
        FileCreation    creation,
        FileOptions     options
    ) noexcept {
        if (valid()) {
            ::SetLastError(ERROR_BUSY);
            return -1;
        }

        HANDLE handle = open_file(
            path,
            access,
            share,
            creation,
            options
        );

        if (!valid_handle(handle)) {
            return -1;
        }

        handle_ = handle;
        return 0;
    }

    int File::close() noexcept {
        if (!valid()) {
            return 0;
        }

        if (close_file(handle_) == -1) {
            return -1;
        }

        handle_ = INVALID_HANDLE_VALUE;
        return 0;
    }

    std::int64_t File::read(
        void*       buffer,
        std::size_t count
    ) noexcept {
        return read_file(handle_, buffer, count);
    }

    std::int64_t File::write(
        const void* buffer,
        std::size_t count
    ) noexcept {
        return write_file(handle_, buffer, count);
    }

    std::int64_t File::read_at(
        void*        buffer,
        std::size_t  count,
        std::int64_t offset
    ) noexcept {
        return core::win::read_at(
            handle_,
            buffer,
            count,
            offset
        );
    }

    std::int64_t File::write_at(
        const void*  buffer,
        std::size_t  count,
        std::int64_t offset
    ) noexcept {
        return core::win::write_at(
            handle_,
            buffer,
            count,
            offset
        );
    }

    std::int64_t File::seek(
        std::int64_t offset,
        SeekOrigin   origin
    ) noexcept {
        return seek_file(
            handle_,
            offset,
            origin
        );
    }

    std::int64_t File::size() const noexcept {
        return file_size(handle_);
    }

    int File::truncate(std::int64_t size) noexcept {
        return truncate_file(handle_, size);
    }

    int File::sync() noexcept {
        return sync_file(handle_);
    }

    bool File::valid() const noexcept {
        return valid_handle(handle_);
    }

    HANDLE File::native_handle() const noexcept {
        return handle_;
    }

    HANDLE File::release() noexcept {
        HANDLE handle = handle_;
        handle_ = INVALID_HANDLE_VALUE;

        return handle;
    }

    int File::reset(HANDLE handle) noexcept {
        if (handle_ == handle) {
            return 0;
        }

        if (valid()) {
            if (close_file(handle_) == -1) {
                return -1;
            }
        }

        handle_ = valid_handle(handle)
            ? handle
            : INVALID_HANDLE_VALUE;

        return 0;
    }
}