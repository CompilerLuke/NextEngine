#pragma once

//
// Created by Lucas  Goetz on 05/06/20.
//

#pragma once

#include "core/core.h"
#include "core/container/hash_map.h"
#include "core/container/tuple.h"
#include "ecs/id.h"

template<typename T>
constexpr int type_id();

COMP
struct Entity {
	uint id;
	Layermask layermask;
};

struct BlockHeader {
	BlockHeader* next;
};

struct ArchetypeStore {
	uint offsets[MAX_COMPONENTS];
	uint block_count;
	uint max_per_block;
	uint entity_count_last_block;
	BlockHeader* blocks;
};

struct ComponentPtr {
	uint component_id;
	void* data;
};

template<typename... T>
Archetype to_archetype() {
	return (1ull | ... | (1ull << type_id<T>()) );
}

template<typename T>
struct maybe {
	bool some = false;
	char buffer[sizeof(T)];

	maybe(T&& value) : some(true) {
		new (buffer) T(value);
	}

	maybe() : some(false) {}

	T operator*() {
		assert(some);
		return *(T*)buffer;
	}

	T* operator->() {
		assert(some);
		return (T*)buffer;
	}

	operator bool() { return some; }
};

namespace refl {
	struct Struct;
}

struct ComponentLifetimeFunc {
	using ConstructorFunc = void(*)(void*);
	using DestructorFunc = void(*)(void*);
	using CopyFunc = void(*)(void*,void*);

	ConstructorFunc constructor;
	DestructorFunc destructor;
	CopyFunc copy;
};

struct World {
	Archetype id_to_arch[MAX_COMPONENTS] = {};
	void* id_to_ptr[MAX_COMPONENTS][MAX_ENTITIES] = {};
	hash_map<Archetype, ArchetypeStore, 103> arches;

	refl::Struct* component_type[MAX_COMPONENTS] = {};
	u64 component_size[MAX_COMPONENTS] = {};
	ComponentLifetimeFunc component_lifetime_funcs[MAX_COMPONENTS] = {};

	u8* world_memory = NULL;
	u64 world_memory_offset = 0;
	u64 world_memory_size;

	BlockHeader* block_free_list = NULL;

	//todo recycle entities
	u64 entity_count = 0;

	World(u64 memory) : world_memory(new u8[memory]), world_memory_size(memory) {
	}

	void clear() {
		world_memory_offset = 0;
	}

	refl::Struct* get_type_for(ComponentPtr ptr) {
		return component_type[ptr.component_id];
	}

	template<typename T>
	void add_component() {
		int component_id = type_id<T>();

		component_type[component_id] = (refl::Struct*)refl_type(T);
		component_size[component_id] = sizeof(T);

		component_lifetime_funcs[component_id].constructor = [](void* ptr) { new (ptr) T(); };

		if constexpr (!std::is_trivially_copyable<T>::value) {
			component_lifetime_funcs[component_id].copy = [](void* dst, void* src) { new (dst) T(*(T*)src); };
			component_lifetime_funcs[component_id].destructor = [](void* ptr) { ((T*)ptr)->~T(); };
		}
	}

	BlockHeader* alloc_block() {
		BlockHeader* block = (BlockHeader*)(world_memory + world_memory_offset);
		world_memory_offset += BLOCK_SIZE;
		return block;
	}

	void prealloc_blocks(uint n) {
		for (uint i = 0; i < n; i++) {
			//todo allocate large blocks
			BlockHeader* block = alloc_block();
			block->next = block_free_list;
			block_free_list = block;
		}
	}


	BlockHeader* get_block() {
		BlockHeader* block = block_free_list;
		if (block) block_free_list = block->next;
		else block = alloc_block();

		block->next = NULL;
		return block;
	}

	void release_block(BlockHeader* header) {
		header->next = block_free_list;
		block_free_list = header;
	}

	uint make_id() {
		return entity_count++;
	}

	void add_block(ArchetypeStore& store) {
		BlockHeader* block = get_block();
		block->next = store.blocks;

		store.blocks = block;
		store.block_count++;
		store.entity_count_last_block = 0;
	}

	ArchetypeStore& make_archetype(Archetype arch) {
		int index = arches.keys.add(arch);

		ArchetypeStore& store = arches.values[index];
		store = {};

		add_block(store);

		u64 combined_size = 0;
		for (uint i = 0; i < MAX_COMPONENTS; i++) {
			if (arch & (1ull << i)) combined_size += component_size[i];
		}

		u64 max_entities_per_block = (BLOCK_SIZE - sizeof(BlockHeader)) / combined_size;
		store.max_per_block = max_entities_per_block;

		//SOA layout, todo handle alignment!
		u64 offset = 0;
		for (uint i = 0; i < MAX_COMPONENTS; i++) {
			if (arch & (1ull << i)) {
				store.offsets[i] = offset;
				offset += component_size[i] * max_entities_per_block;
			}
		}

		return store;
	}

	ArchetypeStore& find_archetype(Archetype arch) {
		int index = arches.keys.index(arch);

		if (index == -1) return make_archetype(arch);
		else return arches.values[index];
	}

	template<typename Component>
	Component* get_component_ptr(u8* data, uint* offsets, uint offset) {
		return (Component*)(data + offsets[type_id<Component>()]) + offset;
	}

	template<typename Component>
	ref_tuple<Component> get_component(u8* data, uint* offsets, uint offset) {
		return ((Component*)(data + offsets[type_id<Component>()]))[offset];
	}

	template<typename Component>
	ref_tuple<Component> init_component(u8* data, uint* offsets, uint offset, ID id) {
		Component* comp = get_component_ptr<Component>(data, offsets, offset);
		new (comp) Component();

		id_to_ptr[type_id<Component>()][id] = comp;

		return *comp;
	}

	uint store_make_space(ArchetypeStore& store) {
		uint offset = store.entity_count_last_block++;
		if (!store.blocks || offset >= store.max_per_block) {
			add_block(store);
			offset = 0;
			store.entity_count_last_block = 1;
		}

		return offset;
	}

	u8* last_block_data(ArchetypeStore& store) {
		return (u8*)(store.blocks + 1);
	}

	Entity& make(Archetype arch = 1) {
		ID id = make_id();

		ArchetypeStore& store = find_archetype(arch);

		uint offset = store_make_space(store);

		u8* data = last_block_data(store); //skip header

		id_to_arch[id] = arch;

		Entity& entity = *(Entity*)(data + offset * sizeof(Entity));
		entity = {};
		entity.id = id;

		id_to_ptr[0][id] = &entity;

		return entity;
	}

	//todo remove duplication!
	template<typename... Args>
	ref_tuple<Entity, Args...> make() {
		ID id = make_id();

		Archetype arch = to_archetype<Args...>();
		ArchetypeStore& store = find_archetype(arch);

		uint offset = store_make_space(store);

		u8* data = last_block_data(store); 

		id_to_arch[id] = arch;

		Entity& entity = *(Entity*)(data + sizeof(Entity) * offset);
		entity = {};
		entity.id = id;

		id_to_ptr[0][id] = &entity;


		printf("MAKING ENTITY WITH ARCHETYPE %i, BASE %p, ENTITY %i\n", arch, data, offset);

		return ref_tuple<Entity>(entity) + (init_component<Args>(data, store.offsets, offset, id) + ...);
	}

	template<typename T>
	T* by_id(ID id) {
		return (T*)id_to_ptr[type_id<T>()][id];
	}

	template<typename T>
	ref_tuple<T> ref_by_id(ID id) {
		return ref_tuple<T>(*(T*)id_to_ptr[type_id<T>()][id]);
	}

	template<typename... Args>
	maybe<ref_tuple<Args...>> get_by_id(ID id) {
		Archetype arch = to_archetype<Args...>();

		if (id_to_arch[id] & arch == arch) {
			return maybe((ref_by_id<Args>(id) + ...));
		}
		else {
			return maybe<ref_tuple<Args...>>();
		}
	}

	Archetype arch_of_id(ID id) {
		return id_to_arch[id];
	}

	void free_now_by_id(ID id) {

	}

	ID pop_store(ArchetypeStore& store) {
		Entity* data = (Entity*)(store.blocks + 1);
		assert(store.entity_count_last_block != 0);
		uint last_offset = --store.entity_count_last_block;

		return data[last_offset].id;
	}

	void shrink_store_to_fit(ArchetypeStore& store) {
		if (store.entity_count_last_block == 0) {
			if (store.blocks = store.blocks->next) {
				store.entity_count_last_block = store.max_per_block;
				release_block(store.blocks);
			}

			store.block_count--; /* todo why does this field exist! */
		}
	}

	void emplace_free_component(uint component_id, ID id, ID last_id, bool call_destructor) {
		void* dst = id_to_ptr[component_id][id];
		void* src = id_to_ptr[component_id][last_id];

		auto destructor = component_lifetime_funcs[component_id].destructor;
		if (call_destructor && destructor) destructor(dst);

		memcpy(dst, src, component_size[component_id]);

		id_to_ptr[component_id][id] = 0;
		id_to_ptr[component_id][last_id] = dst;
	}

	void emplace_move_component(ArchetypeStore& store, uint component_id, ID id, ID last_id, uint offset, u8* data) {
		uint size = component_size[component_id];

		void* ptr = id_to_ptr[component_id][id];
		void* last_element = id_to_ptr[component_id][last_id];
		void* moving_to = data + store.offsets[component_id] + offset * size; //todo turn into function!
		memcpy(moving_to, ptr, size);
		memcpy(ptr, last_element, size);

		id_to_ptr[component_id][id] = moving_to;
		id_to_ptr[component_id][last_id] = ptr;
	}

	void emplace_move_components(ArchetypeStore& store, Archetype arch, ID id, ID last_id) {
		uint offset = store_make_space(store);
		u8* data = last_block_data(store);
		
		for (uint component_id = 0; component_id < MAX_COMPONENTS; component_id++) {
			if ((1ull << component_id) & arch) emplace_move_component(store, component_id, id, last_id, offset, data);
		}
	}

	//todo split into various edge cases: create all, delete all
	void change_archetype(ID id, Archetype from, Archetype to, bool call_lifetime = true) {
		ArchetypeStore* store = from > 0 ? &find_archetype(from) : nullptr;
		ArchetypeStore* new_store = to > 0 ? &find_archetype(to) : nullptr;
		ID last_id = store ? pop_store(*store) : 0;

		Archetype common = from & to;
		Archetype removed = from & ~to;
		Archetype added = to & ~from;

		uint offset = new_store ? store_make_space(*new_store) : 0;
		u8* data = new_store ? last_block_data(*new_store) : nullptr;

		if (new_store) {
			assert(new_store->entity_count_last_block != 0);
			printf("ADDING ENTITY TO ARCHETYPE %ul count: %i\n", to, new_store ? new_store->entity_count_last_block : 0);
		}


		for (uint i = 0; i < MAX_COMPONENTS; i++) {
			Archetype flag = 1ull << i;

			if (flag & common) emplace_move_component(*new_store, i, id, last_id, offset, data);
			else if (flag & removed) emplace_free_component(i, id, last_id, call_lifetime);
			else if (flag & added) {
				printf("MAKING ENTITY WITH ARCHETYPE %i, BASE %p, ENTITY %i\n", to, data, offset);

				void* moving_to = data + new_store->offsets[i] + offset * component_size[i];
				id_to_ptr[i][id] = moving_to;

				if (call_lifetime) {
					auto constructor = component_lifetime_funcs[i];
					component_lifetime_funcs[i].constructor(moving_to);
				}
			}
		}

		id_to_arch[id] = to;
		if (store) shrink_store_to_fit(*store);
	}

	void free_by_id(ID id, bool call_destructor = true) { //todo handle id not existing!
		Archetype arch = id_to_arch[id];
		change_archetype(id, arch, 0, call_destructor);
	}

	template<typename T>
	void free_by_id(ID id) { //todo handle id not existing!		
		Archetype arch = id_to_arch[id];
		ArchetypeStore& store = find_archetype(id_to_arch[id]);
		ID last_id = pop_store(store);
		
		const uint delete_component_id = type_id<T>();
		Archetype new_arch = arch & ~(1ull << delete_component_id);
		id_to_arch[id] = new_arch;

		emplace_move_components(find_archetype(new_arch), new_arch, id, last_id);

		(T*)(id_to_ptr[delete_component_id][id])->~T();
		emplace_free_component(delete_component_id, id, last_id, false);
		
		id_to_arch[id] = new_arch;
		shrink_store_to_fit(store);
	}

	template<typename T>
	void add(ID id) { //todo handle id not existing!
		Archetype arch = id_to_arch[id];
		ArchetypeStore& store = find_archetype(id_to_arch[id]);
		ID last_id = pop_store(store);

		const uint add_component_id = type_id<T>();
		Archetype new_arch = arch | (1ull << add_component_id);
		id_to_arch[id] = new_arch;

		ArchetypeStore& new_store = find_archetype(new_arch);

		emplace_move_components(new_store, arch, id, last_id);
		*get_component_ptr<T>(last_block_data(new_store), new_store.offsets, new_store.entity_count_last_block - 1) = T();
		
		id_to_arch[id] = new_arch;
		shrink_store_to_fit(store);
	}

	tvector<ComponentPtr> components_by_id(ID id, Archetype arch = ~0) {
		arch &= arch_of_id(id);

		tvector<ComponentPtr> components;

		for (uint i = 0; i < MAX_COMPONENTS; i++) {
			if (((1ull << i) & arch) == 0) continue;

			components.append({ i, id_to_ptr[i][id] });
		}

		return components;
	}

	template<typename... Args>
	struct ComponentIterator {
		World& world;
		Archetype arch;
		uint store_index;
		uint entity_index = 0;

		ComponentIterator(World& world, Archetype arch) : world(world), arch(arch) {}

		ArchetypeStore* store = NULL;
		BlockHeader* block = NULL;
		uint size_of_last_block;

		/*void get() {
			template<typename Component>
			ref_tuple<Component> get_component(u8* data, uint* offsets, uint offset) {
				return ((Component*)(data + offsets[type_id<Component>()]))[offset];
			}
		}*/

		void skip() {
			auto& arches = world.arches;

			while (store_index < ARCHETYPE_HASH) {
				if ((arches.keys.keys[store_index] & arch) == arch && arches.values[store_index].blocks) { // || !world.arches.values[store_index].blocks
					store = &world.arches.values[store_index];
					block = store->blocks;
					size_of_last_block = store->entity_count_last_block;



					break;
				}

				store_index++;
			}
		}

		void operator++() {
			if (++entity_index >= size_of_last_block) {
				block = block->next;
				size_of_last_block = store->max_per_block;
				entity_index = 0;

				if (!block) {
					store_index++;
					skip();
				}
			}
		}

		bool operator==(ComponentIterator& other) {
			return store_index == other.store_index;
		}

		bool operator!=(ComponentIterator& other) {
			return store_index != other.store_index;
		}

		ref_tuple<Args...> operator*() {
			u8* data = (u8*)(block + 1);

			//printf("GETTING ENTITY WITH ARCHETYPE %i, BASE %p, ENTITY %i", arch, data, entity_index);
			return (world.get_component<Args>(data, store->offsets, entity_index) + ...);
		}
	};

	template<typename... Args>
	struct ComponentFilter {
		World& world;
		Archetype arch;

		ComponentFilter(World& world, Archetype arch) : world(world), arch(arch) {}

		ComponentIterator<Args...> begin() {
			ComponentIterator<Args...> it(world, arch);
			it.store_index = 0;
			it.skip();
			return it;
		}

		ComponentIterator<Args...> end() {
			ComponentIterator<Args...> it(world, arch);
			it.store_index = ARCHETYPE_HASH;
			return it;
		}
	};

	template<typename... Args>
	ComponentFilter<Entity, Args...> filter(Layermask layermask = ANY_LAYER) {
		Archetype arch = to_archetype<Args...>();
		return ComponentFilter<Entity, Args...>(*this, arch);
	}

	template<typename... Args>
	maybe<ref_tuple<Entity, Args...>> first(Layermask layermask = ANY_LAYER) {
		Archetype arch = to_archetype<Args...>();
		auto filter = ComponentFilter<Entity, Args...>(*this, arch);
		auto begin = filter.begin();
		auto end = filter.end();

		if (begin == end) return maybe<ref_tuple<Entity, Args...>>();
		else return maybe(*begin);
	}

	void begin_frame() {

	}
};

// COMPONENT ID!

#include "system.h"

DEFINE_COMPONENT_ID(Entity, 0)
DEFINE_COMPONENT_ID(Transform, 1)
DEFINE_COMPONENT_ID(LocalTransform, 22)
DEFINE_COMPONENT_ID(Materials, 3)
DEFINE_COMPONENT_ID(ModelRenderer, 4)
DEFINE_COMPONENT_ID(Camera, 5)
DEFINE_COMPONENT_ID(Flyover, 6)
DEFINE_COMPONENT_ID(DirLight, 7)
DEFINE_COMPONENT_ID(Skybox, 8)
DEFINE_COMPONENT_ID(CapsuleCollider, 9)
DEFINE_COMPONENT_ID(SphereCollider, 10)
DEFINE_COMPONENT_ID(BoxCollider, 11)
DEFINE_COMPONENT_ID(PlaneCollider, 12)
DEFINE_COMPONENT_ID(RigidBody, 13)
DEFINE_COMPONENT_ID(StaticTransform, 14)
DEFINE_COMPONENT_ID(Terrain, 16)
DEFINE_COMPONENT_ID(TerrainControlPoint, 17)
DEFINE_COMPONENT_ID(Grass, 18)
DEFINE_COMPONENT_ID(CharacterController, 19)
DEFINE_COMPONENT_ID(PointLight, 20)
DEFINE_COMPONENT_ID(SkyLight, 21)