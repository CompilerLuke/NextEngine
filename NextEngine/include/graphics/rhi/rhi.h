#pragma once

#include "device.h"
#include "buffer.h"
#include "frame_buffer.h"
#include "pipeline.h"
#include "primitives.h"


struct RHI;
struct BufferAllocator;

struct AppInfo {
	const char* app_name;
	const char* engine_name;
};

struct DeviceFeatures {
	bool sampler_anistropy = true;
	bool multi_draw_indirect = true;
};

ENGINE_API void make_RHI(AppInfo& info, DeviceFeatures&);
void ENGINE_API begin_gpu_upload();
void ENGINE_API end_gpu_upload();
void ENGINE_API queue_for_destruction(void*, void(*)(void*)); //may be worth using std::function instead

template<typename T>
void queue_t_for_destruction(T data, void(*func)(T)) {
	queue_for_destruction(data, (void(*)(void*))func);
}

void destroy_RHI();