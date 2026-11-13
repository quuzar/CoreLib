#include "win/thread.hpp"

#include <exception>
#include <limits>
#include <process.h>
#include <stdlib.h>

namespace core::win {

    int create_thread(
        HANDLE* thread,
        ThreadFunction function,
        void* argument,
        std::size_t stack_size,
        DWORD* thread_id
    ) noexcept {
        if (thread == nullptr || function == nullptr ||
            stack_size > (std::numeric_limits<unsigned>::max)()) {
            ::SetLastError(ERROR_INVALID_PARAMETER);
            return -1;
        }

        *thread = nullptr;

        unsigned id{};

        const uintptr_t handle = ::_beginthreadex(
            nullptr,
            static_cast<unsigned>(stack_size),
            function,
            argument,
            0,
            &id
        );

        if (handle == 0) {
            unsigned long error{};

            if (::_get_doserrno(&error) == 0 && error != 0) {
                ::SetLastError(static_cast<DWORD>(error));
            } else {
                ::SetLastError(ERROR_GEN_FAILURE);
            }

            return -1;
        }

        *thread = reinterpret_cast<HANDLE>(handle);

        if (thread_id != nullptr) {
            *thread_id = static_cast<DWORD>(id);
        }

        return 0;
    }

    int join_thread(HANDLE thread, DWORD* result) noexcept {
        if (thread == nullptr || thread == INVALID_HANDLE_VALUE) {
            ::SetLastError(ERROR_INVALID_HANDLE);
            return -1;
        }

        const DWORD thread_id = ::GetThreadId(thread);

        if (thread_id == 0) {
            return -1;
        }

        if (thread_id == ::GetCurrentThreadId()) {
            ::SetLastError(ERROR_POSSIBLE_DEADLOCK);
            return -1;
        }

        const DWORD status = ::WaitForSingleObject(
            thread,
            INFINITE
        );

        if (status != WAIT_OBJECT_0) {
            return -1;
        }

        DWORD exit_code{};

        if (result != nullptr) {
            if (!::GetExitCodeThread(thread, &exit_code)) {
                return -1;
            }
        }

        if (!::CloseHandle(thread)) {
            return -1;
        }

        if (result != nullptr) {
            *result = exit_code;
        }

        return 0;
    }

    int detach_thread(HANDLE thread) noexcept {
        if (thread == nullptr || thread == INVALID_HANDLE_VALUE) {
            ::SetLastError(ERROR_INVALID_HANDLE);
            return -1;
        }

        return ::CloseHandle(thread) ? 0 : -1;
    }

    DWORD current_thread() noexcept {
        return ::GetCurrentThreadId();
    }

    bool equal_threads(DWORD lhs, DWORD rhs) noexcept {
        return lhs == rhs;
    }

    [[noreturn]] void exit_thread(unsigned result) noexcept {
        ::_endthreadex(result);
        std::terminate();
    }

    int yield_thread() noexcept {
        ::SwitchToThread();
        return 0;
    }


    Thread::~Thread() {
        if (joinable()) {
            std::terminate();
        }
    }

    Thread::Thread(Thread&& other) noexcept {
        handle_ = other.handle_;
        id_ = other.id_;
        state_ = other.state_;

        other.handle_ = nullptr;
        other.id_ = 0;
        other.state_ = ThreadState::Empty;
    }

    Thread& Thread::operator=(Thread&& other) noexcept {
        if (this == &other) {
            return *this;
        }

        if (joinable()) {
            std::terminate();
        }

        handle_ = other.handle_;
        id_ = other.id_;
        state_ = other.state_;

        other.handle_ = nullptr;
        other.id_ = 0;
        other.state_ = ThreadState::Empty;

        return *this;
    }

    int Thread::start(
        ThreadFunction function,
        void* argument,
        std::size_t stack_size
    ) noexcept {
        if (joinable()) {
            ::SetLastError(ERROR_BUSY);
            return -1;
        }

        HANDLE handle = nullptr;
        DWORD thread_id{};

        if (create_thread(
            &handle,
            function,
            argument,
            stack_size,
            &thread_id
        ) == -1) {
            return -1;
        }

        handle_ = handle;
        id_ = thread_id;
        state_ = ThreadState::Joinable;

        return 0;
    }

    int Thread::join(DWORD* result) noexcept {
        if (!joinable()) {
            ::SetLastError(ERROR_INVALID_HANDLE);
            return -1;
        }

        if (join_thread(handle_, result) == -1) {
            return -1;
        }

        handle_ = nullptr;
        id_ = 0;
        state_ = ThreadState::Empty;

        return 0;
    }

    int Thread::detach() noexcept {
        if (!joinable()) {
            ::SetLastError(ERROR_INVALID_HANDLE);
            return -1;
        }

        if (detach_thread(handle_) == -1) {
            return -1;
        }

        handle_ = nullptr;
        id_ = 0;
        state_ = ThreadState::Detached;

        return 0;
    }

    bool Thread::joinable() const noexcept {
        return state_ == ThreadState::Joinable;
    }

    ThreadState Thread::state() const noexcept {
        return state_;
    }

    HANDLE Thread::native_handle() const noexcept {
        return joinable() ? handle_ : nullptr;
    }

    DWORD Thread::id() const noexcept {
        return id_;
    }

}
