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

struct AddChild {
	ID parent;
	ID child;
};

void render_hierarchy(tvector<AddChild>& defer_add_child, EntityNode& node, Editor& editor, int indent = 0) {
	EntityNode* real = editor.lister.by_id[node.id];

	ID id = node.id;
	bool selected = editor.selected_id == node.id;

	ImGuiStyle* style = &ImGui::GetStyle();

	ImGui::PushID(id);

	ImVec4 bg_color = selected ? style->Colors[ImGuiCol_ButtonActive] : style->Colors[ImGuiCol_WindowBg];
	bool has_children = node.children.length > 0;

	if (!has_children) {
		ImGui::PushStyleColor(ImGuiCol_Button, bg_color);
		ImGui::Button(node.name.data);
	} else {
		ImGui::PushStyleColor(ImGuiCol_Header, bg_color);
		ImGui::PushStyleColor(ImGuiCol_HeaderActive, bg_color);
		ImGui::PushStyleColor(ImGuiCol_HeaderHovered, style->Colors[ImGuiCol_ButtonHovered]);

		ImGui::SetNextTreeNodeOpen(node.expanded);
		bool expanded = ImGui::TreeNodeEx(node.name.data, (selected ? ImGuiTreeNodeFlags_Selected : 0) | ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick);
		if (expanded != node.expanded) { 
			//not sure what behaviour we want here, filter scene hierarchy should expand the node
			//so that the relevant search result is visible
			//however should it remain open after changing the search result 
			node.expanded = expanded;
			real->expanded = expanded;
		}
	}

	if (ImGui::IsItemClicked()) editor.select(id);

	if (ImGui::BeginDragDropSource()) {
		ImGui::Text(node.name.data);
		//Could also cast u64 to a pointer
		ImGui::SetDragDropPayload("DRAG_AND_DROP_ENTITY", real, sizeof(EntityNode));
		ImGui::EndDragDropSource();
	}
	if (ImGui::BeginDragDropTarget()) {
		if (ImGui::AcceptDragDropPayload("DRAG_AND_DROP_ENTITY")) {
			EntityNode* child = (EntityNode*)ImGui::GetDragDropPayload()->Data; 
			defer_add_child.append(AddChild{ id, child->id });
		}
		ImGui::EndDragDropTarget();
	}


	if (node.expanded && has_children) {
		for (EntityNode& child : node.children) {
			render_hierarchy(defer_add_child, child, editor, indent + 30);
		}
		ImGui::TreePop();
	}

	ImGui::PopStyleColor(has_children ? 3 : 1);
	ImGui::PopID();
}

struct EntityFilter {
	string_view filter = "";
	Archetype archetype = 0;
};

bool filter_hierarchy(EntityNode* result, EntityNode& top, World& world, EntityFilter& filter) {
	Archetype arch = world.arch_of_id(top.id);
	if (arch == 0) return false;

	bool has_archetype = (arch & filter.archetype) == filter.archetype;
	bool name_meets_filter = string_view(top.name).starts_with_ignore_case(filter.filter);
	bool empty_filter = filter.filter.length == 0;
	
	*result = {};
	result->id = top.id;
	result->name = top.name;
	result->expanded = top.expanded;
	result->children.allocator = &temporary_allocator;

	for (EntityNode& child : top.children) {
		EntityNode node;
		if (filter_hierarchy(&node, child, world, filter)) {
			result->children.append(node);
			if (!empty_filter) result->expanded = true;
		}
	}

	return result->children.length > 0 || (has_archetype && name_meets_filter);
}

void remove_child(Lister& lister, EntityNode* child) {
	EntityNode* parent = lister.by_id[child->parent];

	uint index = child - parent->children.data; //todo merge this function with remove folder
	for (uint i = index; i < parent->children.length - 1; i++) {
		parent->children[i] = std::move(parent->children[i + 1]);
		lister.by_id[parent->children[i].id] = parent->children.data + i;
	}

	parent->children.length--;
}

void add_child(Lister& lister, EntityNode& parent, EntityNode&& child) {
	uint capacity = parent.children.capacity;
	child.parent = parent.id;
	parent.expanded = true;
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

void add_child(Lister& lister, World& world, AddChild add) {
	const Transform* transform = world.by_id<Transform>(add.parent);
	if (transform) {
		const Transform* child_transform = world.by_id<Transform>(add.child);
		LocalTransform* child_local = world.m_by_id<LocalTransform>(add.child);

		if (child_transform) {
			if (!child_local) child_local = world.add<LocalTransform>(add.child);
			
			//todo move function into transforms_components
			child_local->scale = child_transform->scale / transform->scale;
			child_local->rotation = child_transform->rotation * glm::inverse(transform->rotation);

			auto position = child_transform->position - transform->position; 
			child_local->position = position * glm::inverse(transform->rotation);

			child_local->owner = add.parent;
		}
	}

	EntityNode* child_ptr = lister.by_id[add.child];
	EntityNode child = std::move(*child_ptr);
	remove_child(lister, child_ptr);

	EntityNode* new_parent = lister.by_id[add.parent];
	add_child(lister, *new_parent, std::move(child));
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

		create_object_popup(*this, world, editor.asset_info.default_material);

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

		tvector<AddChild> deferred_add_child; //is it even necessary to defer movement of nodes

		EntityNode result;
		if (filter_hierarchy(&result, *filter_root, world, entity_filter)) {
			for (EntityNode& node : result.children) {
				render_hierarchy(deferred_add_child, node, editor);
			}
		}

		for (AddChild& add : deferred_add_child) {
			add_child(*this, world, add);
		}
	}

	ImGui::End();
}