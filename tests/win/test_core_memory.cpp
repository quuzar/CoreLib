#include <catch2/catch_test_macros.hpp>
#include <win/memory.hpp>

#include <cstddef>
#include <type_traits>
#include <utility>
#include <windows.h>

using namespace core::win;

namespace {

    class RawMemory {
    public:
        explicit RawMemory(
            std::size_t size,
            PageProtection protection = PageProtection::ReadWrite
        ) noexcept {
            address_ = allocate_memory(size, protection);
        }

        ~RawMemory() {
            if (address_ != nullptr) {
                release_memory(address_);
            }
        }

        RawMemory(const RawMemory&) = delete;
        RawMemory& operator=(const RawMemory&) = delete;

        [[nodiscard]] void* data() const noexcept {
            return address_;
        }

    private:
        void* address_{};
    };

}


TEST_CASE("Memory: type properties", "[win][memory][types]") {
    STATIC_REQUIRE_FALSE(std::is_copy_constructible_v<MemoryRegion>);
    STATIC_REQUIRE_FALSE(std::is_copy_assignable_v<MemoryRegion>);

    STATIC_REQUIRE(std::is_move_constructible_v<MemoryRegion>);
    STATIC_REQUIRE(std::is_move_assignable_v<MemoryRegion>);

    STATIC_REQUIRE(std::is_nothrow_move_constructible_v<MemoryRegion>);
    STATIC_REQUIRE(std::is_nothrow_move_assignable_v<MemoryRegion>);
}


TEST_CASE("Memory: page size", "[win][memory][raw]") {
    SYSTEM_INFO info{};
    ::GetSystemInfo(&info);

    REQUIRE(page_size() > 0);
    CHECK(page_size() == info.dwPageSize);
}


TEST_CASE("Memory: raw allocation", "[win][memory][raw]") {
    const auto size = page_size();

    RawMemory memory(size);

    REQUIRE(memory.data() != nullptr);

    auto* bytes = static_cast<std::byte*>(memory.data());

    bytes[0] = std::byte{0x12};
    bytes[size - 1] = std::byte{0x34};

    CHECK(bytes[0] == std::byte{0x12});
    CHECK(bytes[size - 1] == std::byte{0x34});
}


TEST_CASE("Memory: raw allocation initializes memory to zero", "[win][memory][raw]") {
    const auto size = page_size();

    RawMemory memory(size);

    REQUIRE(memory.data() != nullptr);

    const auto* bytes = static_cast<const std::byte*>(memory.data());

    for (std::size_t i = 0; i < size; ++i) {
        REQUIRE(bytes[i] == std::byte{0});
    }
}


TEST_CASE("Memory: raw memory protection", "[win][memory][raw]") {
    const auto size = page_size();

    RawMemory memory(size);

    REQUIRE(memory.data() != nullptr);

    auto* bytes = static_cast<std::byte*>(memory.data());
    bytes[0] = std::byte{0xAB};

    REQUIRE(protect_memory(
        memory.data(),
        size,
        PageProtection::ReadOnly
    ) == 0);

    MEMORY_BASIC_INFORMATION info{};

    REQUIRE(::VirtualQuery(
        memory.data(),
        &info,
        sizeof(info)
    ) == sizeof(info));

    CHECK(info.State == MEM_COMMIT);
    CHECK(info.Protect == PAGE_READONLY);
    CHECK(bytes[0] == std::byte{0xAB});

    REQUIRE(protect_memory(
        memory.data(),
        size,
        PageProtection::ReadWrite
    ) == 0);

    bytes[0] = std::byte{0xCD};

    CHECK(bytes[0] == std::byte{0xCD});
}


TEST_CASE("Memory: raw allocation with zero size", "[win][memory][errors]") {
    const void* address = allocate_memory(
        0,
        PageProtection::ReadWrite
    );

    const DWORD error = ::GetLastError();

    CHECK(address == nullptr);
    CHECK(error == ERROR_INVALID_PARAMETER);
}


TEST_CASE("Memory: raw invalid protection arguments", "[win][memory][errors]") {
    CHECK(protect_memory(
        nullptr,
        page_size(),
        PageProtection::ReadOnly
    ) == -1);

    CHECK(::GetLastError() == ERROR_INVALID_PARAMETER);

    RawMemory memory(page_size());

    REQUIRE(memory.data() != nullptr);

    CHECK(protect_memory(
        memory.data(),
        0,
        PageProtection::ReadOnly
    ) == -1);

    CHECK(::GetLastError() == ERROR_INVALID_PARAMETER);
}


TEST_CASE("Memory: raw release", "[win][memory][raw]") {
    const auto size = page_size();

    void* address = allocate_memory(
        size,
        PageProtection::ReadWrite
    );

    REQUIRE(address != nullptr);

    REQUIRE(release_memory(address) == 0);

    MEMORY_BASIC_INFORMATION info{};

    REQUIRE(::VirtualQuery(
        address,
        &info,
        sizeof(info)
    ) == sizeof(info));

    CHECK(info.State == MEM_FREE);
}


TEST_CASE("Memory: raw release null address", "[win][memory][errors]") {
    const int result = release_memory(nullptr);
    const DWORD error = ::GetLastError();

    CHECK(result == -1);
    CHECK(error == ERROR_INVALID_PARAMETER);
}


TEST_CASE("Memory: default region", "[win][memory][class]") {
    MemoryRegion region;

    CHECK_FALSE(region.valid());
    CHECK(region.data() == nullptr);
    CHECK(region.size() == 0);

    CHECK(region.reset() == 0);

    CHECK_FALSE(region.valid());
}


TEST_CASE("Memory: invalid region operations", "[win][memory][errors]") {
    MemoryRegion region;

    const int result = region.protect(PageProtection::ReadOnly);
    const DWORD error = ::GetLastError();

    CHECK(result == -1);
    CHECK(error == ERROR_INVALID_ADDRESS);

    CHECK(region.allocate(0, PageProtection::ReadWrite) == -1);
    CHECK(::GetLastError() == ERROR_INVALID_PARAMETER);
}


TEST_CASE("Memory: region allocation", "[win][memory][class]") {
    const auto size = page_size() * 2;

    MemoryRegion region;

    REQUIRE(region.allocate(
        size,
        PageProtection::ReadWrite
    ) == 0);

    REQUIRE(region.valid());
    REQUIRE(region.data() != nullptr);

    CHECK(region.size() == size);

    auto* bytes = static_cast<std::byte*>(region.data());

    bytes[0] = std::byte{0x11};
    bytes[size - 1] = std::byte{0x22};

    CHECK(bytes[0] == std::byte{0x11});
    CHECK(bytes[size - 1] == std::byte{0x22});
}


TEST_CASE("Memory: region protection", "[win][memory][class]") {
    MemoryRegion region;

    REQUIRE(region.allocate(
        page_size(),
        PageProtection::ReadWrite
    ) == 0);

    auto* bytes = static_cast<std::byte*>(region.data());
    bytes[0] = std::byte{0x55};

    REQUIRE(region.protect(PageProtection::ReadOnly) == 0);

    MEMORY_BASIC_INFORMATION info{};

    REQUIRE(::VirtualQuery(
        region.data(),
        &info,
        sizeof(info)
    ) == sizeof(info));

    CHECK(info.Protect == PAGE_READONLY);
    CHECK(bytes[0] == std::byte{0x55});

    REQUIRE(region.protect(PageProtection::NoAccess) == 0);

    REQUIRE(::VirtualQuery(
        region.data(),
        &info,
        sizeof(info)
    ) == sizeof(info));

    CHECK(info.Protect == PAGE_NOACCESS);

    REQUIRE(region.protect(PageProtection::ReadWrite) == 0);

    bytes[0] = std::byte{0x66};

    CHECK(bytes[0] == std::byte{0x66});
}


TEST_CASE("Memory: repeated allocation", "[win][memory][errors]") {
    MemoryRegion region;

    REQUIRE(region.allocate(
        page_size(),
        PageProtection::ReadWrite
    ) == 0);

    const void* address = region.data();

    const int result = region.allocate(
        page_size(),
        PageProtection::ReadOnly
    );

    const DWORD error = ::GetLastError();

    CHECK(result == -1);
    CHECK(error == ERROR_BUSY);

    CHECK(region.valid());
    CHECK(region.data() == address);
}


TEST_CASE("Memory: reset and reuse", "[win][memory][lifecycle]") {
    MemoryRegion region;

    REQUIRE(region.allocate(
        page_size(),
        PageProtection::ReadWrite
    ) == 0);

    REQUIRE(region.reset() == 0);

    CHECK_FALSE(region.valid());
    CHECK(region.data() == nullptr);
    CHECK(region.size() == 0);

    CHECK(region.reset() == 0);

    REQUIRE(region.allocate(
        page_size() * 2,
        PageProtection::ReadWrite
    ) == 0);

    CHECK(region.valid());
    CHECK(region.size() == page_size() * 2);

    REQUIRE(region.reset() == 0);
}


TEST_CASE("Memory: construction from MemoryBlock", "[win][memory][ownership]") {
    const auto size = page_size();

    void* address = allocate_memory(
        size,
        PageProtection::ReadWrite
    );

    REQUIRE(address != nullptr);

    {
        MemoryRegion region(MemoryBlock{
            .address = address,
            .size = size
        });

        CHECK(region.valid());
        CHECK(region.data() == address);
        CHECK(region.size() == size);
    }

    MEMORY_BASIC_INFORMATION info{};

    REQUIRE(::VirtualQuery(
        address,
        &info,
        sizeof(info)
    ) == sizeof(info));

    CHECK(info.State == MEM_FREE);
}


TEST_CASE("Memory: release transfers ownership", "[win][memory][ownership]") {
    MemoryRegion region;

    REQUIRE(region.allocate(
        page_size(),
        PageProtection::ReadWrite
    ) == 0);

    const void* address = region.data();

    const MemoryBlock block = region.release();

    CHECK_FALSE(region.valid());
    CHECK(region.data() == nullptr);
    CHECK(region.size() == 0);

    REQUIRE(block.address == address);
    CHECK(block.size == page_size());

    MEMORY_BASIC_INFORMATION info{};

    REQUIRE(::VirtualQuery(
        block.address,
        &info,
        sizeof(info)
    ) == sizeof(info));

    CHECK(info.State == MEM_COMMIT);

    REQUIRE(release_memory(block.address) == 0);
}


TEST_CASE("Memory: move construction", "[win][memory][move]") {
    MemoryRegion source;

    REQUIRE(source.allocate(
        page_size(),
        PageProtection::ReadWrite
    ) == 0);

    void* address = source.data();

    MemoryRegion destination(std::move(source));

    CHECK_FALSE(source.valid());
    CHECK(source.data() == nullptr);

    CHECK(destination.valid());
    CHECK(destination.data() == address);
    CHECK(destination.size() == page_size());

    auto* bytes = static_cast<std::byte*>(destination.data());

    bytes[0] = std::byte{0x77};

    CHECK(bytes[0] == std::byte{0x77});
}


TEST_CASE("Memory: move assignment", "[win][memory][move]") {
    MemoryRegion source;
    MemoryRegion destination;

    REQUIRE(source.allocate(
        page_size(),
        PageProtection::ReadWrite
    ) == 0);

    REQUIRE(destination.allocate(
        page_size(),
        PageProtection::ReadWrite
    ) == 0);

    void* transferred = source.data();
    void* previous = destination.data();

    destination = std::move(source);

    CHECK_FALSE(source.valid());
    CHECK(destination.valid());
    CHECK(destination.data() == transferred);

    MEMORY_BASIC_INFORMATION info{};

    REQUIRE(::VirtualQuery(
        previous,
        &info,
        sizeof(info)
    ) == sizeof(info));

    CHECK(info.State == MEM_FREE);
}


TEST_CASE("Memory: self move assignment", "[win][memory][move]") {
    MemoryRegion region;

    REQUIRE(region.allocate(
        page_size(),
        PageProtection::ReadWrite
    ) == 0);

    void* address = region.data();

    auto* self = &region;
    region = std::move(*self);

    CHECK(region.valid());
    CHECK(region.data() == address);
}


TEST_CASE("Memory: moving an empty region", "[win][memory][move]") {
    MemoryRegion source;
    MemoryRegion destination(std::move(source));

    CHECK_FALSE(source.valid());
    CHECK_FALSE(destination.valid());

    REQUIRE(destination.allocate(
        page_size(),
        PageProtection::ReadWrite
    ) == 0);

    CHECK(destination.valid());
}
