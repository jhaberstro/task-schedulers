/*
 *  task_distributing_scheduler.hpp
 *  Task Scheduler
 *
 *  Created by Jedd Haberstro on 21/01/2011.
 *  Copyright 2011 DS Media Labs, Inc. All rights reserved.
 *
 */

#ifndef TASK_DISTRIBUTING_SCHEDULER_HPP
#define TASK_DISTRIBUTING_SCHEDULER_HPP

#include "mpsc_queue.hpp"
#include "thread.hpp"
#include <vector>
#include <sys/types.h>
#include <sys/sysctl.h>


typedef void (*task_function) (void*);

class task_distributing_scheduler;
namespace internal
{	
	struct task
	{
		task_function func;
		void* context;
	};
	
	typedef mpsc_queue< task > task_queue;
	
	struct worker_thread_data
	{
		task_queue tasks_;
		thread thread_;
		task_distributing_scheduler* scheduler;
	};
	
	void worker_thread_func(void* data);
	
	int number_of_cores();
}

class task_distributing_scheduler
{	
private:
	
	typedef mpsc_queue< internal::task > task_queue;
	
public:
	
	task_distributing_scheduler(int numWorkerThreads = 0)
	: currentWorker_(0) {
		numTasks_ = 0;
		newTasks_ = 0;
		if (numWorkerThreads == 0) {
			numWorkerThreads = internal::number_of_cores();
		}
		
		for (int i = 0; i < numWorkerThreads; ++i) {
			internal::worker_thread_data worker;
			worker.thread_ = thread(internal::worker_thread_func);
			worker.scheduler = this;
			workerThreads_.push_back(worker);
			workerThreads_[i].thread_.start(&workerThreads_[i]);
		}
	}
	
	~task_distributing_scheduler() {
		wait_for_all_tasks();
		
		for (int i = 0; i < workerThreads_.size(); ++i) {
			workerThreads_[i].thread_.join();
		}
	}
	
	void wait_for_all_tasks() {
		
		// Distribute tasks
		while(numTasks_ > 0) {
			if (newTasks_ > 0) {
				task_queue::node* n = tasks_.pop();
				--newTasks_;
				if (workerThreads_.size() != 0)  {
					workerThreads_[currentWorker_].tasks_.push(n);
				}
				else {
					task_function func = n->value.func;
					void* context = n->value.context;
					func(context);
					--numTasks_;
				}
				
				currentWorker_ += (currentWorker_ + 1) % workerThreads_.size();				
			}
			
			thread::yield();
		}				
	}
	
	void submit_task(task_function func, void* context) {
		++numTasks_;
        ++newTasks_;
		internal::task t = { func, context };
		tasks_.push(new task_queue::node(t));
	}
    
public:
    
    atomic< int > numTasks_;
	atomic< int > newTasks_;
	
private:
	
	std::vector< internal::worker_thread_data > workerThreads_;
	task_queue tasks_;
	unsigned int currentWorker_;
};


namespace internal
{
	void worker_thread_func(void* data) {
		worker_thread_data* workerThreadData = static_cast< worker_thread_data* >(data);
		while(workerThreadData->scheduler->numTasks_ > 0) {
			task_queue::node* n = 0;
			while((n = workerThreadData->tasks_.pop()) != 0) {
				task_function func = n->value.func;
				void* context = n->value.context;
				func(context);
				--(workerThreadData->scheduler->numTasks_);
			}
			
			thread::sleep(0, 2000);
		}
	}
	
	// http://stackoverflow.com/questions/150355/programmatically-find-the-number-of-cores-on-a-machine
	int number_of_cores() {
        int numCPU = 0;
		int mib[4];
		size_t len = sizeof(numCPU); 

		/* set the mib for hw.ncpu */
		mib[0] = CTL_HW;
		mib[1] = HW_AVAILCPU;  // alternatively, try HW_NCPU;

		/* get the number of CPUs from the system */
		sysctl(mib, 2, &numCPU, &len, NULL, 0);

		if( numCPU < 1 ) 
		{
		     mib[1] = HW_NCPU;
		     sysctl( mib, 2, &numCPU, &len, NULL, 0 );

		     if( numCPU < 1 )
		     {
		          numCPU = 1;
		     }
		}
        
        return numCPU;
	}
}

#endif // TASK_DISTRIBUTING_SCHEDULER_HPP
