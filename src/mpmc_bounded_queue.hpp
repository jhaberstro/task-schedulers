/*
 *  mpmc_bounded_queue.hpp
 *  Task Scheduler
 *
 *  Created by Jedd Haberstro on 21/01/2011.
 *  Copyright Jedd Haberstro. All rights reserved.
 *
 */

#ifndef MPMC_BOUNDED_QUEUE_HPP
#define MPMC_BOUNDED_QUEUE_HPP

#include "atomic.hpp"

template< typename T >
class mpmc_bounded_queue
{
public:
	
	mpmc_bounded_queue(size_t size)
	: buffer_(new cell[size]),
	  bufferMask_(size - 1) {
        assert((size >= 2) && ((size & (size - 1)) == 0));
		for (size_t i = 0; i < size; ++i) {
			buffer_[i].sequence.store(i, memory_order_relaxed);
		}
		
		enqueuePos_.store(0, memory_order_relaxed);
		dequeuePos_.store(0, memory_order_relaxed);
	}
	
	~mpmc_bounded_queue() {
		delete [] buffer_;
	}
	
	bool enqueue(T const& data) {
		cell* cell = 0;
		size_t position = enqueuePos_.load(memory_order_relaxed);
		while (true) {
			cell = &buffer_[position & bufferMask_];
			size_t sequence = cell->sequence.load(memory_order_acquire);
			intptr_t difference = static_cast< intptr_t >(sequence) - static_cast< intptr_t >(position);
			if (difference == 0) {
				if (enqueuePos_.compare_exchange_weak(position, position + 1, memory_order_relaxed)) {
					break;
				}
			}
			else if (difference < 0) {
				return false;
			}
			else {
				position = enqueuePos_.load(memory_order_relaxed);
			}
		}
		
		cell->data = data;
		cell->sequence.store(position + 1, memory_order_release);
		
		return true;
	}
	
	bool dequeue(T& data) {
		cell* cell = 0;
		size_t position = dequeuePos_.load(memory_order_relaxed);
		while (true) {
			cell = &buffer_[position & bufferMask_];
			size_t sequence = cell->sequence.load(memory_order_acquire);
			intptr_t difference = static_cast< intptr_t >(sequence) - static_cast< intptr_t >(position + 1);
			if (difference == 0) {
				if (dequeuePos_.compare_exchange_weak(position, position + 1, memory_order_relaxed)) {
					break;
				}
			}
			else if (difference < 0) {
				return false;
			}
			else {
				position = dequeuePos_.load(memory_order_relaxed);
			}
		}
		
		data = cell->data;
		cell->sequence.store(position + bufferMask_ + 1, memory_order_release);
		
		return true;
	}
	
private:
	
	struct cell
	{
		atomic< size_t > sequence;
		T data;
	};
	
	enum { kCachelineSize = 64 };
	typedef char cacheline_pad [kCachelineSize];
	
	cacheline_pad pad0_;
	cell* const buffer_;
	size_t const bufferMask_;
	cacheline_pad pad1_;
	atomic< size_t > enqueuePos_;
	cacheline_pad pad2_;
	atomic< size_t > dequeuePos_;
	cacheline_pad pad3_;
	
	mpmc_bounded_queue(mpmc_bounded_queue const&);
	void operator=(mpmc_bounded_queue const&);
};

#endif // MPMC_BOUNDED_QUEUE_HPP