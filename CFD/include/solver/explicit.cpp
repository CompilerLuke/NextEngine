/*
const real mu = 1.0;
const real rho = 1.0;

const vec3 u_numerator(vec3 u_plus, vec3 u_minus, real p_plus, real p_minus, vec3 normal, real area, real dx) {
	real p = (p_plus + p_minus) / 2.0f;
	vec3 u = (u_plus + u_minus) / 2.0f;

	vec3 convective = -rho * dot(u, normal) * u_plus;
	vec3 pressure = -2 * p * normal;
	vec3 diffusion = 2 * mu * u_plus * dx;

	vec3 result = area * (convective + pressure + diffusion);
	return result;
}

const real u_denominator(vec3 u_plus, vec3 u_minus, real p_plus, real p_minus, vec3 normal, real area, real dx) {
	vec3 u = (u_plus + u_minus) / 2.0f;

	return area * (rho * dot(u, normal) + 2 * mu * dx);
}

void update_velocities(Simulation& simulation) {
	vec3* U = simulation.velocities.data;
	real* P = simulation.pressures.data;

	vec3* U_sum_num = simulation.velocities_numer.data;
	real* U_sum_denom = (real*)simulation.velocities_denom.data;

	memclear_t(U_sum_num, simulation.num_cells);
	memclear_t(U_sum_denom, simulation.num_cells);

	for (Face face : simulation.faces) {
		real area = face.area;
		real dx = face.dx;

		vec3 normal1 = face.normal;
		vec3 normal2 = -face.normal;

		vec3 u_plus = U[face.cell2];
		vec3 u_minus = U[face.cell1];

		real p_plus = P[face.cell2];
		real p_minus = P[face.cell1];

		U_sum_num[face.cell1] += u_numerator(u_plus, u_minus, p_plus, p_minus, normal1, area, dx);
		U_sum_num[face.cell2] += u_numerator(u_minus, u_plus, p_minus, p_plus, normal2, area, dx);

		U_sum_denom[face.cell1] += u_denominator(u_plus, u_minus, p_plus, p_minus, normal1, area, dx);
		U_sum_denom[face.cell2] += u_denominator(u_minus, u_plus, p_minus, p_plus, normal2, area, dx);
	}


	for (BoundaryFace face : simulation.boundary_faces) {
		real area = face.area;
		vec3 normal1 = face.normal;

		vec3 u_minus = U[face.cell];
		vec3 u_plus = -u_minus;
		real p_minus = P[face.cell];

		//(A + B)/2 = C 
		//A + B = 2C
		//A = 2C - b

		//vec3 wall_vel = u_minus - u_minus*dot(u_minus, -normal1);
		//vec3 u_plus = 2 * wall_vel - u_minus;

		U_sum_num[face.cell] += u_numerator(u_plus, u_minus, p_minus, p_minus, normal1, area, 0);
		U_sum_denom[face.cell] += u_denominator(u_plus, u_minus, p_minus, p_minus, normal1, area, 0);
	}

	for (PressureBoundaryFace face : simulation.pressure_boundary_faces) {
		real area = face.area;
		vec3 normal1 = face.normal;

		vec3 u_minus = U[face.cell];
		real p_minus = P[face.cell];
		real p_plus = p_minus; //face.pressure_gradient; //p_minus + face.pressure_gradient * face.dx;

		U_sum_num[face.cell] += u_numerator(u_minus, u_minus, p_plus, p_minus, normal1, area, face.dx);
		U_sum_denom[face.cell] += u_denominator(u_minus, u_minus, p_plus, p_minus, normal1, area, face.dx);
	}

	for (uint i = 0; i < simulation.num_cells; i++) {
		U[i] = U_sum_num[i] / U_sum_denom[i];

		if (U[i] < -1000) {
			printf("What happened!\n");
		}
	}
}

real p_numerator(vec3 u_plus, vec3 u_minus, real p_plus, real p_minus, vec3 normal, real area, real dx) {
	vec3 u = (u_plus + u_minus) / 2;
	real u_conv = mu * dot(u_plus - u_minus, u);
	return area * (p_plus + u_conv) * dx;
}

real p_denominator(vec3 u_plus, vec3 u_minus, real p_plus, real p_minus, vec3 normal, real area, real dx) {
	return area * dx;
}

void update_pressure(Simulation& simulation) {
	vec3* U = simulation.velocities.data;
	real* P = simulation.pressures.data;

	real* P_sum_num = (real*)simulation.velocities_numer.data;
	real* P_sum_denom = (real*)simulation.velocities_denom.data;

	memclear_t(P_sum_num, simulation.num_cells);
	memclear_t(P_sum_denom, simulation.num_cells);

	for (Face face : simulation.faces) {
		real area = face.area;
		real dx = face.dx;

		vec3 normal1 = face.normal;
		vec3 normal2 = -face.normal;

		vec3 u_plus = U[face.cell2];
		vec3 u_minus = U[face.cell1];

		real p_plus = P[face.cell2];
		real p_minus = P[face.cell1];

		P_sum_num[face.cell1] += p_numerator(u_plus, u_minus, p_plus, p_minus, normal1, area, dx);
		P_sum_num[face.cell2] += p_numerator(u_minus, u_plus, p_minus, p_plus, normal2, area, dx);

		P_sum_denom[face.cell1] += p_denominator(u_plus, u_minus, p_plus, p_minus, normal1, area, dx);
		P_sum_denom[face.cell2] += p_denominator(u_minus, u_plus, p_minus, p_plus, normal2, area, dx);
	}

	for (BoundaryFace face : simulation.boundary_faces) {
		real area = face.area;
		vec3 normal1 = face.normal;

		vec3 u_minus = U[face.cell];
		vec3 u_plus = -u_minus;
		real p_minus = P[face.cell];

		P_sum_num[face.cell] += p_numerator(u_plus, u_minus, p_minus, p_minus, normal1, area, face.dx);
		P_sum_denom[face.cell] += p_denominator(u_plus, u_minus, p_minus, p_minus, normal1, area, face.dx);
	}

	for (PressureBoundaryFace face : simulation.pressure_boundary_faces) {
		real area = face.area;
		vec3 normal1 = face.normal;

		vec3 u_minus = U[face.cell];
		real p_minus = P[face.cell];
		real p_plus = p_minus; //face.pressure_gradient; //p_minus + face.pressure_gradient / face.dx;

		P_sum_num[face.cell] += p_numerator(u_minus, u_minus, p_plus, p_minus, normal1, area, face.dx);
		P_sum_denom[face.cell] += p_denominator(u_minus, u_minus, p_plus, p_minus, normal1, area, face.dx);
	}

	for (uint i = 0; i < simulation.num_cells; i++) {
		P[i] = P_sum_num[i] / P_sum_denom[i];

		if (P[i] < 0 || P[i] > 1000) {
			printf("What happened!\n");
		}
	}
}


void draw_simulation_state(Simulation& simulation, bool show_velocity, bool show_pressure) {
	CFDDebugRenderer& debug = simulation.debug;
	clear_debug_stack(debug);

	float max_pressure = 0.0f;
	float max_velocity = 0.0f;

	for (uint i = 0; i < simulation.num_cells; i++) {
		max_pressure = max(max_pressure, simulation.pressures[i]);
		max_velocity = max(max_velocity, length(simulation.velocities[i]));
	}

	if (show_velocity) {
		for (uint i = 0; i < simulation.num_cells; i += 1) {
			vec3 c = simulation.centers[i];
			vec3 u = simulation.velocities[i];
			vec3 t = normalize(u);
			vec3 n = vec3(0, 1, 0);
			vec3 b = normalize(cross(t, n));

			vec4 color = color_map(length(u), 0, max_velocity);

			real length = 0.05;

			vec3 start = c - t * length / 2;
			vec3 end = c + t * length / 2;

			float arrow = 0.25 * length;

			draw_line(debug, start, end, color);
			draw_line(debug, end, c + n * arrow, color);
			draw_line(debug, end, c - n * arrow, color);
		}
	}

	if (show_pressure) {
		for (uint i = 0; i < simulation.num_cells; i += 1) {
			vec3 c = simulation.centers[i];
			real p = simulation.pressures[i];

			real arrow = 0.01;
			vec4 color = color_map(p, 0, max_pressure);

			draw_line(debug, c + vec3(-arrow, 0, 0), c + vec3(arrow, 0, 0), color);
			draw_line(debug, c + vec3(0, -arrow, 0), c + vec3(0, arrow, 0), color);
			draw_line(debug, c + vec3(0, 0, -arrow), c + vec3(0, 0, arrow), color);
		}
	}
}
*/