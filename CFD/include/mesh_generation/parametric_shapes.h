#pragma once

struct CFDVolume;
struct UI;

struct DuctOptions {
	float width = 2.0;
	float height = 2.0;
	float depth = 4;

	float constrain_start = 1.1;
	float constrain_end = 1.3;
	float transition = 0.1;
	float narrow = 1.1;

	float turn_start = 1.1;
	float turn_end = 1.3;

	float turn_angle = 90.0;

	int u_div = 2;
	int v_div = 2;
	int t_div = 2;

	bool pressure_differential = false;

};

CFDVolume generate_duct(const DuctOptions& options);
CFDVolume generate_parametric_mesh();

void parametric_ui(UI&);