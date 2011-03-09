/*
 *  work_stealing_lock_scheduler.hpp
 *  Task Scheduler
 *
 *  Created by Jedd Haberstro on 27/01/2011.
 *  Copyright 2011 DS Media Labs, Inc. All rights reserved.
 *
 */
 
#ifndef WORK_STEALING_LOCK_SCHEDULER_HPP
#define WORK_STEALING_LOCK_SCHEDULER_HPP

#include "atomic.hpp"
#include "work_stealing_lock_deque.hpp"
#include "scheduler_common.hpp"
#include "thread.hpp"
#include <vector>

class work_stealing_lock_scheduler
{
private:
    
    typedef work_stealing_lock_deque< internal::task > task_deque;
    
private:
    
    struct worker_thread_data
	{
		thread thread_;
        task_deque tasks_;
		work_stealing_lock_scheduler* scheduler_;
        int victim_;
	};
	
	static void worker_thread_func(void* data) {
		worker_thread_data* context = static_cast< worker_thread_data* >(data);
		while (!context->scheduler_->kill_) {
			internal::task task;
			while(context->tasks_.try_pop_front(task)) {
				task.func(task.context);
                --(context->scheduler_->numTasks_);
			}
			
            int failure = 0;
            work_stealing_lock_scheduler* scheduler = context->scheduler_;
			while (true) {
			    if (context->scheduler_->kill_) {
			        break;
			    }
			    
                worker_thread_data& victim = *scheduler->workers_[context->victim_];
                internal::task task;
    			if (victim.tasks_.try_pop_back(task)) {
                    task.func(task.context);
                    //context->tasks_.push_back(task);
                    break;
    			}
    			
                context->victim_ = context->victim_ % scheduler->workers_.size();
                ++failure;
                if (failure >= scheduler->workers_.size()) {
                    failure = 0;
                    thread::sleep(0, 1000);
                }
			}			
		}
	}
    
public:
    
    work_stealing_lock_scheduler(size_t numThreads = 0)
    : distributee_(0),
      kill_(false) {
        numTasks_.store(0, memory_order_relaxed);
        if (numThreads == 0) {
			numThreads = internal::number_of_cores();
		}
		
		for (int i = 0; i < numThreads; ++i) {
			worker_thread_data* worker = new worker_thread_data;
			worker->thread_ = thread(worker_thread_func);
			worker->scheduler_ = this;
            worker->victim_ = (i + 1) % numThreads;
			workers_.push_back(worker);
		}
		
        for (int i = 0; i < numThreads; ++i) {
            workers_[i]->thread_.start(workers_[i]);
        }
    }
    
    ~work_stealing_lock_scheduler() {
		kill_ = true;
		for (int i = 0; i < workers_.size(); ++i) {
			workers_[i]->thread_.join();
		}
	}
	
	void wait_for_all_tasks() {
        int backoff = 0;
        // Should investigate the proper usage of the memory_order parameter
	    while(numTasks_.load(memory_order_acquire) != 0) {
	        if (backoff < 10) {
                active_pause();
	        }
	        else if (backoff < 20) {
                for (int i = 0; i != 50; ++i) {
                    active_pause();
                }
	        }
	        else if (backoff < 26) {
                thread::yield();
	        }
	        else {
                thread::sleep(0, 10000000);
	        }
	        
            ++backoff;
	    }
	}
	
	void submit_task(task_function func, void* context) {
        internal::task task = { func, context };
        pthread_t id = thread::current_id();
        for (int i = 0; i < workers_.size(); ++i) {
            if (thread::ids_equal(id, workers_[i]->thread_.id())) {
                workers_[i]->tasks_.push_back(task);
                return;
            }
        }
        
        workers_[distributee_]->tasks_.push_back(task);
        distributee_ = (distributee_ + 1) % workers_.size();
	}
    
protected:
    
    std::vector< worker_thread_data* > workers_;
    atomic< size_t > numTasks_;
    size_t distributee_;
	bool kill_;
};

#endif // WORK_STEALING_LOCK_SCHEDULER_HPP

