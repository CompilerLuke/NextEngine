#include "mesh.h"
#include "mesh/volume_tet_mesh.h"
#include "ui/ui.h"

struct DuctOptions {
	float width = 1.0;
	float height = 1.0;
	float depth = 1.0;

	float constrain_start = 0.0;
	float constrain_end = 2.0;
	float transition = 1.0;
	float narrow = 0.6;

	float turn_start = 1.2;
	float turn_end = 1.2;
};

float lerp(float a, float b, float t) {
	t = clamp(0, 1, t);
	return a * (1.0 - t) + b * t;
}

vec3 curve(const DuctOptions& options, float t) {
	float turn_start = options.turn_start;
	float turn_end = options.turn_end;
	float turn_length = options.turn_end - options.turn_start;
	
	float t_straight = clamp(0, turn_start, t);
	float t_turn = clamp(0, 1, (t - turn_start)/turn_length);
	float t_end = clamp(0, 1, t - turn_end);

	vec3 result;
	result += t_straight * vec3(0, 0, options.depth);
	result += t_end * vec3(options.depth, 0, 0);
	result.x += options.depth * turn_length * PI/4.0f * (1.0-cos(PI * t_turn / 2));
	result.z += options.depth * turn_length * PI/4.0f * (sin(PI * t_turn / 2));

	return result;
}

vec3 curve_tangent(const DuctOptions& options, float t) {
	float turn_length = options.turn_end - options.turn_start;
	float t_turn = clamp(0, 1, (t - options.turn_start) / turn_length);

	if (t < options.turn_start) return vec3(0, 0, 1);
	if (t > options.turn_end) return vec3(1, 0, 0);

	vec3 result;
	result.x = PI/2 * sin(PI * t_turn / 2);
	result.z = PI/2 * cos(PI * t_turn / 2);

	return normalize(result);
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
	
	result += bitangent * ((u - 0.5) * options.width);
	result += normal * ((v - 0.5) * options.height * thick);

	return result;
}

DuctOptions options;

int u_div = 4;
int v_div = 4;
int t_div = 4;

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

CFDVolume generate_parametric_mesh() {
	CFDVolume result;

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

	real pressure_top = 200.0;
	real pressure_bottom = 10.0;



	for (uint i = 0; i < t_div; i++) {
		for (uint j = 0; j < u_div; j++) {
			for (uint k = 0; k < v_div; k++) {
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

				if (i == 0) cell.faces[4].pressure_grad = pressure_top;
				if (i + 1 == t_div) cell.faces[2].pressure_grad = pressure_bottom;

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