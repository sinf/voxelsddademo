#ifndef _RENDER_CORE_H
#define _RENDER_CORE_H
#include <stddef.h>

#include "camera.h"
#include "voxels.h"
#include "materials.h"

#define MAX_DEPTH_VALUE 100000

#define NUM_AO_SAMPLES 20
#define AO_FALLOFF 0.2f

#define ENABLE_RAYCAST 1
#define ENABLE_GAMMA_CORRECTION 1 /* enables/disables 3 sqrts per pixel */
/* #define ENABLE_SPECULAR_TERM 0 */
#define THE_GAMMA_VALUE 2.0f

extern int show_normals;
extern int show_depth_buffer;
extern int enable_shadows;
extern int enable_phong;
extern int enable_aoccl;
extern int enable_dac_method;

extern uint32 materials_rgb[NUM_MATERIALS]; /* rgb colors (any pixel format is ok) */
extern float materials_diff[NUM_MATERIALS][4]; /* rgb diffuse reflection constants. last component is padding */
extern float materials_spec[NUM_MATERIALS][4]; /* rgb specular color & exponent */ 

void set_light_pos( float x, float y, float z );

/* (Re)allocates memory. Restarts render threads */
void resize_render_output( int w, int h );

/* Computes origin & direction of one primary ray. (x,y) are pixel coordinates */
void get_primary_ray( Ray *ray, const Camera *c, const Octree *volume, int x, int y );

/* Used by render_threads.c */
#define RENDER_THREAD_MEM_PER_PIXEL (6*sizeof(float)) /* <- ray_buffer gets allocated based on this value */
void render_part( const Camera *camera, Octree *volume, size_t start_row, size_t end_row, float *ray_buffer );

/* Makes render_output_rgba point to the last frame. The next frame will be rendered into another buffer */
void swap_render_buffers( void );

/* Ray traversal function. see oc_traverse.c. For infinitely long rays, pass NAN as max_ray_depth. Returns ray depth (or max_ray_depth) */
float oc_traverse( const Octree *oc, uint8 *output_mat, float ox, float oy, float oz, float dx, float dy, float dz, float max_ray_depth );

#endif
