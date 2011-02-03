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
    while (true) {
        mutex_state_t state = state_.load(memory_order_acquire);
        if (state == kLocked) {
            active_pause();
        }
        else {
            mutex_state_t newstate = kUnlocked;
            bool success = state_.compare_exchange_weak(newstate, kLocked, memory_order_relaxed);
            if (success) {
                break;
            }
        }
    }
}

inline bool mutex::try_lock() {
    mutex_state_t state = state_.load(memory_order_acquire);
    if (state == kUnlocked) {
        mutex_state_t newstate = kUnlocked;
        return state_.compare_exchange_weak(newstate, kLocked, memory_order_relaxed);
    }
    
    return false;
}

inline void mutex::unlock() {
    state_.store(kUnlocked, memory_order_relaxed);
}


#endif // MUTEX_HPP