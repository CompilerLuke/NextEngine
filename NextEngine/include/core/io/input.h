#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include "core/container/hash_map.h"
#include "graphics/rhi/window.h"

using Key = int;

enum MouseButton { Middle = 0, Right = 1, Left = 2 };
enum KeyAction { Pressed = 1, Released = 0};

struct Input {
	bool active = true;

	glm::vec2 region_min;
	glm::vec2 region_max;
	
	bool first_mouse = true;

	glm::vec2 mouse_position;
	glm::vec2 mouse_offset;

	float scroll_offset = 0;

	hash_map<int, int, 103> keys;

	bool mouse_captured = false;

	int mouse_button_state[3] = { 0, 0, 0 };

	bool ENGINE_API key_down(Key, bool allow_mod = false);
	bool ENGINE_API key_pressed(Key, bool allow_mod = false);

	bool ENGINE_API mouse_button_down(MouseButton);
	bool ENGINE_API mouse_button_pressed(MouseButton);

	float ENGINE_API get_vertical_axis();
	float ENGINE_API get_horizontal_axis();

	struct GLFWwindow* window_ptr;
	void ENGINE_API capture_mouse(bool);
	void ENGINE_API clear();

	ENGINE_API Input();
	void ENGINE_API init(Window&);
};