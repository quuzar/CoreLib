
#include <catch2/catch_test_macros.hpp>
#include <win/sync.hpp>

#include <array>
#include <atomic>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <vector>
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

        bool signal() const noexcept {
            return ::SetEvent(handle_) != FALSE;
        }

        bool wait(DWORD timeout = 3000) const noexcept {
            return ::WaitForSingleObject(handle_, timeout) == WAIT_OBJECT_0;
        }

    private:
        HANDLE handle_{};
    };

}


// ============================================================
// Type properties
// ============================================================

TEST_CASE("Sync: type properties", "[win][sync][types]") {
    STATIC_REQUIRE_FALSE(std::is_copy_constructible_v<Mutex>);
    STATIC_REQUIRE_FALSE(std::is_move_constructible_v<Mutex>);

    STATIC_REQUIRE_FALSE(std::is_copy_constructible_v<ConditionVariable>);
    STATIC_REQUIRE_FALSE(std::is_move_constructible_v<ConditionVariable>);

    STATIC_REQUIRE_FALSE(std::is_copy_constructible_v<Semaphore>);
    STATIC_REQUIRE_FALSE(std::is_move_constructible_v<Semaphore>);

    STATIC_REQUIRE_FALSE(std::is_copy_constructible_v<ReadWriteLock>);
    STATIC_REQUIRE_FALSE(std::is_move_constructible_v<ReadWriteLock>);
}


// ============================================================
// Mutex: raw functions
// ============================================================

TEST_CASE("Sync: raw mutex lifecycle", "[win][sync][mutex][raw]") {
    CRITICAL_SECTION mutex{};

    REQUIRE(init_mutex(&mutex) == 0);

    REQUIRE(lock_mutex(&mutex) == 0);
    REQUIRE(unlock_mutex(&mutex) == 0);

    REQUIRE(destroy_mutex(&mutex) == 0);
}

TEST_CASE("Sync: raw mutex recursion", "[win][sync][mutex][raw]") {
    CRITICAL_SECTION mutex{};

    REQUIRE(init_mutex(&mutex) == 0);

    REQUIRE(lock_mutex(&mutex) == 0);
    REQUIRE(lock_mutex(&mutex) == 0);
    REQUIRE(try_lock_mutex(&mutex) == 0);

    REQUIRE(unlock_mutex(&mutex) == 0);
    REQUIRE(unlock_mutex(&mutex) == 0);
    REQUIRE(unlock_mutex(&mutex) == 0);

    REQUIRE(destroy_mutex(&mutex) == 0);
}

TEST_CASE("Sync: raw mutex contention", "[win][sync][mutex][concurrent]") {
    CRITICAL_SECTION mutex{};

    REQUIRE(init_mutex(&mutex) == 0);
    REQUIRE(lock_mutex(&mutex) == 0);

    Event completed;
    std::atomic<int> result{0};

    std::jthread worker([&] {
        const int status = try_lock_mutex(&mutex);
        const DWORD error = ::GetLastError();

        if (status == 0) {
            unlock_mutex(&mutex);
            result.store(-1);
        } else {
            result.store(error == ERROR_BUSY ? 1 : -2);
        }

        completed.signal();
    });

    CHECK(completed.wait());
    CHECK(result.load() == 1);

    REQUIRE(unlock_mutex(&mutex) == 0);

    worker.join();

    REQUIRE(destroy_mutex(&mutex) == 0);
}

TEST_CASE("Sync: raw mutex invalid arguments", "[win][sync][mutex][errors]") {
    CHECK(init_mutex(nullptr) == -1);
    CHECK(::GetLastError() == ERROR_INVALID_PARAMETER);

    CHECK(lock_mutex(nullptr) == -1);
    CHECK(::GetLastError() == ERROR_INVALID_PARAMETER);

    CHECK(try_lock_mutex(nullptr) == -1);
    CHECK(::GetLastError() == ERROR_INVALID_PARAMETER);

    CHECK(unlock_mutex(nullptr) == -1);
    CHECK(::GetLastError() == ERROR_INVALID_PARAMETER);

    CHECK(destroy_mutex(nullptr) == -1);
    CHECK(::GetLastError() == ERROR_INVALID_PARAMETER);
}


// ============================================================
// Mutex: RAII class
// ============================================================

TEST_CASE("Sync: mutex lifecycle", "[win][sync][mutex]") {
    Mutex mutex;

    CHECK_FALSE(mutex.valid());
    CHECK(mutex.native_handle() == nullptr);

    CHECK(mutex.lock() == -1);
    CHECK(::GetLastError() == ERROR_INVALID_PARAMETER);

    REQUIRE(mutex.init() == 0);

    CHECK(mutex.valid());
    CHECK(mutex.native_handle() != nullptr);

    CHECK(mutex.init() == -1);
    CHECK(::GetLastError() == ERROR_BUSY);

    REQUIRE(mutex.lock() == 0);
    REQUIRE(mutex.try_lock() == 0);

    REQUIRE(mutex.unlock() == 0);
    REQUIRE(mutex.unlock() == 0);

    REQUIRE(mutex.destroy() == 0);

    CHECK_FALSE(mutex.valid());
    CHECK(mutex.destroy() == 0);

    REQUIRE(mutex.init() == 0);
    REQUIRE(mutex.destroy() == 0);
}

TEST_CASE("Sync: mutex concurrent counter", "[win][sync][mutex][concurrent]") {
    Mutex mutex;
    REQUIRE(mutex.init() == 0);

    constexpr int workers = 8;
    constexpr int iterations = 5000;

    int counter = 0;
    std::atomic<int> failures{0};

    std::vector<std::jthread> threads;

    for (int i = 0; i < workers; ++i) {
        threads.emplace_back([&] {
            for (int j = 0; j < iterations; ++j) {
                if (mutex.lock() == -1) {
                    ++failures;
                    return;
                }

                ++counter;

                if (mutex.unlock() == -1) {
                    ++failures;
                    return;
                }
            }
        });
    }

    threads.clear();

    CHECK(failures.load() == 0);
    CHECK(counter == workers * iterations);

    REQUIRE(mutex.destroy() == 0);
}


// ============================================================
// ConditionVariable: raw functions
// ============================================================

TEST_CASE("Sync: raw condition lifecycle", "[win][sync][condition][raw]") {
    CONDITION_VARIABLE condition{};

    REQUIRE(init_condition(&condition) == 0);

    REQUIRE(signal_condition(&condition) == 0);
    REQUIRE(broadcast_condition(&condition) == 0);

    REQUIRE(destroy_condition(&condition) == 0);
}

TEST_CASE("Sync: raw condition invalid arguments", "[win][sync][condition][errors]") {
    CHECK(init_condition(nullptr) == -1);
    CHECK(::GetLastError() == ERROR_INVALID_PARAMETER);

    CHECK(destroy_condition(nullptr) == -1);
    CHECK(::GetLastError() == ERROR_INVALID_PARAMETER);

    CHECK(wait_condition(nullptr, nullptr, 0) == -1);
    CHECK(::GetLastError() == ERROR_INVALID_PARAMETER);

    CHECK(signal_condition(nullptr) == -1);
    CHECK(::GetLastError() == ERROR_INVALID_PARAMETER);

    CHECK(broadcast_condition(nullptr) == -1);
    CHECK(::GetLastError() == ERROR_INVALID_PARAMETER);
}


// ============================================================
// ConditionVariable: RAII class
// ============================================================

TEST_CASE("Sync: condition lifecycle", "[win][sync][condition]") {
    ConditionVariable condition;

    CHECK_FALSE(condition.valid());
    CHECK(condition.native_handle() == nullptr);

    CHECK(condition.signal() == -1);
    CHECK(::GetLastError() == ERROR_INVALID_PARAMETER);

    REQUIRE(condition.init() == 0);

    CHECK(condition.valid());
    CHECK(condition.native_handle() != nullptr);

    CHECK(condition.init() == -1);
    CHECK(::GetLastError() == ERROR_BUSY);

    REQUIRE(condition.signal() == 0);
    REQUIRE(condition.broadcast() == 0);

    REQUIRE(condition.destroy() == 0);

    CHECK_FALSE(condition.valid());

    REQUIRE(condition.init() == 0);
    REQUIRE(condition.destroy() == 0);
}

TEST_CASE("Sync: condition timeout", "[win][sync][condition][timeout]") {
    Mutex mutex;
    ConditionVariable condition;

    REQUIRE(mutex.init() == 0);
    REQUIRE(condition.init() == 0);

    REQUIRE(mutex.lock() == 0);

    const int result = condition.wait(mutex, 20);
    const DWORD error = ::GetLastError();

    CHECK(result == -1);
    CHECK(error == ERROR_TIMEOUT);

    // The critical section must be reacquired after timeout.
    REQUIRE(mutex.unlock() == 0);

    REQUIRE(condition.destroy() == 0);
    REQUIRE(mutex.destroy() == 0);
}

TEST_CASE("Sync: condition signal wakes worker", "[win][sync][condition][concurrent]") {
    Mutex mutex;
    ConditionVariable condition;
    Event started;

    REQUIRE(mutex.init() == 0);
    REQUIRE(condition.init() == 0);

    bool ready = false;
    std::atomic<int> result{0};

    std::jthread worker([&] {
        if (mutex.lock() == -1) {
            result.store(-1);
            started.signal();
            return;
        }

        started.signal();

        while (!ready) {
            if (condition.wait(mutex, 2000) == -1) {
                result.store(-2);
                mutex.unlock();
                return;
            }
        }

        result.store(1);
        mutex.unlock();
    });

    CHECK(started.wait());

    REQUIRE(mutex.lock() == 0);

    ready = true;

    REQUIRE(condition.signal() == 0);
    REQUIRE(mutex.unlock() == 0);

    worker.join();

    CHECK(result.load() == 1);

    REQUIRE(condition.destroy() == 0);
    REQUIRE(mutex.destroy() == 0);
}

TEST_CASE("Sync: condition broadcast wakes workers", "[win][sync][condition][concurrent]") {
    Mutex mutex;
    ConditionVariable condition;
    Event started;

    REQUIRE(mutex.init() == 0);
    REQUIRE(condition.init() == 0);

    constexpr int workers = 4;

    int waiting = 0;
    bool ready = false;

    std::atomic<int> awakened{0};
    std::atomic<int> failures{0};

    std::vector<std::jthread> threads;

    for (int i = 0; i < workers; ++i) {
        threads.emplace_back([&] {
            if (mutex.lock() == -1) {
                ++failures;
                return;
            }

            ++waiting;

            if (waiting == workers) {
                started.signal();
            }

            while (!ready) {
                if (condition.wait(mutex, 3000) == -1) {
                    ++failures;
                    mutex.unlock();
                    return;
                }
            }

            ++awakened;
            mutex.unlock();
        });
    }

    CHECK(started.wait());

    REQUIRE(mutex.lock() == 0);

    ready = true;

    REQUIRE(condition.broadcast() == 0);
    REQUIRE(mutex.unlock() == 0);

    threads.clear();

    CHECK(failures.load() == 0);
    CHECK(awakened.load() == workers);

    REQUIRE(condition.destroy() == 0);
    REQUIRE(mutex.destroy() == 0);
}


// ============================================================
// Semaphore: raw functions
// ============================================================

TEST_CASE("Sync: raw semaphore lifecycle", "[win][sync][semaphore][raw]") {
    HANDLE semaphore{};

    REQUIRE(init_semaphore(&semaphore, 0, 3) == 0);
    REQUIRE(semaphore != nullptr);

    CHECK(try_wait_semaphore(semaphore) == -1);
    CHECK(::GetLastError() == ERROR_TIMEOUT);

    REQUIRE(post_semaphore(semaphore, 2) == 0);

    REQUIRE(wait_semaphore(semaphore, 0) == 0);
    REQUIRE(wait_semaphore(semaphore, 0) == 0);

    CHECK(try_wait_semaphore(semaphore) == -1);
    CHECK(::GetLastError() == ERROR_TIMEOUT);

    REQUIRE(destroy_semaphore(semaphore) == 0);
}

TEST_CASE("Sync: raw semaphore maximum count", "[win][sync][semaphore][errors]") {
    HANDLE semaphore{};

    REQUIRE(init_semaphore(&semaphore, 0, 2) == 0);

    REQUIRE(post_semaphore(semaphore, 2) == 0);

    CHECK(post_semaphore(semaphore) == -1);
    CHECK(::GetLastError() == ERROR_TOO_MANY_POSTS);

    REQUIRE(wait_semaphore(semaphore, 0) == 0);
    REQUIRE(wait_semaphore(semaphore, 0) == 0);

    REQUIRE(destroy_semaphore(semaphore) == 0);
}

TEST_CASE("Sync: raw semaphore invalid arguments", "[win][sync][semaphore][errors]") {
    HANDLE semaphore{};

    CHECK(init_semaphore(nullptr) == -1);
    CHECK(::GetLastError() == ERROR_INVALID_PARAMETER);

    CHECK(init_semaphore(&semaphore, -1, 3) == -1);
    CHECK(::GetLastError() == ERROR_INVALID_PARAMETER);

    CHECK(init_semaphore(&semaphore, 4, 3) == -1);
    CHECK(::GetLastError() == ERROR_INVALID_PARAMETER);

    CHECK(destroy_semaphore(nullptr) == -1);
    CHECK(::GetLastError() == ERROR_INVALID_HANDLE);

    CHECK(wait_semaphore(nullptr, 0) == -1);
    CHECK(::GetLastError() == ERROR_INVALID_HANDLE);

    CHECK(post_semaphore(nullptr) == -1);
    CHECK(::GetLastError() == ERROR_INVALID_HANDLE);
}


// ============================================================
// Semaphore: RAII class
// ============================================================

TEST_CASE("Sync: semaphore lifecycle", "[win][sync][semaphore]") {
    Semaphore semaphore;

    CHECK_FALSE(semaphore.valid());
    CHECK(semaphore.native_handle() == nullptr);

    REQUIRE(semaphore.init(1, 3) == 0);

    CHECK(semaphore.valid());
    CHECK(semaphore.native_handle() != nullptr);

    CHECK(semaphore.init() == -1);
    CHECK(::GetLastError() == ERROR_BUSY);

    REQUIRE(semaphore.wait(0) == 0);

    CHECK(semaphore.try_wait() == -1);
    CHECK(::GetLastError() == ERROR_TIMEOUT);

    REQUIRE(semaphore.post() == 0);
    REQUIRE(semaphore.wait(0) == 0);

    REQUIRE(semaphore.destroy() == 0);

    CHECK_FALSE(semaphore.valid());
    CHECK(semaphore.destroy() == 0);

    REQUIRE(semaphore.init() == 0);
    REQUIRE(semaphore.destroy() == 0);
}

TEST_CASE("Sync: semaphore wakes waiting worker", "[win][sync][semaphore][concurrent]") {
    Semaphore semaphore;

    REQUIRE(semaphore.init(0, 1) == 0);

    std::atomic<int> result{0};

    std::jthread worker([&] {
        result.store(
            semaphore.wait(3000) == 0 ? 1 : -1
        );
    });

    REQUIRE(semaphore.post() == 0);

    worker.join();

    CHECK(result.load() == 1);

    REQUIRE(semaphore.destroy() == 0);
}


// ============================================================
// ReadWriteLock: raw functions
// ============================================================

TEST_CASE("Sync: raw rwlock lifecycle", "[win][sync][rwlock][raw]") {
    SRWLOCK lock{};

    REQUIRE(init_rwlock(&lock) == 0);

    REQUIRE(read_lock(&lock) == 0);
    REQUIRE(unlock_read(&lock) == 0);

    REQUIRE(write_lock(&lock) == 0);
    REQUIRE(unlock_write(&lock) == 0);

    REQUIRE(destroy_rwlock(&lock) == 0);
}

TEST_CASE("Sync: raw rwlock invalid arguments", "[win][sync][rwlock][errors]") {
    CHECK(init_rwlock(nullptr) == -1);
    CHECK(::GetLastError() == ERROR_INVALID_PARAMETER);

    CHECK(read_lock(nullptr) == -1);
    CHECK(::GetLastError() == ERROR_INVALID_PARAMETER);

    CHECK(write_lock(nullptr) == -1);
    CHECK(::GetLastError() == ERROR_INVALID_PARAMETER);

    CHECK(try_read_lock(nullptr) == -1);
    CHECK(::GetLastError() == ERROR_INVALID_PARAMETER);

    CHECK(try_write_lock(nullptr) == -1);
    CHECK(::GetLastError() == ERROR_INVALID_PARAMETER);

    CHECK(unlock_read(nullptr) == -1);
    CHECK(::GetLastError() == ERROR_INVALID_PARAMETER);

    CHECK(unlock_write(nullptr) == -1);
    CHECK(::GetLastError() == ERROR_INVALID_PARAMETER);

    CHECK(destroy_rwlock(nullptr) == -1);
    CHECK(::GetLastError() == ERROR_INVALID_PARAMETER);
}


// ============================================================
// ReadWriteLock: RAII class
// ============================================================

TEST_CASE("Sync: rwlock lifecycle", "[win][sync][rwlock]") {
    ReadWriteLock lock;

    CHECK_FALSE(lock.valid());
    CHECK(lock.native_handle() == nullptr);

    CHECK(lock.lock_read() == -1);
    CHECK(::GetLastError() == ERROR_INVALID_PARAMETER);

    REQUIRE(lock.init() == 0);

    CHECK(lock.valid());
    CHECK(lock.native_handle() != nullptr);

    CHECK(lock.init() == -1);
    CHECK(::GetLastError() == ERROR_BUSY);

    REQUIRE(lock.try_lock_read() == 0);
    REQUIRE(lock.unlock_read() == 0);

    REQUIRE(lock.try_lock_write() == 0);
    REQUIRE(lock.unlock_write() == 0);

    REQUIRE(lock.destroy() == 0);

    CHECK_FALSE(lock.valid());

    REQUIRE(lock.init() == 0);
    REQUIRE(lock.destroy() == 0);
}

TEST_CASE("Sync: rwlock shared readers", "[win][sync][rwlock][concurrent]") {
    ReadWriteLock lock;
    REQUIRE(lock.init() == 0);

    REQUIRE(lock.lock_read() == 0);

    std::atomic<int> result{0};

    std::jthread worker([&] {
        if (lock.try_lock_read() == 0) {
            result.store(1);
            lock.unlock_read();
        } else {
            result.store(-1);
        }
    });

    worker.join();

    CHECK(result.load() == 1);

    REQUIRE(lock.unlock_read() == 0);
    REQUIRE(lock.destroy() == 0);
}

TEST_CASE("Sync: rwlock exclusive access", "[win][sync][rwlock][concurrent]") {
    ReadWriteLock lock;
    REQUIRE(lock.init() == 0);

    REQUIRE(lock.lock_write() == 0);

    std::atomic<int> result{0};

    std::jthread worker([&] {
        const int read_status = lock.try_lock_read();
        const DWORD read_error = ::GetLastError();

        if (read_status == 0) {
            lock.unlock_read();
        }

        const int write_status = lock.try_lock_write();
        const DWORD write_error = ::GetLastError();

        if (write_status == 0) {
            lock.unlock_write();
        }

        result.store(
            read_status == -1 &&
            write_status == -1 &&
            read_error == ERROR_BUSY &&
            write_error == ERROR_BUSY ? 1 : -1
        );
    });

    worker.join();

    CHECK(result.load() == 1);

    REQUIRE(lock.unlock_write() == 0);
    REQUIRE(lock.destroy() == 0);
}

TEST_CASE("Sync: rwlock concurrent writers", "[win][sync][rwlock][concurrent]") {
    ReadWriteLock lock;

    REQUIRE(lock.init() == 0);

    constexpr int workers = 6;
    constexpr int iterations = 4000;

    int counter = 0;
    std::atomic<int> failures{0};

    std::vector<std::jthread> threads;

    for (int i = 0; i < workers; ++i) {
        threads.emplace_back([&] {
            for (int j = 0; j < iterations; ++j) {
                if (lock.lock_write() == -1) {
                    ++failures;
                    return;
                }

                ++counter;

                if (lock.unlock_write() == -1) {
                    ++failures;
                    return;
                }
            }
        });
    }

    threads.clear();

    CHECK(failures.load() == 0);
    CHECK(counter == workers * iterations);

    REQUIRE(lock.destroy() == 0);
}
