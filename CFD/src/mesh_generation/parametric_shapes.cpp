#include "mesh.h"
#include "mesh/volume_tet_mesh.h"
#include "ui/ui.h"

struct DuctOptions {
	float width = 1.0;
    float height = 1.0;
	float depth = 4;

    float constrain_start = 1.1;
    float constrain_end = 1.3;
    float transition = 0.1;
	float narrow = 1.1;

    float turn_start = 1.1;
	float turn_end = 1.3;
    
    float turn_angle = 90.0;
};

float lerp(float a, float b, float t) {
	t = clamp(0, 1, t);
	return a * (1.0 - t) + b * t;
}

vec3 curve_tangent(const DuctOptions& options, float t) {
    real turn_angle = to_radians(options.turn_angle);
    float turn_length = options.turn_end - options.turn_start;
    float t_turn = clamp(0, 1, (t - options.turn_start) / turn_length);

    if (t < options.turn_start) return vec3(0, 0, 1);
    //if (t > options.turn_end) return vec3(1, 0, 0);

    vec3 result;
    result.x = turn_angle * sin(turn_angle * t_turn);
    result.z = turn_angle * cos(turn_angle * t_turn);

    return normalize(result);
}

vec3 curve(const DuctOptions& options, float t) {
    real turn_angle = to_radians(options.turn_angle);
    
    float turn_start = options.turn_start;
    float turn_end = options.turn_end;
    float turn_length = options.turn_end - options.turn_start;
    
    float t_straight = clamp(0, turn_start, t);
    float t_turn = clamp(0, 1, (t - turn_start)/turn_length);
    float t_end = clamp(0, 1, t - turn_end);
    
    vec3 tangent = curve_tangent(options, turn_end);

    vec3 result;
    result += t_straight * vec3(0, 0, options.depth);
    result += t_end * tangent * options.depth;
    result.x += options.depth * turn_length / turn_angle * (1.0-cos(turn_angle * t_turn));
    result.z += options.depth * turn_length / turn_angle * (sin(turn_angle * t_turn));

    return result;
}

float thickness(const DuctOptions& options, float t) {
	if (t > options.constrain_end - options.transition) {
		return lerp(options.narrow, 1.0, (t - options.constrain_end - options.transition) / options.transition);
	}
	if (t > options.constrain_start) {
		return lerp(1.0, options.narrow, (t-options.constrain_start)/options.transition);
	}

	return 1.0;
}

vec3 duct(const DuctOptions& options, float u, float v, float t) {
	float thick = thickness(options, t);

	vec3 result = curve(options, t);
	vec3 tangent = curve_tangent(options, t);
	vec3 normal = vec3(0, 1, 0);
	vec3 bitangent = cross(tangent, normal);
	
	result += bitangent * ((u - 0.5) * options.width * thick);
	result += normal * ((v - 0.5) * options.height * thick);

	return result;
}

DuctOptions options;

int u_div = 8;
int v_div = 8;
int t_div = 32;

template<typename T>
void field(UI& ui, string_view field, T* value, float min = -FLT_MAX, float max = FLT_MAX, float inc_per_pixel = 5.0) {
	begin_hstack(ui);
	text(ui, field).flex_h().align(HLeading); //.color(color4(220,220,220));
	input(ui, value, min, max, inc_per_pixel);
	end_hstack(ui);
}

void parametric_ui(UI& ui) {
	begin_vstack(ui);
	begin_vstack(ui);
	title2(ui, "Shape").font(20);
	field(ui, "width", &options.width);
	field(ui, "heigh", &options.height);
	field(ui, "depth", &options.depth);
	field(ui, "narrow", &options.narrow);
	field(ui, "narrow start", &options.constrain_start);
	field(ui, "narrow end", &options.constrain_end);
	field(ui, "transition", &options.transition);
	field(ui, "turn start", &options.turn_start);
	field(ui, "turn end", &options.turn_end);
    field(ui, "turn angle", &options.turn_angle);
    
	end_vstack(ui);

	begin_vstack(ui);
	title2(ui, "Mesh Subdivision");
	
	begin_hstack(ui);
	text(ui, "u, v, t");
	spacer(ui);
	input(ui, &u_div);
	input(ui, &v_div);
	input(ui, &t_div);
	end_hstack(ui);

	end_vstack(ui);
	end_vstack(ui);
}

#include "mesh_generation/hexcore.h"
#include "graphics/assets/model.h"
#include "graphics/assets/assets.h"
#include "graphics/rhi/primitives.h"
#include "mesh/surface_tet_mesh.h"
#include "components/transform.h"
#include <glm/gtc/matrix_transform.hpp>

SurfaceTriMesh surface_from_mesh(const glm::mat4& mat, Mesh& mesh);

CFDVolume generate_parametric_mesh() {	
	CFDVolume result;
#if 1


	AABB aabb;
	aabb.min = vec3(-options.width, -options.height, 0);
	aabb.max = vec3(options.width, options.height, options.depth);

	Transform trans;
	trans.scale = vec3(options.width * 0.1, options.height * 0.1, options.depth * 0.1);
	trans.position = vec3(0, 0, options.depth * 0.6);

	SurfaceTriMesh mesh = surface_from_mesh(compute_model_matrix(trans), get_Model(primitives.cube)->meshes[0]);

	vector<vertex_handle> boundary_verts;
	vector<Boundary> boundary;
	build_grid(result, mesh, aabb, boundary_verts, boundary, 1.0/u_div, 100);

	for (CFDCell& cell : result.cells) {
		compute_normals(result.vertices, cell);

		for (uint i = 0; i < 8; i++) {
			CFDCell::Face& face = cell.faces[i];

			vec3 pos = result[cell.vertices[hexahedron_shape[i][0]]].position;

			bool front = fabs(pos.z) < FLT_EPSILON;
			bool back = fabs(pos.z - aabb.max.z) < FLT_EPSILON;

			if (face.neighbor.id == -1 && (front || back)) {
				face.pressure = 1000 * face.normal.z;
			}
		}
	}

	return result;
#endif

	float du = 1.0 / u_div;
	float dv = 1.0 / v_div;
	float dt = 1.0 / t_div;

	for (uint i = 0; i <= t_div; i++) {
		for (uint j = 0; j <= u_div; j++) {
			for (uint k = 0; k <= v_div; k++) {
				vec3 point = duct(options, j*du, k*dv, i*dt);
				result.vertices.append({ point });
			}
		}
	}

	uint i_vertex_stride = (u_div + 1) * (v_div + 1);
	uint j_vertex_stride = v_div + 1;
	uint k_vertex_stride = 1;

	uint i_cell_stride = u_div * v_div;
	uint j_cell_stride = v_div;
	uint k_cell_stride = 1;

	auto to_vert = [&](uint i, uint j, uint k) {
		return vertex_handle{ (int)(i * i_vertex_stride + j * j_vertex_stride + k * k_vertex_stride) };
	};

	auto to_cell = [&](uint i, uint j, uint k) {
		return cell_handle{ (int)(i * i_cell_stride + j * j_cell_stride + k * k_cell_stride) };
	};

	auto is_empty = [&](uint i, uint j, uint k) {
		return i > 0.4 * t_div && i < 0.6 * t_div
			&& j>0.4 * u_div && j < 0.6 * u_div
			&& k>0.4 * v_div && k < 0.6 * v_div;
	};

	auto to_neigh = [&](uint i, uint j, uint k) {
		return is_empty(i,j,k) ? cell_handle{-1} : to_cell(i, j, k);
	};

    real pressure_grad = 1000;

	for (uint i = 0; i < t_div; i++) {
		for (uint j = 0; j < u_div; j++) {
			for (uint k = 0; k < v_div; k++) {
				assert(to_cell(i,j,k).id == result.cells.length);

				CFDCell cell{ CFDCell::HEXAHEDRON };

				cell.vertices[0] = to_vert(i, j, k);
				cell.vertices[1] = to_vert(i, j+1, k);
				cell.vertices[2] = to_vert(i+1, j+1, k);
				cell.vertices[3] = to_vert(i+1, j, k);

				cell.vertices[4] = to_vert(i, j, k+1);
				cell.vertices[5] = to_vert(i, j + 1, k+1);
				cell.vertices[6] = to_vert(i + 1, j + 1, k+1);
				cell.vertices[7] = to_vert(i + 1, j, k+1);
			
				if (k > 0) cell.faces[0].neighbor = to_cell(i, j, k-1); //bottom
				if (j > 0) cell.faces[1].neighbor = to_cell(i, j-1, k); //left
				if (i > 0) cell.faces[4].neighbor = to_cell(i-1, j, k); //back
				if (j+1 < u_div) cell.faces[3].neighbor = to_cell(i, j+1, k);  //right
				if (i+1 < t_div) cell.faces[2].neighbor = to_cell(i+1, j, k);  //front
				if (k+1 < v_div) cell.faces[5].neighbor = to_cell(i, j, k+1);  //top

                compute_normals(result.vertices, cell);
                
                if (i == 0) cell.faces[4].pressure = pressure_grad; //velocity * cell.faces[4].normal;
                if (i + 1 == t_div) cell.faces[2].pressure = -pressure_grad; //-velocity * cell.faces[2].normal;

				result.cells.append(cell);
			}
		}
	}

	for (uint i = 1; i < t_div; i++) {
		for (uint j = 1; j < u_div; j++) {
			for (uint k = 1; k < v_div; k++) {
				vec3 avg;
				avg += result[to_vert(i+1, j, k)].position;
				avg += result[to_vert(i-1, j, k)].position;
				avg += result[to_vert(i, j+1, k)].position;
				avg += result[to_vert(i, j-1, k)].position;
				avg += result[to_vert(i, j, k+1)].position;
				avg += result[to_vert(i, j, k-1)].position;
				avg /= 6;

				result[to_vert(i, j, k)].position = avg;
			}
		}
	}
    
    
	return result;
}
