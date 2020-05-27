#include <stdlib.h>
#include <string.h>
#include "voxels.h"
#include "voxels_csg.h"
#include "aabb.h"
#include "tilearray.h"
#include "city.h"

/* Aligned fast sizes that produce optimal octrees:
	1.0
	0.875
	0.75
	0.625
	0.5
	0.375
	0.25
	0.125
	0.0625
*/

#define GROUND_LEVEL 1
#define GROUND_MATERIAL (12 + (rand() & 0x3))

#define FLOOR_MATERIAL 9
#define FLOOR_THICKNESS 0.125

#define WALL_MATERIAL 10
#define WALL_THICKNESS 0.0625

#define VSUPPORT_SIZE 0.5
#define VSUPPORT_MATERIAL FLOOR_MATERIAL

#define WINDOW_PADDING_SIDE 0.375 /* Affects window width */
#define WINDOW_PADDING_LOW 0.125 /* Distance from floor */
#define WINDOW_PADDING_HIGH 0.5 /* Distance from ceiling */

#define STAIR_SIZE 0.0625
#define NUM_STAIRS 16

/* Basic tile types */
#define T_NONE 0
#define T_DIRT 1
#define T_STAIRS_YZ 2
#define T_STAIRS_YX 3
/* Composite tile: OR'd combination of flags */
#define TF_FLOOR 0x4
#define TF_WALL_YZ 0x8
#define TF_WALL_YX 0x10
#define TF_WINDOW_YZ 0x20
#define TF_WINDOW_YX 0x40
/* Special tiles (change other tiles and become normal tiles) */
#define T_DELETE_FLOOR_PREVX 128
#define T_DELETE_FLOOR_PREVZ 129

static void fill_tile( Octree *oc, float pos[3], double scale, int tile )
{
	aabb3f box;
	aabb3f box_copy;
	int n;
	
	/* Initialize box to full tile */
	for( n=0; n<3; n++ )
	{
		box.min[n] = pos[n];
		box.max[n] = pos[n] + scale;
	}
	
	box.min[3] = box.max[3] = 0;
	memcpy( &box_copy, &box, sizeof(box) );
	
	if ( tile == T_DIRT )
	{
		/* Dirt */
		csg_box( oc, &box, GROUND_MATERIAL );
	}
	else if ( tile == T_STAIRS_YZ || tile == T_STAIRS_YX )
	{
		/* Staircase */
		float step = scale * STAIR_SIZE;
		int axis, axis2;
		
		if ( tile == T_STAIRS_YX )
		{
			axis = 0;
			axis2 = 2;
		}
		else
		{
			axis = 2;
			axis2 = 0;
		}
		
		box.max[axis] = box.min[axis] + step;
		box.max[axis2] = box.min[axis2] + scale * 0.5;
		box.max[1] = box.min[1] + 2 * step;
		
		for( n=0; n<NUM_STAIRS; n++ )
		{
			csg_box( oc, &box, WALL_MATERIAL );
			
			box.min[axis] += step;
			box.max[axis] += step;
			box.max[1] += step;
			
			/* Thin stairs */
			box.min[1] += step;
		}
		
		tile = TF_FLOOR | ( axis == 0 ? TF_WALL_YX : TF_WALL_YZ );
		memcpy( &box, &box_copy, sizeof(box) );
		goto composite;
	}
	else /* Tiles 4-31 */
	{
		composite:;
		
		if ( tile & TF_FLOOR )
		{
			/* Concrete floor */
			box.max[1] = box.min[1] + scale * FLOOR_THICKNESS;
			csg_box( oc, &box, FLOOR_MATERIAL );
			memcpy( &box, &box_copy, sizeof(box) );
		}
		if ( tile & TF_WALL_YZ )
		{
			/* Vertical concrete wall, YZ */
			box.max[0] = box.min[0] + scale * WALL_THICKNESS;
			csg_box( oc, &box, WALL_MATERIAL );
			memcpy( &box, &box_copy, sizeof(box) );
		}
		if ( tile & TF_WALL_YX )
		{
			/* Vertical concrete wall, YX */
			box.max[2] = box.min[2] + scale * WALL_THICKNESS;
			csg_box( oc, &box, WALL_MATERIAL );
			memcpy( &box, &box_copy, sizeof(box) );
		}
		if ( tile & TF_WINDOW_YZ )
		{
			/* YZ window */
			box.min[0] += scale * WINDOW_PADDING_SIDE;
			box.max[0] -= scale * WINDOW_PADDING_SIDE;
			box.min[1] += scale * WINDOW_PADDING_LOW;
			box.max[1] -= scale * WINDOW_PADDING_HIGH;
			box.max[2] = box.min[1] + scale * WALL_THICKNESS;
			csg_box( oc, &box, 0 );
			memcpy( &box, &box_copy, sizeof(box) );
		}
		if ( tile & TF_WINDOW_YX )
		{
			/* YX window */
			box.min[2] += scale * WINDOW_PADDING_SIDE;
			box.max[2] -= scale * WINDOW_PADDING_SIDE;
			box.min[1] += scale * WINDOW_PADDING_LOW;
			box.max[1] -= scale * WINDOW_PADDING_HIGH;
			box.max[0] = box.min[1] + scale * WALL_THICKNESS;
			csg_box( oc, &box, 0 );
			memcpy( &box, &box_copy, sizeof(box) );
		}
	}
}

static void choose_tiles( TileArray *array, int pass )
{
	int x, y, z;
	
	int midx1 = array->size[0] / 2;
	int midx2 = midx1 - 1;
	int midz1 = array->size[2] / 2;
	int midz2 = midz1 - 1;
	
	int s = array->size[0] / 4;
	int house_x1 = midx1 - s;
	int house_x2 = midx2 + s;
	int house_z1 = midz1 - s;
	int house_z2 = midz2 + s;
	
	for( x=0; x<array->size[0]; x++ )
	{
		for( y=0; y<array->size[1]; y++ )
		{
			for( z=0; z<array->size[2]; z++ )
			{
				int nbx1, nbx2;
				int nby1, nby2;
				int nbz1, nbz2;
				int *tile_p;
				int tile;
				
				nbx1 = get_tile_at( array, x-1, y, z );
				nbx2 = get_tile_at( array, x+1, y, z );
				nby1 = get_tile_at( array, x, y-1, z );
				nby2 = get_tile_at( array, x, y+1, z );
				nbz1 = get_tile_at( array, x, y, z-1 );
				nbz2 = get_tile_at( array, x, y, z+1 );
				
				tile_p = get_tile_addr( array, x, y, z );
				tile = *tile_p;
				
				if ( y < GROUND_LEVEL )
				{
					/* Dirt */
					tile = T_DIRT;
				}
				else
				{
					if ( x < house_x1 || x > house_x2 || z < house_z1 || z > house_z2 )
					{
						/* Empty borders (except for ground/dirt) */
						tile = T_NONE;
					}
					else if (( y > (GROUND_LEVEL+1)) && ( x == midx1 || x == midx2 ) && ( z == midz1 || z == midz2 ))
					{
						/* Leave a vertical shaft at centre */
						tile = T_NONE;
					}
					else
					{
						if ( pass < 0 )
						{
							/* Final pass: Remove temporary marks */
							if ( tile == T_DELETE_FLOOR_PREVX || tile == T_DELETE_FLOOR_PREVZ )
								tile = T_NONE;
						}
						else if ( pass == 0 )
						{
							/* First pass: Choose a random concrete tile */
							if ( rand() % 8 == 0 )
							{
								tile = T_STAIRS_YZ + ( rand() & 0x1 );
							}
							else
							{
								if ( y == GROUND_LEVEL )
									tile = TF_FLOOR;
								else
									tile = 4 + rand() % 127;
							}
						}
						else
						{
							/* Further passes: Fix invalid tile combinations */
							
							#define WALLS (TF_WALL_YZ|TF_WALL_YX)
							#define IS_STAIRS(x) (( (x) == T_STAIRS_YZ ) || ( (x) == T_STAIRS_YX ))
							
							/* Walls block access to stairs */
							if ( nbx2 == T_DELETE_FLOOR_PREVX || nbz2 == T_DELETE_FLOOR_PREVZ )
								tile &= ~TF_FLOOR;
							else if ( IS_STAIRS(nby1) )
							{
								if (( nby1 == T_STAIRS_YZ ) && ( tile & TF_WALL_YX ))
									tile &= ~TF_WALL_YX;
								else if (( nby1 == T_STAIRS_YX ) && ( tile & TF_WALL_YZ ))
									tile &= ~TF_WALL_YZ;
								
								tile &= ~TF_FLOOR;
							}
							else if ( IS_STAIRS( tile ) )
							{
								/* Stairs leading to nowhere */
								if ( nby2 <= 1 || nbx2 <= 1 || nbz2 <= 1 || nbx1 <= 1 || nbz1 <= 1 )
									tile = TF_FLOOR;
								/* Stairs too close */
								else if ( IS_STAIRS( nby1 ) || IS_STAIRS( nbx1 ) || IS_STAIRS( nbz1 ) )
									tile = TF_FLOOR;
							}
							
							if ( tile == T_DIRT )
								tile = T_NONE;
						}
					}
				}
				
				*tile_p = tile;
			}
		}
	}
}

void generate_city( Octree *oc )
{
	const int n_tiles = 16;
	const double tile_size = oc->size / (double) n_tiles;
	TileArray *tiles;
	int x, y, z;
	
	tiles = alloc_tile_array( n_tiles, n_tiles, n_tiles );
	
	for( x=0; x<3; x++ )
		choose_tiles( tiles, x );
	choose_tiles( tiles, -1 );
	
	for( x=0; x<n_tiles ; x++ )
	{
		for( y=0; y<n_tiles ; y++ )
		{
			for( z=0; z<n_tiles; z++ )
			{
				int tile = *get_tile_addr( tiles, x, y, z );
				
				float pos[3];
				pos[0] = x * tile_size;
				pos[1] = y * tile_size;
				pos[2] = z * tile_size;
				
				fill_tile( oc, pos, tile_size, tile );
			}
		}
	}
	
	free_tile_array( tiles );
	
	#if 0
	aabb3f box;
	box.min[0] = tile_size - VSUPPORT_SIZE * tile_size * 0.5;
	box.max[0] = tile_size + VSUPPORT_SIZE * tile_size * 0.5;
	box.min[2] = tile_size - VSUPPORT_SIZE * tile_size * 0.5;
	box.max[2] = tile_size + VSUPPORT_SIZE * tile_size * 0.5;
	box.min[1] = tile_size;
	box.max[1] = n_tiles * tile_size;
	csg_box( oc, &box, VSUPPORT_MATERIAL );
	
	x = ( n_tiles - 2 ) * tile_size;
	box.min[0] += x;
	box.max[0] += x;
	csg_box( oc, &box, VSUPPORT_MATERIAL );
	
	box.min[2] += x;
	box.max[2] += x;
	csg_box( oc, &box, VSUPPORT_MATERIAL );
	
	box.min[0] -= x;
	box.max[0] -= x;
	csg_box( oc, &box, VSUPPORT_MATERIAL );
	#endif
}
