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

static const float missed = -1.0f;

static float traversal_func( const OctreeNode *parent, uint8 *out_m, float *out_z, int level, unsigned rec_mask,
float tminx, float tminy, float tminz, float tmaxx, float tmaxy, float tmaxz, float max_ray_depth )
{
	float near, far;
	unsigned n;
	
	near = MAX( MAX( tminx, tminy ), tminz );
	far = MIN( MIN( tmaxx, tmaxy ), tmaxz );
	
	if ( near > far )
		return missed;
	
	if ( far < 0.0f )
		return missed;
	
	if ( near > max_ray_depth )
		return missed;
	
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
			float hit_depth;
			
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
			
			hit_depth = traversal_func( parent->children+k, out_m, out_z, level, rec_mask, a[0], a[1], a[2], b[0], b[1], b[2], max_ray_depth );
			
			if ( hit_depth != missed )
				return hit_depth;
		}
	}
	else if ( parent->mat )
	{
		*out_m = ( ALLOW_DEBUG_VISUALS && oc_show_travel_depth ) ? ( level + 2 & MATERIAL_BITMASK ) : parent->mat;
		return near;
	}
	
	return missed;
}

float oc_traverse( const Octree *oc, uint8 *out_m, float ray_ox, float ray_oy, float ray_oz, float ray_dx, float ray_dy, float ray_dz, float max_ray_depth )
{
	int initial_level = oc->root_level - oc_detail_level;
	float size = oc->size;
	float tmin[3], tmax[3];
	unsigned mask = 0;
	float t0, t1, invd;
	float out_z;
	
	/* The same thing:
	mask |= ( 4 & (*(unsigned*)&d) >> 29 ) >> n;
	mask |= ( 4 >> n ) * ( d < 0 );
	*/
	
	#define compute_interval(n,x,d) do { \
		/* Set the bit if direction is negative. Child nodes should then be traversed in reverse order. */ \
		mask |= ( 4 >> n ) * ( d < 0 ); \
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
	out_z = traversal_func( &oc->root, out_m, &out_z, initial_level, mask, tmin[0], tmin[1], tmin[2], tmax[0], tmax[1], tmax[2], max_ray_depth );
	return out_z == missed ? max_ray_depth : out_z;
}
