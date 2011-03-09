#ifndef TASK_HPP
#define TASK_HPP

#include "mpmc_bounded_queue.hpp"
#include "mpsc_queue.hpp"
#include "thread.hpp"
#include <queue>
#include <vector>
#include <iostream>

namespace task_priority_t
{
    enum e
    {
        kLow = 0,
        kNormal,
        kHigh
    };
}

typedef int32_t task_id_t;
enum { kNullTask = -1 };

typedef void (*cpu_task_func) (void* context);

union task_work_item_t
{
    struct { cpu_task_func func; void* context; } cpu_work;
};

struct task_t
{
    task_id_t id;
    task_work_item_t work;
    task_id_t parent;
    int32_t open_work_items;
    task_priority_t::e priority;
    task_id_t dependency;
};

class task_manager_t
{    
private:

	struct worker_thread_data
	{
		thread thread_;
		task_manager_t* scheduler_;
	};
    
    // this function needs to be rewritten!
	static void worker_thread_func(void* data) {
		task_manager_t* context = static_cast< task_manager_t* >(data);
		while (!context->kill) {
			task_t* run = 0;
            if (context->tasks.dequeue(run) == true) {
                if (run->work.cpu_work.func) {
                    run->work.cpu_work.func(run->work.cpu_work.context);
                }
                
                context->decrement_task(run->id);
            }

			//thread::sleep(0, 1000);
		}
	}
	
public:
    
    task_manager_t(size_t maxTasks, size_t numThreads = 0)
    : tasks(maxTasks),
	  kill(false) {
		if (numThreads == 0) {
			numThreads = internal::number_of_cores();
		}

		for (int i = 0; i < numThreads; ++i) {
			worker_thread_data worker;
			worker.thread_ = thread(worker_thread_func);
			worker.scheduler_ = this;
			workers_.push_back(worker);
			workers_[i].thread_.start(this);
		}
	}
    
    task_id_t create_task(cpu_task_func func, void* context, task_priority_t::e priority = task_priority_t::kLow) {
        task_t* newtask = new task_t;
        if (availableIds.empty()) {
            newtask->id = open_tasks.size();
        }
        else {
            newtask->id = availableIds.front();
            availableIds.pop();
        }
        newtask->work.cpu_work.func = func;
        newtask->work.cpu_work.context = context;
        newtask->parent = kNullTask;
        newtask->open_work_items = 2;
        newtask->priority = priority;
        newtask->dependency = kNullTask;
        
        if (newtask->id == open_tasks.size()) {
            open_tasks.push_back(newtask);
        }
        else {
            open_tasks[newtask->id] = newtask;
        }
        
        current_transaction.push(newtask->id);
        return newtask->id;
    }
    
    void add_child(task_id_t parentid, task_id_t childid) {
        task_t* parent = open_tasks[parentid];
        task_t* child = open_tasks[childid];
        
        assert(child->parent == kNullTask);
        assert(child->dependency == kNullTask);
        child->parent = parentid;
        parent->open_work_items += 1;
    }
    
    void submit_current_transaction() {
        while(current_transaction.size() != 0) {
            task_t* task = open_tasks[current_transaction.back()];
            if (task->dependency == kNullTask) {
                tasks.enqueue(task);
            }
            else {
                dependency_tasks.push_back(task);
            }
            
            decrement_task(current_transaction.back());
            current_transaction.pop();
        }
    }
    
    void wait(task_id_t id) {
        task_t* task = open_tasks[id];
        if (task == 0) {
            return;
        }
        
        while (task->open_work_items > 0) {    
            // scan through all tasks that have dependencies
            // and enqueue them if their dependency has completed.
            for (uint32_t i = 0; i < dependency_tasks.size(); ++i) {
                task_t* dependenttask = dependency_tasks[i];
                if (open_tasks[dependenttask->dependency]->open_work_items == 1) {
                    std::swap(dependency_tasks[i], dependency_tasks.back());
                    dependency_tasks.pop_back();
                    dependenttask->dependency = kNullTask;
                    tasks.enqueue(dependenttask);
                }
            }
            
            // help out
            task_t* run = 0;
            if (tasks.dequeue(run) == true) {
                if (run->work.cpu_work.func) {
                    run->work.cpu_work.func(run->work.cpu_work.context);
                }

                decrement_task(run->id);
            } 
        }
    }
    
    void stop() {
        kill = true;
        for (int i = 0; i < workers_.size(); ++i) {
            workers_[i].thread_.join();
        }
    }
    
private:
    
    void decrement_task(task_id_t task) {        
        task_t* current = open_tasks[task];
        while (current != 0) {
            task_t* deletion = current;
            int items = atomic_decrement(current->open_work_items);
            if (items == 0) {
                if (current->parent != kNullTask) {
                    current = open_tasks[current->parent];
                }
                else {
                    current = 0;
                }
                
                // remove the task from the open_list
                availableIds.push(deletion->id);
                delete deletion;
                open_tasks[deletion->id] = 0;
            }
            else {
                current = 0;
            }
        }
    }
    
private:
    
    std::queue< task_id_t > availableIds;
    std::vector< task_t* > open_tasks;
    std::queue< task_id_t > current_transaction;
    std::vector< task_t* > dependency_tasks;
    mpmc_bounded_queue< task_t* > tasks;
    std::vector< worker_thread_data > workers_;
	bool kill;
};

#endif // TASK_HPP
