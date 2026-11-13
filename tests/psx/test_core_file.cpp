#include <catch2/catch_test_macros.hpp>
#include <psx/file.hpp>

#include <array>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <unistd.h>
#include <utility>

using namespace core::psx;

namespace {

class TemporaryFile {
public:
    TemporaryFile() {
        char name[] = "/tmp/corelib-file-XXXXXX";
        const int fd = ::mkstemp(name);
        if (fd == -1) {
            throw std::runtime_error("mkstemp failed");
        }
        path_ = name;
        ::close(fd);
    }

    ~TemporaryFile() { ::unlink(path_.c_str()); }

    TemporaryFile(const TemporaryFile&) = delete;
    TemporaryFile& operator=(const TemporaryFile&) = delete;

    [[nodiscard]] const char* path() const noexcept { return path_.c_str(); }
    void remove() const { ::unlink(path_.c_str()); }

private:
    std::string path_;
};

struct ScopedFd {
    int fd{-1};
    ~ScopedFd() { if (fd >= 0) ::close(fd); }
};

constexpr auto begin = static_cast<SeekOrigin>(SEEK_SET);
constexpr auto current = static_cast<SeekOrigin>(SEEK_CUR);

} // namespace

TEST_CASE("File: raw open and close", "[psx][file][raw]") {
    TemporaryFile temp;
    ScopedFd handle{open_file(temp.path(), OpenMode::ReadWrite,
                              OpenFlag::Create | OpenFlag::Truncate)};
    REQUIRE(handle.fd >= 0);
    REQUIRE(close_file(handle.fd) == 0);
    handle.fd = -1;
}

TEST_CASE("File: raw write, read and seek", "[psx][file][raw]") {
    TemporaryFile temp;
    ScopedFd handle{open_file(temp.path(), OpenMode::ReadWrite, OpenFlag::Truncate)};
    REQUIRE(handle.fd >= 0);

    constexpr std::string_view message = "CoreLib POSIX";
    REQUIRE(write_file(handle.fd, message.data(), message.size()) ==
            static_cast<ssize_t>(message.size()));
    CHECK(file_size(handle.fd) == static_cast<off_t>(message.size()));

    REQUIRE(seek_file(handle.fd, 0, begin) == 0);
    std::array<char, 32> buffer{};
    REQUIRE(read_file(handle.fd, buffer.data(), message.size()) ==
            static_cast<ssize_t>(message.size()));
    CHECK(std::memcmp(buffer.data(), message.data(), message.size()) == 0);
    CHECK(read_file(handle.fd, buffer.data(), buffer.size()) == 0);
}

TEST_CASE("File: raw positioned IO does not move cursor", "[psx][file][raw]") {
    TemporaryFile temp;
    ScopedFd handle{open_file(temp.path(), OpenMode::ReadWrite, OpenFlag::Truncate)};
    REQUIRE(handle.fd >= 0);

    constexpr std::string_view initial = "0123456789";
    REQUIRE(write_file(handle.fd, initial.data(), initial.size()) == 10);
    REQUIRE(seek_file(handle.fd, 3, begin) == 3);

    constexpr std::string_view patch = "AB";
    REQUIRE(write_at(handle.fd, patch.data(), patch.size(), 6) == 2);
    std::array<char, 2> buffer{};
    REQUIRE(read_at(handle.fd, buffer.data(), buffer.size(), 6) == 2);
    CHECK(std::string_view(buffer.data(), buffer.size()) == patch);
    CHECK(seek_file(handle.fd, 0, current) == 3);

    REQUIRE(read_at(handle.fd, buffer.data(), buffer.size(), 0) == 2);
    CHECK(std::string_view(buffer.data(), buffer.size()) == "01");
}

TEST_CASE("File: raw truncate, extend and sync", "[psx][file][raw]") {
    TemporaryFile temp;
    ScopedFd handle{open_file(temp.path(), OpenMode::ReadWrite, OpenFlag::Truncate)};
    REQUIRE(handle.fd >= 0);

    REQUIRE(truncate_file(handle.fd, 4096) == 0);
    CHECK(file_size(handle.fd) == 4096);
    REQUIRE(truncate_file(handle.fd, 12) == 0);
    CHECK(file_size(handle.fd) == 12);
    CHECK(sync_file(handle.fd) == 0);
}

TEST_CASE("File: raw invalid descriptors", "[psx][file][errors]") {
    std::array<char, 8> buffer{};

    errno = 0;
    CHECK(close_file(-1) == -1);
    CHECK(errno == EBADF);
    errno = 0;
    CHECK(read_file(-1, buffer.data(), buffer.size()) == -1);
    CHECK(errno == EBADF);
    errno = 0;
    CHECK(write_file(-1, buffer.data(), buffer.size()) == -1);
    CHECK(errno == EBADF);
    errno = 0;
    CHECK(read_at(-1, buffer.data(), buffer.size(), 0) == -1);
    CHECK(errno == EBADF);
    errno = 0;
    CHECK(write_at(-1, buffer.data(), buffer.size(), 0) == -1);
    CHECK(errno == EBADF);
    errno = 0;
    CHECK(file_size(-1) == -1);
    CHECK(errno == EBADF);
    errno = 0;
    CHECK(truncate_file(-1, 0) == -1);
    CHECK(errno == EBADF);
    errno = 0;
    CHECK(sync_file(-1) == -1);
    CHECK(errno == EBADF);
    errno = 0;
    CHECK(seek_file(-1, 0, begin) == -1);
    CHECK(errno == EBADF);
}

TEST_CASE("File: opening a missing file fails", "[psx][file][errors]") {
    TemporaryFile temp;
    temp.remove();
    errno = 0;
    CHECK(open_file(temp.path(), OpenMode::ReadOnly, OpenFlag::None) == -1);
    CHECK(errno == ENOENT);
}

TEST_CASE("File: default object and invalid operations", "[psx][file][errors]") {
    File file;
    CHECK_FALSE(file.valid());
    CHECK(file.native_handle() == -1);

    std::array<char, 8> buffer{};
    errno = 0;
    CHECK(file.read(buffer.data(), buffer.size()) == -1);
    CHECK(errno == EBADF);
    errno = 0;
    CHECK(file.write(buffer.data(), buffer.size()) == -1);
    CHECK(errno == EBADF);
    CHECK(file.close() == 0);
}

TEST_CASE("File: open, close and reinitialize", "[psx][file][lifecycle]") {
    TemporaryFile temp;
    File file;
    REQUIRE(file.open(temp.path(), OpenMode::ReadWrite, OpenFlag::Truncate) == 0);
    CHECK(file.valid());
    CHECK(file.native_handle() >= 0);

    errno = 0;
    CHECK(file.open(temp.path(), OpenMode::ReadWrite, OpenFlag::None) == -1);
    CHECK(errno == EBUSY);
    CHECK(file.valid());

    REQUIRE(file.close() == 0);
    CHECK_FALSE(file.valid());
    CHECK(file.close() == 0);
    REQUIRE(file.open(temp.path(), OpenMode::ReadWrite, OpenFlag::None) == 0);
    CHECK(file.valid());
}

TEST_CASE("File: class read, write, size and sync", "[psx][file][class]") {
    TemporaryFile temp;
    File file;
    REQUIRE(file.open(temp.path(), OpenMode::ReadWrite, OpenFlag::Truncate) == 0);

    constexpr std::string_view message = "Hello CoreLib";
    REQUIRE(file.write(message.data(), message.size()) ==
            static_cast<ssize_t>(message.size()));
    CHECK(file.size() == static_cast<off_t>(message.size()));
    CHECK(file.sync() == 0);

    REQUIRE(file.seek(0, begin) == 0);
    std::array<char, 32> buffer{};
    REQUIRE(file.read(buffer.data(), message.size()) ==
            static_cast<ssize_t>(message.size()));
    CHECK(std::string_view(buffer.data(), message.size()) == message);
    CHECK(file.read(buffer.data(), buffer.size()) == 0);
}

TEST_CASE("File: class read_at and write_at preserve position", "[psx][file][class]") {
    TemporaryFile temp;
    File file;
    REQUIRE(file.open(temp.path(), OpenMode::ReadWrite, OpenFlag::Truncate) == 0);

    constexpr std::string_view initial = "0123456789";
    REQUIRE(file.write(initial.data(), initial.size()) == 10);
    REQUIRE(file.seek(4, begin) == 4);
    constexpr std::string_view patch = "xy";
    REQUIRE(file.write_at(patch.data(), patch.size(), 6) == 2);

    std::array<char, 2> buffer{};
    REQUIRE(file.read_at(buffer.data(), buffer.size(), 6) == 2);
    CHECK(std::string_view(buffer.data(), buffer.size()) == patch);
    CHECK(file.seek(0, current) == 4);
}

TEST_CASE("File: class truncate", "[psx][file][class]") {
    TemporaryFile temp;
    File file;
    REQUIRE(file.open(temp.path(), OpenMode::ReadWrite, OpenFlag::Truncate) == 0);
    REQUIRE(file.truncate(8192) == 0);
    CHECK(file.size() == 8192);
    REQUIRE(file.truncate(64) == 0);
    CHECK(file.size() == 64);
}

TEST_CASE("File: reject null buffers with positive length", "[psx][file][errors]") {
    TemporaryFile temp;
    File file;
    REQUIRE(file.open(temp.path(), OpenMode::ReadWrite, OpenFlag::Truncate) == 0);
    errno = 0;
    CHECK(file.read(nullptr, 1) == -1);
    CHECK(errno == EINVAL);
    errno = 0;
    CHECK(file.write(nullptr, 1) == -1);
    CHECK(errno == EINVAL);
}

TEST_CASE("File: move constructor transfers ownership", "[psx][file][move]") {
    TemporaryFile temp;
    File source;
    REQUIRE(source.open(temp.path(), OpenMode::ReadWrite, OpenFlag::None) == 0);
    const int fd = source.native_handle();

    File destination(std::move(source));
    CHECK_FALSE(source.valid());
    CHECK(destination.valid());
    CHECK(destination.native_handle() == fd);
    REQUIRE(destination.close() == 0);
}

TEST_CASE("File: move assignment closes old descriptor", "[psx][file][move]") {
    TemporaryFile first;
    TemporaryFile second;
    File destination;
    File source;
    REQUIRE(destination.open(first.path(), OpenMode::ReadWrite, OpenFlag::None) == 0);
    REQUIRE(source.open(second.path(), OpenMode::ReadWrite, OpenFlag::None) == 0);

    const int old_fd = destination.native_handle();
    const int new_fd = source.native_handle();
    REQUIRE(old_fd != new_fd);

    destination = std::move(source);
    CHECK_FALSE(source.valid());
    CHECK(destination.native_handle() == new_fd);

    errno = 0;
    CHECK(::fcntl(old_fd, F_GETFD) == -1);
    CHECK(errno == EBADF);
    REQUIRE(destination.close() == 0);
}

TEST_CASE("File: release transfers descriptor ownership", "[psx][file][ownership]") {
    TemporaryFile temp;
    File file;
    REQUIRE(file.open(temp.path(), OpenMode::ReadWrite, OpenFlag::None) == 0);
    const int fd = file.native_handle();

    const int released = file.release();
    CHECK(released == fd);
    CHECK_FALSE(file.valid());
    CHECK(file.native_handle() == -1);
    REQUIRE(::fcntl(released, F_GETFD) != -1);
    REQUIRE(close_file(released) == 0);
}

TEST_CASE("File: destructor closes owned descriptor", "[psx][file][ownership]") {
    TemporaryFile temp;
    int fd = -1;
    {
        File file;
        REQUIRE(file.open(temp.path(), OpenMode::ReadWrite, OpenFlag::None) == 0);
        fd = file.native_handle();
    }

    errno = 0;
    CHECK(::fcntl(fd, F_GETFD) == -1);
    CHECK(errno == EBADF);
}

TEST_CASE("File: readonly opening and reading", "[psx][file][class]") {
    TemporaryFile temp;
    {
        File writer;
        REQUIRE(writer.open(temp.path(), OpenMode::ReadWrite, OpenFlag::Truncate) == 0);
        constexpr std::string_view message = "Read only";
        REQUIRE(writer.write(message.data(), message.size()) ==
                static_cast<ssize_t>(message.size()));
    }

    File reader;
    REQUIRE(reader.open(temp.path(), OpenMode::ReadOnly, OpenFlag::None) == 0);
    std::array<char, 16> buffer{};
    REQUIRE(reader.read(buffer.data(), 9) == 9);
    CHECK(std::string_view(buffer.data(), 9) == "Read only");
    errno = 0;
    CHECK(reader.write("x", 1) == -1);
    CHECK(errno == EBADF);
}

TEST_CASE("File: copy prohibited, move supported", "[psx][file][types]") {
    STATIC_REQUIRE_FALSE(std::is_copy_constructible_v<File>);
    STATIC_REQUIRE_FALSE(std::is_copy_assignable_v<File>);
    STATIC_REQUIRE(std::is_move_constructible_v<File>);
    STATIC_REQUIRE(std::is_move_assignable_v<File>);
}
