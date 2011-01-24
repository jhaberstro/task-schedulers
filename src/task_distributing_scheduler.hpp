/*
 *  task_distributing_scheduler.hpp
 *  Task Scheduler
 *
 *  Created by Jedd Haberstro on 21/01/2011.
 *  Copyright 2011 DS Media Labs, Inc. All rights reserved.
 *
 */

// The idea here is that both the scheduler thread (main thread) and each worker
// thread has a mpsc queue. Tasks can then be submitted to the scheduler from
// any thread. After tasks have been submitted, they are then doled out by
// the scheduler to each worker thread (scheduler dequeue, enqueue worker's queue).
// Worker threads then dequeue from their local task queue and execute the task.

#ifndef TASK_DISTRIBUTING_SCHEDULER_HPP
#define TASK_DISTRIBUTING_SCHEDULER_HPP

#include "mpmc_bounded_queue.hpp"
#include "scheduler_common.hpp"
#include "thread.hpp"
#include <vector>

class task_distributing_scheduler
{
private:
	
	typedef mpmc_bounded_queue< internal::task > task_queue;
	
private:
	
	struct worker_thread_data
	{
		thread thread_;
		task_distributing_scheduler* scheduler_;
	};
	
	static void worker_thread_func(void* data) {
		worker_thread_data* context = static_cast< worker_thread_data* >(data);
		while (!context->scheduler_->kill_) {
			internal::task task;
			while(context->scheduler_->tasks_.dequeue(task)) {
				task.func(task.context);
			}
			
			thread::sleep(0, 1000);
		}
	}
	
public:
	
	task_distributing_scheduler(size_t maxTasks, size_t numThreads = 0)
	: tasks_(maxTasks),
	  kill_(false) {
		if (numThreads == 0) {
			numThreads = internal::number_of_cores();
		}
		
		for (int i = 0; i < numThreads; ++i) {
			worker_thread_data worker;
			worker.thread_ = thread(worker_thread_func);
			worker.scheduler_ = this;
			workers_.push_back(worker);
			workers_[i].thread_.start(&workers_[i]);
		}
	}
	
	~task_distributing_scheduler() {
		kill_ = true;
		for (int i = 0; i < workers_.size(); ++i) {
			workers_[i].thread_.join();
		}
	}
	
	void wait_for_all_tasks() {
		internal::task task;
		while(tasks_.dequeue(task)) {
			task.func(task.context);
		}
	}
	
	void submit_task(task_function func, void* context) {
		internal::task task = { func, context };
		bool success = tasks_.enqueue(task);
		assert(success);
	}
	
private:
	
	task_queue tasks_;
	std::vector< worker_thread_data > workers_;
	bool kill_;
};


#endif // TASK_DISTRIBUTING_SCHEDULER_HPP
