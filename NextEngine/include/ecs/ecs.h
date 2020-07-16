#pragma once

#include "core/container/vector.h"
#include <assert.h>
#include <memory>
#include "id.h"
#include "system.h"
#include "core/reflection.h"
#include "core/memory/linear_allocator.h"
#include <functional>
#include "core/container/event_dispatcher.h"


#if 0
template<typename T, typename B>
struct Pair {
	T first;
	B second;
};

template<typename T>
struct Slot {
	union {
		Slot<T>* next;
		Pair<T, ID> object;
	};
	bool is_enabled = false;

	Slot() {
		next = NULL;
	}

	~Slot() {
		if (is_enabled) {
			object.first.~T();
		}
	}
};

constexpr unsigned int max_entities = 20000;

struct Component {
	ID id;
	struct ComponentStore* store;
	refl::Struct* type;
	void* data;
};

struct ComponentStore {
	virtual void free_by_id(ID) = 0;
	virtual void actually_free_by_id(ID) = 0;
	virtual void* make_by_id(ID) = 0;
	virtual Component get_by_id(ID) = 0;
	virtual void set_enabled(void*, bool) = 0;
	virtual vector<Component> filter_untyped() = 0;
	virtual refl::Struct* get_component_type() = 0;
	virtual ComponentStore* clone() = 0;
	virtual void copy_from(const ComponentStore*) = 0;
	virtual void fire_callbacks() = 0;
	virtual void clear() = 0;
	virtual ~ComponentStore() {};
};

using typeid_t = int;

/*
template <typename T>
constexpr typeid_t type_id() noexcept
{
	static int const type_id = global_type_id++;
	return type_id;
}
*/

template<typename T>
constexpr typeid_t type_id(); 

template<typename T>
struct Store : ComponentStore {
	T* id_to_obj[max_entities];
	Slot<T>* components;
	Slot<T>* free_slot;
	unsigned int N;

	vector<ID> created;
	vector<ID> freed;

	EventDispatcher<vector<ID>&> on_make;
	EventDispatcher<vector<ID>&> on_free;

	void fire_destructors() {
		vector<ID> freed;

		for (int i = 0; i < N; i++) {
			if (components[i].is_enabled) {
				freed.append(components[i].object.second);
			}
		}

		on_free.broadcast(freed);
	}

	void clear() {
		fire_destructors();

		for (int i = 0; i < max_entities; i++) {
			id_to_obj[i] = NULL;
		}

		for (unsigned int i = 0; i < N - 1; i++) {
			components[i].next = &components[i + 1];
			components[i].is_enabled = false;
		}

		free_slot = &components[0];
	}

	Store(unsigned int max_number) {
		N = max_number;

		components = new Slot<T>[N];
		clear();
	}

	virtual ~Store() final {
		fire_destructors();
		delete[] components;
	}

	T* by_id(ID id) {
		if (id >= max_entities) return NULL;
		return id_to_obj[id];
	}

	const T* by_id(ID id) const {
		if (id >= max_entities) return NULL;
		return id_to_obj[id];
	}

	Component get_by_id(ID id) final {
		return { id, this, (refl::Struct*)refl_type(T), (T*)by_id(id) };
	}

	uint component_id(T* ptr) {
		return (Slot<T>*)ptr - components;
	}

	void free_by_id(ID id) final { //delay freeing till end of frame
		this->freed.append(id);
	}

	void actually_free_by_id(ID id) final { 
		auto obj_ptr = this->by_id(id);
		if (!obj_ptr) return;

		obj_ptr->~T();

		id_to_obj[id] = NULL;
		auto slot = (Slot<T>*) obj_ptr;
		slot->next = free_slot;
		slot->is_enabled = false;
		free_slot = slot;
	}

	T* make(ID id) {
		if (free_slot == NULL) { throw "Out of memory for system"; }

		auto current_free_slot = free_slot;
		this->free_slot = current_free_slot->next;

		new (&current_free_slot->object.first) T();
		current_free_slot->object.second = id;
		current_free_slot->is_enabled = true;

		auto obj_ptr = &current_free_slot->object.first;

		this->created.append(id);

		this->register_component(id, obj_ptr);
		return obj_ptr;
	}

	void* make_by_id(ID id) final {
		return (void*)make(id);
	}

	void register_component(ID id, T* obj) {
		assert(id < max_entities);
		id_to_obj[id] = obj;
	}

	void set_enabled(void* ptr, bool enabled) override final {
		auto slot = (Slot<T>*) ptr;
		slot->is_enabled = enabled;

		ID id = slot->object.second;
		if (enabled) {
			id_to_obj[id] = &slot->object.first;
		}
		else {
			id_to_obj[id] = NULL;
		}
	}

	vector<T*> filter() {
		vector<T*> arr;
		arr.allocator = &temporary_allocator;

		for (unsigned int i = 0; i < this->N; i++) {
			auto slot = &this->components[i];

			if (!slot->is_enabled) continue;
			arr.append(&slot->object.first);
		}

		return arr;
	}

	vector<const T*> filter() const {
		vector<const T*> arr;
		arr.allocator = &temporary_allocator;

		for (unsigned int i = 0; i < this->N; i++) {
			auto slot = &this->components[i];

			if (!slot->is_enabled) continue;
			arr.append(&slot->object.first);
		}

		return arr;
	}

	refl::Struct* get_component_type() override final {
		return (refl::Struct*)refl_type(T);
	}

	vector<Component> filter_untyped() override final { //todo merge definitions
		vector<Component> arr;
		arr.allocator = &temporary_allocator;

		for (unsigned int i = 0; i < this->N; i++) {
			auto slot = &this->components[i];

			if (!slot->is_enabled) continue;
			//arr.append({slot->object.second, this, reflect::TypeResolver<T>::get(), &slot->object.first });
		}

		return arr;
	}

	void fire_callbacks() override final {
		if (created.length > 0) {
			vector<ID> actually_created;
			for (ID id : created) {
				if (this->by_id(id) != NULL) actually_created.append(id);
			}
			
			on_make.broadcast(actually_created);
			created.clear();
		}

		if (freed.length > 0) {
			vector<ID> actually_freed;
			for (ID id : freed) {
				if (this->by_id(id) != NULL) actually_freed.append(id);
			}

			on_free.broadcast(actually_freed);
			for (int i = 0; i < freed.length; i++) {
				actually_free_by_id(freed[i]);
			}
			freed.clear();
		}
	}

	ComponentStore* clone() final {
		return new Store(N);
	}

	void virtual copy_from(const ComponentStore* component_store) const final {

	}

	void virtual copy_from(const ComponentStore* component_store) final {
		Store* store = (Store*)component_store;
		clear();

		for (uint i = 0; i < store->N; i++) {
			Slot<T>* slot = &store->components[i];
			if (!slot->is_enabled) continue;

			T* ptr = make(slot->object.second);
			
			
			//*ptr = *store->by_id(slot->object.second);
		}

		fire_callbacks();
	}
};

COMP
struct Entity {
	bool enabled = true;
	Layermask layermask = GAME_LAYER | PICKING_LAYER;
	Flags flags = STATIC;
};

struct World {
	static constexpr int components_hash_size = 100;

	std::unique_ptr<ComponentStore> components[components_hash_size];
	vector<ID> skipped_ids;
	vector<ID> delay_free_entity;

	template<typename T>
	constexpr void add(Store<T>* store) {
		components[(size_t)type_id<T>()] = std::unique_ptr<ComponentStore>(store);
	}

	template<typename T>
	constexpr Store<T>* get() {
		auto ret = (Store<T>*)(components[(size_t)type_id<T>()].get());
		assert(ret != NULL);
		return ret;
	}

	template<typename T>
	constexpr const Store<T>* get() const {
		auto ret = (const Store<T>*)(components[(size_t)type_id<T>()].get());
		assert(ret != NULL);
		return ret;
	}

	template<typename T>
	void on_make(std::function<void (vector<ID>&)> f) {
		get<T>()->on_make.listen(f);
	}

	template<typename T>
	void on_free(std::function<void(vector<ID>&)> f) {
		get<T>()->on_free.listen(f);
	}

	template<typename T>
	void remove_on_make(std::function<void(vector<ID>&)> f) {
		get<T>()->on_make.remove(f);
	}

	template<typename T>
	void remove_on_free(std::function<void(vector<ID>&)> f) {
		get<T>()->on_free.remove(f);
	}

	template<typename T>
	T* make(ID id) {
		return this->get<T>()->make(id);
	}

	ID ENGINE_API make_ID();
	void ENGINE_API free_ID(ID);

	template<typename T>
	void free_by_id(ID id) {
		this->get<T>()->free_by_id(id);
	}

	void free_by_id(ID id) {
		this->delay_free_entity.append(id);
	}

	template<typename T>
	uint component_id(ID id) {
		return get<T>()->component_id(by_id<T>(id));
	}

	template<typename T>
	uint component_id(T* t) {
		return get<T>()->component_id(t);
	}

	void free_now_by_id(ID id) {
		for (unsigned int i = 0; i < components_hash_size; i++) {
			if (components[i] != NULL) {
				components[i]->actually_free_by_id(id);
			}
		}

		free_ID(id);
	}

	template<typename T>
	ID id_of(T* ptr) const {
		assert(get<T>() != NULL);
		return ((Slot<T>*)ptr)->object.second;
	}

	template<typename T>
	T* by_id(ID id) {
		return get<T>()->by_id(id);
	}

	template<typename T>
	const T* by_id(ID id) const {
		return get<T>()->by_id(id);
	}

	template<typename T>
	vector<T*> filter() {
		Store<T>* store = get<T>();
		return store->filter();
	}

	template<typename T>
	vector<const T*> filter() const {
		const Store<T>* store = get<T>();
		return store->filter();
	}

	template<typename T>
	vector<T*> filter(Layermask layermask) {
		Store<T>* store = get<T>();
		Store<Entity>* entity_store = get<Entity>();

		vector<T*> arr;
		arr.allocator = &temporary_allocator;

		for (unsigned int i = 0; i < store->N; i++) {
			auto slot = &store->components[i];

			if (!slot->is_enabled) continue;

			auto entity = entity_store->by_id(slot->object.second);

			if (entity && entity->enabled && (entity->layermask & layermask)) {
				arr.append(&slot->object.first);
			}
		}

		return arr;
	}

	template<typename T>
	vector<const T*> filter(Layermask layermask) const {
		const Store<T>* store = get<T>();
		const Store<Entity>* entity_store = get<Entity>();

		vector<const T*> arr;
		arr.allocator = &temporary_allocator;

		for (unsigned int i = 0; i < store->N; i++) {
			auto slot = &store->components[i];

			if (!slot->is_enabled) continue;

			auto entity = entity_store->by_id(slot->object.second);

			if (entity && entity->enabled && (entity->layermask & layermask)) {
				arr.append(&slot->object.first);
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

	template<typename T, typename... Args>
	typename std::enable_if<(sizeof...(Args) > 0), vector <ID> >::type
		filter(Layermask layermask) {
		vector<ID> ids;
		ids.allocator = &temporary_allocator;
		vector<T*> filter_by = filter<T>(layermask);

		for (int i = 0; i < filter_by.length; i++) {
			int id = id_of(filter_by[i]);
			if (has_component<Args...>(id)) {
				ids.append(id);
			}
		}

		return ids;
	}

	void begin_frame() {		
		//FIRE CREATION OR DELETION CALLBACKS
		for (unsigned int i = 0; i < components_hash_size; i++) { 
			if (components[i] != NULL) {
				Store<char>* store = (Store<char>*)components[i].get();

				for (ID id : delay_free_entity) {
					store->freed.append(id);
				}

				components[i]->fire_callbacks();
			}
		}

		//REMOVE DELAYED ENITITIES
		for (ID id : delay_free_entity) {
			free_ID(id);
		}

		delay_free_entity.clear();
	}

	void clear() {
		for (int i = 0; i < components_hash_size; i++) {
			if (components[i] != NULL) components[i]->clear();
		}
	}

	void ENGINE_API operator=(const World&);

	World() {
		for (int i = 0; i < components_hash_size; i++) {
			components[i] = NULL;
		}
	}

	vector<Component> ENGINE_API components_by_id(ID id);

private:
	unsigned int current_id = 0;
	vector<ID> freed_entities;
};

#else 
#include "ecs20.h"
#endif 