#include "stdafx.h"
#include "graphics/assets/assets.h"
#include "core/container/hash_map.h"
#include "core/container/sstring.h"
#include "core/serializer.h"
#include "core/memory/linear_allocator.h"
#include "graphics/rhi/rhi.h"
#include "graphics/assets/material.h"
#include <stdio.h>
#include <shaderc/shaderc.h>
#include "core/io/vfs.h"
#include <thread>
#include <mutex>

#include "graphics/assets/shader.h"
#include "graphics/assets/model.h"
#include "graphics/assets/texture.h"

#ifdef RENDER_API_VULKAN
#include "graphics/rhi/vulkan/rhi.h"
#include "graphics/rhi/vulkan/shader.h"
#include "graphics/rhi/vulkan/material.h"
#include "graphics/rhi/vulkan/texture.h"
#endif

struct Assets {
	RHI& rhi;
	shaderc_compiler_t shader_compiler;

	string_buffer asset_path;
	hash_map<sstring, uint, 1000> path_to_handle;

	HandleManager<Model, model_handle> models;
	HandleManager<Shader, shader_handle> shaders;
	HandleManager<Texture, texture_handle> textures;
	HandleManager<Cubemap, cubemap_handle> cubemaps;
	HandleManager<Material, material_handle> materials;
};

Assets* make_Assets(RHI& rhi, string_view path) {
	Assets* assets = PERMANENT_ALLOC(Assets, { rhi });
	assets->asset_path = path;
	assets->shader_compiler = shaderc_compiler_initialize();
	return assets;
}

void destroy_Assets(Assets* assets) {
	shaderc_compiler_release(assets->shader_compiler);
}

string_buffer tasset_path(Assets& assets, string_view filename) {
	string_buffer buffer;
	buffer.allocator = &temporary_allocator;
	buffer.capacity = assets.asset_path.length + filename.length;
	buffer.length = buffer.capacity;
	buffer.data = TEMPORARY_ARRAY(char, buffer.capacity);

	memcpy(buffer.data, assets.asset_path.data, assets.asset_path.length);
	memcpy(buffer.data + assets.asset_path.length, filename.data, filename.length);
	buffer.data[assets.asset_path.length + filename.length] = '\0';
	
	return buffer;
}

string_view current_asset_path_folder(Assets& assets) {
	return assets.asset_path;
}

bool asset_path_rel(Assets& assets, string_view filename, string_buffer* result) {
	if (filename.starts_with(assets.asset_path)) {
		*result = filename.sub(assets.asset_path.size(), filename.size());
		return true;
	}

	return false;
}

ShaderInfo* shader_info(Assets& assets, shader_handle handle) {
	Shader* shader = assets.shaders.get(handle);
	if (shader) return &shader->info;
	return NULL;
}

shader_handle load_SinglePass_Shader(Assets& assets, string_view vfilename, string_view ffilename) {
	shader_flags default_permutations[] = { 0 };
	return load_Shader(assets, vfilename, ffilename, { default_permutations, 1 });
}

shader_handle load_Shader(Assets& assets, string_view vfilename, string_view ffilename) {
	shader_flags default_permutations[] = { SHADER_INSTANCED, SHADER_INSTANCED | SHADER_DEPTH_ONLY };
	return load_Shader(assets, vfilename, ffilename, { default_permutations, 2});
}


void load_Shader(Assets& assets, Shader& shader, slice<shader_flags> permutations) {
	shaderc_compiler_t compiler = assets.shader_compiler;
	string_view vfilename = shader.info.vfilename;
	string_view ffilename = shader.info.ffilename;

	string_buffer vert_source, frag_source;

	if (!readf(assets, vfilename, &vert_source)) throw "Could not read vertex shader file";
	if (!readf(assets, ffilename, &frag_source)) throw "Could not read fragment shader file!";

	shader.info.vfilename = vfilename;
	shader.info.ffilename = ffilename;
	shader.info.v_time_modified = time_modified(assets, vfilename);
	shader.info.f_time_modified = time_modified(assets, ffilename);

	VkDevice device = get_Device(assets.rhi);

	string_buffer err;

	for (shader_flags flags : permutations) {
		//todo use just the end of the filename
		string_buffer vert_cache_file = tformat("shaders/cache/", flags, vfilename.sub(strlen("shaders/"), vfilename.length), ".spirv");
		string_buffer frag_cache_file = tformat("shaders/cache/", flags, ffilename.sub(strlen("shaders/"), ffilename.length), ".spirv");

		i64 vert_time_cached = time_modified(assets, vert_cache_file);
		i64 frag_time_cached = time_modified(assets, frag_cache_file);

		string_buffer spirv_vert, spirv_frag;

		if (vert_time_cached != -1 && vert_time_cached > shader.info.v_time_modified) {
			if (!readf(assets, vert_cache_file, &spirv_vert)) throw "Failed to read cache file";
		}
		else {
			spirv_vert = compile_glsl_to_spirv(assets, compiler, VERTEX_STAGE, vert_source, vfilename, flags, &err);
			if (spirv_vert.length > 0) writef(assets, vert_cache_file, spirv_vert);
		}

		if (frag_time_cached != -1 && frag_time_cached > shader.info.f_time_modified) {
			if (!readf(assets, frag_cache_file, &spirv_frag)) throw "Failed to read cache file";
		}
		else {
			spirv_frag = compile_glsl_to_spirv(assets, compiler, FRAGMENT_STAGE, frag_source, ffilename, flags, &err);
			if (spirv_frag.length > 0) writef(assets, frag_cache_file, spirv_frag);
		}

		if (err.length > 0) {
			fprintf(stderr, "Failed to compile shader %s", err.data);
			throw "Could not compile shader to spirv!";
		}

		ShaderModules modules = {};
		modules.vert = make_ShaderModule(device, spirv_vert);
		modules.frag = make_ShaderModule(device, spirv_frag);

		reflect_module(modules.info, spirv_vert, spirv_frag);

		shader.configs.append(modules);
		shader.config_flags.append(flags);
	}

	log("loaded shader : ", vfilename, " ", ffilename, "\n");
}

ShaderModules* get_shader_config(Assets& assets, shader_handle handle, shader_flags flags) {
	Shader* shader = assets.shaders.get(handle);

	for (int i = 0; i < shader->config_flags.length; i++) {
		if (shader->config_flags[i] == flags) return &shader->configs[i];
	}

	return NULL;
}

shader_handle load_Shader(Assets& assets, string_view vfilename, string_view ffilename, slice<shader_flags> permutations) {
	assert(vfilename.starts_with("shaders/"));
	assert(ffilename.starts_with("shaders/"));
	
	string_buffer merged = tformat(vfilename.sub(8, vfilename.length), ffilename.sub(8, ffilename.length));
	shaderc_compiler_t compiler = assets.shader_compiler;

	int index = assets.path_to_handle.keys.index(merged.view());
	if (index != -1) return { assets.path_to_handle.values[index] }; //todo this assumes the shader permutations are the same! They may not be!

	Shader shader;
	shader.info.vfilename = vfilename;
	shader.info.ffilename = ffilename;
	load_Shader(assets, shader, permutations);

	return assets.shaders.assign_handle(std::move(shader));
}

void reload_Shader(Assets& assets, shader_handle handle) {
	Shader* shader = assets.shaders.get(handle);

	load_Shader(assets, *shader, shader->config_flags);
	log("recompiled shader: ", shader->info.vfilename);
}

void load_Model(Assets& assets, model_handle handle, string_view path, const glm::mat4& matrix) {
	Model model{ path };
	assets.models.assign_handle(handle, std::move(model));
	load_Model(assets, handle, matrix);
}

void load_Model(Assets& assets, model_handle model_handle, const glm::mat4& trans) {
	Model* model = get_Model(assets, model_handle);

	BufferAllocator& buffer_allocator = get_BufferAllocator(assets.rhi);
	string_buffer full_path = tasset_path(assets, model->path);

	load_assimp(model, buffer_allocator, full_path, trans);
}

model_handle load_Model(Assets& assets, string_view path, bool serialized, const glm::mat4& trans) {
	int index = assets.path_to_handle.keys.index(path);
	if (index != -1) return { assets.path_to_handle.values[index] };

	Model model{ path };
	model_handle model_handle = assets.models.assign_handle(std::move(model), serialized);
	assets.path_to_handle.set(path, model_handle.id);
	load_Model(assets, model_handle, trans);
	return model_handle;
}

Model* get_Model(Assets& assets, model_handle handle) {
	return assets.models.get(handle);
}



texture_handle load_Texture(Assets& assets, string_view path, bool serialized) {
	Image image = load_Image(assets, path);
	TextureAllocator& texture_allocator = get_TextureAllocator(assets.rhi);
	
	TextureDesc desc;
	Texture tex = make_TextureImage(texture_allocator, desc, image);

	free_Image(image);

	texture_handle handle = assets.textures.assign_handle(std::move(tex), serialized);
	assets.path_to_handle.set(path, handle.id);

	return handle;
}

void load_TextureBatch(Assets& assets, slice<TextureLoadJob> batch) {
	/*
	string_buffer buffer;
	buffer.allocator = &temporary_allocator;
	DeserializerBuffer serializer;
	constexpr int num_threads = 9;

	std::mutex tasks_lock;
	vector<Texture*> tasks;

	std::mutex result_lock;
	vector<Image> loaded_image;
	vector<Texture*> loaded_image_tex;

	int num_tasks_left = batch.length;

	for (int i = 0; i < batch.length; i++) {
		texture_handle handle = batch[i].handle;
		
		Texture tex;
		tex.filename = batch[i].path;

		//todo assign handle should return pointer
		assets.textures.assign_handle(handle, std::move(tex));
		tasks.append(assets.textures.get(handle));
	}

	std::thread threads[num_threads];

	for (int i = 0; i < num_threads; i++) {
		threads[i] = std::thread([&, i]() {
			while (true) {
				tasks_lock.lock();
				if (tasks.length == 0) {
					tasks_lock.unlock();
					break;
				}

				Texture* tex = tasks.pop();
				tasks_lock.unlock();

				Image image; //load_Image(asset_manager, tex->filename); // assets.textures.load_Image(tex->filename);

				result_lock.lock();
				loaded_image.append(std::move(image));
				loaded_image_tex.append(tex);
				num_tasks_left--;
				result_lock.unlock();
			}
		});
	}


	while (true) {
		result_lock.lock();
		if (loaded_image_tex.length == 0) {
			result_lock.unlock();
			continue;
		}

		Texture* tex = loaded_image_tex.pop();
		Image image = loaded_image.pop();
		result_lock.unlock();

		//tex->texture_id = gl_submit(image);

		if (num_tasks_left == 0 && loaded_image_tex.length == 0) {
			break;
		}
	}

	for (int i = 0; i < num_threads; i++) {
		threads[i].join();
	}*/
}

void load() {

}

u64 underlying_texture(Assets& assets, texture_handle handle) {
	return (u64)assets.textures.get(handle)->image;
}

Texture* get_Texture(Assets& assets, texture_handle handle) {
	return assets.textures.get(handle);
}

material_handle make_Material(Assets& assets, MaterialDesc& desc) {
	Material material = make_material_descriptors(assets.rhi, assets, desc);
	return assets.materials.assign_handle(std::move(material));
}

Material* get_Material(Assets& assets, material_handle handle) {
	return assets.materials.get(handle);
}

void replace_Material(Assets& assets, material_handle handle, MaterialDesc& desc) {
	Material* mat = assets.materials.get(handle);
	if (!mat) {
		Material material;
		material.desc = desc;
		assets.materials.assign_handle(handle, std::move(material));
	}
	else {
		mat->desc = desc;
	}

}

MaterialDesc* material_desc(Assets& assets, material_handle handle) {
	Material* mat = assets.materials.get(handle);
	if (mat) return &mat->desc;
	return NULL;
}

void reload_modified(Assets& assets) {

}

//REFLECT_STRUCT_BEGIN(Model)
//REFLECT_STRUCT_MEMBER(path)
//REFLECT_STRUCT_MEMBER(meshes)
//REFLECT_STRUCT_MEMBER(materials)
//REFLECT_STRUCT_END()

/*
#define MAX_MODELS 100
#define MAX_TEXTURES 100

struct AssetCache {
	hash_map<asset_path, Model, MAX_MODELS> models_path;
	hash_map<asset_path, Texture, MAX_TEXTURES> textures_path;
};

AssetCache cache;

Model& get_model(model_handle handle) {
	assert(cache.models_path.keys.is_full(handle.id));
	return cache.models_path.values[handle.id];
}

constexpr int CHUNK_SIZE = 1024 * 1024;

void load_asset_chunk(DeserializerBuffer& chunk) {
	int model_count = chunk.read_int();

	int vertices_length = chunk.read_int();
	int indices_length = chunk.read_int();

	Vertex* vertices = (Vertex*)chunk.pointer_to_n_bytes(sizeof(Vertex) * vertices_length);
	uint* indices = (uint*)chunk.pointer_to_n_bytes(sizeof(uint) * indices_length);

	VertexBuffer vertex_buffer = RHI::alloc_vertex_buffer(VERTEX_LAYOUT_DEFAULT, vertices_length, vertices, indices_length, indices);

	for (int i = 0; i < model_count; i++) {
		uint handle = chunk.read_int();

		Model& model = cache.models_path.values[handle];
		model.path = *(asset_path*)chunk.pointer_to_n_bytes(sizeof(asset_path));
		model.meshes.length = chunk.read_int();
		model.meshes.data = (Mesh*)chunk.pointer_to_n_bytes(sizeof(Mesh) * model.meshes.length);
		model.vertices.length = chunk.read_int();
		model.vertices.data = (Vertex*)((char*)chunk.data + chunk.read_int());
		model.indices.length = chunk.read_int();
		model.indices.data = (uint*)((char*)chunk.data + chunk.read_int());
		model.aabb = *(AABB*)chunk.pointer_to_n_bytes(sizeof(AABB));

		for (Mesh& mesh : model.meshes) {
			mesh.buffer = vertex_buffer;
			mesh.vertices_offset += mesh.vertices_offset;
			mesh.indices_offset += mesh.indices_offset;
		}
	}
}

model_handle load_Model(string_view path, bool serialized) { //TODO ADD MORE SUPPORT IN HASH_MAP FOR WHAT WE ARE DOING
	int index = cache.models_path.keys.index(path.c_str());
	if (index != -1) return { index };

	//todo add support for string_view

	cache.models_path.keys.add(path.c_str());

	load({ index }, glm::mat4(1.0));

	return { index };
}

*/