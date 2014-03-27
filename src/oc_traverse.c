#include <stdlib.h>
#include <float.h>
#include <xmmintrin.h>
#include "voxels.h"

#define ALLOW_DEBUG_VISUALS 1
#define OCTREE_DEPTH_HARDLIMIT 30
int oc_show_travel_depth = 0;
int oc_detail_level = 0;

#if 0
#include "octree_traversal_test.h"
#else

typedef int fast_int;
typedef unsigned fast_uint;

void oc_traverse( const Octree *oc, const Ray *ray, Material_ID *out_m, float *out_z, float *out_nor[3] )
{
	struct {
		const OctreeNode *node;
		fast_uint n;
		_MM_ALIGN16 vec3f tmin;
		_MM_ALIGN16 vec3f tmax;
		_MM_ALIGN16 vec3f tsplit;
	} stack[OCTREE_DEPTH_HARDLIMIT+2];
	
	fast_uint rec_mask = 0; /* child traversal order */
	fast_uint s; /* current stack level */
	fast_int r; /* loop counter */
	fast_uint max_level = oc->root_level - oc_detail_level;
	
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
	
	#define POP_STACK { s--; continue; }
	
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
			fast_uint n, k;
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
					fast_int level = s;
					*out_m = ( oc_show_travel_depth ) ? ( (Material_ID) (level + 2) ) : parent->mat;
				#else
					*out_m = parent->mat;
				#endif
				
				*out_z = near;
				
				if ( out_nor ) {
					#if 0
					unsigned cx = ray->o[0] + near * ray->d[0] + .5f;
					unsigned cy = ray->o[1] + near * ray->d[1] + .5f;
					unsigned cz = ray->o[2] + near * ray->d[2] + .5f;
					cx %= oc->size;
					cy %= oc->size;
					cz %= oc->size;
					get_voxel_normal( oc, cx, cy, cz, out_nor[0], out_nor[1], out_nor[2] );
					#endif
					/**
					*out_nor[0] = parent->nor[0];
					*out_nor[1] = parent->nor[1];
					*out_nor[2] = parent->nor[2];
					**/
				}
				
				return;
			}
			
			POP_STACK;
		}
	}
	
	*out_m = 0;
	*out_z = FLT_MAX;
	
	if ( out_nor ) {
		*out_nor[0] = *out_nor[1] = *out_nor[2] = 0;
	}
}

#endif
