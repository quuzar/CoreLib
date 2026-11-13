#pragma once

#include <cstddef>
#include <windows.h>

namespace core::win {

    using ThreadFunction = unsigned (__stdcall*)(void*);

    enum class ThreadState {
        Empty,
        Joinable,
        Detached
    };


    int create_thread(
        HANDLE* thread,
        ThreadFunction function,
        void* argument = nullptr,
        std::size_t stack_size = 0,
        DWORD* thread_id = nullptr
    ) noexcept;

    int join_thread(
        HANDLE thread,
        DWORD* result = nullptr
    ) noexcept;

    int detach_thread(HANDLE thread) noexcept;

    DWORD current_thread() noexcept;
    bool equal_threads(DWORD lhs, DWORD rhs) noexcept;

    [[noreturn]] void exit_thread(unsigned result = 0) noexcept;

    int yield_thread() noexcept;


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
            void* argument = nullptr,
            std::size_t stack_size = 0
        ) noexcept;

        int join(DWORD* result = nullptr) noexcept;
        int detach() noexcept;

        [[nodiscard]] bool joinable() const noexcept;
        [[nodiscard]] ThreadState state() const noexcept;

        [[nodiscard]] HANDLE native_handle() const noexcept;
        [[nodiscard]] DWORD id() const noexcept;

    private:
        HANDLE handle_{nullptr};
        DWORD id_{};
        ThreadState state_{ThreadState::Empty};
    };

}
