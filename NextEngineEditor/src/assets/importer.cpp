#include "assets/importer.h"
#include "assets/explorer.h"
#include <graphics/assets/assets.h>
#include <graphics/assets/model.h>
#include <graphics/rhi/rhi.h>

sstring name_from_filename(string_view filename) {
	int after_slash = filename.find_last_of('\\') + 1;
	return filename.sub(after_slash, filename.find_last_of('.'));
}

model_handle import_model(AssetTab& self, string_view filename) {
	model_handle handle = load_Model(filename, true);
	Model* model = get_Model(handle);

	AssetNode asset(AssetNode::Model);
	asset.model.handle = handle;
	asset.model.path = filename;
	asset.model.lod_distance = (slice<float>)model->lod_distance;

	//todo model without uvs will crash the importer
	if (model->materials.length > 8) {
		fprintf(stderr, "Maximum materials per model is 8");
		return { INVALID_HANDLE };
	}

	for (sstring& material : model->materials) {
		asset.model.materials.append(self.info.default_material);
	}

	asset.model.name = name_from_filename(filename);
	add_asset_to_current_folder(self, std::move(asset));

	return handle;
}

void import_texture(Editor& editor, AssetTab& self, string_view filename) {
	texture_handle handle = load_Texture(filename, true);

	AssetNode asset(AssetNode::Texture);
	asset.texture.handle = handle;
	asset.texture.name = name_from_filename(filename);
	asset.texture.path = filename;

	add_asset_to_current_folder(self, std::move(asset));
}

void import_shader(Editor& editor, AssetTab& self, string_view filename) {
	return; //
	/*
	string_buffer frag_filename(filename.sub(0, filename.length - strlen(".vert")));
	frag_filename += ".frag";

	shader_handle handle = self.assets.shaders.load(filename, frag_filename);
	ID id = self.assets.make_ID();
	ShaderAsset* shader_asset = self.assets.make<ShaderAsset>(id);

	add_asset(self, id);
	*/
}

//todo revise this function
void import_filename(Editor& editor, World& world, RenderPass& ctx, AssetTab& self, string_view filename) {
	string_buffer asset_path;
	if (!asset_path_rel(filename, &asset_path)) {
		asset_path = filename.sub(filename.find_last_of('\\') + 1, filename.size());

		string_buffer new_filename = tasset_path(asset_path);
		if (!CopyFileA(filename.c_str(), new_filename.c_str(), true)) {
			log("Could not copy filename");
		}
	}

	log(asset_path);

	if (asset_path.ends_with(".fbx") || asset_path.ends_with(".FBX")) {
		begin_gpu_upload();
		import_model(self, asset_path);
		end_gpu_upload();
	}
	else if (asset_path.ends_with(".jpg") || asset_path.ends_with(".png")) {
		begin_gpu_upload();
		import_texture(editor, self, asset_path);
		end_gpu_upload();
	}
	else if (asset_path.ends_with(".vert") || asset_path.ends_with(".frag")) {
		import_shader(editor, self, asset_path);
	}
	else {
		log("Unsupported extension for", asset_path);
	}
}
