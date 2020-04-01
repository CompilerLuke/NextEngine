#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "core/container/event_dispatcher.h"
#include "core/container/string_buffer.h"
#include <glm/vec2.hpp>
#include "core/core.h"

struct KeyData {
	int key;
	int scancode;
	int action; 
	int mods;
};

struct MouseButtonData {
	int button;
	int action;
	int mods;
};

struct Window {
	string_buffer title;
	bool vSync = true;
	bool full_screen = false;
	unsigned int width = 3840;
	unsigned int height = 2160;

	EventDispatcher<glm::vec2> on_resize;
	EventDispatcher<glm::vec2> on_cursor_pos;
	EventDispatcher<KeyData> on_key;
	EventDispatcher<string_buffer> on_drop;
	EventDispatcher<MouseButtonData> on_mouse_button;
	EventDispatcher<glm::vec2> on_scroll;

	struct GLFWwindow* window_ptr;

	ENGINE_API Window() {};
	void ENGINE_API init();
	bool ENGINE_API should_close();
	void ENGINE_API swap_buffers();
	void ENGINE_API poll_inputs();

	void ENGINE_API override_key_callback(GLFWkeyfun func);
	void ENGINE_API override_char_callback(GLFWcharfun func);
	void ENGINE_API override_mouse_button_callback(GLFWmousebuttonfun func);

	ENGINE_API ~Window();
};