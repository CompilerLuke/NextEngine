#pragma once

#include "graphics/pass/pass.h"

struct ComputeDesc {
	uint workgroup_x = 1;
	uint workgroup_y = 1;
	uint workgroup_z = 1;
};

void make_ComputePass(RenderPass::ID, const ComputeDesc&);