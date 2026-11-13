#include <catch2/catch_test_macros.hpp>
#include <win/file.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <windows.h>

using namespace core::win;

namespace {

    class TemporaryFile {
    public:
        TemporaryFile() {
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
        }

        ~TemporaryFile() {
            ::DeleteFileW(path_.c_str());
        }

        TemporaryFile(const TemporaryFile&) = delete;
        TemporaryFile& operator=(const TemporaryFile&) = delete;

        [[nodiscard]] const wchar_t* path() const noexcept {
            return path_.c_str();
        }

        void remove() const {
            if (!::DeleteFileW(path_.c_str())) {
                throw std::runtime_error("DeleteFileW failed");
            }
        }

    private:
        std::wstring path_;
    };


    class RawHandle {
    public:
        explicit RawHandle(HANDLE handle) noexcept : handle_(handle) {}

        ~RawHandle() {
            if (handle_ != nullptr && handle_ != INVALID_HANDLE_VALUE) {
                close_file(handle_);
            }
        }

        RawHandle(const RawHandle&) = delete;
        RawHandle& operator=(const RawHandle&) = delete;

        [[nodiscard]] HANDLE get() const noexcept {
            return handle_;
        }

        [[nodiscard]] bool valid() const noexcept {
            return handle_ != nullptr &&
                   handle_ != INVALID_HANDLE_VALUE;
        }

    private:
        HANDLE handle_;
    };

}


// ============================================================
// Types and flags
// ============================================================

TEST_CASE("File: type properties", "[win][file][types]") {
    STATIC_REQUIRE_FALSE(std::is_copy_constructible_v<File>);
    STATIC_REQUIRE_FALSE(std::is_copy_assignable_v<File>);

    STATIC_REQUIRE(std::is_move_constructible_v<File>);
    STATIC_REQUIRE(std::is_move_assignable_v<File>);

    STATIC_REQUIRE(std::is_nothrow_move_constructible_v<File>);
    STATIC_REQUIRE(std::is_nothrow_move_assignable_v<File>);
}

TEST_CASE("File: share flags", "[win][file][flags]") {
    const auto flags = FileShare::Read | FileShare::Write;

    CHECK(static_cast<DWORD>(flags) ==
          (FILE_SHARE_READ | FILE_SHARE_WRITE));

    auto combined = FileShare::Read;
    combined |= FileShare::Delete;

    CHECK(static_cast<DWORD>(combined) ==
          (FILE_SHARE_READ | FILE_SHARE_DELETE));
}

TEST_CASE("File: option flags", "[win][file][flags]") {
    const auto flags =
        FileOptions::SequentialScan | FileOptions::WriteThrough;

    CHECK(static_cast<DWORD>(flags) ==
          (FILE_FLAG_SEQUENTIAL_SCAN | FILE_FLAG_WRITE_THROUGH));
}


// ============================================================
// Raw API: opening and closing
// ============================================================

TEST_CASE("File: raw open existing file", "[win][file][raw]") {
    TemporaryFile temp;

    RawHandle handle(open_file(
        temp.path(),
        FileAccess::ReadWrite
    ));

    REQUIRE(handle.valid());
}

TEST_CASE("File: raw open with null path", "[win][file][errors]") {
    const HANDLE handle = open_file(
        nullptr,
        FileAccess::Read
    );

    const DWORD error = ::GetLastError();

    CHECK(handle == INVALID_HANDLE_VALUE);
    CHECK(error == ERROR_INVALID_PARAMETER);
}

TEST_CASE("File: raw open missing file", "[win][file][errors]") {
    TemporaryFile temp;
    temp.remove();

    const HANDLE handle = open_file(
        temp.path(),
        FileAccess::Read
    );

    const DWORD error = ::GetLastError();

    CHECK(handle == INVALID_HANDLE_VALUE);
    CHECK(error == ERROR_FILE_NOT_FOUND);
}

TEST_CASE("File: raw close invalid handle", "[win][file][errors]") {
    const int result = close_file(INVALID_HANDLE_VALUE);
    const DWORD error = ::GetLastError();

    CHECK(result == -1);
    CHECK(error == ERROR_INVALID_HANDLE);
}

TEST_CASE("File: create new fails when file exists", "[win][file][creation]") {
    TemporaryFile temp;

    const HANDLE handle = open_file(
        temp.path(),
        FileAccess::ReadWrite,
        FileShare::Read,
        FileCreation::CreateNew
    );

    const DWORD error = ::GetLastError();

    CHECK(handle == INVALID_HANDLE_VALUE);
    CHECK(error == ERROR_FILE_EXISTS);
}

TEST_CASE("File: create new creates a file", "[win][file][creation]") {
    TemporaryFile temp;
    temp.remove();

    File file;

    REQUIRE(file.open(
        temp.path(),
        FileAccess::ReadWrite,
        FileShare::Read,
        FileCreation::CreateNew
    ) == 0);

    CHECK(file.valid());
    CHECK(file.size() == 0);
}

TEST_CASE("File: create always truncates existing file", "[win][file][creation]") {
    TemporaryFile temp;
    File file;

    REQUIRE(file.open(
        temp.path(),
        FileAccess::ReadWrite
    ) == 0);

    REQUIRE(file.write("hello", 5) == 5);
    REQUIRE(file.close() == 0);

    REQUIRE(file.open(
        temp.path(),
        FileAccess::ReadWrite,
        FileShare::Read,
        FileCreation::CreateAlways
    ) == 0);

    CHECK(file.size() == 0);
}

TEST_CASE("File: open always preserves existing data", "[win][file][creation]") {
    TemporaryFile temp;
    File file;

    REQUIRE(file.open(temp.path(), FileAccess::ReadWrite) == 0);
    REQUIRE(file.write("hello", 5) == 5);
    REQUIRE(file.close() == 0);

    REQUIRE(file.open(
        temp.path(),
        FileAccess::ReadWrite,
        FileShare::Read,
        FileCreation::OpenAlways
    ) == 0);

    CHECK(file.size() == 5);
}

TEST_CASE("File: truncate existing on open", "[win][file][creation]") {
    TemporaryFile temp;
    File file;

    REQUIRE(file.open(temp.path(), FileAccess::ReadWrite) == 0);
    REQUIRE(file.write("hello", 5) == 5);
    REQUIRE(file.close() == 0);

    REQUIRE(file.open(
        temp.path(),
        FileAccess::Write,
        FileShare::Read,
        FileCreation::TruncateExisting
    ) == 0);

    CHECK(file.size() == 0);
}


// ============================================================
// Raw API: read and write
// ============================================================

TEST_CASE("File: raw write and read", "[win][file][raw]") {
    TemporaryFile temp;

    RawHandle file(open_file(temp.path(), FileAccess::ReadWrite));
    REQUIRE(file.valid());

    constexpr std::string_view data = "Hello CoreLib";

    REQUIRE(write_file(file.get(), data.data(), data.size()) ==
            static_cast<std::int64_t>(data.size()));

    REQUIRE(seek_file(file.get(), 0, SeekOrigin::Begin) == 0);

    std::array<char, 32> buffer{};

    REQUIRE(read_file(file.get(), buffer.data(), data.size()) ==
            static_cast<std::int64_t>(data.size()));

    CHECK(std::string_view(buffer.data(), data.size()) == data);
    CHECK(file_size(file.get()) ==
          static_cast<std::int64_t>(data.size()));
}

TEST_CASE("File: raw read at end of file", "[win][file][raw]") {
    TemporaryFile temp;

    RawHandle file(open_file(temp.path(), FileAccess::ReadWrite));
    REQUIRE(file.valid());

    REQUIRE(write_file(file.get(), "abc", 3) == 3);
    REQUIRE(seek_file(file.get(), 0, SeekOrigin::End) == 3);

    char buffer{};

    CHECK(read_file(file.get(), &buffer, 1) == 0);
}

TEST_CASE("File: raw zero byte operations", "[win][file][raw]") {
    TemporaryFile temp;

    RawHandle file(open_file(temp.path(), FileAccess::ReadWrite));
    REQUIRE(file.valid());

    CHECK(read_file(file.get(), nullptr, 0) == 0);
    CHECK(write_file(file.get(), nullptr, 0) == 0);
    CHECK(read_at(file.get(), nullptr, 0, 0) == 0);
    CHECK(write_at(file.get(), nullptr, 0, 0) == 0);

    CHECK(file_size(file.get()) == 0);
}

TEST_CASE("File: raw seek origins", "[win][file][raw]") {
    TemporaryFile temp;

    RawHandle file(open_file(temp.path(), FileAccess::ReadWrite));
    REQUIRE(file.valid());

    REQUIRE(write_file(file.get(), "0123456789", 10) == 10);

    CHECK(seek_file(file.get(), 3, SeekOrigin::Begin) == 3);
    CHECK(seek_file(file.get(), 2, SeekOrigin::Current) == 5);
    CHECK(seek_file(file.get(), -2, SeekOrigin::End) == 8);
    CHECK(seek_file(file.get(), 0, SeekOrigin::Current) == 8);
}

TEST_CASE("File: raw positional read and write", "[win][file][position]") {
    TemporaryFile temp;

    RawHandle file(open_file(temp.path(), FileAccess::ReadWrite));
    REQUIRE(file.valid());

    REQUIRE(write_file(file.get(), "abcdef", 6) == 6);
    REQUIRE(seek_file(file.get(), 2, SeekOrigin::Begin) == 2);

    REQUIRE(write_at(file.get(), "XY", 2, 3) == 2);

    CHECK(seek_file(file.get(), 0, SeekOrigin::Current) == 2);

    std::array<char, 8> buffer{};

    REQUIRE(read_at(file.get(), buffer.data(), 6, 0) == 6);

    CHECK(std::string_view(buffer.data(), 6) == "abcXYf");
    CHECK(seek_file(file.get(), 0, SeekOrigin::Current) == 2);
}

TEST_CASE("File: raw positional read beyond EOF", "[win][file][position]") {
    TemporaryFile temp;

    RawHandle file(open_file(temp.path(), FileAccess::ReadWrite));
    REQUIRE(file.valid());

    REQUIRE(write_file(file.get(), "abc", 3) == 3);
    REQUIRE(seek_file(file.get(), 1, SeekOrigin::Begin) == 1);

    char buffer{};

    CHECK(read_at(file.get(), &buffer, 1, 100) == 0);
    CHECK(seek_file(file.get(), 0, SeekOrigin::Current) == 1);
}

TEST_CASE("File: raw positional write extends file", "[win][file][position]") {
    TemporaryFile temp;

    RawHandle file(open_file(temp.path(), FileAccess::ReadWrite));
    REQUIRE(file.valid());

    constexpr std::int64_t offset = 1024 * 1024;
    const char value = 'X';

    REQUIRE(write_at(file.get(), &value, 1, offset) == 1);

    CHECK(file_size(file.get()) == offset + 1);

    char result{};

    REQUIRE(read_at(file.get(), &result, 1, offset) == 1);
    CHECK(result == 'X');
}

TEST_CASE("File: raw truncate and sync", "[win][file][raw]") {
    TemporaryFile temp;

    RawHandle file(open_file(temp.path(), FileAccess::ReadWrite));
    REQUIRE(file.valid());

    REQUIRE(write_file(file.get(), "abcdefgh", 8) == 8);
    REQUIRE(seek_file(file.get(), 4, SeekOrigin::Begin) == 4);

    REQUIRE(truncate_file(file.get(), 3) == 0);

    CHECK(file_size(file.get()) == 3);
    CHECK(seek_file(file.get(), 0, SeekOrigin::Current) == 4);

    REQUIRE(truncate_file(file.get(), 12) == 0);

    CHECK(file_size(file.get()) == 12);
    CHECK(sync_file(file.get()) == 0);
}


// ============================================================
// Raw API: errors
// ============================================================

TEST_CASE("File: raw invalid handles", "[win][file][errors]") {
    char buffer{};

    CHECK(read_file(INVALID_HANDLE_VALUE, &buffer, 1) == -1);
    CHECK(write_file(INVALID_HANDLE_VALUE, &buffer, 1) == -1);
    CHECK(read_at(INVALID_HANDLE_VALUE, &buffer, 1, 0) == -1);
    CHECK(write_at(INVALID_HANDLE_VALUE, &buffer, 1, 0) == -1);

    CHECK(seek_file(INVALID_HANDLE_VALUE, 0, SeekOrigin::Begin) == -1);
    CHECK(file_size(INVALID_HANDLE_VALUE) == -1);
    CHECK(truncate_file(INVALID_HANDLE_VALUE, 0) == -1);
    CHECK(sync_file(INVALID_HANDLE_VALUE) == -1);

    CHECK(::GetLastError() == ERROR_INVALID_HANDLE);
}

TEST_CASE("File: raw null buffers", "[win][file][errors]") {
    TemporaryFile temp;

    RawHandle file(open_file(temp.path(), FileAccess::ReadWrite));
    REQUIRE(file.valid());

    const auto read_result = read_file(file.get(), nullptr, 1);
    const DWORD read_error = ::GetLastError();

    const auto write_result = write_file(file.get(), nullptr, 1);
    const DWORD write_error = ::GetLastError();

    CHECK(read_result == -1);
    CHECK(read_error == ERROR_INVALID_PARAMETER);

    CHECK(write_result == -1);
    CHECK(write_error == ERROR_INVALID_PARAMETER);
}

TEST_CASE("File: raw negative offsets", "[win][file][errors]") {
    TemporaryFile temp;

    RawHandle file(open_file(temp.path(), FileAccess::ReadWrite));
    REQUIRE(file.valid());

    char buffer{};

    const auto read_result = read_at(file.get(), &buffer, 1, -1);
    const DWORD read_error = ::GetLastError();

    const auto write_result = write_at(file.get(), &buffer, 1, -1);
    const DWORD write_error = ::GetLastError();

    CHECK(read_result == -1);
    CHECK(write_result == -1);

    CHECK(read_error == ERROR_INVALID_PARAMETER);
    CHECK(write_error == ERROR_INVALID_PARAMETER);
}

TEST_CASE("File: raw negative truncate size", "[win][file][errors]") {
    TemporaryFile temp;

    RawHandle file(open_file(temp.path(), FileAccess::ReadWrite));
    REQUIRE(file.valid());

    const int result = truncate_file(file.get(), -1);
    const DWORD error = ::GetLastError();

    CHECK(result == -1);
    CHECK(error == ERROR_INVALID_PARAMETER);
}

TEST_CASE("File: raw oversized IO request", "[win][file][errors]") {
    TemporaryFile temp;

    RawHandle file(open_file(temp.path(), FileAccess::ReadWrite));
    REQUIRE(file.valid());

    if constexpr (sizeof(std::size_t) > sizeof(DWORD)) {
        const auto count = static_cast<std::size_t>(
            static_cast<std::uint64_t>(
                std::numeric_limits<DWORD>::max()
            ) + 1
        );

        char buffer{};

        const auto result = write_file(file.get(), &buffer, count);
        const DWORD error = ::GetLastError();

        CHECK(result == -1);
        CHECK(error == ERROR_INVALID_PARAMETER);
    }
}

TEST_CASE("File: read and write access restrictions", "[win][file][errors]") {
    TemporaryFile temp;

    File reader;
    REQUIRE(reader.open(temp.path(), FileAccess::Read) == 0);

    const char value = 'X';

    const auto result = reader.write(&value, 1);
    const DWORD error = ::GetLastError();

    CHECK(result == -1);
    CHECK(error == ERROR_ACCESS_DENIED);
}

TEST_CASE("File: exclusive sharing", "[win][file][sharing]") {
    TemporaryFile temp;

    File first;

    REQUIRE(first.open(
        temp.path(),
        FileAccess::ReadWrite,
        FileShare::None
    ) == 0);

    const HANDLE second = open_file(
        temp.path(),
        FileAccess::Read,
        FileShare::Read
    );

    const DWORD error = ::GetLastError();

    CHECK(second == INVALID_HANDLE_VALUE);
    CHECK(error == ERROR_SHARING_VIOLATION);

    if (second != INVALID_HANDLE_VALUE) {
        REQUIRE(close_file(second) == 0);
    }

    REQUIRE(first.close() == 0);

    File reopened;
    CHECK(reopened.open(temp.path(), FileAccess::Read) == 0);
}


// ============================================================
// File class: lifecycle
// ============================================================

TEST_CASE("File: default state", "[win][file][class]") {
    File file;

    CHECK_FALSE(file.valid());
    CHECK(file.native_handle() == INVALID_HANDLE_VALUE);

    CHECK(file.close() == 0);
    CHECK(file.release() == INVALID_HANDLE_VALUE);
    CHECK(file.reset() == 0);
}

TEST_CASE("File: operations on invalid object", "[win][file][errors]") {
    File file;
    char buffer{};

    CHECK(file.read(&buffer, 1) == -1);
    CHECK(file.write(&buffer, 1) == -1);

    CHECK(file.read_at(&buffer, 1, 0) == -1);
    CHECK(file.write_at(&buffer, 1, 0) == -1);

    CHECK(file.seek(0, SeekOrigin::Begin) == -1);
    CHECK(file.size() == -1);
    CHECK(file.truncate(0) == -1);
    CHECK(file.sync() == -1);

    CHECK_FALSE(file.valid());
}

TEST_CASE("File: open and close", "[win][file][class]") {
    TemporaryFile temp;
    File file;

    REQUIRE(file.open(temp.path(), FileAccess::ReadWrite) == 0);

    CHECK(file.valid());
    CHECK(file.native_handle() != INVALID_HANDLE_VALUE);

    REQUIRE(file.close() == 0);

    CHECK_FALSE(file.valid());
    CHECK(file.native_handle() == INVALID_HANDLE_VALUE);

    CHECK(file.close() == 0);
}

TEST_CASE("File: repeated opening", "[win][file][class]") {
    TemporaryFile temp;
    File file;

    REQUIRE(file.open(temp.path(), FileAccess::ReadWrite) == 0);

    const HANDLE original = file.native_handle();

    const int result = file.open(temp.path(), FileAccess::Read);
    const DWORD error = ::GetLastError();

    CHECK(result == -1);
    CHECK(error == ERROR_BUSY);

    CHECK(file.valid());
    CHECK(file.native_handle() == original);
}

TEST_CASE("File: reopen after close", "[win][file][class]") {
    TemporaryFile temp;
    File file;

    REQUIRE(file.open(temp.path(), FileAccess::ReadWrite) == 0);
    REQUIRE(file.close() == 0);

    REQUIRE(file.open(temp.path(), FileAccess::Read) == 0);

    CHECK(file.valid());
}

TEST_CASE("File: class read and write", "[win][file][class]") {
    TemporaryFile temp;
    File file;

    REQUIRE(file.open(temp.path(), FileAccess::ReadWrite) == 0);

    constexpr std::string_view data = "CoreLib Windows";

    REQUIRE(file.write(data.data(), data.size()) ==
            static_cast<std::int64_t>(data.size()));

    CHECK(file.size() == static_cast<std::int64_t>(data.size()));

    REQUIRE(file.seek(0, SeekOrigin::Begin) == 0);

    std::array<char, 32> buffer{};

    REQUIRE(file.read(buffer.data(), data.size()) ==
            static_cast<std::int64_t>(data.size()));

    CHECK(std::string_view(buffer.data(), data.size()) == data);
    CHECK(file.read(buffer.data(), 1) == 0);
}

TEST_CASE("File: class positional operations", "[win][file][class]") {
    TemporaryFile temp;
    File file;

    REQUIRE(file.open(temp.path(), FileAccess::ReadWrite) == 0);
    REQUIRE(file.write("0123456789", 10) == 10);

    REQUIRE(file.seek(4, SeekOrigin::Begin) == 4);

    REQUIRE(file.write_at("ABC", 3, 2) == 3);

    CHECK(file.seek(0, SeekOrigin::Current) == 4);

    std::array<char, 16> buffer{};

    REQUIRE(file.read_at(buffer.data(), 10, 0) == 10);

    CHECK(std::string_view(buffer.data(), 10) == "01ABC56789");
    CHECK(file.seek(0, SeekOrigin::Current) == 4);
}

TEST_CASE("File: class truncate and sync", "[win][file][class]") {
    TemporaryFile temp;
    File file;

    REQUIRE(file.open(temp.path(), FileAccess::ReadWrite) == 0);

    REQUIRE(file.write("123456789", 9) == 9);
    REQUIRE(file.truncate(4) == 0);

    CHECK(file.size() == 4);

    REQUIRE(file.truncate(16) == 0);

    CHECK(file.size() == 16);
    CHECK(file.sync() == 0);
}


// ============================================================
// File class: ownership and RAII
// ============================================================

TEST_CASE("File: destructor closes handle", "[win][file][raii]") {
    TemporaryFile temp;

    HANDLE handle = INVALID_HANDLE_VALUE;

    {
        File file;

        REQUIRE(file.open(temp.path(), FileAccess::ReadWrite) == 0);

        handle = file.native_handle();

        REQUIRE(handle != INVALID_HANDLE_VALUE);
    }

    DWORD flags{};

    const BOOL result = ::GetHandleInformation(handle, &flags);
    const DWORD error = ::GetLastError();

    CHECK_FALSE(result);
    CHECK(error == ERROR_INVALID_HANDLE);
}

TEST_CASE("File: release transfers ownership", "[win][file][raii]") {
    TemporaryFile temp;

    File file;

    REQUIRE(file.open(temp.path(), FileAccess::ReadWrite) == 0);

    const HANDLE handle = file.release();
    RawHandle owner(handle);

    REQUIRE(owner.valid());

    CHECK_FALSE(file.valid());
    CHECK(file.native_handle() == INVALID_HANDLE_VALUE);

    DWORD flags{};

    CHECK(::GetHandleInformation(owner.get(), &flags) != FALSE);
}

TEST_CASE("File: reset closes previous handle", "[win][file][raii]") {
    TemporaryFile first_temp;
    TemporaryFile second_temp;

    File first;
    File second;

    REQUIRE(first.open(first_temp.path(), FileAccess::ReadWrite) == 0);
    REQUIRE(second.open(second_temp.path(), FileAccess::ReadWrite) == 0);

    const HANDLE old_handle = first.native_handle();
    const HANDLE new_handle = second.release();

    REQUIRE(first.reset(new_handle) == 0);

    CHECK(first.valid());
    CHECK(first.native_handle() == new_handle);
    CHECK_FALSE(second.valid());

    DWORD flags{};

    const BOOL result = ::GetHandleInformation(old_handle, &flags);
    const DWORD error = ::GetLastError();

    CHECK_FALSE(result);
    CHECK(error == ERROR_INVALID_HANDLE);
}

TEST_CASE("File: reset with same handle", "[win][file][raii]") {
    TemporaryFile temp;
    File file;

    REQUIRE(file.open(temp.path(), FileAccess::ReadWrite) == 0);

    const HANDLE handle = file.native_handle();

    REQUIRE(file.reset(handle) == 0);

    CHECK(file.valid());
    CHECK(file.native_handle() == handle);

    REQUIRE(file.write("X", 1) == 1);
}

TEST_CASE("File: reset to invalid handle", "[win][file][raii]") {
    TemporaryFile temp;
    File file;

    REQUIRE(file.open(temp.path(), FileAccess::ReadWrite) == 0);

    const HANDLE old_handle = file.native_handle();

    REQUIRE(file.reset() == 0);

    CHECK_FALSE(file.valid());
    CHECK(file.native_handle() == INVALID_HANDLE_VALUE);

    DWORD flags{};

    const BOOL result = ::GetHandleInformation(old_handle, &flags);
    const DWORD error = ::GetLastError();

    CHECK_FALSE(result);
    CHECK(error == ERROR_INVALID_HANDLE);
}


// ============================================================
// File class: move semantics
// ============================================================

TEST_CASE("File: move construction", "[win][file][move]") {
    TemporaryFile temp;

    File original;

    REQUIRE(original.open(temp.path(), FileAccess::ReadWrite) == 0);

    const HANDLE handle = original.native_handle();

    File moved(std::move(original));

    CHECK_FALSE(original.valid());
    CHECK(moved.valid());

    CHECK(moved.native_handle() == handle);
    CHECK(original.native_handle() == INVALID_HANDLE_VALUE);

    REQUIRE(moved.write("abc", 3) == 3);
    CHECK(moved.size() == 3);
}

TEST_CASE("File: move assignment", "[win][file][move]") {
    TemporaryFile first_temp;
    TemporaryFile second_temp;

    File first;
    File second;

    REQUIRE(first.open(first_temp.path(), FileAccess::ReadWrite) == 0);
    REQUIRE(second.open(second_temp.path(), FileAccess::ReadWrite) == 0);

    const HANDLE old_handle = first.native_handle();
    const HANDLE transferred = second.native_handle();

    first = std::move(second);

    CHECK(first.valid());
    CHECK_FALSE(second.valid());

    CHECK(first.native_handle() == transferred);

    DWORD flags{};

    const BOOL result = ::GetHandleInformation(old_handle, &flags);
    const DWORD error = ::GetLastError();

    CHECK_FALSE(result);
    CHECK(error == ERROR_INVALID_HANDLE);

    REQUIRE(first.write("moved", 5) == 5);
    CHECK(first.size() == 5);
}

TEST_CASE("File: self move assignment", "[win][file][move]") {
    TemporaryFile temp;
    File file;

    REQUIRE(file.open(temp.path(), FileAccess::ReadWrite) == 0);

    const HANDLE handle = file.native_handle();

    auto* self = &file;
    file = std::move(*self);

    CHECK(file.valid());
    CHECK(file.native_handle() == handle);

    REQUIRE(file.write("X", 1) == 1);
}

TEST_CASE("File: moving an empty object", "[win][file][move]") {
    File original;
    File moved(std::move(original));

    CHECK_FALSE(original.valid());
    CHECK_FALSE(moved.valid());

    TemporaryFile temp;

    REQUIRE(moved.open(temp.path(), FileAccess::ReadWrite) == 0);

    CHECK(moved.valid());
}
