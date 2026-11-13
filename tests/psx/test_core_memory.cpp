#include <catch2/catch_test_macros.hpp>
#include <psx/memory.hpp>

#include <cerrno>
#include <cstddef>
#include <sys/mman.h>
#include <type_traits>
#include <utility>

using namespace core::psx;

namespace {

constexpr auto read_write = static_cast<PageProtection>(PROT_READ | PROT_WRITE);
constexpr auto read_only = static_cast<PageProtection>(PROT_READ);

struct RawMemory {
    void* address{MAP_FAILED};
    std::size_t size{};

    ~RawMemory() {
        if (address != MAP_FAILED && address != nullptr)
            release_memory(address, size);
    }
};

} // namespace

TEST_CASE("Memory: page size is available", "[psx][memory][raw]") {
    CHECK(page_size() > 0);
}

TEST_CASE("Memory: raw allocation, protection and release", "[psx][memory][raw]") {
    const auto page = page_size();
    REQUIRE(page > 0);

    RawMemory memory{allocate_memory(page, read_write), page};
    REQUIRE(memory.address != MAP_FAILED);

    auto* bytes = static_cast<std::byte*>(memory.address);
    bytes[0] = std::byte{0x12};
    bytes[page - 1] = std::byte{0x34};

    REQUIRE(protect_memory(memory.address, page, read_only) == 0);
    CHECK(bytes[0] == std::byte{0x12});
    CHECK(bytes[page - 1] == std::byte{0x34});
    REQUIRE(protect_memory(memory.address, page, read_write) == 0);

    REQUIRE(release_memory(memory.address, page) == 0);
    memory.address = MAP_FAILED;
}

TEST_CASE("Memory: raw zero-sized mapping fails", "[psx][memory][errors]") {
    errno = 0;
    CHECK(allocate_memory(0, read_write) == MAP_FAILED);
    CHECK(errno == EINVAL);
}

TEST_CASE("Memory: default region and invalid operations", "[psx][memory][errors]") {
    MemoryRegion region;
    CHECK_FALSE(region.valid());
    CHECK(region.data() == nullptr);
    CHECK(region.size() == 0);
    CHECK(region.reset() == 0);

    errno = 0;
    CHECK(region.allocate(0, read_write) == -1);
    CHECK(errno == EINVAL);

    errno = 0;
    CHECK(region.protect(read_only) == -1);
    CHECK(errno == EINVAL);
}

TEST_CASE("Memory: region allocation, protection and reuse", "[psx][memory][lifecycle]") {
    const auto page = page_size();
    REQUIRE(page > 0);

    MemoryRegion region;
    REQUIRE(region.allocate(page, read_write) == 0);
    REQUIRE(region.valid());
    REQUIRE(region.data() != nullptr);
    CHECK(region.size() == page);

    auto* bytes = static_cast<std::byte*>(region.data());
    bytes[0] = std::byte{0x5A};
    REQUIRE(region.protect(read_only) == 0);
    CHECK(bytes[0] == std::byte{0x5A});
    REQUIRE(region.protect(read_write) == 0);

    errno = 0;
    CHECK(region.allocate(page, read_write) == -1);
    CHECK(errno == EBUSY);

    REQUIRE(region.reset() == 0);
    CHECK_FALSE(region.valid());
    CHECK(region.reset() == 0);
    REQUIRE(region.allocate(page, read_write) == 0);
    REQUIRE(region.reset() == 0);
}

TEST_CASE("Memory: move construction transfers ownership", "[psx][memory][move]") {
    const auto page = page_size();
    REQUIRE(page > 0);

    MemoryRegion source;
    REQUIRE(source.allocate(page, read_write) == 0);
    void* address = source.data();

    MemoryRegion destination(std::move(source));
    CHECK_FALSE(source.valid());
    CHECK(destination.valid());
    CHECK(destination.data() == address);
    CHECK(destination.size() == page);
    REQUIRE(destination.reset() == 0);
}

TEST_CASE("Memory: move assignment replaces the destination region", "[psx][memory][move]") {
    const auto page = page_size();
    REQUIRE(page > 0);

    MemoryRegion source;
    MemoryRegion destination;
    REQUIRE(source.allocate(page, read_write) == 0);
    REQUIRE(destination.allocate(page, read_write) == 0);
    void* transferred = source.data();

    destination = std::move(source);
    CHECK_FALSE(source.valid());
    CHECK(destination.valid());
    CHECK(destination.data() == transferred);

    destination = std::move(destination);
    CHECK(destination.valid());
    CHECK(destination.data() == transferred);
    REQUIRE(destination.reset() == 0);
}

TEST_CASE("Memory: release gives ownership to the caller", "[psx][memory][ownership]") {
    const auto page = page_size();
    REQUIRE(page > 0);

    MemoryRegion region;
    REQUIRE(region.allocate(page, read_write) == 0);

    const MemoryBlock block = region.release();
    REQUIRE(block.address != nullptr);
    CHECK(block.size == page);
    CHECK_FALSE(region.valid());
    CHECK(region.reset() == 0);
    REQUIRE(release_memory(block.address, block.size) == 0);
}

TEST_CASE("Memory: move-only type", "[psx][memory][types]") {
    STATIC_REQUIRE_FALSE(std::is_copy_constructible_v<MemoryRegion>);
    STATIC_REQUIRE_FALSE(std::is_copy_assignable_v<MemoryRegion>);
    STATIC_REQUIRE(std::is_move_constructible_v<MemoryRegion>);
    STATIC_REQUIRE(std::is_move_assignable_v<MemoryRegion>);
}
