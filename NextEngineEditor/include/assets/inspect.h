#pragma once

#include "graphics/assets/material.h"

struct ImFont;
struct sstring;
struct AssetTab;

void render_name(sstring& name, ImFont* font);
void asset_properties(AssetNode& node, Editor& editor, AssetTab& tab);
MaterialDesc base_shader_desc(shader_handle shader);

bool edit_color(glm::vec3& color, string_view name, glm::vec2 size = glm::vec2(200, 200));
bool edit_color(glm::vec4& color, string_view name, glm::vec2 size = glm::vec2(200, 200));

