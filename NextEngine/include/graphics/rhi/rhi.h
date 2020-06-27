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

void destroy_RHI();