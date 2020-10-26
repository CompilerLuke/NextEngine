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