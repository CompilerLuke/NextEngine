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
ENGINE_API void begin_gpu_upload();
ENGINE_API void end_gpu_upload();
ENGINE_API void queue_for_destruction(void*, void(*)(void*)); //may be worth using std::function instead
ENGINE_API uint get_frame_index();

template<typename T>
void queue_t_for_destruction(T data, void(*func)(T)) {
	queue_for_destruction(data, (void(*)(void*))func);
}

void destroy_RHI();
