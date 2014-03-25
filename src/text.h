#pragma once
#ifndef _TEXT_H
#define _TEXT_H

/* -- Glyph size --
	le_font.bmp: 21.787234042553191 x 32
	small_font.bmp: 10.893617021276595 x 16
	
	11x16
*/

#define GLYPH_W 9
#define GLYPH_H 17

struct SDL_Surface;

/* Returns 0 on failure */
int load_font( void );
void unload_font( void );

void draw_text( struct SDL_Surface *dst, int x, int y, const char text[] );

/* Draws formatted text - printf style */
void draw_text_f( struct SDL_Surface *dst, int x, int y, const char fmt[], ... )
__attribute__(( format(printf,4,5) ));

#endif
