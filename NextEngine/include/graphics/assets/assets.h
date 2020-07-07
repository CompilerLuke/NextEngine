#pragma once

#include "core/handle.h"
#include "core/container/string_view.h"
#include "core/container/sstring.h"
#include "core/container/slice.h"
#include "core/container/string_buffer.h"
#include <glm/glm.hpp>

using shader_flags = u64;

struct RHI;
struct Assets;
struct Level;
struct VertexStreaming;
struct Renderer;
struct Model;
struct Texture;
struct Cubemap;
	
struct TextureLoadJob {
	texture_handle handle;
	sstring path;
};


void make_AssetManager(string_view path);
void destroy_AssetManager();

ENGINE_API void load_assets_in_queue();

ENGINE_API model_handle load_Model(string_view filename, bool serialized = false, const glm::mat4& matrix = glm::mat4(1.0));
ENGINE_API Model* get_Model(model_handle model);
void ENGINE_API load_Model(model_handle handle, string_view filename, const glm::mat4& matrix);
void ENGINE_API load_Model(model_handle handle, const glm::mat4& matrix = glm::mat4(1.0));

ENGINE_API shader_handle load_SinglePass_Shader(string_view vfilename, string_view ffilename);
ENGINE_API shader_handle load_Shader(string_view vfilename, string_view ffilename);
ENGINE_API shader_handle load_Shader(string_view vfilename, string_view ffilename, slice<shader_flags> permutations);

ENGINE_API texture_handle load_Texture(string_view filename, bool serialized = false);
ENGINE_API cubemap_handle load_HDR(string_view filename);
ENGINE_API cubemap_handle load_Cubemap(string_view filename);
ENGINE_API Texture* get_Texture(texture_handle handle);
ENGINE_API Cubemap* get_Cubemap(cubemap_handle handle);
void ENGINE_API load_TextureBatch(slice<TextureLoadJob> batch);

ENGINE_API string_buffer tasset_path(string_view filename);
ENGINE_API string_view current_asset_path_folder();
bool ENGINE_API asset_path_rel(string_view filename, string_buffer*);

void ENGINE_API load_Scene(struct World& world, struct Renderer& renderer, string_view path);