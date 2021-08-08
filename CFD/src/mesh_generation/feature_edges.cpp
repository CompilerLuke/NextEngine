#include "visualization/debug_renderer.h"
#include "mesh/surface_tet_mesh.h"
#include "mesh/feature_edges.h"
#include "mesh/edge_graph.h"
#include "visualization/color_map.h"
#include <algorithm>

tvector<float> curvature_at_verts(SurfaceTriMesh& surface, EdgeGraph& graph, CFDDebugRenderer& debug) {
    tvector<float> vert_curvatures;
    vert_curvatures.resize(surface.positions.length);
    
    for (int i = 1; i < surface.positions.length; i++) {
        auto neighbors = graph.neighbors({i});
        vec3 v0 = surface.positions[i];
        
        float total_angle = 0.0f;
        float total_area = 0.0f;
        
        for (uint i = 0; i < neighbors.length; i++) {
            edge_handle e0 = neighbors[i];
			edge_handle e1 = surface.next_edge(e0, 2);
            
            vec3 v1 = surface.position(e0);
            vec3 v2 = surface.position(e1);
            
            //draw_line(debug, v0, v1);
            //draw_line(debug, v0, v2);
            //suspend_execution(debug);
            
            float angle = acosf(dot(v1-v0, v2-v0) / (length(v1-v0) * length(v2-v0)));
            total_angle += angle;
            
            total_area += length(cross(v1-v0, v2-v0))/2.0f; //magnitude is parallelgram of two vectors -> divided by two, becomes area of triangle
        }
        
        float A = total_area/3;
        float curvature = (2*PI - total_angle) / A;
        vert_curvatures[i] = curvature;


    }
    
    return vert_curvatures;
}

tvector<FeatureCurve> identify_feature_edges(SurfaceTriMesh& surface, EdgeGraph& edge_graph, float feature_angle, float min_quality, CFDDebugRenderer& debug) {
	struct DihedralID {
		float dihedral;
		edge_handle edge;
	};	
	
	uint N = surface.N;

	vec3* normals = TEMPORARY_ZEROED_ARRAY(vec3, surface.tri_count);
	vec3* centers = TEMPORARY_ZEROED_ARRAY(vec3, surface.tri_count);
	float* dihedrals = TEMPORARY_ZEROED_ARRAY(float, N * surface.tri_count);
	bool* visited = TEMPORARY_ZEROED_ARRAY(bool, N * surface.tri_count);

	const float episilon = 0.002;



	for (tri_handle i : surface) {
		vec3 positions[3];
		surface.triangle_verts(i, positions);

		vec3 center = (positions[0]+positions[1]+positions[2])/3;
		normals[i / N] = triangle_normal(positions);
		centers[i / N] = center; // +episilon * normals[i / N];
	}

	tvector<FeatureCurve> result;

	feature_angle = to_radians(feature_angle);

	tvector<DihedralID> largest_dihedral;

	for (tri_handle i : surface) {
		for (uint j = 0; j < 3; j++) {
			tri_handle neighbor = surface.neighbor(i, j);
			float dist = length(centers[i / N] - centers[neighbor / N]);
			dihedrals[i + j] = acos(dot(normals[i / N], normals[neighbor / N]));

			if (surface.indices[i + j].id > surface.indices[i + (j + 1) % 3].id) continue;
			
			float dihedral = dihedrals[i + j]; // / dist;
			
			if (dihedral > feature_angle) largest_dihedral.append({ dihedral, i + j });
		}
	}

	std::sort(largest_dihedral.begin(), largest_dihedral.end(), [](DihedralID& a, DihedralID& b) { return a.dihedral > b.dihedral; });


	tvector<edge_handle> strong_edges;
	tvector<vec3> positions;

	tvector<edge_handle> possible_strong_edges[2];
	tvector<vec3> possible_positions[2];

	for (DihedralID id : largest_dihedral) {
		edge_handle starting_edge = id.edge;
		if (visited[starting_edge]) continue;

		//vec3 p0, p1;
		//surface.edge_verts(starting_edge, &p0, &p1);
		//draw_line(debug, p0, p1, vec4(0, 1, 0, 1));

		for (uint i = 0; i < 2; i++) {
			uint count = 0;

			edge_handle current_edge = i == 1 ? surface.edges[starting_edge] : starting_edge;
			possible_strong_edges[i].clear();
			possible_positions[i].clear();

			if (visited[current_edge]) break; //completed full loop

			while (count++ < 10000) {
				visited[current_edge] = true;
				visited[surface.edges[current_edge]] = true;

				possible_strong_edges[i].append(current_edge);

				vertex_handle v0, v1;
				surface.edge_verts(current_edge, &v0, &v1);



				vec3 p0 = surface.positions[v0.id];
				vec3 p1 = surface.positions[v1.id];
				vec3 p01 = p1 - p0;

				//draw_line(debug, p0, p1, RED_DEBUG_COLOR);

				auto neighbors = edge_graph.neighbors(v1);
				float best_score = 0.0f;

				uint best_edge = 0;

				possible_positions[i].append(p0);

				float dihedral1 = dihedrals[current_edge];

				for (edge_handle edge : neighbors) {
					if (!edge) continue;
					vec3 p2 = surface.position(edge, 0);

					if (p2 == p0) continue;
					vec3 p12 = p2 - p1;
					float dihedral2 = dihedrals[edge];

					float edge_dot = dot(normalize(p01), normalize(p12));
					float dihedral_simmilarity = 1 - fabs(dihedral1 - dihedral2) / fmaxf(dihedral1, dihedral2);

					float score = edge_dot * dihedral_simmilarity;

					//draw_line(debug, p2, p1, color_map(score, 0, 1));
					//suspend_execution(debug);

					if (score > best_score) {
						best_edge = edge;
						best_score = score;
					}
				}

				if (best_score > min_quality) current_edge = surface.edges[best_edge]; //p0 --> p0 <-- p2 : p0 --> p1 --> p2
				else {
					vec3 p1 = surface.position(surface.edges[current_edge]);
					possible_positions[i].append(p1);
					break;
				}

				if (visited[current_edge]) {
					vec3 p1 = surface.position(surface.next_edge(surface.edges[current_edge]));
					possible_positions[i].append(p1);
					break;
				}
			}
		}

		strong_edges.clear();
		positions.clear();

		bool edge_loop1 = possible_strong_edges[0].length > 1;
		bool edge_loop2 = possible_strong_edges[1].length > 1;
		bool merge = edge_loop1 && edge_loop2;

		if (merge) {
			//edge_loop1           m--------------------->
			//edge_loop2 <---------m
			//merge      ----------m---------------------->

			uint n = possible_positions[1].length;
			for (uint i = n - 1; i > 1; i--) {
				strong_edges.append(surface.edges[possible_strong_edges[1][i-1]]);
				positions.append(possible_positions[1][i]);
			}

			strong_edges += possible_strong_edges[0];
			positions += possible_positions[0];
		}
		else {
			uint index = edge_loop2;
			strong_edges += possible_strong_edges[index];
			positions += possible_positions[index];
		}

		if (strong_edges.length > 0) {
			result.append({
				Spline(positions),
				strong_edges
			});
		}
	}

	//suspend_execution(debug);

	const uint n = 11;
	vec4 colors[n] = {
		rgb(9, 189, 75),
		rgb(250, 179, 15),
		rgb(187, 250, 15),
		rgb(17, 209, 199),
		rgb(190, 14, 235),
		rgb(81, 76, 237),
		rgb(76, 237, 181),
		rgb(181, 13, 24),
		rgb(53, 79, 130),
		rgb(58, 148, 97),
		rgb(227, 146, 48),
	};

	uint i = 0; 
    for (FeatureCurve curve : result) {
		vec3 p0, p1;
		uint res = 10;
		
#if 0
		for (uint i = 0; i < curve.edges.length * res; i++) {
			p1 = curve.spline.at_time((float)i / res);
			if (i > 0) {
				draw_line(debug, p0, p1, RED_DEBUG_COLOR);
			}

			p0 = p1;
		}
		suspend_execution(debug);
#endif
        for (edge_handle edge : curve.edges) {
            vec3 p0, p1;
            surface.edge_verts(edge, &p0, &p1);
            p0 += normals[edge / N] * episilon;
            p1 += normals[edge / N] * episilon;
			vec4 color = colors[i % n];
            draw_line(debug, p0, p1, color);
			//suspend_execution(debug);
        }
		i++;
		//printf("=============\n");
    }
	suspend_execution(debug);
	return result;
}
