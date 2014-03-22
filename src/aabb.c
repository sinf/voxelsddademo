#include <float.h>
#include "aabb.h"

int intersect_aabb( const Ray *ray, const aabb3f *aabb, float *enter, float *exit )
{
	float tmin = -FLT_MAX;
	float tmax = FLT_MAX;
	int k;
	
	for( k=0; k<3; k++ )
	{
		float t1, t2;
		
		if ( fabsf(ray->d[k]) < 0.0001f )
		{
			if ( ray->o[k] < aabb->min[k] )
				return 0;
			
			if ( ray->o[k] > aabb->max[k] )
				return 0;
			
			continue;
		}
		
		t2 = 1.0f / ray->d[k];
		t1 = ( aabb->min[k] - ray->o[k] ) * t2;
		t2 = ( aabb->max[k] - ray->o[k] ) * t2;
		
		if ( t1 > t2 )
		{
			float temp = t2;
			t2 = t1;
			t1 = temp;
		}
		
		if ( t1 > tmin )
			tmin = t1;
		
		if ( t2 < tmax )
			tmax = t2;
		
		if ( tmin > tmax )
			return 0;
		
		if ( tmax < 0.0f )
			return 0;
	}
	
	*enter = tmin;
	*exit = tmax;
	return 1;
}

const vec3f AABB_NORMALS[6] = {
	{1.0f, 0.0f, 0.0f},
	{-1.0f, 0.0f, 0.0f},
	{0.0f, 1.0f, 0.0f},
	{0.0f, -1.0f, 0.0f},
	{0.0f, 0.0f, 1.0f},
	{0.0f, 0.0f, -1.0f},
};

int intersect_aabb_nor( const Ray *ray, const aabb3f *aabb, float *enter, int *enter_n, float *exit, int *exit_n )
{
	float tmin = -FLT_MAX;
	float tmax = FLT_MAX;
	int k;
	
	int dummy_n;
	if ( !enter_n ) enter_n = &dummy_n;
	if ( !exit_n ) exit_n = &dummy_n;
	
	for( k=0; k<3; k++ )
	{
		float t1, t2;
		int face;
		
		if ( fabsf(ray->d[k]) < 0.0001f )
		{
			/* The ray is parallel with the plane */
			if ( ray->o[k] < aabb->min[k] )
				return 0;
			
			if ( ray->o[k] > aabb->max[k] )
				return 0;
			
			/* Can only intersect with the other planes */
			continue;
		}
		
		/* Calculate intersections with the 2 slabs */
		t2 = 1.0f / ray->d[k];
		t1 = ( aabb->min[k] - ray->o[k] ) * t2;
		t2 = ( aabb->max[k] - ray->o[k] ) * t2;
		
		if ( t1 > t2 )
		{
			/* Make sure t1 is less than t2 */
			float temp = t2;
			t2 = t1;
			t1 = temp;
			
			/* Intersecting from a positive side */
			face = 0;
		}
		else
		{
			/* Intersecting from a negative side */
			face = 1;
		}
		
		if ( t1 > tmin )
		{
			/* Want largest tmin */
			tmin = t1;
			*enter_n = (k<<1) + face;
		}
		
		if ( t2 < tmax )
		{
			/* Want smallest tmax */
			tmax = t2;
			*exit_n = (k<<1) + !(face);
		}
		
		if ( tmin > tmax )
			return 0;
		
		if ( tmax < 0.0f )
			return 0;
	}
	
	if ( enter ) *enter = tmin;
	if ( exit ) *exit = tmax;
	
	return 1;
}

OverlapStatus aabb_aabb_overlap( const aabb3f *a, const aabb3f *b )
{
	int inside = 1;
	int n;
	
	for( n=0; n<3; n++ )
	{
		if ( a->max[n] <= b->min[n] || a->min[n] >= b->max[n] )
			return NO_TOUCH;
		
		if ( a->min[n] < b->min[n] || a->max[n] > b->max[n] )
			inside = 0;
	}
	
	if ( inside )
		return INSIDE;
	
	return OVERLAP;
}

#include <xmmintrin.h>
#define clamp_ps(x,min,max) _mm_max_ps( _mm_min_ps((x),(max)), (min) )

OverlapStatus aabb_sphere_overlap( aabb3f *box, const Sphere *sph )
{
	float dist = 0.0f;
	int n;
	
	box->min[3] = box->max[3] = 0.0f;
	
	#if PADDED_VEC3_SIZE == 4
	{
		/* 179 instructions */
		const __m128 o = _mm_load_ps( sph->o );
		const __m128 min = _mm_load_ps( box->min );
		const __m128 max = _mm_load_ps( box->max );
		float temp[4];
		__m128 m;
		
		m = clamp_ps( o, min, max );
		m = _mm_sub_ps( o, m );
		m = _mm_mul_ps( m, m );
		
		_mm_store_ps( temp, m );
		
		for( n=0; n<4; n++ )
			dist += temp[n];
	}
	#else
	for( n=0; n<3; n++ )
	{
		/* 218 instructions */
		float temp;
		
		temp = clamp( sph->o[n], box->min[n], box->max[n] );
		temp = sph->o[n] - temp;
		
		dist += temp * temp;
	}
	#endif
	
	dist = sqrtf( dist );
	if ( dist < sph->r )
	{
		float box_r = FLT_MAX;
		
		for( n=0; n<PADDED_VEC3_SIZE; n++ )
		{
			/* GCC manages to vectorize this without help of insintrics */
			float s = box->max[n] - box->min[n];
			box_r = ( s > box_r ) ? s : box_r;
		}
		
		box_r = box_r * sqrtf( 3.0f );
		
		/*
		Above could be written as:
			box_r = sqrtf( box_r * box_r * 3.0f );
		
		box_r is now radius of the AABB's bounding sphere
		*/
		
		if ( (dist+box_r) < sph->r )
			return INSIDE;
		
		return OVERLAP;
	}
	
	return NO_TOUCH;
}
