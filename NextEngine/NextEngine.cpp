// NextEngine.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"

#include "core/allocator.h"
#include "core/temporary.h"

MallocAllocator default_allocator;
TemporaryAllocator temporary_allocator(10000000);