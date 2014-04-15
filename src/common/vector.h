#pragma once
#ifndef _VEC3_H
#define _VEC3_H
#include <math.h>

typedef float vec2f[2];
typedef int vec2i[2];

#if 1
	typedef float vec3f[4];
	typedef int vec3i[4];
	#define PADDED_VEC3_SIZE 4
#else
	typedef float vec3f[3];
	typedef int vec3i[3];
	#define PADDED_VEC3_SIZE 3
#endif

typedef float vec4f[4];
typedef int vec4i[4];

typedef float mat3f[9];
typedef float mat4f[16];

#if 0
static __inline float inv_sqrtf( float x )
{
	float xhalf = 0.5f*x;
	int i = *(int*)&x;
	i = 0x5f3759df - (i>>1);
	x = *(float*)&i;
	return x*(1.5f - xhalf*x*x);
}
#else
#define inv_sqrtf(x) (1.0f/sqrtf(x))
#endif

/* Converts degrees to radians */
#define radians(deg) ((deg)*0.0174532925f)

/* Converts radians to degrees */
#define degrees(rad) ((rad)*57.2957795f)

#define min(x,y) ( (x) < (y) ? (x) : (y) )
#define max(x,y) ( (x) > (y) ? (x) : (y) )
#define clamp(x,low,high) max((low), min((x),(high)))

/* Vector-Scalar macros */
#define vs_add(out,vec,s) do { out[0]=(vec)[0]+(s); out[1]=(vec)[1]+(s); out[2]=(vec)[2]+(s); } while(0)
#define vs_mul(out,vec,s) do { out[0]=(vec)[0]*(s); out[1]=(vec)[1]*(s); out[2]=(vec)[2]*(s); } while(0)
#define vs_div(out,vec,s) do { out[0]=(vec)[0]/(s); out[1]=(vec)[1]/(s); out[2]=(vec)[2]/(s); } while(0)

/* Vector-Vector macros */
#define dot_product( vec0, vec1 ) \
	((vec0)[0]*(vec1)[0] + (vec0)[1]*(vec1)[1] + (vec0)[2]*(vec1)[2])

#define normalize(vec) do { \
	float len = inv_sqrtf( (vec)[0]*(vec)[0] + (vec)[1]*(vec)[1] + (vec)[2]*(vec)[2] ); \
	vs_mul( vec, vec, len ); \
} while(0)

/* Matrix operations */
#define multiply_mat3f( out, mat0, mat1 ) do { \
	int n; \
	for( n=0; n<3; n++ ) \
	{ \
		const float *c = (mat1) + 3*n; \
		out[n] = dot_product( mat0, c ); \
		out[3+n] = dot_product( (mat0)+3, c ); \
		out[6+n] = dot_product( (mat0)+6, c ); \
	} \
} while(0)

#define multiply_vec_mat3f( out, mat, vec ) do { \
	int n; \
	vec3f vec0_copy; \
	memcpy( vec0_copy, (vec), sizeof(vec3f) ); \
	for( n=0; n<3; n++ ) \
		(out)[n] = dot_product( (mat)+3*n, vec0_copy ); \
} while(0)

#define transpose_mat3f( out, mat ) do { \
	int a, b; \
	for( a=0; a<3; a++ ) \
	{ \
		for( b=0; b<3; b++ ) \
			(out)[a*3+b] = (mat)[b*3+a]; \
	} \
} while(0)

#endif
