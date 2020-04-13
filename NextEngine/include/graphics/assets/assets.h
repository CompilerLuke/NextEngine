#pragma once

#include "core/core.h"
#include "core/container/string_buffer.h"

using shader_flags = u64;

struct RHI;
struct Assets;
struct Level;
struct BufferAllocator;
struct Renderer;
struct Model;
struct Texture;
	
struct TextureLoadJob {
	texture_handle handle;
	sstring path;
};

Assets* make_Assets(RHI&, string_view path);
void destroy_Assets(Assets*);

ENGINE_API model_handle load_Model(Assets& assets, string_view filename, bool serialized = false, const glm::mat4& matrix = glm::mat4(1.0));
ENGINE_API Model* get_Model(Assets&, model_handle model);
void ENGINE_API load_Model(Assets& assets, model_handle handle, string_view filename, const glm::mat4& matrix);
void ENGINE_API load_Model(Assets& assets, model_handle handle, const glm::mat4& matrix = glm::mat4(1.0));

ENGINE_API shader_handle load_SinglePass_Shader(Assets& assets, string_view vfilename, string_view ffilename);
ENGINE_API shader_handle load_Shader(Assets& assets, string_view vfilename, string_view ffilename);
ENGINE_API shader_handle load_Shader(Assets& assets, string_view vfilename, string_view ffilename, slice<shader_flags> permutations);

ENGINE_API texture_handle load_Texture(Assets& assets, string_view filename, bool serialized = false);
ENGINE_API Texture* get_Texture(Assets& assets, texture_handle handle);
void ENGINE_API load_TextureBatch(Assets& assets, slice<TextureLoadJob> batch);

ENGINE_API string_buffer tasset_path(Assets& assets, string_view filename);
ENGINE_API string_view current_asset_path_folder(Assets& assets);
bool ENGINE_API asset_path_rel(Assets& assets, string_view filename, string_buffer*);

void ENGINE_API load_Scene(Assets& assets, World& world, Renderer& renderer, string_view path);