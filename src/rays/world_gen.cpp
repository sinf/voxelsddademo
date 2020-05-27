#include <math.h>
#include "bytevec.hpp"

extern "C" {
#include "world_gen.h"
#define VOXEL_INTERNALS
#include "voxels.h"
}

/* A function that generates some volumetric texture. (x,y,z) are the voxel coordinates */
typedef Bytev (*VoxelGenFunc)( Bytev x, Bytev y, Bytev z, Bytev random_tile_id );

enum {
	TILE_SIZE_EXP = 5,
	TILE_SIZE = 1<<TILE_SIZE_EXP
};

static void build_octree_node( Octree *tree, OctreeNode *node, const uint8 voxels[], size_t resol, size_t side, size_t x0, size_t y0, size_t z0 )
{
	size_t n, x, y, z;
	
	oc_expand_node( tree, node );
	side >>= 1;
	
	for( n=0; n<8; n++ )
	{
		x = x0 + ( n >> 2 ) * side;
		y = y0 + ( n >> 1 & 1 ) * side;
		z = z0 + ( n & 1 ) * side;
		
		if ( side == 1 ) {
			/* 2nd lowest level - 2x2x2 */
			node->children[n].mat = voxels[ x * resol * resol + y * resol + z ];
		} else {
			build_octree_node( tree, node->children+n, voxels, resol, side, x, y, z );
		}
	}
	
	node->mat = get_mode_material( node );
	
	for( n=0; n<8; n++ ) {
		if ( node->children[n].children )
			return;
		if ( node->children[n].mat != node->children[0].mat )
			return;
	}
	
	/* all children are leaf nodes and they have the same material */
	oc_collapse_node( tree, node );
}

static void build_tile( uint8 voxels[1<<(3*TILE_SIZE_EXP)], VoxelGenFunc func, uint8 tid )
{
	uint8 bz0[BYTE_VEC_LEN];
	Bytev bx, by, bz, one, zinc, tidv;
	int x, y, z, t;
	
	for( t=0; t<BYTE_VEC_LEN; t++ )
		bz0[t] = t;
	
	zinc = Bytev( BYTE_VEC_LEN );
	one = Bytev( 1 );
	tidv = Bytev( tid );
	bx.clear();
	
	int assertion[ (int) TILE_SIZE >= (int) BYTE_VEC_LEN ];
	(void) assertion;
	
	for( x=0; x<TILE_SIZE; x++, bx+=one ) {
		by.clear();
		for( y=0; y<TILE_SIZE; y++, by+=one ) {
			bz = _mm_load_si128( (const __m128i*) bz0 );
			for( z=0; z<TILE_SIZE; z+=BYTE_VEC_LEN, bz+=zinc ) {
				_mm_store_si128( (__m128i*)( voxels + x * TILE_SIZE * TILE_SIZE + y * TILE_SIZE + z ), func( bx, by, bz, tidv ).x );
			}
		}
	}
}

static void insert_subtree( Octree *tree, OctreeNode *dst, OctreeNode *subtree, int x0, int y0, int z0, int dst_x, int dst_y, int dst_z, int level, int min_level )
{
	int dx, dy, dz, n, s;
	int hx, hy, hz;
	
	oc_expand_node( tree, dst );
	
	s = 1 << --level;
	hx = x0 + s;
	hy = y0 + s;
	hz = z0 + s;
	
	dx = ( dst_x >= hx );
	dy = ( dst_y >= hy );
	dz = ( dst_z >= hz );
	n = dx << 2 | dy << 1 | dz;
	
	if ( level == min_level ) {
		oc_collapse_node( tree, dst->children + n );
		dst->children[n] = *subtree;
	} else {
		x0 += dx * s;
		y0 += dy * s;
		z0 += dz * s;
		insert_subtree( tree, dst->children + n, subtree, x0, y0, z0, dst_x, dst_y, dst_z, level, min_level );
	}
}

static void place_tile( Octree *tree, int tile_x, int tile_y, int tile_z, uint8 tid, VoxelGenFunc func )
{
	OctreeNode *subtree = (OctreeNode*) calloc( 1, sizeof *subtree );
	
	if ( subtree )
	{
		uint8 voxels[1<<(3*TILE_SIZE_EXP)];
		int tx, ty, tz;
		
		tx = tile_x * TILE_SIZE;
		ty = tile_y * TILE_SIZE;
		tz = tile_z * TILE_SIZE;
		
		build_tile( voxels, func, tid );
		build_octree_node( tree, subtree, voxels, TILE_SIZE, TILE_SIZE, 0, 0, 0 );
		insert_subtree( tree, &tree->root, subtree, 0, 0, 0, tx, ty, tz, tree->root_level, TILE_SIZE_EXP );
	}
}

/* Sand */
static Bytev gen_ground( Bytev x, Bytev y, Bytev z, Bytev id ) {
	(void) ( x + y + z );
	return ( id & 3 ) + 12;
}

static Bytev gen_noise( Bytev x, Bytev y, Bytev z, Bytev w )
{
	/* Jenkins hash function */
	__m128i h = x.x;
	h = _mm_add_epi32( h, _mm_slli_epi32( h, 10 ) );
	h = _mm_xor_si128( h, _mm_srli_epi32( h, 6 ) );
	
	h = _mm_add_epi32( h, y.x );
	h = _mm_add_epi32( h, _mm_slli_epi32( h, 10 ) );
	h = _mm_xor_si128( h, _mm_srli_epi32( h, 6 ) );
	
	h = _mm_add_epi32( h, z.x );
	h = _mm_add_epi32( h, _mm_slli_epi32( h, 10 ) );
	h = _mm_xor_si128( h, _mm_srli_epi32( h, 6 ) );
	
	h = _mm_add_epi32( h, w.x );
	h = _mm_add_epi32( h, _mm_slli_epi32( h, 10 ) );
	h = _mm_xor_si128( h, _mm_srli_epi32( h, 6 ) );
	
	h = _mm_add_epi32( h, _mm_srli_epi32( h, 3 ) );
	h = _mm_xor_si128( h, _mm_srli_epi32( h, 11 ) );
	h = _mm_add_epi32( h, _mm_slli_epi32( h, 15 ) );
	
	return Bytev( h );
}

/* The stainless kind */
static Bytev gen_steel( Bytev x, Bytev y, Bytev z, Bytev id ) {
	(void) id;
	return Bytev( 8 ) + ( ( x ^ y ^ z ) & 3 );
}

static Bytev gen_concrete( Bytev x, Bytev y, Bytev z, Bytev id ) {
	(void)( x + y + z + id );
	return Bytev( 10 );
	/*
	Bytev grain = gen_noise( x, y, z, id ) & 3;
	return ( grain + 8 ) & ( grain > 0 );
	*/
}

/* Ground with a concrete slab on top */
static Bytev gen_concrete_base( Bytev x, Bytev y, Bytev z, Bytev id ) {
	return choose( gen_ground( x, y, z, id ), gen_concrete( x, y, z, id ), y < 2*TILE_SIZE/3 );
}

static Bytev gen_simple_stairs( Bytev x, Bytev y, Bytev z, Bytev id )
{
	int s = 2;
	Bytev sx = x >> s;
	Bytev sy = y >> s;
	return gen_concrete( x, y, z, id ) & ( sx > sy ) & ( x - ( 2 << s ) < y );
}

static Bytev gen_floor( Bytev x, Bytev y, Bytev z, Bytev id )
{
	return choose( gen_concrete( x, y, z, id ), Bytev( 0 ), y < TILE_SIZE/6 );
}

static int choose_tile( int x, int y, int z, int id, int s )
{
	const int r = 128 / TILE_SIZE;
	unsigned c = s - 2*r;
	unsigned u = x - r;
	unsigned v = z - r;
	
	(void) id;
	
	if ( u < c && v < c )
	{
		if ( !y ) {
			/* Concrete slab under the building */
			return 2;
		} else {
			/* Pieces of the building */
			return 3;
		}
	}
	
	if ( !y && x > 0 && z > 0 && x+1 < s && z+1 < s )
	{
		/* Ground */
		return 1;
	}
	
	/* Air */
	return 0;
}

void generate_world( struct Octree *oc )
{	
	const VoxelGenFunc tile_funcs[] = {
		NULL,
		gen_ground,
		gen_concrete_base,
		gen_floor
	};
	
	int x, y, z;
	int s = 1 << oc->root_level - TILE_SIZE_EXP;
	uint32 tid = 0x7ad6d567;
	
	oc_clear( oc, 0 );
	
	place_tile( oc, 0, s - 1, 0, 0, gen_floor );
	
	for( x=0; x<s; x++ ) {
		for( y=0; y<s; y++ ) {
			for( z=0; z<s; z++ ) {
				int tile;
				
				tid *= 1664525u;
				tid += 1013904223u;
				tid >>= 8; /* the lowest bits are too predictable and produce artefacts */
				tile = choose_tile( x, y, z, tid, s );
				
				if ( tile > 0 )
					place_tile( oc, x, y, z, tid, tile_funcs[tile] );
			}
		}
	}
	
	/*
	1. Generate ground (sand) with a conrete slab at middle
	2. Ground floor. Put walls, exits/entrances, windows, staircases
	3. Many regular floors. These have walls, windows and staircases. Also internal doors
	4. Top floor
	*/
}
