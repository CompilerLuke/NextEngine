#pragma once

#include "core/math/aabb.h"
#include "mesh.h"

struct GeometryNode {
	uint offset; 
};

struct GeometryAS {
	slice<CFDPolygon> polygons;
	slice<CFDVertex> vertices;
	vector<GeometryLeaf>
};
