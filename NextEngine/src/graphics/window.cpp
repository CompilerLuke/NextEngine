#include "stdafx.h"
#include "graphics/window.h"
#include "logger/logger.h"

void framebuffer_size_callback(GLFWwindow* window_ptr, int width, int height) {
	auto window = (Window*)glfwGetWindowUserPointer(window_ptr);
	window->width = width;
	window->height = height;
	window->on_resize.broadcast(glm::vec2(width, height));
}

void cursor_pos_callback(GLFWwindow* window_ptr, double x, double y) {
	int width, height;
	glfwGetWindowSize(window_ptr, &width, &height);

	auto window = (Window*)glfwGetWindowUserPointer(window_ptr);
	window->on_cursor_pos.broadcast(glm::vec2(x, y + (window->height - height)));
}

void key_callback(GLFWwindow* window_ptr, int key, int scancode, int action, int mods) {
	auto window = (Window*)glfwGetWindowUserPointer(window_ptr);
	window->on_key.broadcast({ key, scancode, action, mods });
}

void mouse_button_callback(GLFWwindow* window_ptr, int button, int action, int mods) {
	auto window = (Window*)glfwGetWindowUserPointer(window_ptr);
	window->on_mouse_button.broadcast({ button, action, mods });
}

void drop_callback(GLFWwindow* window_ptr, int count, const char** paths) {
	auto window = (Window*)glfwGetWindowUserPointer(window_ptr);
	
	for (int i = 0; i < count; i++) {
		window->on_drop.broadcast(StringBuffer(paths[i]));
	}
}

void gl_error_callback(GLenum source, GLenum typ, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* ptr) {
	log(message);
}

void Window::init() {
	glfwInit();

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 4); 
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); 

	glfwWindowHint(GLFW_SAMPLES, 4); 

	auto c_title = title.c_str();

	if (full_screen) {
		auto monitor = glfwGetPrimaryMonitor();
		window_ptr = glfwCreateWindow(width, height, c_title, monitor, NULL);
	}
	else {
		window_ptr = glfwCreateWindow(width, height, c_title, NULL, NULL);
	}
	
	if (window_ptr == NULL) {
		glfwTerminate();
		throw "Failed to create window";
	}

	glfwMakeContextCurrent(window_ptr);
	glfwSetFramebufferSizeCallback(window_ptr, framebuffer_size_callback);
	glfwSetCursorPosCallback(window_ptr, cursor_pos_callback);
	glfwSetKeyCallback(window_ptr, key_callback);
	glfwSetInputMode(window_ptr, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	glfwSetDropCallback(window_ptr, drop_callback);
	glfwSetMouseButtonCallback(window_ptr, mouse_button_callback);

	glfwSetWindowUserPointer(window_ptr, this);

	if (!gladLoadGLLoader((GLADloadproc) glfwGetProcAddress)) {
		throw "Failed to initialize GLAD!";
	}

	//glEnable(GL_DEBUG_OUTPUT);
	//glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
	glDebugMessageCallback((GLDEBUGPROC)gl_error_callback, this); 

	if (vSync) {
		glfwSwapInterval(1);
	}
	else {
		glfwSwapInterval(0);
	}
}

bool Window::should_close() {
	return glfwWindowShouldClose(window_ptr);
}

void Window::swap_buffers() {
	glfwSwapBuffers(window_ptr);
}

void Window::poll_inputs() {
	glfwPollEvents();
}

Window::~Window() {
	glfwDestroyWindow(window_ptr);
	glfwTerminate();
}