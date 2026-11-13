#include <catch2/catch_test_macros.hpp>
#include <psx/sync.hpp>

#include <atomic>
#include <cerrno>
#include <pthread.h>
#include <semaphore.h>
#include <thread>

using namespace core::psx;

namespace {
constexpr auto normal_mutex = static_cast<MutexType>(PTHREAD_MUTEX_NORMAL);
constexpr auto recursive_mutex = static_cast<MutexType>(PTHREAD_MUTEX_RECURSIVE);
} // namespace

TEST_CASE("Sync: raw mutex lifecycle and busy try_lock", "[psx][sync][mutex][raw]") {
    pthread_mutex_t mutex{};
    REQUIRE(init_mutex(&mutex, normal_mutex) == 0);
    REQUIRE(lock_mutex(&mutex) == 0);

    errno = 0;
    CHECK(try_lock_mutex(&mutex) == -1);
    CHECK(errno == EBUSY);

    REQUIRE(unlock_mutex(&mutex) == 0);
    REQUIRE(try_lock_mutex(&mutex) == 0);
    REQUIRE(unlock_mutex(&mutex) == 0);
    REQUIRE(destroy_mutex(&mutex) == 0);
}

TEST_CASE("Sync: raw recursive mutex", "[psx][sync][mutex][raw]") {
    pthread_mutex_t mutex{};
    REQUIRE(init_mutex(&mutex, recursive_mutex) == 0);
    REQUIRE(lock_mutex(&mutex) == 0);
    REQUIRE(try_lock_mutex(&mutex) == 0);
    REQUIRE(unlock_mutex(&mutex) == 0);
    REQUIRE(unlock_mutex(&mutex) == 0);
    REQUIRE(destroy_mutex(&mutex) == 0);
}

TEST_CASE("Sync: mutex class lifecycle", "[psx][sync][mutex]") {
    Mutex mutex;
    CHECK_FALSE(mutex.valid());
    CHECK(mutex.native_handle() == nullptr);

    errno = 0;
    CHECK(mutex.lock() == -1);
    CHECK(errno == EINVAL);

    REQUIRE(mutex.init(normal_mutex) == 0);
    CHECK(mutex.valid());
    CHECK(mutex.native_handle() != nullptr);

    errno = 0;
    CHECK(mutex.init(normal_mutex) == -1);
    CHECK(errno == EBUSY);

    REQUIRE(mutex.lock() == 0);
    errno = 0;
    CHECK(mutex.try_lock() == -1);
    CHECK(errno == EBUSY);
    REQUIRE(mutex.unlock() == 0);
    REQUIRE(mutex.try_lock() == 0);
    REQUIRE(mutex.unlock() == 0);
    REQUIRE(mutex.destroy() == 0);
    CHECK_FALSE(mutex.valid());
    CHECK(mutex.destroy() == 0);
    REQUIRE(mutex.init(normal_mutex) == 0);
    REQUIRE(mutex.destroy() == 0);
}

TEST_CASE("Sync: raw condition variable lifecycle", "[psx][sync][condition][raw]") {
    pthread_cond_t condition{};
    REQUIRE(init_condition(&condition) == 0);
    REQUIRE(signal_condition(&condition) == 0);
    REQUIRE(broadcast_condition(&condition) == 0);
    REQUIRE(destroy_condition(&condition) == 0);
}

TEST_CASE("Sync: condition variable wakes a waiting thread", "[psx][sync][condition]") {
    Mutex mutex;
    ConditionVariable condition;
    REQUIRE(mutex.init(normal_mutex) == 0);
    REQUIRE(condition.init() == 0);

    bool ready = false;
    std::atomic<int> result{0};
    std::thread worker([&] {
        if (mutex.lock() == -1) { result.store(-1); return; }
        while (!ready) {
            if (condition.wait(mutex) == -1) {
                result.store(-2);
                mutex.unlock();
                return;
            }
        }
        result.store(1);
        mutex.unlock();
    });

    const int locked = mutex.lock();
    if (locked == 0) {
        ready = true;
        const int signalled = condition.signal();
        const int unlocked = mutex.unlock();
        CHECK(signalled == 0);
        CHECK(unlocked == 0);
    }
    worker.join();
    REQUIRE(locked == 0);
    CHECK(result.load() == 1);
    REQUIRE(condition.destroy() == 0);
    REQUIRE(mutex.destroy() == 0);
}

TEST_CASE("Sync: condition variable broadcast wakes all waiters", "[psx][sync][condition]") {
    Mutex mutex;
    ConditionVariable condition;
    REQUIRE(mutex.init(normal_mutex) == 0);
    REQUIRE(condition.init() == 0);

    bool ready = false;
    std::atomic<int> awakened{0};
    auto waiter = [&] {
        if (mutex.lock() == -1) return;
        while (!ready) {
            if (condition.wait(mutex) == -1) {
                mutex.unlock();
                return;
            }
        }
        awakened.fetch_add(1);
        mutex.unlock();
    };

    std::thread a(waiter);
    std::thread b(waiter);
    std::thread c(waiter);

    const int locked = mutex.lock();
    if (locked == 0) {
        ready = true;
        const int signalled = condition.broadcast();
        const int unlocked = mutex.unlock();
        CHECK(signalled == 0);
        CHECK(unlocked == 0);
    }
    a.join();
    b.join();
    c.join();
    REQUIRE(locked == 0);
    CHECK(awakened.load() == 3);
    REQUIRE(condition.destroy() == 0);
    REQUIRE(mutex.destroy() == 0);
}

TEST_CASE("Sync: invalid condition object operations", "[psx][sync][condition][errors]") {
    Mutex mutex;
    ConditionVariable condition;
    CHECK_FALSE(condition.valid());
    CHECK(condition.native_handle() == nullptr);

    errno = 0;
    CHECK(condition.signal() == -1);
    CHECK(errno == EINVAL);

    errno = 0;
    CHECK(condition.broadcast() == -1);
    CHECK(errno == EINVAL);

    errno = 0;
    CHECK(condition.wait(mutex) == -1);
    CHECK(errno == EINVAL);

    REQUIRE(condition.init() == 0);
    errno = 0;
    CHECK(condition.init() == -1);
    CHECK(errno == EBUSY);
    REQUIRE(condition.destroy() == 0);
    CHECK(condition.destroy() == 0);
}

TEST_CASE("Sync: raw semaphore wait, try_wait and post", "[psx][sync][semaphore][raw]") {
    sem_t semaphore{};
    REQUIRE(init_semaphore(&semaphore, 1) == 0);
    REQUIRE(wait_semaphore(&semaphore) == 0);

    errno = 0;
    CHECK(try_wait_semaphore(&semaphore) == -1);
    CHECK(errno == EAGAIN);

    REQUIRE(post_semaphore(&semaphore) == 0);
    int value = -1;
    REQUIRE(value_semaphore(&semaphore, &value) == 0);
    CHECK(value == 1);
    REQUIRE(destroy_semaphore(&semaphore) == 0);
}

TEST_CASE("Sync: semaphore class lifecycle and counting", "[psx][sync][semaphore]") {
    Semaphore semaphore;
    CHECK_FALSE(semaphore.valid());
    CHECK(semaphore.native_handle() == nullptr);

    errno = 0;
    CHECK(semaphore.post() == -1);
    CHECK(errno == EINVAL);

    REQUIRE(semaphore.init(2) == 0);
    CHECK(semaphore.valid());
    errno = 0;
    CHECK(semaphore.init(2) == -1);
    CHECK(errno == EBUSY);

    int value = -1;
    REQUIRE(semaphore.value(&value) == 0);
    CHECK(value == 2);
    REQUIRE(semaphore.try_wait() == 0);
    REQUIRE(semaphore.wait() == 0);
    REQUIRE(semaphore.value(&value) == 0);
    CHECK(value == 0);

    errno = 0;
    CHECK(semaphore.try_wait() == -1);
    CHECK(errno == EAGAIN);

    REQUIRE(semaphore.post() == 0);
    REQUIRE(semaphore.value(&value) == 0);
    CHECK(value == 1);
    REQUIRE(semaphore.destroy() == 0);
    CHECK_FALSE(semaphore.valid());
    CHECK(semaphore.destroy() == 0);
}

TEST_CASE("Sync: raw read-write lock", "[psx][sync][rwlock][raw]") {
    pthread_rwlock_t lock{};
    REQUIRE(init_rwlock(&lock) == 0);
    REQUIRE(read_lock(&lock) == 0);

    errno = 0;
    CHECK(try_write_lock(&lock) == -1);
    CHECK(errno == EBUSY);

    REQUIRE(unlock_rwlock(&lock) == 0);
    REQUIRE(write_lock(&lock) == 0);
    REQUIRE(unlock_rwlock(&lock) == 0);
    REQUIRE(destroy_rwlock(&lock) == 0);
}

TEST_CASE("Sync: read-write lock class lifecycle", "[psx][sync][rwlock]") {
    ReadWriteLock lock;
    CHECK_FALSE(lock.valid());
    CHECK(lock.native_handle() == nullptr);

    errno = 0;
    CHECK(lock.lock_read() == -1);
    CHECK(errno == EINVAL);

    REQUIRE(lock.init() == 0);
    CHECK(lock.valid());
    errno = 0;
    CHECK(lock.init() == -1);
    CHECK(errno == EBUSY);

    REQUIRE(lock.lock_read() == 0);
    REQUIRE(lock.try_lock_read() == 0);
    errno = 0;
    CHECK(lock.try_lock_write() == -1);
    CHECK(errno == EBUSY);
    REQUIRE(lock.unlock() == 0);
    REQUIRE(lock.unlock() == 0);

    REQUIRE(lock.lock_write() == 0);
    REQUIRE(lock.unlock() == 0);
    REQUIRE(lock.try_lock_write() == 0);
    REQUIRE(lock.unlock() == 0);

    REQUIRE(lock.destroy() == 0);
    CHECK_FALSE(lock.valid());
    CHECK(lock.destroy() == 0);
}

TEST_CASE("Sync: null raw arguments return EINVAL", "[psx][sync][errors]") {
    errno = 0;
    CHECK(init_mutex(nullptr, normal_mutex) == -1);
    CHECK(errno == EINVAL);
    errno = 0;
    CHECK(destroy_mutex(nullptr) == -1);
    CHECK(errno == EINVAL);
    errno = 0;
    CHECK(lock_mutex(nullptr) == -1);
    CHECK(errno == EINVAL);
    errno = 0;
    CHECK(try_lock_mutex(nullptr) == -1);
    CHECK(errno == EINVAL);
    errno = 0;
    CHECK(unlock_mutex(nullptr) == -1);
    CHECK(errno == EINVAL);

    errno = 0;
    CHECK(init_condition(nullptr) == -1);
    CHECK(errno == EINVAL);
    errno = 0;
    CHECK(destroy_condition(nullptr) == -1);
    CHECK(errno == EINVAL);
    errno = 0;
    CHECK(wait_condition(nullptr, nullptr) == -1);
    CHECK(errno == EINVAL);
    errno = 0;
    CHECK(signal_condition(nullptr) == -1);
    CHECK(errno == EINVAL);
    errno = 0;
    CHECK(broadcast_condition(nullptr) == -1);
    CHECK(errno == EINVAL);

    errno = 0;
    CHECK(init_semaphore(nullptr, 0) == -1);
    CHECK(errno == EINVAL);
    errno = 0;
    CHECK(destroy_semaphore(nullptr) == -1);
    CHECK(errno == EINVAL);
    errno = 0;
    CHECK(wait_semaphore(nullptr) == -1);
    CHECK(errno == EINVAL);
    errno = 0;
    CHECK(try_wait_semaphore(nullptr) == -1);
    CHECK(errno == EINVAL);
    errno = 0;
    CHECK(post_semaphore(nullptr) == -1);
    CHECK(errno == EINVAL);
    errno = 0;
    CHECK(value_semaphore(nullptr, nullptr) == -1);
    CHECK(errno == EINVAL);

    errno = 0;
    CHECK(init_rwlock(nullptr) == -1);
    CHECK(errno == EINVAL);
    errno = 0;
    CHECK(destroy_rwlock(nullptr) == -1);
    CHECK(errno == EINVAL);
    errno = 0;
    CHECK(read_lock(nullptr) == -1);
    CHECK(errno == EINVAL);
    errno = 0;
    CHECK(write_lock(nullptr) == -1);
    CHECK(errno == EINVAL);
    errno = 0;
    CHECK(try_read_lock(nullptr) == -1);
    CHECK(errno == EINVAL);
    errno = 0;
    CHECK(try_write_lock(nullptr) == -1);
    CHECK(errno == EINVAL);
    errno = 0;
    CHECK(unlock_rwlock(nullptr) == -1);
    CHECK(errno == EINVAL);
}
