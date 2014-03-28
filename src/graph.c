#include <stdio.h>
#include "graph.h"
#include "text.h"

static void find_scale( Graph g[1] )
{
	Sint64 low, high, s;
	int n;
	low = high = g->samples[0];
	for( n=1; n<g->bounds.w; n++ ) {
		s = g->samples[n];
		if ( s < low ) low = s;
		if ( s > high ) high = s;
	}
	g->min = low;
	g->max = high;
}

void update_graph( Graph g[1], Sint64 new_sample )
{
	int sp = g->sp;
	g->sp = ( sp + 1 ) % g->bounds.w;
	g->samples[sp] = new_sample;
	if ( new_sample > g->max )
		g->max = new_sample;
	if ( new_sample < g->min )
		g->min = new_sample;
	if ( !( sp & 0x7 ) )
		find_scale( g );
}

static void draw_scale( Graph const g[1], SDL_Surface dst[1], Uint32 color, Sint64 range )
{
	int step, steps = 3;
	int y, y_inc;
	Sint64 value, value_inc, unit_fract, rounding;
	SDL_Rect r;
	
	value = g->min;
	value_inc = range / steps;
	
	y_inc = g->bounds.h * value_inc / range;
	y = g->bounds.y + g->bounds.h;
	
	unit_fract = g->unit_size / 1000;
	rounding = g->show_fraction ? unit_fract : g->unit_size;
	rounding >>= 1;
	
	for( step=0; step<=steps; step++ )
	{
		int tx, ty;
		
		/* Draw a small stick that shows the exact position of this step */
		r.x = g->bounds.x - 30;
		r.y = y;
		r.w = 35;
		r.h = 1;
		SDL_FillRect( dst, &r, color );
		
		tx = g->bounds.x - 7 * GLYPH_W;
		ty = y - GLYPH_H;
		
		if ( g->show_fraction ) {
			draw_text_f( dst, tx, ty, "%3d.%03d",
				(int)( value / g->unit_size ),
				(int)( ( ( value + rounding ) / unit_fract ) % unit_fract )
			);
		} else {
			draw_text_f( dst, tx, ty, "%7d",
				(int)( ( value + rounding ) / g->unit_size ) );
		}
		
		y -= y_inc;
		value += value_inc;
	}
	
	/*
	draw_text_f( dst, g->bounds.x, g->bounds.y + g->bounds.h + 20,
		"Min: %d\nMax: %d\n", (int) g->min, (int) g->max );
	*/
}

void draw_graph( Graph const g[1], SDL_Surface dst[1] )
{
	Uint32 border_color = SDL_MapRGB( dst->format, g->border[0], g->border[1], g->border[2] );
	Uint32 curve_color = SDL_MapRGB( dst->format, g->curve[0], g->curve[1], g->curve[2] );
	SDL_Rect r;
	int x;
	Sint64 range = g->max - g->min;
	
	if ( !range )
		return;
	
	for( x=0; x<g->bounds.w; x++ )
	{
		Sint64 s = g->samples[x];
		Sint64 y = g->bounds.h * ( s - g->min ) / range;
		
		if ( y <= 0 )
			continue;
		
		r.x = g->bounds.x + ( g->bounds.w + x - g->sp ) % g->bounds.w;
		r.y = g->bounds.y + g->bounds.h - y;
		r.w = 1;
		r.h = y;
		
		SDL_FillRect( dst, &r, curve_color );
	}
	draw_scale( g, dst, border_color, range );
}
