#include "assets/info.h"
#include "assets/node.h"
#include "assets/explorer.h"
#include "assets/preview.h"
#include <imgui/imgui.h>


//todo make reflection generate call to constructor
AssetNode::AssetNode() {}

AssetNode::AssetNode(Type type) {
	memset(this, 0, sizeof(AssetNode));
	this->type = type;

	if (type == Shader) shader.shader_arguments.allocator = &default_allocator;
	if (type == Model) {
		model.trans.scale = glm::vec3(1.0);
	}
	if (type == Folder) folder.contents.allocator = &default_allocator;
}

/*
AssetNode::AssetNode(const AssetNode& other) {
    switch (other.type) {
    case Folder:
        type = other.type;
        handle = other.handle;
        
        folder = other.folder;
        break;
        
    default: memcpy(this, &other, sizeof(AssetNode));
    }
}*/

AssetNode::AssetNode(AssetNode&& other) {
	memcpy(this, &other, sizeof(AssetNode));
	memset(&other, 0, sizeof(AssetNode));
}

void AssetNode::operator=(AssetNode&& other) {
	memcpy(this, &other, sizeof(AssetNode));
	memset(&other, 0, sizeof(AssetNode));
}

AssetNode::~AssetNode() {
	if (type == Shader) shader.shader_arguments.~vector();
	if (type == Folder) folder.contents.~vector();
}

void register_asset_ptr(AssetInfo& tab, AssetNode* node) {
	tab.asset_handle_to_node[node->handle.id] = node;

	if (node->type != AssetNode::Folder) tab.asset_type_handle_to_node[node->type][node->asset.handle] = node;
}

AssetNode* get_asset(AssetInfo& tab, asset_handle handle) {
	return tab.asset_handle_to_node[handle.id];
}

void add_to_folder(AssetInfo& self, asset_handle folder_handle, AssetNode&& asset) {
	AssetFolder& folder = get_asset(self, folder_handle)->folder;

	//array resize might  invalidating ptrs
	folder.contents.append(std::move(asset));

	for (AssetNode& asset : folder.contents) {
		register_asset_ptr(self, &asset);
	}
}

#include "shaderGraph.h" //make this not required

void free_asset(AssetPreviewResources& resources, AssetNode& asset) {
	if (asset.type == AssetNode::Material) free_atlas_slot(resources, asset.material.rot_preview);
	if (asset.type == AssetNode::Model)  free_atlas_slot(resources, asset.model.rot_preview);
	if (asset.type == AssetNode::Shader)  free_atlas_slot(resources, asset.shader.graph->rot_preview);
}

void remove_from_folder(AssetInfo& self, asset_handle folder_handle, AssetNode* asset) {
	//todo assert for folder
	AssetFolder& folder = get_asset(self, folder_handle)->folder;

	uint index = asset - folder.contents.data;
	assert(index < folder.contents.length);

	for (uint i = index; i < folder.contents.length - 1; i++) {
		folder.contents[i] = std::move(folder.contents[i + 1]);
		register_asset_ptr(self, folder.contents.data + i);
	}

	folder.contents.length--;
}

asset_handle make_asset_handle(AssetInfo& info) {
	return info.free_ids.pop();
}

asset_handle add_asset_to_current_folder(AssetTab& tab, AssetNode&& asset) {
	asset_handle handle = make_asset_handle(tab.info);
	asset.handle = handle;
	tab.preview_resources.render_preview_for.append(handle);

	if (asset.type == AssetNode::Material) asset.material.rot_preview.preview_tex_coord = tab.preview_resources.free_atlas_slot.pop();
	if (asset.type == AssetNode::Model)  asset.model.rot_preview.preview_tex_coord = tab.preview_resources.free_atlas_slot.pop();

	add_to_folder(tab.info, tab.explorer.current_folder, std::move(asset));
	return handle;
}

void move_to_folder(AssetInfo& self, asset_handle src_folder, asset_handle dst_folder, AssetNode&& asset) {
	AssetNode* asset_folder_ptr = &asset;
	add_to_folder(self, dst_folder, std::move(asset));
	remove_from_folder(self, src_folder, asset_folder_ptr);
}

void accept_move_to_folder(AssetInfo& tab, AssetNode** ptr) {
	for (uint type = 0; type < AssetNode::Type::Count; type++) {
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(drop_types[type])) {
			uint id = *(uint*)payload->Data;
			*ptr = tab.asset_type_handle_to_node[type][id];
		}
	}
}

void make_AssetInfo(AssetInfo& info) {
	for (uint i = MAX_ASSETS - 1; i > 0; i--) {
		info.free_ids.append({ i });
	}

	info.toplevel = AssetNode(AssetNode::Folder);
	info.toplevel.handle = make_asset_handle(info);
	info.toplevel.folder.name = "Assets";
	register_asset_ptr(info, &info.toplevel);

	printf("Size of %i", sizeof(ModelAsset));
}
