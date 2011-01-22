/*
 *  atomic.hpp
 *  Task Scheduler
 *
 *  Created by Jedd Haberstro on 21/01/2011.
 *  Copyright 2011 DS Media Labs, Inc. All rights reserved.
 *
 */

#ifndef ATOMIC_HPP
#define ATOMIC_HPP

#include <TargetConditionals.h>


void compiler_barrier() {
	asm volatile ("" ::: "memory");
}

// Returns new value
template< typename T >
inline T atomic_increment(T& value) {
	return __sync_add_and_fetch(&value, 1);
}

template< typename T >
inline T atomic_decrement(T& value) {
	return __sync_sub_and_fetch(&value, 1);
}

template< typename T >
inline T load_acquire(T const& x) {
	T result = x;
	compiler_barrier();
	return result;
}

template< typename T >
inline void store_release(T& out, T x) {
	compiler_barrier();
	out = x;
}

// 64-bit only
template< typename T >
inline T exchange_pointer(T volatile* address, T value) {
	T result;
	asm volatile(
		"lock; xchgq %0, %1\n\t"
		: "=r" (result), "=m" (*address)
		: "0" (value), "m"(*address)
		: "memory"
	);
	
	return result;
}

// sizeof(T) must be <= 8
template< typename T >
class atomic
{
public:
	
	typedef T value_type;
	
	value_type operator=(value_type v) {
		store_release(value_, v);
        return value_;
	}
	
	value_type operator++() {
		return atomic_increment(value_);
	}
	
	value_type operator++(int) {
		return atomic_increment(value_) - 1;
	}
	
	value_type operator--() {
		return atomic_decrement(value_);
	}
	
	value_type operator--(int) {
		return atomic_decrement(value_) + 1;
	}
	
	operator value_type() const {
		return load_acquire(value_);
	}
	
private:
	
	T value_;
};

#endif // ATOMIC_HPP