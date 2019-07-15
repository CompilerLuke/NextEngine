#include "editor/diffUtil.h"
#include "core/temporary.h"
#include "editor/editor.h"

DiffUtil::DiffUtil(void* ptr, reflect::TypeDescriptor* type) 
: real_ptr(ptr), type(type) {
	
	copy_ptr = temporary_allocator.allocate(type->size);
	memcpy(copy_ptr, real_ptr, type->size);
}

void DiffUtil::submit(Editor& editor, const char* name) {

}

Diff::Diff(unsigned int size) {
	this->buffer = new char[size];
}

Diff::~Diff() {
	delete this->buffer;
}
