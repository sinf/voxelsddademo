#include <xmmintrin.h>
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

#define fmin(x,y) ((x)<(y)?(x):(y))
#define fmax(x,y) ((x)>(y)?(x):(y))
OverlapStatus aabb_sphere_overlap( aabb3f *box, const Sphere *sph )
{
	float minx, miny, minz, maxx, maxy, maxz;
	float px, py, pz;
	float dx, dy, dz;
	float ox, oy, oz;
	float sq;
	float r2;
	float fc;
	
	minx = box->min[0];
	miny = box->min[1];
	minz = box->min[2];
	
	maxx = box->max[0];
	maxy = box->max[1];
	maxz = box->max[2];
	
	ox = sph->o[0];
	oy = sph->o[1];
	oz = sph->o[2];
	r2 = sph->r;
	r2 *= r2;
	
	/* Distance to furthest corner */
	dx = fmax( ox - minx, maxx - ox );
	dy = fmax( oy - miny, maxy - oy );
	dz = fmax( oz - minz, maxz - oz );
	fc = dx*dx + dy*dy + dz*dz;
	
	/* Clamp sphere origin into the box */
	px = fmax( fmin( ox, maxx ), minx );
	py = fmax( fmin( oy, maxy ), miny );
	pz = fmax( fmin( oz, maxz ), minz );
	
	/* Compute squared distance */
	dx = px - ox;
	dy = py - oy;
	dz = pz - oz;
	sq = dx*dx + dy*dy + dz*dz;
	
	return ( sq < r2 ) << ( fc < r2 );
}
