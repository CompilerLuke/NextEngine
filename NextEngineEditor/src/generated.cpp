#include "C:\Users\User\source\repos\NextEngine\NextEngineEditor\src\generated.h"
#include "core/memory/linear_allocator.h"
#include "core/serializer.h"
#include "core/reflection.h"
#include "core/container/array.h"
#include "C:\Users\User\source\repos\NextEngine\NextEngineEditor\include\lister.h"

refl::Struct init_EntityNode() {
	refl::Struct type("EntityNode", sizeof(EntityNode));
	type.fields.append({"name", offsetof(EntityNode, name), refl::TypeResolver<sstring>::get() });
	type.fields.append({"id", offsetof(EntityNode, id), refl::TypeResolver<ID>::get() });
	type.fields.append({"expanded", offsetof(EntityNode, expanded), refl::TypeResolver<bool>::get() });
	type.fields.append({"children", offsetof(EntityNode, children), refl::TypeResolver<vector<EntityNode>>::get() });
	return type;
}

void write_to_buffer(SerializerBuffer& buffer, EntityNode& data) {
    write_to_buffer(buffer, data.name);
    write_to_buffer(buffer, data.id);
    write_to_buffer(buffer, data.expanded);
	for (uint i = 0; i < data.children.length; i++) {
         write_to_buffer(buffer, data.children[i]);
    }
}

 refl::Type* refl::TypeResolver<EntityNode>::get() {
	static refl::Struct type = init_EntityNode();
	return &type;
}
