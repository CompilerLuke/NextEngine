#include "stdafx.h"
#include "graphics/device.h"
#include <glad/glad.h>

void Device::bind() {
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_MULTISAMPLE);
	
	glClearColor(clear_colour.r, clear_colour.g, clear_colour.b, clear_colour.a);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT  | GL_STENCIL_BUFFER_BIT);

	glViewport(0, 0, width, height);
}