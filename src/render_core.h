#ifndef _RENDER_CORE_H
#define _RENDER_CORE_H
#include <stddef.h>

#include "camera.h"
#include "voxels.h"
#include "materials.h"

#define MAX_DEPTH_VALUE 100000

#define ENABLE_DAC 1

#define ENABLE_RAYCAST 1
#define ENABLE_GAMMA_CORRECTION 0 /* enables/disables 3 sqrts per pixel */
/* #define ENABLE_SPECULAR_TERM 0 */
#define THE_GAMMA_VALUE 2.0f

/* todo: try to get rid of these globals */
extern Octree *the_volume;
extern Camera camera;
extern int enable_shadows;
extern int show_normals;
extern int enable_phong;

extern uint32 materials_rgb[NUM_MATERIALS]; /* rgb colors (any pixel format is ok) */
extern float materials_diff[NUM_MATERIALS][4]; /* rgb diffuse reflection constants. last component is padding */
extern float materials_spec[NUM_MATERIALS][4]; /* rgb specular color & exponent */ 

extern size_t render_resx, render_resy;
extern uint32 *render_output_rgba;

void set_light_pos( float x, float y, float z );

/* (Re)allocates memory. Restarts render threads */
void resize_render_output( int w, int h );

/* Computes origin & direction of one primary ray. (x,y) are pixel coordinates */
void get_primary_ray( Ray *ray, const Camera *c, int x, int y );

/* Used by render_threads.c */
#define RENDER_THREAD_MEM_PER_PIXEL (6*sizeof(float)) /* <- ray_buffer gets allocated based on this value */
void render_part( size_t start_row, size_t end_row, float *ray_buffer );

/* Makes render_output_rgba point to the last frame. The next frame will be rendered into another buffer */
void swap_render_buffers( void );

#endif
