//
//  spsc_queue.hpp
//  Task Scheduler
//
//  Created by Jedd Haberstro on 3/12/11.
//  Copyright 2011 Student. All rights reserved.
//

#ifndef SPSC_QUEUE_HPP
#define SPSC_QUEUE_HPP

#include "scheduler_common.hpp"
#include <cstddef>

template< typename T >
class spsc_queue
{
public:
    
    spsc_queue() {
        node* n = new node;
        n->next_ = 0;
        tail_ = head_ = first_ = tail_copy_ = n;
    }
    
    ~spsc_queue() {
        node*n = first_;
        do {
            node* next = n->next_;
            delete n;
            n = next;
        } while (n);
    }
    
    void enqueue(T v) {
        node* n = alloc_node();
        n->next_ = 0;
        n->value_ = v;
        store_release(head_->next_, n);
        head_ = n;
    }
    
    bool dequeue(T& v) {
        if (load_acquire(tail_->next_)) {
            v = tail_->next_->value_;
            store_release(tail_, tail_->next_);
            return true;
        }
        else {
            return false;
        }
    }
    
private:
    
    spsc_queue(spsc_queue const&);
    spsc_queue& operator=(spsc_queue const&);
    
private:
    
    struct node
    {
        node* next_;
        T value_;
    }
    
    node* tail_;
    char cache_line_pad_[CACHE_LINE_SIZE];
    node* head_;
    node* first_;
    node* tail_copy_;
    
    node* alloc_node() {
        if (first_ != tail_copy_) {
            node* n = first_;
            first_ = first_->next_;
            return n;
        }
        
        tail_copy_ = load_acquire(tail_);
        if (first_ != tail_copy_) {
            node* n = first_;
            first_ = first_->next_;
            return n;
        }
        
        node* n = new node;
        return n;
    }
};

#endif // SPSC_QUEUE_HPP