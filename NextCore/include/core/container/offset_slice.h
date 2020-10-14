#pragma once

#include "core/core.h"
#include "core/container/slice.h"


template<typename T>
struct offset_ptr {
	int offset;

	offset_ptr() : offset(0) {}
	offset_ptr(offset_ptr& other) { set_base(data); }
	offset_ptr(offset_ptr&& other) { 
		other.offset = 0;
		set_base(other); 
	}

	void set_base(T* data) { offset = (u8*)data - (u8*)this; }

	void operator=(T* data) { set_base(data); }

	T* data() { return (T*)((u8*)this + offset); }
	const T* data() const { return (T*)((u8*)this + offset); }

	operator T*() { return data(); }

	T* operator->() { return data(); }
	const T* operator->() const { return data(); }

	T* operator+(T* other) const { return data() + other; }
	u64 operator-(T* other) const { return data() - other; }
};

//NOTE THIS LIMITS THE MAXIMAL OFFSET TO BE 4GB
template<typename T>
struct offset_slice {
	offset_ptr<T> data;
	uint length;

	offset_slice() : length(0) {}
	
	void operator=(slice<T> slice) { 
		this->data = slice.data;
		this->length = slice.length; 
	}

	T& operator[](uint i) { return data[i]; }
	const T& operator[](uint i) const { return data[i]; }

	T* begin() { return data; }
	T* end() { return data + length; }

	const T* begin() const { return data; }
	const T* end() const { return data + length; }

	operator slice<T>() { return { data, length }; }
};
