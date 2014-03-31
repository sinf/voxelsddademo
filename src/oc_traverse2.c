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

#define fmin(x,y) ((x)<(y)?(x):(y))
#define fmax(x,y) ((x)>(y)?(x):(y))

static size_t intersect_with_aabb( uint32 *ray_ids, size_t num_rays,
	float const *ray_o[3], float const *ray_d[3],
	float const aabb_min[3], float const aabb_max[3], float out_depth[] )
{
	size_t r;
	for( r=0; r<num_rays; r++ )
	{
		int k;
		uint32 id = ray_ids[r];
		float tmaxs[3], tmins[3];
		float tmax, tmin;
		uint32 *last;
		
		if ( id + 1 )
		{
			for( k=0; k<3; k++ )
			{
				float o, d;
				float t0, t1;
				
				d = 1.0f / ray_d[k][id];
				o = ray_o[k][id];
				
				/* d = 1.0f / d; */
				t0 = ( aabb_min[k] - o ) * d;
				t1 = ( aabb_max[k] - o ) * d;
				
				tmin = fmin( t0, t1 );
				tmax = fmax( t0, t1 );
				
				tmins[k] = tmin;
				tmaxs[k] = tmax;
			}
			
			tmin = fmax( tmins[0], fmax( tmins[1], tmins[2] ) );
			tmax = fmin( tmaxs[0], fmin( tmaxs[1], tmaxs[2] ) );
			out_depth[id] = tmin;
			
			if ( tmax > 0.0f && tmin < tmax ) {
				/* The ray did not miss the box. Do not reject! */
				continue;
			}
		}
		
		/* The ray was terminated before or it missed the box, so reject it */
		last = --num_rays + ray_ids;
		ray_ids[r--] = *last;
		*last = id;
	}
	return num_rays;
}

static void process_leaf( const OctreeNode *node,
	int octree_level,
	uint32 *id_array, size_t num_rays,
	uint8 out_mat[] )
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
		size_t id = id_array[r];
		out_mat[id] = mat;
		id_array[r] = -1; /* kill this ray */
	}
}

static void traverse_branch( const OctreeNode *node,
	int octree_level,
	uint32 *id_array, size_t num_rays,
	float const *ray_o[3], float const *ray_d[3],
	uint8 out_mat[],
	float out_depth[],
	const int iter,
	float const aabb_min[3],
	float const aabb_max[3]
	)
{
	int m;
	
	octree_level--;
	
	for( m=0; m<8; m++ )
	{
		int child_id = m ^ iter;
		OctreeNode *child = node->children + child_id;
		float child_min[3], child_max[3];
		size_t subset_len;
		int k;
		
		for( k=0; k<3; k++ ) {
			float split = ( aabb_min[k] + aabb_max[k] ) * 0.5f;
			if ( child_id & ( 4 >> k ) ) {
				child_min[k] = split;
				child_max[k] = aabb_max[k];
			} else {
				child_min[k] = aabb_min[k];
				child_max[k] = split;
			}
		}
		
		subset_len = intersect_with_aabb( id_array, num_rays, ray_o, ray_d, child_min, child_max, out_depth );
		
		if ( !subset_len )
			continue;
		
		if ( !octree_level || !child->children )
			process_leaf( child, octree_level, id_array, subset_len, out_mat );
		else
			traverse_branch( child, octree_level, id_array, subset_len, ray_o, ray_d, out_mat, out_depth, iter, child_min, child_max );
	}
}

void oc_traverse_dac( const Octree oc[1],
	size_t ray_count,
	float const *ray_o[3],
	float const *ray_d[3],
	uint8 out_mat[],
	float out_depth[] )
{
	uint32 *id_arrays[8];
	size_t id_count[8] = {0};
	uint32 r;
	float aabb_min[3] = {0};
	float aabb_max[3];
	int t;
	
	if ( !ray_count )
		return;
	
	aabb_max[0] = aabb_max[1] = aabb_max[2] = oc->size;
	id_arrays[0] = aligned_alloc( 16, 8 * sizeof( uint32 ) * ray_count );
	
	for( t=1; t<8; t++ )
		id_arrays[t] = id_arrays[0] + t * ray_count;
	
	for( r=0; r<ray_count; r++ )
	{
		int k;
		uint32 iter = 0;
		
		out_mat[r] = 0;
		out_depth[r] = MAX_DEPTH_VALUE;
		
		for( k=0; k<3; k++ ) {
			if ( ray_d[k][r] < 0 ) {
				/* Ray direction is negative. Child nodes should be traversed in reverse order along this axis. */
				iter |= 4 >> k;
			}
		}
		
		id_arrays[iter][ id_count[iter]++ ] = r;
	}
	
	for( t=0; t<8; t++ ) {
		traverse_branch( &oc->root,
			oc->root_level - oc_detail_level + 1,
			id_arrays[t], id_count[t],
			ray_o, ray_d,
			out_mat, out_depth,
			t,
			aabb_min, aabb_max );
	}
}
