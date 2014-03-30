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
	
	oc = calloc( 1, sizeof(Octree) );
	
	/* Have only the root node */
	oc->num_nodes = 1;
	
	oc->size = 1 << toplevel;
	oc->root_level = toplevel;
	
	oc->root.mat = 0;
	oc->root.children = NULL;
	
	return oc;
}

static uint64 get_morton_code( uint64 x, uint64 y, uint64 z )
{
	uint64 a = 0;
	int s;
	for( s=0; s<21; s++ ) {
		a |= ( x & 1 ) << ( s + 2 );
		a |= ( y & 1 ) << ( s + 1 );
		a |= ( z & 1 ) << s;
		x >>= 1;
		y >>= 1;
		z >>= 1;
	}
	#if 0
	a |= ( z & 1 ) << 21; /* the last available bit */
	#endif
	return a;
}

void oc_free( Octree *oc )
{
	oc_collapse_node( oc, &oc->root );
	assert( oc->num_nodes == 1 );
	free( oc );
}

void oc_clear( Octree *oc, int m )
{
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
	uint8 m;
	
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
int get_mode_material( OctreeNode *node )
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
