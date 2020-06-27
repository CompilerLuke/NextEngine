#pragma once

#include "core/core.h"
#include "core/reflection.h"

constexpr uint INVALID_HANDLE = 0;

struct model_handle {
	uint id = INVALID_HANDLE;
	REFLECT(ENGINE_API)
};

struct texture_handle {
	uint id = INVALID_HANDLE;
	REFLECT(ENGINE_API)
};

struct shader_handle {
	uint id = INVALID_HANDLE;
	REFLECT(ENGINE_API)
};

struct cubemap_handle {
	uint id = INVALID_HANDLE;
	REFLECT(ENGINE_API)
};

struct material_handle {
	uint id = INVALID_HANDLE;
	REFLECT(ENGINE_API)
};

struct env_probe_handle {
	uint id = INVALID_HANDLE;
	REFLECT(ENGINE_API)
};

struct shader_config_handle {
	uint id = INVALID_HANDLE;
	REFLECT(ENGINE_API)
};

struct uniform_handle {
	uint id = INVALID_HANDLE;
	REFLECT(ENGINE_API)
};

struct pipeline_handle {
	uint id = INVALID_HANDLE;
	REFLECT(ENGINE_API)
};

struct pipeline_layout_handle {
	u64 id = INVALID_HANDLE;
};

struct descriptor_set_handle {
	uint id = INVALID_HANDLE;
};


struct render_pass_handle {
	u64 id = INVALID_HANDLE;
};

struct command_buffer_handle {
	u64 id = INVALID_HANDLE;
};

struct frame_buffer_handle {
	u64 id = INVALID_HANDLE;
};