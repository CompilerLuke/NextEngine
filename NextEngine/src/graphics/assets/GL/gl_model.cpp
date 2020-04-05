#include "stdafx.h"

#ifdef RENDER_API_OPENGL

#include "core/memory/linear_allocator.h"
#include "graphics/rhi/draw.h"
#include "graphics/renderer/renderer.h"
#include "graphics/renderer/material_system.h"
#include "graphics/assets/model.h"

REFLECT_STRUCT_BEGIN(Vertex)
REFLECT_STRUCT_MEMBER(position)
REFLECT_STRUCT_MEMBER(normal)
REFLECT_STRUCT_MEMBER(tex_coord)
REFLECT_STRUCT_MEMBER(tangent)
REFLECT_STRUCT_MEMBER(bitangent)
REFLECT_STRUCT_END()

/*
REFLECT_STRUCT_BEGIN(Mesh)
REFLECT_STRUCT_MEMBER(material_id)

REFLECT_STRUCT_MEMBER(vertices_length)
REFLECT_STRUCT_MEMBER(vertices_offset)
REFLECT_STRUCT_MEMBER(indices_length)
REFLECT_STRUCT_MEMBER(indices_offset)
REFLECT_STRUCT_END()*/

ModelManager::ModelManager(Level& level) : level(level) {

}
#endif