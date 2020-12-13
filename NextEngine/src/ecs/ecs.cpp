//
//  ecs.cpp
//  NextEngine
//
//  Created by Antonella Calvia on 01/11/2020.
//

#include "ecs/ecs.h"
#include "core/reflection.h"

struct ComponentDiff {
    uint component_id;
    uint mask;
    refl::DiffOfType diff;
};

void copy_diff(u8* dst, u8* src, refl::DiffOfType& diff) {
    printf("Copied diff %p %p\n", dst, src);
    for (refl::DiffField& field : diff.fields) {
        if (field.type == refl::UNCHANGED_DIFF) {
            memcpy(dst + field.current_offset, src + field.previous_offset, field.current_type->size);
        }
        else if (field.type == refl::REMOVED_DIFF || field.type == refl::ADDED_DIFF) {
            //TODO CALL DESTRUCTOR!
        }
        else {
            copy_diff(dst + field.current_offset, src + field.previous_offset, field.diff);
        }
    }
}

refl::Type* deep_copy(refl::Type* type, hash_map<refl::Type*, refl::Type*, 103>& copied) {
    if (type->type != refl::Type::Struct && type->type != refl::Type::Enum && type->type != refl::Type::Union) {
        return type;
    }
    
    if (refl::Type** type_copy = copied.get(type); type_copy) return *type_copy;
    
    refl::Type* type_copy;
    switch (type->type) {
        case refl::Type::Struct: {
            refl::Struct* copy = new refl::Struct(*(refl::Struct*)type);
            //copy->name = ;
            for (refl::Field& field : copy->fields) {
                field.type = deep_copy(field.type, copied);
            }
            type_copy = copy;
            break;
        }
        case refl::Type::Enum: {
            refl::Enum* copy = new refl::Enum(*(refl::Enum*)type);
            type_copy = copy;
            break;
        }
            
        case refl::Type::Union: {
            refl::Union* copy = new refl::Union(*(refl::Union*)type);

            for (refl::Field& field : copy->fields) {
                field.type = deep_copy(field.type, copied);
            }
            type_copy = copy;
            break;
        }
    }
    
    copied[type] = type_copy;
    return type_copy;
}


void World::register_components(slice<struct RegisterComponent> components) {
    refl::DiffOfType diffs[MAX_COMPONENTS];
    Archetype diff_mask = 0;
    
    //todo massive leak copied types are cleaned up!
    hash_map<refl::Type*, refl::Type*, 103> copied_type;
    
    for (struct RegisterComponent& component : components) {
        ID component_id = component.component_id;
        
        refl::Struct* previous = component_type[component_id];
        if (previous) {
            assert(component.type->type == refl::Type::Struct);
            
            refl::DiffOfType diff = refl::diff_type(previous, component.type);
            if (diff.type != refl::UNCHANGED_DIFF) {
                diff_mask |= 1ul << component_id;
                diffs[component_id] = diff;
            }
        }
        
        component_lifetime_funcs[component_id] = component.funcs;
        component_size[component_id] = component.type->size;
        component_type[component_id] = (refl::Struct*)deep_copy(component.type, copied_type);
        component_kind[component_id] = component.kind;
    }
    
    if (diff_mask == 0) return;
    
    //todo implement deep diff
    for (uint i = 0; i < ARCHETYPE_HASH; i++) {
        if (!arches.is_full(i)) continue;
        
        Archetype archetype = arches.keys[i];
        
        if (!(archetype & diff_mask)) continue;
        if (!arches.values[i].blocks) continue;
        
        ArchetypeStore store = arches.values[i];
        
        printf("Archetype was modified %llu, last block count %i\n", archetype, store.entity_count_last_block);
        
        bool changed_size = false;
        for (uint i = 0; i < MAX_COMPONENTS; i++) {
            Archetype mask = 1ul << i;
            if (mask & diff_mask && mask & archetype && diffs[i].current_size != diffs[i].previous_size) {
                changed_size = true;
                break;
            }
        }
        
        BlockHeader* block = store.blocks;
        ArchetypeStore* new_store;
        if (changed_size) {
            new_store = &make_archetype(archetype);
        }
        
        BlockHeader* copy_to = changed_size ? new_store->blocks : nullptr;
        uint entity_count = store.entity_count_last_block;
        uint entity_offset = 0;
        uint dst_entity_offset = 0;
        
        while (true) {
            u8* data = (u8*)(block + 1);
            
            if (changed_size) {
                u8* dst_data = (u8*)(copy_to + 1);
                uint count = min(entity_count - entity_offset, new_store->max_per_block - dst_entity_offset);
                
                for (uint comp = 0; comp < MAX_COMPONENTS; comp++) {
                    Archetype mask = 1ul << comp;
                    u8* component_data = data + store.offsets[comp];
                    u8* dst_component_data = dst_data + new_store->offsets[comp];
                    u64 size = component_size[comp];
                    
                    if ((mask & diff_mask) == 0) {
                        memcpy(dst_component_data + dst_entity_offset * size, component_data + entity_offset * size, count * component_size[comp]);
                    } else {
                        refl::DiffOfType& diff = diffs[comp];
                        
                        printf("Calling constructor count %i\n", count);
                        component_lifetime_funcs[comp].constructor(dst_component_data + dst_entity_offset*diff.current_size, count);
                        
                        for (uint i = 0; i < count; i++) {
                            copy_diff(dst_component_data + (i+dst_entity_offset) * diff.current_size, component_data + (i+entity_offset) * diff.previous_size, diff);
                        }
                    }
                    
                    for (uint i = 0; i < count; i++) {
                        id_to_ptr[comp][i] = dst_component_data + i * size;
                    }
                }
                
                entity_offset += count;
                dst_entity_offset += count;
                
                if (entity_offset == entity_count) {
                    entity_count = store.max_per_block;
                    entity_offset = 0;
                    
                    BlockHeader* last_block = block;
                    block = block->next;
                    
                    release_block(last_block); //Done copying from the block
                    
                    if (!block) break;
                }
                
                if (dst_entity_offset == entity_count) {
                    dst_entity_offset = 0;
                    
                    BlockHeader* new_block = get_block();
                    new_block->next = copy_to;
                    copy_to = new_block;
                }
            } else {
                
                if (!block) break;
            }
        }
        
        if (changed_size) {
            new_store->entity_count_last_block = dst_entity_offset;
            new_store->blocks = copy_to;
            
            printf("Last block count %i\n", new_store->entity_count_last_block);
        }
    }
}

World& World::operator=(const World& from) {
    memcpy(id_to_arch, from.id_to_arch, sizeof(id_to_arch));
    memcpy(component_type, from.component_type, sizeof(component_type));
    memcpy(component_size, from.component_size, sizeof(component_size));
    memcpy(component_lifetime_funcs, from.component_lifetime_funcs, sizeof(component_lifetime_funcs));
    memcpy(&arches, &from.arches, sizeof(arches));
    memcpy(&free_ids, &from.free_ids, sizeof(free_ids));

    //CLEAR
    memset(id_to_ptr, 0, sizeof(id_to_ptr));
    world_memory_offset = 0;
    block_free_list = nullptr;

    //COPY ARCHETYPES
    //todo replace 103 with constant
    for (uint i = 0; i < ARCHETYPE_HASH; i++) {
        if (!from.arches.is_full(i)) continue;

        Archetype arch = from.arches.keys[i];
        const ArchetypeStore& from_arch_store = from.arches.values[i];
        ArchetypeStore& arch_store = arches.values[i];

        BlockHeader* from_header = from_arch_store.blocks;
        if (!from_header) continue;
        
        BlockHeader* header = get_block();
        arch_store.blocks = header;

        uint entity_count = from_arch_store.entity_count_last_block;
        uint max_per_block = from_arch_store.max_per_block;

        while (from_header) {
            u8* from_data = (u8*)(from_header + 1);
            u8* data = (u8*)(header + 1);

            for (uint component = 0; component < MAX_COMPONENTS; component++) {
                if (!has_component(arch, component)) continue;
                u8* from_components = from_data + from_arch_store.offsets[component];
                u8* components = data + arch_store.offsets[component];

                uint size = component_size[component];

                auto copy = component_lifetime_funcs[component].copy;
                if (copy) copy(components, from_components, entity_count);
                else memcpy(components, from_components, size * entity_count);

                Entity* entities = (Entity*)data;
                for (uint entity = 0; entity < entity_count; entity++) {
                    ID id = entities[entity].id;
                    id_to_ptr[component][id] = components + entity * size;
                }
            }

            from_header = from_header->next;
            if (!from_header) break;
            
            header->next = get_block();
            header = header->next;
            entity_count = max_per_block;
        }
    }

    return *this;
}

ID World::clone(ID id) {
    Archetype arch = arch_of_id(id);
    ArchetypeStore& store = arches[arch];
    
    ID new_id = make_id();
    
    uint offset = store_make_space(store);
    u8* data = (u8*)(store.blocks + 1);
    
    for (uint i = 0; i < MAX_COMPONENTS; i++) {
        if (!has_component(arch, i)) continue;
        
        u8* src = (u8*)id_to_ptr[i][id];
        u8* dst = data + store.offsets[i] + component_size[i] * offset;
        
        auto func = component_lifetime_funcs[i].copy;
        if (func) func(dst, src, 1);
        else memcpy(dst, src, component_size[i]);
        
        id_to_ptr[i][new_id] = dst;
    }
    
    id_to_arch[new_id] = arch;
    
    return new_id;
}
