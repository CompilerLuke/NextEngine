#pragma once

#include "core/core.h"
#include <assert.h>

using hash_meta = uint;

inline uint hash_func(void* ptr) { return (u64)ptr >> 32; }
inline uint hash_func(uint hash) { return hash; }
inline uint hash_func(const char* str) {
	int hash = 7;

	while (*str++) {
		hash = hash * 31 + *str;
	}

	return hash;
}

template<typename K>
struct hash_set_base {
	uint capacity;
	hash_meta* meta = {};
	K* keys;

	hash_set_base(uint capacity, hash_meta* meta, K* keys)
	: capacity(capacity), meta(meta), keys(keys) {}

	void clear() {
		for (uint i = 0; i < capacity; i++) {
			meta[i] = {};
			keys[i] = {};
		}
	}

	bool matches(uint hash, const K& key, uint probe_hash) const {
		uint hash_truncated = (hash << 1) >> 1;
		uint found_hash = meta[probe_hash] >> 1;
		return found_hash == hash_truncated && keys[probe_hash] == key;
	}

	bool is_full(uint probe_hash) const {
		return meta[probe_hash] & 0x1;
	}

	int add(K key) {
		const u64 hash = hash_func(key);
		uint probe_hash = hash % capacity;
		uint it = 0;
		while (is_full(probe_hash)) {
			assert(it != capacity);
			if (matches(hash, key, probe_hash)) return probe_hash; //Already exists

			probe_hash++;
			it++;
			probe_hash %= capacity;
		}

		meta[probe_hash] = hash << 1 | 0x1;
		keys[probe_hash] = key;

		return probe_hash;
	}

	int index(K key) {
		const u64 hash = hash_func(key);
		uint probe_hash = hash % capacity;

		while (is_full(probe_hash) && !matches(hash, key, probe_hash)) {
			probe_hash++;
			probe_hash %= capacity;
		}

		if (is_full(probe_hash)) return probe_hash;
		else return -1;
	}
};

template<typename K, typename V>
struct key_value {
	K& key;
	V& value;
};


template<typename K, typename V>
struct hash_map_it {
	hash_meta* meta;
	K* keys;
	V* values;
	uint length;
	uint i;

	void skip_empty() {
		while (i < length && !(meta[i] & 0x1))  {
			i++;
		}
	}

	void operator++() {
		i++;
		skip_empty();
	}

	bool operator==(hash_map_it<K,V>& other) const {
		return this->i == other.i;
	}

	bool operator!=(hash_map_it<K,V>& other) const {
		return !(*this == other);
	}

	key_value<K,V> operator*() {
		return { keys[i], values[i] };
	}
};

template<typename K, typename V>
struct hash_map_base : hash_set_base<K> {
	V* values = {};

	hash_map_base(uint capacity, hash_meta* meta, K* keys, V* values)
	: hash_set_base<K>::hash_set_base(capacity, meta, keys), values(values) {
        
        
	}
    
	uint set(K key, const V& value) {
		int index = this->keys.add(key);
		values[index] = value;
		return index;
	}

	V& operator[](K key) {
		return values[this->add(key)];
	}

	const V& operator[](K key) const {
		return values[this->index(key)];
	}

	hash_map_it<K, V> begin() {
		hash_map_it<K,V> it{ this->meta, this->keys, this->values, this->capacity, 0 };
		it.skip_empty();
		return it;
	}

	hash_map_it<K, V> end() {
		return { this->meta, this->keys, this->values, this->capacity, this->capacity };
	}

	V* get(K key) {
		int index = this->keys.index(key);
		if (index != -1) return &values[index];
		else return nullptr;
	}
};

template <typename K, uint N>
struct hash_set {
	hash_meta meta[N] = {};
	K keys[N] = {};

	uint capacity() { return N; }

	void clear() {
		for (uint i = 0; i < N; i++) {
			meta[i] = {};
			keys[i] = {};
		}
	}

	bool matches(uint hash, const K& key, uint probe_hash) const { 
		return hash_set_base<K>::hash_set_base(N, meta, keys).matches(hash, key, probe_hash);
	}

	void remove(const K& key) {
		int i = index(key);
		if (i == -1) return;
		meta[i] = {};
		keys[i] = {};
	}

	bool is_full(uint probe_hash) const { return meta[probe_hash] & 0x1; }
	int add(K key) { return hash_set_base<K>(N, meta, keys).add(key); }
	int index(K key) { return hash_set_base<K>(N, meta, keys).index(key); }
	bool contains(K key) { return hash_set_base<K>(N, meta, keys).index(key) != -1; }
};

template <typename K, typename V, uint N>
struct hash_map : hash_set<K, N> {
	V values[N] = {};
	
	uint capacity() { return N;}

	void clear() {
		for (uint i = 0; i < N; i++) {
			this->meta[i] = {};
			this->keys[i] = {};
			this->values[i] = {};
		}
	}

	uint set(K key, const V& value) {
		int index = this->add(key);
		values[index] = value;
		return index;
	}

	void remove(const K& key) {
		int i = index(key);
		if (i == -1) return;
		meta[i] = {};
		keys[i] = {};
		values[i] = {};
	}
    
    const V& operator[](K key) const {
        return values[this->index(key)];
    }

	V& operator[](K key) {
		return values[this->add(key)];
	}


	hash_map_it<K, V> begin() {
		hash_map_it<K, V> it{ this->meta, this->keys, this->values, N, 0 };
		it.skip_empty();
		return it;
	}

	hash_map_it<K, V> end() {
		return { this->meta, this->keys, values, N, N };
	}

	V* get(K key) {
		int i = this->index(key);
		if (i != -1) return &values[i];
		else return nullptr;
	}
};
