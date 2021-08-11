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
    bool is_boundary1;
    bool is_boundary2;
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

    SparseMt velocity_matrix;
	SparseMt pressure_matrix;
    vec_x H_Matrix;
    vec_x inv_A_matrix;
    
	vec_x pressure_source_term;
    vec_x velocity_pressure_source_term;
    vec_x velocity_extra_source_term;
    vec_x velocity_source_term;
	vector<T> coeffs;

	vector<Eigen::Matrix3d> velocity_gradients;
	vector<Eigen::Vector3d> pressure_gradients;

	vector<vec3> centers;
	vector<real> volumes;
	vec_x velocities;
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

bool is_boundary(const CFDCell& cell) {
    const ShapeDesc& shape = shapes[cell.type];
    for (uint f = 0; f < shape.num_faces; f++) {
        if (cell.faces[f].neighbor.id == -1) {
            return true;
        }
    }
    return false;
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

	result.velocities.resize(3* result.num_cells);
    result.pressures.resize(result.num_cells);
    
    result.pressure_matrix.resize(result.num_cells, result.num_cells);
    result.velocity_matrix.resize(3*result.num_cells, 3*result.num_cells);
    
	for (uint i = 0; i < mesh.cells.length; i++) {
		const CFDCell& cell = mesh.cells[i];
		const ShapeDesc& shape = shapes[cell.type];

		uint cell_id = i;

		vec3 center = compute_centroid(mesh, cell.vertices, shape.num_verts);

		result.centers[i] = center;

		real volume = 0.0f;
        
        bool is_on_boundary = is_boundary(cell);

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
                
                cell_handle cell_h = {(int)cell_id};
                
                result.faces.append(Face{area, 0, normal * ratio, 0, 0, cell_id, (uint)neighbor.id, is_on_boundary, is_boundary(mesh[neighbor])});
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
		result.velocities(i*3 + 0) = 0;
		result.velocities(i*3 + 1) = 0;
		result.velocities(i*3 + 2) = 0;
		result.pressures(i) = 0.0;
	}

	suspend_execution(debug);

	return result;
}

vec3 get_velocity(Simulation& simulation, uint cell) {
	vec3 result;
	result.x = simulation.velocities(cell * 3 + 0);
	result.y = simulation.velocities(cell * 3 + 1);
	result.z = simulation.velocities(cell * 3 + 2);
	return result;
};

real get_pressure(Simulation& simulation, uint cell) {
	return simulation.pressures[cell];
};

vec3 get_H(Simulation& sim, uint cell) {
    vec3 source;
    source.x = sim.H_Matrix[cell*3 + 0];
    source.y = sim.H_Matrix[cell*3 + 1];
    source.z = sim.H_Matrix[cell*3 + 2];
    return source;
};

Eigen::Matrix3d get_velocity_gradient(Simulation& simulation, uint cell) {
	return simulation.velocity_gradients[cell];
}

vec3 get_pressure_gradient(Simulation& simulation, uint cell) {
	return from(simulation.pressure_gradients[cell]);
}

real rho = 1;
real mu = 1;

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
		real pressure_up = get_pressure(simulation, f.cell2);
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

		velocity_gradients[f.cell] += velocity * normal.transpose();
		pressure_gradients[f.cell] += f.pressure * normal;
	}

	for (uint i = 0; i < simulation.num_cells; i++) {
		velocity_gradients[i] /= simulation.volumes[i];
		pressure_gradients[i] /= simulation.volumes[i];
	}
}

void build_velocity_matrix(Simulation& simulation) {
    vector<T>& coeffs = simulation.coeffs;
    vec_x& pressure_source = simulation.velocity_pressure_source_term;
    vec_x& extra_source = simulation.velocity_extra_source_term;

    coeffs.clear();
    pressure_source.resize(simulation.num_cells * 3);
    extra_source.resize(simulation.num_cells * 3);
    pressure_source.fill(0);
    extra_source.fill(0);

    //momentum U coefficient
    auto m_U = [&](uint n, uint m, vec3 coeff) {
        coeffs.append(T(n*3 + 0, m*3 + 0, coeff.x));
        coeffs.append(T(n*3 + 1, m*3 + 1, coeff.y));
        coeffs.append(T(n*3 + 2, m*3 + 2, coeff.z));
    };

    auto m_U_dot = [&](uint n, uint m, vec3 coeff) {
        for (uint i = 0; i < 3; i++) {
            for (uint j = 0; j < 3; j++) {
                coeffs.append(T(n*3 + i, m*3 + j, coeff[j]));
            }
        }
    };

    auto m_S_P = [&](uint cell, vec3 value) {
        pressure_source[cell * 3 + 0] += value.x;
        pressure_source[cell * 3 + 1] += value.y;
        pressure_source[cell * 3 + 2] += value.z;
    };
    
    auto m_S_E = [&](uint cell, vec3 value) {
        extra_source[cell * 3 + 0] += value.x;
        extra_source[cell * 3 + 1] += value.y;
        extra_source[cell * 3 + 2] += value.z;
    };

    real rho = 1;
    real mu = 1;

    auto face_contribution = [&](uint cell, uint neigh, vec3 vel_down, vec3 vel_up, vec3 pressure_down, vec3 pressure_up, vec3 normal, real parallel, vec3 ortho, real area, real dx) {
        vec3 anormal = normal * area;
        
        Eigen::Matrix3d vel_gradient_down = get_velocity_gradient(simulation,cell);
        Eigen::Matrix3d vel_gradient_up = get_velocity_gradient(simulation, neigh);

        bool upwind = dot(normal, vel_down) > 0;
        
        vec3 pressure_face = (pressure_down + pressure_up) / 2.0f;
        vec3 vel_face = upwind ? vel_down : vel_up; // (vel_up + vel_down) / 2.0f;
        Eigen::Matrix3d vel_gradient_face = (vel_gradient_down + vel_gradient_up) / 2;
        
        real conv_coeff = rho * dot(vel_face, anormal);
        
        real inv_volume = 1.0 / simulation.volumes[cell];

        //convective acceleration
        m_U(cell, upwind ? cell : neigh, conv_coeff * inv_volume);

        //pressure source
        m_S_P(cell, -pressure_face * anormal * inv_volume);

        //velocity gradient
        m_U(cell, neigh, -area * mu * dx * parallel * inv_volume);
        m_U(cell, cell,   area * mu * dx * parallel * inv_volume);
        //orthogonal corrector
        m_S_E(cell, area * mu * from(vel_gradient_face * to(ortho)) * inv_volume);
    };

    for (const Face& face : simulation.faces) {
        vec3 vel_down = get_velocity(simulation, face.cell1);
        vec3 vel_up = get_velocity(simulation, face.cell2);
        real pressure_down = get_pressure(simulation, face.cell1);
        real pressure_up = get_pressure(simulation, face.cell2);
        
        face_contribution(face.cell1, face.cell2, vel_down, vel_up, pressure_down, pressure_up, face.normal, face.parallel, face.ortho, face.area, face.dx);
        face_contribution(face.cell2, face.cell1, vel_up, vel_down, pressure_up, pressure_down, -face.normal, face.parallel, -face.ortho, face.area, face.dx);
    }

    for (const BoundaryFace& face : simulation.boundary_faces) {
        uint cell = face.cell;
        real area = face.area;
        real dx = face.dx;
        vec3 anormal = face.normal * area;
        vec3 pressure = get_pressure(simulation, cell);
        real volume = 1.0 / simulation.volumes[cell];

        m_S_P(cell, -pressure * anormal * volume);
        m_U(cell, cell, 2 * area * mu * dx * volume);
    }

    for (const PressureBoundaryFace& face : simulation.pressure_boundary_faces) {
        uint cell = face.cell;
        real area = face.area;
        real dx = face.dx;
        vec3 anormal = face.normal * area;
        vec3 vel_face = get_velocity(simulation, cell);
        real volume = 1.0 / simulation.volumes[cell];

        //convective acceleration
        m_U(cell, cell, rho * dot(vel_face, anormal) * volume);
        m_S_P(cell, -face.pressure * anormal * volume);
    }

    simulation.velocity_source_term = simulation.velocity_extra_source_term + simulation.velocity_pressure_source_term;
    simulation.velocity_matrix.setFromTriplets(coeffs.begin(), coeffs.end());
}

vec3 unpack3(vec_x& vec, uint i) {
    vec3 result;
    result.x = vec[i*3 + 0];
    result.y = vec[i*3 + 1];
    result.z = vec[i*3 + 2];
    
    return result;
}

void check_H(Simulation& simulation) {
    const auto A_Diagonal = simulation.velocity_matrix.diagonal();
    auto& H = simulation.H_Matrix;
    auto& V_Matrix = simulation.velocity_matrix;
    auto& inv_A_matrix = simulation.inv_A_matrix;
    
    vec_x result = A_Diagonal.cwiseProduct(simulation.velocities) - H;
    vec_x result_exact = simulation.velocity_matrix*simulation.velocities;
    
    Simulation& sim = simulation;
    
    vec_x flatten_p;
    flatten_p.resize(sim.num_cells*3);
    
    for (uint i = 0; i < sim.num_cells; i++) {
        flatten_p[i*3 + 0] = sim.pressure_gradients[i][0];
        flatten_p[i*3 + 1] = sim.pressure_gradients[i][1];
        flatten_p[i*3 + 2] = sim.pressure_gradients[i][2];
    }
    
    vec_x pred_velocities = inv_A_matrix.cwiseProduct(H) - inv_A_matrix.cwiseProduct(flatten_p);
    
    for (uint i = 0; i < simulation.num_cells; i++) {
        vec3 residual = unpack3(result, i);
        vec3 residual_exact = unpack3(result, i);
        vec3 gradient = -get_pressure_gradient(simulation, i);
        vec3 extra_source = unpack3(simulation.velocity_extra_source_term, i);
        vec3 pressure_source = unpack3(simulation.velocity_pressure_source_term, i);
        
        vec3 u = unpack3(simulation.velocities, i);
        vec3 pred_u = unpack3(pred_velocities, i);
        
        real r = length(u - pred_u);
        
        if (r > 0.5) {
            printf("%f\n", r);
        }
    }
}

void compute_H(Simulation& simulation) {
    const auto A_Diagonal = simulation.velocity_matrix.diagonal();
    auto& H = simulation.H_Matrix;
    auto& V_Matrix = simulation.velocity_matrix;
    auto& inv_A_matrix = simulation.inv_A_matrix;
    
    inv_A_matrix= A_Diagonal;
    
    H = inv_A_matrix.cwiseProduct(simulation.velocities) - V_Matrix*simulation.velocities + simulation.velocity_extra_source_term;
    
    int row = 0;
    H.maxCoeff(&row);
    
    vec3 diagonal = unpack3(inv_A_matrix, row/3);
    
    inv_A_matrix = A_Diagonal.cwiseInverse();
    

    check_H(simulation);
}

void build_pressure_matrix(Simulation& simulation, bool first) {
	vector<T>& coeffs = simulation.coeffs; 
	vec_x& source = simulation.pressure_source_term;

	coeffs.clear();
	source.resize(simulation.num_cells);
	source.fill(0);
    
    auto& inv_A_matrix = simulation.inv_A_matrix;

	//momentum P coefficient
	auto p_S = [&](uint c, real value) {
		source[c] += value;
	};

	//pressure 
	auto p_P = [&](uint n, uint m, real coeff) {
		coeffs.append(T(n, m, coeff));
	};

	auto face_contribution = [&](uint cell, uint neigh, vec3 normal, real parallel, vec3 ortho, real area, real dx, bool is_boundary) {
		vec3 anormal = normal * area;
        
		vec3 pressure_gradient_down = get_pressure_gradient(simulation, cell);
		vec3 pressure_gradient_up = get_pressure_gradient(simulation, neigh);
		vec3 pressure_gradient_face = (pressure_gradient_down + pressure_gradient_up) / 2;

        real A_cell = inv_A_matrix(cell*3);
        real A_neigh = inv_A_matrix(neigh*3);
        real A = (A_cell + A_neigh) / 2;

		//pressure gradient
		p_P(cell, neigh, area * dx * parallel * A);
		p_P(cell, cell, -area * dx * parallel * A);
		//orthogonal corrector
		p_S(cell, -A * area * dot(pressure_gradient_face, ortho));

		//H matrix source
        vec3 source = (A_cell*get_H(simulation, cell) + A_neigh*get_H(simulation, neigh)) / 2.0;
        
        if (!is_boundary) {
            p_S(cell, dot(source, anormal));
        }
	};

	for (const Face& face : simulation.faces) {
		face_contribution(face.cell1, face.cell2, face.normal, face.parallel, face.ortho, face.area, face.dx, face.is_boundary1);
		face_contribution(face.cell2, face.cell1, -face.normal, face.parallel, -face.ortho, face.area, face.dx, face.is_boundary2);
	}

	for (const BoundaryFace& face : simulation.boundary_faces) {
		uint cell = face.cell;
		real area = face.area;
		real dx = face.dx;
		vec3 anormal = face.normal * area;
        real A = inv_A_matrix(face.cell*3);
        vec3 source = A*get_H(simulation, face.cell);

        //p_S(cell, dot(source, anormal));
    }

	for (const PressureBoundaryFace& face : simulation.pressure_boundary_faces) {
		uint cell = face.cell;
		real area = face.area;
		real dx = face.dx;
		vec3 anormal = face.normal * area;
		vec3 vel_face = get_velocity(simulation, cell);
        real A = inv_A_matrix(face.cell*3);
        vec3 source = A*get_H(simulation, face.cell);

		p_P(cell, cell, area * -2 * dx * A);
		p_S(cell, -area * 2 * face.pressure * dx * A);
        //p_S(cell, dot(source, anormal));
	}

	simulation.pressure_matrix.setFromTriplets(coeffs.begin(), coeffs.end());
}

void correct_velocity(Simulation& sim, real relaxation) {
    vec_x& denom = sim.pressure_source_term;
    denom.fill(0);
    
    
    for (uint i = 0; i < sim.num_cells; i++) {
        real inv_A = sim.inv_A_matrix[i*3];
        
        vec3 new_velocity = inv_A * get_H(sim, i) - inv_A * get_pressure_gradient(sim, i);
        vec3 old_velocity = get_velocity(sim, i);
        
        vec3 velocity = old_velocity + (new_velocity - old_velocity)*relaxation;
        
        sim.velocities[i*3+0] = velocity.x;
        sim.velocities[i*3+1] = velocity.y;
        sim.velocities[i*3+2] = velocity.z;
    }
}

void compute_mass_residual(Simulation& sim) {
    vec_x residual;
    residual.resize(sim.num_cells);
    residual.fill(0);
    
    for (Face face : sim.faces) {
        vec3 anormal = face.area * face.normal;
        vec3 vel = (get_velocity(sim, face.cell1) + get_velocity(sim, face.cell2)) / 2.0f;
        residual(face.cell1) += dot(vel, anormal);
        residual(face.cell2) -= dot(vel, anormal);
    }
    
    for (PressureBoundaryFace face : sim.pressure_boundary_faces) {
        vec3 anormal = face.area * face.normal;
        vec3 vel = get_velocity(sim, face.cell);
        residual(face.cell) += dot(vel, anormal);
    }
    
    for (uint i = 0; i < sim.num_cells; i++) {
        residual[i] /= sim.volumes[i];
    }
    
    printf("Mean mass residual %f\n", residual.mean());
}

#include <iostream>

void solve_matrix(Simulation& simulation, SparseMt& sparse, vec_x& values, vec_x& source, bool first, real relaxation) {
	Eigen::BiCGSTAB<Eigen::SparseMatrix<real>> solver;
	solver.compute(sparse);

	//std::cout << sparse << std::endl;
	//std::cout << source << std::endl;
    
	if (first) {
		values = solver.solve(source);
	}
	else {
		vec_x new_values = solver.solveWithGuess(source, values);
        values += (new_values - values) * relaxation;
        printf("Relaxation %f\n", relaxation);
	}

	//std::cout << "Solution" << std::endl;
	//std::cout << values << std::endl;
}

void draw_vector_field(Simulation& simulation, vec_x& vec) {
    CFDDebugRenderer& debug = simulation.debug;
    clear_debug_stack(debug);
    
    float max_velocity = 0.0f;
    
    for (uint i = 0; i < simulation.num_cells; i++) {
        vec3 u;
        u.x = vec[i*3 + 0];
        u.y = vec[i*3 + 1];
        u.z = vec[i*3 + 2];
        
        max_velocity = fmaxf(max_velocity, length(u));
    }
    
    for (uint i = 0; i < simulation.num_cells; i += 1) {
        vec3 c = simulation.centers[i];
        vec3 u;
        u.x = vec[i*3 + 0];
        u.y = vec[i*3 + 1];
        u.z = vec[i*3 + 2];
        
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

void draw_scalar_field(Simulation& simulation, vec_x& vec) {
	CFDDebugRenderer& debug = simulation.debug;
	clear_debug_stack(debug);

	float max_pressure = 0.0f;

	for (uint i = 0; i < simulation.num_cells; i++) {
		real pressure = vec[i];
		max_pressure = fmaxf(max_pressure, pressure);
	}

    for (uint i = 0; i < simulation.num_cells; i += 1) {
        vec3 c = simulation.centers[i];
        real p = vec[i];
        
        //vec3 gradient = get_pressure_gradient(simulation, i);
        //p = length(gradient);

        real arrow = 0.1;
        vec4 color = color_map(p, 0, max_pressure);

        draw_line(debug, c + vec3(-arrow, 0, 0), c + vec3(arrow, 0, 0), color);
        draw_line(debug, c + vec3(0, -arrow, 0), c + vec3(0, arrow, 0), color);
        draw_line(debug, c + vec3(0, 0, -arrow), c + vec3(0, 0, arrow), color);
    }
}


CFDResults simulate(CFDVolume& volume, CFDDebugRenderer& debug) {
	Simulation sim = make_simulation(volume, debug);
	
	CFDResults result;
    
    sim.H_Matrix.resize(sim.num_cells*3);
    sim.inv_A_matrix.resize(sim.num_cells*3);
    sim.velocity_extra_source_term.resize(sim.num_cells*3);
    sim.H_Matrix.fill(0);
    sim.inv_A_matrix.fill(1);
    sim.velocity_extra_source_term.fill(0);
    
    build_pressure_matrix(sim, true);
    solve_matrix(sim, sim.pressure_matrix, sim.pressures, sim.pressure_source_term, false, 1.0);
    draw_scalar_field(sim, sim.pressures);
    suspend_execution(debug);

    real velocity_relaxation = 0.8;
    real pressure_relaxation = 0.2;
    
	uint n = 100;
	for (uint i = 0; i < n; i++) {
		printf("Iteration %i, (%ix%i)\n", i, sim.num_cells * 4, sim.num_cells * 4);
		compute_gradients(sim);
        
        build_velocity_matrix(sim);
        solve_matrix(sim, sim.velocity_matrix, sim.velocities, sim.velocity_source_term, i==0, velocity_relaxation);
        compute_H(sim);
        correct_velocity(sim, 1.0);
        
        printf("Max velocity %f\n", sim.velocities.maxCoeff());
        //draw_vector_field(sim, sim.velocities);
        //suspend_execution(debug);
        
        //draw_vector_field(sim, sim.H_Matrix);
        printf("Max H matrix %f\n", sim.H_Matrix.maxCoeff());
        //suspend_execution(debug);
        
        build_pressure_matrix(sim, false);
        solve_matrix(sim, sim.pressure_matrix, sim.pressures, sim.pressure_source_term, false, pressure_relaxation);
        
        //draw_simulation_state(sim, true, false);
        //suspend_execution(debug);
        
        draw_scalar_field(sim, sim.pressures);
        suspend_execution(debug);
        
        compute_gradients(sim);
        
        printf("Corrected continuity\n");
        correct_velocity(sim, 1.0);
        compute_mass_residual(sim);
        
        draw_vector_field(sim, sim.velocities);
        suspend_execution(debug);
	}
    
    printf("Done!\n");

	//result.velocities = std::move(simulation.velocities);
	//result.pressures = std::move(simulation.pressures);

	return result;
}
