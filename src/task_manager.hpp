#ifndef TASK_HPP
#define TASK_HPP

#include "spin_lock.hpp"
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
    task_id depends_on;
};

void task_initialize(task_t* task) {
    task->id = kNullTask;
    task->work.cpu_work.func = 0;
    task->work.cpu_work.context = 0;
    task->open_work_items = 0;
    task->depends_on = kNullTask;
}

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
    
    task_manager(size_t maxTasks, size_t numThreads = -1)
    : tasks(maxTasks),
      max_tasks(maxTasks),
      num_tasks(0),
	  kill(false),
      waiting_on_task(false) {
		if (numThreads == -1) {
			numThreads = internal::number_of_cores() - 1;
		}
        
        open_tasks = new task_t[maxTasks];
        for (int i = 0; i < maxTasks; ++i) {
            availableIds.push(i);
            task_initialize(&open_tasks[i]);
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
        assert(num_tasks == 0);
        delete [] open_tasks;
        mpsc_queue< task_id >::node* n = 0;
        while ((n = availableIds.pop()) != 0) {
            delete n;
            n = 0;
        }
    }
    
    // Help functions
    void add(cpu_task_func func, void* context) {
        end_add(begin_add(func, context));
    }
    
    task_id begin_add(cpu_task_func func, void* context) {
        atomic_increment(num_tasks);        
        mpsc_queue< task_id >::node* popped = availableIds.pop();
        assert(popped != 0);
        task_id id = popped->value;
        delete popped;
        
        task_t* newtask = &open_tasks[id];
        assert(newtask->id == kNullTask);
        newtask->id = id;
        
        newtask->work.cpu_work.func = func;
        newtask->work.cpu_work.context = context;
        newtask->parent = kNullTask;
        newtask->open_work_items = 2;
        newtask->depends_on = kNullTask;
        
        return newtask->id;
    }
    
    void end_add(task_id id) {
        task_t* task = &open_tasks[id];
        decrement_task(id);
        if (task->depends_on == kNullTask) {
            tasks.enqueue(task);
        }
        else {
            dependency_lock.lock();
            dependents_hold.push_back(task);
            dependency_lock.unlock();
        }
    }
    
    void add_child(task_id parentid, task_id childid) {
        task_t* parent = &open_tasks[parentid];
        task_t* child = &open_tasks[childid];
        
        assert(child->parent == kNullTask);
        //assert(child->dependency == kNullTask);
        child->parent = parentid;
        atomic_increment(parent->open_work_items);
    }
    
    void add_dependency(task_id taskid, task_id dependentid) {
        task_t* dependent = &open_tasks[dependentid];
        assert(dependent->depends_on == kNullTask);
        dependent->depends_on = taskid;
    }
    
    // Can only be called on the main thread currently
    void wait(task_id id) {
        if (!waiting_on_task) {
            waiting_on_task = true;
        }
        
        task_t* task = &open_tasks[id];
        while (task->open_work_items > 0) {
            // help out
            task_t* run = 0;
            if (tasks.dequeue(run) == true) {
                if (run->work.cpu_work.func) {
                    run->work.cpu_work.func(run->work.cpu_work.context);
                }

                decrement_task(run->id);
            }
            
            evaluate_dependencies();
        }
        
        waiting_on_task = false;
    }
    
    void stop() {
        kill = true;
        for (int i = 0; i < workers_.size(); ++i) {
            workers_[i].thread_.join();
        }
    }
    
private:
    
    void decrement_task(task_id task) {        
        task_t* current = &open_tasks[task];
        while (current != 0) {
            if (!waiting_on_task) {
                evaluate_dependencies();
            }
            
            task_t* deletion = current;
            int items = atomic_decrement(current->open_work_items);
            if (items == 0) {
                if (current->parent != kNullTask) {
                    current = &open_tasks[current->parent];
                }
                else {
                    current = 0;
                }
                                
                // remove the task from the open_list
                atomic_decrement(num_tasks);
                task_id deletedid = deletion->id;
                task_initialize(deletion);
                availableIds.push(deletedid);
            }
            else {
                current = 0;
            }
        }
    }
    
    void evaluate_dependencies() {
        if (dependency_lock.try_lock()) {
            std::queue< int > deletions;
            for (int i = 0; i < dependents_hold.size(); ++i) {
                task_t* dependent = dependents_hold[i];
                task_t* depends_on = &open_tasks[dependent->depends_on];
                if (depends_on->open_work_items <= 0) {
                    deletions.push(i);
                    tasks.enqueue(dependent);
                }
            }

            while (deletions.empty() == false) {
                int index = deletions.back();
                deletions.pop();
                dependents_hold.erase(dependents_hold.begin() + index);
            }
            
            dependency_lock.unlock();
        }
    }
    
    //void verify_dag()
    
private:
    
    mpsc_queue< task_id > availableIds;
    mpmc_bounded_queue< task_t* > tasks;
    std::vector< task_t* > dependents_hold;
    spin_lock dependency_lock;
    std::vector< worker_thread_data > workers_;
    task_t* open_tasks;
    int32_t max_tasks;
    int32_t num_tasks;
	bool kill;
    bool waiting_on_task;
};

#endif // TASK_HPP
