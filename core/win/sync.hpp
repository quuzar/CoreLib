
#pragma once

#include <climits>
#include <windows.h>

namespace core::win {

    int init_mutex(CRITICAL_SECTION* mutex) noexcept;
    int destroy_mutex(CRITICAL_SECTION* mutex) noexcept;

    int lock_mutex(CRITICAL_SECTION* mutex) noexcept;
    int try_lock_mutex(CRITICAL_SECTION* mutex) noexcept;
    int unlock_mutex(CRITICAL_SECTION* mutex) noexcept;


    class Mutex {
    public:
        Mutex() noexcept = default;
        ~Mutex();

        Mutex(const Mutex&) = delete;
        Mutex& operator=(const Mutex&) = delete;
        Mutex(Mutex&&) = delete;
        Mutex& operator=(Mutex&&) = delete;

        int init() noexcept;
        int destroy() noexcept;

        int lock() noexcept;
        int try_lock() noexcept;
        int unlock() noexcept;

        [[nodiscard]] bool valid() const noexcept;
        [[nodiscard]] CRITICAL_SECTION* native_handle() noexcept;

    private:
        CRITICAL_SECTION mutex_{};
        bool initialized_{false};
    };


    int init_condition(CONDITION_VARIABLE* condition) noexcept;
    int destroy_condition(CONDITION_VARIABLE* condition) noexcept;

    int wait_condition(
        CONDITION_VARIABLE* condition,
        CRITICAL_SECTION* mutex,
        DWORD timeout = INFINITE
    ) noexcept;

    int signal_condition(CONDITION_VARIABLE* condition) noexcept;
    int broadcast_condition(CONDITION_VARIABLE* condition) noexcept;


    class ConditionVariable {
    public:
        ConditionVariable() noexcept = default;
        ~ConditionVariable() = default;

        ConditionVariable(const ConditionVariable&) = delete;
        ConditionVariable& operator=(const ConditionVariable&) = delete;
        ConditionVariable(ConditionVariable&&) = delete;
        ConditionVariable& operator=(ConditionVariable&&) = delete;

        int init() noexcept;
        int destroy() noexcept;

        int wait(Mutex& mutex, DWORD timeout = INFINITE) noexcept;
        int signal() noexcept;
        int broadcast() noexcept;

        [[nodiscard]] bool valid() const noexcept;
        [[nodiscard]] CONDITION_VARIABLE* native_handle() noexcept;

    private:
        CONDITION_VARIABLE condition_{};
        bool initialized_{false};
    };


    int init_semaphore(
        HANDLE* semaphore,
        LONG initial = 0,
        LONG maximum = LONG_MAX
    ) noexcept;

    int destroy_semaphore(HANDLE semaphore) noexcept;

    int wait_semaphore(
        HANDLE semaphore,
        DWORD timeout = INFINITE
    ) noexcept;

    int try_wait_semaphore(HANDLE semaphore) noexcept;
    int post_semaphore(HANDLE semaphore, LONG count = 1) noexcept;


    class Semaphore {
    public:
        Semaphore() noexcept = default;
        ~Semaphore();

        Semaphore(const Semaphore&) = delete;
        Semaphore& operator=(const Semaphore&) = delete;
        Semaphore(Semaphore&&) = delete;
        Semaphore& operator=(Semaphore&&) = delete;

        int init(LONG initial = 0, LONG maximum = LONG_MAX) noexcept;
        int destroy() noexcept;

        int wait(DWORD timeout = INFINITE) noexcept;
        int try_wait() noexcept;
        int post(LONG count = 1) noexcept;

        [[nodiscard]] bool valid() const noexcept;
        [[nodiscard]] HANDLE native_handle() const noexcept;

    private:
        HANDLE semaphore_{nullptr};
    };


    int init_rwlock(SRWLOCK* lock) noexcept;
    int destroy_rwlock(SRWLOCK* lock) noexcept;

    int read_lock(SRWLOCK* lock) noexcept;
    int write_lock(SRWLOCK* lock) noexcept;

    int try_read_lock(SRWLOCK* lock) noexcept;
    int try_write_lock(SRWLOCK* lock) noexcept;

    int unlock_read(SRWLOCK* lock) noexcept;
    int unlock_write(SRWLOCK* lock) noexcept;


    class ReadWriteLock {
    public:
        ReadWriteLock() noexcept = default;
        ~ReadWriteLock() = default;

        ReadWriteLock(const ReadWriteLock&) = delete;
        ReadWriteLock& operator=(const ReadWriteLock&) = delete;
        ReadWriteLock(ReadWriteLock&&) = delete;
        ReadWriteLock& operator=(ReadWriteLock&&) = delete;

        int init() noexcept;
        int destroy() noexcept;

        int lock_read() noexcept;
        int lock_write() noexcept;

        int try_lock_read() noexcept;
        int try_lock_write() noexcept;

        int unlock_read() noexcept;
        int unlock_write() noexcept;

        [[nodiscard]] bool valid() const noexcept;
        [[nodiscard]] SRWLOCK* native_handle() noexcept;

    private:
        SRWLOCK lock_{};
        bool initialized_{false};
    };

}
