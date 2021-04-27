#include "visualization/debug_renderer.h"
#include "mesh/surface_tet_mesh.h"
#include "mesh/feature_edges.h"
#include "mesh/edge_graph.h"
#include <algorithm>

FeatureCurves identify_feature_edges(SurfaceTriMesh& surface, float feature_angle, float min_quality, CFDDebugRenderer& debug) {
	struct DihedralID {
		float dihedral;
		edge_handle edge;
	};

	EdgeGraph edge_graph = build_edge_graph(surface);
	vec3* normals = TEMPORARY_ZEROED_ARRAY(vec3, surface.tri_count);
	vec3* centers = TEMPORARY_ZEROED_ARRAY(vec3, surface.tri_count);
	float* dihedrals = TEMPORARY_ZEROED_ARRAY(float, 3 * surface.tri_count);
	bool* visited = TEMPORARY_ZEROED_ARRAY(bool, 3 * surface.tri_count);
	DihedralID* largest_dihedral = TEMPORARY_ZEROED_ARRAY(DihedralID, 3 * surface.tri_count);

	const float episilon = 0.002;

	for (tri_handle i : surface) {
		vec3 positions[3];
		vec3 center;
		for (uint j = 0; j < 3; j++) {
			positions[j] = surface.positions[surface.indices[i + j].id];
			center += positions[j];
		}

		center /= 3;
		normals[i / 3] = triangle_normal(positions);
		centers[i / 3] = center; // +episilon * normals[i / 3];
	}

	FeatureCurves result;

	feature_angle = to_radians(feature_angle);

	for (tri_handle i : surface) {
		for (uint j = 0; j < 3; j++) {
			tri_handle neighbor = surface.neighbor(i, j);
			float dist = length(centers[i / 3] - centers[neighbor / 3]);
			dihedrals[i + j] = acos(dot(normals[i / 3], normals[neighbor / 3]));

			if (surface.indices[i + j].id > surface.indices[i + (j + 1) % 3].id) continue;
			largest_dihedral[i + j].dihedral = dihedrals[i + j] / dist;
			largest_dihedral[i + j].edge = i + j;
		}
	}

	std::sort(largest_dihedral, largest_dihedral + 3 * surface.tri_capacity, [](DihedralID& a, DihedralID& b) { return a.dihedral > b.dihedral; });

	tvector<edge_handle> possible_strong_edges;
	tvector<vec3> positions;

	for (uint i = 0; i < 3 * surface.tri_capacity; i++) {
		if (largest_dihedral[i].dihedral < feature_angle) break;

		edge_handle starting_edge = largest_dihedral[i].edge;

		for (uint i = 0; i < 1; i++) {
			uint count = 0;

			edge_handle current_edge = i == 1 ? surface.edges[starting_edge] : starting_edge;
			possible_strong_edges.length = 0;
			positions.length = 0;

			while (count++ < 10000) {
				visited[current_edge] = true;
				visited[surface.edges[current_edge]] = true;

				possible_strong_edges.append(current_edge);

				vertex_handle v0, v1;
				surface.edge_verts(current_edge, &v0, &v1);

				vec3 p0 = surface.positions[v0.id];
				vec3 p1 = surface.positions[v1.id];
				vec3 p01 = p0 - p1;

				auto neighbors = edge_graph.neighbors(v0);
				float best_score = 0.0f;

				uint best_edge = 0;

				positions.append(p0);

				float dihedral1 = dihedrals[current_edge];
				//draw_line(debug, p0, p1, vec4(1, 0, 0, 1));
				//suspend_execution(debug);

				for (edge_handle edge : neighbors) {
					vec3 p2 = surface.position(edge, 0);
					if (p2 == p1) continue;
					vec3 p12 = p2 - p0;
					float dihedral2 = dihedrals[edge];

					float edge_dot = dot(normalize(p01), normalize(p12));
					float dihedral_simmilarity = 1 - fabs(dihedral1 - dihedral2) / fmaxf(dihedral1, dihedral2);

					float score = edge_dot * dihedral_simmilarity;

					if (score > best_score) {
						best_edge = edge;
						best_score = score;
					}
				}

				if (best_score > min_quality) current_edge = best_edge;
				else break;

				if (visited[current_edge]) {
					//strong_edges += possible_strong_edges;
					break;
				}
			}

			if (possible_strong_edges.length > 5) {
				result.splines.append(Spline(positions));
				result.edges += possible_strong_edges;
			}
		}
	}

	for (edge_handle edge : result.edges) {
		vec3 p0, p1;
		surface.edge_verts(edge, &p0, &p1);
		p0 += normals[edge / 3] * episilon;
		p1 += normals[edge / 3] * episilon;
		draw_line(debug, p0, p1, vec4(1, 0, 0, 1));
	}

	return result;
}