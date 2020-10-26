#pragma once

#include "core/core.h"
#include <assert.h>

const uint MAX_THREADS = 64;

struct worker_handle {
	uint id = 0;
};

CORE_API uint get_worker_id();
CORE_API uint hardware_thread_count();
CORE_API uint worker_thread_count();