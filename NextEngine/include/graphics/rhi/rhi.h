#pragma once

#include "core/core.h"

struct RHI;

struct AppInfo {
	const char* app_name;
	const char* engine_name;
};

struct DeviceFeatures {
	bool sampler_anistropy = true;
	bool multi_draw_indirect = true;
};

ENGINE_API RHI* make_RHI(AppInfo& info, DeviceFeatures&);
void ENGINE_API begin_gpu_upload(RHI& rhi);
void ENGINE_API end_gpu_upload(RHI& rhi);
//void ENGINE_API destroy_RHI(RHI* rhi);