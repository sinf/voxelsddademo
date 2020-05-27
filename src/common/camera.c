#include <stdlib.h>
#include <string.h>
#include <xmmintrin.h>
#include "camera.h"
#include "render_core.h"

void rotate_camera( Camera *cam, float yaw, float pitch )
{
	cam->yaw += yaw;
	cam->pitch += pitch;
	cam->yaw = fmodf( cam->yaw, 2*M_PI );
	cam->pitch = clamp( cam->pitch, -M_PI*0.5, M_PI*0.5 );
}

void move_camera_local( Camera *cam, vec3f motion )
{
	vec3f temp;
	int n;
	
	multiply_vec_mat3f( temp, cam->eye_to_world, motion );
	
	for( n=0; n<PADDED_VEC3_SIZE; n++ )
		cam->pos[n] += temp[n];
}

void set_projection( Camera *c, float fovx, float aspect_ratio )
{
	c->fovx = clamp( fovx, radians(5), radians(175) );
	c->fovy = c->fovx / aspect_ratio;
}


static void expand_matrix( float b[16], const float a[9] )
{
	memcpy( b, a, 3 * sizeof *a );
	memcpy( b+4, a+3, 3 * sizeof *a );
	memcpy( b+8, a+6, 3 * sizeof *a );
	b[3] = b[7] = b[11] = b[15] = 0;
}

static void transpose_4x4( float out[16], const float in[16] )
{
	__m128 a, b, c, d;
	a = _mm_load_ps( in );
	b = _mm_load_ps( in + 4 );
	c = _mm_load_ps( in + 8 );
	d = _mm_load_ps( in + 12 );
	_MM_TRANSPOSE4_PS( a, b, c, d );
	_mm_store_ps( out, a );
	_mm_store_ps( out + 4, b );
	_mm_store_ps( out + 8, c );
	_mm_store_ps( out + 12, d );
}

static void make_frustum( float m[16], float r, float t, float n, float f )
{
	memset( m, 0, sizeof(float)*16 );
	m[0] = 1 / r;
	m[5] = 1 / t;
	m[10] = -2 / ( f - n );
	m[11] = ( n - f ) / ( f - n );
}
/*
0 1 2 3
4 5 6 7
8 9 10 11
12 13 14 15
*/

static void mult_mat4( float c[16], const float a[16], const float b[16] )
{
	int i, j, k;
	for( i=0; i<4; i++ ) {
		for( j=0; j<4; j++ ) {
			float d = 0;
			for( k=0; k<4; k++ ) {
				d += a[4*i+k] * b[j+4*k];
			}
			c[4*i+j] = d;
		}
	}
}

static void calc_mvp( Camera *camera )
{
	const float fix[] = {
		-1, 0, 0,
		0, -1, 0,
		0, 0, 1,
		0
	};
	float m0[9], m[16], f[16];
	float z;
	
	multiply_mat3f( m0, camera->eye_to_world, fix );
	expand_matrix( m, m0 );
	m[15] = 1;
	
	z = -calc_raydir_z( camera );
	make_frustum( f, screen_uv_min[0] / z, screen_uv_min[1] / z, 0, 10000 );
	
	mult_mat4( camera->mvp, m, f );
}

void update_camera_matrix( Camera *cam )
{
	const float yaw = cam->yaw;
	const float pitch = cam->pitch;
	
	const float cy = cosf( yaw );
	const float sy = sinf( yaw );
	const float cp = cosf( pitch );
	const float sp = sinf( pitch );
	
	float tolh[9] = {
		1, 0, 0,
		0, 1, 0,
		0, 0, 1
	};
	
	mat3f a;
	mat3f b;
	
	/* Rotation matrix for yaw */
	a[0] = cy;
	a[1] = 0.0f;
	a[2] = sy;
	
	a[3] = 0.0f;
	a[4] = 1.0f;
	a[5] = 0.0f;
	
	a[6] = -sy;
	a[7] = 0.0f;
	a[8] = cy;

	/* Rotation matrix for pitch */
	b[0] = 1.0f;
	b[1] = 0.0f;
	b[2] = 0.0f;
	
	b[3] = 0.0f;
	b[4] = cp;
	b[5] = sp;
	
	b[6] = 0.0f;
	b[7] = -sp;
	b[8] = cp;
	
	/* Combine yaw and pitch rotations */
	multiply_mat3f( cam->eye_to_world, a, b );
	
	transpose_mat3f( cam->world_to_eye, cam->eye_to_world );
	calc_mvp( cam );
}
