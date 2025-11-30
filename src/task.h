#pragma once
#include <coroutine>
#include <exception>

template<typename T = void>
class task;

template<>
class task<void> {
public:
	struct promise_type {
		task get_return_object() {
			return task{
				std::coroutine_handle<promise_type>::from_promise(*this)
			};
		}

		// Start suspended so caller can call .start()
		std::suspend_always initial_suspend() const noexcept { return {}; }

		// Allow coroutine to suspend at the end until destructed
		std::suspend_always final_suspend() const noexcept { return {}; }

		void return_void() noexcept {}

		void unhandled_exception() {
			exception = std::current_exception();
		}

		std::exception_ptr exception;
	};

	using handle = std::coroutine_handle<promise_type>;

	task(handle h) : coro(h) {}
	task(task&& other) noexcept : coro(other.coro) {
		other.coro = nullptr;
	}

	~task() {
		if (coro)
			coro.destroy();
	}

	void start() {
		coro.resume();
	}

private:
	handle coro;
};
