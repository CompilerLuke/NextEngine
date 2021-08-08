#include "solver.h"
#include "engine/handle.h"
#include "core/memory/linear_allocator.h"
#include "visualization/debug_renderer.h"
#include "visualization/color_map.h"

#include "vendor/eigen/Eigen/Sparse"
#include "vendor/eigen/Eigen/IterativeLinearSolvers"

using vec_x = Eigen::VectorXd;
using SparseMt = Eigen::SparseMatrix<real>;
using T = Eigen::Triplet<real>;

Eigen::Vector3d to(vec3 vec) { return Eigen::Vector3d(vec.x, vec.y, vec.z); }
vec3 from(Eigen::Vector3d vec) { return vec3(vec[0], vec[1], vec[2]); }

struct Face {
	real area;
	real dx;
	vec3 normal;
	real parallel;
	vec3 ortho;
	uint cell1, cell2;
};

struct BoundaryFace {
	real area;
	real dx;
	vec3 normal;
	uint cell;
};

struct PressureBoundaryFace {
	real area;
	real dx;
	vec3 normal;
	real pressure;
	uint cell;
};

struct Simulation {
	CFDDebugRenderer& debug;

	uint num_cells;
	vector<Face> faces;
	vector<BoundaryFace> boundary_faces;
	vector<PressureBoundaryFace> pressure_boundary_faces;

	SparseMt sparse_matrix;
	vec_x source_term;
	vector<T> coeffs;

	vector<Eigen::Matrix3d> velocity_gradients;
	vector<Eigen::Vector3d> pressure_gradients;

	vector<vec3> centers;
	vector<real> volumes;
	vec_x vel_and_pressure;

	vector<vec3> velocities_numer;
	vector<vec3> velocities_denom;
};

real triangle_area(vec3 v[3]) {
	vec3 v01 = v[1] - v[0];
	vec3 v02 = v[2] - v[0];

	return length(cross(v01, v02))/2.0f;
}

real quad_area(vec3 v[4]) {
	vec3 v0[3] = {v[0], v[1], v[2]};
	vec3 v1[3] = { v[0], v[2], v[3] };

	return triangle_area(v0) + triangle_area(v1);
}

void right_angle_corrector(vec3 normal, vec3 to_center, real* parallel, vec3* ortho) {
	to_center = normalize(to_center);
	*parallel = dot(normal, to_center);
	*ortho = normal - to_center*(*parallel);
}

uint EQ = 4;
uint VAR = 4;

Simulation make_simulation(CFDVolume& mesh, CFDDebugRenderer& debug) {
	Simulation result{ debug };

	result.num_cells = mesh.cells.length;
	result.centers.resize(result.num_cells);
	result.volumes.resize(result.num_cells);
	result.velocities_numer.resize(result.num_cells);
	result.velocities_denom.resize(result.num_cells);
	result.velocity_gradients.resize(result.num_cells);
	result.pressure_gradients.resize(result.num_cells);

	result.vel_and_pressure.resize(VAR * result.num_cells);
	result.sparse_matrix.resize(EQ * result.num_cells, VAR * result.num_cells);

	for (uint i = 0; i < mesh.cells.length; i++) {
		const CFDCell& cell = mesh.cells[i];
		const ShapeDesc& shape = shapes[cell.type];

		uint cell_id = i;

		vec3 center = compute_centroid(mesh, cell.vertices, shape.num_verts);

		result.centers[i] = center;

		real volume = 0.0f;

		for (uint f = 0; f < shape.num_faces; f++) {
			const ShapeDesc::Face& face = shape[f];

			vec3 face_center;
			vec3 v[4];
			for (uint j = 0; j < face.num_verts; j++) {
				v[j] = mesh.vertices[cell.vertices[face[j]].id].position;
				face_center += v[j];
			}
			face_center /= face.num_verts;

			bool is_quad = face.num_verts == 4;

			vec3 normal = is_quad ? quad_normal(v) : triangle_normal(v);
			real area = is_quad ? quad_area(v) : triangle_area(v);

			cell_handle neighbor = cell.faces[f].neighbor;
			bool boundary = neighbor.id == -1;

			volume += area * dot(face_center, normal);

			if (boundary) {
				draw_line(debug, face_center, face_center + 0.05*normal, vec4(0,0,0,1));
				real dx = 1.0 / (2.0*length(face_center - center));
				if (cell.faces[f].pressure_grad != 0.0f) {
					
					result.pressure_boundary_faces.append(PressureBoundaryFace{ area, dx, normal, cell.faces[f].pressure_grad, cell_id });
				}
				else {
					result.boundary_faces.append(BoundaryFace{ area, dx, normal, cell_id });
				}
			}
			else if (neighbor.id > cell_id) {
				vec3 center_plus = compute_centroid(mesh, mesh[neighbor].vertices, 8);
				vec3 center_minus = center;

				vec3 t = center_plus - center_minus;

				real ratio = dot(t, normal) / length(t);
				result.faces.append(Face{area, 0, normal * ratio, 0, 0, cell_id, (uint)neighbor.id});
			}
		}

		volume /= 3;
		result.volumes[cell_id] = volume;
	}

	for (Face& face : result.faces) {
		vec3 center_minus = result.centers[face.cell1];
		vec3 center_plus = result.centers[face.cell2];

		face.dx = 1.0 / length(center_plus - center_minus);
		right_angle_corrector(face.normal, center_plus - center_minus, &face.parallel, &face.ortho);
	}

	for (uint i = 0; i < result.num_cells; i++) {
		result.vel_and_pressure(i*4 + 0) = 0;
		result.vel_and_pressure(i*4 + 1) = 0;
		result.vel_and_pressure(i*4 + 2) = 0;
		result.vel_and_pressure(i*4 + 3) = 1.0;
	}

	suspend_execution(debug);

	return result;
}

vec3 get_velocity(Simulation& simulation, uint cell) {
	vec3 result;
	result.x = simulation.vel_and_pressure(cell * VAR + 0);
	result.y = simulation.vel_and_pressure(cell * VAR + 1);
	result.z = simulation.vel_and_pressure(cell * VAR + 2);
	return result;
};

real get_pressure(Simulation& simulation, uint cell) {
	return simulation.vel_and_pressure[cell * VAR + 3];
};

Eigen::Matrix3d get_velocity_gradient(Simulation& simulation, uint cell) {
	return simulation.velocity_gradients[cell];
}

vec3 get_pressure_gradient(Simulation& simulation, uint cell) {
	return from(simulation.pressure_gradients[cell]);
}

void compute_gradients(Simulation& simulation) {
	auto& velocity_gradients = simulation.velocity_gradients;
	auto& pressure_gradients = simulation.pressure_gradients;

	for (uint i = 0; i < simulation.num_cells; i++) {
		velocity_gradients[i].fill(0.0f);
		pressure_gradients[i].fill(0.0f);
	}

	for (Face f : simulation.faces) {
		vec3 velocity_down = get_velocity(simulation, f.cell1);
		vec3 velocity_up = get_velocity(simulation, f.cell2);
		vec3 velocity = (velocity_down + velocity_up) / 2;
		
		real pressure_down = get_pressure(simulation, f.cell1);
		real pressure_up = get_pressure(simulation, f.cell1);
		real pressure = (pressure_down + pressure_up) / 2;

		Eigen::Vector3d _velocity(velocity.x, velocity.y, velocity.z);
		Eigen::Vector3d normal(f.normal.x, f.normal.y, f.normal.z);
		normal *= f.area;

		Eigen::Matrix3d vel_gradient = _velocity * normal.transpose();
		Eigen::Vector3d pressure_gradient = pressure * normal;
	
		velocity_gradients[f.cell1] += vel_gradient;
		pressure_gradients[f.cell1] += pressure_gradient;

		velocity_gradients[f.cell2] -= vel_gradient;
		pressure_gradients[f.cell2] -= pressure_gradient;
	}

	for (BoundaryFace f : simulation.boundary_faces) {
		Eigen::Vector3d normal(f.normal.x, f.normal.y, f.normal.z);
		normal *= f.area;

		real pressure = get_pressure(simulation, f.cell);
		pressure_gradients[f.cell] += pressure * normal;
	}

	for (PressureBoundaryFace f : simulation.pressure_boundary_faces) {
		Eigen::Vector3d velocity = to(get_velocity(simulation, f.cell));
		Eigen::Vector3d normal = to(f.normal) * f.area;

		real pressure = get_pressure(simulation, f.cell);
		velocity_gradients[f.cell] += velocity * normal.transpose();
		pressure_gradients[f.cell] += pressure * normal;
	}

	for (uint i = 0; i < simulation.num_cells; i++) {
		velocity_gradients[i] /= simulation.volumes[i];
		pressure_gradients[i] /= simulation.volumes[i];
	}
}

void build_coupled_matrix(Simulation& simulation) {
	vector<T>& coeffs = simulation.coeffs; 
	vec_x& source = simulation.source_term;	
	

	coeffs.clear();
	source.resize(simulation.num_cells * EQ);
	source.fill(0);

	//momentum U coefficient
	auto m_U = [&](uint n, uint m, vec3 coeff) {
		coeffs.append(T(n*EQ + 0, m*VAR + 0, coeff.x));
		coeffs.append(T(n*EQ + 1, m*VAR + 1, coeff.y));
		coeffs.append(T(n*EQ + 2, m*VAR + 2, coeff.z));
	};

	auto m_U_dot = [&](uint n, uint m, vec3 coeff) {
		for (uint i = 0; i < 3; i++) {
			for (uint j = 0; j < 3; j++) {
				coeffs.append(T(n * EQ + i, m * VAR + j, coeff[j]));
			}
		}
	};

	auto m_S = [&](uint cell, vec3 value) {
		source[cell * EQ + 0] += value.x;
		source[cell * EQ + 1] += value.y;
		source[cell * EQ + 2] += value.z;
	};

	//momentum P coefficient
	auto m_P = [&](uint c, uint n, vec3 coeff) {
		coeffs.append(T(c*EQ + 0, n*VAR + 3, coeff.x));
		coeffs.append(T(c*EQ + 1, n*VAR + 3, coeff.y));
		coeffs.append(T(c*EQ + 2, n*VAR + 3, coeff.z));
	};

	//pressure U coefficient, dot product
	auto p_U = [&](uint n, uint m, vec3 coeff) {
		coeffs.append(T(n*EQ + 3, m*VAR + 0, coeff.x));
		coeffs.append(T(n*EQ + 3, m*VAR + 1, coeff.y));
		coeffs.append(T(n*EQ + 3, m*VAR + 2, coeff.z));
	};

	auto p_S = [&](uint c, real value) {
		source[c*EQ + 3] += value;
	};

	//pressure 
	auto p_P = [&](uint n, uint m, real coeff) {
		coeffs.append(T(n*EQ + 3, m*VAR + 3, coeff));
	};

	auto& vel_and_pressure = simulation.vel_and_pressure;

	real rho = 1;
	real mu = 1;

	auto face_contribution = [&](uint cell, uint neigh, vec3 vel_down, vec3 vel_up, vec3 normal, real parallel, vec3 ortho, real area, real dx) {
		vec3 anormal = normal * area;
		
		vec3 pressure_gradient_down = get_pressure_gradient(simulation, cell);
		vec3 pressure_gradient_up = get_pressure_gradient(simulation, neigh);

		Eigen::Matrix3d vel_gradient_down = get_velocity_gradient(simulation,cell);
		Eigen::Matrix3d vel_gradient_up = get_velocity_gradient(simulation, neigh);

		vec3 vel_face = (vel_up + vel_down) / 2.0f;
		Eigen::Matrix3d vel_gradient_face = (vel_gradient_down + vel_gradient_up) / 2;
		vec3 pressure_gradient_face = (pressure_gradient_down + pressure_gradient_up) / 2;

		real conv_coeff = rho * dot(vel_face, anormal);

		/*
		Eigen::Vector3d _anormal = to(anormal);
		vec3 div_coeff;
		div_coeff.x = vel_gradient_face.col(0).dot(_anormal);
		div_coeff.y = vel_gradient_face.col(1).dot(_anormal);
		div_coeff.z = vel_gradient_face.col(2).dot(_anormal);
		*/
		vec3 div_coeff = from(vel_gradient_face.transpose() * to(anormal));

		auto& coef = coeffs;

		p_U(cell, cell, 10*0.5*anormal);
		p_U(cell, neigh, 10*0.5*anormal);

		//convective acceleration
		m_U(cell, neigh, 0.5*conv_coeff);
		m_U(cell, cell, 0.5*conv_coeff);

		//pressure source
		m_P(cell, neigh, 0.5 * anormal);
		m_P(cell, cell, 0.5 * anormal);

		//velocity gradient
		m_U(cell, neigh, -area * mu * dx * parallel);
		m_U(cell, cell,   area * mu * dx * parallel);
		//orthogonal corrector
		m_S(cell, -area * mu * from(vel_gradient_face * to(ortho)));

		//pressure gradient
		p_P(cell, neigh, area * dx * parallel);
		p_P(cell, cell, -area * dx * parallel);
		//orthogonal corrector
		p_S(cell, -area * dot(pressure_gradient_face, ortho));

		//velocity divergence source
		p_U(cell, neigh, 0.5 * rho * div_coeff);
		p_U(cell, cell,  0.5 * rho * div_coeff);
	};

	bool first = false;

	for (const Face& face : simulation.faces) {
		vec3 vel_down = get_velocity(simulation, face.cell1);
		vec3 vel_up = get_velocity(simulation, face.cell2);

		face_contribution(face.cell1, face.cell2, vel_down, vel_up, face.normal, face.parallel, face.ortho, face.area, face.dx);
		face_contribution(face.cell2, face.cell1, vel_up, vel_down, -face.normal, face.parallel, -face.ortho, face.area, face.dx);
	}

	for (const BoundaryFace& face : simulation.boundary_faces) {
		uint cell = face.cell;
		real area = face.area;
		real dx = face.dx;
		vec3 anormal = face.normal * area;
		vec3 vel = get_velocity(simulation, cell);

		m_P(cell, cell, anormal);
		m_U(cell, cell, 2 * area * mu * dx);
	}

	for (const PressureBoundaryFace& face : simulation.pressure_boundary_faces) {
		uint cell = face.cell;
		real area = face.area;
		real dx = face.dx;
		vec3 anormal = face.normal * area;
		vec3 vel_face = get_velocity(simulation, cell);

		p_U(cell, cell, 10*anormal);

		m_U(cell, cell, rho * dot(vel_face, anormal));
		m_S(cell, -face.pressure * anormal);

		p_P(cell, cell, area * -2 * dx);
		source[cell*4 + 3] -= area * 2 * face.pressure * dx;
	}

	simulation.sparse_matrix.setFromTriplets(coeffs.begin(), coeffs.end());
}

#include <iostream>

void solve_coupled_matrix(Simulation& simulation, bool first) {
	Eigen::BiCGSTAB<Eigen::SparseMatrix<real>> solver;
	solver.compute(simulation.sparse_matrix);

	std::cout << simulation.sparse_matrix << std::endl;
	std::cout << simulation.source_term << std::endl;

	if (first) {
		simulation.vel_and_pressure = solver.solve(simulation.source_term);
	}
	else {
		simulation.vel_and_pressure = solver.solveWithGuess(simulation.source_term, simulation.vel_and_pressure);
	}
	std::cout << "Solution" << std::endl;
	std::cout << simulation.vel_and_pressure << std::endl;
}

void draw_simulation_state(Simulation& simulation, bool show_velocity, bool show_pressure) {
	CFDDebugRenderer& debug = simulation.debug;
	clear_debug_stack(debug);

	float max_pressure = 0.0f;
	float max_velocity = 0.0f;

	for (uint i = 0; i < simulation.num_cells; i++) {
		vec3 vel = get_velocity(simulation, i);
		real pressure = get_pressure(simulation, i);
		
		max_pressure = fmaxf(max_pressure, pressure);
		max_velocity = fmaxf(max_velocity, length(vel));
	}

	std::cout << "Max velocity! " << max_velocity << std::endl;
	std::cout << "Max pressure! " << max_pressure << std::endl;

	if (show_velocity) {
		for (uint i = 0; i < simulation.num_cells; i += 1) {
			vec3 c = simulation.centers[i];
			vec3 u = get_velocity(simulation, i);
			vec3 t = normalize(u);
			vec3 n = vec3(0, 1, 0);
			vec3 b = normalize(cross(t, n));

			vec4 color = color_map(length(u), 0, max_velocity);

			real l = 0.3*length(u)/max_velocity;

			vec3 start = c - t * l / 2;
			vec3 end = c + t * l / 2;

			float arrow = 0.1 * l;

			draw_line(debug, start, end, color);
			draw_line(debug, end, end + (n-t)*arrow, color);
			draw_line(debug, end, end + (-n-t)*arrow, color);
		}
	}

	if (show_pressure) {
		for (uint i = 0; i < simulation.num_cells; i += 1) {
			vec3 c = simulation.centers[i];
			real p = get_pressure(simulation, i);

			real arrow = 0.1;
			vec4 color = color_map(p, 0, max_pressure);

			draw_line(debug, c + vec3(-arrow, 0, 0), c + vec3(arrow, 0, 0), color);
			draw_line(debug, c + vec3(0, -arrow, 0), c + vec3(0, arrow, 0), color);
			draw_line(debug, c + vec3(0, 0, -arrow), c + vec3(0, 0, arrow), color);
		}
	}
}


CFDResults simulate(CFDVolume& volume, CFDDebugRenderer& debug) {
	Simulation simulation = make_simulation(volume, debug);
	
	CFDResults result;

	uint n = 20;
	for (uint i = 0; i < n; i++) {
		printf("Iteration %i, (%ix%i)\n", i, simulation.num_cells * 4, simulation.num_cells * 4);
		compute_gradients(simulation);
		build_coupled_matrix(simulation);
		solve_coupled_matrix(simulation, i == 0);
			
		draw_simulation_state(simulation, true, false);
		suspend_execution(debug);
		draw_simulation_state(simulation, false, true);
		suspend_execution(debug);
	}

	//result.velocities = std::move(simulation.velocities);
	//result.pressures = std::move(simulation.pressures);

	return result;
}