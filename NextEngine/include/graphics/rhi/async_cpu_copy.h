#pragma once

#include "engine/handle.h"
#include "core/core.h"
#include "graphics/assets/texture.h"

struct AsyncCopyResources;
struct CommandBuffer;

ENGINE_API AsyncCopyResources* make_async_copy_resources(uint size);
ENGINE_API void destroy_async_copy_resources(AsyncCopyResources& resources);
ENGINE_API void async_copy_image(CommandBuffer& cmd_buffer, texture_handle image_handle, AsyncCopyResources& copy,
                                 TextureAspect aspect);
ENGINE_API bool receive_transfer(AsyncCopyResources& resources, uint size, void* dst);
ENGINE_API void* receive_mapped_transfer(AsyncCopyResources& resources);
