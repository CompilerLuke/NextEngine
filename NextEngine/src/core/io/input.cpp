#include "stdafx.h"
#include "core/io/input.h"
#include "core/io/logger.h"
#include <GLFW/glfw3.h>

void on_cursor_pos(Input* self, glm::vec2 mouse_position) {
	if (!self->mouse_captured && (mouse_position.x > self->region_max.x || mouse_position.y > self->region_max.y || mouse_position.x < self->region_min.x || mouse_position.y < self->region_min.y)) {
		self->active = false;
		return;
	}
	
	self->active = true;

	mouse_position -= self->region_min;
	
	if (self->first_mouse) {
		self->mouse_position = mouse_position;
		self->first_mouse = false;
	}

	auto xpos = mouse_position.x;
	auto ypos = mouse_position.y;

	auto xoffset = xpos - self->mouse_position.x;
	auto yoffset = self->mouse_position.y - ypos;

	self->mouse_offset = glm::vec2(xoffset, yoffset);
	self->mouse_position = mouse_position;
}


void on_scroll(Input* self, glm::vec2 offset) {
	self->scroll_offset = offset.y;
}

void on_key(Input* self, KeyData& key_data) {
	if (!self->active) return;

	self->keys[key_data.key] = key_data.action;
}

void on_mouse_button(Input* self, MouseButtonData& data) {
	if (!self->active) return;

	int button;
	if (data.button == GLFW_MOUSE_BUTTON_LEFT) button = Left;
	else if (data.button == GLFW_MOUSE_BUTTON_RIGHT) button = Right;
	else if (data.button == GLFW_MOUSE_BUTTON_MIDDLE) button = Middle;
	else return;

	self->mouse_button_state[button] = data.action;
}

Input::Input() {}

void Input::init(Window& window) {
	window.on_cursor_pos.listen([this](glm::vec2 pos) { on_cursor_pos(this, pos); });
	window.on_key.listen([this](KeyData data) { on_key(this, data); });
	window.on_mouse_button.listen([this](MouseButtonData data) { on_mouse_button(this, data); });
	window.on_scroll.listen([this](glm::vec2 offset) { on_scroll(this, offset); });
	
	this->window_ptr = window.window_ptr;

	this->region_min = glm::vec2(0, 0);
	this->region_max = glm::vec2(window.width, window.height);
}

bool is_mod_down(Input* input) {
	return input->keys[GLFW_KEY_LEFT_CONTROL] != GLFW_RELEASE;
}

bool Input::key_down(Key key, bool allow_mod) {
	if (!allow_mod && is_mod_down(this))
		return false;

	auto state = this->keys[key];
	return state == GLFW_PRESS || state == GLFW_REPEAT;
}

bool Input::key_pressed(Key key, bool allow_mod) {
	if (!allow_mod && is_mod_down(this)) return false;

	return this->keys[key] == GLFW_PRESS;
}

bool Input::mouse_button_down(MouseButton button) {
	auto state = this->mouse_button_state[button];
	return state == GLFW_PRESS || state == GLFW_REPEAT;
}

bool Input::mouse_button_pressed(MouseButton button) {
	auto state = this->mouse_button_state[button];
	return state == GLFW_PRESS;
}

void Input::capture_mouse(bool capture) {
	glfwSetInputMode(window_ptr, GLFW_CURSOR, capture ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
	mouse_captured = capture;
}

float Input::get_vertical_axis() {
	if (key_down(GLFW_KEY_W)) return 1;
	if (key_down(GLFW_KEY_S)) return -1;
	return 0;
}

float Input::get_horizontal_axis() {
	if (key_down(GLFW_KEY_D)) return 1;
	if (key_down(GLFW_KEY_A)) return -1;
	return 0;
}

void Input::clear() {
	mouse_offset = glm::vec2(0);
	scroll_offset = 0.0f;

	if (!active) {
		this->keys.clear();
		return;
	}

	for (auto pair : this->keys) {
		if (pair.value == GLFW_PRESS) {
			this->keys[pair.key] = GLFW_REPEAT; 
		}
	}

	for (unsigned int i = 0; i < 3; i++) {
		if (this->mouse_button_state[i] == GLFW_PRESS) {
			this->mouse_button_state[i] = GLFW_REPEAT;
		}
	}
}