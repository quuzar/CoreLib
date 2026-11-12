#pragma once

#include <cstddef>
#include <pthread.h>

namespace core::psx {

    using ThreadFunction = void* (*)(void*);

    enum class ThreadState {
        Empty,
        Joinable,
        Detached
    };

    int create_thread(
        pthread_t*      thread,
        ThreadFunction  function,
        void*           argument = nullptr
    ) noexcept;

    int join_thread(
        pthread_t   thread,
        void**      result = nullptr
    ) noexcept;

    int detach_thread(pthread_t thread) noexcept;

    pthread_t current_thread() noexcept;
    bool equal_threads(
        pthread_t lhs,
        pthread_t rhs
    ) noexcept;

    [[noreturn]] void exit_thread(void* result = nullptr);

    int yield_thread() noexcept;

    int init_thread_attributes(pthread_attr_t* attributes) noexcept;
    int destroy_thread_attributes(pthread_attr_t* attributes) noexcept;

    int set_thread_stack_size(
        pthread_attr_t* attributes,
        std::size_t     size
    ) noexcept;

    int create_thread(
        pthread_t*            thread,
        ThreadFunction        function,
        void*                 argument,
        const pthread_attr_t* attributes
    ) noexcept;


    class Thread {
    public:
        Thread() noexcept = default;
        ~Thread();

        Thread(const Thread&) = delete;
        Thread& operator=(const Thread&) = delete;

        Thread(Thread&& other) noexcept;
        Thread& operator=(Thread&& other) noexcept;

        int start(
            ThreadFunction function,
            void*          argument = nullptr
        ) noexcept;

        int start(
            ThreadFunction        function,
            void*                 argument,
            const pthread_attr_t* attributes
        ) noexcept;

        int join(void** result = nullptr) noexcept;
        int detach() noexcept;

        [[nodiscard]] bool joinable() const noexcept;
        [[nodiscard]] ThreadState state() const noexcept;
        [[nodiscard]] pthread_t native_handle() const noexcept;

    private:
        pthread_t thread_{};
        ThreadState state_{ThreadState::Empty};
    };

}
