#include "win/sync.hpp"
#include <exception>

namespace core::win {

    int init_mutex(CRITICAL_SECTION* mutex) noexcept {
        if (mutex == nullptr) {
            ::SetLastError(ERROR_INVALID_PARAMETER);
            return -1;
        }

        return ::InitializeCriticalSectionEx(mutex, 0, 0) ? 0 : -1;
    }

    int destroy_mutex(CRITICAL_SECTION* mutex) noexcept {
        if (mutex == nullptr) {
            ::SetLastError(ERROR_INVALID_PARAMETER);
            return -1;
        }

        ::DeleteCriticalSection(mutex);
        return 0;
    }

    int lock_mutex(CRITICAL_SECTION* mutex) noexcept {
        if (mutex == nullptr) {
            ::SetLastError(ERROR_INVALID_PARAMETER);
            return -1;
        }

        ::EnterCriticalSection(mutex);
        return 0;
    }

    int try_lock_mutex(CRITICAL_SECTION* mutex) noexcept {
        if (mutex == nullptr) {
            ::SetLastError(ERROR_INVALID_PARAMETER);
            return -1;
        }

        if (!::TryEnterCriticalSection(mutex)) {
            ::SetLastError(ERROR_BUSY);
            return -1;
        }

        return 0;
    }

    int unlock_mutex(CRITICAL_SECTION* mutex) noexcept {
        if (mutex == nullptr) {
            ::SetLastError(ERROR_INVALID_PARAMETER);
            return -1;
        }

        ::LeaveCriticalSection(mutex);
        return 0;
    }


    Mutex::~Mutex() {
        if (initialized_) {
            destroy_mutex(&mutex_);
        }
    }

    int Mutex::init() noexcept {
        if (initialized_) {
            ::SetLastError(ERROR_BUSY);
            return -1;
        }

        if (init_mutex(&mutex_) == -1) {
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
            ::SetLastError(ERROR_INVALID_PARAMETER);
            return -1;
        }

        return lock_mutex(&mutex_);
    }

    int Mutex::try_lock() noexcept {
        if (!initialized_) {
            ::SetLastError(ERROR_INVALID_PARAMETER);
            return -1;
        }

        return try_lock_mutex(&mutex_);
    }

    int Mutex::unlock() noexcept {
        if (!initialized_) {
            ::SetLastError(ERROR_INVALID_PARAMETER);
            return -1;
        }

        return unlock_mutex(&mutex_);
    }

    bool Mutex::valid() const noexcept {
        return initialized_;
    }

    CRITICAL_SECTION* Mutex::native_handle() noexcept {
        return initialized_ ? &mutex_ : nullptr;
    }

    int init_condition(CONDITION_VARIABLE* condition) noexcept {
        if (condition == nullptr) {
            ::SetLastError(ERROR_INVALID_PARAMETER);
            return -1;
        }

        ::InitializeConditionVariable(condition);
        return 0;
    }

    int destroy_condition(CONDITION_VARIABLE* condition) noexcept {
        if (condition == nullptr) {
            ::SetLastError(ERROR_INVALID_PARAMETER);
            return -1;
        }

        return 0;
    }

    int wait_condition(
        CONDITION_VARIABLE* condition,
        CRITICAL_SECTION* mutex,
        DWORD timeout
    ) noexcept {
        if (condition == nullptr || mutex == nullptr) {
            ::SetLastError(ERROR_INVALID_PARAMETER);
            return -1;
        }

        return ::SleepConditionVariableCS(
            condition,
            mutex,
            timeout
        ) ? 0 : -1;
    }

    int signal_condition(CONDITION_VARIABLE* condition) noexcept {
        if (condition == nullptr) {
            ::SetLastError(ERROR_INVALID_PARAMETER);
            return -1;
        }

        ::WakeConditionVariable(condition);
        return 0;
    }

    int broadcast_condition(CONDITION_VARIABLE* condition) noexcept {
        if (condition == nullptr) {
            ::SetLastError(ERROR_INVALID_PARAMETER);
            return -1;
        }

        ::WakeAllConditionVariable(condition);
        return 0;
    }


    int ConditionVariable::init() noexcept {
        if (initialized_) {
            ::SetLastError(ERROR_BUSY);
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

        initialized_ = false;
        return 0;
    }

    int ConditionVariable::wait(Mutex& mutex, DWORD timeout) noexcept {
        if (!initialized_ || !mutex.valid()) {
            ::SetLastError(ERROR_INVALID_PARAMETER);
            return -1;
        }

        return wait_condition(
            &condition_,
            mutex.native_handle(),
            timeout
        );
    }

    int ConditionVariable::signal() noexcept {
        if (!initialized_) {
            ::SetLastError(ERROR_INVALID_PARAMETER);
            return -1;
        }

        return signal_condition(&condition_);
    }

    int ConditionVariable::broadcast() noexcept {
        if (!initialized_) {
            ::SetLastError(ERROR_INVALID_PARAMETER);
            return -1;
        }

        return broadcast_condition(&condition_);
    }

    bool ConditionVariable::valid() const noexcept {
        return initialized_;
    }

    CONDITION_VARIABLE* ConditionVariable::native_handle() noexcept {
        return initialized_ ? &condition_ : nullptr;
    }

    int init_semaphore(
        HANDLE* semaphore,
        LONG initial,
        LONG maximum
    ) noexcept {
        if (semaphore == nullptr || initial < 0 ||
            maximum <= 0 || initial > maximum) {
            ::SetLastError(ERROR_INVALID_PARAMETER);
            return -1;
        }

        HANDLE handle = ::CreateSemaphoreW(
            nullptr,
            initial,
            maximum,
            nullptr
        );

        if (handle == nullptr) {
            return -1;
        }

        *semaphore = handle;
        return 0;
    }

    int destroy_semaphore(HANDLE semaphore) noexcept {
        if (semaphore == nullptr || semaphore == INVALID_HANDLE_VALUE) {
            ::SetLastError(ERROR_INVALID_HANDLE);
            return -1;
        }

        return ::CloseHandle(semaphore) ? 0 : -1;
    }

    int wait_semaphore(HANDLE semaphore, DWORD timeout) noexcept {
        if (semaphore == nullptr || semaphore == INVALID_HANDLE_VALUE) {
            ::SetLastError(ERROR_INVALID_HANDLE);
            return -1;
        }

        const DWORD result = ::WaitForSingleObject(
            semaphore,
            timeout
        );

        if (result == WAIT_OBJECT_0) {
            return 0;
        }

        if (result == WAIT_TIMEOUT) {
            ::SetLastError(ERROR_TIMEOUT);
        }

        return -1;
    }

    int try_wait_semaphore(HANDLE semaphore) noexcept {
        return wait_semaphore(semaphore, 0);
    }

    int post_semaphore(HANDLE semaphore, LONG count) noexcept {
        if (semaphore == nullptr || semaphore == INVALID_HANDLE_VALUE) {
            ::SetLastError(ERROR_INVALID_HANDLE);
            return -1;
        }

        if (count <= 0) {
            ::SetLastError(ERROR_INVALID_PARAMETER);
            return -1;
        }

        return ::ReleaseSemaphore(
            semaphore,
            count,
            nullptr
        ) ? 0 : -1;
    }


    Semaphore::~Semaphore() {
        if (valid()) {
            destroy_semaphore(semaphore_);
        }
    }

    int Semaphore::init(LONG initial, LONG maximum) noexcept {
        if (valid()) {
            ::SetLastError(ERROR_BUSY);
            return -1;
        }

        return init_semaphore(
            &semaphore_,
            initial,
            maximum
        );
    }

    int Semaphore::destroy() noexcept {
        if (!valid()) {
            return 0;
        }

        if (destroy_semaphore(semaphore_) == -1) {
            return -1;
        }

        semaphore_ = nullptr;
        return 0;
    }

    int Semaphore::wait(DWORD timeout) noexcept {
        return wait_semaphore(semaphore_, timeout);
    }

    int Semaphore::try_wait() noexcept {
        return try_wait_semaphore(semaphore_);
    }

    int Semaphore::post(LONG count) noexcept {
        return post_semaphore(semaphore_, count);
    }

    bool Semaphore::valid() const noexcept {
        return semaphore_ != nullptr &&
               semaphore_ != INVALID_HANDLE_VALUE;
    }

    HANDLE Semaphore::native_handle() const noexcept {
        return semaphore_;
    }

    int init_rwlock(SRWLOCK* lock) noexcept {
        if (lock == nullptr) {
            ::SetLastError(ERROR_INVALID_PARAMETER);
            return -1;
        }

        ::InitializeSRWLock(lock);
        return 0;
    }

    int destroy_rwlock(SRWLOCK* lock) noexcept {
        if (lock == nullptr) {
            ::SetLastError(ERROR_INVALID_PARAMETER);
            return -1;
        }

        return 0;
    }

    int read_lock(SRWLOCK* lock) noexcept {
        if (lock == nullptr) {
            ::SetLastError(ERROR_INVALID_PARAMETER);
            return -1;
        }

        ::AcquireSRWLockShared(lock);
        return 0;
    }

    int write_lock(SRWLOCK* lock) noexcept {
        if (lock == nullptr) {
            ::SetLastError(ERROR_INVALID_PARAMETER);
            return -1;
        }

        ::AcquireSRWLockExclusive(lock);
        return 0;
    }

    int try_read_lock(SRWLOCK* lock) noexcept {
        if (lock == nullptr) {
            ::SetLastError(ERROR_INVALID_PARAMETER);
            return -1;
        }

        if (!::TryAcquireSRWLockShared(lock)) {
            ::SetLastError(ERROR_BUSY);
            return -1;
        }

        return 0;
    }

    int try_write_lock(SRWLOCK* lock) noexcept {
        if (lock == nullptr) {
            ::SetLastError(ERROR_INVALID_PARAMETER);
            return -1;
        }

        if (!::TryAcquireSRWLockExclusive(lock)) {
            ::SetLastError(ERROR_BUSY);
            return -1;
        }

        return 0;
    }

    int unlock_read(SRWLOCK* lock) noexcept {
        if (lock == nullptr) {
            ::SetLastError(ERROR_INVALID_PARAMETER);
            return -1;
        }

        ::ReleaseSRWLockShared(lock);
        return 0;
    }

    int unlock_write(SRWLOCK* lock) noexcept {
        if (lock == nullptr) {
            ::SetLastError(ERROR_INVALID_PARAMETER);
            return -1;
        }

        ::ReleaseSRWLockExclusive(lock);
        return 0;
    }


    int ReadWriteLock::init() noexcept {
        if (initialized_) {
            ::SetLastError(ERROR_BUSY);
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

        initialized_ = false;
        return 0;
    }

    int ReadWriteLock::lock_read() noexcept {
        if (!initialized_) {
            ::SetLastError(ERROR_INVALID_PARAMETER);
            return -1;
        }

        return read_lock(&lock_);
    }

    int ReadWriteLock::lock_write() noexcept {
        if (!initialized_) {
            ::SetLastError(ERROR_INVALID_PARAMETER);
            return -1;
        }

        return write_lock(&lock_);
    }

    int ReadWriteLock::try_lock_read() noexcept {
        if (!initialized_) {
            ::SetLastError(ERROR_INVALID_PARAMETER);
            return -1;
        }

        return try_read_lock(&lock_);
    }

    int ReadWriteLock::try_lock_write() noexcept {
        if (!initialized_) {
            ::SetLastError(ERROR_INVALID_PARAMETER);
            return -1;
        }

        return try_write_lock(&lock_);
    }

    int ReadWriteLock::unlock_read() noexcept {
        if (!initialized_) {
            ::SetLastError(ERROR_INVALID_PARAMETER);
            return -1;
        }

        return core::win::unlock_read(&lock_);
    }

    int ReadWriteLock::unlock_write() noexcept {
        if (!initialized_) {
            ::SetLastError(ERROR_INVALID_PARAMETER);
            return -1;
        }

        return core::win::unlock_write(&lock_);
    }

    bool ReadWriteLock::valid() const noexcept {
        return initialized_;
    }

    SRWLOCK* ReadWriteLock::native_handle() noexcept {
        return initialized_ ? &lock_ : nullptr;
    }

}
