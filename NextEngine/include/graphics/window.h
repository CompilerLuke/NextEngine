#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <string>
#include "core/eventDispatcher.h"
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
	std::string title = "";
	bool vSync = true;
	bool full_screen = false;
	unsigned int width = 3840;
	unsigned int height = 2160;

	EventDispatcher<glm::vec2> on_resize;
	EventDispatcher<glm::vec2> on_cursor_pos;
	EventDispatcher<KeyData> on_key;
	EventDispatcher<std::string> on_drop;
	EventDispatcher<MouseButtonData> on_mouse_button;

	GLFWwindow* window_ptr;

	ENGINE_API Window() {}
	void ENGINE_API init();
	bool ENGINE_API should_close();
	void ENGINE_API swap_buffers();
	void ENGINE_API poll_inputs();

	ENGINE_API ~Window();
};