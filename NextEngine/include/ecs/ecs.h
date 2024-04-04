#pragma once

#include "engine/core.h"
#include "id.h"
#include "component_ids.h" //local components
#include "ecs/component_ids.h" //engine components
#include "core/container/hash_map.h"
#include "core/container/tuple.h"
#include "core/container/tvector.h"
#include "core/container/array.h"
#include "core/container/slice.h"

COMP
struct Entity {
	ID id;
};

struct BlockHeader {
	BlockHeader* next;
	EntityFlags flags;
};

REFL
struct ArchetypeStore {
	uint offsets[64];
	uint dirty_hierarchy_offset[3];
	uint dirty_hierarchy_count[3];
	uint block_count;
	uint max_per_block;
	uint entity_count_last_block;
	REFL_FALSE BlockHeader* blocks;
};

//todo reflection parser isn't able to find constant's yet, offsets should [MAX_COMPONENTS]

struct ComponentPtr {
	uint component_id;
	void* data;
};

template<typename... T>
Archetype to_archetype(EntityFlags flags = 0) {
	return ((1ull | flags) | ... | (1ull << type_id<T>()));
}

template<typename... Args>
EntityQuery EntityQuery::with_all(EntityFlags flags) {
	return { (all | to_archetype<Args...>(flags)) & (~1ull), some, none };
}

template<typename... Args>
EntityQuery EntityQuery::with_none(EntityFlags flags) {
	return { all, some, none | (to_archetype<Args...>(flags) & ~1ull) };
}

template<typename... Args>
EntityQuery EntityQuery::with_some(EntityFlags flags) {
	return { all, some | (to_archetype<Args...>(flags) & ~1ull), none };
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

struct SerializerBuffer;
struct DeserializerBuffer;

using DirtyComponentHierarchyMask = uint;
const DirtyComponentHierarchyMask DIRTY_COMPONENT_HIERARCHY = 1 << 0;
const DirtyComponentHierarchyMask ADDED_COMPONENT_HIERARCHY = 1 << 1;
const DirtyComponentHierarchyMask REMOVED_COMPONENT_HIERARCHY = 1 << 2;

enum ComponentKind {
	REGULAR_COMPONENT,
	FLAG_COMPONENT,
	SYSTEM_COMPONENT
};

namespace refl {
	struct Struct;
    struct Type;
}

struct ComponentLifetimeFunc {
	using ConstructorFunc = void(*)(void*, uint count);
	using DestructorFunc = void(*)(void*, uint count);
	using CopyFunc = void(*)(void*, void*, uint count);
	using SerializeFunc = void(*)(SerializerBuffer&, void*, uint count);
	using DeserializeFunc = void(*)(DeserializerBuffer&, void*, uint count);

	ConstructorFunc constructor;
	DestructorFunc destructor;
	CopyFunc copy;
	SerializeFunc serialize;
	DeserializeFunc deserialize;
};

struct RegisterComponent {
    uint component_id;
    refl::Type* type;
    ComponentLifetimeFunc funcs;
	ComponentKind kind;
};

struct World {
	Archetype id_to_arch[MAX_ENTITIES] = {};
	void* id_to_ptr[MAX_COMPONENTS][MAX_ENTITIES] = {};
	uint dirty_components[MAX_COMPONENTS][MAX_ENTITIES / 16];
	hash_map<Archetype, ArchetypeStore, ARCHETYPE_HASH> arches;

	refl::Struct* component_type[MAX_COMPONENTS] = {};
	u64 component_size[MAX_COMPONENTS] = {};
	ComponentLifetimeFunc component_lifetime_funcs[MAX_COMPONENTS] = {};
	ComponentKind component_kind[MAX_COMPONENTS] = {};
	Archetype system_component_mask = 0;

	BlockHeader* block_free_list = NULL;

	array<MAX_ENTITIES, ID> free_ids;

    Allocator* allocator = nullptr;

	World(Allocator* allocator = &default_allocator) : allocator(allocator) {
		for (int i = MAX_ENTITIES - 1; i > 0; i--) {
			free_ids.append(i);
		}
	}

    ~World();
    
    ENGINE_API void register_components(slice<struct RegisterComponent> components);
    ENGINE_API ID clone(ID id);

	refl::Struct* get_type_for(ComponentPtr ptr) {
		return component_type[ptr.component_id];
	}

	World(const World& world) = delete;
    ENGINE_API World& operator=(const World& from);

	BlockHeader* alloc_block() {
        return (BlockHeader*)allocator->allocate(BLOCK_SIZE);
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
        if (block) {
            assert(block != block->next);
            block_free_list = block->next;
        }
		else block = alloc_block();

		block->next = NULL;
		return block;
	}

	void release_block(BlockHeader* header) {
		header->next = block_free_list;
		block_free_list = header;
	}

	uint make_id() {
		return free_ids.pop();
	}

	void add_block(ArchetypeStore& store) {
		BlockHeader* block = get_block();
        assert(store.blocks != block);
		block->next = store.blocks;

		store.blocks = block;
		store.block_count++;
		store.entity_count_last_block = 0;
	}

	ArchetypeStore& make_archetype(Archetype arch) {
		ArchetypeStore& store = arches[arch];
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

		for (uint i = 0; i < MAX_COMPONENTS; i++) { //note: offset of Entity(component_id: 0) is always 0
			if (has_component(arch, i)) {
				store.offsets[i] = offset;
				offset += component_size[i] * max_entities_per_block;
			}
		}

		/*
		uint dirty_bits = max_entities_per_block;

		for (uint i = 0; i < 3; i++) {
			store.dirty_hierarchy_offset[i] = offset;
			store.dirty_hierarchy_count[i] = dirty_bits;
			offset += min(1, dirty_bits / 8);
			dirty_bits = dirty_bits & 1 ? 1 + (dirty_bits / 2) : dirty_bits / 2;
		}*/

		return store;
	}

	ArchetypeStore& find_archetype(Archetype arch) {
		int index = arches.index(arch);

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
	ref_tuple<Entity, Args...> make(EntityFlags flags = 0) {
		ID id = make_id();

		Archetype arch = to_archetype<Args...>(flags);
		ArchetypeStore& store = find_archetype(arch);

		uint offset = store_make_space(store);

		u8* data = last_block_data(store);

		id_to_arch[id] = arch;

		Entity& entity = *(Entity*)(data + sizeof(Entity) * offset);
		entity = {};
		entity.id = id;

		id_to_ptr[0][id] = &entity;


		//printf("MAKING ENTITY WITH ARCHETYPE %i, BASE %p, OFFSET %i, ENTITY %i\n", arch, data, offset, id);

		return ref_tuple<Entity>(entity) + (init_component<Args>(data, store.offsets, offset, id) + ...);
	}

	template<typename T>
	const T* by_id(ID id) const {
		return (T*)id_to_ptr[type_id<T>()][id];
	}

	template<typename T>
	T* m_by_id(ID id) {
		return (T*)id_to_ptr[type_id<T>()][id];
	}

	template<typename T>
	ref_tuple<T> ref_by_id(ID id) {
		return ref_tuple<T>(*(T*)id_to_ptr[type_id<T>()][id]);
	}

	template<typename... Args>
	maybe<ref_tuple<Args...>> get_by_id(ID id) {
		Archetype arch = to_archetype<Args...>();

		if ((id_to_arch[id] & arch) == arch) {
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
            BlockHeader* block = store.blocks;
            BlockHeader* previous_block = block->next;
			if (previous_block) store.entity_count_last_block = store.max_per_block;
				
            release_block(block);

            store.blocks = previous_block;
			assert(store.block_count != 0);
			store.block_count--; /* todo why does this field exist! */
		}
	}

	void emplace_free_component(uint component_id, ID id, ID last_id, bool call_destructor) {
		void* dst = id_to_ptr[component_id][id];
		void* src = id_to_ptr[component_id][last_id];

		auto destructor = component_lifetime_funcs[component_id].destructor;
		if (call_destructor && destructor) destructor(dst, 1);

        id_to_ptr[component_id][id] = 0;
        if (id != last_id) {
            memcpy(dst, src, component_size[component_id]);

            id_to_ptr[component_id][last_id] = dst;
        }
	}

	void emplace_move_component(ArchetypeStore& store, uint component_id, ID id, ID last_id, uint offset, u8* data) {
		uint size = component_size[component_id];
        
        void* ptr = id_to_ptr[component_id][id];
        void* moving_to = data + store.offsets[component_id] + offset * size; //todo turn into function!
        memcpy(moving_to, ptr, size);
        id_to_ptr[component_id][id] = moving_to;
        
        if (id != last_id) {
            void* last_element = id_to_ptr[component_id][last_id];
            memcpy(ptr, last_element, size);
            id_to_ptr[component_id][last_id] = ptr;
        }
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
        
        //printf("Destroying %i, moving %i in it's place\n", id, last_id);

		Archetype common = from & to;
		Archetype removed = from & ~to;
		Archetype added = to & ~from;

		uint offset = new_store ? store_make_space(*new_store) : 0;
		u8* data = new_store ? last_block_data(*new_store) : nullptr;

		if (new_store) {
			assert(new_store->entity_count_last_block != 0);
			//printf("ADDING ENTITY TO ARCHETYPE %ul count: %i\n", to, new_store ? new_store->entity_count_last_block : 0);
		}

		for (uint i = 0; i < MAX_COMPONENTS; i++) {
			Archetype flag = 1ull << i;

			if (flag & common) emplace_move_component(*new_store, i, id, last_id, offset, data);
			else if (flag & removed) emplace_free_component(i, id, last_id, call_lifetime);
			else if (flag & added) {
				//printf("MAKING ENTITY WITH ARCHETYPE %i, BASE %p, ENTITY %i\n", to, data, offset);

				void* moving_to = data + new_store->offsets[i] + offset * component_size[i];
				id_to_ptr[i][id] = moving_to;

				if (call_lifetime) {
					auto constructor = component_lifetime_funcs[i];
					component_lifetime_funcs[i].constructor(moving_to, 1);
				}
			}
		}

		id_to_arch[id] = to;
		if (store) shrink_store_to_fit(*store);
	}

	void free_by_id(ID id, bool call_destructor = true) { //todo handle id not existing!
		Archetype arch = id_to_arch[id];
		assert(arch != 0);
		change_archetype(id, arch, system_component_mask, call_destructor); //keep system components alive
        free_ids.append(id);
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

		((T*)id_to_ptr[delete_component_id][id])->~T();
		emplace_free_component(delete_component_id, id, last_id, false);

		id_to_arch[id] = new_arch;
		shrink_store_to_fit(store);
	}

	template<typename T>
	T* add(ID id) { //todo handle id not existing!
		Archetype arch = id_to_arch[id];
		ArchetypeStore& store = find_archetype(id_to_arch[id]);
		ID last_id = pop_store(store);

		const uint add_component_id = type_id<T>();
		Archetype new_arch = arch | (1ull << add_component_id);
		id_to_arch[id] = new_arch;

		ArchetypeStore& new_store = find_archetype(new_arch);

		emplace_move_components(new_store, arch, id, last_id);
		T* component = get_component_ptr<T>(last_block_data(new_store), new_store.offsets, new_store.entity_count_last_block - 1);
		*component = T();

		id_to_arch[id] = new_arch;
		id_to_ptr[add_component_id][id] = component;
		shrink_store_to_fit(store);

		return component;
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
		EntityQuery query;
		uint store_index;
		uint entity_index = 0;

		ComponentIterator(World& world, EntityQuery query) : world(world), query(query) {}

		ArchetypeStore* store = NULL;
		BlockHeader* block = NULL;
		uint size_of_last_block;
		u8* data;

		void skip_archetype() {
			auto& arches = world.arches;

			while (store_index < ARCHETYPE_HASH) {
				Archetype arch = arches.keys[store_index];
				bool not_empty = arches.values[store_index].blocks;
				
				if (not_empty && (arch & query.all) == query.all && (query.some == 0 || (arch & query.some) != 0) && (arch & query.none) == 0) { // || !world.arches.values[store_index].blocks
					store = &world.arches.values[store_index];
					block = store->blocks;
					size_of_last_block = store->entity_count_last_block;
					data = (u8*)(block + 1);

					break;
				}

				store_index++;
			}
		}

		void next() {
			if (++entity_index >= size_of_last_block) {
				block = block->next;
				data = (u8*)(block + 1);
				size_of_last_block = store->max_per_block;
				entity_index = 0;

				if (!block) {
					store_index++;
					skip_archetype();
				}
			}
		}

		/*void skip_entities() {
			while (store_index < ARCHETYPE_HASH && ((Entity*)data)[entity_index].flags & flags != flags) {
				next();
			}
		}*/

		void operator++() {
			next();
			//skip_entities();
		}

		bool operator==(const ComponentIterator& other) const {
			return store_index == other.store_index;
		}

		bool operator!=(const ComponentIterator& other) const {
			return store_index != other.store_index;
		}

		ref_tuple<Args...> operator*() {
			return (world.get_component<Args>(data, store->offsets, entity_index) + ...);
		}
	};

	template<typename... Args>
	struct ComponentFilter {
		World& world;
		EntityQuery query;

		ComponentFilter(World& world, EntityQuery query) : world(world), query(query) {
			this->query.all |= to_archetype<Args...>();
			assert((query.all & query.none) == 0);
			assert((query.all & query.some) == 0);
			assert((query.some & query.none) == 0);
		}

		ComponentIterator<Args...> begin() {
			ComponentIterator<Args...> it(world, query);
			it.store_index = 0;
			it.skip_archetype();
			//it.skip_entities();
			return it;
		}

		ComponentIterator<Args...> end() {
			ComponentIterator<Args...> it(world, query);
			it.store_index = ARCHETYPE_HASH;
			return it;
		}
	};

	template<typename... Args>
	ComponentFilter<Entity, Args...> filter(EntityQuery query = EntityQuery()) {
		return ComponentFilter<Entity, Args...>(*this, query);
	}

	template<typename... Args>
	maybe<ref_tuple<Entity, Args...>> first(EntityQuery query = EntityQuery()) {
		auto filter = ComponentFilter<Entity, Args...>(*this, query);
		auto begin = filter.begin();
		auto end = filter.end();

		if (begin == end) return maybe<ref_tuple<Entity, Args...>>();
		else return maybe(*begin);
	}

	void begin_frame() {
		for (uint i = 0; i < ARCHETYPE_HASH; i++) {
			if (!arches.is_full(i)) continue;

			ArchetypeStore& store = arches.values[i];
			uint count = store.entity_count_last_block;
			
			/*EntityFlags zero_change_mask = ~(CREATED_THIS_FRAME | MODIFIED_THIS_FRAME | DESTROYED_THIS_FRAME);

			for (BlockHeader* header = store.blocks; header != nullptr; header = header->next) {
				Entity* entities = (Entity*)(header + 1);
				for (uint i = 0; i < count; i++) entities[i].flags &= zero_change_mask;
				count = store.max_per_block;
			}*/
		}
	}
};
