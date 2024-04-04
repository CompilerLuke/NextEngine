#include "assets/explorer.h"
#include "assets/inspect.h"
#include <imgui/imgui.h>
#include "ecs/ecs.h"
#include "editor.h"
#include <imgui/imgui_internal.h>
#include "components/camera.h"
#include "core/io/logger.h"
#include "engine/vfs.h"
#include "engine/input.h"
#include "graphics/assets/assets.h"
#include "graphics/renderer/renderer.h"
#include "core/profiler.h"
#include "core/job_system/job.h"

#undef min

float GRID_SZ = 40.0f;

ShaderNode::ShaderNode(Type type, ChannelType output_type) {
	this->output.type = output_type;
	this->type = type;
}

ShaderNode::ShaderNode() {}

ShaderEditor::ShaderEditor() {}

ShaderNode::InputChannel::InputChannel(string_view name, ChannelType type) {
	memset(this, 0, sizeof(InputChannel));

	this->type = type;
}

ShaderNode::InputChannel::InputChannel(string_view name, glm::vec4 value) : InputChannel(name, Channel4) {
	this->value4 = value;
}

ShaderNode::InputChannel::InputChannel(string_view name, glm::vec3 value) : InputChannel(name, Channel3) {
	this->value3 = value;
}

ShaderNode::InputChannel::InputChannel(string_view name, glm::vec2 value) : InputChannel(name, Channel2) {
	this->value2 = value;
}

ShaderNode::InputChannel::InputChannel(string_view name, float value) : InputChannel(name, Channel1) {
	this->value1 = value;
}

glm::vec2 calc_screen_space(ShaderGraph& graph,  glm::vec2 position = {}) {
	position.x += ImGui::GetWindowPos().x + graph.scrolling.x;
	position.y += ImGui::GetWindowPos().y + 50.0f + graph.scrolling.y;
	return graph.scale * position;
}


float calc_extra_height(ShaderGraph& graph, ShaderNode* node) {
	if (node->type == ShaderNode::TEXTURE_NODE) return graph.scale * 380 + 10.0f;
	if (node->type == ShaderNode::PBR_NODE) return graph.scale * 380 + 10.0f;
	return 0;
}

float override_width_of_node(ShaderNode* self);

glm::vec2 calc_size(ShaderGraph& graph, ShaderNode* self) {
	float width = override_width_of_node(self);
    
    //(self->inputs.length * 2.5f) +
	return  graph.scale * glm::vec2(width, 80.0f + fmax(1.0f, self->inputs.length) * 40.0f) + glm::vec2(0, calc_extra_height(graph, self));
}

bool mouse_hovering(glm::vec2 position, glm::vec2 size) {
	ImVec2 prev = ImGui::GetCurrentWindow()->DC.CursorPos;
	
	ImGui::SetCursorScreenPos(ImVec2(position.x, position.y));
	ImGui::InvisibleButton("node", ImVec2(size.x, size.y));

	bool hovered = ImGui::IsItemHovered();

	ImGui::SetCursorScreenPos(prev);

	return hovered;
}

bool was_dragging = false;

bool mouse_begin_drag() {	
	return ImGui::IsMouseDragging(ImGuiMouseButton_Left) && !was_dragging;
}

bool mouse_hovering_node(ShaderGraph& graph, ShaderNode* self) {
	return mouse_hovering(calc_screen_space(graph, self->position), calc_size(graph, self));
}

template<typename T>
ShaderNode::Link* make_link(T& channel, shader_node_handle) {}

template<>
ShaderNode::Link* make_link(ShaderNode::InputChannel& channel, shader_node_handle node) {
	channel.link.to = node;
	channel.link.from.id = { INVALID_HANDLE };
	return &channel.link;
}

template<>
ShaderNode::Link* make_link(ShaderNode::OutputChannel& channel, shader_node_handle node) {
	channel.links.append(ShaderNode::Link());
	ShaderNode::Link* link = &channel.links[channel.links.length - 1];
	link->to.id = { INVALID_HANDLE };
	link->from = node;
	return link;
}

template<typename T>
void connect_link(ShaderGraph& graph, T& channel, shader_node_handle, unsigned int index) {}

template<>
void connect_link(ShaderGraph& graph, ShaderNode::InputChannel& channel, shader_node_handle node, unsigned int index) {
	ShaderNode::Link* link = graph.action.link;

	if (!link->to.id || !link->from.id) return;
	
	link->to = node;
	link->index = index;

	//Links should be owned by input channel
	channel.link = *link;

	//Remove link from result node
	ShaderNode* node_ptr = graph.nodes_manager.get(link->from);

	for (uint i = 0; i < node_ptr->output.links.length; i++) {
		if (&node_ptr->output.links[i] == link) {
			ShaderNode::Link popped_link = node_ptr->output.links.pop();
			node_ptr->output.links.data[i] = popped_link; //shrink and replace
			break;
		}
	}
}

template<typename T>
void connect_temporary_link(ShaderGraph& graph, T& channel, shader_node_handle, unsigned int index) {}

template<>
void connect_temporary_link(ShaderGraph& graph, ShaderNode::OutputChannel& channel, shader_node_handle node, unsigned int index) {
	ShaderNode::Link* link = graph.action.link;
	
	if (link->from.id) return; //already is an output
	if (node.id == graph.action.link->to.id) return; //Trying to connect same nodes input to its output
	
	link->from = node;
}


template<>
void connect_temporary_link(ShaderGraph& graph, ShaderNode::InputChannel& channel, shader_node_handle node, unsigned int index) {
	ShaderNode::Link* link = graph.action.link;
	
	if (link->to.id) return; //already is an input
	if(node.id == graph.action.link->from.id) return; //Trying to connect same nodes input to its output
	
	link->to = node;
	link->index = index;
}

template<typename T>
bool is_input(T&) {};

template<>
bool is_input(ShaderNode::InputChannel& channel) { return true; }

template<>
bool is_input(ShaderNode::OutputChannel& channel) { return false; }

template<typename T>
void draw_connector(ShaderGraph& graph, glm::vec2 start_position, shader_node_handle node, T& connector, unsigned int index = 0) {
	auto window = ImGui::GetCurrentWindow();
	auto draw_list = window->DrawList;

	connector.position = start_position;
	
	draw_list->AddCircleFilled(ImVec2(start_position.x, start_position.y), 10.0f * graph.scale, ImColor(120, 120, 120));

	glm::vec2 begin_hitbox = glm::vec2(start_position.x - graph.scale * (is_input(connector) ? 30.0f : 20.0f), start_position.y - graph.scale * 20.0f);
	glm::vec2 hitbox_size = glm::vec2(graph.scale * 40.0f);

	if (graph.action.type == ShaderGraph::Action::None && ImGui::IsMouseDragging(ImGuiMouseButton_Left) && mouse_hovering(begin_hitbox, hitbox_size)) {
		graph.action.type = ShaderGraph::Action::Dragging_Connector;
		graph.action.link = make_link(connector, node);
		graph.action.link->index = index;
	}

	if (graph.action.type == ShaderGraph::Action::Dragging_Connector && mouse_hovering(begin_hitbox, hitbox_size)) {
		connect_temporary_link(graph, connector, node, index);
	}

	if (graph.action.type == ShaderGraph::Action::Releasing_Connector && mouse_hovering(begin_hitbox, hitbox_size)) {
		connect_link(graph, connector, node, index);
		graph.action.type = ShaderGraph::Action::None;
	}

	//draw_list->AddRectFilled(ImVec2(begin_hitbox.x, begin_hitbox.y), ImVec2(begin_hitbox.x + hitbox_size.x, begin_hitbox.y + hitbox_size.y), ImColor(255, 255, 255));
}

glm::vec2 calc_pos_of_node(ShaderGraph& graph, shader_node_handle handle, bool is_selected);

glm::vec2 calc_pos_of_output(ShaderGraph& graph, shader_node_handle handle) {
	return graph.nodes_manager.get(handle)->output.position;
}

glm::vec2 calc_pos_of_input(ShaderGraph& graph, shader_node_handle handle, unsigned int index) {
	return graph.nodes_manager.get(handle)->inputs[index].position;
}

ImVec2 calc_link_pos(ShaderGraph& graph, ShaderNode::Link* link) {
	glm::vec2 result;
	if (link->from.id) result = calc_pos_of_output(graph, link->from);
	if (link->to.id) result = calc_pos_of_input(graph, link->to, link->index);

	return ImVec2(result.x, result.y);
}

void render_bezier_link(ShaderGraph& graph, ImVec2 start, ImVec2 end, bool not_connected = false) {
	glm::vec2 v_start(start.x, start.y);
	glm::vec2 v_end(end.x, end.y);

	float bezier = 200.0f;

	float length = glm::length(v_start.x - v_end.x);

	length = glm::min(length * 0.5f, 50.0f * graph.scale);

	glm::vec2 cp0, cp1;

	if (not_connected && start.x > end.x) {
		std::swap(v_start, v_end);
		std::swap(start, end);
	}
	
	if (v_start.x > v_end.x && false) {
		cp0 = v_start - glm::vec2(length, 0);
		cp1 = v_end + glm::vec2(length, 0);
	}
	else {
		cp0 = v_start + glm::vec2(length, 0);
		cp1 = v_end - glm::vec2(length, 0);
	}

	ImGui::GetCurrentWindow()->DrawList->AddLine(start, ImVec2(cp0.x, cp0.y), ImColor(120, 120, 120), graph.scale * 5.f);
	ImGui::GetCurrentWindow()->DrawList->AddLine(ImVec2(cp0.x, cp0.y), ImVec2(cp1.x, cp1.y), ImColor(120, 120, 120), graph.scale * 5.f);
	ImGui::GetCurrentWindow()->DrawList->AddLine(ImVec2(cp1.x, cp1.y), end, ImColor(120, 120, 120), graph.scale * 5.f);
}


void render_complete_link(ShaderGraph& graph, ShaderNode::Link* link) {
	ShaderNode* node_result = graph.nodes_manager.get(link->from);
	ShaderNode* node_input = graph.nodes_manager.get(link->to);

	if (node_result && node_input) {
		glm::vec2 pos1 = calc_pos_of_output(graph, link->from); 
		glm::vec2 pos2 = calc_pos_of_input(graph, link->to, link->index);

		render_bezier_link(graph, pos1, pos2);
	}
}

void render_link(ShaderGraph& graph, ShaderNode::Link* link, bool is_input) {
	
	if (graph.action.type == ShaderGraph::Action::Dragging_Connector && graph.action.link == link) {
		if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
			ShaderNode* node_result = graph.nodes_manager.get(link->from);
			ShaderNode* node_input = graph.nodes_manager.get(link->to);

			if (node_result && node_input) {
				render_complete_link(graph, link);
				if (is_input) link->from.id = INVALID_HANDLE;
				else link->to.id = INVALID_HANDLE;
			}
			else {
				render_bezier_link(graph, calc_link_pos(graph, link), ImGui::GetMousePos(), true);
			}
		}
		else {
			graph.action.type = ShaderGraph::Action::Releasing_Connector;
		}
	}
	else if (graph.action.type == ShaderGraph::Action::Releasing_Connector && graph.action.link == link) {
		graph.action.type = ShaderGraph::Action::None;
	}
	else {
		render_complete_link(graph, link);
	}
}

void render_input(ShaderGraph& graph, shader_node_handle handle, unsigned int i, const char** names, bool render_default) {
	const char* name = names[i];
	
	ShaderNode* node = graph.nodes_manager.get(handle);
	ShaderNode::InputChannel& input = node->inputs[i];
	
	auto window = ImGui::GetCurrentWindow();

	glm::vec2 start_position = glm::vec2(window->DC.CursorPos.x - 10.0f * graph.scale, window->DC.CursorPos.y + 20.0f * graph.scale);

	draw_connector(graph, start_position, handle, input, i);

	bool is_connected = input.link.from.id != INVALID_HANDLE;

	ImGui::PushID(handle.id);
    ImGui::SetNextItemWidth(200 * graph.scale);
    ImGui::Dummy(ImVec2(0, 40.0f * graph.scale));
    
    ImGui::SameLine();
    
	if (input.type == Channel1 && !is_connected && render_default) {
        ImGui::SetNextItemWidth(200 * graph.scale);
		ImGui::InputFloat(name, &input.value1);
	}
    else if (input.type == Channel2 && !is_connected && render_default) {
        ImGui::SameLine();
        ImGui::InputFloat2(names[i], &input.value2.x);
    } else if (input.type == Channel3 && !is_connected && render_default) {
		edit_color(input.value3, name, glm::vec2(graph.scale) * 35.0f);
		ImGui::SameLine();
		ImGui::Text(name);
	}
	else if (input.type == Channel4 && !is_connected && render_default) {
		edit_color(input.value4, name, glm::vec2(graph.scale) * 35.0f);
		ImGui::SameLine();
		ImGui::Text(name);
	}
	else {
		ImGui::Text(name);
	}
    

	ImGui::PopID();
}

void render_output(ShaderGraph& graph, shader_node_handle handle) {
	ShaderNode* node = graph.nodes_manager.get(handle);
	ShaderNode::OutputChannel& output = node->output;
	float width = override_width_of_node(node);

	auto window = ImGui::GetCurrentWindow();

	if (output.type != ChannelNone) {
		glm::vec2 start_position = glm::vec2(window->DC.CursorPos.x + (width - 10.0f) * graph.scale, window->DC.CursorPos.y + 20.0f * graph.scale);
		draw_connector(graph, start_position, handle, output);
	}
}

glm::vec2 snap_to_grid(ShaderGraph& graph, glm::vec2 position) {
	return glm::vec2(glm::round(position.x / GRID_SZ) * GRID_SZ, glm::round(position.y / GRID_SZ) * GRID_SZ);
}

glm::vec2 screenspace_to_position(ShaderGraph& graph, glm::vec2 position) {
	position /= graph.scale;

	position.x -= ImGui::GetWindowPos().x + graph.scrolling.x;
	position.y -= ImGui::GetWindowPos().y + graph.scrolling.y + 50.0f;

	return position;
}


glm::vec2 calc_pos_of_node(ShaderGraph& graph, shader_node_handle handle, bool is_selected) {
	auto self = graph.nodes_manager.get(handle);

	glm::vec2 position = calc_screen_space(graph, self->position);
	bool is_snapping = ImGui::IsKeyDown((int)Key::Left_Control);
	
	//If Dragging this node
	if ((graph.action.type == ShaderGraph::Action::Moving_Node && is_selected) || (graph.action.type == ShaderGraph::Action::Dragging_Node && graph.action.drag_node.id == handle.id)) {

		bool is_done = !ImGui::IsMouseDragging(ImGuiMouseButton_Left);
		glm::vec2 delta = glm::vec2(ImGui::GetMouseDragDelta().x, ImGui::GetMouseDragDelta().y);

		if (graph.action.type == ShaderGraph::Action::Moving_Node) { //Solution to this problem, is to create a new action to apply the movement
			is_done = ImGui::IsMouseClicked(0);
			delta = glm::vec2(ImGui::GetMousePos().x - graph.action.mouse_pos.x, ImGui::GetMousePos().y - graph.action.mouse_pos.y);
		}
		
		if (!is_done) {
			position += delta;
			if (is_snapping) position = calc_screen_space(graph, snap_to_grid(graph, screenspace_to_position(graph, position)));
		}
		else {
			graph.action.type = ShaderGraph::Action::ApplyMove;
			graph.action.mouse_pos = delta;
		}
	}

	if (graph.action.type == ShaderGraph::Action::ApplyMove && is_selected) {
		self->position += graph.action.mouse_pos / graph.scale;

		if (is_snapping) self->position = snap_to_grid(graph, self->position); //LEFT OF HERE

		position = calc_screen_space(graph, self->position);
	}

	return position;
}

void render_node_inner(ShaderGraph& graph, ShaderNode* self, shader_node_handle handle);

void deselect(ShaderGraph& graph, shader_node_handle handle) {
	for (auto& selected_handle : graph.selected) {
		if (selected_handle.id == handle.id) {
			selected_handle = graph.selected.pop();
			break;
		}
	}
}

void render_node(ShaderGraph& graph, shader_node_handle handle) {
	auto self = graph.nodes_manager.get(handle);

	glm::vec2 size = calc_size(graph, self);

	auto window = ImGui::GetCurrentWindow();
	
	auto draw_list = ImGui::GetWindowDrawList();

	bool is_selected = false;
	for (auto selected_handle : graph.selected) { //todo should add equals operator to handle to avoid this
		if (selected_handle.id == handle.id) {
			is_selected = true;
			break;
		}
	}

	if (graph.action.type == ShaderGraph::Action::BoxSelectApply) {
		glm::vec2 start = graph.action.mouse_pos;
		glm::vec2 end = start + graph.action.mouse_delta;
		
		if (start.x > end.x) std::swap(start.x, end.x);
		if (start.y > end.y) std::swap(start.y, end.y);
		
		glm::vec2 position = calc_screen_space(graph, self->position);
		
		if (position.x > start.x && position.y > start.y && position.x + size.x < end.x && position.y + size.y < end.y) { //In Box Selection
			if (ImGui::IsKeyDown((int)Key::Left_Shift)) {
				deselect(graph, handle);
				is_selected = false;
			}
			else if (!is_selected) {
				is_selected = true;
				graph.selected.append(handle);
			}
		}

		
	}

	//Begin move
	if (ImGui::IsKeyDown((int)Key::G)) {
		graph.action.type = ShaderGraph::Action::Moving_Node;
		graph.action.mouse_pos = glm::vec2(ImGui::GetMousePos().x, ImGui::GetMousePos().y);
	}

	glm::vec2 position = calc_pos_of_node(graph, handle, is_selected);

	if (is_selected) {
		draw_list->ChannelsSetCurrent(2);

		float frame_border = 1.0f;
		draw_list->AddRect(ImVec2(position.x - frame_border, position.y - frame_border), ImVec2(position.x + size.x + frame_border, position.y + size.y + frame_border), ImColor(255, 150, 0), 5.0f);
	}

	draw_list->AddRectFilled(ImVec2(position.x, position.y), ImVec2(position.x + size.x, position.y + size.y), ImColor(80, 80, 80), 5.0f);
	draw_list->AddRectFilled(ImVec2(position.x, position.y), ImVec2(position.x + size.x, position.y + 35.0f * graph.scale), ImColor(40, 40, 40), 5.0f);
	draw_list->AddRectFilled(ImVec2(position.x, position.y + 30.0f * graph.scale), ImVec2(position.x + size.x, position.y + 40.0f * graph.scale), ImColor(40, 40, 40));

	float padding = graph.scale * 10.0f;

	ImGui::SetCursorScreenPos(ImVec2(position.x + padding, position.y + 10 * graph.scale));
	
	//Draw Node Window
	ImGui::BeginGroup(); // Lock horizontal position
	//ImGui::PopID();

	render_node_inner(graph, self, handle);

	for (int i = 0; i < self->inputs.length; i++) {
		auto& input = self->inputs[i];
	}

	ImGui::EndGroup();

	//Select or unselect
	if (ImGui::IsMouseClicked(0) && mouse_hovering_node(graph, self) && graph.action.type == ShaderGraph::Action::None) {
		if (ImGui::IsKeyDown((int)Key::Left_Shift)) {
			if (is_selected) {
				is_selected = false;
				deselect(graph, handle);
			}
			else {
				graph.selected.append(handle);
			}
		}
		else {
			graph.selected.clear();
			graph.selected.append(handle);
			is_selected = true;
		}
	}

	//Begin Drag
	if (graph.action.type == ShaderGraph::Action::None && ImGui::IsMouseDragging(ImGuiMouseButton_Left) && mouse_hovering_node(graph, self) && is_selected) {
		graph.action.type = ShaderGraph::Action::Dragging_Node;
		graph.action.drag_node = handle;
	}

	draw_list->ChannelsSetCurrent(1);
}

MaterialDesc mat_desc_for_shader_graph(ShaderAsset& asset) {
	ShaderGraph& graph = *asset.graph;

	MaterialDesc desc{ asset.handle };
	
	for (ParamDesc& p : graph.dependencies) {
		desc.params.append(p);
	}

	for (ParamDesc& p : graph.parameters) {
		desc.params.append(p);
	}

	//mat_vec2(desc, "transformUVs", glm::vec2(1.0f));

	return desc;
}

void set_params_for_shader_graph(AssetTab& asset_tab, shader_handle shader_handle) {
	AssetNode* asset_node = asset_tab.info.asset_type_handle_to_node[AssetNode::Shader][shader_handle.id];
	ShaderAsset* asset = asset_node ? &asset_node->shader : nullptr;
	if (!asset) return;

	MaterialDesc desc = mat_desc_for_shader_graph(*asset);


}

void render_preview(ShaderAsset* asset, AssetPreviewResources& preview, material_handle default_material) {	
	ShaderGraph& graph = *asset->graph;
	
	material_handle mat_handle = graph.preview_mat_handle;
	if (!mat_handle.id) mat_handle = default_material;

	MaterialAsset material_asset;
	material_asset.handle = mat_handle;
	material_asset.rot_preview = graph.rot_preview;

	render_preview_for(preview, material_asset);
}

glm::vec2 shift_by_grid(ShaderGraph& graph, glm::vec2 pos, glm::vec2 shift) {
	return calc_screen_space(graph, screenspace_to_position(graph, pos) + shift);
}

void draw_grid(ShaderGraph& graph, float grid_sz, float width) {
	auto draw_list = ImGui::GetCurrentWindow()->DrawList;

	//Draw Grid
	ImU32 GRID_COLOR = IM_COL32(200, 200, 200, 40);
	ImVec2 win_pos = ImGui::GetWindowPos();
	win_pos.y += 50.0f;
	ImVec2 canvas_sz = ImGui::GetWindowSize();
	
	GRID_SZ = grid_sz; //todo turn into paremeter
	glm::vec2 start = calc_screen_space(graph, snap_to_grid(graph, screenspace_to_position(graph, glm::vec2(0, 0))));

	for (glm::vec2 pos = start; pos.x < canvas_sz.x + win_pos.x;) {
		draw_list->AddLine(ImVec2(pos.x, win_pos.y), ImVec2(pos.x, canvas_sz.y + win_pos.y), GRID_COLOR, width);
		pos = shift_by_grid(graph, pos, glm::vec2(GRID_SZ, 0));
	}


	for (glm::vec2 pos = start; pos.y < canvas_sz.y + win_pos.y;) {
		glm::vec2 rel_pos = screenspace_to_position(graph, pos);
		draw_list->AddLine(ImVec2(win_pos.x, pos.y), ImVec2(canvas_sz.x + win_pos.x, pos.y), GRID_COLOR, width);
		pos = shift_by_grid(graph, pos, glm::vec2(0, GRID_SZ));
	}

	GRID_SZ = 40.0f;
}

void set_scale(ShaderGraph* graph, Input& input, float scale) {
	scale = glm::clamp(scale, 0.1f, 2.0f);
    scale = ceilf(scale * 8) / 8;

	glm::vec2 mouse_pos(ImGui::GetMousePos().x, ImGui::GetMousePos().y);
	glm::vec2 mouse_rel = screenspace_to_position(*graph, mouse_pos);

	if (scale != graph->scale) {		
		graph->scale = scale;
		graph->scrolling += screenspace_to_position(*graph, mouse_pos) - mouse_rel; //Zoom in a point
	}
}

void ShaderGraph::render(World& world, RenderPass& ctx, Input& input) {
	ImGui::PushClipRect(ImGui::GetCursorScreenPos(), ImVec2(ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionMax().x, ImGui::GetCursorScreenPos().y + ImGui::GetContentRegionMax().y), true);

	float scroll_speed = 0.2f;

    static float non_snapped = scale;
    
	if (input.scroll_offset > 0) {
        non_snapped *= (1.0 + input.scroll_offset * scroll_speed);
		set_scale(this, input, non_snapped);
	}
	else if (input.scroll_offset < 0) {
        non_snapped /= (1.0 - input.scroll_offset * scroll_speed);
		set_scale(this, input, non_snapped);
	}
	
	float target_font_size = 30.0f * scale;

	auto g = ImGui::GetCurrentContext();
	float fontBaseSize = g->FontBaseSize;
	float fontSize = g->FontSize;

	//g->FontSize = target_font_size;
	//g->FontBaseSize = (g->FontSize / fontSize) * g->FontBaseSize;

	draw_grid(*this, 40.0f, 1.0f);
	draw_grid(*this, 40.0f * 5.0f, 3.0f);

	auto draw_list = ImGui::GetCurrentWindow()->DrawList;

	draw_list->ChannelsSplit(4);

	draw_list->ChannelsSetCurrent(1);
    
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2.5 * scale, 2.5 * scale));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0,0));

	//Draw Nodes
	for (unsigned int i = 0; i < nodes_manager.slots.length; i++) {
		render_node(*this, nodes_manager.index_to_handle(i));
	}
    
    ImGui::PopStyleVar(2);

	draw_list->ChannelsSetCurrent(0);

	//Draw Links
	for (unsigned int i = 0; i < nodes_manager.slots.length; i++) {
		ShaderNode* node = nodes_manager.get(nodes_manager.index_to_handle(i));

		for (auto& input : node->inputs) {
			render_link(*this, &input.link, true);
		}

		for (auto& link : node->output.links) {
			render_link(*this, &link, false);
		}
	}

	draw_list->ChannelsSetCurrent(3);

	if (action.type == Action::BoxSelect) {
		glm::vec2 start = action.mouse_pos;
		glm::vec2 end = start + glm::vec2(ImGui::GetMouseDragDelta());

		float length = 5.0f;

		auto line_color = ImColor(200, 200, 200);
		float line_width = 2.0f;

		if (start.x > end.x) std::swap(start.x, end.x);
		if (start.y > end.y) std::swap(start.y, end.y);

		for (float x = start.x; x < end.x - length; x += length * 2) draw_list->AddLine(ImVec2(x, start.y), ImVec2(x + length, start.y), line_color, line_width);
		for (float x = start.x; x < end.x - length; x += length * 2) draw_list->AddLine(ImVec2(x, end.y), ImVec2(x + length, end.y), line_color, line_width);
		for (float y = start.y; y < end.y - length; y += length * 2) draw_list->AddLine(ImVec2(start.x, y), ImVec2(start.x, y + length), line_color, line_width);
		for (float y = start.y; y < end.y - length; y += length * 2) draw_list->AddLine(ImVec2(end.x, y), ImVec2(end.x, y + length), line_color, line_width);

		draw_list->AddRectFilled(ImVec2(start.x, start.y), ImVec2(end.x, end.y), ImColor(40.0f, 40.0f, 40.0f, 0.2f));
	}


	draw_list->ChannelsMerge();

	g->FontBaseSize = fontBaseSize;
	g->FontSize = fontSize;

	ImGui::PopClipRect();
}

string_buffer get_dependency(shader_node_handle node) {
	return tformat("_dep", node.id);
}

ShaderCompiler::ShaderCompiler(ShaderGraph& graph) : graph(graph) {

}

void ShaderCompiler::begin() {
	contents +=
		"#include shaders/vert_helper.glsl\n"
		"#include shaders/pbr_helper.glsl\n"
		"layout (location = 0) out vec4 FragColor;\n";

	if (graph.parameters.length + graph.dependencies.length > 0) {
		contents += "struct Material {\n";

		for (ParamDesc& param : graph.dependencies) {
			if (param.type == Param_Image) {
				contents += "    sampler2D ";
				contents += param.name;
				contents += ";\n";
			}
		}

		for (unsigned int i = 0; i < graph.parameters.length; i++) {
			ParamDesc& param = graph.parameters[i];

			if (param.type == Param_Image) {
				contents += "    channel4 ";
				contents += graph.parameters[i].name;
				contents += ";\n";
			}

			if (param.type == Param_Float) {
				contents += "    float ";
				contents += graph.parameters[i].name;
				contents += ";\n";
			}
		}
		contents += "};\n";
	}

	contents += 
		"void main()\n"
		"{\n"
		;
}

void ShaderCompiler::end() {
	contents +=
		"#endif\n"
		"}\n";
}

//todo deactivate links on delete

void ShaderCompiler::compile_input(ShaderNode::InputChannel& input) {
	if (input.link.from.id == INVALID_HANDLE) {
		if (input.type == Channel1) {
			contents += format(input.value1);
		}

		if (input.type == Channel2) {
			contents += format("vec2(", input.value2.x, ",", input.value2.y, ")");
		}

		if (input.type == Channel3) {
			contents += format("vec3(", input.value3.x, ",", input.value3.y, ",", input.value3.z, ")");
		}

		if (input.type == Channel4) {
			contents += format("vec4(", input.value3.x, ",", input.value3.y, ",", input.value3.z, ",", input.value4.w, ")");
		}
	}
	else {
		ShaderNode* output = graph.nodes_manager.get(input.link.from);

		if (output->output.type > input.type) {
			compile_node(input.link.from);
				
			if (input.type == Channel1) contents += ".x";
			if (input.type == Channel2) contents += ".xy";
			if (input.type == Channel3) contents += ".xyz";
		}
		else if (output->output.type < input.type) {
			if (input.type == Channel4) {
				if (output->output.type == Channel1) {
					contents += "vec4(vec3(";
					compile_node(input.link.from);
					contents += "), 1.0f)";
				}

				if (output->output.type == Channel2) {
					contents += "vec4(";
					compile_node(input.link.from);
					contents += "0.0f, 1.0f)";
				}

				if (output->output.type == Channel3) {
					contents += "vec4(";
					compile_node(input.link.from);
					contents += "1.0f)";
				}
			}
				
			if (input.type == Channel3) {
				if (output->output.type == Channel1) {
					contents += "vec3(";
					compile_node(input.link.from);
					contents += ")";
				}

				if (output->output.type == Channel2) {
					contents += "vec3(";
					compile_node(input.link.from);
					contents += "0.0f)";
				}
			}

			if (input.type == Channel2) {
				if (output->output.type == Channel1) {
					contents += "vec2(";
					compile_node(input.link.from);
					contents += ")";
				}
			}
		}
		else {
			compile_node(input.link.from);
		}
	}
}

void ShaderCompiler::compile() {
	find_dependencies();

	begin();

	shader_node_handle base_node = { INVALID_HANDLE };
		
	for (int i = 0; i < graph.nodes_manager.slots.length; i++) {
		ShaderNode* node = graph.nodes_manager.get(graph.nodes_manager.index_to_handle(i));
		
		if (node->type == ShaderNode::PBR_NODE) {
			base_node = graph.nodes_manager.index_to_handle(i);
			break;
		}
	}

	if (base_node.id == INVALID_HANDLE) {
		log("Shader Graph missing base node");
	}

	compile_node(base_node);

	end();
}


struct BackgroundCompilationInput {
	shader_handle handle;
	string_buffer name;
	string_buffer fragment_src;
	material_handle mat_handle;
	MaterialDesc previous_mat_desc;
	MaterialDesc current_mat_desc;
	LinearAllocator allocator;
	u64 memory_size;
};

struct BackgroundCompilation {
	BackgroundCompilationInput input;
	atomic_counter elap = 0;
	atomic_counter head = 0;
	atomic_counter tail = 1;
	std::atomic_flag compiling;
};

bool receive_background_compilation(BackgroundCompilation& compilation, BackgroundCompilationInput* input) {
	uint tail = compilation.tail.load(std::memory_order_relaxed);
	while (true) {
		uint elap = compilation.elap.load(std::memory_order_relaxed);
		//elap must be odd, meaning the write completed
		if (elap == tail) {
			if (compilation.tail.compare_exchange_strong(tail, tail + 2, std::memory_order_acquire)) {
				*input = std::move(compilation.input);
				compilation.elap.store(elap + 1, std::memory_order_release);

				return true;
			}
		}
		else if (tail - elap > 0) {
			break;
		}
	}

	return false;
}

void send_background_compilation(BackgroundCompilation& compilation, BackgroundCompilationInput&& input) {
	uint head = compilation.head.load(std::memory_order_relaxed);
	while (true) {
		uint elap = compilation.elap.load(std::memory_order_relaxed);
		if (elap == head) { //read completed/empty and safe to write to
			if (compilation.head.compare_exchange_strong(head, head + 2, std::memory_order_acquire)) {
				compilation.input = std::move(input);
				compilation.elap.store(elap + 1, std::memory_order_release);
				break;
			}
		}
		else if (elap + 1 == head) {  //read has not completed
			if (compilation.head.compare_exchange_strong(head, head + 2, std::memory_order_acquire)) {
				if (!compilation.tail.compare_exchange_strong(elap, elap + 2, std::memory_order_acquire)) {
					compilation.head.store(head); //roll back, began reading element
					continue;
				}

				compilation.input = std::move(input);
				compilation.elap.store(elap + 2, std::memory_order_release);
				break;
			}
		}
		else { //currently being read from
			continue; //problem here is write may get preempted before elap is stored
		}
	}
}

#include "core/context.h"

void compile_in_background(BackgroundCompilation& compilation) {
	if (compilation.compiling.test_and_set()) {
		return;
	}
	
	BackgroundCompilationInput input;
	if (!receive_background_compilation(compilation, &input)) {
		compilation.compiling.clear();
		return;
	}

	{

		LinearAllocator& linear_allocator = input.allocator;
		Context context = {};
		context.temporary_allocator = &linear_allocator;

		ScopedContext scoped(context);

		if (reload_Shader(input.handle)) {
			update_Material(input.mat_handle, input.previous_mat_desc, input.current_mat_desc);
		}
	}

	compilation.compiling.clear();

	JobDesc job_desc(compile_in_background, &compilation);
	add_jobs(PRIORITY_MEDIUM, job_desc, nullptr);
}

//todo move into compile and seperate out for load Shader
void load_Shader_for_graph(ShaderAsset& asset) {
	ShaderGraph& graph = *asset.graph;
	
	if (asset.handle.id == INVALID_HANDLE) {
		string_buffer file_path = tformat("data/shader_output/", asset.name, ".frag");
		asset.handle = load_Shader("shaders/pbr.vert", file_path);
		
		graph.preview_mat_desc = mat_desc_for_shader_graph(asset);
		graph.preview_mat_handle = make_Material(graph.preview_mat_desc);
	}
	else {
		static BackgroundCompilation compilation;
		const uint background_memory_size = kb(256);
		static char* background_memory = (char*)malloc(background_memory_size);

		MaterialDesc previous_mat_desc = graph.preview_mat_desc;
		graph.preview_mat_desc = mat_desc_for_shader_graph(asset);

		BackgroundCompilationInput input;
		input.name = asset.name;
		input.handle = asset.handle;
		input.mat_handle = asset.graph->preview_mat_handle;
		input.previous_mat_desc = previous_mat_desc;
		input.current_mat_desc = graph.preview_mat_desc;
		input.memory = background_memory;
		input.memory_size = background_memory_size;

		send_background_compilation(compilation, std::move(input));
		
		JobDesc job_desc(compile_in_background, &compilation);
		add_jobs(PRIORITY_MEDIUM, job_desc, nullptr);
	}
}

//todo ShaderGraph needs serious efforts to function again
void compile_shader_graph(AssetInfo& asset_tab, AssetNode& asset_node) {	
	static string_buffer last_output;
	
	Profile compile("Compile Shader Graph");

	ShaderAsset& asset = asset_node.shader;
	ShaderCompiler compiler(*asset.graph);
	compiler.compile();

	if (last_output == compiler.contents) return; //todo rather wastefull

	string_buffer file_path = format("data/shader_output/", asset.name, ".frag");

	if (!io_writef(file_path, compiler.contents)) {
		log("Could not open file to write shader output");
		return;
	}

	load_Shader_for_graph(asset);

	asset_tab.asset_type_handle_to_node[AssetNode::Shader][asset.handle.id] = &asset_node;	
	//reload(assets, asset->handle);

	last_output = string_view(compiler.contents);
}

void new_node_popup(ShaderAsset* asset);

void ShaderEditor::render(World& world, Editor& editor, RenderPass& ctx, Input& input) {
	if (!this->open) return;

	AssetInfo& tab = editor.asset_info;
	AssetPreviewResources& preview = editor.asset_tab.preview_resources;

	if (ImGui::Begin("Shader Graph Editor", &this->open, ImGuiWindowFlags_NoScrollbar)) {
		if (this->current_shader.id == INVALID_HANDLE) {
			ImGui::Text("Please select a shader");
			ImGui::End();
			return;
		}

		AssetNode& asset = *get_asset(tab, current_shader);
		ShaderGraph* graph = asset.shader.graph;

		ImGui::SetNextItemWidth(ImGui::GetContentRegionMax().x * 0.8);
		render_name(asset.shader.name);

		ImGui::SameLine();
		if (ImGui::Button("Compile")) {
			compile_shader_graph(tab, asset);
			render_preview(&asset.shader, preview, tab.default_material);
		}

		if (input.key_mod_pressed(Key::R)) {
			compile_shader_graph(tab, asset);
			render_preview(&asset.shader, preview, tab.default_material);
		}

		if (input.key_mod_pressed(Key::S)) {
			set_scale(graph, input, 1.0f);
		}

		if (input.key_mod_pressed(Key::C)) {
			graph->scrolling = glm::vec2(0, 0);
			graph->scale = 1.0f;
		}

		if (input.key_pressed(Key::X)) {
			for (auto handle : graph->selected) {
				graph->nodes_manager.free(handle);
			}
		}

		float font_size = 35.0f * graph->scale;
		
		if (font_size > 90.0f) {
			ImGui::PushFont(font[9]);
		}
		else {
            uint font_lod = (int)glm::floor(font_size / 10.0f);
            ImGui::SetWindowFontScale(font_size / font[font_lod]->FontSize);
            ImGui::PushFont(font[font_lod]);

			//log("Scaling to font size ", font_size);
			//log("Got font size ", 10.0f * ((int)glm::ceil(font_size / 10.0f)));
		}
    
		graph->render(world, ctx, input);

		glm::vec2 mouse_drag_begin(ImGui::GetMousePos().x, ImGui::GetMousePos().y);
		mouse_drag_begin.x -= ImGui::GetMouseDragDelta().x;
		mouse_drag_begin.y -= ImGui::GetMouseDragDelta().y;

		glm::vec2 window_a(ImGui::GetWindowPos().x, ImGui::GetWindowPos().y);
		glm::vec2 window_b(window_a.x + ImGui::GetWindowSize().x, window_a.y + ImGui::GetWindowSize().y);

		bool mouse_drag_within_window = window_a.x < mouse_drag_begin.x && window_a.y < mouse_drag_begin.y && window_b.x > mouse_drag_begin.x && window_b.y > mouse_drag_begin.y;

		//TODOS box select and deselect dont work properly, nor does delete, also duplicate hasnt been implemented, or copy and paste for that matter

		//ImGui::IsAnyItemHovered()
		bool any_item_hovered = ImGui::GetCurrentContext()->HoveredId != 0;

		if (!any_item_hovered && mouse_begin_drag && graph->action.type == ShaderGraph::Action::None && mouse_drag_within_window && !ImGui::IsKeyDown((int)Key::Left_Shift)) {
			//log("Is active ", (int)ImGui::IsAnyItemActive());
			graph->action.type = ShaderGraph::Action::BoxSelect;
			graph->action.mouse_pos = glm::vec2(ImGui::GetMousePos().x, ImGui::GetMousePos().y);
		}
        
        bool is_scrolling = ImGui::IsMouseDragging(ImGuiMouseButton_Middle)
        || (ImGui::IsMouseDragging(ImGuiMouseButton_Left) && ImGui::IsKeyDown((int)Key::Left_Shift));

        if (mouse_drag_within_window && is_scrolling) {
            graph->scrolling += glm::vec2(ImGui::GetIO().MouseDelta.x, ImGui::GetIO().MouseDelta.y) / graph->scale;
        }
        

		if (graph->action.type == ShaderGraph::Action::ApplyMove || graph->action.type == ShaderGraph::Action::BoxSelectApply) {
			graph->action.type = ShaderGraph::Action::None;
		}

		//ImGui::IsAnyItemHovered()
		if (!any_item_hovered && !ImGui::IsAnyItemActive() && ImGui::IsMouseDown(0) && graph->action.type == ShaderGraph::Action::None && mouse_drag_within_window) {
			graph->selected.clear();
		}

		if (graph->action.type == ShaderGraph::Action::BoxSelect && !ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
			graph->action.type = ShaderGraph::Action::BoxSelectApply;
			graph->action.mouse_delta.x = ImGui::GetMouseDragDelta().x;
			graph->action.mouse_delta.y = ImGui::GetMouseDragDelta().y;
		}

		if (ImGui::IsKeyPressed((int)Key::Escape)) {
			graph->action.type = ShaderGraph::Action::None;
		}

		if (ImGui::IsKeyPressed((int)Key::Left_Shift) && ImGui::IsKeyPressed((int)Key::D)) {
			//for () {
			//
			//}
		}

		was_dragging = ImGui::IsMouseDragging(ImGuiMouseButton_Left);

		//Create property node
		{
			unsigned int param_id;

			glm::vec2 position = screenspace_to_position(*graph, ImGui::GetMousePos());

			mouse_hovering(glm::vec2(ImGui::GetWindowPos()) + glm::vec2(0, 100), glm::vec2(ImGui::GetWindowSize()) - glm::vec2(0, 100));

			if (ImGui::BeginDragDropTarget()) {
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DRAG_AND_DROP_PARAM_FLOAT")) {
					param_id = *(unsigned int*)payload->Data;

					ShaderNode node(ShaderNode::PARAM_NODE, Channel1);
					node.param_id = param_id;
					node.position = position;
					graph->nodes_manager.assign_handle(std::move(node));
				}
				ImGui::EndDragDropTarget();
			}
		}

        ImGui::SetWindowFontScale(1.0);
		ImGui::PopFont();

		if (ImGui::GetIO().MouseClicked[1] && mouse_drag_within_window) {
			ImGui::OpenPopup("NewNode");
			node_popup_filter = "";
		}

		new_node_popup(&asset.shader);

	}

	ImGui::End();
}

ShaderAsset* make_new_shader(World& world, AssetTab& self, Editor& editor) {
	AssetNode asset(AssetNode::Shader);
	asset.shader.name = "Shader Graph";

	add_asset_to_current_folder(self, std::move(asset));

	return NULL;
}

ShaderNode make_pbr_node();

void shader_graph_properties(AssetNode& asset, Editor& editor, AssetTab& self) {	
	ShaderAsset* shader = &asset.shader;
	render_name(shader->name);
	
	ImGui::SetNextTreeNodeOpen(true);
	ImGui::CollapsingHeader("Shader Graph");

	if (ImGui::Button("Open Shader Editor")) {
		editor.shader_editor.open = true;
		editor.shader_editor.current_shader = self.explorer.selected;
	}

	if (shader->graph == NULL) {
		shader->graph = new ShaderGraph();
		shader->graph->nodes_manager.assign_handle(make_pbr_node(), true);
		shader->graph->rot_preview.preview_tex_coord = self.preview_resources.free_atlas_slot.pop();
	}

	compile_shader_graph(self.info, asset); //todo use undo redo system to detect change

	ImGui::SetNextTreeNodeOpen(true);
	ImGui::CollapsingHeader("Properties");

	for (unsigned int i = 0; i < shader->graph->parameters.length; i++) {
		ParamDesc& param = shader->graph->parameters[i];
		string_view name = param.name;

		if (param.type == Param_Float) {
			ImGui::InputFloat(name.c_str(), &param.real);
		
			if (ImGui::BeginDragDropSource()) {
				ImGui::Text(name.c_str());
				ImGui::SetDragDropPayload("DRAG_AND_DROP_PARAM_FLOAT", &i, sizeof(unsigned int));
				ImGui::EndDragDropSource();
			}
		}

		if (param.type == Param_Image) {
			ImGui::Image({ param.image }, ImVec2(200, 200));
			
			accept_drop("DRAG_AND_DROP_IMAGE", &param.image, sizeof(texture_handle));
			
			ImGui::SameLine();

			ImGui::Button(name.c_str()); //todo implement rename

			if (ImGui::BeginDragDropSource()) {
				ImGui::Image({ param.image }, ImVec2(200, 200));
				ImGui::SetDragDropPayload("DRAG_AND_DROP_IMAGE_PARAM", &i, sizeof(string_view));
				ImGui::EndDragDropSource();
			}

	
		}
	}

	ImGui::NewLine();
	


	static string_buffer buffer_name;
	
	if (ImGui::Button("New Property")) {
		buffer_name.length = 0;
		ImGui::OpenPopup("New Property");
	}

	if (ImGui::BeginPopup("New Property")) {
		
		ImGui::InputText("Name", buffer_name);

		ParamDesc p;
		p.name = buffer_name.view();

		bool create_property = true;

		if (ImGui::Button("Float")) {
			p.type = Param_Float;
			p.real = 1.0f;
		}
		else if (ImGui::Button("Image")) {
			p.type = Param_Image;
			p.image = 0;
		}
		else create_property = false;

		if (buffer_name.length == 0) create_property = false;

		if (create_property) {
			shader->graph->parameters.append(p);
		}

		ImGui::EndPopup();
	}

	rot_preview(self.preview_resources, shader->graph->rot_preview);
	render_preview(shader, self.preview_resources, self.info.default_material);
}

string_view get_param_name(ShaderGraph& graph, unsigned int i) {
	return graph.parameters[i].name;
}



/*
#include "core/serializer.h"

REFLECT_BEGIN_ENUM(ChannelType)
REFLECT_ENUM_VALUE(Channel1)
REFLECT_ENUM_VALUE(Channel2)
REFLECT_ENUM_VALUE(Channel3)
REFLECT_ENUM_VALUE(ChannelNone)
REFLECT_END_ENUM()

REFLECT_STRUCT_BEGIN(ShaderNode::Link)
REFLECT_STRUCT_MEMBER(to)
REFLECT_STRUCT_MEMBER(from)
REFLECT_STRUCT_MEMBER(index)
REFLECT_STRUCT_END()

REFLECT_UNION_BEGIN(ShaderNode::InputChannel)
REFLECT_UNION_FIELD(link)
REFLECT_UNION_FIELD(position)
REFLECT_UNION_CASE_BEGIN()
REFLECT_UNION_CASE(value1)
REFLECT_UNION_CASE(value2)
REFLECT_UNION_CASE(value3)
REFLECT_UNION_END()

REFLECT_STRUCT_BEGIN(ShaderNode::OutputChannel)
REFLECT_STRUCT_MEMBER(type)
REFLECT_STRUCT_MEMBER(links)
REFLECT_STRUCT_MEMBER(position)
REFLECT_STRUCT_END()



void deserialize_shader_asset(DeserializerBuffer& buffer, ShaderAsset* asset) {
	if (asset->graph == NULL) {
		asset->graph = new ShaderGraph();
	}
	
	buffer.read_string(asset->name);
	ShaderGraph* graph = asset->graph;

	unsigned int num = buffer.read_int();

	for (unsigned int i = 0; i < num; i++) {
		shader_node_handle handle = { buffer.read_int() };

		ShaderNode node;
		node.type = (ShaderNode::Type) buffer.read_int();
		node.collapsed = buffer.read_byte();

		buffer.read_struct((refl::Struct*) reflect::TypeResolver<glm::vec2>::get(), &node.position);
		buffer.read_array((reflect::TypeDescriptor_Vector*)  reflect::TypeResolver<decltype(node.inputs)>::get(), &node.inputs);
		buffer.read_struct((refl::Struct*) reflect::TypeResolver<decltype(node.output)>::get(), &node.output);

		if (node.type == ShaderNode::MATH_NODE) {
			node.tex_node.from_param = buffer.read_byte();
		}
		if (node.type == ShaderNode::MATH_NODE) {
			node.math_op = (ShaderNode::MathOp)buffer.read_int();
		}
		if (node.type == ShaderNode::PARAM_NODE) {
			node.param_id = buffer.read_int();
		}

		graph->nodes_manager.assign_handle(handle, std::move(node));
	}

	buffer.read_int();

	buffer.read_array((reflect::TypeDescriptor_Vector*) reflect::TypeResolver<vector<ParamDesc>>::get(), &graph->parameters);

	unsigned num_names = buffer.read_int();
	
	vector<string_buffer> param_names;
	
	for (int i = 0; i < num_names; i++) {
		buffer.read_string(graph->parameters[i].name);
	}

}

void serialize_shader_asset(SerializerBuffer& buffer,  ShaderAsset* asset) {
	ShaderGraph* graph = asset->graph;

	buffer.write_string(asset->name);
	buffer.write_int(graph->nodes_manager.slots.length);

	for (unsigned int i = 0; i < graph->nodes_manager.slots.length; i++) {
		buffer.write_int(graph->nodes_manager.index_to_handle(i).id);

		ShaderNode* node = &graph->nodes_manager.slots[i];
		
		buffer.write_int(node->type);
		buffer.write_byte(node->collapsed);
		buffer.write_struct((refl::Struct*) reflect::TypeResolver<glm::vec2>::get(), &node->position);
		buffer.write_array((reflect::TypeDescriptor_Vector*)  reflect::TypeResolver<decltype(node->inputs)>::get(), &node->inputs);
		buffer.write_struct((refl::Struct*) reflect::TypeResolver<decltype(node->output)>::get(), &node->output);

		if (node->type == ShaderNode::MATH_NODE) {
			buffer.write_byte(node->tex_node.from_param);
		}
		if (node->type == ShaderNode::MATH_NODE) {
			buffer.write_int(node->math_op);
		}
		if (node->type == ShaderNode::PARAM_NODE) {
			buffer.write_int(node->param_id);
		}
	}

	buffer.write_array((reflect::TypeDescriptor_Vector*) reflect::TypeResolver<decltype(graph->parameters)>::get(), &graph->parameters);
}
*/
