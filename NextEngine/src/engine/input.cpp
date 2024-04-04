#include "engine/input.h"
#include "core/io/logger.h"
#include "graphics/rhi/window.h"
#include "core/time.h"
#include <GLFW/glfw3.h>

void on_cursor_pos(Input* self, glm::vec2 screen_mouse_position) {
    glm::vec2 past_screen_position = self->screen_mouse_position;
    self->screen_mouse_position = screen_mouse_position;
    
	if (!self->mouse_captured && (screen_mouse_position.x > self->region_max.x || screen_mouse_position.y > self->region_max.y || screen_mouse_position.x < self->region_min.x || screen_mouse_position.y < self->region_min.y)) {
		self->active = false;
		return;
	}
	
	glm::vec2 mouse_position = screen_mouse_position - self->region_min;
	
	if (self->first_mouse || !self->active) {
        self->screen_mouse_position = screen_mouse_position;
		self->first_mouse = false;
        if (self->mouse_based_active) {
            self->active = true;
        }
	}

	auto xoffset = screen_mouse_position.x - past_screen_position.x;
	auto yoffset = past_screen_position.y - screen_mouse_position.y;

	self->mouse_offset = glm::vec2(xoffset, yoffset);
	self->mouse_position = mouse_position;
    self->screen_mouse_position = screen_mouse_position;
    
    
}


void on_scroll(Input* self, glm::vec2 offset) {
	self->scroll_offset = offset.y;
}

bool is_mod_down(Input* input, ModKeys mask) {
    bool result =
		(input->keys[GLFW_KEY_LEFT_CONTROL] != GLFW_RELEASE <= (bool)(mask & ModKeys::Control))
		&& (input->keys[GLFW_KEY_LEFT_SHIFT] != GLFW_RELEASE <= (bool)(mask & ModKeys::Shift))
		&& (input->keys[GLFW_KEY_LEFT_ALT] != GLFW_RELEASE <= (bool)(mask & ModKeys::Alt));
    return result;
}

void on_key(Input* self, KeyData& key_data) {
	if (!self->active) return;

    int c = key_data.key;
	self->keys[c] = key_data.action;
}

void on_char(Input* self, uint key) {
    self->last_key = key;
}

void on_mouse_button(Input* self, MouseButtonData& data) {
	if (!self->active) return;

	MouseButton button;
	if (data.button == GLFW_MOUSE_BUTTON_LEFT) button = MouseButton::Left;
	else if (data.button == GLFW_MOUSE_BUTTON_RIGHT) button = MouseButton::Right;
	else if (data.button == GLFW_MOUSE_BUTTON_MIDDLE) button = MouseButton::Middle;
	else return;

	self->mouse_button_state[(int)button] = data.action;
}

Input::Input() {}

void Input::init(Window& window) {
	window.on_cursor_pos.listen([this](glm::vec2 pos) { on_cursor_pos(this, pos); });
	window.on_key.listen([this](KeyData data) { on_key(this, data); });
    window.on_char.listen([this](uint key) { on_char(this, key); });
	window.on_mouse_button.listen([this](MouseButtonData data) { on_mouse_button(this, data); });
	window.on_scroll.listen([this](glm::vec2 offset) { on_scroll(this, offset); });
	
	this->window_ptr = window.window_ptr;

	//this->region_min = glm::vec2(0, 0);
	//this->region_max = glm::vec2(window.width, window.height);
}

bool is_mod_key(Key key) {
	switch (key) {
    case Key::Left_Super: return true;
	case Key::Left_Control: return true;
	case Key::Left_Shift: return true;
	case Key::Left_Alt: return true;
	default: return false;
	}
}

bool Input::key_down(Key key, ModKeys allow_mod) {
	if (!is_mod_key(key) && !is_mod_down(this, allow_mod))
		return false;

	auto state = this->keys[(int)key];
	return state == GLFW_PRESS || state == GLFW_REPEAT;
}

bool Input::key_down_timeout(Key key, float timeout, ModKeys allow_mod) {
    bool pressed = key_pressed(key, allow_mod);
    return pressed;
    /*if (pressed) return true;
    
    double t = last_pressed[(uint)key];
    pressed = t - Time::now() > timeout;
    
//if (pressed)
    return pressed;*/
}

bool Input::key_mod_pressed(Key key, Key mod) {
    bool mod_down;
    if (mod == Key::Control) mod_down = key_down(Key::Left_Control) || key_down(Key::Right_Control) || key_down(Key::Left_Super) || key_down(Key::Right_Super);
    else mod_down = key_down(mod);
    
    return mod_down && keys[(int)key] == GLFW_PRESS;
}

bool Input::key_pressed(Key key, ModKeys allow_mod) {
	if (!is_mod_down(this, allow_mod)) return false;

	return keys[(uint)key] == GLFW_PRESS;
}

bool Input::mouse_button_down(MouseButton button) {
	auto state = this->mouse_button_state[(int)button];
	return state == GLFW_PRESS || state == GLFW_REPEAT;
}

bool Input::mouse_button_pressed(MouseButton button) {
	auto state = this->mouse_button_state[(int)button];
	return state == GLFW_PRESS;
}

void Input::capture_mouse(bool capture) {
    if (mouse_captured == capture) return;
	glfwSetInputMode(window_ptr, GLFW_CURSOR, capture ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
	mouse_captured = capture;
    double xpos, ypos;
    glfwGetCursorPos(window_ptr, &xpos, &ypos);
    screen_mouse_position.x = xpos;
    screen_mouse_position.y = ypos;
}

float Input::get_vertical_axis() {
	if (key_down(Key::W, ModKeys::Shift)) return 1;
	if (key_down(Key::S, ModKeys::Shift)) return -1;
	return 0;
}

float Input::get_horizontal_axis() {
	if (key_down(Key::D, ModKeys::Shift)) return 1;
	if (key_down(Key::A, ModKeys::Shift)) return -1;
	return 0;
}

void Input::clear() {
	mouse_offset = glm::vec2(0);
	scroll_offset = 0.0f;
    last_key = 0;

	if (!active) {
        for (uint i = 0; i < (uint)Key::Last; i++) keys[i] = 0;
		return;
	}

    for (uint i = 0; i < (uint)Key::Last; i++) {
		if (keys[i] == GLFW_PRESS) keys[i] = GLFW_REPEAT;
	}

	for (unsigned int i = 0; i < 3; i++) {
		if (this->mouse_button_state[i] == GLFW_PRESS) {
			this->mouse_button_state[i] = GLFW_REPEAT;
		}
	}
}
