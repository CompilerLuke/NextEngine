#include "UI/ui.h"
#include "editor/lister.h"
#include "editor/selection.h"
#include "ecs/ecs.h"
#include "components/transform.h"
#include "core/memory/linear_allocator.h"
#include "core/io/logger.h"
#include "graphics/rhi/primitives.h"
#include "components.h"
#include "cfd_ids.h"

REFL
struct EntityNode {
	sstring name = "Root";
	ID id = 0;
	bool expanded = false;
	vector<EntityNode> children; //todo optimization for 1-2 children
	int parent = -1;
};

struct Lister {
	World& world;
	Selection& selection;
	UI& ui;

	EntityNode root_node;
	EntityNode* by_id[MAX_ENTITIES];

	string_buffer filter;
};

Lister* make_lister(World& world, Selection& selection, UI& ui) {
	Lister* lister = PERMANENT_ALLOC(Lister, { world, selection, ui });
	lister->by_id[0] = &lister->root_node;
	return lister;
}

void destroy_lister(Lister* lister) {

}

struct AddChild {
	ID parent;
	ID child;
};

EntityNode* node_by_id(Lister& lister, ID id) {
	if (id == 0) return &lister.root_node;
	else return lister.by_id[id];
}

sstring& name_of_entity(Lister& lister, ID id) {
	return node_by_id(lister, id)->name;
}

void render_hierarchy(Lister& lister, UI& ui, EntityNode& node, Selection& selection) {
	EntityNode* real = node_by_id(lister, node.id);
	UITheme& theme = get_ui_theme(ui);

	ID id = node.id;
	bool selected = selection.is_selected(node.id);

	bool& hovered = get_state<bool>(ui);

	StackView& stack_view = begin_hstack(ui)
		.on_click([&selection, id] { selection.select(id); })
		.width({ Perc,100 })
		.on_hover([&](bool hover) { hovered = hover; });
	
	if (hovered) stack_view.background(color4(52, 159, 235, 1));
	if (selected) stack_view.background(theme.color(ThemeColor::ButtonHover));

	bool has_children = node.children.length > 0;

	text(ui, node.name);
	if (has_children) {
		spacer(ui);
		if (node.expanded) {
			text(ui, "^").on_click([=] { real->expanded = false; });
		}
		else {
			text(ui, ">").on_click([=] { real->expanded = true; });
		}
	}

	/*
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
	*/

	end_hstack(ui);
	if (node.expanded && has_children) {
		begin_vstack(ui).padding(0).padding(ELeading, 10);
		
		for (EntityNode& child : node.children) {
			render_hierarchy(lister, ui, child, selection);
		}
		
		end_vstack(ui);
	}
}

struct EntityFilter {
	string_view filter = "";
	Archetype archetype = 0;
};

const ID ROOT_NODE = 0;

bool filter_hierarchy(EntityNode* result, EntityNode& top, World& world, EntityFilter& filter) {
	Archetype arch = world.arch_of_id(top.id);
	if (top.id != ROOT_NODE && arch == 0) return false;

	bool has_archetype = (arch & filter.archetype) == filter.archetype;
	bool name_meets_filter = string_view(top.name).starts_with_ignore_case(filter.filter);
	bool empty_filter = filter.filter.length == 0;

	*result = {};
	result->id = top.id;
	result->name = top.name;
	result->expanded = top.expanded;
	result->children.allocator = &get_temporary_allocator();

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
	EntityNode* parent = node_by_id(lister, child->parent);

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

void add_child(Lister& lister, ID parent, EntityNode&& child_node) {
	World& world = lister.world;
	ID child = child_node.id;

	const Transform* transform = parent == 0 ? nullptr : world.by_id<Transform>(parent);
	if (transform) {
		const Transform* child_transform = world.by_id<Transform>(child);
		LocalTransform* child_local = world.m_by_id<LocalTransform>(child);

		if (child_transform) {
			if (!child_local) child_local = world.add<LocalTransform>(child);

			//todo move function into transforms_components
			child_local->scale = child_transform->scale / transform->scale;
			child_local->rotation = child_transform->rotation * glm::inverse(transform->rotation);

			auto position = child_transform->position - transform->position;
			child_local->position = position * glm::inverse(transform->rotation);

			child_local->owner = parent;
		}
	}

	EntityNode* new_parent = node_by_id(lister, parent);
	add_child(lister, *new_parent, std::move(child_node));
}

void add_child(Lister& lister, ID parent, ID child) {
	EntityNode* child_ptr = lister.by_id[child];
	EntityNode child_node = std::move(*child_ptr);
	remove_child(lister, child_ptr);

	add_child(lister, parent, std::move(child_node));
}

EntityNode remove_child(Lister& lister, EntityNode& parent, ID id) {
	EntityNode* node = node_by_id(lister, id);
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

void create_under_selected(Lister& lister, string_view name, ID id) {
	EntityNode* parent = node_by_id(lister, lister.selection.selected);
	if (!parent) {
		printf("Found no parent!\n");
		return;
	}

	add_child(lister, lister.selection.selected, { name, id });
}

void spawn_primitive(Lister& lister, model_handle primitive, const char* name, material_handle default_material) {
	auto [e, trans, mesh] = lister.world.make<Transform, CFDMesh>();
	mesh.model = primitive;

	create_under_selected(lister, name, e.id);
}

void create_object_popup(Lister& lister) {
	/*
	if (ImGui::IsWindowHovered()) {
		if (ImGui::GetIO().MouseClicked[1]) ImGui::OpenPopup("CreateObject");
	}

	if (ImGui::BeginPopup("CreateObject")) {
		if (ImGui::MenuItem("New Terrain")) { //todo handle undos
			auto [e, trans, terrain, materials] = world.make<Transform, Terrain, Materials>();
			materials.materials.append(default_material);

			create_under_selected(editor, "Terrain", e.id);
		}

		if (ImGui::MenuItem("New Grass")) {
			auto [e, trans, grass, materials] = world.make<Transform, Grass, Materials>();
			materials.materials.append(default_material);

			create_under_selected(editor, "Grass", e.id);
		}

		if (ImGui::MenuItem("New Empty")) {
			auto [e, trans] = world.make<Transform>();

			create_under_selected(editor, "Empty", e.id);
		}

		if (ImGui::MenuItem("New Cube")) spawn_primitive(editor, world, primitives.cube, "Cube", default_material);

		if (ImGui::MenuItem("New Plane")) spawn_primitive(editor, world, primitives.quad, "Plane", default_material);

		if (ImGui::MenuItem("New Sphere")) spawn_primitive(editor, world, primitives.sphere, "Sphere", default_material);

		ImGui::EndPopup();
	}
	*/
}

void clone_entity(Lister& lister, ID parent, EntityNode& node) {
	ID new_id = lister.world.clone(node.id);

	char buffer[sstring::N];
	snprintf(buffer, sstring::N, "%s Copy", node.name.data);
	add_child(lister, parent, { buffer, new_id });

	for (EntityNode child : node.children) {
		clone_entity(lister, new_id, child);
	}
}

void clone_entity(Lister& lister, ID id) {
	EntityNode* node = node_by_id(lister, id);
	if (!node) {
		fprintf(stderr, "ID is not associated with node");
		return;
	}

	clone_entity(lister, node->parent, *node);
}

void render_lister(Lister& lister) {
	UI& ui = lister.ui;

	begin_vstack(ui);
	begin_hstack(ui);
		text(ui, "Filter");
		input(ui, &lister.filter).flex_h();
	end_hstack(ui);

	string_view filter = lister.filter;

	EntityNode* filter_root = &lister.root_node;
	EntityFilter entity_filter;
	entity_filter.filter = filter;

	create_object_popup(lister);

	if (filter.starts_with("#")) {
		auto splice = filter.sub(1, filter.size());

		ID id;
		if (!string_to_uint(splice, &id) || (id < 0 || id >= MAX_ENTITIES)) {
			text(ui, "Please enter a valid ID");
		}

		EntityNode* e = lister.by_id[id];
		if (e) filter_root = e;
		else text(ui, "Gameobject with id not found");
	}
	else if (filter.starts_with(":")) {
		auto name = filter.sub(1, filter.size());
		bool found = false;

		for (int i = 0; i < MAX_COMPONENTS; i++) {
			refl::Struct* comp_type = lister.world.component_type[i];

			if (comp_type != NULL && comp_type->name != name) {
				entity_filter.archetype = 1 << i;
				found = true;
				break;
			}
		}

		if (!found) {
			text(ui, "No such component");
		}
	}

	tvector<AddChild> deferred_add_child; //is it even necessary to defer movement of nodes

	EntityNode result;
	if (filter_hierarchy(&result, *filter_root, lister.world, entity_filter)) {
		for (EntityNode& node : result.children) {
			render_hierarchy(lister, ui, node, lister.selection);
		}
	}
	
	end_vstack(ui);
}
