#pragma once

#include "core/core.h"

using hash_meta = uint;

inline uint hash_func(uint hash) { return hash; }
inline uint hash_func(const char* str) {
	int hash = 7;

	while (*str++) {
		hash = hash * 31 + *str;
	}

	return hash;
}

template<typename K, int N>
struct hash_set {
	hash_meta meta[N] = {};
	K keys[N];

	void clear() {
		meta[N] = {};
		keys[N] = {};
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
		uint probe_hash = hash % N;

		while (is_full(probe_hash)) {
			if (matches(hash, key, probe_hash)) return probe_hash; //Already exists

			probe_hash++;
			probe_hash %= N;
		}

		meta[probe_hash] = hash << 1 | 0x1;
		keys[probe_hash] = key;

		return probe_hash;
	}

	int index(K key) {
		const u64 hash = hash_func(key);
		uint probe_hash = hash % N;

		while (is_full(probe_hash) && !matches(hash, key, probe_hash)) {
			probe_hash++;
			probe_hash %= N;
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
	int length;
	int i;

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

template<typename K, typename V, int N>
struct hash_map {
	hash_set<K, N> keys = {};
	V values[N] = {};

	uint set(K key, const V& value) {
		int index = keys.add(key);
		values[index] = value;
		return index;
	}

	V& operator[](K key) {
		return values[keys.add(key)];
	}

	const V& operator[](K key) const {
		return values[keys.index(key)];
	}

	void clear() {
		keys.clear();
	}

	hash_map_it<K, V> begin() {
		hash_map_it<K,V> it{ keys.meta, keys.keys, values, N, 0 };
		it.skip_empty();
		return it;
	}

	hash_map_it<K, V> end() {
		return { keys.meta, keys.keys, values, N, N };
	}

	V* get(K key) {
		int index = keys.index(key);
		if (index != -1) return &values[index];
		else return NULL;
	}
};