#ifndef _RENDER_CORE_H
#define _RENDER_CORE_H
#include <stddef.h>

#include "camera.h"
#include "voxels.h"
#include "materials.h"

/* todo: try to get rid of these globals */
extern Octree *the_volume;
extern Camera camera;
extern int enable_shadows;
extern int show_normals;
extern int enable_phong;
extern Material materials[NUM_MATERIALS];
extern size_t render_resx, render_resy;
extern uint32 *render_output_rgba;

void set_light_pos( float x, float y, float z );

/* (Re)allocates memory. Restarts render threads */
void resize_render_output( int w, int h );

/* Computes origin & direction of one primary ray. (x,y) are pixel coordinates */
void get_primary_ray( Ray *ray, const Camera *c, int x, int y );

/* Used by render_threads.c */
void render_part( size_t start_row, size_t end_row, float *ray_buffer );

/* Makes render_output_rgba point to the last frame. The next frame will be rendered into another buffer */
void swap_render_buffers( void );

#endif
