#include "stdafx.h"
#include "reflection/reflection.h"
#include <glm/vec3.hpp>
#include <glm/vec2.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/quaternion.hpp>
#include "editor/displayComponents.h"

#define PRIMITIVE_TYPE_DESCRIPTOR(type) \
struct TypeDescriptor_##type : TypeDescriptor { \
	TypeDescriptor_##type() : TypeDescriptor{ #type , sizeof(type) } {}; \
	bool render_fields(void* data, const std::string& prefix, struct World& world) { \
		return render_fields_primitive((type*)data, prefix); \
	} \
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

	PRIMITIVE_TYPE_DESCRIPTOR(float);
	PRIMITIVE_TYPE_DESCRIPTOR(bool);
	PRIMITIVE_TYPE_DESCRIPTOR(int);
	
	using UINT = unsigned int;
	PRIMITIVE_TYPE_DESCRIPTOR(UINT);
	//--------------------------------------------------------
	// A type descriptor for std::string
	//--------------------------------------------------------

	struct TypeDescriptor_StdString : TypeDescriptor {
		TypeDescriptor_StdString() : TypeDescriptor{ "std::string", sizeof(std::string) } {
		}
		bool render_fields(void* data, const std::string& prefix, struct World& world) {
			render_fields_primitive((std::string*)data, prefix); 
			return true;
		} 
	};

	template <>
	TypeDescriptor* getPrimitiveDescriptor<std::string>() {
		static TypeDescriptor_StdString typeDesc;
		return &typeDesc;
	}

	struct TypeDescriptor_GlmVec3 : TypeDescriptor {
		TypeDescriptor_GlmVec3() : TypeDescriptor{ "glm::vec3", sizeof(glm::vec3) } {

		}
		bool render_fields(void* data, const std::string& prefix, struct World& world) {
			return render_fields_primitive((glm::vec3*)data, prefix);
		}
	};

	template<>
	TypeDescriptor* getPrimitiveDescriptor<glm::vec3>() {
		static TypeDescriptor_GlmVec3 typeDesc;
		return &typeDesc;
	}

	struct TypeDescriptor_GlmVec2 : TypeDescriptor {
		TypeDescriptor_GlmVec2() : TypeDescriptor{ "glm::vec2", sizeof(glm::vec2) } {}
	
		bool render_fields(void* data, const std::string& prefix, struct World& world) {
			return render_fields_primitive((glm::vec2*)data, prefix);
		}
	};

	template<>
	TypeDescriptor* getPrimitiveDescriptor<glm::vec2>() {
		static TypeDescriptor_GlmVec2 typeDesc;
		return &typeDesc;
	}

	struct TypeDescriptor_Mat4 : TypeDescriptor {
		TypeDescriptor_Mat4() : TypeDescriptor{ "glm::mat4", sizeof(glm::vec3)} {}
		bool render_fields(void* data, const std::string& prefix, struct World& world) {
			return render_fields_primitive((glm::mat4*)data, prefix);
		}
	};

	template<>
	TypeDescriptor* getPrimitiveDescriptor<glm::mat4>() {
		static TypeDescriptor_Mat4 typeDesc;
		return &typeDesc;
	}

	struct TypeDescriptor_GlmQuat : TypeDescriptor {
		TypeDescriptor_GlmQuat() : TypeDescriptor{ "glm::quat", sizeof(glm::quat) } {}

		bool render_fields(void* data, const std::string& prefix, struct World& world) {
			return render_fields_primitive((glm::quat*)data, prefix);
		}
	};

	template<>
	TypeDescriptor* getPrimitiveDescriptor<glm::quat>() {
		static TypeDescriptor_GlmQuat typeDesc;
		return &typeDesc;
	}
} // namespace reflect