#pragma once

#include "graphics/culling/aabb.h"
#include "mesh.h"

struct GeometryNode {
	uint offset; 
};

struct GeometryAS {
	slice<CFDPolygon> polygons;
	slice<CFDVertex> vertices;
	vector<GeometryLeaf>
};
