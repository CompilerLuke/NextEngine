#include "mesh/surface_tet_mesh.h"
#include "mesh_generation/surface_point_placement.h"
#include "mesh_generation/cross_field.h"
#include "mesh_generation/point_octotree.h"
#include "mesh/input_mesh_bvh.h"

SurfacePointPlacement::SurfacePointPlacement(SurfaceTriMesh& tri_mesh, SurfaceCrossField& cross, PointOctotree& octotree, InputModelBVH& bvh)
: mesh(tri_mesh), cross_field(cross), octo(octotree), bvh(bvh) {

}

void SurfacePointPlacement::propagate() {
	//vector<FeatureEdge> features = cross_field.get_feature_edges();

	
}

