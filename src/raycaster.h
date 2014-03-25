#pragma once
#ifndef _RAYCASTER_H
#define _RAYCASTER_H

#include "camera.h"
#include "voxels.h"
#include "materials.h"

extern Octree *the_volume;
extern Camera camera;
extern uint8 *render_output_m;
extern float *render_output_z;
extern int enable_shadows;
extern unsigned render_resx, render_resy;
extern Material materials[NUM_MATERIALS];

/* Resizes render_output_... and cleans up the previous stuff. */
void resize_render_output( int w, int h, uint32 *output_rgba );

/* Starts N threads that are used for rendering.
	Any previous threads will be stopped and cleaned up.
	Note: A new thread is created even if N=1
	New threads are not created only when N<=0 */
void start_render_threads( int N );

/* Does nothing if no threads are running. */
void stop_render_threads( void );

/* This function updates render_output_... buffers
	using whatever rendering threads are running */
void render_volume( void );

/* Useful for shooting a single ray */
void get_primary_ray( Ray *ray, const Camera *c, int x, int y );

#endif
