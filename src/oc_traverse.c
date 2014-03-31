#include <stdlib.h>
#include <xmmintrin.h>
#include "voxels.h"
#include "types.h"
#include "render_core.h"

#define ALLOW_DEBUG_VISUALS 1
int oc_show_travel_depth = 0;
int oc_detail_level = 0;

#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAX(a,b) ((a)>(b)?(a):(b))

static int traversal_func( const OctreeNode *parent, uint8 *out_m, float *out_z, int level, unsigned rec_mask, float tminx, float tminy, float tminz, float tmaxx, float tmaxy, float tmaxz )
{
	float near, far;
	unsigned n;
	
	near = MAX( MAX( tminx, tminy ), tminz );
	far = MIN( MIN( tmaxx, tmaxy ), tmaxz );
	
	if ( near > far )
		return 0;
	
	if ( far < 0.0f )
		return 0;
	
	if ( parent->children && level > 0 )
	{
		float tsplitx, tsplity, tsplitz;
		
		tsplitx = ( tminx + tmaxx ) * 0.5f;
		tsplity = ( tminy + tmaxy ) * 0.5f;
		tsplitz = ( tminz + tmaxz ) * 0.5f;
		
		level--;
		
		for( n=0; n<8; n++ )
		{
			unsigned k = n ^ rec_mask;
			float a[3], b[3];
			
			#define get_child_interval(r,split,lo,hi) \
				if ( n & ( 4 >> r ) ) { \
					a[r] = split; \
					b[r] = hi; \
				} else { \
					a[r] = lo; \
					b[r] = split; \
				}
			
			get_child_interval( 0, tsplitx, tminx, tmaxx );
			get_child_interval( 1, tsplity, tminy, tmaxy );
			get_child_interval( 2, tsplitz, tminz, tmaxz );
			
			if ( traversal_func( parent->children+k, out_m, out_z, level, rec_mask, a[0], a[1], a[2], b[0], b[1], b[2] ) )
				return 1;
		}
	}
	else if ( parent->mat )
	{
		*out_z = near;
		*out_m = ( ALLOW_DEBUG_VISUALS && oc_show_travel_depth ) ? ( level + 2 & MATERIAL_BITMASK ) : parent->mat;
		return 1;
	}
	
	return 0;
}

void oc_traverse( const Octree *oc, uint8 *out_m, float *out_z, float ray_ox, float ray_oy, float ray_oz, float ray_dx, float ray_dy, float ray_dz )
{
	int initial_level = oc->root_level; /* oc->root_level - max_recursion_level */
	float size = oc->size;
	float tmin[3], tmax[3];
	unsigned mask = 0;
	float t0, t1, invd;
	
	#define compute_interval(n,x,d) do { \
		/* Set the bit if direction is negative. Child nodes should then be traversed in reverse order. */ \
		mask |= ( 4 & (*(unsigned*)&d) >> 29 ) >> n; \
		/* Compute intervals */ \
		invd = 1.0f / d; \
		t0 = -x * invd; \
		t1 = ( size - x ) * invd; \
		tmin[n] = MIN( t0, t1 ); \
		tmax[n] = MAX( t0, t1 ); \
	} while(0)
	
	compute_interval( 0, ray_ox, ray_dx );
	compute_interval( 1, ray_oy, ray_dy );
	compute_interval( 2, ray_oz, ray_dz );
	
	*out_m = 0;
	*out_z = MAX_DEPTH_VALUE;
	
	traversal_func( &oc->root, out_m, out_z, initial_level, mask, tmin[0], tmin[1], tmin[2], tmax[0], tmax[1], tmax[2] );
}
