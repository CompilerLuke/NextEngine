#include "stdafx.h"
#include "core/memory/allocator.h"
#include "core/memory/linear_allocator.h"
#include "core/io/logger.h"
#include <stdlib.h>

void* BlockAllocator::allocate(std::size_t size) {
	auto free_block = this->free_block;
	while (free_block) {
		if (free_block->offset + size < block_size) {
			auto ptr = free_block->data + free_block->offset;
			free_block->offset += size;
			free_block->count++;

			return ptr;
		}

		free_block = free_block->next;
	}

	log("Out of memory for default allocator");
	exit(1);
}


void BlockAllocator::deallocate(void* ptr) {
	if (ptr == NULL) return;

	for (int i = 0; i < num_blocks; i++) {
		if (&blocks[i].data <= ptr && ptr < &blocks[i].data + blocks[i].count) {
			blocks[i].count -= 1;
			if (blocks[i].count == 0) {
				blocks[i].offset = 0;

				blocks[i].next = free_block;
				free_block = &blocks[i];
			}

			break;
		}
	}
}

void BlockAllocator::next_empty_block() {
	for (int i = 0; i < num_blocks; i++) {
		if (blocks[i].count == 0) {
			blocks[i].next = free_block;
			free_block = &blocks[i];
		}
	}

	log("No empty block left");
}

BlockAllocator::BlockAllocator() {
	for (int i = 0; i < num_blocks - 1; i++) {
		blocks[i].next = &blocks[i + 1];
	}

	free_block = &blocks[0];
}

//MALLOC ALLOCATOR WRAPPER
void* MallocAllocator::allocate(std::size_t size) {
	return new char[size];
}

void MallocAllocator::deallocate(void* ptr) {
	delete[] ptr;
}

//GLOBAL Allocators
#include "core/context.h"

MallocAllocator default_allocator;
thread_local LinearAllocator temporary_allocator;
thread_local LinearAllocator permanent_allocator;

Allocator& get_allocator() {
	return *get_context().allocator;
}

LinearAllocator& get_temporary_allocator() {
	return *get_context().temporary_allocator;
}

LinearAllocator& get_default_allocator() {
	return permanent_allocator;
}

LinearAllocator& get_permanent_allocator() {
	return permanent_allocator;
}

LinearAllocator& get_thread_local_temporary_allocator() {
	return temporary_allocator;
}

LinearAllocator& get_thread_local_permanent_allocator() {
	return permanent_allocator;
}