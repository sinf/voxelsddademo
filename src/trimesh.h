#pragma once
#ifndef _TRIMESH_H
#define _TRIMESH_H
#include "opengl.h"
#include "vector.h"
#include "texture.h"

typedef struct Vertex
{
	vec3f pos;
	vec2f uv[2];
} Vertex;

#define MAX_MESH_VERTS 8192
typedef struct Trimesh
{
	int num_verts;
	Vertex vertices[MAX_MESH_VERTS];
	GLuint vbo;
	Texture *tex;
} Trimesh;

/* Loads a wavefront OBJ mesh. Does not load textures or materials.
	Only 3D triangles WITH texture coordinates are supported.
	All unsupported features are ignored. */
Trimesh *load_wavefront_obj( const char filename[] );

/* Multiply vertex coordinates by scale */
void scale_mesh( Trimesh *m, double sx, double sy, double sz );

/* Creates vbo if its 0. Then binds the vbo. Unbinds any VBO's if m is NULL */
void select_mesh( Trimesh *m );

/* Draws the previously bound mesh (remembers previous vbo) */
void draw_mesh( void );

#endif
