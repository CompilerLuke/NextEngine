#include "stdafx.h"
#include "reflection/reflection.h"
#include <glm/vec3.hpp>
#include <glm/vec2.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/quaternion.hpp>
#include "editor/displayComponents.h"
#include "core/string_buffer.h"

#define PRIMITIVE_TYPE_DESCRIPTOR(kind, type) \
struct TypeDescriptor_##type : TypeDescriptor { \
	TypeDescriptor_##type() : TypeDescriptor{ kind, #type , sizeof(type) } {}; \
}; \
\
template<> \
TypeDescriptor* getPrimitiveDescriptor<type>() { \
	static TypeDescriptor_##type typeDesc; \
	return &typeDesc;  \
} 

namespace reflect {

	//--------------------------------------------------------
	// A type descriptor for int
	//--------------------------------------------------------

	PRIMITIVE_TYPE_DESCRIPTOR(Float_Kind, float);
	PRIMITIVE_TYPE_DESCRIPTOR(Bool_Kind, bool);
	PRIMITIVE_TYPE_DESCRIPTOR(Int_Kind, int);
	
	using UINT = unsigned int;
	PRIMITIVE_TYPE_DESCRIPTOR(Unsigned_Int_Kind, UINT);
	//--------------------------------------------------------
	// A type descriptor for std::string
	//--------------------------------------------------------

	struct TypeDescriptor_StringBuffer : TypeDescriptor {
		TypeDescriptor_StringBuffer() : TypeDescriptor{ StringBuffer_Kind, "StringBuffer", sizeof(StringBuffer) } {
		}
	};

	template <>
	TypeDescriptor* getPrimitiveDescriptor<StringBuffer>() {
		static TypeDescriptor_StringBuffer typeDesc;
		return &typeDesc;
	}

	template<>
	TypeDescriptor* getPrimitiveDescriptor<glm::vec3>() {
		static TypeDescriptor_Struct typeDesc("glm::vec3", sizeof(glm::vec3), {
			{ "x", offsetof(glm::vec3, x), reflect::TypeResolver<float>::get(), reflect::NoTag },
			{ "y", offsetof(glm::vec3, y), reflect::TypeResolver<float>::get(), reflect::NoTag },
			{ "z", offsetof(glm::vec3, z), reflect::TypeResolver<float>::get(), reflect::NoTag }
		});
		return &typeDesc;
	}

	template<>
	TypeDescriptor* getPrimitiveDescriptor<glm::vec2>() {
		static TypeDescriptor_Struct typeDesc("glm::vec", sizeof(glm::vec2), {
			{ "x", offsetof(glm::vec2, x), reflect::TypeResolver<float>::get(), reflect::NoTag },
			{ "y", offsetof(glm::vec2, y), reflect::TypeResolver<float>::get(), reflect::NoTag }
		});
		return &typeDesc;
	}

	void init_mat4_type(TypeDescriptor_Struct* type) {
		type->size = sizeof(glm::vec2);
		type->name = "glm::mat4";
		for (int i = 0; i < 16; i++) {
			type->members.push_back({ "mat_field", i * sizeof(float), reflect::TypeResolver<float>::get(), reflect::NoTag });
		}
	}

	template<>
	TypeDescriptor* getPrimitiveDescriptor<glm::mat4>() {
		static TypeDescriptor_Struct typeDesc(init_mat4_type);
		return &typeDesc;
	}

	void init_quat_type(TypeDescriptor_Struct* type) {
		type->size = sizeof(glm::quat);
		type->name = "glm::quat";
		type->members.push_back({ "x", offsetof(glm::quat, x), reflect::TypeResolver<float>::get(), reflect::NoTag });
		type->members.push_back({ "y", offsetof(glm::quat, y), reflect::TypeResolver<float>::get(), reflect::NoTag });
		type->members.push_back({ "z", offsetof(glm::quat, z), reflect::TypeResolver<float>::get(), reflect::NoTag });
		type->members.push_back({ "w", offsetof(glm::quat, w), reflect::TypeResolver<float>::get(), reflect::NoTag });
	}

	template<>
	TypeDescriptor* getPrimitiveDescriptor<glm::quat>() {
		static TypeDescriptor_Struct typeDesc(init_quat_type);
		return &typeDesc;
	}
} // namespace reflect