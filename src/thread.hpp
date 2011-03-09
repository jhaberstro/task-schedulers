/*
 *  thread.hpp
 *  Task Scheduler
 *
 *  Created by Jedd Haberstro on 20/01/2011.
 *  Copyright 2011 DS Media Labs, Inc. All rights reserved.
 *
 */

#ifndef THREAD_HPP
#define THREAD_HPP

#include <algorithm>
#include <cassert>
#include <limits>
#include <pthread.h>
#include <stdint.h>
#include <time.h>

typedef void (*thread_function) (void*);
 
namespace internal
{
	struct thread_func_data
    {
        thread_function func;
        void* arg;
    };
    
    inline void* thread_func_proxy(void* arg) {
        thread_func_data* data = (thread_func_data*)arg;
        data->func(data->arg);
        delete data;
        return 0;
    }
}

class thread
{
public:
		
	static pthread_t current_id();
	
	static bool ids_equal(pthread_t lhs, pthread_t rhs);
    
	static void sleep(uint64_t seconds, uint64_t nanoseconds);
    
    static void yield();

	thread();
	
	explicit thread(thread_function function);
	
	thread(thread const& other);
	
	~thread();
	
	void start(void* arg = 0);
	
	void join();
	
	void detach();
	
	pthread_t id() const;
	
	bool running() const;
	
	void swap(thread& other);
	
	thread& operator=(thread const& other);
	
	bool operator==(thread const& other);

	bool operator!=(thread const& other);
	
private:
	
	thread_function func_;
	pthread_t id_;
	bool launched_;
};


inline pthread_t thread::current_id() {
	return pthread_self();
}

inline bool thread::ids_equal(pthread_t lhs, pthread_t rhs) {
    return pthread_equal(lhs, rhs) != 0;
}


inline void thread::sleep(uint64_t seconds, uint64_t nanoseconds) {
    assert(seconds <= std::numeric_limits< time_t >::max());
    assert(nanoseconds <= std::numeric_limits< long >::max());
    timespec ts;
    ts.tv_sec = static_cast< time_t >(seconds);
    ts.tv_nsec = static_cast< long >(nanoseconds);
    
    timespec remainder;
    while (nanosleep(&ts, &remainder) == -1) {
        ts = remainder;
    }
}

inline void thread::yield() {
	pthread_yield_np();
}

inline thread::thread()
: launched_(false) {
}

inline thread::thread(thread_function function)
: func_(function), launched_(false) {
}

inline thread::thread(thread const& other)
: func_(other.func_),
  id_(other.id_),
  launched_(other.launched_) {
}

inline thread::~thread() {
}

void thread::start(void* arg) {
    assert(launched_ == false);
    internal::thread_func_data* data = new internal::thread_func_data;
    data->func = func_;
    data->arg = arg;
    
    pthread_attr_t attr;
    int err = pthread_attr_init(&attr);
    assert(err == 0);
    err = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    assert(err == 0);
    
    err = pthread_create(&id_, &attr, internal::thread_func_proxy, (void*)data);
    assert(err == 0);
    
    err = pthread_attr_destroy(&attr);
    assert(err == 0);
    
    launched_ = true;
}

inline void thread::join() {
	assert(launched_ == true);
    void* value = 0;
    int err = pthread_join(id_, &value);
    assert(err == 0);
    launched_ = false;
}

inline void thread::detach() {
	assert(launched_ == true);
	int err = pthread_detach(id_);
	assert(err == 0);
}

inline pthread_t thread::id() const {
	return id_;
}

inline bool thread::running() const {
	return launched_;
}

inline void thread::swap(thread& other) {
	std::swap(func_, other.func_);
	std::swap(id_, other.id_);
	std::swap(launched_, other.launched_);
}

inline thread& thread::operator=(thread const& other) {
	if (this != &other) {
		thread(other).swap(*this);
	}
	
	return *this;
}

inline bool thread::operator==(thread const& other) {
	return pthread_equal(id_, other.id_) != 0;
}

inline bool thread::operator!=(thread const& other) {
	return pthread_equal(id_, other.id_) == 0;
}

#endif // THREAD_HPP