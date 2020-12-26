#pragma once


#include "core/container/event_dispatcher.h"
#include "core/container/string_buffer.h"
#include <glm/vec2.hpp>
#include "engine/core.h"

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

enum class CursorShape {
    Arrow, IBeam, Crosshair, Hand, HResize, VResize
};

struct GLFWwindow;
struct GLFWcursor;
typedef void(*keyfun) (GLFWwindow *, int, int, int, int);
typedef void(*charfun) (GLFWwindow *, unsigned int);
typedef void(*mousebuttonfun) (GLFWwindow *, int, int, int);

struct Window {
	string_buffer title;
	bool vSync = true;
	bool full_screen = false;
	unsigned int width = 3840;
	unsigned int height = 2160;
    CursorShape cursor_shape = CursorShape::Arrow;

	EventDispatcher<glm::vec2> on_resize;
	EventDispatcher<glm::vec2> on_cursor_pos;
	EventDispatcher<KeyData> on_key;
	EventDispatcher<string_buffer> on_drop;
	EventDispatcher<MouseButtonData> on_mouse_button;
	EventDispatcher<glm::vec2> on_scroll;

	GLFWwindow* window_ptr;
    GLFWcursor* current_cursor = nullptr;

	Window() {};
	~Window();

	void ENGINE_API init();
	bool ENGINE_API should_close();
	void ENGINE_API swap_buffers();
	void ENGINE_API poll_inputs();
	void ENGINE_API override_key_callback(keyfun func);
	void ENGINE_API override_char_callback(charfun func);
	void ENGINE_API override_mouse_button_callback(mousebuttonfun func);
	void ENGINE_API wait_events();
	void ENGINE_API get_framebuffer_size(int* width, int* height);
    void ENGINE_API get_window_size(int* width, int* height);
    void ENGINE_API get_dpi(int* horizontal, int* vertical);
    void ENGINE_API set_cursor(CursorShape shape);
    bool ENGINE_API is_visible();

	ENGINE_API void* get_native_window();

	static ENGINE_API Window* from(GLFWwindow*);
};
