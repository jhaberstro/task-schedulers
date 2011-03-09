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
#include <cassert>


void active_pause() {
    asm volatile ("pause");
}

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
	//return __sync_lock_test_and_set(address, value);
	
	#if 0
	T result;
	asm volatile(
		"lock; xchgq %0, %1\n\t"
		: "=r" (result), "=m" (*address)
		: "0" (value), "m"(*address)
		: "memory"
	);

	return result;
	#endif
	
	T compare, result = value;
	do {
		compare = result;
		result = __sync_val_compare_and_swap(address, compare, value);
	}
	while(result != compare);
	
	return result;
}


enum memory_order
{
	memory_order_relaxed,
    memory_order_consume,
    memory_order_acquire,
    memory_order_release,
    memory_order_acq_rel,
    memory_order_seq_cst
};

// sizeof(T) must be <= 8
template< typename T >
class atomic
{
public:
	
	typedef T value_type;
	
public:
	
	value_type load(memory_order order) const volatile {
		assert(order != memory_order_release && order != memory_order_acq_rel);
		value_type v = value_;
		compiler_barrier();
		return v;
	}
	
	void store(T value, memory_order order) volatile {
		assert(
			order != memory_order_consume &&
			order != memory_order_acquire &&
			order != memory_order_acq_rel
		);
		
		if (order == memory_order_seq_cst) {
			exchange_pointer(&value_, value);
		}
		else {
			compiler_barrier();
			value_ = value;
		}
	}
	
	bool compare_exchange_weak(T& compare, T exchange, memory_order order) volatile {
		T previous = __sync_val_compare_and_swap(&value_, compare, exchange);
		if (previous == compare) {
			return true;
		}
		
		compare = previous;
		return false;
	}
	
	value_type exchange(T v, memory_order order) volatile {
        T previous = exchange_pointer(&value_, v);
        return previous;
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
	
	operator T() const {
        return load(memory_order_seq_cst);
	}
	
private:
	
	T volatile value_;
};

#endif // ATOMIC_HPP