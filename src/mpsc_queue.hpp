/*
 *  mpsc_queue.hpp
 *  Task Scheduler
 *
 *  Created by Jedd Haberstro on 20/01/2011.
 *  Copyright 2011 DS Media Labs, Inc. All rights reserved.
 *
 */

#ifndef MPSC_QUEUE_HPP
#define MPSC_QUEUE_HPP

#include "atomic.hpp"

template< typename T >
class mpsc_queue
{
public:
	
	struct node
	{
		node()
		: next(0) {
		}
		
		node(T const& v)
		: next(0), value(v) {
		}
		
		node* volatile next;
		T value;
	};
	
public:
	
	mpsc_queue() {
		head_ = tail_ = new node;
	}
	
	void push(T& v) {
        node* n = new node(v);
		n->next = 0;
		node* previous = exchange_pointer(&head_, n);
		previous->next = n;
	}
	
	node* pop() {
		node* tail = tail_;
		node* next = tail->next;
		if (next != 0) {
			tail_ = next;
			tail->value = next->value;
			return tail;
		}
		
		return 0;
	}
	
private:
	
	node* volatile head_;
	node* tail_;
};

#endif // MPSC_QUEUE_HPP