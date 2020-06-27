#pragma once

//
// Created by Lucas  Goetz on 05/06/20.
//

#pragma once

#include "core/core.h"
#include "core/container/hash_map.h"
#include "core/container/tuple.h"

template<typename T>
constexpr int type_id();

namespace ecs {

	constexpr uint MAX_COMPONENTS = 64;
	constexpr uint MAX_ENTITIES = 10000;
	constexpr uint ARCHETYPE_HASH = 103;
	constexpr uint BLOCK_SIZE = kb(8);
	constexpr uint WORLD_SIZE = mb(50);
	using Archetype = u64;

	struct Entity {
		uint id;
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

	template<typename... T>
	Archetype to_archetype() {
		return ((1 << type_id<T>()) | ...);
	}


	struct World {
		Archetype id_to_arch[MAX_COMPONENTS];
		void* id_to_ptr[MAX_COMPONENTS][MAX_ENTITIES];
		hash_map<Archetype, ArchetypeStore, 103> arches;

		u64 component_size[MAX_COMPONENTS];

		u8* world_memory = NULL;
		u64 world_memory_offset = 0;
		u64 world_memory_size;

		BlockHeader* block_free_list = NULL;

		//todo recycle entities
		u64 entity_count;

		void clear() {
			world_memory_offset = 0;
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
				if (arch & (1 << i)) combined_size += component_size[i];
			}

			u64 max_entities_per_block = (BLOCK_SIZE - sizeof(BlockHeader)) / combined_size;

			//SOA layout
			u64 offset = 0;
			for (uint i = 0; i < MAX_COMPONENTS; i++) {
				if (arch & (1 << i)) {
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
			u64 id = type_id<Component>();
			u64 byte_offset = offsets[id] + sizeof(Component) * offset;
			return (Component*)(data + byte_offset);
		}

		//todo remove duplication
		template<typename Component>
		ref_tuple<Component> get_component(u8* data, uint* offsets, uint offset) {
			return *get_component_ptr<Component>(data,offsets,offset);
		}

		template<typename Component>
		ref_tuple<Component> init_component(u8* data, uint* offsets, uint offset, Entity e) {
			Component* comp = get_component_ptr<Component>(data, offsets, offset);
			*comp = {};

			id_to_ptr[type_id<Component>()][e.id] = comp;

			return *comp;
		}

		template<typename... Args>
		auto make() {
			Entity e{ make_id() };

			Archetype arch = to_archetype<Args...>();
			ArchetypeStore& store = find_archetype(arch);

			BlockHeader* header = store.blocks + 1;
			u8* data = (u8*)(header + 1); //skip header

			uint offset = store.entity_count_last_block++;
			if (offset >= store.max_per_block) {
				add_block(store);
				offset = 0;
			}

			id_to_arch[e.id] = arch;

			return (init_component<Args>(data, store.offsets, offset, e) + ...);
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

			void skip() {
				while (store_index < ARCHETYPE_HASH 
					&& (world.arches.keys.keys[store_index] & arch) != arch
					&& !world.arches.values[store_index].blocks) {
					store_index++;
				}

				if (store_index < ARCHETYPE_HASH) {
					store = &world.arches.values[store_index];
					block = store->blocks;
				}
			}

			void operator++() {
				bool is_last_block = store->blocks == block;
				uint amount = is_last_block ? store->entity_count_last_block : store->max_per_block;

				if (entity_index >= amount) {
					block = block->next;
					entity_index = 0;

					if (!block) {
						store_index++;
						skip();
					}
				}
				else {
					entity_index++;
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
		ComponentFilter<Args...> filter() {
			Archetype arch = to_archetype<Args...>();
			return ComponentFilter<Args...>(*this, arch);
		}
	};
}