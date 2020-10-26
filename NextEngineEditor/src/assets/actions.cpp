#include "assets/explorer.h"
#include "assets/inspect.h"
#include "graphics/rhi/pipeline.h"
#include "graphics/assets/assets.h"

material_handle make_new_material(AssetTab& self, Editor& editor) {
	MaterialDesc desc = base_shader_desc(load_Shader("shaders/pbr.vert", "shaders/pbr.frag"));
	material_handle handle = make_Material(desc, true);

	AssetNode asset(AssetNode::Material);
	asset.material.desc = desc;
	asset.material.handle = handle;
	asset.material.name = "Empty";

	add_asset_to_current_folder(self, std::move(asset));

	return handle;
}
