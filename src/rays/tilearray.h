#pragma once
#ifndef _TILEARRAY_H
#define _TILEARRAY_H

typedef struct TileArray
{
	int size[3];
	int *tiles;
} TileArray;

TileArray *alloc_tile_array( int sx, int sy, int sz );
void free_tile_array( TileArray *ta );
int *get_tile_addr( TileArray *t, int x, int y, int z );
int get_tile_at( TileArray *t, int x, int y, int z );

#endif
