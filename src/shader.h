#pragma once
#ifndef _SHADER_H
#define _SHADER_H
#include "opengl.h"

/* Reads code from file, creates and compiles the shader.
	'shader_type' must be either GL_FRAGMENT_SHADER or GL_VERTEX_SHADER */
GLuint compile_shader( const char filename[], GLenum shader_type );

/* Combines vertex and fragment shaders to a single shader program.
	vs MUST be a vertex shader
	fs MUST be a fragment shader */
GLuint create_shader_program( GLuint vs, GLuint fs );

#endif
