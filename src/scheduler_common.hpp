/*
 *  scheduler_common.hpp
 *  Task Scheduler
 *
 *  Created by Jedd Haberstro on 22/01/2011.
 *  Copyright 2011 DS Media Labs, Inc. All rights reserved.
 *
 */

#ifndef SCHEDULER_COMMON_HPP
#define SCHEDULER_COMMON_HPP

#include <sys/types.h>
#include <sys/sysctl.h>

#define CACHE_LINE_SIZE 64

typedef void (*task_function) (void*);

namespace internal
{
    struct task
	{
		task_function func;
		void* context;
	};
	
	// http://stackoverflow.com/questions/150355/programmatically-find-the-number-of-cores-on-a-machine
    int number_of_cores() {
        int numCPU = 0;
        int mib[4];
        size_t len = sizeof(numCPU); 
        
        /* set the mib for hw.ncpu */
        mib[0] = CTL_HW;
        mib[1] = HW_AVAILCPU;  // alternatively, try HW_NCPU;
        
        /* get the number of CPUs from the system */
        sysctl(mib, 2, &numCPU, &len, NULL, 0);
        
        if (numCPU < 1) {
            mib[1] = HW_NCPU;
            sysctl(mib, 2, &numCPU, &len, NULL, 0);
            
            if (numCPU < 1) {
                numCPU = 1;
            }
        }
        
        return numCPU;
    }
}

#endif // SCHEDULER_COMMON_HPP