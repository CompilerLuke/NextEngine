#include "mesh_generation/surface_point_placement.h"
#include "mesh_generation/cross_field.h"

SurfacePointPlacement::SurfacePointPlacement(SurfaceTriMesh& mesh, SurfaceCrossField& cross, PointOctotree& octotree, InputModelBVH& bvh)
: mesh(mesh), cross_field(cross), octo(octotree), bvh(bvh) {

}

void SurfacePointPlacement::propagate() {
	//vector<FeatureEdge> features = cross_field.get_feature_edges();

	
}

