/*
 *  mutex.hpp
 *  Task Scheduler
 *
 *  Created by Jedd Haberstro on 25/01/2011.
 *  Copyright 2011 DS Media Labs, Inc. All rights reserved.
 *
 */

#ifndef MUTEX_HPP
#define MUTEX_HPP

#include "atomic.hpp"
#include <algorithm>

class mutex
{
private:    
    enum mutex_state_t
    {
        kUnlocked = 0,
        kLocked = 1
    };
    
public:
    
	mutex();
			
	void lock();
	
	bool try_lock();
	
	void unlock();
	
private:
    
    mutex(mutex const& other);
	mutex& operator=(mutex const& other);
	
private:
	
    atomic< mutex_state_t > state_;
};


inline mutex::mutex() {
    state_.store(kUnlocked, memory_order_relaxed);
}


inline void mutex::lock() {
    static mutex_state_t oldstate_ = kUnlocked;
    unsigned int delay = 1;
    while()
}

inline bool mutex::try_lock() {
    static mutex_state_t oldstate_ = kUnlocked;
    mutex_state_t state = state_.load(memory_order_acquire);
    if (state == kUnlocked) {
        return state_.compare_exchange_weak(oldstate_, kLocked, memory_order_acquire);
    }
    
    return false;
}

inline void mutex::unlock() {
    state_.store(kUnlocked, memory_order_release);
}


#endif // MUTEX_HPP