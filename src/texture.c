#include <emmintrin.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "opengl.h"
#include "texture.h"
#include "loadbmp/loadbmp.h"

Texture *alloc_texture( int w, int h, int num_channels, TextureFormat fmt, int flags )
{
	Texture *tex = calloc( 1, sizeof(Texture) );
	int channel_size = 0;
	
	if ( num_channels < 1 )
	{
		printf( "%s: Error: num_channels < 1\n", __func__ );
		abort();
	}
	
	switch( fmt )
	{
		case TEX_BYTE:
			channel_size = 1;
			break;
		
		case TEX_FLOAT:
			channel_size = sizeof(float);
			break;
		
		default:
			printf( "%s: Error: invalid texture format\n", __func__ );
			abort();
	}
	
	if ( w < 1 || h < 1 )
	{
		printf( "%s: Error: invalid texture dimensions (%dx%d)\n", __func__, w, h );
		abort();
	}
	
	tex->w = w;
	tex->h = h;
	tex->channels = num_channels;
	tex->fmt = fmt;
	tex->data.u8 = aligned_alloc( sizeof( __m128 ), w * h * num_channels * channel_size );
	tex->flags = flags;
	
	tex->gl_tex_id = 0;
	tex->gl_tex_target = GL_TEXTURE_2D;
	tex->gl_tex_filter = GL_LINEAR;
	
	if ( flags & TEX_RECTANGLE )
	{
		if ( !GLEE_ARB_texture_rectangle )
		{
			printf( "Error: Unsupported OpenGL extension: ARB_texture_rectangle\n" );
			abort();
		}
		
		tex->gl_tex_target = GL_TEXTURE_RECTANGLE_ARB;
		tex->gl_tex_filter = GL_NEAREST;
	}
	
	return tex;
}

Texture *load_texture( const char filename[], int flags )
{
	Texture *tex;
	int y;
	BMP_ERROR_CODE e;
	uint32 *bmp_data;
	int w, h;
	
	printf( "Loading texture... %s\n", filename );
	e = load_bmp( filename, &bmp_data, &w, &h, 0 );
	
	if ( e != BMP_SUCCESS )
	{
		printf( "Failed to load BMP: %s\n", BMP_ERROR_MESSAGES[e] );
		abort();
	}
	
	/* Note: might be bad if the image is NPOT */
	tex = alloc_texture( w, h, 4, TEX_BYTE, flags );
	
	for( y=0; y<h; y++ )
	{
		void *src = bmp_data + y * w;
		void *dst = tex->data.u32 + y * tex->w;
		
		memcpy( dst, src, 4 * w );
	}
	
	free( bmp_data );
	return tex;
}

void delete_texture( Texture *tex )
{
	if ( tex )
	{
		glDeleteTextures( 1, &tex->gl_tex_id );
		free( tex->data.u8 );
		free( tex );
	}
}

/* The number of color channels should be used as an index to these tables */
static const GLenum INTERNAL_FORMAT_TABLE8[] = {
	0,
	GL_LUMINANCE8,
	GL_LUMINANCE8_ALPHA8,
	GL_RGBA8,
	GL_RGBA8
};
static const GLenum INTERNAL_FORMAT_TABLE32F[] = {
	0,
	GL_LUMINANCE32F_ARB,
	GL_LUMINANCE_ALPHA32F_ARB,
	GL_RGB32F_ARB,
	GL_RGBA32F_ARB
};
static const GLenum FORMAT_TABLE[] = {
	0,
	GL_LUMINANCE,
	GL_LUMINANCE_ALPHA,
	GL_BGR,
	GL_BGRA
};

void upload_texture( Texture *tex )
{
	GLenum internal_format, data_type, format;
	GLenum tex_target;
	GLuint tex_id;
	GLenum error;
	
	if ( !tex )
		return;
	
	tex_target = tex->gl_tex_target;
	tex_id = tex->gl_tex_id;
	format = FORMAT_TABLE[ tex->channels ];
	
	switch( tex->fmt )
	{
		case TEX_BYTE:
			internal_format = INTERNAL_FORMAT_TABLE8[ tex->channels ];
			data_type = GL_UNSIGNED_BYTE;
			break;
		
		case TEX_FLOAT:
			/* Use the extension to get high precision (if available) */
			if ( GLEE_ARB_texture_float )
				internal_format = INTERNAL_FORMAT_TABLE32F[ tex->channels ];
			else
				internal_format = INTERNAL_FORMAT_TABLE8[ tex->channels ];
			data_type = GL_FLOAT;
			break;
			
		default:
			printf( "%s: Invalid texture format (should not happen)\n", __func__ );
			abort();
	}
	
	/* Clear the error flag */
	error = glGetError();
	
	glPixelStorei( GL_UNPACK_SWAP_BYTES, GL_FALSE );
	glPixelStorei( GL_UNPACK_LSB_FIRST, GL_TRUE );
	glPixelStorei( GL_UNPACK_ROW_LENGTH, 0 );
	glPixelStorei( GL_UNPACK_SKIP_PIXELS, 0 );
	
	if ( tex->fmt == TEX_FLOAT || tex->channels == 4 )
		glPixelStorei( GL_UNPACK_ALIGNMENT, 4 );
	else
		glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
	
	if ( tex_id == 0 )
	{
		/* Create new texture */
		glGenTextures( 1, &tex_id );
		glBindTexture( tex_target, tex_id );
		glTexImage2D( tex_target, 0, internal_format, tex->w, tex->h, 0, format, data_type, tex->data.u8 );
	}
	else
	{
		/* Update old data */
		glBindTexture( tex_target, tex_id );
		glTexSubImage2D( tex_target, 0, 0, 0, tex->w, tex->h, format, data_type, tex->data.u8 );
	}
	
	glTexParameteri( tex_target, GL_TEXTURE_MIN_FILTER, tex->gl_tex_filter );
	glTexParameteri( tex_target, GL_TEXTURE_MAG_FILTER, tex->gl_tex_filter );
	
	error = glGetError();
	if ( error != GL_NO_ERROR )
	{
		printf( "Failed to upload texture: %s\n", gluErrorString(error) );
		abort();
	}
	
	glBindTexture( tex_target, 0 );
	tex->gl_tex_id = tex_id;
}
