#include <stdlib.h>

static int traversal_func( const OctreeNode *parent, float tmin[3], float tmax[3], int level, unsigned rec_mask, Material_ID *out_m, float *out_z )
{
	float near = tmin[2];
	float far = tmax[2];
	unsigned n;
	
	for( n=0; n<2; n++ )
	{
		const float a = tmin[n];
		const float b = tmax[n];
		
		if ( a > near )
			near = a;
		
		if ( b < far )
			far = b;
	}
	
	if ( near > far )
		return 0;
	
	if ( far < 0.0f )
		return 0;
	
	if ( parent->children && level > 0 )
	{
		float tsplit[3];
		for( n=0; n<3; n++ )
			tsplit[n] = ( tmin[n] + tmax[n] ) * 0.5f;
		
		level--;
		
		for( n=0; n<8; n++ )
		{
			unsigned k = n ^ rec_mask;
			float a[3], b[3];
			int r;
			
			for( r=0; r<3; r++ )
			{
				if ( n & ( 4 >> r ) ) /* why n instead of k? */
				{
					a[r] = tsplit[r];
					b[r] = tmax[r];
				}
				else
				{
					a[r] = tmin[r];
					b[r] = tsplit[r];
				}
			}
			
			if ( traversal_func( parent->children+k, a, b, level, rec_mask, out_m, out_z, out_nor ) == 1 )
				return 1;
		}
	}
	else if ( parent->mat != 0 )
	{
		*out_z = near;
		*out_m = ( oc_show_travel_depth ) ? ( (Material_ID) level + 2 ) : parent->mat;
		return 1;
	}
	
	return 0;
}

void oc_traverse( const Octree *oc, const Ray *ray, Material_ID *out_m, float *out_z )
{
	int initial_level = oc->root_level; /* oc->root_level - max_recursion_level */
	float size = oc->size;
	float tmin[3], tmax[3];
	int n;
	unsigned mask = 0;
	
	for( n=0; n<3; n++ )
	{
		tmin[n] = -ray->o[n] / ray->d[n];
		tmax[n] = ( size - ray->o[n] ) / ray->d[n];
		
		if ( tmin[n] > tmax[n] )
		{
			/* Make sure tmin < tmax */
			float temp = tmin[n];
			tmin[n] = tmax[n];
			tmax[n] = temp;
			
			/* Ray direction is negative. Child nodes should be traversed in reverse order. */
			mask |= 4 >> n;
		}
	}
	
	*out_m = 0;
	*out_z = FLT_MAX;
	
	traversal_func( &oc->root, tmin, tmax, initial_level, mask, out_m, out_z );
}
