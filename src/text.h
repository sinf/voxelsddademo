#pragma once
#ifndef _TEXT_H
#define _TEXT_H

#ifndef __GNUC__
#define __attribute__(x)
#endif

/* -- Glyph size --
	le_font.bmp: 21.787234042553191 x 32
	small_font.bmp: 10.893617021276595 x 16
	
	11x16
*/

#define GLYPH_W 7
#define GLYPH_H 10

/* Draws a block of text.
	Note: font texture must be loaded and bound to currently active texture unit
	(OpenGL default: GL_TEXTURE0) */
void draw_text( int x, int y, const char text[] );

/* Draws formatted text - printf style */
void draw_text_f( int x, int y, const char fmt[], ... ) \
	__attribute__(( format(printf,3,4) ));

#endif
