#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <float.h>
#include <assert.h>

#define VOXEL_INTERNALS 1
#include "voxels.h"

Octree *oc_init( int toplevel )
{
	Octree *oc;
	size_t bricks;
	
	oc = calloc( 1, sizeof(Octree) );
	
	/* Have only the root node */
	oc->num_nodes = 1;
	
	oc->size = 1 << toplevel;
	oc->root_level = toplevel;
	
	oc->root.mat = 0;
	oc->root.children = NULL;
	
	bricks = oc->nor_bricks_x = ( oc->size + NOR_BRICK_S - 1 ) / NOR_BRICK_S;
	bricks = bricks * bricks * bricks;
	oc->nor_bricks = calloc( bricks, sizeof oc->nor_bricks[0] );
	oc->nor_density = calloc( bricks, sizeof oc->nor_density[0] );
	
	return oc;
}

static void clear_normals( Octree *oc )
{
	size_t b, n;
	
	b = oc->nor_bricks_x;
	b = b*b*b;
	
	for( n=0; n<b; n++ ) {
		if ( oc->nor_bricks[n] )
			free( oc->nor_bricks[n] );
	}
	
	memset( oc->nor_density, 0, b * sizeof oc->nor_density[0] );
}

void set_voxel_normal( Octree *oc, unsigned x, unsigned y, unsigned z, float nx, float ny, float nz )
{
	size_t bx, by, bz;
	size_t b;
	size_t nbx;
	float *nors;
	float *p;
	
	bx = x / NOR_BRICK_S;
	by = y / NOR_BRICK_S;
	bz = z / NOR_BRICK_S;
	
	x %= NOR_BRICK_S;
	y %= NOR_BRICK_S;
	z %= NOR_BRICK_S;
	
	nbx = oc->nor_bricks_x;
	b = bx * nbx * nbx + by * nbx + bz;
	nors = oc->nor_bricks[b];
	
	if ( nx == ny && ny == nz && nz == 0.0f && oc->nor_density[b] ) {
		oc->nor_density[b] -= 1;
		if ( !oc->nor_density[b] ) {
			free( nors );
			oc->nor_bricks[b] = NULL;
			return;
		}
	}
	
	if ( !nors )
		oc->nor_bricks[b] = nors = aligned_alloc( 16, NOR_BRICK_S3 * sizeof( float ) * 3 );
	
	oc->nor_density[b] += 1;
	
	p = nors + x*NOR_BRICK_S2*3 + y*NOR_BRICK_S*3 + z*3;
	p[0] = nx;
	p[1] = ny;
	p[2] = nz;
}

void get_voxel_normal( Octree const *oc, unsigned x, unsigned y, unsigned z, float *nx, float *ny, float *nz )
{
	size_t bx, by, bz, nbx, b;
	float *nors;
	float *n;
	
	bx = x / NOR_BRICK_S;
	by = y / NOR_BRICK_S;
	bz = z / NOR_BRICK_S;
	x %= NOR_BRICK_S;
	y %= NOR_BRICK_S;
	z %= NOR_BRICK_S;
	
	nbx = oc->nor_bricks_x;
	b = bx * nbx * nbx + by * nbx + bz;
	nors = oc->nor_bricks[b];
	
	if ( !nors ) {
		nx[0] = ny[1] = nz[2] = 0;
		return;
	}
	
	n = nors + x*NOR_BRICK_S2*3 + y*NOR_BRICK_S*3 + z*3;
	*nx = n[0];
	*ny = n[1];
	*nz = n[2];
}

void oc_free( Octree *oc )
{
	clear_normals( oc );
	free( oc->nor_bricks );
	free( oc->nor_density );
	
	oc_collapse_node( oc, &oc->root );
	assert( oc->num_nodes == 1 );
	free( oc );
}

void oc_clear( Octree *oc, Material_ID m )
{
	clear_normals( oc );
	oc_collapse_node( oc, &oc->root );
	assert( oc->num_nodes == 1 );
	oc->root.mat = m;
}


#define X ~0
/* Makes recursion code more readable and consistent. Also allows to use loops */
const int OC_RECURSION_MASK[8][3] = {
	{0, 0, 0},
	{0, 0, X},
	{0, X, 0},
	{0, X, X},
	{X, 0, 0},
	{X, 0, X},
	{X, X, 0},
	{X, X, X}
};
#undef X

void oc_expand_node( Octree *oc, OctreeNode *node )
{
	int n;
	Material_ID m;
	
	if ( node->children )
		return;
	
	node->children = calloc( 8, sizeof(OctreeNode) );
	oc->num_nodes += 8;
	
	m = node->mat;
	for( n=0; n<8; n++ )
		node->children[n].mat = m;
}

void oc_collapse_node( Octree *oc, OctreeNode *node )
{
	if ( node->children )
	{
		int n;
		for( n=0; n<8; n++ )
			oc_collapse_node( oc, &node->children[n] );
		
		free( node->children );
		node->children = NULL;
		oc->num_nodes -= 8;
	}
}

void get_node_bounds( aabb3f *bounds, const vec3i pos, int size )
{
	int n;
	for( n=0; n<PADDED_VEC3_SIZE; n++ )
	{
		bounds->min[n] = pos[n];
		bounds->max[n] = pos[n] + size;
	}
}

/* Chooses the most frequent (mode) material in children
Material zero is treated as a second class citizen */
Material_ID get_mode_material( OctreeNode *node )
{
	int count[NUM_MATERIALS] = {0};
	int freq, index;
	int n;
	
	for( n=1; n<8; n++ ) {
		int m = node->children[n].mat;
		count[m] += 1;
	}
	
	index = 0;
	freq = 0;
	
	for( n=1; n<NUM_MATERIALS; n++ )
	{
		if ( count[n] >= freq )
		{
			freq = count[n];
			index = n;
		}
	}
	
	if ( !freq )
		index = 0;
	
	return index;
}
