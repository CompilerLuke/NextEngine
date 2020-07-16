#include "generated.h"
#include <core/types.h>
#include "lister.h"

refl::Struct init_EntityNode() {
	refl::Struct type("EntityNode", sizeof(EntityNode));
	type.fields.append({"name", offsetof(EntityNode, name), get_sstring_type()});
	type.fields.append({"id", offsetof(EntityNode, id), get_ID_type()});
	type.fields.append({"expanded", offsetof(EntityNode, expanded), get_bool_type()});
	type.fields.append({"children", offsetof(EntityNode, children), make_vector_type(get_EntityNode_type())});
	type.fields.append({"parent", offsetof(EntityNode, parent), get_int_type()});
	return type;
}

void write_EntityNode_to_buffer(SerializerBuffer& buffer, EntityNode& data) {
    write_sstring_to_buffer(buffer, data.name);
    write_ID_to_buffer(buffer, data.id);
    write_n_to_buffer(buffer, &data.expanded, sizeof(bool));
    write_uint_to_buffer(buffer, data.children.length);
	for (uint i = 0; i < data.children.length; i++) {
         write_EntityNode_to_buffer(buffer, data.children[i]);
    }
    write_n_to_buffer(buffer, &data.parent, sizeof(int));
}

void read_EntityNode_from_buffer(DeserializerBuffer& buffer, EntityNode& data) {
    read_sstring_from_buffer(buffer, data.name);
    read_ID_from_buffer(buffer, data.id);
    read_n_from_buffer(buffer, &data.expanded, sizeof(bool));
    read_uint_from_buffer(buffer, data.children.length);
    data.children.resize(data.children.length);
	for (uint i = 0; i < data.children.length; i++) {
         read_EntityNode_from_buffer(buffer, data.children[i]);
    }
    read_n_from_buffer(buffer, &data.parent, sizeof(int));
}

refl::Struct* get_EntityNode_type() {
	static refl::Struct type = init_EntityNode();
	return &type;
}

#include "C:\Users\User\source\repos\NextEngine\NextEngineEditor\/include/component_ids.h"#include "ecs/ecs.h"
#include "engine/application.h"


APPLICATION_API void register_components(World& world) {

};