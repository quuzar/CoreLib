
#include <catch2/catch_test_macros.hpp>
#include <win/thread.hpp>

#include <array>
#include <atomic>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <windows.h>

using namespace core::win;

namespace {

    class Event {
    public:
        Event() {
            handle_ = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);

            if (handle_ == nullptr) {
                throw std::runtime_error("CreateEventW failed");
            }
        }

        ~Event() {
            ::CloseHandle(handle_);
        }

        Event(const Event&) = delete;
        Event& operator=(const Event&) = delete;

        [[nodiscard]] HANDLE handle() const noexcept {
            return handle_;
        }

        bool wait(DWORD timeout = 3000) const noexcept {
            return ::WaitForSingleObject(handle_, timeout) == WAIT_OBJECT_0;
        }

    private:
        HANDLE handle_{};
    };


    unsigned __stdcall return_value(void*) {
        return 42;
    }

    unsigned __stdcall increment_worker(void* argument) {
        auto* counter = static_cast<std::atomic<int>*>(argument);

        ++(*counter);

        return 7;
    }

    unsigned __stdcall stress_worker(void* argument) {
        auto* counter = static_cast<std::atomic<int>*>(argument);

        for (int i = 0; i < 10000; ++i) {
            ++(*counter);
        }

        return 0;
    }

    unsigned __stdcall id_worker(void* argument) {
        auto* id = static_cast<DWORD*>(argument);

        *id = current_thread();

        return 0;
    }

    unsigned __stdcall event_worker(void* argument) {
        const HANDLE event = static_cast<HANDLE>(argument);

        return ::SetEvent(event) ? 0 : 1;
    }

    unsigned __stdcall exit_worker(void*) {
        exit_thread(73);
    }

}


// ============================================================
// Type properties
// ============================================================

TEST_CASE("Thread: type properties", "[win][thread][types]") {
    STATIC_REQUIRE_FALSE(std::is_copy_constructible_v<Thread>);
    STATIC_REQUIRE_FALSE(std::is_copy_assignable_v<Thread>);

    STATIC_REQUIRE(std::is_move_constructible_v<Thread>);
    STATIC_REQUIRE(std::is_move_assignable_v<Thread>);

    STATIC_REQUIRE(std::is_nothrow_move_constructible_v<Thread>);
    STATIC_REQUIRE(std::is_nothrow_move_assignable_v<Thread>);
}


// ============================================================
// Raw API: creation and joining
// ============================================================

TEST_CASE("Thread: raw create and join", "[win][thread][raw]") {
    HANDLE handle{};
    DWORD id{};

    REQUIRE(create_thread(
        &handle,
        return_value,
        nullptr,
        0,
        &id
    ) == 0);

    REQUIRE(handle != nullptr);

    CHECK(id != 0);
    CHECK(::GetThreadId(handle) == id);

    DWORD result{};

    REQUIRE(join_thread(handle, &result) == 0);

    CHECK(result == 42);
}

TEST_CASE("Thread: raw function argument", "[win][thread][raw]") {
    std::atomic<int> counter{0};

    HANDLE handle{};

    REQUIRE(create_thread(
        &handle,
        increment_worker,
        &counter
    ) == 0);

    DWORD result{};

    REQUIRE(join_thread(handle, &result) == 0);

    CHECK(counter.load() == 1);
    CHECK(result == 7);
}

TEST_CASE("Thread: raw join without exit code", "[win][thread][raw]") {
    HANDLE handle{};

    REQUIRE(create_thread(&handle, return_value) == 0);
    REQUIRE(join_thread(handle) == 0);
}

TEST_CASE("Thread: raw custom stack size", "[win][thread][raw]") {
    HANDLE handle{};

    REQUIRE(create_thread(
        &handle,
        return_value,
        nullptr,
        1024 * 1024
    ) == 0);

    DWORD result{};

    REQUIRE(join_thread(handle, &result) == 0);

    CHECK(result == 42);
}


// ============================================================
// Raw API: thread identity
// ============================================================

TEST_CASE("Thread: current thread identity", "[win][thread][raw]") {
    const DWORD id = current_thread();

    REQUIRE(id != 0);

    CHECK(id == ::GetCurrentThreadId());
    CHECK(equal_threads(id, id));
    CHECK_FALSE(equal_threads(id, 0));
}

TEST_CASE("Thread: worker thread identity", "[win][thread][raw]") {
    DWORD worker_id{};

    HANDLE handle{};
    DWORD created_id{};

    REQUIRE(create_thread(
        &handle,
        id_worker,
        &worker_id,
        0,
        &created_id
    ) == 0);

    REQUIRE(join_thread(handle) == 0);

    CHECK(worker_id == created_id);
    CHECK(worker_id != current_thread());

    CHECK(equal_threads(worker_id, created_id));
    CHECK_FALSE(equal_threads(worker_id, current_thread()));
}


// ============================================================
// Raw API: detach and exit
// ============================================================

TEST_CASE("Thread: raw detach", "[win][thread][raw]") {
    Event completed;

    HANDLE handle{};

    REQUIRE(create_thread(
        &handle,
        event_worker,
        completed.handle()
    ) == 0);

    REQUIRE(detach_thread(handle) == 0);

    CHECK(completed.wait());
}

TEST_CASE("Thread: raw explicit exit", "[win][thread][raw]") {
    HANDLE handle{};

    REQUIRE(create_thread(
        &handle,
        exit_worker
    ) == 0);

    DWORD result{};

    REQUIRE(join_thread(handle, &result) == 0);

    CHECK(result == 73);
}

TEST_CASE("Thread: raw yield", "[win][thread][raw]") {
    CHECK(yield_thread() == 0);
}


// ============================================================
// Raw API: errors
// ============================================================

TEST_CASE("Thread: raw null thread pointer", "[win][thread][errors]") {
    CHECK(create_thread(nullptr, return_value) == -1);
    CHECK(::GetLastError() == ERROR_INVALID_PARAMETER);
}

TEST_CASE("Thread: raw null function", "[win][thread][errors]") {
    HANDLE handle{};

    CHECK(create_thread(&handle, nullptr) == -1);
    CHECK(::GetLastError() == ERROR_INVALID_PARAMETER);

    CHECK(handle == nullptr);
}

TEST_CASE("Thread: raw invalid join handle", "[win][thread][errors]") {
    CHECK(join_thread(nullptr) == -1);
    CHECK(::GetLastError() == ERROR_INVALID_HANDLE);

    CHECK(join_thread(INVALID_HANDLE_VALUE) == -1);
    CHECK(::GetLastError() == ERROR_INVALID_HANDLE);
}

TEST_CASE("Thread: raw invalid detach handle", "[win][thread][errors]") {
    CHECK(detach_thread(nullptr) == -1);
    CHECK(::GetLastError() == ERROR_INVALID_HANDLE);

    CHECK(detach_thread(INVALID_HANDLE_VALUE) == -1);
    CHECK(::GetLastError() == ERROR_INVALID_HANDLE);
}

TEST_CASE("Thread: raw self join protection", "[win][thread][errors]") {
    const HANDLE current = ::GetCurrentThread();

    const int result = join_thread(current);
    const DWORD error = ::GetLastError();

    CHECK(result == -1);
    CHECK(error == ERROR_POSSIBLE_DEADLOCK);
}

TEST_CASE("Thread: raw oversized stack", "[win][thread][errors]") {
    if constexpr (sizeof(std::size_t) > sizeof(unsigned)) {
        const auto size = static_cast<std::size_t>(
            static_cast<std::uint64_t>(
                (std::numeric_limits<unsigned>::max)()
            ) + 1
        );

        HANDLE handle{};

        const int result = create_thread(
            &handle,
            return_value,
            nullptr,
            size
        );

        const DWORD error = ::GetLastError();

        CHECK(result == -1);
        CHECK(error == ERROR_INVALID_PARAMETER);
        CHECK(handle == nullptr);
    }
}


// ============================================================
// Thread class: lifecycle
// ============================================================

TEST_CASE("Thread: default state", "[win][thread][class]") {
    Thread thread;

    CHECK_FALSE(thread.joinable());

    CHECK(thread.state() == ThreadState::Empty);
    CHECK(thread.native_handle() == nullptr);
    CHECK(thread.id() == 0);
}

TEST_CASE("Thread: start and join", "[win][thread][class]") {
    Thread thread;

    REQUIRE(thread.start(return_value) == 0);

    CHECK(thread.joinable());
    CHECK(thread.state() == ThreadState::Joinable);

    CHECK(thread.native_handle() != nullptr);
    CHECK(thread.id() != 0);

    DWORD result{};

    REQUIRE(thread.join(&result) == 0);

    CHECK(result == 42);

    CHECK_FALSE(thread.joinable());
    CHECK(thread.state() == ThreadState::Empty);

    CHECK(thread.native_handle() == nullptr);
    CHECK(thread.id() == 0);
}

TEST_CASE("Thread: start with argument", "[win][thread][class]") {
    std::atomic<int> counter{0};

    Thread thread;

    REQUIRE(thread.start(
        increment_worker,
        &counter
    ) == 0);

    DWORD result{};

    REQUIRE(thread.join(&result) == 0);

    CHECK(counter.load() == 1);
    CHECK(result == 7);
}

TEST_CASE("Thread: start with stack size", "[win][thread][class]") {
    Thread thread;

    REQUIRE(thread.start(
        return_value,
        nullptr,
        1024 * 1024
    ) == 0);

    DWORD result{};

    REQUIRE(thread.join(&result) == 0);

    CHECK(result == 42);
}

TEST_CASE("Thread: repeated start", "[win][thread][errors]") {
    Thread thread;

    REQUIRE(thread.start(return_value) == 0);

    const HANDLE handle = thread.native_handle();

    const int status = thread.start(return_value);
    const DWORD error = ::GetLastError();

    CHECK(status == -1);
    CHECK(error == ERROR_BUSY);

    CHECK(thread.joinable());
    CHECK(thread.native_handle() == handle);

    DWORD result{};

    REQUIRE(thread.join(&result) == 0);

    CHECK(result == 42);
}

TEST_CASE("Thread: join on empty object", "[win][thread][errors]") {
    Thread thread;

    CHECK(thread.join() == -1);
    CHECK(::GetLastError() == ERROR_INVALID_HANDLE);

    CHECK_FALSE(thread.joinable());
}

TEST_CASE("Thread: detach on empty object", "[win][thread][errors]") {
    Thread thread;

    CHECK(thread.detach() == -1);
    CHECK(::GetLastError() == ERROR_INVALID_HANDLE);

    CHECK_FALSE(thread.joinable());
}

TEST_CASE("Thread: invalid start function", "[win][thread][errors]") {
    Thread thread;

    CHECK(thread.start(nullptr) == -1);
    CHECK(::GetLastError() == ERROR_INVALID_PARAMETER);

    CHECK_FALSE(thread.joinable());
    CHECK(thread.state() == ThreadState::Empty);
}

TEST_CASE("Thread: reuse after join", "[win][thread][lifecycle]") {
    Thread thread;

    REQUIRE(thread.start(return_value) == 0);
    REQUIRE(thread.join() == 0);

    REQUIRE(thread.start(return_value) == 0);

    DWORD result{};

    REQUIRE(thread.join(&result) == 0);

    CHECK(result == 42);
    CHECK_FALSE(thread.joinable());
}


// ============================================================
// Thread class: detach
// ============================================================

TEST_CASE("Thread: detach", "[win][thread][class]") {
    Event completed;

    Thread thread;

    REQUIRE(thread.start(
        event_worker,
        completed.handle()
    ) == 0);

    REQUIRE(thread.detach() == 0);

    CHECK_FALSE(thread.joinable());
    CHECK(thread.state() == ThreadState::Detached);

    CHECK(thread.native_handle() == nullptr);
    CHECK(thread.id() == 0);

    CHECK(completed.wait());
}

TEST_CASE("Thread: join after detach", "[win][thread][errors]") {
    Event completed;

    Thread thread;

    REQUIRE(thread.start(
        event_worker,
        completed.handle()
    ) == 0);

    REQUIRE(thread.detach() == 0);

    CHECK(thread.join() == -1);
    CHECK(::GetLastError() == ERROR_INVALID_HANDLE);

    CHECK(completed.wait());
}

TEST_CASE("Thread: reuse after detach", "[win][thread][lifecycle]") {
    Event completed;

    Thread thread;

    REQUIRE(thread.start(
        event_worker,
        completed.handle()
    ) == 0);

    REQUIRE(thread.detach() == 0);
    CHECK(completed.wait());

    REQUIRE(thread.start(return_value) == 0);

    DWORD result{};

    REQUIRE(thread.join(&result) == 0);

    CHECK(result == 42);
}


// ============================================================
// Thread class: move semantics
// ============================================================

TEST_CASE("Thread: move construction", "[win][thread][move]") {
    Thread source;

    REQUIRE(source.start(return_value) == 0);

    const HANDLE handle = source.native_handle();
    const DWORD id = source.id();

    Thread destination(std::move(source));

    CHECK_FALSE(source.joinable());
    CHECK(source.state() == ThreadState::Empty);

    CHECK(source.native_handle() == nullptr);
    CHECK(source.id() == 0);

    CHECK(destination.joinable());
    CHECK(destination.state() == ThreadState::Joinable);

    CHECK(destination.native_handle() == handle);
    CHECK(destination.id() == id);

    DWORD result{};

    REQUIRE(destination.join(&result) == 0);

    CHECK(result == 42);
}

TEST_CASE("Thread: move assignment", "[win][thread][move]") {
    Thread source;
    Thread destination;

    REQUIRE(source.start(return_value) == 0);

    const HANDLE handle = source.native_handle();
    const DWORD id = source.id();

    destination = std::move(source);

    CHECK_FALSE(source.joinable());
    CHECK(source.state() == ThreadState::Empty);

    CHECK(destination.joinable());

    CHECK(destination.native_handle() == handle);
    CHECK(destination.id() == id);

    DWORD result{};

    REQUIRE(destination.join(&result) == 0);

    CHECK(result == 42);
}

TEST_CASE("Thread: self move assignment", "[win][thread][move]") {
    Thread thread;

    REQUIRE(thread.start(return_value) == 0);

    const HANDLE handle = thread.native_handle();
    const DWORD id = thread.id();

    auto* self = &thread;
    thread = std::move(*self);

    CHECK(thread.joinable());
    CHECK(thread.native_handle() == handle);
    CHECK(thread.id() == id);

    DWORD result{};

    REQUIRE(thread.join(&result) == 0);

    CHECK(result == 42);
}

TEST_CASE("Thread: move empty object", "[win][thread][move]") {
    Thread source;
    Thread destination(std::move(source));

    CHECK_FALSE(source.joinable());
    CHECK_FALSE(destination.joinable());

    CHECK(source.state() == ThreadState::Empty);
    CHECK(destination.state() == ThreadState::Empty);

    REQUIRE(destination.start(return_value) == 0);

    DWORD result{};

    REQUIRE(destination.join(&result) == 0);

    CHECK(result == 42);
}

TEST_CASE("Thread: move detached object", "[win][thread][move]") {
    Event completed;

    Thread source;

    REQUIRE(source.start(
        event_worker,
        completed.handle()
    ) == 0);

    REQUIRE(source.detach() == 0);

    Thread destination(std::move(source));

    CHECK(source.state() == ThreadState::Empty);
    CHECK(destination.state() == ThreadState::Detached);

    CHECK_FALSE(source.joinable());
    CHECK_FALSE(destination.joinable());

    CHECK(completed.wait());
}


// ============================================================
// Thread class: concurrency
// ============================================================

TEST_CASE("Thread: multiple concurrent workers", "[win][thread][concurrent]") {
    constexpr std::size_t workers = 8;
    constexpr int iterations = 10000;

    std::atomic<int> counter{0};

    std::array<Thread, workers> threads{};

    int started = 0;

    for (auto& thread : threads) {
        if (thread.start(stress_worker, &counter) == 0) {
            ++started;
        }
    }

    int joined = 0;

    for (auto& thread : threads) {
        if (thread.joinable() && thread.join() == 0) {
            ++joined;
        }
    }

    CHECK(started == static_cast<int>(workers));
    CHECK(joined == static_cast<int>(workers));

    CHECK(counter.load() == static_cast<int>(workers) * iterations);
}
