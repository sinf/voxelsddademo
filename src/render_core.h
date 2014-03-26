#ifndef _RENDER_CORE_H
#define _RENDER_CORE_H
#include <stddef.h>

#include "camera.h"
#include "voxels.h"
#include "materials.h"

extern Octree *the_volume;
extern Camera camera;
extern volatile int enable_shadows;
extern volatile int show_normals;
extern Material materials[NUM_MATERIALS];
extern size_t render_resx, render_resy;

/* (Re)allocates memory. Restarts render threads */
void resize_render_output( int w, int h, uint32 *output_rgba );

/* Computes origin & direction of one primary ray. (x,y) are pixel coordinates */
void get_primary_ray( Ray *ray, const Camera *c, int x, int y );

/* Used by render_threads.c */
void render_part( size_t start_row, size_t end_row, float *ray_buffer );

#endif
