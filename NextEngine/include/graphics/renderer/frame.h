#pragma once

#include "graphics/pass/render_pass.h"

struct GPUSubmission {
    RenderPass render_passes[RenderPass::ScenePassCount];
    RenderPass screen_render_pass;
};

struct RenderStages {
    
};

struct RenderSystem {
    virtual void extract_render_data();
};
