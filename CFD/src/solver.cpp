#if 1

#include "solver.h"
#include "engine/handle.h"
#include "core/memory/linear_allocator.h"
#include "visualization/debug_renderer.h"
#include "visualization/color_map.h"

#include "vendor/eigen/Eigen/Sparse"
#include "vendor/eigen/Eigen/IterativeLinearSolvers"

#include "core/time.h"
#include "core/math/interpolation.h"

#include "visualization/visualizer.h"

using vec_x = Eigen::VectorXd;
using SparseMt = Eigen::SparseMatrix<real>;
using T = Eigen::Triplet<real>;

Eigen::Vector3d to(vec3 vec) { return Eigen::Vector3d(vec.x, vec.y, vec.z); }
vec3 from(Eigen::Vector3d vec) { return vec3(vec[0], vec[1], vec[2]); }
vec3 unpack3(vec_x& vec, uint i) {
    vec3 result;
    result.x = vec[i*3 + 0];
    result.y = vec[i*3 + 1];
    result.z = vec[i*3 + 2];
    
    return result;
}

struct Face {
	real dx;
	vec3 normal;
	vec3 ortho;
	uint cell1, cell2;
    bool is_boundary1;
    bool is_boundary2;
};

struct BoundaryFace {
	real dx;
	vec3 normal;
    vec3 cell_to_face;
	uint cell;
    vec3 velocity;
};

struct PressureBoundaryFace {
	real dx;
    real pressure_dx;
	vec3 normal;
	real pressure_grad;
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
	vector<real> inv_volumes;
	vec_x velocities;
    vec_x pressures;
    vec_x velocities_last;
    vec_x pressures_last;
    vec_x vof;
    
    real rho1 = 1;
    real rho2 = 1;
    real mu1 = 1;
    real mu2 = 1;
    real g = 0;
    
    real t = 0;
    uint tstep = 0;
};

void right_angle_corrector(vec3 normal, vec3 to_center, real* parallel, vec3* ortho);
/*{
	to_center = normalize(to_center);
    *parallel = dot(normal, to_center);
	*ortho = normal - to_center*(*parallel);
}*/

bool is_boundary(const CFDCell& cell) {
    const ShapeDesc& shape = shapes[cell.type];
    for (uint f = 0; f < shape.num_faces; f++) {
        if (cell.faces[f].neighbor.id == -1) {
            return true;
        }
    }
    return false;
}

real base_pressure = 1e4;

Simulation* make_simulation(CFDVolume& mesh, CFDDebugRenderer& debug) {
    Simulation* sim_p = new Simulation{debug };
    Simulation& sim = *sim_p;
    
	sim.num_cells = mesh.cells.length;
	sim.centers.resize(sim.num_cells);
	sim.inv_volumes.resize(sim.num_cells);
	sim.velocity_gradients.resize(sim.num_cells);
	sim.pressure_gradients.resize(sim.num_cells);

	sim.velocities.resize(3* sim.num_cells);
    sim.pressures.resize(sim.num_cells);
    sim.vof.resize(sim.num_cells);
    
    sim.velocity_pressure_source_term.resize(sim.num_cells*3);
    sim.pressure_matrix.resize(sim.num_cells, sim.num_cells);
    sim.velocity_matrix.resize(3*sim.num_cells, 3*sim.num_cells);
    
	for (uint i = 0; i < mesh.cells.length; i++) {
		const CFDCell& cell = mesh.cells[i];
		const ShapeDesc& shape = shapes[cell.type];

		uint cell_id = i;

		vec3 center = compute_centroid(mesh, cell.vertices, shape.num_verts);

		sim.centers[i] = center;

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

            real area = is_quad ? quad_area(v) : triangle_area(v);
			vec3 normal = is_quad ? quad_normal(v) : triangle_normal(v);
            normal *= area;

			cell_handle neighbor = cell.faces[f].neighbor;
			bool boundary = neighbor.id == -1;

			volume += dot(face_center, normal);

			if (boundary) {
                vec3 cell_to_face = face_center - center;
                real dx = length(cell_to_face);
				real inv_dx = 1.0 / (2.0*dx) * area;
				if (cell.faces[f].pressure != 0.0f) {
                    draw_quad(debug, v, BLUE_DEBUG_COLOR);
					sim.pressure_boundary_faces.append(PressureBoundaryFace{ inv_dx, cell.faces[f].pressure*dx, normal, cell.faces[f].pressure, cell_id });
				}
				else {
					sim.boundary_faces.append(BoundaryFace{ inv_dx, normal, cell_to_face, cell_id, cell.faces[f].velocity });
				}
			}
			else if (neighbor.id > cell_id) {
                sim.faces.append(Face{0, normal, 0, cell_id, (uint)neighbor.id, is_on_boundary, is_boundary(mesh[neighbor])});
			}
		}

		volume /= 3;
		sim.inv_volumes[cell_id] = 1.0 / volume;
	}

	for (Face& face : sim.faces) {
		vec3 center_minus = sim.centers[face.cell1];
		vec3 center_plus = sim.centers[face.cell2];
        
        vec3 t = center_plus - center_minus;

        real parallel;
		right_angle_corrector(face.normal, center_plus - center_minus, &parallel, &face.ortho);
        face.dx = 1.0 / length(t) * parallel;
	}

	for (uint i = 0; i < sim.num_cells; i++) {
		sim.velocities(i*3 + 0) = 0;
		sim.velocities(i*3 + 1) = 0;
		sim.velocities(i*3 + 2) = 0;
        sim.pressures(i) = base_pressure; // - sim.centers[i].y * sim.rho2;;
        sim.vof(i) = 1.0; //sim.centers[i].y > 0 ? 1.0 : 0.0
        sim.velocity_gradients[i].fill(0);
        sim.pressure_gradients[i].fill(0);
	}
    
    CFDResults result;
    
    Eigen::setNbThreads(4);
    
    sim.H_Matrix.resize(sim.num_cells*3);
    sim.inv_A_matrix.resize(sim.num_cells*3);
    sim.velocity_extra_source_term.resize(sim.num_cells*3);
    sim.H_Matrix.fill(0);
    sim.inv_A_matrix.fill(1);
    sim.velocity_extra_source_term.fill(0);
    sim.velocity_pressure_source_term.fill(0);
    
    sim.velocities_last = sim.velocities;
    sim.pressures_last = sim.pressures;

	return sim_p;
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

Eigen::Matrix3d get_velocity_gradient(Simulation& simulation, uint cell) {
    return simulation.velocity_gradients[cell];
}

vec3 get_pressure_gradient(Simulation& simulation, uint cell) {
    return from(simulation.pressure_gradients[cell]);
}

vec3 get_H(Simulation& sim, uint cell) {
    vec3 source;
    source.x = sim.H_Matrix[cell*3 + 0];
    source.y = sim.H_Matrix[cell*3 + 1];
    source.z = sim.H_Matrix[cell*3 + 2];
    return source;
};

vec3 pressure_gradient_at_wall(Simulation& sim, const BoundaryFace& f) {
    vec3 H = get_H(sim, f.cell);
    vec3 inv_A = unpack3(sim.inv_A_matrix, f.cell);
    //vec3 U = get_velocity(sim, f.cell);
    
    return vec3(0,sim.rho2*sim.g,0); //,f.normal);// - dot(U/inv_A,f.normal);
    //return pressure_gradient_area;
}

void compute_gradients(Simulation& simulation, real t) {
	auto& velocity_gradients = simulation.velocity_gradients;
	auto pressure_gradients = simulation.pressure_gradients;

	for (uint i = 0; i < simulation.num_cells; i++) {
		velocity_gradients[i].fill(0.0f);
		pressure_gradients[i].fill(0.0f);
	}

	for (Face f : simulation.faces) {
		vec3 velocity_down = get_velocity(simulation, f.cell1);
		vec3 velocity_up = get_velocity(simulation, f.cell2);
		Eigen::Vector3d velocity = to((velocity_down + velocity_up) / 2);
		
		real pressure_down = get_pressure(simulation, f.cell1);
		real pressure_up = get_pressure(simulation, f.cell2);
		real pressure = (pressure_down + pressure_up) / 2;

        Eigen::Vector3d normal = to(f.normal);

		Eigen::Matrix3d vel_gradient = velocity * normal.transpose();
		Eigen::Vector3d pressure_gradient = pressure * normal;
	
		velocity_gradients[f.cell1] += vel_gradient;
		pressure_gradients[f.cell1] += pressure_gradient;

		velocity_gradients[f.cell2] -= vel_gradient;
		pressure_gradients[f.cell2] -= pressure_gradient;
	}

	for (BoundaryFace f : simulation.boundary_faces) {
        Eigen::Vector3d normal = to(f.normal);
        
        real gravity = simulation.rho2 * simulation.g;
        vec3 gradient = pressure_gradient_at_wall(simulation, f);
        
		real pressure = get_pressure(simulation, f.cell) + dot(gradient, f.cell_to_face);
        velocity_gradients[f.cell] += to(f.velocity) * to(f.normal).transpose();
		pressure_gradients[f.cell] += pressure * normal;
	}

	for (PressureBoundaryFace f : simulation.pressure_boundary_faces) {
		Eigen::Vector3d velocity = to(get_velocity(simulation, f.cell));
        real pressure = get_pressure(simulation, f.cell);
		Eigen::Vector3d normal = to(f.normal);
    
		velocity_gradients[f.cell] += velocity * normal.transpose();
		pressure_gradients[f.cell] += (pressure + f.pressure_dx) * normal;
	}

	for (uint i = 0; i < simulation.num_cells; i++) {
		velocity_gradients[i] *= simulation.inv_volumes[i];
		pressure_gradients[i] *= simulation.inv_volumes[i];
        //printf("Gradient %d\n", pressure_gradients[i][1]);
	}
    
    simulation.pressure_gradients = pressure_gradients;
}

void velocity_at_face(Simulation& simulation, uint cell, uint neigh) {
    
}

void vof_property(Simulation& sim, real* mu, real* rho, real vof) {
    *mu = lerp(sim.mu1, sim.mu2, vof);
    *rho = lerp(sim.rho1, sim.rho2, vof);
}

void build_velocity_matrix(Simulation& sim, real t, real dt) {
    vector<T>& coeffs = sim.coeffs;
    vec_x& pressure_source = sim.velocity_pressure_source_term;
    vec_x& extra_source = sim.velocity_extra_source_term;

    coeffs.clear();
    pressure_source.resize(sim.num_cells * 3);
    extra_source.resize(sim.num_cells * 3);
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

    auto face_contribution = [&](uint cell, uint neigh, vec3 vel_down, vec3 vel_up, vec3 pressure_down, vec3 pressure_up, vec3 normal, vec3 ortho, real dx) {
        
        Eigen::Matrix3d vel_gradient_down = get_velocity_gradient(sim,cell);
        Eigen::Matrix3d vel_gradient_up = get_velocity_gradient(sim, neigh);

        bool upwind = dot(normal, vel_down) > 0;
        
        real vof = (sim.vof[cell] + sim.vof[neigh]) / 2.0;
        real mu, rho;
        vof_property(sim, &mu, &rho, vof);

        vec3 pressure_face = (pressure_down + pressure_up) / 2.0f;
        vec3 vel_face = upwind ? vel_down : vel_up; // (vel_up + vel_down) / 2.0f;
        Eigen::Matrix3d vel_gradient_face = (vel_gradient_down + vel_gradient_up) / 2;
        
        real conv_coeff = rho * dot(vel_face, normal);
        
        real inv_volume = sim.inv_volumes[cell];

        //convective acceleration
        m_U(cell, upwind ? cell : neigh, conv_coeff * inv_volume);

        //pressure source
        m_S_P(cell, -pressure_face * normal * inv_volume);

        //velocity gradient
        m_U(cell, neigh, -mu * dx * inv_volume);
        m_U(cell, cell,   mu * dx * inv_volume);
        //orthogonal corrector
        m_S_E(cell, mu * from(vel_gradient_face * to(ortho)) * inv_volume);
    };


    for (const Face& face : sim.faces) {
        vec3 vel_down = get_velocity(sim, face.cell1);
        vec3 vel_up = get_velocity(sim, face.cell2);
        real pressure_down = get_pressure(sim, face.cell1);
        real pressure_up = get_pressure(sim, face.cell2);
        
        face_contribution(face.cell1, face.cell2, vel_down, vel_up, pressure_down, pressure_up, face.normal, face.ortho, face.dx);
        face_contribution(face.cell2, face.cell1, vel_up, vel_down, pressure_up, pressure_down, -face.normal, -face.ortho, face.dx);
    }

    for (const BoundaryFace& f : sim.boundary_faces) {
        vec3 pressure = get_pressure(sim, f.cell);
        vec3 pressure_grad = pressure_gradient_at_wall(sim, f); // get_pressure_gradient(sim, f.cell);
        real inv_volume = sim.inv_volumes[f.cell];

        real vof = sim.vof[f.cell];
        real mu, rho;
        vof_property(sim, &mu, &rho, vof);
        
        vec3 velocity = get_velocity(sim, f.cell);
        vec3 pressure_extrap = pressure + dot(pressure_grad, f.cell_to_face);
        
        //if (fabs(dot(velocity, f.velocity)) > 0.5)
        m_U(f.cell, f.cell, rho * dot(f.velocity, f.normal) * inv_volume);
    
        m_U(f.cell, f.cell, 2 * mu * f.dx * inv_volume); //todo eliminate 2 term
        m_S_E(f.cell, 2 * mu * f.dx * f.velocity * inv_volume);
        
        m_S_P(f.cell, -pressure_extrap * f.normal * inv_volume);
    }

    for (const PressureBoundaryFace& f : sim.pressure_boundary_faces) {
        vec3 vel_face = get_velocity(sim, f.cell);
        real inv_volume = sim.inv_volumes[f.cell];
        
        real vof = sim.vof[f.cell];
        real mu, rho;
        vof_property(sim, &mu, &rho, vof);
        
        real pressure_face = get_pressure(sim, f.cell) + f.pressure_dx;
    
        //convective acceleration
        m_U(f.cell, f.cell, rho * dot(vel_face, f.normal) * inv_volume);
        m_S_P(f.cell, -pressure_face * f.normal * inv_volume);
    }
    
    vec_x cell_coeff(sim.num_cells*3);
    for (uint i = 0; i < sim.num_cells; i++) {
        real mu, rho;
        vof_property(sim, &mu, &rho, sim.vof[i]);
        
        real coeff = rho/dt;
        
        cell_coeff[i*3 + 0] = coeff;
        cell_coeff[i*3 + 1] = coeff;
        cell_coeff[i*3 + 2] = coeff;
        
        sim.velocity_extra_source_term[i*3 + 1] += rho*sim.g;
    }
    
    sim.velocity_extra_source_term += cell_coeff.cwiseProduct(sim.velocities_last);
    
    sim.velocity_source_term = sim.velocity_extra_source_term + sim.velocity_pressure_source_term;
    sim.velocity_matrix.setFromTriplets(coeffs.begin(), coeffs.end());
    
    sim.velocity_matrix.diagonal().array() += cell_coeff.array();
}

void check_H(Simulation& simulation) {
    const auto A_Diagonal = simulation.velocity_matrix.diagonal();
    auto& H = simulation.H_Matrix;
    auto& V_Matrix = simulation.velocity_matrix;
    auto& inv_A_matrix = simulation.inv_A_matrix;
    
    //H = A_Diagonal.cwiseProduct(simulation.velocities) - V_Matrix*simulation.velocities + simulation.velocity_extra_source_term;
    
    //AU - H
    //AU - (AU - MU + S)
    //AU - (AU - P-S + S)
    //-(-P) = P
    
    vec_x result = A_Diagonal.cwiseProduct(simulation.velocities) - H;
    vec_x result_exact = simulation.velocity_pressure_source_term; //simulation.velocity_matrix*simulation.velocities;
    
    printf("Difference between pressure AU - H%f\n", (result - result_exact).maxCoeff());
    
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
        vec3 gradient = -get_pressure_gradient(simulation, i);
        vec3 extra_source = unpack3(simulation.velocity_extra_source_term, i);
        vec3 pressure_source = unpack3(simulation.velocity_pressure_source_term, i);
        
        vec3 u = unpack3(simulation.velocities, i);
        vec3 pred_u = unpack3(pred_velocities, i);
        
        real r = length(residual - gradient);
        
        if (r > 0.5) {
            printf("difference: %f, residual: %f, LHS: %f, gradient: %f, extra: %f \n", r, length(residual), length(pressure_source), length(gradient), length(extra_source));
        }
    }
}

void compute_H(Simulation& simulation) {
    const auto A_Diagonal = simulation.velocity_matrix.diagonal();
    auto& H = simulation.H_Matrix;
    auto& V_Matrix = simulation.velocity_matrix;
    auto& inv_A_matrix = simulation.inv_A_matrix;
    
    inv_A_matrix= A_Diagonal;
    
    auto& source = simulation.velocity_extra_source_term; //simulation.velocity_source_term - simulation.velocity_pressure_source_term;
    
    H = A_Diagonal.cwiseProduct(simulation.velocities) - simulation.velocity_pressure_source_term;
    //V_Matrix*simulation.velocities + simulation.velocity_extra_source_term;
    //simulation.velocity_pressure_source_term; //simulation.velocity_source_term + source;
    
    int row = 0;
    H.maxCoeff(&row);
    
    vec3 diagonal = unpack3(inv_A_matrix, row/3);
    
    inv_A_matrix = A_Diagonal.cwiseInverse();
    

    check_H(simulation);
}

#include <iostream>

void build_pressure_matrix(Simulation& simulation, bool first, real t) {
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

    uint bottom_row = 1;
    
	//pressure 
	auto p_P = [&](uint n, uint m, real coeff) {
        //if (n < bottom_row && m != n) return;
        //else if (m == 0) source[n] -= base_pressure*coeff;
		//else
        coeffs.append(T(n, m, coeff));
	};

	auto face_contribution = [&](uint cell, uint neigh, vec3 normal, vec3 ortho, real dx, bool is_boundary) {
        
		vec3 pressure_gradient_down = get_pressure_gradient(simulation, cell);
		vec3 pressure_gradient_up = get_pressure_gradient(simulation, neigh);
		vec3 pressure_gradient_face = (pressure_gradient_down + pressure_gradient_up) / 2;

        real A_cell = inv_A_matrix(cell*3);
        real A_neigh = inv_A_matrix(neigh*3);
        real A = (A_cell + A_neigh) / 2;

		//pressure gradient
		p_P(cell, neigh, dx * A_cell);
		p_P(cell, cell, -dx * A_cell);
		//orthogonal corrector
		p_S(cell, -A_cell * dot(pressure_gradient_face, ortho));

		//H matrix source
        vec3 source = (A_cell*get_H(simulation, cell) + A_neigh*get_H(simulation, neigh)) / 2.0;
        
        //if (!is_boundary) {
            p_S(cell, dot(source, normal));
        //}
	};

	for (const Face& face : simulation.faces) {
		face_contribution(face.cell1, face.cell2, face.normal, face.ortho, face.dx, face.is_boundary1);
		face_contribution(face.cell2, face.cell1, -face.normal, -face.ortho, face.dx, face.is_boundary2);
	}
    
    //C = (A + B)
    //-C + B
    //(-A - B) + B
    //-A

	for (const BoundaryFace& f : simulation.boundary_faces) {
		real dx = f.dx;
        real A = inv_A_matrix(f.cell*3);
        vec3 source = //A*get_H(simulation, f.cell);
        -A*unpack3(simulation.velocity_pressure_source_term, f.cell);

        //vec3 velocity = get_velocity(simulation, f.cell) - f.velocity;
        //real d = dot(normalize(f.normal), velocity);
        
        //p_P(f.cell, f.cell, -2 * f.dx * A);
        p_S(f.cell, -A*dot(pressure_gradient_at_wall(simulation, f), f.normal));
        p_S(f.cell, dot(source, f.normal));
        //p_S(f.cell, 2 * d * dx * A);
    }

	for (const PressureBoundaryFace& f: simulation.pressure_boundary_faces) {
		vec3 vel_face = get_velocity(simulation, f.cell);
        real A = inv_A_matrix(f.cell*3);
        vec3 source = A*get_H(simulation, f.cell);

		//p_P(f.cell, f.cell, -2 * f.dx * A);
		p_S(f.cell, -f.pressure_grad * length(f.normal) * A);
        p_S(f.cell, dot(source, f.normal));
	}

    /*for (uint i= 0;i<bottom_row;i++) {
        coeffs.append(T(i,0,0));
    }*/
	simulation.pressure_matrix.setFromTriplets(coeffs.begin(), coeffs.end());
    /*for (uint i =0;i<bottom_row;i++) {
        simulation.pressure_matrix.diagonal()[i] += 1.0;
        simulation.pressure_source_term[i] += base_pressure;
    }*/
    //std::cout << simulation.pressure_matrix << std::endl;
    //std::cout << simulation.pressure_source_term << std::endl;
}

void correct_velocity(Simulation& sim, real relaxation) {
    //return;
    
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
    
    for (Face f : sim.faces) {
        vec3 vel_down = get_velocity(sim, f.cell1);
        vec3 vel_up = get_velocity(sim, f.cell2);
        vec3 vel = dot(f.normal,vel_down) > 0 ? vel_down : vel_up;
        
        residual(f.cell1) += dot(vel, f.normal);
        residual(f.cell2) -= dot(vel, f.normal);
    }
    
    for (PressureBoundaryFace f : sim.pressure_boundary_faces) {
        vec3 vel = get_velocity(sim, f.cell);
        residual(f.cell) += dot(vel, f.normal);
    }
    
    for (uint i = 0; i < sim.num_cells; i++) {
        residual[i] *= sim.inv_volumes[i];
    }
    
    printf("Mean mass residual %f\n", residual.mean());
}

#include <iostream>

void under_relax(SparseMt& sparse, const vec_x& previous, vec_x& source, real alpha) {
    if (alpha > 1.0) return;
    real factor = (1 - alpha) / alpha;
    
    source += sparse.diagonal().cwiseProduct(previous) * factor;
    sparse.diagonal() += sparse.diagonal() * factor;
}

real solve_matrix(Simulation& simulation, SparseMt& sparse, vec_x& values, vec_x& source, bool first, real relaxation) {
	Eigen::BiCGSTAB<SparseMt> solver;
	
    if (!first) under_relax(sparse, values, source, relaxation);
    
    solver.compute(sparse);

	//std::cout << sparse << std::endl;
	//std::cout << source << std::endl;
    
    vec_x new_values;
    if (first) new_values = solver.solve(source);
    else new_values = solver.solveWithGuess(source, values);

    real change = ((new_values - values).cwiseAbs() / fmaxf(0.001, fabs(values.maxCoeff()))).maxCoeff();
    /// values).coeffMean();
    
    values = new_values;
    
	//std::cout << "Solution" << std::endl;
	//std::cout << values << std::endl;
    //std::cout << "End" << std::endl;
    //std::cout << sparse * values << std::endl;
    
    return change;
}

void draw_vector_field(Simulation& simulation, vec_x& vec) {
    CFDDebugRenderer& debug = simulation.debug;
    clear_debug_stack(debug);
    
    float max_velocity = 10.0f;
    
    /*for (uint i = 0; i < simulation.num_cells; i++) {
        vec3 u;
        u.x = vec[i*3 + 0];
        u.y = vec[i*3 + 1];
        u.z = vec[i*3 + 2];
        
        max_velocity = fmaxf(max_velocity, length(u));
    }*/
    
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

        vec3 start = c; // - t * l / 2;
        vec3 end = c + t * l;

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

CFDResults simulate_timestep(Simulation& sim, real dt) {
    CFDDebugRenderer& debug = sim.debug;
    real t = sim.t;
    
    real velocity_relaxation = 1.0;
    real pressure_relaxation = 0.6;
    
    for (PressureBoundaryFace& face : sim.pressure_boundary_faces) {
        face.pressure_grad = (face.normal.z > 0 ? 1 : -1) * sin(t) * 100;
    }
    
    //sim.tstep++;
    if (sim.tstep++ == 0) {
        build_pressure_matrix(sim, true, 0);
        
        //while(true) {
            solve_matrix(sim, sim.pressure_matrix, sim.pressures, sim.pressure_source_term, true, pressure_relaxation);
            draw_scalar_field(sim, sim.pressures);
            
            real max = sim.pressures.maxCoeff();
            printf("Min pressure %f\n", sim.pressures.minCoeff());
            printf("Max pressure %f\n", sim.pressures.maxCoeff());
            //return {};
            //if(max > 0) break;
        
        suspend_execution(debug);
        //}
        
        sim.velocities_last = sim.velocities;
        sim.pressures_last = sim.pressures;
    }
    
    real convergence = 0.1 / 100.0;
    
    real start_time = Time::now();
    
    uint max_inner_steps = 100;
    
    for (uint i = 0; i < max_inner_steps; i++) {
        printf("Iteration %i, (%ix%i)\n", i, sim.num_cells * 4, sim.num_cells * 4);
        
        compute_gradients(sim, t);
        build_velocity_matrix(sim, t, dt);

        real velocity_change = solve_matrix(sim, sim.velocity_matrix, sim.velocities, sim.velocity_source_term, i==0, velocity_relaxation);

        //draw_vector_field(sim, sim.velocities);
        
        compute_H(sim);
        
        printf("Max velocity %f\n", sim.velocities.maxCoeff());
        printf("Max H matrix %f\n", sim.H_Matrix.maxCoeff());
        
        //suspend_execution(debug);
        //break;
        
        auto pressures = sim.pressures;
        
        compute_gradients(sim, t);
        build_pressure_matrix(sim, false, t);
        real pressure_change = solve_matrix(sim, sim.pressure_matrix, sim.pressures, sim.pressure_source_term, false, pressure_relaxation);
        
        real min = sim.pressures.minCoeff();
        for (uint i = 0; i < sim.num_cells; i++) {
            sim.pressures[i] -= min;
        }

        printf("Min pressure %f\n", sim.pressures.minCoeff());
        printf("Max pressure %f\n", sim.pressures.maxCoeff());

        //draw_scalar_field(sim, sim.pressures);
        //suspend_execution(debug);
        
        compute_gradients(sim, t);
        
        printf("Before\n");
        compute_mass_residual(sim);
        printf("Corrected continuity\n");
        correct_velocity(sim, velocity_relaxation);
        compute_mass_residual(sim);
        
        printf("Change %f %f n", velocity_change*100, pressure_change*100);
        
        if (velocity_change < convergence && pressure_change < convergence) {
            printf("Converged after %i iterations!\n", i+1);
            break;
        }
    }
    
    sim.velocities_last = sim.velocities;
    sim.pressures_last = sim.pressures;
    
    //draw_vector_field(sim, sim.velocities_last);
    suspend_execution(debug);
    
    sim.t += dt;
    t = sim.t;
    
    /*compute_gradients(sim, t);
    build_pressure_matrix(sim, false, t);
    real pressure_change = solve_matrix(sim, sim.pressure_matrix, sim.pressures, sim.pressure_source_term, false, 1.0);
    correct_velocity(sim, velocity_relaxation);
    
    compute_gradients(sim, t);*/

    CFDResults result;
    result.max_velocity = 0;
    result.max_pressure = 0;
    result.velocities.resize(sim.num_cells);
    result.pressures.resize(sim.num_cells);
    result.vof.resize(sim.num_cells);
    for (uint i = 0; i < sim.num_cells; i++) {
        result.velocities[i] = unpack3(sim.velocities, i);
        result.vof[i] = sim.vof(i);
        result.pressures[i] = sim.pressures[i];
        // length(from(sim.pressure_gradients[i]));
        //sim.pressures[i];
        
        result.max_velocity = fmaxf(result.max_velocity, length(result.velocities[i]));
        result.max_pressure = fmaxf(result.max_pressure, result.pressures[i]);
    }
    result.max_velocity = 1.0;
	//result.velocities = std::move(simulation.velocities);
	//result.pressures = std::move(simulation.pressures);
    
    real end_time = Time::now();
    printf("Completed after %.3f s!\n", end_time - start_time);

	return result;
}


#endif
