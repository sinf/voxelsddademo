#include <stdlib.h>
#include <string.h>
#include "camera.h"

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

void update_camera_matrix( Camera *cam )
{
	const float yaw = cam->yaw;
	const float pitch = cam->pitch;
	
	const float cy = cosf( yaw );
	const float sy = sinf( yaw );
	const float cp = cosf( pitch );
	const float sp = sinf( pitch );
	
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
	
	/* asd s */
	transpose_mat3f( cam->world_to_eye, cam->eye_to_world );
}
