#pragma once

#include "core/core.h"

struct AsyncCopyResources;
struct CommandBuffer;

AsyncCopyResources* make_async_copy_resources(uint size);
void destroy_async_copy_resources(AsyncCopyResources& resources);
void async_copy_image(CommandBuffer& cmd_buffer, texture_handle image_handle, AsyncCopyResources& copy);
void receive_transfer(AsyncCopyResources& resources, uint size, void* dst);

