#include "stdafx.h"
#include "editor/shaderGraph.h"
#include "logger/logger.h"
#include "graphics/texture.h"
#include "editor/editor.h"

vector<ShaderNode> node_defaults;

void init_node_info(ShaderNode::Type type) {
	if (type == ShaderNode::PBR_NODE) {

	}
}

const char* pbr_inputs[7] = { "diffuse", "metallic", "roughness", "emission", "normal", "alpha", "alpha_clip" };

//Defaults
ShaderNode make_pbr_node() {
	ShaderNode node(ShaderNode::PBR_NODE, ChannelNone);
	node.inputs.append(ShaderNode::InputChannel("diffuse", Channel3));
	node.inputs.append(ShaderNode::InputChannel("metallic", Channel1));
	node.inputs.append(ShaderNode::InputChannel("roughness", Channel1));
	node.inputs.append(ShaderNode::InputChannel("emission", glm::vec3(0)));
	node.inputs.append(ShaderNode::InputChannel("normal", glm::vec3(0.5, 0.5, 1.0f)));
	node.inputs.append(ShaderNode::InputChannel("alpha", 1.0f));
	node.inputs.append(ShaderNode::InputChannel("alpha_clip", 0.0f));

	return node;
}

const char* math_inputs[2] = { "a", "b" };

ShaderNode make_math_node() {
	ShaderNode node(ShaderNode::MATH_NODE, Channel1);
	node.inputs.append(ShaderNode::InputChannel("a", Channel1));
	node.inputs.append(ShaderNode::InputChannel("b", Channel1));
	node.math_op = ShaderNode::Add;

	return node;
}

const char* clamp_inputs[3] = { "value", "low", "hight" };

ShaderNode make_clamp_node() {
	ShaderNode node(ShaderNode::CLAMP_NODE, Channel1);
	node.inputs.append(ShaderNode::InputChannel("value", Channel1));
	node.inputs.append(ShaderNode::InputChannel("low", Channel1));
	node.inputs.append(ShaderNode::InputChannel("high", 1.0f));

	return node;
}

const char* noise_inputs[1] = { "uvs" };

ShaderNode make_noise_node() {
	ShaderNode node(ShaderNode::NOISE_NODE, Channel1);
	node.inputs.append(ShaderNode::InputChannel("uvs", Channel2));

	return node;
}

const char* remap_inputs[3] = { "value", "from", "to" };

ShaderNode make_remap_node() {
	ShaderNode node(ShaderNode::REMAP_NODE, Channel1);
	node.inputs.append(ShaderNode::InputChannel("value", Channel1));
	node.inputs.append(ShaderNode::InputChannel("from", glm::vec2(-1.0, 1.0f)));
	node.inputs.append(ShaderNode::InputChannel("to", glm::vec2(0, 1.0f)));

	return node;
}

const char* step_inputs[2] = { "a", "b" };

ShaderNode make_step_node() {
	ShaderNode node(ShaderNode::STEP_NODE, Channel1);
	node.inputs.append(ShaderNode::InputChannel("a", 0.0f));
	node.inputs.append(ShaderNode::InputChannel("b", 0.5f));

	return node;
}

const char* vec_inputs[3] = { "a", "b" };

ShaderNode make_vec_node() {
	ShaderNode node(ShaderNode::VEC_NODE, Channel2);
	node.inputs.append(ShaderNode::InputChannel("a", Channel1));
	node.inputs.append(ShaderNode::InputChannel("b", Channel1));

	return node;
}

const char* blend_inputs[3] = { "a", "b", "factor" };

ShaderNode make_blend_node() {
	ShaderNode node(ShaderNode::BLEND_NODE, Channel1);
	node.inputs.append(ShaderNode::InputChannel("a", Channel1));
	node.inputs.append(ShaderNode::InputChannel("b", Channel1));
	node.inputs.append(ShaderNode::InputChannel("factor", Channel1));

	return node;
}

ShaderNode make_time_node() {
	return ShaderNode(ShaderNode::TIME_NODE, Channel1);
}

const char* tex_coords_inputs[3] = { "scale", "offset" };

ShaderNode make_tex_coords_node() {
	ShaderNode node(ShaderNode::TEX_COORDS, Channel2);
	node.type = ShaderNode::TEX_COORDS;
	node.inputs.append(ShaderNode::InputChannel("scale", glm::vec2(1)));
	node.inputs.append(ShaderNode::InputChannel("offset", glm::vec2(0)));

	return node;
}

const char* tex_inputs[3] = { "uvs" };

ShaderNode make_texture_node() {
	ShaderNode node(ShaderNode::TEXTURE_NODE, Channel4);
	node.inputs.append(ShaderNode::InputChannel("uvs", Channel2));
	node.tex_node.from_param = false;
	node.tex_node.tex_handle = { INVALID_HANDLE };

	return node;
}

void render_inputs(ShaderGraph& graph, Handle<ShaderNode> handle, unsigned int num, const char** names, unsigned int offset = 0) {
	for (unsigned int i = 0; i < num; i++) {
		render_input(graph, handle, i + offset, names);
	}
}

void render_title(ShaderGraph& graph, StringView title) {
	ImGui::Text(title.c_str());
	ImGui::Dummy(ImVec2(0, 10 * graph.scale));
}

//Rendering
float override_width_of_node(ShaderNode* self) {
	if (self->type == ShaderNode::TIME_NODE || self->type == ShaderNode::PARAM_NODE) return 200.0f;
	if (self->type == ShaderNode::TEXTURE_NODE || self->type == ShaderNode::PBR_NODE) return 400.0f;
	
	return 300.0f;
}

void render_pbr_node(ShaderGraph& graph, ShaderNode* self, Handle<ShaderNode> handle) {
	render_title(graph, "PBR");
	render_output(graph, handle);
	render_inputs(graph, handle, 4, pbr_inputs);
	render_input(graph, handle, 4, pbr_inputs, false);
	render_inputs(graph, handle, 2, pbr_inputs, 5);

	ImGui::Image((ImTextureID)texture::id_of(graph.rot_preview.preview), ImVec2(380 * graph.scale, 380 * graph.scale), ImVec2(0, 1), ImVec2(1, 0));
}

void render_remap_node(ShaderGraph& graph, ShaderNode* self, Handle<ShaderNode> handle) {
	render_title(graph, "Remap");
	render_output(graph, handle);
	render_inputs(graph, handle, 3, remap_inputs);
}

void render_noise_node(ShaderGraph& graph, ShaderNode* self, Handle<ShaderNode> handle) {
	render_title(graph, "Noise");
	render_output(graph, handle);
	render_input(graph, handle, 0, noise_inputs, false);
}

void render_time_node(ShaderGraph& graph, ShaderNode* self, Handle<ShaderNode> handle) {
	render_title(graph, "Time");
	render_output(graph, handle);
}

void render_step_node(ShaderGraph& graph, ShaderNode* self, Handle<ShaderNode> handle) {
	render_title(graph, "Step");
	render_output(graph, handle);
	render_inputs(graph, handle, 2, step_inputs);
}

void node_select_type(ShaderGraph& graph, ShaderNode* self, ChannelType typ) {
	ShaderNode* a = graph.nodes_manager.get(self->inputs[0].link.from);
	ShaderNode* b = graph.nodes_manager.get(self->inputs[1].link.from);

	if (a && b) {
		typ = (ChannelType) glm::max((int)a->output.type, (int)b->output.type);
	}
	else if (a) {
		typ = a->output.type;
	}
	else if (b) {
		typ = b->output.type;
	}

	self->inputs[0].type = typ;
	self->inputs[1].type = typ;
	self->output.type = typ;
}

void render_math_node(ShaderGraph& graph, ShaderNode* self, Handle<ShaderNode> handle) {
	StringView ops[8] = {
		"Add", "Sub", "Mul", "Div", "Sin", "Div", "Sin One", "Cos One"
	};

	if (ImGui::Button(tformat("Math - ", ops[self->math_op], "##", handle.id).c_str())) {
		ImGui::OpenPopup(tformat("SelectMathOp##", handle.id).c_str());
	}

	ImGui::Dummy(ImVec2(0, 6.5f * graph.scale));

	if (ImGui::BeginPopup(tformat("SelectMathOp##", handle.id).c_str())) {
		for (unsigned int i = 0; i < 8; i++) {
			if (ImGui::Button(ops[i].c_str())) {
				self->math_op = (ShaderNode::MathOp)i;
			}
		}

		ImGui::EndPopup();
	}

	render_output(graph, handle);
	render_inputs(graph, handle, 2, math_inputs);
	node_select_type(graph, self, Channel1);
}

void render_vec_node(ShaderGraph& graph, ShaderNode* self, Handle<ShaderNode> handle) {
	if (ImGui::Button(tformat("Vec", (int)self->output.type + 1, "##", handle.id).c_str())) {
		ImGui::OpenPopup(tformat("OpenVec", handle.id).c_str());
	}

	ImGui::Dummy(ImVec2(0, 6.5f * graph.scale));

	if (ImGui::BeginPopup(tformat("OpenVec", handle.id).c_str())) {
		for (unsigned int i = 0; i < 4; i++) {
			if (ImGui::Button(tformat("Vec", i + 1).c_str())) {
				self->output.type = (ChannelType)i;
			}
		}

		ImGui::EndPopup();
	}

	render_output(graph, handle);
	render_inputs(graph, handle, 2, vec_inputs);

	if (self->inputs[0].type > self->output.type) self->inputs[0].type = self->output.type;
	self->inputs[1].type = (ChannelType)(self->output.type - self->inputs[0].type - 1);
}

void render_clamp_node(ShaderGraph& graph, ShaderNode* self, Handle<ShaderNode> handle) {
	render_title(graph, "Clamp");
	render_output(graph, handle);
	render_inputs(graph, handle, 3, clamp_inputs);
}

void render_blend_node(ShaderGraph& graph, ShaderNode* node, Handle<ShaderNode> handle) {
	render_title(graph, "Blend");
	render_output(graph, handle);
	render_inputs(graph, handle, 3, blend_inputs);
	node_select_type(graph, node, Channel3);
}

void render_texture_node(ShaderGraph& graph, ShaderNode* self, Handle<ShaderNode> handle) {
	Handle<Texture> tex_handle;

	if (self->tex_node.from_param) {
		StringBuffer title = tformat("Texture Param ", get_param_name(graph, self->tex_node.param_id));
		render_title(graph, title);
		render_output(graph, handle);

		tex_handle = graph.parameters[self->tex_node.param_id].image;
	}
	else {
		render_title(graph, "Texture");
		render_output(graph, handle);

		tex_handle = self->tex_node.tex_handle;
	}

	render_input(graph, handle, 0, false);
	
	ImGui::Image((ImTextureID)texture::id_of(tex_handle), ImVec2(380 * graph.scale, 380 * graph.scale), ImVec2(0, 1), ImVec2(1, 0));

	if (ImGui::BeginDragDropTarget()) {
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DRAG_AND_DROP_IMAGE")) {
			memcpy(&self->tex_node.tex_handle, payload->Data, sizeof(Handle<Texture>));
			self->tex_node.from_param = false;
		}

		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DRAG_AND_DROP_IMAGE_PARAM")) {
			self->tex_node.param_id = (unsigned int)payload->Data;
			self->tex_node.from_param = true;
		}

		ImGui::EndDragDropTarget();
	}
}

void render_tex_coords(ShaderGraph& graph, ShaderNode* self, Handle<ShaderNode> handle) {
	render_title(graph, "Tex Coords");
	render_output(graph, handle);
	render_inputs(graph, handle, 2, tex_coords_inputs);
}

void render_param_node(ShaderGraph& graph, ShaderNode* self, Handle<ShaderNode> handle) {
	render_title(graph, tformat("Param ", get_param_name(graph, self->param_id)).c_str());
	render_output(graph, handle);
}

void render_node_inner(ShaderGraph& graph, ShaderNode* self, Handle<ShaderNode> handle) {
	if (self->type == ShaderNode::PBR_NODE) render_pbr_node(graph, self, handle);
	if (self->type == ShaderNode::TEXTURE_NODE) render_texture_node(graph, self, handle);
	if (self->type == ShaderNode::TEX_COORDS) render_tex_coords(graph, self, handle);
	if (self->type == ShaderNode::MATH_NODE) render_math_node(graph, self, handle);
	if (self->type == ShaderNode::BLEND_NODE) render_blend_node(graph, self, handle);
	if (self->type == ShaderNode::TIME_NODE) render_time_node(graph, self, handle);
	if (self->type == ShaderNode::CLAMP_NODE) render_clamp_node(graph, self, handle);
	if (self->type == ShaderNode::VEC_NODE) render_vec_node(graph, self, handle);
	if (self->type == ShaderNode::REMAP_NODE) render_remap_node(graph, self, handle);
	if (self->type == ShaderNode::STEP_NODE) render_step_node(graph, self, handle);
	if (self->type == ShaderNode::NOISE_NODE) render_noise_node(graph, self, handle);
	if (self->type == ShaderNode::PARAM_NODE) render_param_node(graph, self, handle);
}

//Spawning
void spawn_node(ShaderGraph& graph, glm::vec2 position, ShaderNode&& node) {
	node.position = position;
	graph.nodes_manager.make(std::move(node));
}

StringBuffer node_popup_filter = "";

void node_option(StringView name, ShaderAsset* asset, glm::vec2 mouse_rel, ShaderNode (make_node)()) {
	if (!name.starts_with(node_popup_filter)) return;
	if (ImGui::Button(tformat("New ", name, " Node").c_str())) spawn_node(*asset->graph, mouse_rel, make_texture_node());
}

void new_node_popup(ShaderAsset* asset) {
	glm::vec2 window_pos = to_vec2(ImGui::GetWindowPos());

	if (ImGui::BeginPopup("NewNode")) {

		glm::vec2 position = to_vec2(ImGui::GetWindowPos()); //todo move back into assetTab
		position /= asset->graph->scale;

		position.x -= window_pos.x + asset->graph->scrolling.x;
		position.y -= window_pos.y + asset->graph->scrolling.y + 50.0f;

		ImGui::InputText("filter", node_popup_filter);

		node_option("Texture", asset, position, make_texture_node);
		node_option("PBR", asset, position, make_pbr_node);
		node_option("Tex Coords", asset, position, make_tex_coords_node);
		node_option("Math", asset, position, make_math_node);
		node_option("Blend", asset, position, make_blend_node);
		node_option("Time", asset, position, make_time_node);
		node_option("Clamp", asset, position, make_clamp_node);
		node_option("Vec", asset, position, make_vec_node);
		node_option("Remap", asset, position, make_remap_node);
		node_option("Step", asset, position, make_step_node);
		node_option("Noise", asset, position, make_noise_node);

		ImGui::EndPopup();
	}
}

//Compiling
void ShaderCompiler::find_dependencies() {
	graph.dependencies.clear();

	for (int i = 0; i < graph.nodes_manager.slots.length; i++) {
		ShaderNode* node = graph.nodes_manager.get(graph.nodes_manager.index_to_handle(i));

		if (node->type == ShaderNode::TEXTURE_NODE) {
			if (node->tex_node.from_param) continue;

			Param param;
			param.type = Param_Image;
			param.image = node->tex_node.tex_handle;
			param.loc = { graph.nodes_manager.index_to_handle(i).id };

			graph.dependencies.append(param);
		}

		if (node->type == ShaderNode::TIME_NODE) graph.requires_time = true;
	}
}

void compile_func(ShaderCompiler* self, ShaderNode* node, const char* name, unsigned int num) {
	self->contents += name;
	self->contents += "(";
	
	for (unsigned int i = 0; i < num; i++) {
		self->compile_input(node->inputs[i]);
		if (i < num - 1) self->contents += ",";
	}
	self->contents += ")";
}

void ShaderCompiler::compile_node(Handle<ShaderNode> handle) {
	ShaderNode* node = graph.nodes_manager.get(handle);

	if (node->type == ShaderNode::PBR_NODE) {
		contents += "if (";
		compile_input(node->inputs[5]);
		contents += "<";
		compile_input(node->inputs[6]);
		contents += ") discard;";

		contents += "\n#ifndef IS_DEPTH_ONLY\n\n";

		contents += "FragColor = vec4(pow("; 
		compile_input(node->inputs[3]);
		contents += ", vec3(2.2)), 1.0) + pbr_frag(pow(";

		compile_input(node->inputs[0]);
		contents += ", vec3(2.2)),\n";

		contents += "calc_normals_from_tangent(";
		compile_input(node->inputs[4]);
		contents += "),\n";

		compile_input(node->inputs[1]);
		contents += ",\n";

		compile_input(node->inputs[2]);
		contents += ",\n";

		contents += "1.0f);\n";
	}

	if (node->type == ShaderNode::VEC_NODE) {
		if (node->output.type == Channel4) compile_input(node->inputs[0]);
		if(node->output.type == Channel3) compile_func(this, node, "vec3", 2);
		if(node->output.type == Channel2) compile_func(this, node, "vec2", 2);
	}

	if (node->type == ShaderNode::TEX_COORDS) {
		compile_func(this, node, "calc_tex_coords", 2);
	}

	if (node->type == ShaderNode::TIME_NODE) {
		contents += "_dep_time";
	}

	if (node->type == ShaderNode::CLAMP_NODE) {
		compile_func(this, node, "clamp", 3);
	}

	if (node->type == ShaderNode::REMAP_NODE) {
		compile_func(this, node, "remap", 3);
	}

	if (node->type == ShaderNode::STEP_NODE) {
		contents += "float(";
		compile_input(node->inputs[0]);
		contents += ">=";
		compile_input(node->inputs[1]);
		contents += ")";
	}

	if (node->type == ShaderNode::NOISE_NODE) {
		contents += "noise(";

		if (!node->inputs[0].link.from.id) contents += "TexCoords";
		else compile_input(node->inputs[0]);

		contents += ")";
	}

	if (node->type == ShaderNode::PARAM_NODE) {
		contents += get_param_name(graph, node->param_id);
	}

	if (node->type == ShaderNode::MATH_NODE) {
		bool is_trig_one = node->math_op == ShaderNode::SinOne || node->math_op == ShaderNode::CosOne;
		if (node->math_op == ShaderNode::Sin || node->math_op == ShaderNode::Cos || is_trig_one) {
			if (node->math_op == ShaderNode::Sin) contents += "sin(";
			if (node->math_op == ShaderNode::SinOne) contents += "(0.5f + 0.5f * sin(";
			if (node->math_op == ShaderNode::Cos) contents += "cos(";
			if (node->math_op == ShaderNode::CosOne) contents += "(0.5f + 0.5f * cos(";

			compile_input(node->inputs[0]);
			contents += " * ";
			compile_input(node->inputs[1]);
			contents += ")";

			if (is_trig_one) contents += ")";
		}
		else {
			contents += "(";

			compile_input(node->inputs[0]);

			if (node->math_op == ShaderNode::Add) contents += "+";
			if (node->math_op == ShaderNode::Sub) contents += "-";
			if (node->math_op == ShaderNode::Mul) contents += "*";
			if (node->math_op == ShaderNode::Div) contents += "/";

			compile_input(node->inputs[1]);

			contents += ")";
		}
	}

	if (node->type == ShaderNode::BLEND_NODE) {
		compile_func(this, node, "mix", 3);
	}

	if (node->type == ShaderNode::TEXTURE_NODE) {
		contents += "texture(";
		if (node->tex_node.from_param) {
			contents += get_param_name(graph, node->tex_node.param_id);
		}
		else {
			contents += get_dependency(handle);
		}
		contents += ", ";

		if (!node->inputs[0].link.from.id) contents += "TexCoords";
		else compile_input(node->inputs[0]);

		contents += ")";
	}
}