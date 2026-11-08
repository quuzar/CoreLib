#include "psx/file.hpp"
#include <sys/stat.h>
#include <stdexcept>

namespace core::psx {
    int open_file(
        const char* path,
        OpenMode    mode,
        OpenFlag    flags,
        mode_t      permissions
    ) noexcept {
        const int native_flags = static_cast<int>(flags) | static_cast<int>(mode);

        return ::open(
            path,
            native_flags,
            permissions
        );
    }

    int close_file(
        const int fd
    ) noexcept {
        return ::close(fd);
    }

    ssize_t read_file(
        const int         fd,
        void*             buffer,
        const std::size_t count
    ) noexcept {
        return ::read(
            fd,
            buffer,
            count
        );
    }

    ssize_t write_file(
        const int         fd,
        const void*       buffer,
        const std::size_t count
    ) noexcept {
        return ::write(
            fd,
            buffer,
            count
        );
    }

    off_t seek_file(
        const int        fd,
        const off_t      offset,
        const SeekOrigin origin
    ) noexcept {
        return ::lseek(
            fd,
            offset,
            static_cast<int>(origin)
        );
    }

    ssize_t read_at(
        const int         fd,
        void*             buffer,
        const std::size_t count,
        const off_t       offset
    ) noexcept {
        return ::pread(
            fd,
            buffer,
            count,
            offset
        );
    }

    ssize_t write_at(
        const int         fd,
        const void*       buffer,
        const std::size_t count,
        const off_t       offset
    ) noexcept {
        return ::pwrite(
            fd,
            buffer,
            count,
            offset
        );
    }

    off_t file_size(
        const int fd
    ) noexcept {
        struct stat info{};

        if (::fstat(fd, &info) == -1) {
            return -1;
        }

        return info.st_size;
    }

    int truncate_file(
        const int   fd,
        const off_t size
    ) noexcept {
        return ::ftruncate(
            fd,
            size
        );
    }

    int sync_file(
        const int fd
    ) noexcept {
        return ::fsync(fd);
    }
};

namespace core::psx {
    File::File(const int fd) noexcept {
        fd_ = fd;
    }

    File::File(File &&other) noexcept {
        fd_ = other.fd_;
        other.fd_ = -1;
    }

    File::~File() {
        if (fd_ != -1) {
            close_file(fd_);
        }
    }

    File& File::operator=(File &&other) noexcept {
        const auto fd_other = other.fd_;

        if (&other == this) {
            return *this;
        }

        if (fd_ != -1 && fd_other == fd_) {
            std::terminate();
        }

        if (fd_ != -1) {
            close_file(fd_);
        }

        fd_ = fd_other;
        other.fd_ = -1;

        return *this;
    }

    int File::open(
        const char* path,
        OpenMode    mode,
        OpenFlag    flags,
        mode_t      permissions
    ) noexcept {
        if (fd_ != -1) {
            errno = EBUSY;
            return -1;
        }

        fd_ = open_file(
            path,
            mode,
            flags,
            permissions
        );

        if (fd_ != -1) {
            return 0;
        }
        return -1;
    }

    int File::close() noexcept {
        if (fd_ == -1) {
            return 0;
        }

        const int fd = fd_;
        fd_ = -1;

        return close_file(fd);
    }

    ssize_t File::read(
        void*       buffer,
        std::size_t count
    ) noexcept {
        if (fd_ == -1) {
            errno = EBADF;
            return -1;
        }

        if (buffer == nullptr && count != 0) {
            errno = EINVAL;
            return -1;
        }

        return read_file(
            fd_,
            buffer,
            count
        );
    }

    ssize_t File::write(
        const void* buffer,
        std::size_t count
    ) noexcept {
        if (fd_ == -1) {
            errno = EBADF;
            return -1;
        }

        if (buffer == nullptr && count != 0) {
            errno = EINVAL;
            return -1;
        }

        return write_file(
            fd_,
            buffer,
            count
        );
    }

    ssize_t File::read_at(
        void*       buffer,
        std::size_t count,
        off_t       offset
    ) noexcept {
        if (fd_ == -1) {
            errno = EBADF;
            return -1;
        }

        if (buffer == nullptr && count != 0) {
            errno = EINVAL;
            return -1;
        }

        return ::core::psx::read_at(
            fd_,
            buffer,
            count,
            offset
        );
    }

    ssize_t File::write_at(
        const void* buffer,
        std::size_t count,
        off_t       offset
    ) noexcept {
        if (fd_ == -1) {
            errno = EBADF;
            return -1;
        }

        if (buffer == nullptr && count != 0) {
            errno = EINVAL;
            return -1;
        }


        return ::core::psx::write_at(
            fd_,
            buffer,
            count,
            offset
        );
    }

    off_t File::seek(
        off_t       offset,
        SeekOrigin  origin
    ) noexcept {
        if (fd_ == -1) {
            errno = EBADF;
            return -1;
        }

        return seek_file(
            fd_,
            offset,
            origin
        );
    }

    off_t File::size() const noexcept {
        if (fd_ == -1) {
            errno = EBADF;
            return -1;
        }

       return file_size(fd_);
    }

    int File::truncate(off_t size) noexcept {
        if (fd_ == -1) {
            errno = EBADF;
            return -1;
        }

        return truncate_file(
            fd_,
            size
        );
    }

    int File::sync() noexcept {
        if (fd_ == -1) {
            errno = EBADF;
            return -1;
        }

        return sync_file(fd_);
    }

    bool File::valid() const noexcept {
        return fd_ != -1;
    }

    int File::native_handle() const noexcept {
        return fd_;
    }

    int File::release() noexcept {
        const int fd = fd_;
        fd_ = -1;
        return fd;
    }

    int File::reset(int fd) noexcept {
        if (fd_ == fd) {
            return 0;
        }

        const int old_fd = fd_;
        fd_ = fd;

        if (old_fd != -1) {
            return close_file(old_fd);
        }

        return 0;
    }
}
