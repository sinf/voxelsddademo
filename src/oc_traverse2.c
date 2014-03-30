#include <stdlib.h>
#include <xmmintrin.h>
#include <string.h>
#include "voxels.h"
#include "oc_traverse2.h"
#include "render_core.h"

#define OCTREE_DEPTH_HARDLIMIT 15
#define ALLOW_DEBUG_VISUALS 1

extern int oc_show_travel_depth;
extern int oc_detail_level;

typedef struct {
	float tmin[3];
	uint32 iter;
	float tmax[3];
	uint32 id;
} RayParams;

static size_t filter_rays_iterator( RayParams out[], RayParams const in[], size_t num_rays, uint8 rec_mask )
{
	size_t r, num_out=0;
	for( r=0; r<num_rays; r++ )
	{
		if ( in[r].iter != rec_mask )
			continue;
		
		out[num_out++] = in[r];
	}
	return num_out;
}

static size_t filter_rays( RayParams out[], RayParams const in[], size_t num_rays, int child_id, uint8 dead_bits[] )
{
	size_t num_out = 0;
	size_t r;
	for( r=0; r<num_rays; r++ )
	{
		RayParams o;
		unsigned g;
		float near, far;
		size_t id = in[r].id;
		
		if ( dead_bits[id] )
			continue;
		
		o.id = id;
		
		/* Subdivide the interval */
		for( g=0; g<3; g++ )
		{
			float tmin, tmax, split;
			
			split = ( in[r].tmin[g] + in[r].tmax[g] ) * 0.5f;
			
			if ( child_id & ( 4 >> g ) ) {
				tmin = split;
				tmax = in[r].tmax[g];
			} else {
				tmin = in[r].tmin[g];
				tmax = split;
			}
			
			o.tmin[g] = tmin;
			o.tmax[g] = tmax;
		}
		/* Test for AABB intersection */
		near = fmax( fmax( o.tmin[0], o.tmin[1] ), o.tmin[2] );
		far = fmin( fmin( o.tmax[0], o.tmax[1] ), o.tmax[2] );
		if ( near > far || far < 0.0f ) {
			continue;
		}
		
		out[num_out++] = o;
	}
	return num_out;
}

static void process_leaf( const OctreeNode *node,
	int octree_level,
	size_t num_rays,
	RayParams rays[],
	uint8 out_mat[],
	float out_depth[],
	uint8 dead_bits[] )
{
	uint8 mat = node->mat;
	size_t r;
	
	if ( !mat )
		return;
	
	#if ALLOW_DEBUG_VISUALS
	if ( oc_show_travel_depth )
	{
		mat = octree_level + 2;
		mat &= MATERIAL_BITMASK;
	}
	#endif
	
	for( r=0; r<num_rays; r++ )
	{
		size_t id;
		float a, b, c, z;
		
		id = rays[r].id;
		
		a = rays[r].tmin[0];
		b = rays[r].tmin[1];
		c = rays[r].tmin[2];
		z = fmax( a, fmax( b, c ) );
		
		out_mat[id] = mat;
		out_depth[id] = z;
		
		dead_bits[id] = 1;
	}
}

static void traverse_branch( const OctreeNode *node,
	int octree_level,
	size_t ray_count,
	RayParams rays[],
	uint8 out_mat[],
	float out_depth[],
	int iter,
	uint8 dead_bits[] )
{
	RayParams *rays2 = rays + ray_count; /* subset of current rays */
	int m;
	
	octree_level--;
	
	for( m=0; m<8; m++ )
	{
		int child_id = m ^ iter;
		OctreeNode *child = node->children + child_id;
		size_t rc2 = filter_rays( rays2, rays, ray_count, m, dead_bits ); /* why pass m instead of child_id??? */
		if ( !rc2 )
			continue;
		if ( !octree_level || !child->children )
			process_leaf( child, octree_level, rc2, rays2, out_mat, out_depth, dead_bits );
		else
			traverse_branch( child, octree_level, rc2, rays2, out_mat, out_depth, iter, dead_bits );
	}
}

void oc_traverse_dac( const Octree oc[1],
	size_t ray_count,
	float const *ray_o[3],
	float const *ray_d[3],
	uint8 out_mat[],
	float out_depth[] )
{
	uint8 *dead_bits;
	RayParams *params;
	
	size_t r;
	float aabb_min = 0;
	float aabb_max = oc->size;
	int t;
	
	dead_bits = calloc( ray_count, 1 );
	params = aligned_alloc( 16, sizeof( *params ) * ray_count * ( OCTREE_DEPTH_HARDLIMIT + 8 ) );
	
	for( r=0; r<ray_count; r++ )
	{
		int k;
		int iter = 0;
		
		for( k=0; k<3; k++ ) {
			float tmin, tmax;
			float d;
			
			/* Intersect ray with the bounding box of the root node */
			d = ray_d[k][r];
			tmin = ( aabb_min - ray_o[k][r] ) / d;
			tmax = ( aabb_max - ray_o[k][r] ) / d;
			
			if ( tmin > tmax ) {
				/* Ray direction is negative. Child nodes should be traversed in reverse order along this axis. */
				iter |= 4 >> k;
			}
			
			params[r].tmin[k] = fmin( tmin, tmax );
			params[r].tmax[k] = fmax( tmin, tmax );
		}
		
		params[r].id = r;
		params[r].iter = iter;
		
		out_mat[r] = 0;
		out_depth[r] = MAX_DEPTH_VALUE;
	}
	
	for( t=0; t<8; t++ ) {
		RayParams *rays2 = params + ray_count;
		size_t rc2 = filter_rays_iterator( rays2, params, ray_count, t );
		traverse_branch( &oc->root, oc->root_level - oc_detail_level + 1, rc2, rays2, out_mat, out_depth, t, dead_bits );
	}
	
	free( params );
}
