#include "graphics/rhi/vulkan/vulkan.h"
#include "graphics/rhi/vulkan/material.h"
#include "graphics/rhi/vulkan/shader.h"
#include "graphics/rhi/vulkan/texture.h"
#include "graphics/rhi/vulkan/buffer.h"
#include "graphics/rhi/vulkan/shader_access.h"
#include "graphics/assets/assets.h"
#include <stdio.h>

//todo this can be moved out of RHI
//doesn't depend directly on anything vulkan related

//todo move most of the code into helper functions
//as the material system isn't the most materials general representation of shader access
//Move many small buffers into one large one
//avoid doing many map and unmaps for performance reasons
//

MaterialAllocator::MaterialAllocator(VkDevice device, VkPhysicalDevice physical_device, DescriptorPool& descriptor_pool)
: device(device), physical_device(physical_device), descriptor_pool(descriptor_pool) {}

void MaterialAllocator::init() {
	SamplerDesc sampler_desc;
	sampler_desc.min_filter = Filter::Linear;
	sampler_desc.mag_filter = Filter::Linear;
	sampler_desc.mip_mode = Filter::Linear;
	sampler_desc.wrap_u = Wrap::Repeat;
	sampler_desc.wrap_v = Wrap::Repeat;

	default_sampler = query_Sampler(sampler_desc);
}

MaterialAllocator::~MaterialAllocator() {

}

Stage from_vk_stage(VkShaderStageFlags flags) {
	Stage stage{};
	if (flags & VK_SHADER_STAGE_VERTEX_BIT) stage |= VERTEX_STAGE;
	if (flags & VK_SHADER_STAGE_FRAGMENT_BIT) stage |= FRAGMENT_STAGE;
	return stage;
}

void MaterialAllocator::make(MaterialDesc& desc, Material* material) {	
	material->info = { desc.shader, desc.draw_state };

	shader_flags permutations[] = { SHADER_INSTANCED };

	//todo allow multiple shader permutations
	//bool initialized = material->set.id != INVALID_HANDLE;

	material->index = (material->index + 1) % MAX_FRAMES_IN_FLIGHT;

	for (int perm = 0; perm < 1; perm++) {
		ShaderModules* module = get_shader_config(desc.shader, permutations[perm]);

		if (module->info.sets.length <= 2) {
			bool contains_ubo_fields = false;
			if (desc.params.length == 0) continue;

			ShaderInfo* info = shader_info(desc.shader);
			fprintf(stderr, "Shader %s %s, does not support this material setup!\n", info->vfilename, info->ffilename);
			abort();
		}

		bool requires_ubo = false;
		for (ParamDesc& param : desc.params) {
			if (param.type != Param_Image) {
				requires_ubo = true;
				break;
			}
		}

		DescriptorSetInfo& material_bindings = module->info.sets[MATERIAL_SET];
		UBOInfo* ubo_info = requires_ubo ? &material_bindings.ubos[0] : nullptr;

		DescriptorDesc descriptor_desc;
		UBOBuffer& ubo_buffer = material->ubos[material->index];
		if (requires_ubo && material->sets[material->index].id == INVALID_HANDLE) {
			ubo_buffer = alloc_ubo_buffer(ubo_info->size, UBO_MAP_UNMAP);
			add_ubo(descriptor_desc, from_vk_stage(material_bindings.bindings[ubo_info->id].stage), ubo_buffer, 0);
		}

		char ubo_memory[256] = {};

		uint sampler_index = 0;
		uint uniform_index = 0;
			
		for (ParamDesc& param : desc.params) {
			bool is_ubo_field = param.type != Param_Image;
			bool is_cubemap = param.type == Param_Cubemap;
			bool is_channel = param.type == Param_Channel1 || param.type == Param_Channel2 || param.type == Param_Channel3 || param.type == Param_Channel4 || is_cubemap;
			bool is_sampler = param.type == Param_Image || is_channel || is_cubemap;
			
			if (is_sampler) {
				SamplerInfo& sampler_info = material_bindings.samplers[sampler_index++];
				DescriptorBindingInfo& binding_info = material_bindings.bindings[sampler_info.id];
					
				if (binding_info.name.length() != 0 && binding_info.name != param.name) {
					fprintf(stderr, "Material Param Description matters.\n");
					fprintf(stderr, "Description specifies %s, but shader declares %s\n", binding_info.name.data, param.name);
					abort();
				}

				uint handle = param.image;
				if (handle == INVALID_HANDLE) handle = default_textures.white.id;

				//todo make it platform agnostic type
				Stage stage = from_vk_stage(binding_info.stage);

				if (is_cubemap) {
					cubemap_handle cubemap{ handle };
					add_combined_sampler(descriptor_desc, stage, default_sampler, cubemap, binding_info.binding);
				}
				else {
					texture_handle texture{ handle };
					add_combined_sampler(descriptor_desc, stage, default_sampler, texture, binding_info.binding);
				}
			}

			if (is_ubo_field) {
				if (!ubo_info) {
					fprintf(stderr, "Material Description and Shader Layout do not match!");
					abort();
				}

				UBOFieldInfo& ubo_field_info = ubo_info->fields[uniform_index++];
					
				char* field_ptr = (char*)ubo_memory + ubo_field_info.offset;

				char name[100];
				if (is_channel) snprintf(name, 100, "%s_scalar", param.name.data);
				else snprintf(name, 100, "%s", param.name.data);

				printf("UBO FIELD %s, param %s %i\n", ubo_field_info.name.data, name, ubo_field_info.offset);

				if (ubo_field_info.name != name) {
					fprintf(stderr, "Material Description and Shader Layout do not match!");
					abort();
				}

				//todo check that is the correct channel type!!!

				if (param.type == Param_Int) *(int*)field_ptr = param.integer;
				if (param.type == Param_Float || param.type == Param_Channel1) *(float*)field_ptr = param.real;
				if (param.type == Param_Vec2 || param.type == Param_Channel2) *(glm::vec2*)field_ptr = param.vec2;
				if (param.type == Param_Vec3 || param.type == Param_Channel3 || param.type == Param_Cubemap) *(glm::vec3*)field_ptr = param.vec3;
				if (param.type == Param_Vec3 || param.type == Param_Channel4 || param.type == Param_Cubemap) *(glm::vec4*)field_ptr = param.vec4;
			}
		}

		if (material_bindings.samplers.length != sampler_index) {
			fprintf(stderr, "Mismatch in the number of image parameters!\n");
			abort();
		}

		if (ubo_info && ubo_info->fields.length != uniform_index) {
			fprintf(stderr, "Mismatch in the number of ubo fields!\n");
			abort();
		}

		if (ubo_info) memcpy_ubo_buffer(ubo_buffer, ubo_info->size, ubo_memory);

		// TODO FIX BUG, WHERE DESCRIPTOR IS STILL IN USE!
		// ONE POSSIBILITY IS A DESCRIPTOR REUSE SYSTEM, WHERE WHEN
		// THE FRAME INDEX IS RESET THE DESCRIPTOR IS FREED
		// OTHERWISE WE WILL x3 the number of descriptors that we actually need!

		printf("UPDATING DESCRIPTOR #%i\n", material->index);
		update_descriptor_set(material->sets[material->index], descriptor_desc);
		printf("DESCRIPTOR %p\n", get_descriptor_set(material->sets[material->index]));
	}
}

void MaterialAllocator::update(MaterialDesc& from, MaterialDesc& to, Material* material) {
	if (from.shader.id != to.shader.id) {
		memset(material->sets, 0, sizeof(material->sets)); //todo unnecesarilly reallocates UBO Buffer
	}
	
	make(to, material);
}
