#pragma once

#include "engine/core.h"
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include "core/container/hash_map.h"

enum class MouseButton { Middle = 0, Right = 1, Left = 2 };
enum class KeyAction { Pressed = 1, Released = 0};

enum class Key {
	Space = 32,
	Apostrophe = 39,
	Comma = 44,
	Minus = 45,
	Period = 46,
	Slash = 47,
	Num_0 = 48,
	Num_1 = 49,
	Num_2 = 50,
	Num_3 = 51,
	Num_4 = 52,
	Num_5 = 53,
	Num_6 = 54,
	Num_7 = 55,
	Num_8 = 56,
	Num_9 = 57,
	Semicolon = 59,
	Equal = 61,
	A = 65,
	B = 66,
	C = 67,
	D = 68,
	E = 69,
	F = 70,
	G = 71,
	H = 72,
	I = 73,
	J = 74,
	K = 75,
	L = 76,
	M = 77,
	N = 78,
	O = 79,
	P = 80,
	Q = 81,
	R = 82,
	S = 83,
	T = 84,
	U = 85,
	V = 86,
	W = 87,
	X = 88,
	Y = 89,
	Z = 90,
	Left_Bracket = 91,
	Backslash = 92,
	Right_Bracket = 93,
	Grave_Accent = 96,
	World_1 = 161,
	World_2 = 162,
	Escape = 256,
	Enter = 257,
	Tab = 258,
	Backspace = 259,
	Insert = 260,
	Delete = 261,
	Right = 262,
	Left = 263,
	Down = 264,
	Up = 265,
	Page_Up = 266,
	Page_Down = 267,
	Home = 268,
	End = 269,
	Caps_Lock = 280,
	Scroll_Lock = 281,
	Num_Lock = 282,
	Print_Screen = 283,
	Pause = 284,
	F1 = 290,
	F2 = 291,
	F3 = 292,
	F4 = 293,
	F5 = 294,
	F6 = 295,
	F7 = 296,
	F8 = 297,
	F9 = 298,
	F10 = 299,
	F11 = 300,
	F12 = 301,
	F13 = 302,
	F14 = 303,
	F15 = 304,
	F16 = 305,
	F17 = 306,
	F18 = 307,
	F19 = 308,
	F20 = 309,
	F21 = 310,
	F22 = 311,
	F23 = 312,
	F24 = 313,
	F25 = 314,
	Kp_0 = 320,
	Kp_1 = 321,
	Kp_2 = 322,
	Kp_3 = 323,
	Kp_4 = 324,
	Kp_5 = 325,
	Kp_6 = 326,
	Kp_7 = 327,
	Kp_8 = 328,
	Kp_9 = 329,
	Kp_Decimal = 330,
	Kp_Divide = 331,
	Kp_Multiply = 332,
	Kp_Subtract = 333,
	Kp_Add = 334,
	Kp_Enter = 335,
	Kp_Equal = 336,
	Left_Shift = 340,
	Left_Control = 341,
	Left_Alt = 342,
	Left_Super = 343,
	Right_Shift = 344,
	Right_Control = 345,
	Right_Alt = 346,
	Right_Super = 347,
	Menu = 348,
    Control = 3, //Special Mod Key (Left_Control, Right_Control, Right_Super, Left_Super)
	Last = Menu,
};

enum class ModKeys {
	None = 0,
	Shift = 1 << 0,
	Control = 1 << 1,
	Alt = 1 << 2,
};

inline ModKeys operator|(ModKeys a, ModKeys b) { return (ModKeys)((uint)a | (uint)b); }
inline uint operator&(ModKeys a, ModKeys b) { return ((uint)a & (uint)b); }

struct Input {
	bool active = true;
    bool mouse_based_active = true;

	glm::vec2 region_min = glm::vec2(0,0);
	glm::vec2 region_max = glm::vec2(FLT_MAX,FLT_MAX);
	
	bool first_mouse = true;

    glm::vec2 screen_mouse_position;
	glm::vec2 mouse_position;
	glm::vec2 mouse_offset;

	float scroll_offset = 0;
    int last_key;

    double last_pressed[(uint)Key::Last] = {};
    int keys[(uint)Key::Last] = {};

	bool mouse_captured = false;

	int mouse_button_state[3] = { 0, 0, 0 };

	bool ENGINE_API key_down(Key, ModKeys allow_mod = ModKeys::None);
	bool ENGINE_API key_pressed(Key, ModKeys allow_mod = ModKeys::None);
    bool ENGINE_API key_down_timeout(Key, float timeout, ModKeys allow_mod = ModKeys::None);
	bool ENGINE_API key_mod_pressed(Key key, Key mod = Key::Control);

	bool ENGINE_API mouse_button_down(MouseButton button = MouseButton::Left);
	bool ENGINE_API mouse_button_pressed(MouseButton button = MouseButton::Left);

	float ENGINE_API get_vertical_axis();
	float ENGINE_API get_horizontal_axis();

	struct GLFWwindow* window_ptr;
	void ENGINE_API capture_mouse(bool);
	void ENGINE_API clear();

	ENGINE_API Input();
	void ENGINE_API init(struct Window&);
};
