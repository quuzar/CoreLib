#include <catch2/catch_test_macros.hpp>
#include <psx/mapping.hpp>
#include <psx/memory.hpp>

#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <sys/mman.h>
#include <type_traits>
#include <unistd.h>
#include <utility>

using namespace core::psx;

namespace {

constexpr auto read_write = static_cast<PageProtection>(PROT_READ | PROT_WRITE);
constexpr auto shared = static_cast<MappingMode>(MAP_SHARED);
constexpr auto private_mapping = static_cast<MappingMode>(MAP_PRIVATE);
constexpr auto sync_flag = static_cast<SyncFlag>(MS_SYNC);

class TemporaryFile {
public:
    explicit TemporaryFile(std::size_t size) {
        char name[] = "/tmp/corelib-mapping-XXXXXX";
        fd_ = ::mkstemp(name);
        if (fd_ == -1)
            throw std::runtime_error("mkstemp failed");
        path_ = name;
        if (::ftruncate(fd_, static_cast<off_t>(size)) == -1) {
            ::close(fd_);
            ::unlink(path_.c_str());
            throw std::runtime_error("ftruncate failed");
        }
    }

    ~TemporaryFile() {
        ::close(fd_);
        ::unlink(path_.c_str());
    }

    TemporaryFile(const TemporaryFile&) = delete;
    TemporaryFile& operator=(const TemporaryFile&) = delete;

    [[nodiscard]] int fd() const noexcept { return fd_; }

private:
    int fd_{-1};
    std::string path_;
};

struct RawMapping {
    void* address{MAP_FAILED};
    std::size_t size{};

    ~RawMapping() {
        if (address != MAP_FAILED && address != nullptr)
            unmap_file(address, size);
    }
};

} // namespace

TEST_CASE("Mapping: raw shared map, flush and unmap", "[psx][mapping][raw]") {
    const auto page = page_size();
    REQUIRE(page > 0);
    TemporaryFile file(page);

    RawMapping mapping{map_file(file.fd(), page, 0, read_write, shared), page};
    REQUIRE(mapping.address != MAP_FAILED);
    auto* bytes = static_cast<std::byte*>(mapping.address);
    bytes[0] = std::byte{0x5A};
    bytes[page - 1] = std::byte{0x6B};

    REQUIRE(flush_mapping(mapping.address, page, sync_flag) == 0);
    std::byte first{};
    std::byte last{};
    REQUIRE(::pread(file.fd(), &first, 1, 0) == 1);
    REQUIRE(::pread(file.fd(), &last, 1, static_cast<off_t>(page - 1)) == 1);
    CHECK(first == std::byte{0x5A});
    CHECK(last == std::byte{0x6B});

    REQUIRE(unmap_file(mapping.address, page) == 0);
    mapping.address = MAP_FAILED;
}

TEST_CASE("Mapping: private changes do not modify the file", "[psx][mapping][raw]") {
    const auto page = page_size();
    REQUIRE(page > 0);
    TemporaryFile file(page);
    const char initial = 'A';
    REQUIRE(::pwrite(file.fd(), &initial, 1, 0) == 1);

    RawMapping mapping{map_file(file.fd(), page, 0, read_write, private_mapping), page};
    REQUIRE(mapping.address != MAP_FAILED);
    static_cast<char*>(mapping.address)[0] = 'B';

    char stored{};
    REQUIRE(::pread(file.fd(), &stored, 1, 0) == 1);
    CHECK(stored == 'A');
}

TEST_CASE("Mapping: raw invalid descriptor is rejected", "[psx][mapping][errors]") {
    const auto page = page_size();
    REQUIRE(page > 0);
    errno = 0;
    CHECK(map_file(-1, page, 0, read_write, shared) == MAP_FAILED);
    CHECK(errno == EBADF);
}

TEST_CASE("Mapping: default region rejects invalid operations", "[psx][mapping][errors]") {
    MappedRegion region;
    CHECK_FALSE(region.valid());
    CHECK(region.data() == nullptr);
    CHECK(region.size() == 0);
    CHECK(region.reset() == 0);

    errno = 0;
    CHECK(region.flush(sync_flag) == -1);
    CHECK(errno == EINVAL);

    errno = 0;
    CHECK(region.map(-1, 0, 0, read_write, shared) == -1);
    CHECK(errno == EINVAL);

    errno = 0;
    CHECK(region.map(-1, 1, -1, read_write, shared) == -1);
    CHECK(errno == EINVAL);
}

TEST_CASE("Mapping: region supports unaligned file offsets", "[psx][mapping][class]") {
    const auto page = page_size();
    REQUIRE(page > 0);
    TemporaryFile file(page * 2);

    constexpr off_t offset = 13;
    constexpr std::size_t length = 64;
    constexpr std::array<char, 4> message{'T', 'e', 's', 't'};
    REQUIRE(::pwrite(file.fd(), message.data(), message.size(), offset) ==
            static_cast<ssize_t>(message.size()));

    MappedRegion region;
    REQUIRE(region.map(file.fd(), length, offset, read_write, shared) == 0);
    REQUIRE(region.valid());
    CHECK(region.size() == length);
    REQUIRE(region.data() != nullptr);
    CHECK(std::memcmp(region.data(), message.data(), message.size()) == 0);

    auto* bytes = static_cast<char*>(region.data());
    bytes[0] = 'B';
    REQUIRE(region.flush(sync_flag) == 0);

    char stored{};
    REQUIRE(::pread(file.fd(), &stored, 1, offset) == 1);
    CHECK(stored == 'B');

    errno = 0;
    CHECK(region.map(file.fd(), length, 0, read_write, shared) == -1);
    CHECK(errno == EBUSY);
    REQUIRE(region.reset() == 0);
    CHECK_FALSE(region.valid());
    CHECK(region.reset() == 0);
}

TEST_CASE("Mapping: region can be reused after reset", "[psx][mapping][lifecycle]") {
    const auto page = page_size();
    REQUIRE(page > 0);
    TemporaryFile file(page);

    MappedRegion region;
    REQUIRE(region.map(file.fd(), page, 0, read_write, shared) == 0);
    REQUIRE(region.reset() == 0);
    REQUIRE(region.map(file.fd(), page, 0, read_write, shared) == 0);
    REQUIRE(region.reset() == 0);
}

TEST_CASE("Mapping: move constructor transfers mapping", "[psx][mapping][move]") {
    const auto page = page_size();
    REQUIRE(page > 0);
    TemporaryFile file(page);

    MappedRegion source;
    REQUIRE(source.map(file.fd(), page, 0, read_write, shared) == 0);
    void* address = source.data();

    MappedRegion destination(std::move(source));
    CHECK_FALSE(source.valid());
    CHECK(destination.valid());
    CHECK(destination.data() == address);
    CHECK(destination.size() == page);
    REQUIRE(destination.reset() == 0);
}

TEST_CASE("Mapping: move assignment replaces an existing mapping", "[psx][mapping][move]") {
    const auto page = page_size();
    REQUIRE(page > 0);
    TemporaryFile file(page * 2);

    MappedRegion source;
    MappedRegion destination;
    REQUIRE(source.map(file.fd(), page, 0, read_write, shared) == 0);
    REQUIRE(destination.map(file.fd(), page, page, read_write, shared) == 0);
    void* address = source.data();

    destination = std::move(source);
    CHECK_FALSE(source.valid());
    CHECK(destination.valid());
    CHECK(destination.data() == address);

    destination = std::move(destination);
    CHECK(destination.data() == address);
    REQUIRE(destination.reset() == 0);
}

TEST_CASE("Mapping: release transfers unmap responsibility", "[psx][mapping][ownership]") {
    const auto page = page_size();
    REQUIRE(page > 0);
    TemporaryFile file(page);

    MappedRegion region;
    REQUIRE(region.map(file.fd(), 64, 7, read_write, shared) == 0);

    const MappingBlock block = region.release();
    CHECK_FALSE(region.valid());
    REQUIRE(block.address != nullptr);
    CHECK(block.size == 64);
    CHECK(block.offset == 7);
    CHECK(block.mapped_size == 71);
    REQUIRE(unmap_file(block.address, block.mapped_size) == 0);
}

TEST_CASE("Mapping: move-only type", "[psx][mapping][types]") {
    STATIC_REQUIRE_FALSE(std::is_copy_constructible_v<MappedRegion>);
    STATIC_REQUIRE_FALSE(std::is_copy_assignable_v<MappedRegion>);
    STATIC_REQUIRE(std::is_move_constructible_v<MappedRegion>);
    STATIC_REQUIRE(std::is_move_assignable_v<MappedRegion>);
}
