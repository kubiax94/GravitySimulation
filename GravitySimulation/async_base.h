#pragma once
#include <atomic>
#include <future>
#include <optional>

enum class async_type
{
	THREAD,
	TASK,
	FUTURE
};

enum class async_status
{
	IDLE,
	PROCESSING,
	COMPLETED,
	FAILED,
	CANCELLED,
	PAUSED
};

enum class async_priority
{
	LOW,
	MEDIUM,
	HIGH
};

template<typename ResultType = bool>
class async_base
{
protected:
	std::atomic<async_status> status_{ async_status::IDLE };
	std::future<ResultType> future_;
	async_priority priority_;

	// For Control
	std::atomic<bool> should_cancel_{ false };
	std::atomic<bool> should_pause_{ false };
	std::mutex pause_mutex_;
	std::condition_variable pause_cv_;

	std::atomic<float> progress_{ 0.0f };
	std::function<void(const ResultType&)> on_success_;
	std::function<void(const std::string&)> on_failure_;
	std::function<void(float)> on_progress_;
	std::function<bool()> should_continue_;

	std::chrono::steady_clock::time_point start_time_;
	std::chrono::microseconds max_execution_time_{ std::chrono::hours(1) };

	bool should_continue_execution() {
		if (should_cancel_) return false;

		if (should_pause_)
		{
			std::unique_lock<std::mutex> lock(pause_mutex_);
			pause_cv_.wait(lock, [this]() { return !should_pause_.load() || should_cancel_.load(); });
		}

		return !should_cancel_;
	}

public:

	virtual ~async_base()
	{
		cancel();
		if (future_.valid())
			future_.wait();
	}

	virtual ResultType execute_task() = 0;

	std::future<ResultType> execute_async(async_priority priority = async_priority::MEDIUM) {
		if (is_processing())
			return std::move(future_);

		priority_ = priority;
		status_ = async_status::PROCESSING;
		progress_ = 0.0f;
		should_cancel_ = false;
		should_pause_ = false;
		start_time_ = std::chrono::steady_clock::now();

		future_ = std::async(std::launch::async, [this]() -> ResultType {
			try {
				ResultType result = execution_with_control();

				if (!should_cancel_) {
					status_ = async_status::COMPLETED;
					progress_ = 1.0f;
					if (on_success_)
						on_success_(result);
				}
				return result;
			}
			catch (const std::exception& e)
			{
				if (!should_cancel_)
				{
					status_ = async_status::FAILED;

					if (on_failure_)
						on_failure_(e.what());
				}
				throw;
			}
		});
		return std::move(future_);

	}

	void cancel() {

		should_cancel_ = true;
		should_pause_ = false;
		status_ = async_status::CANCELLED;
		pause_cv_.notify_all();
	}

	void pause() {
		if (is_processing())
		{
			should_pause_ = true;
			status_ = async_status::PAUSED;
		}
	}

	void resume() {
		if (is_paused())
		{
			should_pause_ = false;
			status_ = async_status::PROCESSING;
			pause_cv_.notify_all();
		}
	}

	void set_timeout(const std::chrono::milliseconds& timeout) { max_execution_time_ = timeout; }
	void set_cancellation_condition(const std::function<bool()>& condition) { should_continue_ = condition; }
	void set_priority(const async_priority& priority) { priority_ = priority; }

	bool is_completed() const { return status_ == async_status::COMPLETED; }
	bool is_canceled() const { return status_ == async_status::CANCELLED; }
	bool is_failed() const { return status_ == async_status::FAILED; }
	bool is_processing() const { return status_ == async_status::PROCESSING; }
	bool is_paused() const { return status_ == async_status::PAUSED; }

	async_status get_status() const { return status_; }
	float get_progress() const { return progress_.load(); }
	async_priority get_priority() const { return priority_; }

	std::chrono::milliseconds get_execution_time() const {
		if (start_time_ == std::chrono::steady_clock::time_point{})
		{
			return std::chrono::milliseconds::zero();
		}
		return std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::steady_clock::now() - start_time_);
	}

	bool is_timeout() const {
		return get_execution_time() >= max_execution_time_;
	}

	bool wait_for_completion(const std::chrono::milliseconds& timeout) {

		if (!is_processing() && !is_paused())
			return is_completed();

		if (timeout == std::chrono::milliseconds::max())
		{
			future_.wait();
			return is_completed();
		}
		else
		{
			auto status = future_.wait_for(timeout);
			return status == std::future_status::ready;
		}
	}

	std::optional<ResultType> try_get_result(std::chrono::milliseconds timeout = std::chrono::milliseconds::zero()) {
		if (!future_.valid())
			return std::nullopt;

		if (wait_for_completion(timeout)) {
			try {
				return future_.get();
			}
			catch (...) {
				return std::nullopt;
			}
		}
	}

private:
	ResultType execution_with_control() {
		if (!should_continue_)
		{
			throw std::runtime_error("Operation was cancelled before execution");
		}

		return execute_task();
	}
};