#pragma once

#include "engine/handle.h"
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
struct Shader;
	
struct TextureLoadJob {
	texture_handle handle;
	sstring path;
};

struct DefaultTextures {
	texture_handle white;
	texture_handle black;
	texture_handle normal;
	texture_handle checker;
};

struct DefaultMaterials {
	material_handle missing;
};

extern ENGINE_API DefaultTextures default_textures;
extern ENGINE_API DefaultMaterials default_materials;

void make_AssetManager(string_view path, string_view engine_path);
void destroy_AssetManager();

ENGINE_API void load_assets_in_queue();

ENGINE_API model_handle load_Model(string_view filename, bool serialized = false, const glm::mat4& matrix = glm::mat4(1.0));
ENGINE_API Model* get_Model(model_handle model);
void ENGINE_API load_Model(model_handle handle, string_view filename, const glm::mat4& matrix, slice<float> lod_distance = {});

ENGINE_API shader_handle load_SinglePass_Shader(string_view vfilename, string_view ffilename);
ENGINE_API shader_handle load_Shader(string_view vfilename, string_view ffilename);
ENGINE_API shader_handle load_Shader(string_view vfilename, string_view ffilename, slice<shader_flags> permutations);
ENGINE_API void load_Shader(shader_handle, string_view vfilename, string_view ffilename);
ENGINE_API void load_Shader(Shader&);
ENGINE_API bool load_Shader(Shader&, string_buffer&);
ENGINE_API Shader* get_Shader(shader_handle);
ENGINE_API bool reload_Shader(shader_handle);
ENGINE_API bool reload_modified_shaders();

ENGINE_API texture_handle load_Texture(string_view filename, bool serialized = false);
ENGINE_API void load_Texture(texture_handle handle, string_view filename);
ENGINE_API cubemap_handle load_HDR(string_view filename);
ENGINE_API cubemap_handle load_Cubemap(string_view filename);
ENGINE_API Texture* get_Texture(texture_handle handle);
ENGINE_API Cubemap* get_Cubemap(cubemap_handle handle);
void ENGINE_API load_TextureBatch(slice<TextureLoadJob> batch);

ENGINE_API string_buffer tasset_path(string_view filename);
ENGINE_API string_view current_asset_path_folder();
bool ENGINE_API asset_path_rel(string_view filename, string_buffer*);

void ENGINE_API load_Scene(struct World& world, struct Renderer& renderer, string_view path);
