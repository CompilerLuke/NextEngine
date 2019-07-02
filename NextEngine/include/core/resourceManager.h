#pragma once

#include "handleManager.h"
#include <unordered_map>
#include <string>

template<typename T, typename Desc>
struct ResourceManager {
	struct Resource {
		std::string path; //blank if created at runtime
		Desc resource_desc;
		T resource;

		Resource(struct RHI& rhi, Desc&& desc, std::string path = "") {
			this->resource_desc = desc;
			this->resource = dec.make(rhi);
			this->path = path;
		}
	};

	HandleManager<Resource> resources;

	using Handle = Handle<Resource>;

	std::unordered_map<std::string, Handle> handle_by_path;

	Handle make(struct RHI& rhi, Desc&& desc) {
		return resources.make(Resource(rhi, desc));
	}

	Handle load(struct RHI& rhi, const std::string& path, Desc desc) {
		if (handle_by_path.find(path) != handle_by_path.end()) {
			return handle_by_path[path];
		}

		return resources.make(Resource(rhi, desc, path));
	}

	Resource* get_resource(Handle handle) {
		return resources.get(handle);
	}

	T* get(Handle handle) {
		return &get_resource(handle)->resource;
	}

	T* get_resource_desc(Handle handle) {
		return &get_resource(handle)->resource_desc;
	}

	void free(Handle handle) {
		resources.free(handle);
	}
};