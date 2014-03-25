#include <stdlib.h>
#include <float.h>
#include <xmmintrin.h>
#include "voxels.h"
#include "oc_traverse2.h"

#define OCTREE_DEPTH_HARDLIMIT 30
#define ALLOW_DEBUG_VISUALS 1

extern int oc_show_travel_depth;
extern int oc_detail_level;

typedef struct {
	float tmin[3];
	float tmax[3];
	size_t id;
} RayParams;

static size_t filter_rays( RayParams out[], RayParams const in[], uint8 const in_rec_mask[], uint8 const in_alive[], size_t num_rays, uint8 rec_mask, int child_id )
{
	size_t num_out = 0;
	size_t r;
	for( r=0; r<num_rays; r++ )
	{
		RayParams o;
		unsigned g;
		float near, far;
		size_t id = in[r].id;
		
		if ( !in_alive[id] )
			continue;
		
		if ( in_rec_mask[id] != rec_mask )
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
	RayParams const rays[], 
	uint8 alive[],
	uint8 out_mat[],
	float out_depth[],
	float *out_nor[3] )
{
	Material_ID mat = node->mat;
	size_t r;
	
	if ( !mat )
		return;
	
	#if ALLOW_DEBUG_VISUALS
	if ( oc_show_travel_depth )
		mat = octree_level + 2;
	#endif
	
	for( r=0; r<num_rays; r++ )
	{
		size_t id;
		float a, b, c, z;
		
		a = rays[r].tmin[0];
		b = rays[r].tmin[1];
		c = rays[r].tmin[2];
		z = fmax( a, fmax( b, c ) );
		
		id = rays[r].id;
		out_mat[id] = mat;
		out_depth[id] = z;
		alive[id] = 0;
		
		if ( out_nor ) {
			out_nor[0][id] = node->nor[0];
			out_nor[1][id] = node->nor[1];
			out_nor[2][id] = node->nor[2];
		}
	}
}

static void traverse_branch( const OctreeNode *node,
	int octree_level,
	size_t ray_count,
	RayParams const rays[],
	uint8 const rec_mask[],
	uint8 alive[],
	uint8 out_mat[],
	float out_depth[],
	float *out_nor[3] )
{
	RayParams *rays2 = (RayParams*) rays + ray_count; /* subset of current rays */
	int iter;
	
	octree_level--;
	
	/* For each possible order of iterating trough the 8 sub-cubes */
	for( iter=0; iter<8; iter++ )
	{
		int m;
		for( m=0; m<8; m++ )
		{
			int child_id = m ^ iter;
			OctreeNode *child = node->children + child_id;
			size_t rc2 = filter_rays( rays2, rays, rec_mask, alive, ray_count, iter, m /* why m instead of child_id??? */ );
			if ( !rc2 )
				continue;
			if ( !octree_level || !child->children )
				process_leaf( child, octree_level, rc2, rays2, alive, out_mat, out_depth, out_nor );
			else
				traverse_branch( child, octree_level, rc2, rays2, rec_mask, alive, out_mat, out_depth, out_nor );
		}
	}
}

void oc_traverse_dac( const Octree oc[1],
	size_t ray_count,
	float const *ray_o[3],
	float const *ray_d[3],
	uint8 out_mat[],
	float out_depth[],
	float *out_nor[3] )
{
	RayParams *params;
	uint8 *rec_mask;
	uint8 *alive;
	size_t r;
	float aabb_min = 0;
	float aabb_max = oc->size;
	
	params = aligned_alloc( sizeof( __m128 ), ( sizeof( *params ) + sizeof( *rec_mask ) + sizeof( *alive ) ) * ray_count * OCTREE_DEPTH_HARDLIMIT );
	rec_mask = (uint8*)( params + ray_count );
	alive = rec_mask + ray_count;
	
	for( r=0; r<ray_count; r++ )
	{
		int k;
		int iter = 0;
		
		alive[r] = 1;
		params[r].id = r;
		
		for( k=0; k<3; k++ ) {
			float tmin, tmax;
			
			/* Intersect ray with the bounding box of the root node */
			tmin = ( aabb_min - ray_o[k][r] ) / ray_d[k][r];
			tmax = ( aabb_max - ray_o[k][r] ) / ray_d[k][r];
			
			if ( tmin > tmax ) {
				/* Make sure tmin < tmax */
				float temp = tmin;
				tmin = tmax;
				tmax = temp;
				/* Ray direction is negative. Child nodes should be traversed in reverse order along this axis. */
				iter |= 4 >> k;
			}
			
			params[r].tmin[k] = tmin;
			params[r].tmax[k] = tmax;
		}
		
		rec_mask[r] = iter;
		out_mat[r] = 0;
		out_depth[r] = FLT_MAX;
	}
	
	traverse_branch( &oc->root, oc->root_level - oc_detail_level, ray_count, params, rec_mask, alive, out_mat, out_depth, out_nor );
	
	free( params );
}
