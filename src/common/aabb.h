#pragma once
#ifndef _AABB_H
#define _AABB_H
#include "types.h"
#include "vector.h"
#include "ray.h"

typedef struct aabb3i
{
	int min[4];
	int max[4];
} aabb3i;

typedef struct aabb3f {
	/* 4th component must be always zero */
	float min[4];
	float max[4];
} aabb3f;

extern const vec3f AABB_NORMALS[6];
enum {
	AABB_NOR_X=0,
	AABB_NOR_X_NEG,
	AABB_NOR_Y,
	AABB_NOR_Y_NEG,
	AABB_NOR_Z,
	AABB_NOR_Z_NEG
};

/* None of the arguments must be NULL: */
int intersect_aabb( const Ray *ray, const aabb3f *aabb, float *enter, float *exit );

/* Any of enter, enter_n, exit and exit_n can be NULL: */
int intersect_aabb_nor( const Ray *ray, const aabb3f *aabb, float *enter, int *enter_n, float *exit, int *exit_n );

typedef struct Sphere {
	float o[3];
	float r;
} Sphere;

typedef enum {
	NO_TOUCH=0,
	OVERLAP,
	INSIDE /* a inside b */
} OverlapStatus;

OverlapStatus aabb_aabb_overlap( const aabb3f * restrict a, const aabb3f * restrict b );

/* Sets 4th components of bounding box to 0 */
OverlapStatus aabb_sphere_overlap( aabb3f *a, const Sphere *b );

#endif
