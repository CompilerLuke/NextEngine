#include "stdafx.h"
#include "editor/diffUtil.h"
#include "core/temporary.h"
#include "editor/editor.h"
#include "logger/logger.h"
#include <glm/gtc/epsilon.hpp>

DiffUtil::DiffUtil(void* ptr, reflect::TypeDescriptor* type, Allocator* allocator) 
: real_ptr(ptr), type(type) {
	
	copy_ptr = allocator->allocate(type->size);
	memcpy(copy_ptr, real_ptr, type->size);
}

constexpr float EPSILON = 0.01;

bool DiffUtil::submit(Editor& editor, const char* name) {
	vector<unsigned int> offsets_diff;
	vector<unsigned int> sizes;
	unsigned int size = 0.0f;
	
	if (this->type->kind == reflect::Struct_Kind) {
		reflect::TypeDescriptor_Struct* type = (reflect::TypeDescriptor_Struct*)this->type;

		for (auto& member : type->members) {
			auto original_value = (char*)copy_ptr + member.offset;
			auto new_value = (char*)real_ptr + member.offset;

			bool modified = false;

			if (memcmp(new_value, original_value, member.type->size) != 0) {
				offsets_diff.append(member.offset);
				sizes.append(member.type->size);
				size += member.type->size;
			}
		}
	} 
	else if (this->type->kind == reflect::Union_Kind) {
		reflect::TypeDescriptor_Union* type = (reflect::TypeDescriptor_Union*)this->type;
		
		if (memcmp(copy_ptr, real_ptr, type->size) != 0) {
			offsets_diff.append(0);
			sizes.append(type->size);
			size += type->size;
		}
	}
	else {
		throw "Unsupported type";
	}

	if (offsets_diff.length > 0) {
		Diff diff(size);
		diff.target = real_ptr;
		diff.name = name;

		unsigned int offset_buffer = 0;

		for (unsigned int i = 0; i < offsets_diff.length; i++) {
			memcpy((char*)diff.redo_buffer + offset_buffer, (char*)real_ptr + offsets_diff[i], sizes[i]);
			memcpy((char*)diff.undo_buffer + offset_buffer, (char*)copy_ptr + offsets_diff[i], sizes[i]);

			offset_buffer += sizes[i];
		}

		diff.offsets = std::move(offsets_diff);
		diff.sizes = std::move(sizes);

		editor.submit_diff(std::move(diff));

		return true;
	}

	return false;
}

Diff::Diff(unsigned int size) {
	this->undo_buffer = new char[size];
	this->redo_buffer = new char[size];
}

Diff::Diff(Diff&& other) {
	this->redo_buffer = other.redo_buffer;
	this->undo_buffer = other.undo_buffer;
	this->target = other.target;
	this->offsets = std::move(other.offsets);
	this->sizes = std::move(other.sizes);
	
	other.redo_buffer = NULL;
	other.undo_buffer = NULL;
}

void apply_diff(Diff& diff, void* buffer) {
	unsigned int offset_buffer = 0;

	for (unsigned int i = 0; i < diff.offsets.length; i++) {
		memcpy((char*)diff.target + diff.offsets[i], (char*)buffer + offset_buffer, diff.sizes[i]);
		offset_buffer += diff.sizes[i];
	}
}

void Diff::undo() {
	apply_diff(*this, this->undo_buffer);
}

void Diff::redo() {
	apply_diff(*this, this->redo_buffer);
}


Diff::~Diff() {
	delete this->undo_buffer;
	delete this->redo_buffer;
}
