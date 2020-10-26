#include "graphics/assets/assets.h"
#include "core/container/hash_map.h"
#include "core/container/sstring.h"
#include "core/container/string_view.h"
#include "core/serializer.h"
#include "core/memory/linear_allocator.h"
#include "graphics/rhi/rhi.h"
#include "engine/handle.h"

#include "graphics/assets/material.h"
#include <stdio.h>
#include <shaderc/shaderc.h>
#include "engine/vfs.h"
#include <thread>
#include <mutex>

#include "graphics/assets/assets_store.h"
#include <vendor/stb_image.h>
#include "graphics/renderer/ibl.h"

#include "core/io/logger.h"
#include "core/profiler.h"

Assets assets;
DefaultTextures default_textures;
DefaultMaterials default_materials;

void make_AssetManager(string_view path) {
	assets.asset_path = path;

	init_primitives();
	make_cubemap_pass_resources(assets.cubemap_pass_resources);

	default_textures.white = { 1 };
	default_textures.black = { 2 };
	default_textures.normal = { 3 };
	default_textures.checker = { 4 };

	load_Texture(default_textures.white, "engine/white.png");
	load_Texture(default_textures.black, "engine/black.png");
	load_Texture(default_textures.normal, "engine/normal.jpg");
	load_Texture(default_textures.checker, "engine/checker.jpg");

	global_shaders.pbr = { 1 }; 
	global_shaders.tree = { 2 };
	global_shaders.gizmo = { 3 }; 
	global_shaders.grass = { 4 };

	load_Shader(global_shaders.pbr, "shaders/pbr.vert", "shaders/pbr.frag");
	load_Shader(global_shaders.tree, "shaders/tree.vert", "shaders/tree.frag");
	load_Shader(global_shaders.gizmo, "shaders/gizmo.vert", "shaders/gizmo.frag");
	load_Shader(global_shaders.grass, "shaders/grass.vert", "shaders/tree.frag");

	default_materials.missing = { 1 };
	{
		MaterialDesc desc{ global_shaders.pbr };

		mat_channel3(desc, "diffuse", glm::vec3(1.0), default_textures.checker);
		mat_channel1(desc, "metallic", 0.0f);
		mat_channel1(desc, "roughness", 0.5f);
		mat_channel1(desc, "normal", 1.0, default_textures.normal);
		mat_vec2(desc, "tiling", glm::vec3(1.0));

		make_Material(default_materials.missing, desc);
	}
}

void destroy_AssetManager() {}

string_buffer tasset_path(string_view filename) {
	string_buffer buffer;
	buffer.allocator = &get_temporary_allocator();
	buffer.length = assets.asset_path.length + filename.length;
	buffer.capacity = buffer.length;
	buffer.data = TEMPORARY_ARRAY(char, buffer.capacity + 1);

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

array<2, shader_flags> default_permutations = { SHADER_INSTANCED, SHADER_INSTANCED | SHADER_DEPTH_ONLY };

shader_handle load_Shader(string_view vfilename, string_view ffilename) {
	return load_Shader(vfilename, ffilename, default_permutations);
}

Shader* get_Shader(shader_handle handle) {
	return assets.shaders.get(handle);
}

bool load_Shader(Shader& shader, string_buffer& err) {
	//std::scoped_lock scoped_lock(shader.mutex);
	
	string_view vfilename = shader.info.vfilename;
	string_view ffilename = shader.info.ffilename;

	string_buffer vert_source, frag_source;

	if (!io_readf(vfilename, &vert_source)) {
		err = "Could not read vertex shader file";
		return false;
	}

	if (!io_readf(ffilename, &frag_source)) {
		err = "Could not read fragment shader file!";
		return false;
	}

	shader.info.vfilename = vfilename;
	shader.info.ffilename = ffilename;
	shader.info.v_time_modified = io_time_modified(vfilename);
	shader.info.f_time_modified = io_time_modified(ffilename);
	shader.configs.resize(shader.config_flags.length);

	log("Loading shader ", vfilename, "with ", shader.config_flags.length, "permutations\n");

	for (uint i = 0; i < shader.config_flags.length; i++) {
		shader_flags flags = shader.config_flags[i];

		//todo use just the end of the filename
		string_buffer vert_cache_file = tformat("shaders/cache/", flags, vfilename.sub(strlen("shaders/"), vfilename.length), ".spirv");
		string_buffer frag_cache_file = tformat("shaders/cache/", flags, ffilename.sub(strlen("shaders/"), ffilename.length), ".spirv");

		i64 vert_time_cached = io_time_modified(vert_cache_file);
		i64 frag_time_cached = io_time_modified(frag_cache_file);

		string_buffer spirv_vert, spirv_frag;

		if (vert_time_cached != -1 && vert_time_cached > shader.info.v_time_modified) {
			if (!io_readf(vert_cache_file, &spirv_vert)) {
				err = "Failed to read cache file";
				return false;
			}
		}
		else {
			log("Recompiling vertex shader\n");
			
			spirv_vert = compile_glsl_to_spirv(rhi.shader_compiler, VERTEX_STAGE, vert_source, vfilename, flags, &err);
			if (spirv_vert.length > 0) io_writef(vert_cache_file, spirv_vert);
		}

		if (err.length > 0) {
			fprintf(stderr, "Failed to compile shader %s", err.data);
			return false;
		}

		if (frag_time_cached != -1 && frag_time_cached > shader.info.f_time_modified) {
			if (!io_readf(frag_cache_file, &spirv_frag)) {
				err = "Failed to read cache file";
				return false;
			}
		}
		else {
			log("Recompiling fragment shader\n");

			spirv_frag = compile_glsl_to_spirv(rhi.shader_compiler, FRAGMENT_STAGE, frag_source, ffilename, flags, &err);
			if (spirv_frag.length > 0) io_writef(frag_cache_file, spirv_frag);
		}

		if (err.length > 0) {
			fprintf(stderr, "Failed to compile shader %s", err.data);
			return false;
		}

		ShaderModules modules = {};
		modules.vert = make_ShaderModule(spirv_vert);
		modules.frag = make_ShaderModule(spirv_frag);

		reflect_module(modules.info, spirv_vert, spirv_frag);
		gen_descriptor_layouts(modules);

		if (shader.configs[i].vert) vkDestroyShaderModule(rhi.device, shader.configs[i].vert, nullptr);
		if (shader.configs[i].frag)	vkDestroyShaderModule(rhi.device, shader.configs[i].frag, nullptr);
		
		shader.configs[i] = modules; 

		log("Compiled config %i\n", flags);
	}

	log("Loaded all configs for shader : ", vfilename, " ", ffilename, "\n");
	return true;
}

void load_Shader(Shader& shader) {
	string_buffer err;
	if (!load_Shader(shader, err)) throw "Could not compile shader";
}

ShaderModules* get_shader_config(shader_handle handle, shader_flags flags) {
	Shader* shader = assets.shaders.get(handle);

	for (int i = 0; i < shader->config_flags.length; i++) {
		if (shader->config_flags[i] == flags) return &shader->configs[i];
	}

	return NULL;
}

void load_Shader(shader_handle handle, string_view vfilename, string_view ffilename) {
	Shader shader;
	shader.info.vfilename = vfilename;
	shader.info.ffilename = ffilename;
	shader.config_flags = (slice<shader_flags>)default_permutations;

	string_buffer merged = tformat(vfilename.sub(8, vfilename.length), ffilename.sub(8, ffilename.length));

	assets.path_to_handle.set(merged.view(), handle.id);

	load_Shader(shader);
	assets.shaders.assign_handle(handle, std::move(shader));

}

shader_handle load_Shader(string_view vfilename, string_view ffilename, slice<shader_flags> permutations) {
	//assert(vfilename.starts_with("shaders/"));
	//assert(ffilename.starts_with("shaders/"));
	
	string_buffer merged = tformat(vfilename.sub(8, vfilename.length), ffilename.sub(8, ffilename.length));

	int index = assets.path_to_handle.keys.index(merged.view());
	if (index != -1) return { assets.path_to_handle.values[index] }; //todo this assumes the shader permutations are the same! They may not be!

	Shader shader;
	shader.info.vfilename = vfilename;
	shader.info.ffilename = ffilename;
	shader.config_flags = permutations;
	load_Shader(shader);

	return assets.shaders.assign_handle(std::move(shader));
}

bool reload_Shader(shader_handle handle) {
	Profile profile("Compile shader");
	Shader* shader = assets.shaders.get(handle);

	//todo simplify load_Shader could set configs in place

	string_buffer err;
	if (!load_Shader(*shader, err)) {
		return false;
	}

	profile.end();

	log("recompiled shader: ", shader->info.vfilename, " ", shader->info.ffilename);

	Profile reload_pipeline("Reload pipeline");

	PipelineCache& cache = rhi.pipeline_cache;

	for (uint i = 0; i < MAX_PIPELINE; i++) {
		if (!cache.keys.is_full(i)) continue;

		PipelineDesc& desc = cache.keys.keys[i];
		if (desc.shader.id != handle.id) continue;

		reload_Pipeline(desc);
		queue_t_for_destruction<VkPipeline>(cache.pipelines[i], [](VkPipeline pipeline) {
			vkDestroyPipeline(rhi.device, pipeline, nullptr);
		});
	}

	return true;
}

bool reload_modified_shaders() {
	Profile profile("Reload modified shaders");
	
	auto& shaders = assets.shaders;
	bool modified = false;

	for (uint i = 0; i < shaders.slots.length; i++) {	
		ShaderInfo& info = shaders.slots[i].info;
		
		i64 v_time_modified = io_time_modified(info.vfilename);
		i64 f_time_modified = io_time_modified(info.ffilename);
	
		if (v_time_modified > info.v_time_modified || f_time_modified > info.f_time_modified) {
			shader_handle handle = shaders.index_to_handle(i);
			reload_Shader(handle);
			modified = true;
		}
	}

	return modified;
}


void load_Model(model_handle handle, string_view path, const glm::mat4& matrix, slice<float> lod_distance) {
	Model model;
	model.lod_distance = lod_distance;

	string_buffer full_path = tasset_path(path);

	load_assimp(&model, full_path, matrix);
	assets.models.assign_handle(handle, std::move(model));
}

VertexBuffer get_vertex_buffer(model_handle model_handle, uint mesh_index, uint lod) {
	return assets.models.get(model_handle)->meshes[mesh_index].buffer[lod];
}

model_handle load_Model(string_view path, bool serialized, const glm::mat4& trans) {
	if (uint* cached = assets.path_to_handle.get(path)) return { *cached };

	int index = assets.path_to_handle.keys.index(path);
	if (index != -1) return { assets.path_to_handle.values[index] };

	Model model;
	string_buffer full_path = tasset_path(path);

	load_assimp(&model, full_path, trans);

	model_handle model_handle = assets.models.assign_handle(std::move(model), serialized);
	assets.path_to_handle.set(path, model_handle.id);
	return model_handle;
}

Model* get_Model(model_handle handle) {
	return assets.models.get(handle);
}


texture_handle alloc_Texture(const TextureDesc& desc) {
	Texture tex = alloc_TextureImage(rhi.texture_allocator, desc);
	return assets.textures.assign_handle(std::move(tex), false);
}

texture_handle upload_Texture(const Image& image, bool serialized) {
	Texture tex = make_TextureImage(rhi.texture_allocator, image);
	return assets.textures.assign_handle(std::move(tex), serialized);
}

Image load_Image(string_view filename, bool reverse) {
	string_buffer real_filename = tasset_path(filename); 

	stbi_set_flip_vertically_on_load(reverse);

	Image image;

	image.data = stbi_load(real_filename.c_str(), &image.width, &image.height, &image.num_channels, STBI_rgb_alpha); //todo might waste space
	image.num_channels = 4;
	image.format = TextureFormat::UNORM;
	//image.usage = TextureUsage::Sampled | TextureUsage::TransferDst;

	if (!image.data) throw "Could not load texture";

	return image;
}

void free_Image(Image& image) {
	stbi_image_free(image.data);
}

void load_Texture(texture_handle handle, string_view path) {
	Image image = load_Image(path);
	assets.textures.assign_handle(handle, make_TextureImage(rhi.texture_allocator, image));
	assets.path_to_handle.set(path, handle.id);
	free_Image(image);
}

texture_handle load_Texture(string_view path, bool serialized) {
	if (uint* cached = assets.path_to_handle.get(path)) return { *cached };

	printf("LOADING TEXTURE %s\n", path.c_str());
	
	Image image = load_Image(path);
	texture_handle handle = upload_Texture(image, serialized);
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
	for (TextureLoadJob job : batch) {
		load_Texture(job.handle, job.path);
	}

	/*
	string_buffer buffer;
	buffer.allocator = &get_temporary_allocator();
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

material_handle make_Material(MaterialDesc& desc, bool serialized) {
	Material material;
	rhi.material_allocator.make(desc, &material);
	return assets.materials.assign_handle(std::move(material), serialized);
}

void make_Material(material_handle handle, MaterialDesc& desc) {
	Material material;
	rhi.material_allocator.make(desc, &material);
	assets.materials.assign_handle(handle, std::move(material));
}

Material* get_Material(material_handle handle) {
	return assets.materials.get(handle);
}

void update_Material(material_handle handle, MaterialDesc& from, MaterialDesc& to) {
	Material* mat = assets.materials.get(handle);

	rhi.material_allocator.update(from, to, mat);
}

MaterialPipelineInfo material_pipeline_info(material_handle handle) {
	Material* mat = assets.materials.get(handle);
	if (mat) return mat->info;
	return {};
}

pipeline_handle query_pipeline(material_handle material, RenderPass::ID render_pass, uint subpass) {
	MaterialPipelineInfo info = material_pipeline_info(material);

	PipelineDesc desc = {};
	desc.render_pass = render_pass_by_id(render_pass);
	desc.shader = info.shader;
	desc.vertex_layout = VERTEX_LAYOUT_DEFAULT;
	desc.instance_layout = INSTANCE_LAYOUT_MAT4X4;
	desc.shader_flags = render_pass_type_by_id(render_pass, subpass) == RenderPass::Color ? SHADER_INSTANCED : SHADER_INSTANCED | SHADER_DEPTH_ONLY;
	desc.state = info.state;
	desc.subpass = subpass;

	return query_Pipeline(desc);
}

void reload_modified() {

}

//cubemaps are somewhat of a special case as they require 
//processing on the graphics queue which is somewhat awkward

void load_assets_in_queue() {
	return;
	begin_gpu_upload();

	EquirectangularToCubemapJob job;
	while (assets.equirectangular_to_cubemap_jobs.dequeue(&job)) {
		Cubemap* cubemap = assets.cubemaps.get(job.handle);

		*cubemap = convert_equirectangular_to_cubemap(assets.cubemap_pass_resources, job.env_map);

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