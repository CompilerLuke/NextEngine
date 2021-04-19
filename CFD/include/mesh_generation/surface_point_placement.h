#pragma once

struct SurfaceTriMesh;
struct SurfaceCrossField;
struct PointOctotree;
struct InputModelBVH;

class SurfacePointPlacement {
	SurfaceTriMesh& mesh;
	SurfaceCrossField& cross_field;
	PointOctotree& octo;
	InputModelBVH& bvh;

public:
	SurfacePointPlacement(SurfaceTriMesh&, SurfaceCrossField&, PointOctotree&, InputModelBVH&);

	void propagate();
};