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

struct shader_config_handle {
	uint id = INVALID_HANDLE;
	REFLECT(ENGINE_API)
};

struct uniform_handle {
	uint id = INVALID_HANDLE;
	REFLECT(ENGINE_API)
};