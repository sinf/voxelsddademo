#ifndef _GRAPH_H
#define _GRAPH_H

#include <SDL.h>

enum { MAX_GRAPH_W = 64 };

typedef struct {
	SDL_Rect bounds;
	Uint8 border[3], curve[3]; /* colors */
	Sint64 unit_size; /* How large integer value corresponds to 1.0. Must be at least 1000 when show_fraction is nonzero */
	int show_fraction;
	int sp; /* next sample position in the cyclic sample buffer */
	Sint64 total; /* total value of all buffered samples */
	Sint64 min, max;
	Sint64 samples[MAX_GRAPH_W];
} Graph;

void update_graph( Graph g[1], Sint64 new_sample );
void draw_graph( Graph const g[1], SDL_Surface dst[1] );

#endif
