#include "mesh/surface_tet_mesh.h"
#include "mesh_generation/cross_field.h"
#include "visualization/debug_renderer.h"
#include "visualization/color_map.h"
#include <glm/gtx/rotate_vector.hpp>
#include "core/job_system/job.h"
#include "core/memory/linear_allocator.h"

SurfaceCrossField::SurfaceCrossField(SurfaceTriMesh& mesh, CFDDebugRenderer& debug, slice<edge_handle> feature_edges)
: mesh(mesh), feature_edges(feature_edges), debug(debug) {

	centers.resize(mesh.tri_count);
	theta_cell_center[0].resize(mesh.tri_count);
	theta_cell_center[1].resize(mesh.tri_count);
    edge_flux_stencil[0].resize(mesh.tri_count / 32 + 1);
    edge_flux_stencil[1].resize(mesh.tri_count / 32 + 1);
	distance_cell_center[0].resize(mesh.tri_count);
	distance_cell_center[1].resize(mesh.tri_count);

	edge_flux_boundary.resize(mesh.tri_count * 3);

	current = 0;
}

float angle_between(vec3 v0, vec3 v1, vec3 vn) {
	float angle = acos(clamp(-1, 1, dot(v0, v1)));
	vec3 v2 = cross(v0, v1);
	if (dot(vn, v2) < 0) { // Or > 0
		angle = -angle;
	}
	return angle;
}

struct PropagateJob {
    uint begin;
    uint end;
    vec3* centers;
    edge_handle* edges;
    Cross* edge_flux_boundary;
    Cross* cross_center_in;
    uint* active_in;
    
    Cross* cross_center_out;
    uint* active_out;
    float residual;
};

void propagate_cross(PropagateJob& job) {
    float residual = 0.0f;
    uint nskipped = false;
    
    for (uint i = job.begin; i < job.end; i++) {
        uint active = job.active_in[i / 32];
        uint mask = 1 << i%32;
        if (!active && i%32 == 0) {
            i += 31;
            nskipped += 32;
            continue;
        }
        if (!(active & mask)) {
            nskipped++;
            continue;
        }
        
        job.active_out[i / 32] |= mask;

        Cross cross1 = job.cross_center_in[i];
        vec3 center = job.centers[i];

        bool never_assigned = cross1.tangent == 0;
        vec3 basis = never_assigned ? normalize(cross(cross1.normal, vec3(1, 0, 0))) : cross1.tangent;

        //CROSS
        uint active_edges = 0;
        float sum_of_theta = 0;
        float weight = 0;
        for (uint j = 0; j < 3; j++) {
            edge_handle edge = i*3 + j;
            edge_handle opp_edge = job.edges[edge];
            uint opp_cell = opp_edge / 3;
            uint mask = 1 << opp_cell % 32;
            
            Cross edge_flux_boundary = job.edge_flux_boundary[edge];
            Cross opp_cross = job.cross_center_in[opp_cell];
            
            bool is_boundary_edge = !(edge_flux_boundary.tangent == 0.0);
            bool active = !(opp_cross.tangent == 0.0);
            
            job.active_out[opp_cell / 32] |= mask;
            
            if (!is_boundary_edge && !active) continue;
            
            Cross cross2 = is_boundary_edge ? edge_flux_boundary : opp_cross;
            
            float dx = length(center - job.centers[opp_cell]);
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
        
        job.cross_center_out[i] = cross1;
        
        residual += fabs(sum_of_theta);
    }
    
    residual /= job.end - job.begin;
    job.residual = nskipped ? 1000 : residual;
}

const float scale = 0.2;

void draw_cross(CFDDebugRenderer& debug, vec3 center, Cross cross, vec4 color) {
    draw_line(debug, center - scale * cross.normal, center + scale * cross.normal, color);
    draw_line(debug, center - scale * cross.tangent, center + scale * cross.tangent, color);
    draw_line(debug, center - scale * cross.bitangent, center + scale * cross.bitangent, color);
}

void compute_distance() {
    /*
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
    */
}

float average(slice<float> values) {
    float sum = 0.0f;
    for (float value : values) {
        sum += value;
    }
    return sum / values.length;
}

void SurfaceCrossField::propagate() {
    //suspend_execution(debug);
    //clear_debug_stack(debug);
    
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
        
        tri_handle tri1 = TRI(edge);
        tri_handle tri2 = TRI(mesh.edges[edge]);
		
		vec3 v0[3];
		vec3 v1[3];
		mesh.triangle_verts(tri1, v0);
		mesh.triangle_verts(tri2, v1);

		vec3 normal1 = triangle_normal(v0);
		vec3 normal2 = triangle_normal(v1);
		vec3 tangent = normalize(e1-e0);
		vec3 bitangent1 = cross(normal1, tangent);
		vec3 bitangent2 = cross(normal2, tangent);

		vec3 center = (e0 + e1) / 2;

		Cross cross1{ tangent,normal1,bitangent1 };
		Cross cross2{ tangent,normal2,bitangent2 };

		//draw_cross(debug, center, cross1);
		//draw_cross(debug, center, cross2);
        
        tri1 /= 3;
        tri2 /= 3;

        edge_flux_stencil[!current][tri1 / 32] |= 1 << tri1 % 32;
        edge_flux_stencil[!current][tri2 / 32] |= 1 << tri2 % 32;
        
		//edge_flux_stencil[edge] = true;
		//edge_flux_stencil[mesh.edges[edge]] = true;
		edge_flux_boundary[edge] = cross1;
		edge_flux_boundary[mesh.edges[edge]] = cross2;
	}

	uint max_it = 1000;
    const uint MAX_CHUNK = 100;
    uint chunks = min(worker_thread_count(), MAX_CHUNK);

	for (uint i = 0; i < max_it; i++) {
        //bool draw = i = i % 1 == 0;
		//if (draw) {
		//	suspend_execution(debug);
		//	clear_debug_stack(debug);
		//}

		bool past = !current;
        
        float residuals[MAX_CHUNK] = {};
        JobDesc jobs[MAX_CHUNK];
        PropagateJob data[MAX_CHUNK];
        
        uint n_per_chunk = (int)ceilf((float)(mesh.tri_count-1) / chunks);
        
        for (uint i = 0; i < chunks; i++) {
            PropagateJob& job = data[i];
            job.begin = i * n_per_chunk + 1;
            job.end = min((i+1) * n_per_chunk + 1, mesh.tri_count);
            
            job.edge_flux_boundary = edge_flux_boundary.data;
            job.centers = centers.data;
            job.edges = mesh.edges;
            job.cross_center_in = theta_cell_center[past].data;
            job.cross_center_out = theta_cell_center[current].data;
            job.active_in = edge_flux_stencil[past].data;
            job.active_out = edge_flux_stencil[current].data;
            
            jobs[i] = JobDesc(propagate_cross, &job);
        }
        
        wait_for_jobs(PRIORITY_HIGH, {jobs, chunks});
        
        float residual = 0.0f;
        for (uint i = 0; i < chunks; i++) residual += data[i].residual;
        residual /= chunks;
        
        printf("Iteration %i, residual %f\n", i, residual);
        
        if (residual < 0.015) break;
        
        current = !current;
    }
    
    /*for (tri_handle tri : mesh) {
        Cross cross = theta_cell_center[current][tri/3];
        //draw_cross(debug, centers[tri/3], theta_cell_center[current][tri/3], vec4(0,0,0,1));
    }*/
    //suspend_execution(debug);
}

Cross interpolate(vec3 position, Cross cross, uint n, vec3* positions, Cross* neighbors) {
    float sum_theta = 0.0f;
    float sum_w = 0.0f;
    
    for (uint i = 0; i < n; i++) {
        float theta = neighbors[i].angle_between(cross.normal, cross.tangent);
        float w = 1.0 / sq(positions[i] - position);
        
        sum_theta += w*theta;
        sum_w += w;
    }
    
    sum_theta /= sum_w;
    
    Cross result;
    result.normal = cross.normal;
    result.tangent = glm::rotate(glm::vec3(cross.tangent), sum_theta, glm::vec3(cross.normal));
    result.bitangent = ::cross(result.normal, result.tangent);
    
    return result;
}

Cross SurfaceCrossField::at_tri(tri_handle tri) {
    return theta_cell_center[!current][tri/3];
}

Cross SurfaceCrossField::at_tri(tri_handle tri, vec3 pos) {
    Cross cross = theta_cell_center[current][tri/3];
    
    vec3 positions[3];
    Cross neighbors[3];
    
    for (uint i = 0; i < 3; i++) {
        tri_handle neighbor = mesh.neighbor(tri, i);
        neighbors[i] = theta_cell_center[current][neighbor / 3];
        positions[i] = centers[neighbor / 3];
    }
    
    return cross; //interpolate(pos, cross, 3, positions, neighbors);
}

Cross SurfaceCrossField::at_edge(edge_handle edge) {
    return edge_flux_boundary[edge];
}
