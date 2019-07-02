#pragma once

#include <algorithm>
#include "reflection.h"
#include "id.h"
#include "layermask.h"
#include "vfs.h"
#include "system.h"
#include "vector.h"

extern int global_type_id;
using typeid_t = int;

template <typename T>
constexpr typeid_t type_id() noexcept
{
	static int const type_id = global_type_id++;
	return type_id;
}

struct Entity {
	bool enabled = true;
	Layermask layermask = game_layer | picking_layer;

	REFLECT()
};

struct World {
	using ArchetypeID = int;

	static constexpr int num_archetypes = 103;
	static constexpr int num_components = sizeof(ArchetypeID) * 8;
	static constexpr int block_size = 16000;
	static constexpr int num_entities = 1000;

	std::size_t size_of_components[num_components];

	std::size_t size_of(std::size_t size_of_component) {
		return sizeof(ID) + std::max(size_of_component, sizeof(void*));
	}

	struct Archetype {
		unsigned int arche_type = 0;
		void** blocks[num_components]; //points to next 16k block
		void* next_components[num_components];
		std::size_t* size_of_components;


		void alloc_block() {
			std::size_t size;
			int num_archetypes;
			for (int i = 0; i < num_components; i++) {
				if (arche_type & (1 << i)) {
					size += size_of_components[i];
				}
				num_archetypes++;
			}

			auto block = (void**)new char[block_size];
			int num_components = (block_size - (sizeof(void*) * num_archetypes)) / size;

			std::size_t offset = 0;
			for (int i = 0; i < World::num_components; i++) {
				if (arche_type & (1 << i)) {
					auto prev = this->blocks[i];
					this->blocks[i] = block + offset;
					*this->blocks[i] = prev;

					for (unsigned int i = 0; i < num_components - 1; i++) {
						*(void**)(char*)(this->blocks[i] + sizeof(void*) + (i * size_of_components[i])) = (this->blocks[i] + sizeof(void*) + ((i + 1) * size_of_components[i]));
					}


					auto prev = this->next_components[i];
					this->next_components[i] = this->blocks[0] + sizeof(void*);
					*(void**)this->next_components[i] = prev;

					offset += size_of_components[i];
				}
			}
		}

		void* next(typeid_t id) {
			if (next_components[id] == NULL)
				alloc_block();

			auto current = next_components[id];
			next_components[id] = *(void**)current;
			return current;
		}

		void free(typeid_t id, void* ptr) {
			auto current = next_components[id];
			*(void**)ptr = current;
			next_components[id] = ptr;
		}

		Archetype(ArchetypeID arch, std::size_t* size_of_components) : arche_type(arch), size_of_components(size_of_components) {
			alloc_block();
		}

		~Archetype() {
			auto block = blocks[type_id<Entity>()]; //single block for all components
			while (true) {
				auto next_block = (void**)*block;
				delete block;
				block = next_block;
				if (*block == NULL) break;
			}
		}
	};

	ID current_id;
	vector<std::unique_ptr<Archetype>> archetypes[num_archetypes];
	void* components_by_id[num_entities];
	int archetype_of_id[num_entities];

	vector<std::unique_ptr<System>> systems;
	Level level;

	template<typename T>
	void add_component() {
		this->size_of_components[type_id(T)] = size_of(sizeof(T));
	}

	void add_system(System* system) {
		systems.push_back(std::unique_ptr<System>(system));
	}

	template<typename T>
	void free_by_id(ID id) {
		int archetype = archetype_of_id[id];

		Archetype* prev_arch = get_archetype(archetype);
		assert(prev_arch);

		archetype &= ~(1 << type_id(T));

		Archetype* arch = get_archetype(archetype);
		if (!arch) {
			insert_archetype(archetype, new Archetype(archetype, size_of_components));
		}

		prev_arch->free(type_id(T), (char*)this->components_by_id[type_id(T)][id] - sizeof(ID));

		for (int i = 0; i < num_components; i++) {
			if (arche_type & (1 << i)) {
				auto prev_component = (char*)this->components_by_id[i][id] - sizeof(ID);
				auto new_component = arch->next(i);
				memcpy(new_component, prev_component, this->size_of_components[i])
					prev_arch->free(i, prev_component);
			}
		}
	}

	Archetype* get_archetype(ArchetypeID id) {
		auto& bucket = this->archetypes[id % num_archetypes];
		for (int i = 0; i < bucket.size(); i++) {
			if (bucket[i].get()->arche_type == id) return bucket[i].get();
		}
		return false;
	}

	Archetype* insert_archetype(ArchetypeID id, Archetype* arch) {
		auto& bucket = this->archetypes[id % num_archetypes];
		bucket.push_back(std::unique_ptr<Archetype>(arch));
	}

	template<typename T>
	T* make(ID id) {
		int archetype = archetype_of_id[id];
		assert(archetype >= 0);
		assert(!(archetype & (1 << type_id(T))));

		int next_archetype = archetype | (1 >> type_id(T));

		Archetype* arch = get_archetype(archetype);
		if (!arch) {
			insert_archetype(archetype, new Archetype(next_archetype, size_of_components));
		}

		if (archetype > 0) {
			Archetype* prev_arch = get_archetype(archetype);
			assert(prev_arch);

			for (int i = 0; i < num_components; i++) {
				if (arche_type & (1 << i)) {
					auto prev_component = (char*)this->components_by_id[i][id] - sizeof(ID);
					auto new_component = arch->next(i);
					memcpy(new_component, prev_component, this->size_of_components[i])
						prev_arch->free(prev_component);
				}
			}
		}

		archetype |= type_id(T);

		auto ptr = arch->next(type_id(T));
		*(ID*)ptr = id;
		ptr += sizeof(ID);
		this->components_by_id[id] = ptr;
		this->archetype_of_id[id] = archetype;
		return (T*)arch->next(type_id(T));
	}

	template<typename T>
	ID id_of(T* ptr) {
		return (T*)(char*)ptr - sizeof(ID);
	}

	ID make_ID();
	void free_ID(ID);

	template<>
	void free_by_id<Entity>(ID id) {
		auto archetype = this->archetype_of_id[id];
		auto arch = get_archetype(archetype);

		for (int i = 0; i < num_components; i++) {
			if (archetype & (1 >> i)) {
				auto prev_component = this->components_by_id[i][id];
				arch->free(i, prev_component);
				this->components_by_id[i][id] = NULL;
			}
		}

		this->archetype_of_id[id] = 0;
		this->free_ID(id);
	}

	template<typename T>
	T* by_id(ID id) {
		return (T*)this->components_by_id[type_id(T)][id];
	}

	template<typename T>
	vector<T*> filter(Layermask layermask) {
		vector<T*> arr;
		ArchetypeID looking_for = (1 << type_id(T)) & (1 << type_id(Entity));

		for (int i = 0; i < num_archetypes; i++) {
			auto& bucket = archetypes[i];
			for (int i = 0; i < bucket.size(); i++) {
				Archetype* arch = bucket[i].get();
				if (arch->archetype & looking_for) {
					auto entity_block = (char*)arch->blocks[type_id(Entity)] + sizeof(void*);
					auto next_entity_block = *(void**)(entity_block - sizeof(void*));

					while (next_entity_block) {
						next_entity_block = *(void**)(entity_block - sizeof(void*);

						while (true) {
							if (*(ID*)entity_block < 0) continue;
							Entity* entity = (Entity*)(entity_block + sizeof(ID));

							if (entity->enabled && entity->layermask & layermask) {
								arr.push_back((T*)(this->components_by_id[*(ID*)entity_block]));
							}

							entity_block += size_of(sizeof(Entity));
						}

						entity_block = next_entity_block + sizeof(void*);
					}
				}
			}
		}

		return arr;
	}

	template <typename... T>
	typename std::enable_if<(sizeof...(T) == 0), bool>::type
		has_component(ID id) {
		return true;
	}

	template<typename T, typename... Args>
	bool has_component(ID id) {
		return by_id<T>(id) != NULL && has_component<Args...>(id);
	}

	template<typename A, typename... Args>
	typename std::enable_if<(sizeof...(Args) > 0), vector <ID> >::type
		filter(Layermask layermask) {
		vector<ID> ids;

		for (int i = 0; i < num_entities; i++) {
			if (has_component<A, Args...>(i)) {
				auto entity = by_id<Entity>(i);
				if (entity && entity->enabled && entity->layermask & layermask) {
					ids.push_back(i);
				}
			}
		}

		return ids;
	}

	ID make_ID();
	void free_ID();

	void render(RenderParams& params) {
		for (int i = 0; i < systems.size(); i++) {
			auto system = systems[i].get();
			system->render(*this, params);
		}
	}

	void update(UpdateParams& params) {
		for (int i = 0; i < systems.size(); i++) {
			auto system = systems[i].get();
			system->update(*this, params);
		}
	}

	World() {
		for (int i = 0; i < num_entities; i++) {
			components_by_id[i] = NULL;
			archetype_of_id[i] = 0;
		}
	}

	vector<Component> components_by_id(ID id);

private:
	unsigned int current_id = 0;
	vector<ID> freed_entities;
};
