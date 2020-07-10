#include "lister.h"
#include "ecs/ecs.h"
#include "components/transform.h"
#include <unordered_map>
#include <imgui/imgui.h>
#include "editor.h"
#include "core/memory/linear_allocator.h"
#include "core/io/logger.h"
#include "terrain.h"
#include "grass.h"
#include "components/terrain.h"


string_buffer name_with_id(ID id, string_view name) {
	if (name.length != 0) return tformat("#", id, " : ", name);
	else return tformat("#", id);
}


void render_hierarchy(EntityNode& node, Editor& editor, int indent = 0) {
	EntityNode* real = editor.lister.by_id[node.id];
	
	ID id = node.id;
	bool selected = editor.selected_id == node.id;
		
	ImGuiStyle* style = &ImGui::GetStyle();

	ImGui::Dummy(ImVec2(indent, 0));
	ImGui::SameLine();

	if (selected) ImGui::PushStyleColor(ImGuiCol_Button, style->Colors[ImGuiCol_ButtonActive]);

	ImGui::PushID(id);
	if (ImGui::Button(node.name.data)) editor.select(id); // 	if (ImGui::CollapsingHeader()
	 
	ImGui::PopID();

	if (selected) ImGui::PopStyleColor();

	if (real->expanded) {
		for (EntityNode& child : node.children) {
			render_hierarchy(child, editor, indent + 30);
		}
	}
}

struct EntityFilter {
	string_view filter = "";
	Archetype archetype = 0;
};

bool filter_hierarchy(EntityNode* result, EntityNode& top, World& world, EntityFilter& filter) {
	Archetype arch = world.arch_of_id(top.id);

	bool has_archetype = (arch & filter.archetype) == filter.archetype;
	bool name_meets_filter = string_view(top.name).starts_with(filter.filter);
	
	*result = {};
	result->id = top.id;
	result->name = top.name;
	result->children.allocator = &temporary_allocator;

	for (EntityNode& child : top.children) {
		EntityNode node;
		if (filter_hierarchy(&node, child, world, filter)) {
			result->children.append(node);
		}
	}

	return result->children.length > 0 || (has_archetype && name_meets_filter);
}

void add_child(Lister& lister, EntityNode& parent, EntityNode&& child) {
	uint capacity = parent.children.capacity;
	
	parent.children.append(std::move(child));

	if (capacity == parent.children.capacity) { //NO RESIZE
		lister.by_id[child.id] = &parent.children.last();
	}
	else { //RESIZE MEANING POINTERS ARE INVALIDATED!
		for (EntityNode& children : parent.children) {
			lister.by_id[children.id] = &children;
		}
	}
}

EntityNode remove_child(Lister& lister, EntityNode& parent, ID id) {
	EntityNode* node = lister.by_id[id];
	assert(node);

	uint position = node - parent.children.data;
	assert(position < parent.children.length);

	EntityNode moved = std::move(parent.children[position]);

	for (uint i = position; i + 1 < parent.children.length; i++) {
		EntityNode& insert_at = parent.children[i];
		insert_at = std::move(parent.children[i + 1]);
		lister.by_id[insert_at.id] = &insert_at;
	}
	parent.children.length--;

	return moved;
}

void register_entity(Lister& lister, string_view name, ID id) {
	add_child(lister, lister.root_node, { name, id });
}

/*
void get_hierarchy(vector<EntityEditor*>& active_named, World& world, vector<NameHierarchy*>& top, std::unordered_map<ID, NameHierarchy*> names) {
	for (auto name : world.filter<EntityEditor>()) {
		names[world.id_of(name)] = TEMPORARY_ALLOC(NameHierarchy, name->name );
		names[world.id_of(name)]->children.allocator = &temporary_allocator;
	}

	for (auto name : active_named) {
		ID id = world.id_of(name);
		NameHierarchy* hierarchy = names[id];
		hierarchy->id = id;
	
		auto local = world.by_id<LocalTransform>(id);
		if (local) {
			auto owner_named = names[local->owner];
			if (local->owner != 0 && owner_named)
				owner_named->children.append(hierarchy);
			else
				top.append(hierarchy);
		}
		else
			top.append(hierarchy);
	}
}*/

void create_object_popup(Lister& lister, World& world, material_handle default_material) {
	if (ImGui::IsWindowHovered()) {
		if (ImGui::GetIO().MouseClicked[1]) ImGui::OpenPopup("CreateObject");
	}

	if (ImGui::BeginPopup("CreateObject")) {
		if (ImGui::MenuItem("New Terrain")) { //todo handle undos
			auto [e,trans,terrain,materials] = world.make<Transform, Terrain, Materials>();
			materials.materials.append(default_material);
			
			register_entity(lister, "Terrain", e.id);
		}

		if (ImGui::MenuItem("New Grass")) {
			auto[e, trans, grass, materials] = world.make<Transform, Grass, Materials>();
			materials.materials.append(default_material);

			register_entity(lister, "Grass", e.id);
		}

		if (ImGui::MenuItem("New Empty")) {
			Entity e = world.make();

			register_entity(lister, "Empty", e.id);
		}

		ImGui::EndPopup();
	}
}


void Lister::render(World& world, Editor& editor, RenderPass& params) {
	if (ImGui::Begin("Lister")) {
		ImGui::InputText("filter", filter);

		EntityNode* filter_root = &root_node;
		EntityFilter entity_filter;
		entity_filter.filter = filter;

		create_object_popup(*this, world, editor.asset_tab.default_material);

		if (filter.starts_with("#")) {
			auto splice = filter.sub(1, filter.size());

			ID id;
			if (!string_to_uint(splice, &id) || (id < 0 || id >= MAX_ENTITIES)) {
				ImGui::Text("Please enter a valid ID");
			}

			EntityNode* e = by_id[id]; 
			if (e) filter_root = e;
			else ImGui::Text("Gameobject with id not found");
		} 
		else if (filter.starts_with(":")) {
			auto name = filter.sub(1, filter.size());
			bool found = false;

			for (int i = 0; i < MAX_COMPONENTS; i++) {
				refl::Struct* comp_type = world.component_type[i];

				if (comp_type != NULL && comp_type->name != name) {
					entity_filter.archetype = 1 << i;
					found = true;
					break;
				}
			}

			if (!found) {
				ImGui::Text("No such component");
			}
		}

		EntityNode result;
		if (filter_hierarchy(&result, *filter_root, world, entity_filter)) {
			for (EntityNode& node : filter_root->children) {
				render_hierarchy(node, editor);
			}
		}

	}

	ImGui::End();
}