//
//  spin_lock.hpp
//  Task Scheduler
//
//  Created by Jedd Haberstro on 3/13/11.
//  Copyright 2011 Student. All rights reserved.
//

#ifndef SPINLOCK_HPP
#define SPINLOCK_HPP

#include "atomic.hpp"

class spin_lock
{
public:
    
    spin_lock()
    : lock_(0) {
    }
    
    void lock() {
        while (true) {
            if (!exchange32(&lock_, 1)) {
                return;
            }
            
            while (lock_) {
                active_pause();
            }
        }
    }
    
    bool try_lock() {
        return exchange32(&lock_, 1);
    }
    
    void unlock() {
        compiler_barrier();
        lock_ = 0;
    }
    
private:
    
    spin_lock(spin_lock const&);
    spin_lock& operator=(spin_lock const&);
    
private:
    
    int32_t lock_;
};

#endif //  SPINLOCK_HPP