#include <stdlib.h>
#include <xmmintrin.h>
#include "voxels.h"
#include "types.h"
#include "render_core.h"

#define ALLOW_DEBUG_VISUALS 1
#define OCTREE_DEPTH_HARDLIMIT 30
int oc_show_travel_depth = 0;
int oc_detail_level = 0;

#if 0
#include "octree_traversal_test.h"
#else

typedef unsigned uint;

void oc_traverse( const Octree *oc, const Ray *ray, uint8 *out_m, float *out_z )
{
	struct {
		const OctreeNode *node;
		uint n;
		_MM_ALIGN16 vec3f tmin;
		_MM_ALIGN16 vec3f tmax;
		_MM_ALIGN16 vec3f tsplit;
	} stack[OCTREE_DEPTH_HARDLIMIT+2];
	
	uint rec_mask = 0; /* child traversal order */
	uint s; /* current stack level */
	int r; /* loop counter */
	uint max_level = oc->root_level - oc_detail_level;
	/** uint64 voxel_index = 0; **/
	
	/* Calculate ray intersection with root node */
	stack[0].node = &oc->root;
	stack[0].n = 0;
	for( r=0; r<3; r++ )
	{
		float *tmin = stack[0].tmin + r;
		float *tmax = stack[0].tmax + r;
		float *tsplit = stack[0].tsplit + r;
		float inv_dir = 1.0f / ray->d[r];
		
		*tmin = ( 0.0f - ray->o[r] ) * inv_dir;
		*tmax = ( oc->size - ray->o[r] ) * inv_dir;
		*tsplit = ( *tmin + *tmax ) * 0.5f;
		
		if ( *tmin > *tmax )
		{
			/* Make sure tmin < tmax */
			float temp = *tmin;
			*tmin = *tmax;
			*tmax = temp;
			
			/* Ray direction is negative. Child nodes should be traversed in reverse order along this axis. */
			rec_mask |= 4 >> r;
		}
	}
	
	#define POP_STACK { s--; /* voxel_index>>=3; */ continue; }
	
	/* When s=0 and gets subtracted it will overflow to INT_MAX and loop terminates */
	for( s=0; s<OCTREE_DEPTH_HARDLIMIT; )
	{
		const OctreeNode *parent = stack[s].node;
		const float *tmin = stack[s].tmin;
		const float *tmax = stack[s].tmax;
		
		float near = tmin[2];
		float far = tmax[2];
		
		for( r=0; r<2; r++ )
		{
			const float a = tmin[r];
			const float b = tmax[r];
			
			if ( a > near )
				near = a;
			
			if ( b < far )
				far = b;
		}
		
		if ( near > far || far < 0.0f )
			POP_STACK;
		
		if ( parent->children && ( !ALLOW_DEBUG_VISUALS || s < max_level ) )
		{
			uint n, k;
			const float *tsplit;
			
			n = stack[s].n;
			
			if ( n == 8 )
				POP_STACK;
			
			k = n ^ rec_mask;
			tsplit = stack[s].tsplit;
			stack[s].n = n + 1;
			
			s += 1;
			stack[s].node = parent->children + k;
			stack[s].n = 0;
			
			/*
			voxel_index = voxel_index << 3 | k;
			*/
			
			for( r=0; r<3; r++ )
			{
				float *a = stack[s].tmin;
				float *b = stack[s].tmax;
				
				if ( n & ( 4 >> r ) )
				{
					a[r] = tsplit[r];
					b[r] = tmax[r];
				}
				else
				{
					a[r] = tmin[r];
					b[r] = tsplit[r];
				}
				
				stack[s].tsplit[r] = ( a[r] + b[r] ) * 0.5f;
			}
		}
		else
		{
			if ( parent->mat != 0 )
			{
				#if ALLOW_DEBUG_VISUALS
					int level = s;
					*out_m = ( oc_show_travel_depth ) ? ( level + 2 ) : parent->mat;
				#else
					*out_m = parent->mat;
				#endif
				
				*out_z = near;
				return;
			}
			
			POP_STACK;
		}
	}
	
	*out_m = 0;
	*out_z = MAX_DEPTH_VALUE;
}

#endif
