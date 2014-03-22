#pragma once
#ifndef _TEXTURE_H
#define _TEXTURE_H
#include "opengl.h"
#include "types.h"

#define TEX_RECTANGLE 1 /* Allows NPOT textures. Also: forces 'Nearest' filter */

typedef enum TextureFormat {
	TEX_BYTE=0,
	TEX_FLOAT,
	N_TEXTURE_FORMATS
} TextureFormat;

typedef struct Texture {
	/* Note: allocated size might be larger */
	int w, h;
	 /* 1=gray, 2=gray-alpha, 3=rgb, 4=rgba */
	int channels;
	TextureFormat fmt;
	/* Various pointers to data for easy pixel manipulation */
	union {
		int8 *s8;
		uint8 *u8;
		float *f32;
		uint32 *u32;
	} data;
	int flags;
	
	GLuint gl_tex_id; /* OpenGL texture name */
	GLenum gl_tex_target; /* GL_TEXTURE_2D or GL_TEXTURE_RECTANGLE */
	GLenum gl_tex_filter; /* GL_LINEAR or GL_NEAREST */
} Texture;

/* Description for alloc_texture() arguments:
	w: Minimum width of texture (padded to multiples of 4. Actual allocated width is stored in Texture.padded_w
	h: Minimum height of texture
	num_channels: Must be 1, 2, 3 or 4
	fmt: Must be either TEX_BYTE or TEX_FLOAT
	flags:
		TEX_RECTANGLE: Texture does not have to be power of 2
		TEX_NO_FILTER: Crisp texel edges (not blurred when magnified)
Notes:
	-Internal precision might be 8bit even if TEX_FLOAT is used (depends on hardware)
	-Does NOT upload texture to OpenGL
*/

Texture *alloc_texture( int w, int h, int num_channels, TextureFormat fmt, int flags );

/* Loads a texture from a BMP file. */
Texture *load_texture( const char filename[], int flags );

/* Does nothing if tex is NULL */
void delete_texture( Texture *tex );

/* Sends texture to OpenGL hardware and updates gl_tex_id.
Will upload the entire texture even if nothing has changed. */
void upload_texture( Texture *tex );

#endif
