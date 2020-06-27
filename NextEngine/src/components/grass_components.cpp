#include "components/grass.h"

REFLECT_STRUCT_BEGIN(Grass)
REFLECT_STRUCT_MEMBER(placement_model)
REFLECT_STRUCT_MEMBER(cast_shadows)
REFLECT_STRUCT_MEMBER(width)
REFLECT_STRUCT_MEMBER(height)
REFLECT_STRUCT_MEMBER(max_height)
REFLECT_STRUCT_MEMBER(density)
REFLECT_STRUCT_MEMBER(random_rotation)
REFLECT_STRUCT_MEMBER(random_scale)
REFLECT_STRUCT_MEMBER(align_to_terrain_normal)
REFLECT_STRUCT_MEMBER_TAG(transforms, reflect::HideInInspectorTag)
REFLECT_STRUCT_END()
