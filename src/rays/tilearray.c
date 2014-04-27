#include <stdlib.h>
#include "tilearray.h"

TileArray *alloc_tile_array( int sx, int sy, int sz )
{
	TileArray *ta = malloc( sizeof(TileArray) );
	ta->size[0] = sx;
	ta->size[1] = sy;
	ta->size[2] = sz;
	ta->tiles = calloc( sx*sy*sz, sizeof(int) );
	return ta;
}

void free_tile_array( TileArray *ta )
{
	free( ta->tiles );
	free( ta );
}

int *get_tile_addr( TileArray *t, int x, int y, int z )
{
	if ( x < 0 || x >= t->size[0] || y < 0 || y >= t->size[1] || z < 0 || z >= t->size[2] )
		return NULL;
	
	return t->tiles
		+ x * t->size[1] * t->size[2]
		+ y * t->size[2]
		+ z;
}

int get_tile_at( TileArray *t, int x, int y, int z )
{
	int *p = get_tile_addr( t, x, y, z );
	return p ? *p : -1;
}
