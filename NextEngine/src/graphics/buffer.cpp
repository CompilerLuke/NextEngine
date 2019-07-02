#include "stdafx.h"
#include "graphics/buffer.h"

void VertexBuffer::bind() {
	glBindVertexArray(vao);
}

GLenum numberType_to_gl(NumberType type) {
	if (type == Float) return GL_FLOAT;
	else return GL_INT;
}

VertexBuffer::VertexBuffer(VertexBuffer&& other) {
	this->length = other.length;
	this->vao = other.vao;
	
	other.length = 0;
	other.vao = 0;
}

VertexBuffer::~VertexBuffer() {
	if (vao == 0) return;
	glDeleteVertexArrays(1, &vao); //todo clear buffers
}

