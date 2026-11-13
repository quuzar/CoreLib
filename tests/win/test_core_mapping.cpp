#include <catch2/catch_test_macros.hpp>
#include <win/mapping.hpp>
#include <win/file.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <windows.h>

using namespace core::win;

namespace {

    std::size_t allocation_granularity() noexcept {
        SYSTEM_INFO info{};
        ::GetSystemInfo(&info);
        return info.dwAllocationGranularity;
    }

    class TemporaryFile {
    public:
        explicit TemporaryFile(std::size_t size) {
            wchar_t directory[MAX_PATH]{};
            wchar_t filename[MAX_PATH]{};

            const DWORD length = ::GetTempPathW(MAX_PATH, directory);

            if (length == 0 || length >= MAX_PATH) {
                throw std::runtime_error("GetTempPathW failed");
            }

            if (::GetTempFileNameW(directory, L"CLB", 0, filename) == 0) {
                throw std::runtime_error("GetTempFileNameW failed");
            }

            path_ = filename;

            if (file_.open(path_.c_str(), FileAccess::ReadWrite) == -1 ||
                file_.truncate(static_cast<std::int64_t>(size)) == -1) {
                file_.close();
                ::DeleteFileW(path_.c_str());
                throw std::runtime_error("Temporary file initialization failed");
            }
        }

        ~TemporaryFile() {
            file_.close();
            ::DeleteFileW(path_.c_str());
        }

        TemporaryFile(const TemporaryFile&) = delete;
        TemporaryFile& operator=(const TemporaryFile&) = delete;

        [[nodiscard]] HANDLE handle() const noexcept {
            return file_.native_handle();
        }

        [[nodiscard]] File& file() noexcept {
            return file_;
        }

    private:
        std::wstring path_;
        File file_;
    };

    class RawMapping {
    public:
        explicit RawMapping(void* address) noexcept
            : address_(address) {}

        ~RawMapping() {
            if (address_ != nullptr) {
                unmap_file(address_);
            }
        }

        RawMapping(const RawMapping&) = delete;
        RawMapping& operator=(const RawMapping&) = delete;

        [[nodiscard]] void* data() const noexcept {
            return address_;
        }

        int reset() noexcept {
            if (address_ == nullptr) {
                return 0;
            }

            if (unmap_file(address_) == -1) {
                return -1;
            }

            address_ = nullptr;
            return 0;
        }

    private:
        void* address_{};
    };

}


TEST_CASE("Mapping: type properties", "[win][mapping][types]") {
    STATIC_REQUIRE_FALSE(std::is_copy_constructible_v<MappedRegion>);
    STATIC_REQUIRE_FALSE(std::is_copy_assignable_v<MappedRegion>);

    STATIC_REQUIRE(std::is_move_constructible_v<MappedRegion>);
    STATIC_REQUIRE(std::is_move_assignable_v<MappedRegion>);

    STATIC_REQUIRE(std::is_nothrow_move_constructible_v<MappedRegion>);
    STATIC_REQUIRE(std::is_nothrow_move_assignable_v<MappedRegion>);
}


TEST_CASE("Mapping: allocation granularity", "[win][mapping][raw]") {
    const auto granularity = allocation_granularity();

    CHECK(granularity > 0);
    CHECK(granularity >= page_size());
}


TEST_CASE("Mapping: raw shared read and write", "[win][mapping][raw]") {
    TemporaryFile temp(allocation_granularity());

    REQUIRE(temp.file().write("Hello", 5) == 5);

    RawMapping mapping(map_file(
        temp.handle(),
        page_size(),
        0,
        PageProtection::ReadWrite,
        MappingMode::Shared
    ));

    REQUIRE(mapping.data() != nullptr);

    auto* bytes = static_cast<char*>(mapping.data());

    CHECK(std::string_view(bytes, 5) == "Hello");

    bytes[0] = 'W';

    REQUIRE(flush_mapping(
        mapping.data(),
        page_size()
    ) == 0);

    REQUIRE(temp.file().seek(0, SeekOrigin::Begin) == 0);

    char buffer[6]{};

    REQUIRE(temp.file().read(buffer, 5) == 5);
    CHECK(std::string_view(buffer, 5) == "Wello");
}


TEST_CASE("Mapping: raw read only mapping", "[win][mapping][raw]") {
    TemporaryFile temp(allocation_granularity());

    REQUIRE(temp.file().write("ReadOnly", 8) == 8);

    RawMapping mapping(map_file(
        temp.handle(),
        page_size(),
        0,
        PageProtection::ReadOnly,
        MappingMode::Shared
    ));

    REQUIRE(mapping.data() != nullptr);

    const auto* bytes = static_cast<const char*>(mapping.data());

    CHECK(std::string_view(bytes, 8) == "ReadOnly");

    MEMORY_BASIC_INFORMATION info{};

    REQUIRE(::VirtualQuery(
        mapping.data(),
        &info,
        sizeof(info)
    ) == sizeof(info));

    CHECK(info.Protect == PAGE_READONLY);
}


TEST_CASE("Mapping: raw private copy on write", "[win][mapping][private]") {
    TemporaryFile temp(allocation_granularity());

    REQUIRE(temp.file().write("ABCDE", 5) == 5);

    RawMapping mapping(map_file(
        temp.handle(),
        page_size(),
        0,
        PageProtection::ReadWrite,
        MappingMode::Private
    ));

    REQUIRE(mapping.data() != nullptr);

    auto* bytes = static_cast<char*>(mapping.data());

    CHECK(std::string_view(bytes, 5) == "ABCDE");

    bytes[0] = 'X';
    bytes[1] = 'Y';

    CHECK(std::string_view(bytes, 5) == "XYCDE");

    REQUIRE(temp.file().seek(0, SeekOrigin::Begin) == 0);

    char buffer[6]{};

    REQUIRE(temp.file().read(buffer, 5) == 5);

    CHECK(std::string_view(buffer, 5) == "ABCDE");
}


TEST_CASE("Mapping: raw aligned offset", "[win][mapping][raw]") {
    const auto granularity = allocation_granularity();

    TemporaryFile temp(granularity * 2);

    REQUIRE(temp.file().seek(
        static_cast<std::int64_t>(granularity),
        SeekOrigin::Begin
    ) == static_cast<std::int64_t>(granularity));

    REQUIRE(temp.file().write("OFFSET", 6) == 6);

    RawMapping mapping(map_file(
        temp.handle(),
        page_size(),
        static_cast<std::int64_t>(granularity),
        PageProtection::ReadOnly,
        MappingMode::Shared
    ));

    REQUIRE(mapping.data() != nullptr);

    const auto* bytes = static_cast<const char*>(mapping.data());

    CHECK(std::string_view(bytes, 6) == "OFFSET");
}


TEST_CASE("Mapping: raw invalid file handle", "[win][mapping][errors]") {
    const void* address = map_file(
        INVALID_HANDLE_VALUE,
        page_size(),
        0,
        PageProtection::ReadOnly,
        MappingMode::Shared
    );

    const DWORD error = ::GetLastError();

    CHECK(address == nullptr);
    CHECK(error == ERROR_INVALID_HANDLE);
}


TEST_CASE("Mapping: raw zero size", "[win][mapping][errors]") {
    TemporaryFile temp(allocation_granularity());

    const void* address = map_file(
        temp.handle(),
        0,
        0,
        PageProtection::ReadOnly,
        MappingMode::Shared
    );

    const DWORD error = ::GetLastError();

    CHECK(address == nullptr);
    CHECK(error == ERROR_INVALID_PARAMETER);
}


TEST_CASE("Mapping: raw negative offset", "[win][mapping][errors]") {
    TemporaryFile temp(allocation_granularity());

    const void* address = map_file(
        temp.handle(),
        page_size(),
        -1,
        PageProtection::ReadOnly,
        MappingMode::Shared
    );

    const DWORD error = ::GetLastError();

    CHECK(address == nullptr);
    CHECK(error == ERROR_INVALID_PARAMETER);
}


TEST_CASE("Mapping: raw unaligned offset", "[win][mapping][errors]") {
    TemporaryFile temp(allocation_granularity());

    const void* address = map_file(
        temp.handle(),
        page_size(),
        1,
        PageProtection::ReadOnly,
        MappingMode::Shared
    );

    const DWORD error = ::GetLastError();

    CHECK(address == nullptr);
    CHECK(error == ERROR_MAPPED_ALIGNMENT);
}


TEST_CASE("Mapping: raw unsupported protection", "[win][mapping][errors]") {
    TemporaryFile temp(allocation_granularity());

    const void* address = map_file(
        temp.handle(),
        page_size(),
        0,
        PageProtection::NoAccess,
        MappingMode::Shared
    );

    const DWORD error = ::GetLastError();

    CHECK(address == nullptr);
    CHECK(error == ERROR_INVALID_PARAMETER);
}


TEST_CASE("Mapping: raw invalid unmap", "[win][mapping][errors]") {
    const int result = unmap_file(nullptr);
    const DWORD error = ::GetLastError();

    CHECK(result == -1);
    CHECK(error == ERROR_INVALID_PARAMETER);
}


TEST_CASE("Mapping: raw invalid flush", "[win][mapping][errors]") {
    const int result = flush_mapping(nullptr, page_size());
    const DWORD error = ::GetLastError();

    CHECK(result == -1);
    CHECK(error == ERROR_INVALID_PARAMETER);
}


TEST_CASE("Mapping: raw unmap releases view", "[win][mapping][raw]") {
    TemporaryFile temp(allocation_granularity());

    RawMapping mapping(map_file(
        temp.handle(),
        page_size(),
        0,
        PageProtection::ReadWrite,
        MappingMode::Shared
    ));

    REQUIRE(mapping.data() != nullptr);

    void* address = mapping.data();

    REQUIRE(mapping.reset() == 0);

    MEMORY_BASIC_INFORMATION info{};

    REQUIRE(::VirtualQuery(
        address,
        &info,
        sizeof(info)
    ) == sizeof(info));

    CHECK(info.State == MEM_FREE);
}


TEST_CASE("Mapping: default region", "[win][mapping][class]") {
    MappedRegion region;

    CHECK_FALSE(region.valid());
    CHECK(region.data() == nullptr);
    CHECK(region.size() == 0);

    CHECK(region.reset() == 0);
}


TEST_CASE("Mapping: invalid region operations", "[win][mapping][errors]") {
    MappedRegion region;

    const int result = region.flush();
    const DWORD error = ::GetLastError();

    CHECK(result == -1);
    CHECK(error == ERROR_INVALID_ADDRESS);

    CHECK(region.reset() == 0);

    TemporaryFile temp(allocation_granularity());

    CHECK(region.map(
        temp.handle(),
        0,
        0,
        PageProtection::ReadWrite,
        MappingMode::Shared
    ) == -1);

    CHECK(::GetLastError() == ERROR_INVALID_PARAMETER);
}


TEST_CASE("Mapping: region shared mapping", "[win][mapping][class]") {
    TemporaryFile temp(allocation_granularity());

    REQUIRE(temp.file().write("CoreLib", 7) == 7);

    MappedRegion region;

    REQUIRE(region.map(
        temp.handle(),
        page_size(),
        0,
        PageProtection::ReadWrite,
        MappingMode::Shared
    ) == 0);

    REQUIRE(region.valid());

    CHECK(region.size() == page_size());
    CHECK(std::string_view(
        static_cast<const char*>(region.data()), 7
    ) == "CoreLib");

    auto* bytes = static_cast<char*>(region.data());

    bytes[0] = 'X';

    REQUIRE(region.flush() == 0);

    REQUIRE(temp.file().seek(0, SeekOrigin::Begin) == 0);

    char buffer[8]{};

    REQUIRE(temp.file().read(buffer, 7) == 7);

    CHECK(std::string_view(buffer, 7) == "XoreLib");
}


TEST_CASE("Mapping: region unaligned offset", "[win][mapping][offset]") {
    TemporaryFile temp(allocation_granularity());

    constexpr std::int64_t offset = 13;
    constexpr std::size_t size = 128;

    REQUIRE(temp.file().seek(offset, SeekOrigin::Begin) == offset);
    REQUIRE(temp.file().write("HELLO", 5) == 5);

    MappedRegion region;

    REQUIRE(region.map(
        temp.handle(),
        size,
        offset,
        PageProtection::ReadWrite,
        MappingMode::Shared
    ) == 0);

    REQUIRE(region.valid());

    CHECK(region.size() == size);

    auto* bytes = static_cast<char*>(region.data());

    REQUIRE(bytes != nullptr);
    CHECK(std::string_view(bytes, 5) == "HELLO");

    bytes[0] = 'Y';

    REQUIRE(region.flush() == 0);

    REQUIRE(temp.file().seek(offset, SeekOrigin::Begin) == offset);

    char value{};

    REQUIRE(temp.file().read(&value, 1) == 1);
    CHECK(value == 'Y');
}


TEST_CASE("Mapping: region offset beyond first granularity", "[win][mapping][offset]") {
    const auto granularity = allocation_granularity();

    TemporaryFile temp(granularity * 3);

    const auto offset = static_cast<std::int64_t>(granularity + 19);

    REQUIRE(temp.file().seek(offset, SeekOrigin::Begin) == offset);
    REQUIRE(temp.file().write("SECOND", 6) == 6);

    MappedRegion region;

    REQUIRE(region.map(
        temp.handle(),
        128,
        offset,
        PageProtection::ReadOnly,
        MappingMode::Shared
    ) == 0);

    REQUIRE(region.valid());

    CHECK(region.size() == 128);

    const auto* bytes = static_cast<const char*>(region.data());

    CHECK(std::string_view(bytes, 6) == "SECOND");
}


TEST_CASE("Mapping: region private mapping", "[win][mapping][private]") {
    TemporaryFile temp(allocation_granularity());

    REQUIRE(temp.file().write("ORIGINAL", 8) == 8);

    MappedRegion region;

    REQUIRE(region.map(
        temp.handle(),
        page_size(),
        0,
        PageProtection::ReadWrite,
        MappingMode::Private
    ) == 0);

    REQUIRE(region.valid());

    auto* bytes = static_cast<char*>(region.data());

    bytes[0] = 'X';
    bytes[1] = 'X';

    CHECK(std::string_view(bytes, 8) == "XXIGINAL");

    REQUIRE(temp.file().seek(0, SeekOrigin::Begin) == 0);

    char buffer[9]{};

    REQUIRE(temp.file().read(buffer, 8) == 8);

    CHECK(std::string_view(buffer, 8) == "ORIGINAL");
}


TEST_CASE("Mapping: region repeated mapping", "[win][mapping][errors]") {
    TemporaryFile temp(allocation_granularity());

    MappedRegion region;

    REQUIRE(region.map(
        temp.handle(),
        page_size(),
        0,
        PageProtection::ReadWrite,
        MappingMode::Shared
    ) == 0);

    void* address = region.data();

    const int result = region.map(
        temp.handle(),
        page_size(),
        0,
        PageProtection::ReadOnly,
        MappingMode::Shared
    );

    const DWORD error = ::GetLastError();

    CHECK(result == -1);
    CHECK(error == ERROR_BUSY);

    CHECK(region.valid());
    CHECK(region.data() == address);
}


TEST_CASE("Mapping: region arithmetic overflow", "[win][mapping][errors]") {
    TemporaryFile temp(allocation_granularity());

    MappedRegion region;

    const auto size = (std::numeric_limits<std::size_t>::max)();

    const int result = region.map(
        temp.handle(),
        size,
        1,
        PageProtection::ReadOnly,
        MappingMode::Shared
    );

    const DWORD error = ::GetLastError();

    CHECK(result == -1);
    CHECK(error == ERROR_ARITHMETIC_OVERFLOW);
    CHECK_FALSE(region.valid());
}


TEST_CASE("Mapping: region reset and reuse", "[win][mapping][lifecycle]") {
    TemporaryFile temp(allocation_granularity());

    MappedRegion region;

    REQUIRE(region.map(
        temp.handle(),
        page_size(),
        0,
        PageProtection::ReadWrite,
        MappingMode::Shared
    ) == 0);

    REQUIRE(region.reset() == 0);

    CHECK_FALSE(region.valid());
    CHECK(region.data() == nullptr);
    CHECK(region.size() == 0);

    CHECK(region.reset() == 0);

    REQUIRE(region.map(
        temp.handle(),
        page_size(),
        0,
        PageProtection::ReadOnly,
        MappingMode::Shared
    ) == 0);

    CHECK(region.valid());

    REQUIRE(region.reset() == 0);
}


TEST_CASE("Mapping: view survives file handle close", "[win][mapping][lifecycle]") {
    TemporaryFile temp(allocation_granularity());

    REQUIRE(temp.file().write("ALIVE", 5) == 5);

    MappedRegion region;

    REQUIRE(region.map(
        temp.handle(),
        page_size(),
        0,
        PageProtection::ReadOnly,
        MappingMode::Shared
    ) == 0);

    REQUIRE(temp.file().close() == 0);

    const auto* bytes = static_cast<const char*>(region.data());

    REQUIRE(bytes != nullptr);
    CHECK(std::string_view(bytes, 5) == "ALIVE");

    REQUIRE(region.reset() == 0);
}


TEST_CASE("Mapping: construction from MappingBlock", "[win][mapping][ownership]") {
    TemporaryFile temp(allocation_granularity());

    void* address = map_file(
        temp.handle(),
        page_size(),
        0,
        PageProtection::ReadWrite,
        MappingMode::Shared
    );

    REQUIRE(address != nullptr);

    {
        MappedRegion region(MappingBlock{
            .address = address,
            .mapped_size = page_size(),
            .offset = 0,
            .size = page_size()
        });

        CHECK(region.valid());
        CHECK(region.data() == address);
        CHECK(region.size() == page_size());
    }

    MEMORY_BASIC_INFORMATION info{};

    REQUIRE(::VirtualQuery(
        address,
        &info,
        sizeof(info)
    ) == sizeof(info));

    CHECK(info.State == MEM_FREE);
}


TEST_CASE("Mapping: release transfers ownership", "[win][mapping][ownership]") {
    TemporaryFile temp(allocation_granularity());

    constexpr std::int64_t offset = 17;
    constexpr std::size_t size = 100;

    MappedRegion region;

    REQUIRE(region.map(
        temp.handle(),
        size,
        offset,
        PageProtection::ReadWrite,
        MappingMode::Shared
    ) == 0);

    void* data = region.data();

    const MappingBlock block = region.release();

    CHECK_FALSE(region.valid());
    CHECK(region.data() == nullptr);
    CHECK(region.size() == 0);

    REQUIRE(block.address != nullptr);

    CHECK(block.offset == 17);
    CHECK(block.size == size);
    CHECK(block.mapped_size == size + 17);

    CHECK(
        static_cast<std::byte*>(block.address) + block.offset == data
    );

    REQUIRE(unmap_file(block.address) == 0);
}


TEST_CASE("Mapping: move construction", "[win][mapping][move]") {
    TemporaryFile temp(allocation_granularity());

    MappedRegion source;

    REQUIRE(source.map(
        temp.handle(),
        page_size(),
        0,
        PageProtection::ReadWrite,
        MappingMode::Shared
    ) == 0);

    void* address = source.data();

    MappedRegion destination(std::move(source));

    CHECK_FALSE(source.valid());
    CHECK(source.data() == nullptr);

    CHECK(destination.valid());
    CHECK(destination.data() == address);
    CHECK(destination.size() == page_size());

    auto* bytes = static_cast<char*>(destination.data());

    bytes[0] = 'A';

    CHECK(bytes[0] == 'A');
}


TEST_CASE("Mapping: move assignment", "[win][mapping][move]") {
    TemporaryFile temp(allocation_granularity() * 2);

    MappedRegion source;
    MappedRegion destination;

    REQUIRE(source.map(
        temp.handle(),
        page_size(),
        0,
        PageProtection::ReadWrite,
        MappingMode::Shared
    ) == 0);

    REQUIRE(destination.map(
        temp.handle(),
        page_size(),
        static_cast<std::int64_t>(allocation_granularity()),
        PageProtection::ReadWrite,
        MappingMode::Shared
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


TEST_CASE("Mapping: self move assignment", "[win][mapping][move]") {
    TemporaryFile temp(allocation_granularity());

    MappedRegion region;

    REQUIRE(region.map(
        temp.handle(),
        page_size(),
        0,
        PageProtection::ReadWrite,
        MappingMode::Shared
    ) == 0);

    void* address = region.data();

    auto* self = &region;
    region = std::move(*self);

    CHECK(region.valid());
    CHECK(region.data() == address);
}


TEST_CASE("Mapping: moving an empty region", "[win][mapping][move]") {
    MappedRegion source;
    MappedRegion destination(std::move(source));

    CHECK_FALSE(source.valid());
    CHECK_FALSE(destination.valid());

    TemporaryFile temp(allocation_granularity());

    REQUIRE(destination.map(
        temp.handle(),
        page_size(),
        0,
        PageProtection::ReadOnly,
        MappingMode::Shared
    ) == 0);

    CHECK(destination.valid());
}
