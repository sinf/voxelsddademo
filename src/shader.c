#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include "opengl.h"
#include "shader.h"

static int get_status( GLuint object, GLenum mode )
{
	GLint status;
	int ret;
	
	if ( glIsShader(object) )
	{
		glGetShaderiv( object, mode, &status );
		switch( mode )
		{
			case GL_DELETE_STATUS:
			case GL_COMPILE_STATUS:
				ret = ( status == GL_TRUE );
				break;
			default:
				ret = status;
				break;
		}
	}
	else
	{
		glGetProgramiv( object, mode, &status );
		switch( mode )
		{
			case GL_DELETE_STATUS:
			case GL_LINK_STATUS:
			case GL_VALIDATE_STATUS:
				ret = ( status == GL_TRUE );
				break;
			default:
				ret = status;
				break;
		}
	}
	
	return ret;
}

static const char *get_info_log( GLuint object )
{
	static char buf[2048];
	GLsizei len;
	
	if ( glIsShader( object ) )
		glGetShaderInfoLog( object, sizeof(buf), &len, buf );
	else
		glGetProgramInfoLog( object, sizeof(buf), &len, buf );
	
	return buf;
}

GLuint compile_shader( const char filename[], GLenum shader_type )
{
	char buf[16384];
	int filesize;
	FILE *file;
	GLuint shader;
	
	printf( "Loading shader... %s\n", filename );
	
	file = fopen( filename, "r" );
	if ( !file )
	{
		printf( "Error: failed to open file: %s\n", strerror(errno) );
		abort();
	}
	
	/* Read the whole file */
	filesize = fread( buf, 1, sizeof(buf) - 1, file );
	buf[filesize] = '\0';	
	fclose( file );
	
	/* Compile */
	shader = glCreateShader( shader_type );
	{
		const char *buf_p[1];
		buf_p[0] = buf;
		glShaderSource( shader, 1, buf_p, &filesize );
	}
	glCompileShader( shader );
	
	/* Error report */
	printf( "%s", get_info_log( shader ) );
	if ( get_status( shader, GL_COMPILE_STATUS ) == 0 )
	{
		printf( "Failed to compile shader\n" );
		abort();
	}
	
	return shader;
}

GLuint create_shader_program( GLuint vs, GLuint fs )
{
	GLuint prog = glCreateProgram();
	
	if ( vs )
	{
		if ( get_status( vs, GL_SHADER_TYPE ) != GL_VERTEX_SHADER )
		{
			printf( "Error: %s: parameter #1 is not a VERTEX shader\n", __func__ );
			abort();
		}
		glAttachShader( prog, vs );
	}
	
	if ( fs )
	{
		if ( get_status( fs, GL_SHADER_TYPE ) != GL_FRAGMENT_SHADER )
		{
			printf( "Error: %s: parameter #2 is not a FRAGMENT shader\n", __func__ );
			abort();
		}
		glAttachShader( prog, fs );
	}
	
	glLinkProgram( prog );
	
	#if 0
	glValidateProgram( prog );
	#endif
	
	/* Error report */
	printf( "%s", get_info_log( prog ) );
	if ( !get_status( prog, GL_LINK_STATUS ) )
		abort();
	
	/**
	if ( !get_status( prog, GL_VALIDATE_STATUS ) )
		abort();
	*/
	
	return prog;
}
