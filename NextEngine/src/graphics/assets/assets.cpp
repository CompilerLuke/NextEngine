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

#include "graphics/assets/assets_store.h"
#include <stb_image.h>
#include "graphics/renderer/ibl.h"

#include "core/io/logger.h"

Assets assets;

void make_AssetManager(string_view path) {
	assets.asset_path = path;

	init_primitives();
	make_cubemap_pass_resources(assets.cubemap_pass_resources);
}

void destroy_AssetManager() {}

string_buffer tasset_path(string_view filename) {
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

string_view current_asset_path_folder() {
	return assets.asset_path;
}

bool asset_path_rel(string_view filename, string_buffer* result) {
	if (filename.starts_with(assets.asset_path)) {
		*result = filename.sub(assets.asset_path.size(), filename.size());
		return true;
	}

	return false;
}

ShaderInfo* shader_info(shader_handle handle) {
	Shader* shader = assets.shaders.get(handle);
	if (shader) return &shader->info;
	return NULL;
}

shader_handle load_SinglePass_Shader(string_view vfilename, string_view ffilename) {
	shader_flags default_permutations[] = { 0 };
	return load_Shader(vfilename, ffilename, { default_permutations, 1 });
}

shader_handle load_Shader(string_view vfilename, string_view ffilename) {
	shader_flags default_permutations[] = { SHADER_INSTANCED, SHADER_INSTANCED | SHADER_DEPTH_ONLY };
	return load_Shader(vfilename, ffilename, { default_permutations, 2});
}


void load_Shader(Shader& shader, slice<shader_flags> permutations) {
	string_view vfilename = shader.info.vfilename;
	string_view ffilename = shader.info.ffilename;

	string_buffer vert_source, frag_source;

	if (!io_readf(vfilename, &vert_source)) throw "Could not read vertex shader file";
	if (!io_readf(ffilename, &frag_source)) throw "Could not read fragment shader file!";

	shader.info.vfilename = vfilename;
	shader.info.ffilename = ffilename;
	shader.info.v_time_modified = io_time_modified(vfilename);
	shader.info.f_time_modified = io_time_modified(ffilename);

	string_buffer err;

	for (shader_flags flags : permutations) {
		//todo use just the end of the filename
		string_buffer vert_cache_file = tformat("shaders/cache/", flags, vfilename.sub(strlen("shaders/"), vfilename.length), ".spirv");
		string_buffer frag_cache_file = tformat("shaders/cache/", flags, ffilename.sub(strlen("shaders/"), ffilename.length), ".spirv");

		i64 vert_time_cached = io_time_modified(vert_cache_file);
		i64 frag_time_cached = io_time_modified(frag_cache_file);

		string_buffer spirv_vert, spirv_frag;

		if (vert_time_cached != -1 && vert_time_cached > shader.info.v_time_modified) {
			if (!io_readf(vert_cache_file, &spirv_vert)) throw "Failed to read cache file";
		}
		else {
			spirv_vert = compile_glsl_to_spirv(rhi.shader_compiler, VERTEX_STAGE, vert_source, vfilename, flags, &err);
			if (spirv_vert.length > 0) io_writef(vert_cache_file, spirv_vert);
		}

		if (frag_time_cached != -1 && frag_time_cached > shader.info.f_time_modified) {
			if (!io_readf(frag_cache_file, &spirv_frag)) throw "Failed to read cache file";
		}
		else {
			spirv_frag = compile_glsl_to_spirv(rhi.shader_compiler, FRAGMENT_STAGE, frag_source, ffilename, flags, &err);
			if (spirv_frag.length > 0) io_writef(frag_cache_file, spirv_frag);
		}

		if (err.length > 0) {
			fprintf(stderr, "Failed to compile shader %s", err.data);
			throw "Could not compile shader to spirv!";
		}

		ShaderModules modules = {};
		modules.vert = make_ShaderModule(spirv_vert);
		modules.frag = make_ShaderModule(spirv_frag);

		reflect_module(modules.info, spirv_vert, spirv_frag);
		gen_descriptor_layouts(modules);

		shader.configs.append(modules);
		shader.config_flags.append(flags);
	}

	log("loaded shader : ", vfilename, " ", ffilename, "\n");
}

ShaderModules* get_shader_config(shader_handle handle, shader_flags flags) {
	Shader* shader = assets.shaders.get(handle);

	for (int i = 0; i < shader->config_flags.length; i++) {
		if (shader->config_flags[i] == flags) return &shader->configs[i];
	}

	return NULL;
}

shader_handle load_Shader(string_view vfilename, string_view ffilename, slice<shader_flags> permutations) {
	assert(vfilename.starts_with("shaders/"));
	assert(ffilename.starts_with("shaders/"));
	
	string_buffer merged = tformat(vfilename.sub(8, vfilename.length), ffilename.sub(8, ffilename.length));

	int index = assets.path_to_handle.keys.index(merged.view());
	if (index != -1) return { assets.path_to_handle.values[index] }; //todo this assumes the shader permutations are the same! They may not be!

	Shader shader;
	shader.info.vfilename = vfilename;
	shader.info.ffilename = ffilename;
	load_Shader(shader, permutations);

	return assets.shaders.assign_handle(std::move(shader));
}

void reload_Shader(shader_handle handle) {
	Shader* shader = assets.shaders.get(handle);

	load_Shader(*shader, shader->config_flags);
	log("recompiled shader: ", shader->info.vfilename);
}

void load_Model(model_handle handle, string_view path, const glm::mat4& matrix) {
	Model model{ path };
	assets.models.assign_handle(handle, std::move(model));
	load_Model(handle, matrix);
}

void load_Model(model_handle model_handle, const glm::mat4& trans) {
	Model* model = get_Model(model_handle);

	string_buffer full_path = tasset_path(model->path);

	load_assimp(model, full_path, trans);
}

VertexBuffer get_VertexBuffer(model_handle model_handle, uint mesh_index) {
	return assets.models.get(model_handle)->meshes[mesh_index].buffer;
}

model_handle load_Model(string_view path, bool serialized, const glm::mat4& trans) {
	if (uint* cached = assets.path_to_handle.get(path)) return { *cached };

	int index = assets.path_to_handle.keys.index(path);
	if (index != -1) return { assets.path_to_handle.values[index] };

	Model model{ path };
	model_handle model_handle = assets.models.assign_handle(std::move(model), serialized);
	assets.path_to_handle.set(path, model_handle.id);
	load_Model(model_handle, trans);
	return model_handle;
}

Model* get_Model(model_handle handle) {
	return assets.models.get(handle);
}


texture_handle alloc_Texture(const TextureDesc& desc) {
	Texture tex = alloc_TextureImage(rhi.texture_allocator, desc);
	return assets.textures.assign_handle(std::move(tex), false);
}

texture_handle upload_Texture(const Image& image) {
	Texture tex = make_TextureImage(rhi.texture_allocator, image);
	return assets.textures.assign_handle(std::move(tex), false);
}

Image load_Image(string_view filename, bool reverse) {
	string_buffer real_filename = tasset_path(filename); 

	stbi_set_flip_vertically_on_load(reverse);

	Image image;

	image.data = stbi_load(real_filename.c_str(), &image.width, &image.height, &image.num_channels, STBI_rgb_alpha); //todo might waste space
	image.num_channels = 4;

	if (!image.data) throw "Could not load texture";

	return image;
}

void free_Image(Image& image) {
	stbi_image_free(image.data);
}

texture_handle load_Texture(string_view path, bool serialized) {
	if (uint* cached = assets.path_to_handle.get(path)) return { *cached };

	printf("LOADING TEXTURE %s\n", path.c_str());
	
	Image image = load_Image(path);
	image.format = TextureFormat::UNORM;
	image.usage = TextureUsage::Sampled | TextureUsage::TransferDst;

	texture_handle handle = upload_Texture(image);
	free_Image(image);

	assets.path_to_handle.set(path, handle.id);

	return handle;
}

void blit_image(CommandBuffer& cmd_buffer, Filter filter, texture_handle src_handle, ImageOffset src_region[2], texture_handle dst_handle, ImageOffset dst_region[2]) {
	Texture* src = get_Texture(src_handle);
	Texture* dst = get_Texture(dst_handle);

	blit_image(cmd_buffer, filter, *src, src_region, *dst, dst_region);
}

//todo could be moved somewhere else
static hash_map<SamplerDesc, Sampler, 13> sampler_cache;

sampler_handle query_Sampler(const SamplerDesc& sampler_desc) {
	int index = sampler_cache.keys.index(sampler_desc);
	if (index == -1) {
		Sampler sampler = make_TextureSampler(sampler_desc);
		index = sampler_cache.set(sampler_desc, sampler);
	}

	return { (uint)(index + 1) };
}

Sampler get_Sampler(sampler_handle sampler_handle) {
	return sampler_cache.values[sampler_handle.id - 1];
}

u64 hash_func(SamplerDesc& sampler_desc) {
	return ((uint)sampler_desc.min_filter << 0)
		| ((uint)sampler_desc.mag_filter << 2)
		| ((uint)sampler_desc.mip_mode << 4)
		| ((uint)sampler_desc.wrap_u << 6)
		| ((uint)sampler_desc.wrap_v << 8)
		| ((uint)sampler_desc.max_anisotropy << 10);
}

void load_TextureBatch(slice<TextureLoadJob> batch) {
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

u64 underlying_texture(texture_handle handle) {
	return (u64)assets.textures.get(handle)->image;
}

Texture* get_Texture(texture_handle handle) {
	return assets.textures.get(handle);
}

Cubemap* get_Cubemap(cubemap_handle handle) {
	return assets.cubemaps.get(handle);
}

material_handle make_Material(MaterialDesc& desc) {
	Material material;
	rhi.material_allocator.make(desc, &material);
	return assets.materials.assign_handle(std::move(material));
}

Material* get_Material(material_handle handle) {
	return assets.materials.get(handle);
}

void replace_Material(material_handle handle, MaterialDesc& desc) {
	Material* mat = assets.materials.get(handle);
	if (!mat) assets.materials.assign_handle(handle, {});

	rhi.material_allocator.make(desc, mat);
}

MaterialDesc* material_desc(material_handle handle) {
	Material* mat = assets.materials.get(handle);
	if (mat) return &mat->desc;
	return NULL;
}

void reload_modified() {

}

//cubemaps are somewhat of a special case as they require 
//processing on the graphics queue which is somewhat awkward

void load_assets_in_queue() {
	return;
	begin_gpu_upload();

	while (EquirectangularToCubemapJob* job = assets.equirectangular_to_cubemap_jobs.try_dequeue()) {
		Cubemap* cubemap = assets.cubemaps.get(job->handle);

		*cubemap = convert_equirectangular_to_cubemap(assets.cubemap_pass_resources, job->env_map);

		printf("LOADING HDR\n");
	}

	end_gpu_upload();
}


cubemap_handle load_HDR(string_view filename) {
	if (uint* cached = assets.path_to_handle.get(filename)) return { *cached };

	string_buffer real_filename = tasset_path(filename);

	
	Image image = {};
	image.data = stbi_loadf(real_filename.c_str(), &image.width, &image.height, &image.num_channels, STBI_rgb_alpha);
	image.num_channels = 4;
	image.format = TextureFormat::HDR;

	assert(image.data);

	Texture texture = make_TextureImage(rhi.texture_allocator, image);
	texture_handle env_map = assets.textures.assign_handle(std::move(texture));

	printf("LOADED ENV MAP %p\n", texture.image);

	stbi_image_free(image.data);
	

	//texture_handle env_map = load_Texture("Wood_2//Stylized_Wood_basecolor.jpg");

	Cubemap cubemap = convert_equirectangular_to_cubemap(assets.cubemap_pass_resources, env_map);

	return assets.cubemaps.assign_handle(std::move(cubemap));
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