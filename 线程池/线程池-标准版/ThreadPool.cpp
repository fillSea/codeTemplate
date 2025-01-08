#include "ThreadPool.h"

ThreadPool::~ThreadPool()
{
	stop();
}

ThreadPool& ThreadPool::instance()
{
	static ThreadPool ins;
	return ins;
}

int ThreadPool::idleThreadCount()
{
	return thread_num_;
}

ThreadPool::ThreadPool(unsigned int num)
{
	if (num <= 1)
		thread_num_ = 2;
	else
		thread_num_ = num;
}

void ThreadPool::start()
{
	for (int i = 0; i < thread_num_; ++i)
	{
		pool_.emplace_back([this] {
			while (!this->stop_.load())
			{
				Task task;
				{
					std::unique_lock<std::mutex> cv_mt(cv_mt_);
					this->cv_lock_.wait(cv_mt, [this] {
						return this->stop_.load() || !this->tasks_.empty();
						});
					if (this->tasks_.empty()) return;

					task = std::move(this->tasks_.front());
					this->tasks_.pop();
				}

				--thread_num_;
				task();
				++thread_num_;
			}
			});
	}
}

void ThreadPool::stop()
{
	stop_.store(true);
	cv_lock_.notify_all();
	for (auto& td : pool_)
	{
		if (td.joinable())
		{
			std::cout << "join thread " << td.get_id() << std::endl;
			td.join();
		}
	}
}
