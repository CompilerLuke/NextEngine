#include "C:\Users\User\source\repos\NextEngine\NextEngineEditor\src\generated.h"
#include "core/memory/linear_allocator.h"
#include "core/serializer.h"
#include "core/reflection.h"
#include "core/container/array.h"
#include "C:\Users\User\source\repos\NextEngine\NextEngineEditor\include\lister.h"

refl::Struct init_EntityEditor() {
	refl::Struct type("EntityEditor", sizeof(EntityEditor));
	type.fields.append({"name", offsetof(EntityEditor, name), refl::TypeResolver<string_buffer>::get() });
	type.fields.append({"children", offsetof(EntityEditor, children), refl::TypeResolver<vector<ID>>::get() });
	return type;
}

void write_to_buffer(SerializerBuffer& buffer, EntityEditor& data) {
    write_to_buffer(buffer, data.name);
	for (uint i = 0; i < data.children.length; i++) {
         write_to_buffer(buffer, data.children[i]);
    }
}

 refl::Type* refl::TypeResolver<EntityEditor>::get() {
	static refl::Struct type = init_EntityEditor();
	return &type;
}
