
#pragma once

#include <pthread.h>
#include <semaphore.h>

namespace core::psx {

    enum class MutexType : int {
        Normal     = PTHREAD_MUTEX_NORMAL,
        Recursive  = PTHREAD_MUTEX_RECURSIVE,
        ErrorCheck = PTHREAD_MUTEX_ERRORCHECK
    };


    int init_mutex(
        pthread_mutex_t* mutex,
        MutexType type = MutexType::Normal
    ) noexcept;
    int destroy_mutex(pthread_mutex_t* mutex) noexcept;

    int lock_mutex(pthread_mutex_t* mutex) noexcept;
    int try_lock_mutex(pthread_mutex_t* mutex) noexcept;
    int unlock_mutex(pthread_mutex_t* mutex) noexcept;


    int init_condition(pthread_cond_t* condition) noexcept;
    int destroy_condition(pthread_cond_t* condition) noexcept;

    int wait_condition(pthread_cond_t* condition, pthread_mutex_t* mutex) noexcept;
    int signal_condition(pthread_cond_t* condition) noexcept;
    int broadcast_condition(pthread_cond_t* condition) noexcept;


    int init_semaphore(sem_t* semaphore, unsigned int value = 0) noexcept;
    int destroy_semaphore(sem_t* semaphore) noexcept;

    int wait_semaphore(sem_t* semaphore) noexcept;
    int try_wait_semaphore(sem_t* semaphore) noexcept;
    int post_semaphore(sem_t* semaphore) noexcept;
    int value_semaphore(sem_t* semaphore, int* value) noexcept;


    int init_rwlock(pthread_rwlock_t* lock) noexcept;
    int destroy_rwlock(pthread_rwlock_t* lock) noexcept;

    int read_lock(pthread_rwlock_t* lock) noexcept;
    int write_lock(pthread_rwlock_t* lock) noexcept;
    int try_read_lock(pthread_rwlock_t* lock) noexcept;
    int try_write_lock(pthread_rwlock_t* lock) noexcept;
    int unlock_rwlock(pthread_rwlock_t* lock) noexcept;

    class Mutex {
    public:
        Mutex() noexcept = default;
        ~Mutex();

        Mutex(const Mutex&) = delete;
        Mutex& operator=(const Mutex&) = delete;

        Mutex(Mutex&&) = delete;
        Mutex& operator=(Mutex&&) = delete;

        int init(MutexType type = MutexType::Normal) noexcept;
        int destroy() noexcept;

        int lock() noexcept;
        int try_lock() noexcept;
        int unlock() noexcept;

        [[nodiscard]] bool valid() const noexcept;
        [[nodiscard]] pthread_mutex_t* native_handle() noexcept;

    private:
        pthread_mutex_t mutex_{};
        bool initialized_{false};
    };


    class ConditionVariable {
    public:
        ConditionVariable() noexcept = default;
        ~ConditionVariable();

        ConditionVariable(const ConditionVariable&) = delete;
        ConditionVariable& operator=(const ConditionVariable&) = delete;

        ConditionVariable(ConditionVariable&&) = delete;
        ConditionVariable& operator=(ConditionVariable&&) = delete;

        int init() noexcept;
        int destroy() noexcept;

        int wait(Mutex& mutex) noexcept;
        int signal() noexcept;
        int broadcast() noexcept;

        [[nodiscard]] bool valid() const noexcept;
        [[nodiscard]] pthread_cond_t* native_handle() noexcept;

    private:
        pthread_cond_t condition_{};
        bool initialized_{false};
    };


    class Semaphore {
    public:
        Semaphore() noexcept = default;
        ~Semaphore();

        Semaphore(const Semaphore&) = delete;
        Semaphore& operator=(const Semaphore&) = delete;

        Semaphore(Semaphore&&) = delete;
        Semaphore& operator=(Semaphore&&) = delete;

        int init(unsigned int value = 0) noexcept;
        int destroy() noexcept;

        int wait() noexcept;
        int try_wait() noexcept;
        int post() noexcept;
        int value(int* result) noexcept;

        [[nodiscard]] bool valid() const noexcept;
        [[nodiscard]] sem_t* native_handle() noexcept;

    private:
        sem_t semaphore_{};
        bool initialized_{false};
    };


    class ReadWriteLock {
    public:
        ReadWriteLock() noexcept = default;
        ~ReadWriteLock();

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
        int unlock() noexcept;

        [[nodiscard]] bool valid() const noexcept;
        [[nodiscard]] pthread_rwlock_t* native_handle() noexcept;

    private:
        pthread_rwlock_t lock_{};
        bool initialized_{false};
    };

}
