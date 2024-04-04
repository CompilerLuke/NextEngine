#pragma once

#include "node.h"

const uint MAX_ASSETS = 1000;
const uint MAX_PER_ASSET_TYPE = 1024;

struct AssetInfo {
	array<MAX_ASSETS, asset_handle> free_ids = 0;
	AssetNode* asset_handle_to_node[MAX_ASSETS];
	AssetNode* asset_type_handle_to_node[AssetNode::Count][MAX_ASSETS];
	AssetNode toplevel;
	material_handle default_material;
};

struct AssetTab;
struct AssetPreviewResources;

void register_asset_ptr(AssetInfo& tab, AssetNode* node);
AssetNode* get_asset(AssetInfo& tab, asset_handle handle);
void add_to_folder(AssetInfo& self, asset_handle folder_handle, AssetNode&& asset);
void free_asset(AssetPreviewResources& resources, AssetNode& asset);
void remove_from_folder(AssetInfo& self, asset_handle folder_handle, AssetNode* asset);
asset_handle make_asset_handle(AssetInfo& info);
asset_handle add_asset_to_current_folder(AssetTab& tab, AssetNode&& asset);
void move_to_folder(AssetInfo& self, asset_handle src_folder, asset_handle dst_folder, AssetNode&& asset);
void accept_move_to_folder(AssetInfo& tab, AssetNode** ptr);
void make_AssetInfo(AssetInfo& info);
