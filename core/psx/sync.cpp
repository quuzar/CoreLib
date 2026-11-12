#include "psx/sync.hpp"
#include <cerrno>

namespace core::psx {
    int init_mutex(
        pthread_mutex_t* mutex,
        MutexType type
    ) noexcept {
        if (mutex == nullptr) {
            errno = EINVAL;
            return -1;
        }

        pthread_mutexattr_t attr{};

        int result = ::pthread_mutexattr_init(&attr);

        if (result != 0) {
            errno = result;
            return -1;
        }

        result = ::pthread_mutexattr_settype(
            &attr,
            static_cast<int>(type)
        );

        if (result == 0) {
            result = ::pthread_mutex_init(mutex, &attr);
        }

        ::pthread_mutexattr_destroy(&attr);

        if (result != 0) {
            errno = result;
            return -1;
        }

        return 0;
    }

    int destroy_mutex(pthread_mutex_t* mutex) noexcept {
        if (mutex == nullptr) {
            errno = EINVAL;
            return -1;
        }

        const int result = ::pthread_mutex_destroy(mutex);

        if (result != 0) {
            errno = result;
            return -1;
        }

        return 0;
    }

    int lock_mutex(pthread_mutex_t* mutex) noexcept {
        if (mutex == nullptr) {
            errno = EINVAL;
            return -1;
        }

        const int result = ::pthread_mutex_lock(mutex);

        if (result != 0) {
            errno = result;
            return -1;
        }

        return 0;
    }

    int try_lock_mutex(pthread_mutex_t* mutex) noexcept {
        if (mutex == nullptr) {
            errno = EINVAL;
            return -1;
        }

        const int result = ::pthread_mutex_trylock(mutex);

        if (result != 0) {
            errno = result;
            return -1;
        }

        return 0;
    }

    int unlock_mutex(pthread_mutex_t* mutex) noexcept {
        if (mutex == nullptr) {
            errno = EINVAL;
            return -1;
        }

        const int result = ::pthread_mutex_unlock(mutex);

        if (result != 0) {
            errno = result;
            return -1;
        }

        return 0;
    }
}

namespace core::psx {
    Mutex::~Mutex() {
        if (initialized_) {
            destroy_mutex(&mutex_);
        }
    }

    int Mutex::init(MutexType type) noexcept {
        if (initialized_) {
            errno = EBUSY;
            return -1;
        }

        if (init_mutex(&mutex_, type) == -1) {
            return -1;
        }

        initialized_ = true;
        return 0;
    }

    int Mutex::destroy() noexcept {
        if (!initialized_) {
            return 0;
        }

        if (destroy_mutex(&mutex_) == -1) {
            return -1;
        }

        initialized_ = false;
        return 0;
    }

    int Mutex::lock() noexcept {
        if (!initialized_) {
            errno = EINVAL;
            return -1;
        }

        return lock_mutex(&mutex_);
    }

    int Mutex::try_lock() noexcept {
        if (!initialized_) {
            errno = EINVAL;
            return -1;
        }

        return try_lock_mutex(&mutex_);
    }

    int Mutex::unlock() noexcept {
        if (!initialized_) {
            errno = EINVAL;
            return -1;
        }

        return unlock_mutex(&mutex_);
    }

    bool Mutex::valid() const noexcept {
        return initialized_;
    }

    pthread_mutex_t* Mutex::native_handle() noexcept {
        if (!initialized_) {
            return nullptr;
        }

        return &mutex_;
    }
}

namespace core::psx {
    int init_condition(pthread_cond_t* condition) noexcept {
        if (condition == nullptr) {
            errno = EINVAL;
            return -1;
        }

        const int result = ::pthread_cond_init(condition, nullptr);

        if (result != 0) {
            errno = result;
            return -1;
        }

        return 0;
    }

    int destroy_condition(pthread_cond_t* condition) noexcept {
        if (condition == nullptr) {
            errno = EINVAL;
            return -1;
        }

        const int result = ::pthread_cond_destroy(condition);

        if (result != 0) {
            errno = result;
            return -1;
        }

        return 0;
    }

    int wait_condition(
        pthread_cond_t* condition,
        pthread_mutex_t* mutex
    ) noexcept {
        if (condition == nullptr || mutex == nullptr) {
            errno = EINVAL;
            return -1;
        }

        const int result = ::pthread_cond_wait(condition, mutex);

        if (result != 0) {
            errno = result;
            return -1;
        }

        return 0;
    }

    int signal_condition(pthread_cond_t* condition) noexcept {
        if (condition == nullptr) {
            errno = EINVAL;
            return -1;
        }

        const int result = ::pthread_cond_signal(condition);

        if (result != 0) {
            errno = result;
            return -1;
        }

        return 0;
    }

    int broadcast_condition(pthread_cond_t* condition) noexcept {
        if (condition == nullptr) {
            errno = EINVAL;
            return -1;
        }

        const int result = ::pthread_cond_broadcast(condition);

        if (result != 0) {
            errno = result;
            return -1;
        }

        return 0;
    }
}

namespace core::psx {
    ConditionVariable::~ConditionVariable() {
        if (initialized_) {
            destroy_condition(&condition_);
        }
    }

    int ConditionVariable::init() noexcept {
        if (initialized_) {
            errno = EBUSY;
            return -1;
        }

        if (init_condition(&condition_) == -1) {
            return -1;
        }

        initialized_ = true;
        return 0;
    }

    int ConditionVariable::destroy() noexcept {
        if (!initialized_) {
            return 0;
        }

        if (destroy_condition(&condition_) == -1) {
            return -1;
        }

        initialized_ = false;
        return 0;
    }

    int ConditionVariable::wait(Mutex& mutex) noexcept {
        if (!initialized_ || !mutex.valid()) {
            errno = EINVAL;
            return -1;
        }

        return wait_condition(&condition_, mutex.native_handle());
    }

    int ConditionVariable::signal() noexcept {
        if (!initialized_) {
            errno = EINVAL;
            return -1;
        }

        return signal_condition(&condition_);
    }

    int ConditionVariable::broadcast() noexcept {
        if (!initialized_) {
            errno = EINVAL;
            return -1;
        }

        return broadcast_condition(&condition_);
    }

    bool ConditionVariable::valid() const noexcept {
        return initialized_;
    }

    pthread_cond_t* ConditionVariable::native_handle() noexcept {
        if (!initialized_) {
            return nullptr;
        }

        return &condition_;
    }
}

namespace core::psx {
    int init_semaphore(
        sem_t* semaphore,
        unsigned int value
    ) noexcept {
        if (semaphore == nullptr) {
            errno = EINVAL;
            return -1;
        }

        return ::sem_init(semaphore, 0, value);
    }

    int destroy_semaphore(sem_t* semaphore) noexcept {
        if (semaphore == nullptr) {
            errno = EINVAL;
            return -1;
        }

        return ::sem_destroy(semaphore);
    }

    int wait_semaphore(sem_t* semaphore) noexcept {
        if (semaphore == nullptr) {
            errno = EINVAL;
            return -1;
        }

        return ::sem_wait(semaphore);
    }

    int try_wait_semaphore(sem_t* semaphore) noexcept {
        if (semaphore == nullptr) {
            errno = EINVAL;
            return -1;
        }

        return ::sem_trywait(semaphore);
    }

    int post_semaphore(sem_t* semaphore) noexcept {
        if (semaphore == nullptr) {
            errno = EINVAL;
            return -1;
        }

        return ::sem_post(semaphore);
    }

    int value_semaphore(
        sem_t* semaphore,
        int* value
    ) noexcept {
        if (semaphore == nullptr || value == nullptr) {
            errno = EINVAL;
            return -1;
        }

        return ::sem_getvalue(semaphore, value);
    }
}

namespace core::psx {
    Semaphore::~Semaphore() {
        if (initialized_) {
            destroy_semaphore(&semaphore_);
        }
    }

    int Semaphore::init(unsigned int value) noexcept {
        if (initialized_) {
            errno = EBUSY;
            return -1;
        }

        if (init_semaphore(&semaphore_, value) == -1) {
            return -1;
        }

        initialized_ = true;
        return 0;
    }

    int Semaphore::destroy() noexcept {
        if (!initialized_) {
            return 0;
        }

        if (destroy_semaphore(&semaphore_) == -1) {
            return -1;
        }

        initialized_ = false;
        return 0;
    }

    int Semaphore::wait() noexcept {
        if (!initialized_) {
            errno = EINVAL;
            return -1;
        }

        return wait_semaphore(&semaphore_);
    }

    int Semaphore::try_wait() noexcept {
        if (!initialized_) {
            errno = EINVAL;
            return -1;
        }

        return try_wait_semaphore(&semaphore_);
    }

    int Semaphore::post() noexcept {
        if (!initialized_) {
            errno = EINVAL;
            return -1;
        }

        return post_semaphore(&semaphore_);
    }

    int Semaphore::value(int* result) noexcept {
        if (!initialized_ || result == nullptr) {
            errno = EINVAL;
            return -1;
        }

        return value_semaphore(&semaphore_, result);
    }

    bool Semaphore::valid() const noexcept {
        return initialized_;
    }

    sem_t* Semaphore::native_handle() noexcept {
        if (!initialized_) {
            return nullptr;
        }

        return &semaphore_;
    }
}

namespace core::psx {
    int init_rwlock(pthread_rwlock_t* lock) noexcept {
        if (lock == nullptr) {
            errno = EINVAL;
            return -1;
        }

        const int result = ::pthread_rwlock_init(lock, nullptr);

        if (result != 0) {
            errno = result;
            return -1;
        }

        return 0;
    }

    int destroy_rwlock(pthread_rwlock_t* lock) noexcept {
        if (lock == nullptr) {
            errno = EINVAL;
            return -1;
        }

        const int result = ::pthread_rwlock_destroy(lock);

        if (result != 0) {
            errno = result;
            return -1;
        }

        return 0;
    }

    int read_lock(pthread_rwlock_t* lock) noexcept {
        if (lock == nullptr) {
            errno = EINVAL;
            return -1;
        }

        const int result = ::pthread_rwlock_rdlock(lock);

        if (result != 0) {
            errno = result;
            return -1;
        }

        return 0;
    }

    int write_lock(pthread_rwlock_t* lock) noexcept {
        if (lock == nullptr) {
            errno = EINVAL;
            return -1;
        }

        const int result = ::pthread_rwlock_wrlock(lock);

        if (result != 0) {
            errno = result;
            return -1;
        }

        return 0;
    }

    int try_read_lock(pthread_rwlock_t* lock) noexcept {
        if (lock == nullptr) {
            errno = EINVAL;
            return -1;
        }

        const int result = ::pthread_rwlock_tryrdlock(lock);

        if (result != 0) {
            errno = result;
            return -1;
        }

        return 0;
    }

    int try_write_lock(pthread_rwlock_t* lock) noexcept {
        if (lock == nullptr) {
            errno = EINVAL;
            return -1;
        }

        const int result = ::pthread_rwlock_trywrlock(lock);

        if (result != 0) {
            errno = result;
            return -1;
        }

        return 0;
    }

    int unlock_rwlock(pthread_rwlock_t* lock) noexcept {
        if (lock == nullptr) {
            errno = EINVAL;
            return -1;
        }

        const int result = ::pthread_rwlock_unlock(lock);

        if (result != 0) {
            errno = result;
            return -1;
        }

        return 0;
    }
}

namespace core::psx {
    ReadWriteLock::~ReadWriteLock() {
        if (initialized_) {
            destroy_rwlock(&lock_);
        }
    }

    int ReadWriteLock::init() noexcept {
        if (initialized_) {
            errno = EBUSY;
            return -1;
        }

        if (init_rwlock(&lock_) == -1) {
            return -1;
        }

        initialized_ = true;
        return 0;
    }

    int ReadWriteLock::destroy() noexcept {
        if (!initialized_) {
            return 0;
        }

        if (destroy_rwlock(&lock_) == -1) {
            return -1;
        }

        initialized_ = false;
        return 0;
    }

    int ReadWriteLock::lock_read() noexcept {
        if (!initialized_) {
            errno = EINVAL;
            return -1;
        }

        return read_lock(&lock_);
    }

    int ReadWriteLock::lock_write() noexcept {
        if (!initialized_) {
            errno = EINVAL;
            return -1;
        }

        return write_lock(&lock_);
    }

    int ReadWriteLock::try_lock_read() noexcept {
        if (!initialized_) {
            errno = EINVAL;
            return -1;
        }

        return try_read_lock(&lock_);
    }

    int ReadWriteLock::try_lock_write() noexcept {
        if (!initialized_) {
            errno = EINVAL;
            return -1;
        }

        return try_write_lock(&lock_);
    }

    int ReadWriteLock::unlock() noexcept {
        if (!initialized_) {
            errno = EINVAL;
            return -1;
        }

        return unlock_rwlock(&lock_);
    }

    bool ReadWriteLock::valid() const noexcept {
        return initialized_;
    }

    pthread_rwlock_t* ReadWriteLock::native_handle() noexcept {
        if (!initialized_) {
            return nullptr;
        }

        return &lock_;
    }
}