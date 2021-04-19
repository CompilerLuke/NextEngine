#include "mesh_generation/cross_field.h"
#include "visualization/debug_renderer.h"
#include "visualization/color_map.h"
#include <glm/gtx/rotate_vector.hpp>

SurfaceCrossField::SurfaceCrossField(SurfaceTriMesh& mesh, CFDDebugRenderer& debug, slice<edge_handle> feature_edges)
: mesh(mesh), feature_edges(feature_edges), debug(debug) {

	centers.resize(mesh.tri_count);
	theta_cell_center[0].resize(mesh.tri_count);
	theta_cell_center[1].resize(mesh.tri_count);
	distance_cell_center[0].resize(mesh.tri_count);
	distance_cell_center[1].resize(mesh.tri_count);

	edge_flux_stencil.resize(mesh.tri_count * 3);
	edge_flux_boundary.resize(mesh.tri_count * 3);

	current = 0;
}

const float scale = 0.2;

void draw_cross(CFDDebugRenderer& debug, vec3 center, Cross& cross, vec4 color = vec4(0,0,0,1)) {
	draw_line(debug, center - scale * cross.normal, center + scale * cross.normal, color);
	draw_line(debug, center - scale * cross.tangent, center + scale * cross.tangent, color);
	draw_line(debug, center - scale * cross.bitangent, center + scale * cross.bitangent, color);
}

float angle_between(vec3 v0, vec3 v1, vec3 vn) {
	float angle = acos(clamp(-1, 1, dot(v0, v1)));
	vec3 v2 = cross(v0, v1);
	if (dot(vn, v2) < 0) { // Or > 0
		angle = -angle;
	}
	return angle;
}

void SurfaceCrossField::propagate() {
	for (tri_handle tri : mesh) {
		vec3 p[3];
		mesh.triangle_verts(tri, p);

		vec3 n = triangle_normal(p); 

		theta_cell_center[0][tri / 3].normal = n;
		theta_cell_center[1][tri / 3].normal = n;
		centers[tri / 3] = (p[0] + p[1] + p[2]) / 3.0f;
	}
	
	for (edge_handle edge : feature_edges) {
		vec3 e0, e1;
		mesh.edge_verts(edge, &e0, &e1);
		
		vec3 v0[3];
		vec3 v1[3];
		mesh.triangle_verts(TRI(edge), v0);
		mesh.triangle_verts(TRI(mesh.edges[edge]), v1);

		vec3 normal1 = triangle_normal(v0);
		vec3 normal2 = triangle_normal(v1);
		vec3 tangent = normalize(e1-e0);
		vec3 bitangent1 = cross(normal1, tangent);
		vec3 bitangent2 = cross(normal2, tangent);

		vec3 center = (e0 + e1) / 2;

		Cross cross1{ tangent,normal1,bitangent1 };
		Cross cross2{ tangent,normal2,bitangent2 };

		draw_cross(debug, center, cross1);
		draw_cross(debug, center, cross2);

		edge_flux_stencil[edge] = true;
		edge_flux_stencil[mesh.edges[edge]] = true;
		edge_flux_boundary[edge] = cross1;
		edge_flux_boundary[mesh.edges[edge]] = cross2;
	}

	uint n_it = 100;
	for (uint i = 0; i < n_it; i++) {
		bool draw = i = i % 1 == 0;
		if (draw) {
			suspend_execution(debug);
			clear_debug_stack(debug);
		}

		bool past = !current;

		for (tri_handle tri : mesh) {
			Cross cross1 = theta_cell_center[past][tri / 3];
			float distance1 = distance_cell_center[past][tri / 3];
			vec3 center = centers[tri / 3];

			bool never_assigned = cross1.tangent == 0;
			vec3 basis = never_assigned ? normalize(cross(cross1.normal, vec3(1, 0, 0))) : cross1.tangent;

			//CROSS
			uint active_edges = 0;
			float sum_of_theta = 0;
			float weight = 0;
			for (uint i = 0; i < 3; i++) {
				edge_handle edge = tri + i;
				edge_handle opp_edge = mesh.edges[edge];

				bool is_boundary_edge = edge_flux_stencil[edge];
				Cross cross2 = is_boundary_edge ? edge_flux_boundary[edge] : theta_cell_center[past][opp_edge / 3];
				
				if (!is_boundary_edge && cross2.tangent == 0.0) continue;

				float dx = length(center - centers[opp_edge/3]);
				float w = (is_boundary_edge ? 10 : 1) / dx; 

				float theta = cross2.angle_between(basis, cross1.normal);
				
				sum_of_theta += w * theta;
				weight += w;
				active_edges++;
			}

			if (weight == 0.0f) continue;
			sum_of_theta /= weight;

			const float f = 0.7;

			cross1.tangent = glm::rotate(glm::vec3(basis), never_assigned ? sum_of_theta : f * sum_of_theta, glm::vec3(cross1.normal));
			cross1.bitangent = cross(cross1.normal, cross1.tangent);

			//DISTANCE
			float min_distance = never_assigned ? FLT_MAX : distance1;
			

			for (uint i = 0; i < 3; i++) {
				edge_handle edge = tri + i;
				edge_handle opp_edge = mesh.edges[edge];

				bool is_boundary_edge = edge_flux_stencil[edge];

				vec3 e0, e1;
				mesh.edge_verts(edge, &e0, &e1);

				vec3 edge_center = (e0 + e1) / 2.0f;
				float distance2 = is_boundary_edge ? 0 : distance_cell_center[past][opp_edge / 3];
				if (!is_boundary_edge && distance2 == 0) continue;
				if (!never_assigned && distance2 > distance1) continue;

				vec3 center2 = is_boundary_edge ? edge_center : centers[opp_edge / 3];
				vec3 dist_vec = center2 - center;

				float length_in_cross_dir;
				cross1.best_axis(dist_vec, &length_in_cross_dir);
				//length_in_cross_dir = fabs(dot(cross1.bitangent, dist_vec));
				distance2 += length_in_cross_dir;

				min_distance = fminf(min_distance, distance2);
			}

			vec4 colors[4] = {vec4(0,0,0,1), vec4(0,0,1,1), vec4(0,1,0,1), vec4(0,0,0,1)};
			if (draw) {
				vec3 p[3];
				mesh.triangle_verts(tri, p);

				draw_triangle(debug, p, color_map(min_distance, 0, 10.0f));
				draw_cross(debug, center, cross1, colors[active_edges]);
			}

			theta_cell_center[current][tri / 3] = cross1;
			distance_cell_center[current][tri / 3] = min_distance;
		}

		current = past;
	}
}

/*
vector<FeatureEdge> SurfaceCrossField::get_feature_edges() {
	vector<FeatureEdge> features;
	for (edge_handle edge : feature_edges) {
		features.append({ edge, edge_flux_boundary[edge], edge_flux_stencil[mesh.edges[edge]] });
	}

	return features;
}
*/