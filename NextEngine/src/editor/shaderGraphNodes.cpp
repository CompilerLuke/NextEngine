#include "stdafx.h"
#include "editor/shaderGraph.h"
#include "logger/logger.h"
#include "graphics/texture.h"

//Defaults
ShaderNode make_pbr_node() {
	ShaderNode node(ShaderNode::PBR_NODE, ShaderNode::ChannelNone);
	node.inputs.append(ShaderNode::InputChannel("diffuse", ShaderNode::Channel3));
	node.inputs.append(ShaderNode::InputChannel("metallic", ShaderNode::Channel1));
	node.inputs.append(ShaderNode::InputChannel("roughness", ShaderNode::Channel1));
	node.inputs.append(ShaderNode::InputChannel("emission", glm::vec3(0)));
	node.inputs.append(ShaderNode::InputChannel("normal", glm::vec3(0.5, 0.5, 1.0f)));
	node.inputs.append(ShaderNode::InputChannel("alpha", 1.0f));
	node.inputs.append(ShaderNode::InputChannel("alpha_clip", 0.0f));

	return node;
}

ShaderNode make_math_node() {
	ShaderNode node(ShaderNode::MATH_NODE, ShaderNode::Channel1);
	node.inputs.append(ShaderNode::InputChannel("a", ShaderNode::Channel1));
	node.inputs.append(ShaderNode::InputChannel("b", ShaderNode::Channel1));
	node.math_op = ShaderNode::Add;

	return node;
}

ShaderNode make_clamp_node() {
	ShaderNode node(ShaderNode::CLAMP_NODE, ShaderNode::Channel1);
	node.inputs.append(ShaderNode::InputChannel("value", ShaderNode::Channel1));
	node.inputs.append(ShaderNode::InputChannel("low", ShaderNode::Channel1));
	node.inputs.append(ShaderNode::InputChannel("high", 1.0f));

	return node;
}

ShaderNode make_noise_node() {
	ShaderNode node(ShaderNode::NOISE_NODE, ShaderNode::Channel1);
	node.inputs.append(ShaderNode::InputChannel("uvs", ShaderNode::Channel2));

	return node;
}

ShaderNode make_remap_node() {
	ShaderNode node(ShaderNode::REMAP_NODE, ShaderNode::Channel1);
	node.inputs.append(ShaderNode::InputChannel("value", ShaderNode::Channel1));
	node.inputs.append(ShaderNode::InputChannel("low1", 0.0f));
	node.inputs.append(ShaderNode::InputChannel("high1", 1.0f));
	node.inputs.append(ShaderNode::InputChannel("low2", -1.0f));
	node.inputs.append(ShaderNode::InputChannel("high2", 1.0f));

	return node;
}

ShaderNode make_step_node() {
	ShaderNode node(ShaderNode::STEP_NODE, ShaderNode::Channel1);
	node.inputs.append(ShaderNode::InputChannel("a", 0.0f));
	node.inputs.append(ShaderNode::InputChannel("b", 0.5f));

	return node;
}

ShaderNode make_vec_node() {
	ShaderNode node(ShaderNode::VEC_NODE, ShaderNode::Channel2);
	node.inputs.append(ShaderNode::InputChannel("a", ShaderNode::Channel1));
	node.inputs.append(ShaderNode::InputChannel("b", ShaderNode::Channel1));

	return node;
}

ShaderNode make_blend_node() {
	ShaderNode node(ShaderNode::BLEND_NODE, ShaderNode::Channel1);
	node.inputs.append(ShaderNode::InputChannel("a", ShaderNode::Channel1));
	node.inputs.append(ShaderNode::InputChannel("b", ShaderNode::Channel1));
	node.inputs.append(ShaderNode::InputChannel("factor", ShaderNode::Channel1));

	return node;
}

ShaderNode make_time_node() {
	return ShaderNode(ShaderNode::TIME_NODE, ShaderNode::Channel1);
}

ShaderNode make_tex_coords_node() {
	ShaderNode node(ShaderNode::TEX_COORDS, ShaderNode::Channel2);
	node.type = ShaderNode::TEX_COORDS;
	node.inputs.append(ShaderNode::InputChannel("scale", glm::vec2(1)));
	node.inputs.append(ShaderNode::InputChannel("offset", glm::vec2(0)));

	return node;
}


ShaderNode make_texture_node() {
	ShaderNode node(ShaderNode::TEXTURE_NODE, ShaderNode::Channel4);
	node.inputs.append(ShaderNode::InputChannel("uvs", ShaderNode::Channel2));
	node.tex = { INVALID_HANDLE };

	return node;
}

void render_inputs(ShaderGraph& graph, Handle<ShaderNode> handle, unsigned int num, unsigned int offset = 0) {
	for (unsigned int i = 0; i < num; i++) {
		render_input(graph, handle, i + offset);
	}
}

void render_title(ShaderGraph& graph, const char* title) {
	ImGui::Text(title);
	ImGui::Dummy(ImVec2(0, 10 * graph.scale));
}

//Rendering
float override_width_of_node(ShaderNode* self) {
	if (self->type == ShaderNode::TIME_NODE) return 200.0f;
	if (self->type == ShaderNode::TEXTURE_NODE || self->type == ShaderNode::PBR_NODE) return 400.0f;
	
	return 300.0f;
}

void render_pbr_node(ShaderGraph& graph, ShaderNode* self, Handle<ShaderNode> handle) {
	render_title(graph, "PBR");
	render_output(graph, handle);
	render_inputs(graph, handle, 4);
	render_input(graph, handle, 4, false);
	render_inputs(graph, handle, 2, 5);

	ImGui::Image((ImTextureID)texture::id_of(graph.rot_preview.preview), ImVec2(380 * graph.scale, 380 * graph.scale), ImVec2(0, 1), ImVec2(1, 0));
}

void render_remap_node(ShaderGraph& graph, ShaderNode* self, Handle<ShaderNode> handle) {
	render_title(graph, "Remap");
	render_output(graph, handle);
	render_inputs(graph, handle, 5);
}

void render_noise_node(ShaderGraph& graph, ShaderNode* self, Handle<ShaderNode> handle) {
	render_title(graph, "Noise");
	render_output(graph, handle);
	render_input(graph, handle, 0, false);
}

void render_time_node(ShaderGraph& graph, ShaderNode* self, Handle<ShaderNode> handle) {
	render_title(graph, "Time");
	render_output(graph, handle);
}

void render_step_node(ShaderGraph& graph, ShaderNode* self, Handle<ShaderNode> handle) {
	render_title(graph, "Step");
	render_output(graph, handle);
	render_inputs(graph, handle, 2);
}

void node_select_type(ShaderGraph& graph, ShaderNode* self, ShaderNode::ChannelType typ) {
	ShaderNode* a = graph.nodes_manager.get(self->inputs[0].link.from);
	ShaderNode* b = graph.nodes_manager.get(self->inputs[1].link.from);

	if (a && b) {
		typ = (ShaderNode::ChannelType) glm::max((int)a->output.type, (int)b->output.type);
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
	render_inputs(graph, handle, 2);
	node_select_type(graph, self, ShaderNode::Channel1);
}

void render_vec_node(ShaderGraph& graph, ShaderNode* self, Handle<ShaderNode> handle) {
	if (ImGui::Button(tformat("Vec", (int)self->output.type + 1, "##", handle.id).c_str())) {
		ImGui::OpenPopup(tformat("OpenVec", handle.id).c_str());
	}

	ImGui::Dummy(ImVec2(0, 6.5f * graph.scale));

	if (ImGui::BeginPopup(tformat("OpenVec", handle.id).c_str())) {
		for (unsigned int i = 0; i < 4; i++) {
			if (ImGui::Button(tformat("Vec", i + 1).c_str())) {
				self->output.type = (ShaderNode::ChannelType)i;
			}
		}

		ImGui::EndPopup();
	}

	render_output(graph, handle);
	render_inputs(graph, handle, 2);

	if (self->inputs[0].type > self->output.type) self->inputs[0].type = self->output.type;
	self->inputs[1].type = (ShaderNode::ChannelType)(self->output.type - self->inputs[0].type - 1);
}

void render_clamp_node(ShaderGraph& graph, ShaderNode* self, Handle<ShaderNode> handle) {
	render_title(graph, "Clamp");
	render_output(graph, handle);
	render_inputs(graph, handle, 3);
}

void render_blend_node(ShaderGraph& graph, ShaderNode* node, Handle<ShaderNode> handle) {
	render_title(graph, "Blend");
	render_output(graph, handle);
	render_inputs(graph, handle, 3);
	node_select_type(graph, node, ShaderNode::Channel3);
}

void render_texture_node(ShaderGraph& graph, ShaderNode* self, Handle<ShaderNode> handle) {
	render_title(graph, "Texture");
	render_output(graph, handle);

	render_input(graph, handle, 0, false);

	ImGui::Image((ImTextureID)texture::id_of(self->tex), ImVec2(380 * graph.scale, 380 * graph.scale), ImVec2(0, 1), ImVec2(1, 0));
	accept_drop("DRAG_AND_DROP_IMAGE", &self->tex, sizeof(Handle<Texture>));
}

void render_tex_coords(ShaderGraph& graph, ShaderNode* self, Handle<ShaderNode> handle) {
	render_title(graph, "Tex Coords");
	render_output(graph, handle);
	render_inputs(graph, handle, 2);
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
}

//Spawning
void spawn_node(ShaderGraph& graph, glm::vec2 position, ShaderNode&& node) {
	node.position = position;
	graph.nodes_manager.make(std::move(node));
}

void new_node_popup(ShaderAsset* asset) {
	glm::vec2 mouse_rel = screenspace_to_position(*asset->graph, glm::vec2(ImGui::GetMousePos().x, ImGui::GetMousePos().y));

	if (ImGui::BeginPopup("NewNode")) {
		if (ImGui::Button("New Texture Node")) spawn_node(*asset->graph, mouse_rel, make_texture_node());
		if (ImGui::Button("New PBR Node")) spawn_node(*asset->graph, mouse_rel, make_pbr_node());
		if (ImGui::Button("New Tex Coords Node")) spawn_node(*asset->graph, mouse_rel, make_tex_coords_node());
		if (ImGui::Button("New Math Node")) spawn_node(*asset->graph, mouse_rel, make_math_node());
		if (ImGui::Button("New Blend Node")) spawn_node(*asset->graph, mouse_rel, make_blend_node());
		if (ImGui::Button("New Time Node")) spawn_node(*asset->graph, mouse_rel, make_time_node());
		if (ImGui::Button("New Clamp Node")) spawn_node(*asset->graph, mouse_rel, make_clamp_node());
		if (ImGui::Button("New Vec Node")) spawn_node(*asset->graph, mouse_rel, make_vec_node());
		if (ImGui::Button("New Remap Node")) spawn_node(*asset->graph, mouse_rel, make_remap_node());
		if (ImGui::Button("New Step Node")) spawn_node(*asset->graph, mouse_rel, make_step_node());
		if (ImGui::Button("New Noise Node")) spawn_node(*asset->graph, mouse_rel, make_noise_node());

		ImGui::EndPopup();
	}
}

//Compiling
void ShaderCompiler::find_dependencies() {
	graph.dependencies.clear();

	for (int i = 0; i < graph.nodes_manager.slots.length; i++) {
		ShaderNode* node = graph.nodes_manager.get(graph.nodes_manager.index_to_handle(i));

		if (node->type == ShaderNode::TEXTURE_NODE) {
			Param param;
			param.type = Param_Image;
			param.image = node->tex;
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
		if (node->output.type == ShaderNode::Channel4) compile_input(node->inputs[0]);
		if(node->output.type == ShaderNode::Channel3) compile_func(this, node, "vec3", 2);
		if(node->output.type == ShaderNode::Channel2) compile_func(this, node, "vec2", 2);
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
		compile_func(this, node, "remap", 5);
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
		contents += get_dependency(handle);
		contents += ", ";

		if (!node->inputs[0].link.from.id) contents += "TexCoords";
		else compile_input(node->inputs[0]);

		contents += ")";
	}
}