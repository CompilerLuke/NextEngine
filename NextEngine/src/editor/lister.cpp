#include "stdafx.h"
#include "editor/lister.h"
#include "ecs/ecs.h"
#include "components/transform.h"
#include <unordered_map>
#include "imgui.h"
#include "editor/editor.h"
#include "core/temporary.h"

REFLECT_STRUCT_BEGIN(EntityEditor)
REFLECT_STRUCT_MEMBER(name)
REFLECT_STRUCT_END()

std::string name_with_id(World& world, ID id) {
	auto name = world.by_id<EntityEditor>(id);
	if (name) return "#" + std::to_string(id) + " : " + name->name;
	else return "#" + std::to_string(id);
}

void render_hierarchies(vector<struct NameHierarchy*> & top, World& world, Editor& editor, RenderParams& params, int indent = 0);

struct NameHierarchy {
	std::string& name;
	vector<NameHierarchy*> children;
	ID id;

	NameHierarchy(std::string& name) : name(name) {};

	void render_hierarchy(World& world, Editor& editor, RenderParams& params, int indent = 0) {
		bool selected = editor.selected_id == id;
		
		ImGuiStyle* style = &ImGui::GetStyle();

		ImGui::Dummy(ImVec2(indent, 0));
		ImGui::SameLine();

		if (selected) ImGui::PushStyleColor(ImGuiCol_Button, style->Colors[ImGuiCol_ButtonActive]);
		if (ImGui::Button(name.c_str())) {
			editor.select(id);
		}

		if (selected) ImGui::PopStyleColor();

		render_hierarchies(this->children, world, editor, params, indent + 30);
	}

	NameHierarchy(const NameHierarchy& other) : name(other.name) {
	}
};

void render_hierarchies(vector<NameHierarchy*>& top, World& world, Editor& editor, RenderParams& params, int indent) {
	for (auto child : top) {
		child->render_hierarchy(world, editor, params, indent);
	}
}

void get_hierarchy(vector<EntityEditor*>& active_named, World& world, vector<NameHierarchy*>& top, std::unordered_map<ID, NameHierarchy*> names) {
	for (auto name : world.filter<EntityEditor>()) {
		names[world.id_of(name)] = TEMPORARY_ALLOC(NameHierarchy, name->name );
	}

	for (auto name : active_named) {
		ID id = world.id_of(name);
		NameHierarchy* hierarchy = names[id];
		hierarchy->id = id;
	
		auto local = world.by_id<LocalTransform>(id);
		if (local) {
			auto owner_named = names[id];
			if (owner_named)
				owner_named->children.append(hierarchy);
			else
				top.append(hierarchy);
		}
		else
			top.append(hierarchy);
	}
}

void Lister::render(World& world, Editor& editor, RenderParams& params) {
	vector<NameHierarchy*> top;
	std::unordered_map<ID, NameHierarchy*> names;

	char buf[50];
	std::memcpy(buf, filter.c_str(), filter.size() + 1);

	this->filter = std::string(buf);

	ImGui::SetNextWindowSize(ImVec2(0.15 * params.width, params.height));

	if (ImGui::Begin("Lister")) {
		ImGui::InputText("filter", buf, 50);

		if (filter.find("#") == 0) {
			auto splice = filter.substr(1, filter.size());

			ID id;
			try {
				id = std::stoi(splice);
			}
			catch(const std::exception&) {
				ImGui::Text("Please enter a valid integer");
			}

			auto e = world.by_id<EntityEditor>(id);
			if (e) {
				vector<EntityEditor*> active_named = { e };
				get_hierarchy(active_named, world, top, names);
				render_hierarchies(top, world, editor, params);
			}
		} 
		else if (filter.find(":") == 0) {
			auto name = filter.substr(1, filter.size());

			throw "Not implemented yet";
		}
		else {
			auto no_filter_active_named = world.filter<EntityEditor>();
			vector<EntityEditor*> active_named;

			for (auto name : no_filter_active_named) {
				if (name->name.find(filter) == 0) {
					active_named.append(name);
				}
			}

			get_hierarchy(active_named, world, top, names);
			render_hierarchies(top, world, editor, params);
		}
	}

	ImGui::End();
}