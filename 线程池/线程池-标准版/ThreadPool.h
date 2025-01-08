#pragma once
#include <iostream>
#include <atomic>
#include <vector>
#include <queue>
#include <thread>
#include <future>

class NoneCopy
{
public:
	~NoneCopy(){}
protected:
	NoneCopy(){}
private:
	NoneCopy(const NoneCopy&) = delete;
	NoneCopy& operator=(const NoneCopy&) = delete;
};

class ThreadPool : public NoneCopy
{
	using Task = std::packaged_task<void()>;
public:
	~ThreadPool();
	static ThreadPool& instance();
	int idleThreadCount();
	template<class F, class... Args>
	auto commit(F&& f, Args&&... args) ->
		std::future<decltype(std::forward<F>(f)(std::forward<Args>(args)...))>;
private:
	ThreadPool(unsigned int num = std::thread::hardware_concurrency());
	void start();
	void stop();

	std::atomic_int thread_num_;
	std::vector<std::thread> pool_;
	std::queue<Task> tasks_;
	std::atomic_bool stop_;

	std::mutex cv_mt_;
	std::condition_variable cv_lock_;
};

template<class F, class ...Args>
inline auto ThreadPool::commit(F&& f, Args && ...args) -> std::future<decltype(std::forward<F>(f)(std::forward<Args>(args)...))>
{
	using RetType = std::future<decltype(std::forward<F>(f)(std::forward<Args>(args)...))>;
	if (stop_.load())
	{
		return std::future<RetType>{};
	}

	auto task = std::make_shared<std::packaged_task<RetType()>>(
		std::bind(std::forward<F>(f), std::forward<Args>(args)...)
	);

	std::future<RetType> ret = task->get_future();
	{
		std::lock_guard<std::mutex> cv_mt(cv_mt_);
		this->tasks_.emplace([task] {
			(*task)();
			});
	}

	cv_lock_.notify_one();
	return ret;
}
