// NextEngine.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"

#include "core/allocator.h"
#include "core/temporary.h"

MallocAllocator ENGINE_API default_allocator;
TemporaryAllocator ENGINE_API temporary_allocator(10000000);