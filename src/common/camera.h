#pragma once
#ifndef _CAMERA_H
#define _CAMERA_H
#include "vector.h"
#include "types.h"

#define ZNEAR 0.01
#define ZFAR 50.0

typedef struct Camera {
	/* World coordinates */
	_MM_ALIGN16 vec3f pos;
	
	/* Radians: */
	float yaw, pitch;
	float fovx;
	float fovy;
	
	_MM_ALIGN16 mat3f eye_to_world; /* Used for software ray casting */
	_MM_ALIGN16 mat3f world_to_eye; /* Modelview */
	_MM_ALIGN16 mat4f eye_to_view;
} Camera;

/* Simply rotates yaw & pitch and makes sure the values stay in proper range. */
void rotate_camera( Camera *cam, float delta_yaw, float delta_pitch );

/* Moves camera relative to itself */
void move_camera_local( Camera *, vec3f );

/* Updates fovx and projection matrix (=eye_to_ndc).
Notes:
	-fovx is in radians
	-fovx will be clamped to a sensible range
	-aspect_ratio is screenWidth / screenHeight
*/
void set_projection( Camera *, float fovx, float aspect_ratio );

/* Updates world_to_eye and eye_to_world */
void update_camera_matrix( Camera * );

#endif
