/*
 *  work_stealing_lock_deque.hpp
 *  Task Scheduler
 *
 *  Created by Jedd Haberstro on 27/01/2011.
 *  Copyright 2011 DS Media Labs, Inc. All rights reserved.
 *
 */

#ifndef WORK_STEALING_LOCK_DEQUE_HPP
#define WORK_STEALING_LOCK_DEQUE_HPP

#include "mutex.hpp"
#include <deque>

template< typename T >
class work_stealing_lock_deque
{
public:
    
    typedef T value_type;
    
public:
    
    work_stealing_lock_deque();
    
    ~work_stealing_lock_deque();
    
    bool try_pop_back(value_type& value);
    
    bool try_pop_front(value_type& value);
    
    void push_back(value_type const& value);
    
private:
    
    std::deque< T > deque_;
    mutex mutex_;
};


template< typename T >
work_stealing_lock_deque< T >::work_stealing_lock_deque() {
}

template< typename T >
work_stealing_lock_deque< T >::~work_stealing_lock_deque() {
}

template< typename T >
inline bool work_stealing_lock_deque< T >::try_pop_back(typename work_stealing_lock_deque< T >::value_type& value) {
    mutex_.lock();
    if (deque_.empty()) {
        mutex_.unlock();
        return false;
    }
    
    value = deque_.back();
    deque_.pop_back();
    mutex_.unlock();
    return true;
}

template< typename T >
inline bool work_stealing_lock_deque< T >::try_pop_front(typename work_stealing_lock_deque< T >::value_type& value) {
    mutex_.lock();
    if (deque_.empty()) {
        mutex_.unlock();
        return false;
    }
    
    value = deque_.front();
    deque_.pop_front();
    mutex_.unlock();
    return true;
}

template< typename T >
inline void work_stealing_lock_deque< T >::push_back(typename work_stealing_lock_deque< T >::value_type const& value) {
    mutex_.lock();
    deque_.push_back(value);
    mutex_.unlock();
}

#endif // WORK_STEALING_LOCK_DEQUE_HPP