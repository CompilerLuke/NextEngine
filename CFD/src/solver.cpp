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
	
	vector<vec3> velocities;
	vector<vec3> next_velocities;
	vec_x pressures;

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

Simulation make_simulation(CFDVolume& mesh, CFDDebugRenderer& debug) {
	Simulation result{ debug };

	result.num_cells = mesh.cells.length;
	result.centers.resize(result.num_cells);
	result.volumes.resize(result.num_cells);
	result.velocities_numer.resize(result.num_cells);
	result.velocities_denom.resize(result.num_cells);
	result.velocity_gradients.resize(result.num_cells);
	result.pressure_gradients.resize(result.num_cells);

	result.velocities.resize(result.num_cells);
	result.next_velocities.resize(result.num_cells);
	result.pressures.resize(result.num_cells);
	result.sparse_matrix.resize(result.num_cells, result.num_cells);

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
		result.velocities[i] = vec3(0);
		result.pressures[i] = 1.0;
	}

	suspend_execution(debug);

	return result;
}

vec3 get_velocity(Simulation& simulation, uint cell) {
	return simulation.velocities[cell];
};

real get_pressure(Simulation& simulation, uint cell) {
	return simulation.pressures(cell);
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

real rho = 1;
real mu = 0.1;

void predictor_step(Simulation& sim, real dt) {
	auto& next_velocities = sim.next_velocities;

	for (uint i = 0; i < sim.num_cells; i++) {
		next_velocities[i] = {};
	}

	auto face_contribution = [&](uint cell, real area, real dx, vec3 normal, vec3 vel_down, vec3 vel_up, real pressure_down, real pressure_up) {
		vec3 vel_face = (vel_up + vel_down) / 2.0f;
		vec3 pressure_face = (pressure_up + pressure_down) / 2.0f;

		//vec3 conv_accel = from((to(vel_face) * to(vel_face).transpose()) * to(normal));
		vec3 conv_accel2 = vel_face * dot(vel_face, normal);

		vec3 flux = 0;
		//flux -= 0.1*conv_accel2;
		flux += mu * (vel_up - vel_down) * dx;
		flux -= pressure_face * normal;

		next_velocities[cell] += area * flux;
	};

	for (Face face : sim.faces) {
		real dx = face.dx;
		vec3 normal = face.normal;
		vec3 area = face.area;

		vec3 vel_down = get_velocity(sim, face.cell1);
		vec3 vel_up = get_velocity(sim, face.cell2);
		real pressure_down = get_pressure(sim, face.cell1);
		real pressure_up = get_pressure(sim, face.cell2);

		face_contribution(face.cell1, face.area, face.dx, face.normal, vel_down, vel_up, pressure_down, pressure_up);
		face_contribution(face.cell2, face.area, face.dx, -face.normal, vel_up, vel_down, pressure_up, pressure_down);
	}

	for (BoundaryFace f : sim.boundary_faces) {
		vec3 vel = get_velocity(sim, f.cell);
		real pressure = get_pressure(sim, f.cell);
		face_contribution(f.cell, f.area, f.dx, f.normal, vel, -vel, pressure, pressure);
	}

	for (PressureBoundaryFace f : sim.pressure_boundary_faces) {
		vec3 vel = get_velocity(sim, f.cell);
		real down_pressure = get_pressure(sim, f.cell);
		real up_pressure = 2 * f.pressure - down_pressure;
		face_contribution(f.cell, f.area, f.dx, f.normal, vel, vel, down_pressure, up_pressure);
	}

	for (uint i = 0; i < sim.num_cells; i++) {
		vec3 old_velocity = sim.velocities[i];
		vec3 new_velocity = sim.next_velocities[i] / sim.volumes[i];
		next_velocities[i] = old_velocity + dt * new_velocity;
	}

	std::swap(sim.next_velocities, sim.velocities);
}

void corrector_step(Simulation& sim, real dt) {
	auto& next_velocities = sim.next_velocities;
	
	for (uint i = 0; i < sim.num_cells; i++) {
		next_velocities[i] = {};
	}
	
	auto face_contribution = [&](uint cell, real area, vec3 normal, real pressure_down, real pressure_up) {
		real pressure = (pressure_down + pressure_up) / 2.0f;
		
		vec3 flux;
		flux = -1.0/rho * normal * pressure;

		next_velocities[cell] += area * flux;
	};

	for (Face& f : sim.faces) {
		real pressure_down = get_pressure(sim, f.cell1);
		real pressure_up = get_pressure(sim, f.cell2);

		face_contribution(f.cell1, f.area, f.normal, pressure_down, pressure_up);
		face_contribution(f.cell2, f.area, -f.normal, pressure_up, pressure_down);
	}

	for (BoundaryFace& f : sim.boundary_faces) {
		real pressure = get_pressure(sim, f.cell);

		face_contribution(f.cell, f.area, f.normal, pressure, pressure);
	}

	for (PressureBoundaryFace& f : sim.pressure_boundary_faces) {
		real pressure = get_pressure(sim, f.cell);

		face_contribution(f.cell, f.area, f.normal, pressure, 2*f.pressure - pressure);
	}

	for (uint i = 0; i < sim.num_cells; i++) {
		sim.next_velocities[i] = sim.velocities[i] + dt * next_velocities[i] / sim.volumes[i];
	}

	std::swap(sim.next_velocities, sim.velocities);
}

void build_poisson_matrix(Simulation& sim, real dt) {
	vector<T>& coeffs = sim.coeffs; 
	vec_x& source = sim.source_term;	
	
	coeffs.clear();
	source.resize(sim.num_cells);
	source.fill(0);

	auto face_contribution = [&](uint cell, uint neigh, real dx, real area, vec3 normal, vec3 vel_down, vec3 vel_up) {
		vec3 vel_face = (vel_down + vel_up) / 2.0f;
		
		real flux;
		flux = rho/dt * dot(vel_face, normal);

		coeffs.append(T(cell, neigh, area * dx));
		coeffs.append(T(cell, cell, -area * dx));

		source[cell] += -area * flux;
	};

	for (Face& f : sim.faces) {
		vec3 vel_down = get_velocity(sim, f.cell1);
		vec3 vel_up = get_velocity(sim, f.cell2);

		face_contribution(f.cell1, f.cell2, f.dx, f.area, f.normal, vel_down, vel_up);
		face_contribution(f.cell2, f.cell1, f.dx, f.area, -f.normal, vel_up, vel_down);
	}

	for (PressureBoundaryFace& f : sim.pressure_boundary_faces) {
		coeffs.append(T(f.cell, f.cell, -2 * f.area * f.dx));
		source[f.cell] -= 2 * f.area * f.dx * f.pressure;
	}

	sim.sparse_matrix.setFromTriplets(coeffs.begin(), coeffs.end());
}

#include <iostream>

void solve_poisson_matrix(Simulation& simulation, bool first) {
	Eigen::BiCGSTAB<Eigen::SparseMatrix<real>> solver;
	solver.compute(simulation.sparse_matrix);

	//std::cout << simulation.sparse_matrix << std::endl;

	//std::cout << "Source" << std::endl;
	//std::cout << simulation.source_term << std::endl;

	if (first) {
		simulation.pressures = solver.solve(simulation.source_term);
	}
	else {
		simulation.pressures = solver.solveWithGuess(simulation.source_term, simulation.pressures);
	}

	//std::cout << "Solution" << std::endl;
	//std::cout << simulation.pressures << std::endl;
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

	uint n = 100;
	for (uint i = 0; i < n; i++) {
		real dt = i == 0 ? 1 : 1.0 / 1000;

		printf("Iteration %i, (%ix%i), dt %f\n", i, simulation.num_cells * 4, simulation.num_cells * 4, dt);
		//compute_gradients(simulation);
		build_poisson_matrix(simulation, dt);
		solve_poisson_matrix(simulation, false);
		
		uint inner = 10;
		for (uint i = 0; i < inner; i++) {
			predictor_step(simulation, dt / inner);
		}
		
		/*for (uint i = 0; i < inner; i++) {
			corrector_step(simulation, dt / inner);
		}*/
			
		draw_simulation_state(simulation, true, false);
		suspend_execution(debug);
		
		draw_simulation_state(simulation, false, true);
		suspend_execution(debug);
	}

	//result.velocities = std::move(simulation.velocities);
	//result.pressures = std::move(simulation.pressures);

	return result;
}