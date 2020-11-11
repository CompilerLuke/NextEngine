//
//  atomic.h
//  NextCore
//
//  Created by Antonella Calvia on 01/11/2020.
//

#pragma once

#ifdef _MSC_VER
#    include <windows.h>
#    define TASK_YIELD() YieldProcessor()
#    define TASK_COMPILER_BARRIER _ReadWriteBarrier();
#    define TASK_MEMORY_BARRIER std::atomic_thread_fence(std::memory_order_seq_cst);
#else
#    include <emmintrin.h>
#    define TASK_YIELD() _mm_pause();
#    define TASK_COMPILER_BARRIER asm volatile("" ::: "memory");
#    define TASK_MEMORY_BARRIER asm volatile("mfence" ::: "memory");
#endif
