#include "psx/thread.hpp"

#include <cerrno>
#include <sched.h>
#include <exception>

namespace core::psx {

    int create_thread(
        pthread_t*     thread,
        ThreadFunction function,
        void*          argument
    ) noexcept {
        return create_thread(thread, function, argument, nullptr);
    }

    int create_thread(
        pthread_t*           thread,
        ThreadFunction       function,
        void*                argument,
        const pthread_attr_t* attributes
    ) noexcept {
        if (thread == nullptr || function == nullptr) {
            errno = EINVAL;
            return -1;
        }

        const int result = ::pthread_create(
            thread,
            attributes,
            function,
            argument
        );

        if (result != 0) {
            errno = result;
            return -1;
        }

        return 0;
    }

    int join_thread(pthread_t thread, void** result) noexcept {
        const int status = ::pthread_join(thread, result);

        if (status != 0) {
            errno = status;
            return -1;
        }

        return 0;
    }

    int detach_thread(pthread_t thread) noexcept {
        const int result = ::pthread_detach(thread);

        if (result != 0) {
            errno = result;
            return -1;
        }

        return 0;
    }

    pthread_t current_thread() noexcept {
        return ::pthread_self();
    }

    bool equal_threads(pthread_t lhs, pthread_t rhs) noexcept {
        return ::pthread_equal(lhs, rhs) != 0;
    }

    [[noreturn]] void exit_thread(void* result) {
        ::pthread_exit(result);
    }

    int yield_thread() noexcept {
        return ::sched_yield();
    }

    int init_thread_attributes(pthread_attr_t* attributes) noexcept {
        if (attributes == nullptr) {
            errno = EINVAL;
            return -1;
        }

        const int result = ::pthread_attr_init(attributes);

        if (result != 0) {
            errno = result;
            return -1;
        }

        return 0;
    }

    int destroy_thread_attributes(pthread_attr_t* attributes) noexcept {
        if (attributes == nullptr) {
            errno = EINVAL;
            return -1;
        }

        const int result = ::pthread_attr_destroy(attributes);

        if (result != 0) {
            errno = result;
            return -1;
        }

        return 0;
    }

    int set_thread_stack_size(
        pthread_attr_t* attributes,
        std::size_t     size
    ) noexcept {
        if (attributes == nullptr) {
            errno = EINVAL;
            return -1;
        }

        const int result = ::pthread_attr_setstacksize(attributes, size);

        if (result != 0) {
            errno = result;
            return -1;
        }

        return 0;
    }
}

namespace  core::psx {

    Thread::~Thread() {
        if (joinable()) {
            std::terminate();
        }
    }

    Thread::Thread(Thread&& other) noexcept {
        thread_ = other.thread_;
        state_ = other.state_;

        other.thread_ = {};
        other.state_ = ThreadState::Empty;
    }

    Thread& Thread::operator=(Thread&& other) noexcept {
        if (this == &other) {
            return *this;
        }

        if (joinable()) {
            std::terminate();
        }

        thread_ = other.thread_;
        state_ = other.state_;

        other.thread_ = {};
        other.state_ = ThreadState::Empty;

        return *this;
    }


    int Thread::start(
        ThreadFunction function,
        void* argument
    ) noexcept {
        return start(function, argument, nullptr);
    }

    int Thread::start(
        ThreadFunction function,
        void* argument,
        const pthread_attr_t* attributes
    ) noexcept {
        if (joinable()) {
            errno = EBUSY;
            return -1;
        }

        pthread_t thread{};

        if (create_thread(&thread, function, argument, attributes) == -1) {
            return -1;
        }

        thread_ = thread;
        state_ = ThreadState::Joinable;

        return 0;
    }


    int Thread::join(void** result) noexcept {
        if (!joinable()) {
            errno = EINVAL;
            return -1;
        }

        if (join_thread(thread_, result) == -1) {
            return -1;
        }

        thread_ = {};
        state_ = ThreadState::Empty;

        return 0;
    }

    int Thread::detach() noexcept {
        if (!joinable()) {
            errno = EINVAL;
            return -1;
        }

        if (detach_thread(thread_) == -1) {
            return -1;
        }

        thread_ = {};
        state_ = ThreadState::Detached;

        return 0;
    }


    bool Thread::joinable() const noexcept {
        return state_ == ThreadState::Joinable;
    }

    ThreadState Thread::state() const noexcept {
        return state_;
    }

    pthread_t Thread::native_handle() const noexcept {
        if (!joinable()) {
            return {};
        }

        return thread_;
    }
}
