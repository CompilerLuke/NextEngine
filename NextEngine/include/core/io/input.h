#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include "core/container/hash_map.h"
#include "graphics/rhi/window.h"

using Key = int;

enum MouseButton { Middle = 0, Right = 1, Left = 2 };
enum KeyAction { Pressed = 1, Released = 0};

struct ENGINE_API Input {
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

	bool key_down(Key, bool allow_mod = false);
	bool key_pressed(Key, bool allow_mod = false);

	bool mouse_button_down(MouseButton);
	bool mouse_button_pressed(MouseButton);

	float get_vertical_axis();
	float get_horizontal_axis();

	struct GLFWwindow* window_ptr;
	void capture_mouse(bool);
	void clear();

	Input();
	void init(Window&);
};