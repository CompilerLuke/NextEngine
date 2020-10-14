#pragma once

#include "core/core.h"

constexpr uint INVALID_HANDLE = 0;

REFL struct model_handle {
	uint id = INVALID_HANDLE;
};

REFL struct texture_handle {
	uint id = INVALID_HANDLE;
};

REFL struct shader_handle {
	uint id = INVALID_HANDLE;
};

REFL struct cubemap_handle {
	uint id = INVALID_HANDLE;
};

REFL struct material_handle {
	uint id = INVALID_HANDLE;
};

REFL struct env_probe_handle {
	uint id = INVALID_HANDLE;
};

REFL struct shader_config_handle {
	uint id = INVALID_HANDLE;
};

REFL struct uniform_handle {
	uint id = INVALID_HANDLE;
};

REFL struct pipeline_handle {
	uint id = INVALID_HANDLE;
};

REFL struct pipeline_layout_handle {
	u64 id = INVALID_HANDLE;
};

REFL struct descriptor_set_handle {
	uint id = INVALID_HANDLE;
};


REFL struct render_pass_handle {
	u64 id = INVALID_HANDLE;
};

REFL struct command_buffer_handle {
	u64 id = INVALID_HANDLE;
};

REFL struct frame_buffer_handle {
	u64 id = INVALID_HANDLE;
};