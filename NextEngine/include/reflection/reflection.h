#pragma once

#include "core/allocator.h"
#include "core/vector.h"
#include <string>
#include <cstddef>
#include <utility>
#include <vector>

struct World;

namespace reflect {

	//--------------------------------------------------------
	// Base class of all type descriptors
	//--------------------------------------------------------

	enum TypeDescriptor_Kind {
		Ptr_Kind,
		Vector_Kind,
		Struct_Kind,
		Enum_Kind,
		Union_Kind,
		Bool_Kind,
		Int_Kind,
		Unsigned_Int_Kind,
		Float_Kind,
		StdString_Kind
	};

	struct TypeDescriptor {
		TypeDescriptor_Kind kind;
		const char* name;
		size_t size;
	};

	//--------------------------------------------------------
	// Finding type descriptors
	//--------------------------------------------------------

	// Declare the function template that handles primitive types such as int, std::string, etc.:
	template <typename T>
	TypeDescriptor* getPrimitiveDescriptor();

	// A helper class to find TypeDescriptors in different ways:
	struct DefaultResolver {
		template <typename T> static char func(decltype(&T::Reflection));
		template <typename T> static int func(...);
		template <typename T>
		struct IsReflected {
			enum { value = (sizeof(func<T>(nullptr)) == sizeof(char)) };
		};

		// This version is called if T has a static member named "Reflection":
		template <typename T, typename std::enable_if<IsReflected<T>::value, int>::type = 0>
		static TypeDescriptor* get() {
			return &T::Reflection;
		}



		// This version is called otherwise:
		template <typename T, typename std::enable_if<!IsReflected<T>::value, int>::type = 0>
		static TypeDescriptor* get() {
			return getPrimitiveDescriptor<T>();
		}
	};

	// This is the primary class template for finding all TypeDescriptors:
	template <typename T>
	struct TypeResolver {
		static TypeDescriptor* get() {
			return DefaultResolver::get<T>();
		}
	};

	//--------------------------------------------------------
	// Type descriptors for user-defined structs/classes
	//--------------------------------------------------------

	enum TypeTag {
		NoTag,
		LayermaskTag,
		ModelIDTag,
		ShaderIDTag,
		TextureIDTag,
		CubemapIDTag,
	};

	struct TypeDescriptor_Struct : TypeDescriptor {
		struct Member {
			const char* name;
			size_t offset;
			TypeDescriptor* type;
			TypeTag tag;
		};

		std::vector<Member> members;

		TypeDescriptor_Struct(void(*init)(TypeDescriptor_Struct*)) : TypeDescriptor{ Struct_Kind, nullptr, 0 } {
			init(this);
		}
		TypeDescriptor_Struct(const char* name, size_t size, const std::initializer_list<Member>& init) : TypeDescriptor{ Struct_Kind, name, size }, members{ init } {
		}
	};

	struct TypeDescriptor_Union : TypeDescriptor {
		struct Member {
			const char* name;
			size_t offset;
			TypeDescriptor* type;
			TypeTag tag;
		};

		size_t tag_offset = 0;

		std::vector<Member> cases;
		std::vector<Member> members;

		TypeDescriptor_Union(void(*init)(TypeDescriptor_Union*)) : TypeDescriptor{ Union_Kind, nullptr, 0 } {
			init(this);
		}
	};

	struct TypeDescriptor_Enum : TypeDescriptor {
		std::vector<std::pair<std::string, int>> values;

		TypeDescriptor_Enum(void(*init)(TypeDescriptor_Enum*)) : TypeDescriptor{ Enum_Kind, nullptr, 0 } {
			init(this);
		}
	};


#define REFLECT() \
    friend struct reflect::DefaultResolver; \
    static reflect::TypeDescriptor_Struct Reflection; \
    static void initReflection(reflect::TypeDescriptor_Struct*);

#define REFLECT_STRUCT_BEGIN(type) \
    reflect::TypeDescriptor_Struct type::Reflection{type::initReflection}; \
    void type::initReflection(reflect::TypeDescriptor_Struct* typeDesc) { \
        using T = type; \
        typeDesc->name = #type; \
        typeDesc->size = sizeof(T); \
        typeDesc->members = {

#define REFLECT_GENERIC_STRUCT_BEGIN(type) \
    reflect::TypeDescriptor_Struct type::Reflection{type::initReflection}; \
	template<>\
    void type::initReflection(reflect::TypeDescriptor_Struct* typeDesc) { \
        using T = type; \
        typeDesc->name = #type; \
        typeDesc->size = sizeof(T); \
        typeDesc->members = {

#define REFLECT_STRUCT_MEMBER(name) \
            {#name, offsetof(T, name), reflect::TypeResolver<decltype(T::name)>::get(), reflect::NoTag},

#define REFLECT_STRUCT_MEMBER_TAG(name, tag) \
            {#name, offsetof(T, name), reflect::TypeResolver<decltype(T::name)>::get(), tag},

#define REFLECT_STRUCT_END() \
        }; \
    }

#define REFLECT_UNION() \
    friend struct reflect::DefaultResolver; \
    static reflect::TypeDescriptor_Union Reflection; \
    static void initReflection(reflect::TypeDescriptor_Union*);

#define REFLECT_UNION_BEGIN(Type) \
	reflect::TypeDescriptor_Union Type::Reflection{Type::initReflection}; \
    void Type::initReflection(reflect::TypeDescriptor_Union* typeDesc) { \
        using T = Type; \
		typeDesc->tag_offset = offsetof(T, type); \
        typeDesc->name = #Type; \
        typeDesc->size = sizeof(T); \
        typeDesc->cases = {

#define REFLECT_UNION_FIELD(name) }; \
	typeDesc->members = {{#name, offsetof(T, name), reflect::TypeResolver<decltype(T::name)>::get(), reflect::NoTag}}; \
	typeDesc->cases = {

#define REFLECT_UNION_CASE(name) REFLECT_STRUCT_MEMBER(name)
#define REFLECT_UNION_CASE_TAG(name, tag) REFLECT_STRUCT_MEMBER_TAG(name, tag)

#define REFLECT_UNION_END() REFLECT_STRUCT_END()

#define REFLECT_BEGIN_ENUM(type) \
	void init##type(reflect::TypeDescriptor_Enum*); \
	\
	template<> \
	reflect::TypeDescriptor* reflect::getPrimitiveDescriptor<type>() { \
			static reflect::TypeDescriptor_Enum type(init##type); \
			return &type; \
	} \
	void init##type(reflect::TypeDescriptor_Enum* e) { \
		e->size = sizeof(type); \
		e->values = {

#define REFLECT_ENUM_VALUE(name) {#name, name},

#define REFLECT_END_ENUM() }; }

//--------------------------------------------------------
// Type descriptors for vector
//--------------------------------------------------------

struct TypeDescriptor_Vector : TypeDescriptor {
    TypeDescriptor* itemType;
	std::string name_buffer;

    template <typename ItemType>
    TypeDescriptor_Vector(ItemType*)
        : TypeDescriptor{Vector_Kind, "vector<>", sizeof(vector<ItemType>)},
                         itemType{TypeResolver<ItemType>::get()} {
        
		this->name_buffer = std::string("vector<") + std::string(itemType->name) + std::string(">");
		this->name = this->name_buffer.c_str();
    }
};

// Partially specialize TypeResolver<> for vectors:
// Partially specialize TypeResolver<> for vectors:
template <typename T>
class TypeResolver<vector<T>> {
public:
	static TypeDescriptor* get() {
		static TypeDescriptor_Vector typeDesc{ (T*) nullptr };
		return &typeDesc;
	}
};

struct TypeDescriptor_Pointer : TypeDescriptor {
	TypeDescriptor* itemType;
	std::string name_buffer;

	template <typename ItemType>
	TypeDescriptor_Pointer(ItemType*) :
		TypeDescriptor{ Ptr_Kind, "*", sizeof(void*) },
		itemType{ TypeResolver<ItemType>::get() } {

		this->name_buffer = std::string(itemType->name) + "*";
		this->name = this->name_buffer.c_str();
	}
};

// Partially specialize TypeResolver<> for pointer:
template <typename T>
class TypeResolver<T*> {
public:
	static TypeDescriptor* get() {
		static TypeDescriptor_Pointer typeDesc{ (T*) nullptr };
		return &typeDesc;
	}
};

} // namespace reflect