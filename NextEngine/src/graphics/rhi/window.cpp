#include "graphics/rhi/window.h"
#include "core/io/logger.h"

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

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
    // + (window->height - height)
    // todo account for title bar
	window->on_cursor_pos.broadcast(glm::vec2(x, y));
}

void key_callback(GLFWwindow* window_ptr, int key, int scancode, int action, int mods) {
	auto window = (Window*)glfwGetWindowUserPointer(window_ptr);
	window->on_key.broadcast({ key, scancode, action, mods });
}

void char_callback(GLFWwindow* window_ptr, uint c) {
    auto window = (Window*)glfwGetWindowUserPointer(window_ptr);
    window->on_char.broadcast(c);
}

void mouse_button_callback(GLFWwindow* window_ptr, int button, int action, int mods) {
	auto window = (Window*)glfwGetWindowUserPointer(window_ptr);
	window->on_mouse_button.broadcast({ button, action, mods });
}

void drop_callback(GLFWwindow* window_ptr, int count, const char** paths) {
	auto window = (Window*)glfwGetWindowUserPointer(window_ptr);
	
	for (int i = 0; i < count; i++) {
		window->on_drop.broadcast(string_buffer(paths[i]));
	}
}

void scroll_callback(GLFWwindow* window_ptr, double xoffset, double yoffset)
{
	auto window = (Window*)glfwGetWindowUserPointer(window_ptr);

	window->on_scroll.broadcast(glm::vec2(xoffset, yoffset));
}

#ifdef RENDER_API_OPENGL
//void gl_error_callback(GLenum source, GLenum typ, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* ptr) {
//	log(message);
//}
#endif

void Window::init() {
	glfwInit();

#ifdef RENDER_API_OPENGL
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 4); 
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); 
#elif RENDER_API_VULKAN
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
#endif

	if (borderless) {
		glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
	}
	
	//glfwWindowHint(GLFW_SAMPLES, 4); 
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
    glfwSetCharCallback(window_ptr, char_callback);
	glfwSetInputMode(window_ptr, GLFW_CURSOR, GLFW_CURSOR);
	glfwSetDropCallback(window_ptr, drop_callback);
	glfwSetMouseButtonCallback(window_ptr, mouse_button_callback);
	glfwSetScrollCallback(window_ptr, scroll_callback);

	glfwSetWindowUserPointer(window_ptr, this);

#ifdef RENDER_API_OPENGL
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
		throw "Failed to initialize GLAD!";
	}

	//glEnable(GL_DEBUG_OUTPUT);
	glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
	glDebugMessageCallback((GLDEBUGPROC)gl_error_callback, this); 
#endif 

	if (vSync) {
		glfwSwapInterval(1);
	}
	else {
		glfwSwapInterval(0);
	}
}

void* Window::get_native_window() {
#ifdef NE_PLATFORM_WINDOWS
	return (void*)glfwGetWin32Window(window_ptr);
#elif NE_PLATFORM_MACOSX
    return (void*)glfwGetCocoaWindow(window_ptr);
#endif
}

void Window::wait_events() {
	glfwWaitEvents();
}

void Window::get_framebuffer_size(int* width, int* height) {
	glfwGetFramebufferSize(window_ptr, width, height);
}

void Window::get_window_size(int* width, int* height) {
    glfwGetWindowSize(window_ptr, width, height);
}

void Window::get_dpi(int* horizontal, int* vertical) {
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    
    int width_mm, height_mm;
    glfwGetMonitorPhysicalSize(monitor, &width_mm, &height_mm);
    
    float xscale, yscale;
    glfwGetMonitorContentScale(monitor, &xscale, &yscale);
    
    const GLFWvidmode * mode = glfwGetVideoMode(monitor);
    
    float width_px = mode->width;
    float height_px = mode->height;
    
    float width_i = width_mm / 25.4;
    float height_i = height_mm / 25.4;
    
    *horizontal = roundf(width_px  * xscale / width_i);
    *vertical = roundf(height_px * yscale / height_i);
}

void Window::set_cursor(CursorShape shape) {
    if (shape == cursor_shape) return;
    
    //todo implement caching if this is slow
    
    int cursor_shapes[] = {GLFW_ARROW_CURSOR, GLFW_IBEAM_CURSOR, GLFW_CROSSHAIR_CURSOR, GLFW_HAND_CURSOR, GLFW_HRESIZE_CURSOR, GLFW_VRESIZE_CURSOR};
    
    if (current_cursor) glfwDestroyCursor(current_cursor);
    
    current_cursor = glfwCreateStandardCursor(cursor_shapes[(uint)shape]);
    glfwSetCursor(window_ptr, current_cursor);
    
    cursor_shape = shape;
}

bool Window::is_visible() {
    return glfwGetWindowAttrib(window_ptr, GLFW_FOCUSED);
}

void Window::override_key_callback(GLFWkeyfun func) {
	glfwSetKeyCallback(window_ptr, func);
}

void Window::override_char_callback(GLFWcharfun func) {
	glfwSetCharCallback(window_ptr, func);
}

void Window::override_mouse_button_callback(GLFWmousebuttonfun func) {
	glfwSetMouseButtonCallback(window_ptr, func);
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

Window* Window::from(GLFWwindow* ptr) {
	return (Window*)glfwGetWindowUserPointer(ptr);
}
