#ifndef TASK_HPP
#define TASK_HPP

#include "mpmc_bounded_queue.hpp"
#include "mpsc_queue.hpp"
#include "thread.hpp"
#include <queue>
#include <vector>
#include <iostream>

typedef int32_t task_id;
enum { kNullTask = -1 };

typedef void (*cpu_task_func) (void* context);

union task_work_item
{
    struct { cpu_task_func func; void* context; } cpu_work;
};

struct task_t
{
    task_id id;
    task_work_item work;
    task_id parent;
    int32_t open_work_items;
    std::vector< task_id > dependents;
    int32_t dependency_fulfilled;
};

class task_manager
{    
private:

	struct worker_thread_data
	{
		thread thread_;
		task_manager* scheduler_;
	};
    
    // this function needs to be rewritten!
	static void worker_thread_func(void* data) {
		task_manager* context = static_cast< task_manager* >(data);
		while (!context->kill) {
			task_t* run = 0;
            while (context->tasks.dequeue(run) == true) {
                if (context->kill) {
                    return;
                }
                
                if (run->work.cpu_work.func) {
                    run->work.cpu_work.func(run->work.cpu_work.context);
                }
                
                context->decrement_task(run->id);
            }

			thread::sleep(0, 1000);
		}
	}
	
public:
    
    task_manager(size_t maxTasks, size_t numThreads = 0)
    : tasks(maxTasks),
	  kill(false) {
		if (numThreads == 0) {
			numThreads = internal::number_of_cores() - 1;
		}

		for (int i = 0; i < numThreads; ++i) {
			worker_thread_data worker;
			worker.thread_ = thread(worker_thread_func);
			worker.scheduler_ = this;
			workers_.push_back(worker);
			workers_[i].thread_.start(this);
		}
	}
    
    ~task_manager() {
        stop();
        for (int i = 0; i < open_tasks.size(); ++i) {
            task_t* t = open_tasks[i];
            if (t) {
                delete t;
                t = 0;
            }
        }
        
        mpsc_queue< task_id >::node* n = 0;
        while ((n = availableIds.pop()) != 0) {
            delete n;
            n = 0;
        }
    }
    
    task_id create_task(cpu_task_func func, void* context) {
        task_t* newtask = new task_t;
        
        mpsc_queue< task_id >::node* popped = availableIds.pop();
        if (popped == 0) {
            newtask->id = open_tasks.size();
        }
        else {
            newtask->id = popped->value;
            delete popped;
        }
        
        newtask->work.cpu_work.func = func;
        newtask->work.cpu_work.context = context;
        newtask->parent = kNullTask;
        newtask->open_work_items = 2;
        newtask->dependency_fulfilled = 0;
        
        if (newtask->id == open_tasks.size()) {
            open_tasks.push_back(newtask);
        }
        else {
            open_tasks[newtask->id] = newtask;
        }
        
        current_transaction.push_back(newtask->id);
        return newtask->id;
    }
    
    void add_child(task_id parentid, task_id childid) {
        task_t* parent = open_tasks[parentid];
        task_t* child = open_tasks[childid];
        
        assert(child->parent == kNullTask);
        //assert(child->dependency == kNullTask);
        child->parent = parentid;
        parent->open_work_items += 1;
    }
    
    void add_dependency(task_id taskid, task_id dependentid) {
        task_t* parent = open_tasks[taskid];
        task_t* dependent = open_tasks[dependentid];
        parent->dependents.push_back(dependentid);
        dependent->dependency_fulfilled += 1;
        
    }
    
    void submit_current_transaction() {
        for (int i = 0; i < current_transaction.size(); ++i) {
            task_id id = current_transaction[i];
            task_t* task = open_tasks[id];
            if (task->dependency_fulfilled == 0) {
                tasks.enqueue(task);
            }
            else {
                dependency_tasks.push_back(task);
            }
            
            decrement_task(id);
        }
        
        current_transaction.clear();
    }
    
    void wait(task_id id) {
        task_t* task = open_tasks[id];
        if (task == 0) {
            return;
        }
        
        while (task->open_work_items > 0) {    
            // scan through all tasks that have dependencies
            // and enqueue them if their dependencies has completed.
            for (uint32_t i = 0; i < dependency_tasks.size(); ++i) {
                task_t* dependenttask = dependency_tasks[i];
                if (dependenttask->dependency_fulfilled == 0) {
                    std::swap(dependency_tasks[i], dependency_tasks.back());
                    dependency_tasks.pop_back();
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
    
    void decrement_task(task_id task) {        
        task_t* current = open_tasks[task];
        while (current != 0) {
            task_t* deletion = current;
            int items = atomic_decrement(current->open_work_items);
            if (items == 0) {
                for (int i = 0; i < current->dependents.size(); ++i) {
                    atomic_decrement(open_tasks[current->dependents[i]]->dependency_fulfilled);
                }
                
                if (current->parent != kNullTask) {
                    current = open_tasks[current->parent];
                }
                else {
                    current = 0;
                }
                                
                // remove the task from the open_list
                availableIds.push(deletion->id);
                
                open_tasks[deletion->id] = 0;
                delete deletion;
            }
            else {
                current = 0;
            }
        }
    }
    
private:
    
    mpsc_queue< task_id > availableIds;
    std::vector< task_t* > open_tasks;
    std::vector< task_id > current_transaction;
    std::vector< task_t* > dependency_tasks;
    mpmc_bounded_queue< task_t* > tasks;
    std::vector< worker_thread_data > workers_;
	bool kill;
};

#endif // TASK_HPP
