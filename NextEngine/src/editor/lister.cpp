#include "stdafx.h"
#include "editor/lister.h"
#include "ecs/ecs.h"
#include "components/transform.h"
#include <unordered_map>
#include "imgui.h"
#include "editor/editor.h"
#include "core/temporary.h"
#include "logger/logger.h"
#include "editor/terrain.h"

REFLECT_STRUCT_BEGIN(EntityEditor)
REFLECT_STRUCT_MEMBER(name)
REFLECT_STRUCT_END()

StringBuffer name_with_id(World& world, ID id) {
	auto name = world.by_id<EntityEditor>(id);
	if (name) return tformat("#", id, " : ", name->name);
	else return tformat("#", id);
}

void render_hierarchies(vector<struct NameHierarchy*> & top, World& world, Editor& editor, RenderParams& params, StringView filter, int indent = 0);

struct NameHierarchy {
	StringView name;
	vector<NameHierarchy*> children;
	ID id;

	NameHierarchy(StringView name) : name(name) {};

	void render_hierarchy(World& world, Editor& editor, RenderParams& params, StringView filter, int indent = 0) {
		if (!name.starts_with(filter)) return;

		bool selected = editor.selected_id == id;
		
		ImGuiStyle* style = &ImGui::GetStyle();

		ImGui::Dummy(ImVec2(indent, 0));
		ImGui::SameLine();

		if (selected) ImGui::PushStyleColor(ImGuiCol_Button, style->Colors[ImGuiCol_ButtonActive]);

		ImGui::PushID(id);
		if (ImGui::Button(name.c_str())) {
			editor.select(id);
		}
		ImGui::PopID();

		if (selected) ImGui::PopStyleColor();

		render_hierarchies(this->children, world, editor, params, filter, indent + 30);
	}

	NameHierarchy(const NameHierarchy& other) : name(other.name) {
	}
};

void render_hierarchies(vector<NameHierarchy*>& top, World& world, Editor& editor, RenderParams& params, StringView filter, int indent) {
	for (auto child : top) {
		child->render_hierarchy(world, editor, params, filter, indent);
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

	this->filter = StringBuffer(buf);

	ImGui::SetNextWindowSize(ImVec2(0.15 * params.width, params.height));

	if (ImGui::Begin("Lister")) {
		ImGui::InputText("filter", buf, 50);

		if (ImGui::IsWindowHovered()) {
			if (ImGui::GetIO().MouseClicked[1]) ImGui::OpenPopup("CreateObject");
		}

		if (ImGui::BeginPopup("CreateObject"))
		{
			if (ImGui::MenuItem("New Terrain"))
			{
				ID id = world.make_ID();
				Entity* e = world.make<Entity>(id);
				Transform* trans = world.make<Transform>(id);
				Terrain* terrain = world.make<Terrain>(id);
				EntityEditor* name = world.make<EntityEditor>(id);
				Materials* mat = world.make<Materials>(id);
				mat->materials.append(editor.asset_tab.default_material);
				name->name = "Terrain";
			}

			ImGui::EndPopup();
		}

		if (filter.starts_with("#")) {
			auto splice = filter.sub(1, filter.size());

			ID id;
			if (!string_to_uint(splice, &id)) {
				ImGui::Text("Please enter a valid integer");
			}

			auto e = world.by_id<EntityEditor>(id);
			if (e) {
				vector<EntityEditor*> active_named = { e };
				get_hierarchy(active_named, world, top, names);
				render_hierarchies(top, world, editor, params, filter);
			}
		} 
		else if (filter.find(':') == 0) {
			auto name = filter.sub(1, filter.size());

			throw "Not implemented yet";
		}
		else {
			auto no_filter_active_named = world.filter<EntityEditor>();
			vector<EntityEditor*> active_named;

			for (auto name : no_filter_active_named) {
				if (name->name.starts_with(filter)) {
					active_named.append(name);
				}
			}

			get_hierarchy(active_named, world, top, names);
			render_hierarchies(top, world, editor, params, filter);
		}
	}

	ImGui::End();
}