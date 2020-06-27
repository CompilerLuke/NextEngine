#include "core/memory/linear_allocator.h"
#include "core/reflection2.h"
#include "components/transform.h"
LinearAllocator reflection_allocator(kb(512));

StructType init_Trans() {
	StructType type;
	type.type = Type::Struct;
	type.name = "Trans";
	type.fields.allocator = &reflection_allocator;
	type.fields.append({"position", offsetof(Trans, position), get_type<glm::vec3>() });
	type.fields.append({"rotation", offsetof(Trans, rotation), get_type<glm::quat>() });
	type.fields.append({"scale", offsetof(Trans, scale), get_type<glm::vec3>() });

	return type;
}

template<>
Type* get_type<Trans>() {
	static StructType type = init_Trans();
	return &type;
};

