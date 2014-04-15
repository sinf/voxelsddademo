#include <stdio.h>
#include <stdarg.h>
#include <SDL.h>
#include "text.h"

static SDL_Surface *font = NULL;

int load_font( void )
{
	SDL_Surface *a;
	
	a = SDL_LoadBMP( "data/font_9x17.bmp" );
	
	if ( !a )
		return 0;
	
	font = SDL_DisplayFormat( a );
	SDL_FreeSurface( a );
	SDL_SetColorKey( font, SDL_SRCCOLORKEY|SDL_SRCALPHA, SDL_MapRGB( font->format, 0xFF, 0, 0 ) );
	
	return 1;
}

void unload_font( void )
{
	if ( font ) SDL_FreeSurface( font );
	font = NULL;
}

void draw_text( struct SDL_Surface *dst, int x0, int y, const char text[] )
{
	const char *c;
	int x = x0;
	
	for( c=text; ( *c != '\0' ); c++ )
	{
		unsigned index;
		
		if ( *c == '\n' )
		{
			y += GLYPH_H;
			x = x0;
			continue;
		}
		
		index = *c - 33;
		
		if ( index < 94 )
		{
			SDL_Rect r, p;
			r.x = 0;
			r.y = index * GLYPH_H;
			r.w = GLYPH_W;
			r.h = GLYPH_H;
			p.x = x;
			p.y = y;
			SDL_BlitSurface( font, &r, dst, &p );
		}
		
		x += GLYPH_W;
	}
}

void draw_text_f( struct SDL_Surface *dst, int x, int y, const char fmt[], ... )
{
	char buf[512];
	va_list args;
	
	va_start( args, fmt );
	vsnprintf( buf, sizeof(buf), fmt, args );
	va_end( args );
	
	draw_text( dst, x, y, buf );
}
