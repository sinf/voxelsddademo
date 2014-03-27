#include <stdio.h>
#include "graph.h"
#include "text.h"

static void find_scale( Graph g[1] )
{
	Sint64 low, high;
	int n;
	low = high = g->samples[0];
	for( n=1; n<g->bounds.w; n++ ) {
		int s = g->samples[n];
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

static void draw_outlines( SDL_Surface dst[1], SDL_Rect const box[1], Uint32 color )
{
	SDL_Rect e, s, w;
	
	/*
	n.x = box->x + 1;
	n.y = box->y;
	n.w = box->w - 2;
	n.h = 1;
	*/
	
	e.x = box->x + box->w - 1;
	e.y = box->y;
	e.w = 1;
	e.h = box->h;
	
	w.x = box->x;
	w.y = box->y;
	w.h = box->h;
	w.w = 1;
	
	s.x = box->x + 1;
	s.y = box->y + box->h - 1;
	s.w = box->w - 2;
	s.h = 1;
	
	/*
	SDL_FillRect( dst, &n, color );
	*/
	SDL_FillRect( dst, &s, color );
	SDL_FillRect( dst, &w, color );
	SDL_FillRect( dst, &e, color );
}

static void draw_scale( Graph const g[1], SDL_Surface dst[1], Uint32 color )
{
	int step, steps = 3;
	int y, y_inc;
	Sint64 value, value_inc;
	Sint64 range;
	SDL_Rect r;
	
	range = g->max - g->min;
	value = g->min;
	value_inc = range / steps;
	
	y_inc = g->bounds.h * value_inc / range;
	y = g->bounds.y + g->bounds.h - y_inc;
	
	for( step=0; step<steps; step++ )
	{
		/* Draw a small stick that shows the exact position of this step */
		r.x = g->bounds.x;
		r.y = y;
		r.w = 20;
		r.h = 1;
		SDL_FillRect( dst, &r, color );
		
		draw_text_f( dst, g->bounds.x, y-GLYPH_H, "%d M", (int)( ( 500000 + value ) / 1000000 ) );
		
		y -= y_inc;
		value += value_inc;
	}
}

void draw_graph( Graph const g[1], SDL_Surface dst[1] )
{
	Uint32 border_color = SDL_MapRGB( dst->format, g->border[0], g->border[1], g->border[2] );
	Uint32 curve_color = SDL_MapRGB( dst->format, g->curve[0], g->curve[1], g->curve[2] );
	SDL_Rect r;
	int x;
	Sint64 range = g->max - g->min;
	
	if ( range )
	{
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
	}
	
	draw_scale( g, dst, border_color );
	
	r = g->bounds;
	r.x--;
	r.y--;
	r.w += 2;
	r.h += 2;
	draw_outlines( dst, &r, border_color );
}
