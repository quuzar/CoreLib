#include <catch2/catch_test_macros.hpp>
#include <psx/thread.hpp>

#include <atomic>
#include <cerrno>
#include <future>
#include <memory>
#include <pthread.h>
#include <type_traits>
#include <utility>

using namespace core::psx;

namespace {

void* increment(void* argument) noexcept {
    auto* counter = static_cast<std::atomic<int>*>(argument);
    counter->fetch_add(1, std::memory_order_relaxed);
    return argument;
}

void* complete_promise(void* argument) noexcept {
    auto promise = std::unique_ptr<std::promise<void>>(
        static_cast<std::promise<void>*>(argument)
    );
    promise->set_value();
    return nullptr;
}

struct ThreadGuard {
    Thread& thread;
    ~ThreadGuard() {
        if (thread.joinable())
            thread.join();
    }
};

struct ThreadAttributes {
    pthread_attr_t value{};
    bool initialized{false};

    ~ThreadAttributes() {
        if (initialized)
            destroy_thread_attributes(&value);
    }
};

} // namespace

TEST_CASE("Thread: raw creation and joining return the worker result", "[psx][thread][raw]") {
    std::atomic<int> counter{0};
    pthread_t handle{};

    REQUIRE(create_thread(&handle, increment, &counter) == 0);
    void* result = nullptr;
    REQUIRE(join_thread(handle, &result) == 0);
    CHECK(result == &counter);
    CHECK(counter.load() == 1);
}

TEST_CASE("Thread: raw creation with attributes", "[psx][thread][raw]") {
    ThreadAttributes attributes;
    REQUIRE(init_thread_attributes(&attributes.value) == 0);
    attributes.initialized = true;
    REQUIRE(set_thread_stack_size(&attributes.value, 1024 * 1024) == 0);

    std::atomic<int> counter{0};
    pthread_t handle{};
    REQUIRE(create_thread(&handle, increment, &counter, &attributes.value) == 0);
    REQUIRE(join_thread(handle) == 0);
    CHECK(counter.load() == 1);
}

TEST_CASE("Thread: invalid raw arguments", "[psx][thread][errors]") {
    pthread_t handle{};

    errno = 0;
    CHECK(create_thread(nullptr, increment) == -1);
    CHECK(errno == EINVAL);

    errno = 0;
    CHECK(create_thread(&handle, nullptr) == -1);
    CHECK(errno == EINVAL);

    errno = 0;
    CHECK(init_thread_attributes(nullptr) == -1);
    CHECK(errno == EINVAL);

    errno = 0;
    CHECK(destroy_thread_attributes(nullptr) == -1);
    CHECK(errno == EINVAL);

    errno = 0;
    CHECK(set_thread_stack_size(nullptr, 1024 * 1024) == -1);
    CHECK(errno == EINVAL);
}

TEST_CASE("Thread: current thread and yielding", "[psx][thread][raw]") {
    CHECK(equal_threads(current_thread(), ::pthread_self()));
    CHECK(yield_thread() == 0);
}

TEST_CASE("Thread: default state and invalid lifecycle operations", "[psx][thread][errors]") {
    Thread thread;
    CHECK_FALSE(thread.joinable());
    CHECK(thread.state() == ThreadState::Empty);

    errno = 0;
    CHECK(thread.join() == -1);
    CHECK(errno == EINVAL);

    errno = 0;
    CHECK(thread.detach() == -1);
    CHECK(errno == EINVAL);

    errno = 0;
    CHECK(thread.start(nullptr) == -1);
    CHECK(errno == EINVAL);
}

TEST_CASE("Thread: class start, join and reuse", "[psx][thread][lifecycle]") {
    std::atomic<int> counter{0};
    Thread thread;
    ThreadGuard guard{thread};

    REQUIRE(thread.start(increment, &counter) == 0);
    CHECK(thread.joinable());
    CHECK(thread.state() == ThreadState::Joinable);
    CHECK(equal_threads(thread.native_handle(), thread.native_handle()));

    errno = 0;
    CHECK(thread.start(increment, &counter) == -1);
    CHECK(errno == EBUSY);

    void* result = nullptr;
    REQUIRE(thread.join(&result) == 0);
    CHECK(result == &counter);
    CHECK(counter.load() == 1);
    CHECK_FALSE(thread.joinable());
    CHECK(thread.state() == ThreadState::Empty);

    REQUIRE(thread.start(increment, &counter) == 0);
    REQUIRE(thread.join() == 0);
    CHECK(counter.load() == 2);
}

TEST_CASE("Thread: move construction transfers join responsibility", "[psx][thread][move]") {
    std::atomic<int> counter{0};
    Thread source;
    REQUIRE(source.start(increment, &counter) == 0);

    Thread destination(std::move(source));
    ThreadGuard guard{destination};
    CHECK_FALSE(source.joinable());
    CHECK(source.state() == ThreadState::Empty);
    CHECK(destination.joinable());
    REQUIRE(destination.join() == 0);
    CHECK(counter.load() == 1);
}

TEST_CASE("Thread: move assignment transfers join responsibility", "[psx][thread][move]") {
    std::atomic<int> counter{0};
    Thread source;
    REQUIRE(source.start(increment, &counter) == 0);
    Thread destination;

    destination = std::move(source);
    ThreadGuard guard{destination};
    CHECK_FALSE(source.joinable());
    CHECK(destination.joinable());
    REQUIRE(destination.join() == 0);
    CHECK(counter.load() == 1);

    destination = std::move(destination);
    CHECK_FALSE(destination.joinable());
}

TEST_CASE("Thread: detached worker completes independently", "[psx][thread][lifecycle]") {
    auto promise = std::make_unique<std::promise<void>>();
    auto completed = promise->get_future();
    Thread thread;

    REQUIRE(thread.start(complete_promise, promise.get()) == 0);
    promise.release(); // worker now owns the promise
    ThreadGuard guard{thread};

    REQUIRE(thread.detach() == 0);
    CHECK_FALSE(thread.joinable());
    CHECK(thread.state() == ThreadState::Detached);
    completed.wait();
}

TEST_CASE("Thread: move-only type", "[psx][thread][types]") {
    STATIC_REQUIRE_FALSE(std::is_copy_constructible_v<Thread>);
    STATIC_REQUIRE_FALSE(std::is_copy_assignable_v<Thread>);
    STATIC_REQUIRE(std::is_move_constructible_v<Thread>);
    STATIC_REQUIRE(std::is_move_assignable_v<Thread>);
}
