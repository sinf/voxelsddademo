#include <stdio.h>
#include <stdarg.h>
#include <GL/gl.h>
#include "text.h"

void draw_text( int x0, int y, const char text[] )
{
	GLint verts[4*2];
	GLfloat tex_coords[4*2];
	const char *c;
	int x = x0;
	
	glEnableClientState( GL_VERTEX_ARRAY );
	glVertexPointer( 2, GL_INT, 0, verts );
	
	glEnableClientState( GL_TEXTURE_COORD_ARRAY );
	glTexCoordPointer( 2, GL_FLOAT, 0, tex_coords );
	
	for( c=text; ( *c != '\0' ); c++ )
	{	
		if ( *c == '\n' )
		{
			y += GLYPH_H;
			x = x0;
			continue;
		}
		
		if ( *c != ' ' )
		{
			const double w = 1.0 / 94.0;
			int index = *c - 33;
		
			if ( index < 0 || index > 94 )
				continue;
			
			verts[0] = verts[6] = x;
			verts[1] = verts[3] = y;
			verts[4] = verts[2] = x + GLYPH_W;
			verts[7] = verts[5] = y + GLYPH_H;
			
			tex_coords[0] = tex_coords[6] = index * w;
			tex_coords[1] = tex_coords[3] = 0.0f;
			tex_coords[4] = tex_coords[2] = tex_coords[0] + w;
			tex_coords[7] = tex_coords[5] = 1.0f;
			
			glDrawArrays( GL_QUADS, 0, 4 );
		}
		
		x += GLYPH_W;
	}
	
	glDisableClientState( GL_VERTEX_ARRAY );
	glDisableClientState( GL_TEXTURE_COORD_ARRAY );
}

void draw_text_f( int x, int y, const char fmt[], ... )
{
	char buf[512];
	va_list args;
	
	va_start( args, fmt );
	vsnprintf( buf, sizeof(buf), fmt, args );
	va_end( args );
	
	draw_text( x, y, buf );
}
