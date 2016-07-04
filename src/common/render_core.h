#ifndef _RENDER_CORE_H
#define _RENDER_CORE_H
#include <stddef.h>

#include "camera.h"
#include "voxels.h"
#include "materials.h"

#define NUM_AO_SAMPLES 32 /* must be a multiple of 4 */
#define AO_FALLOFF 0.35f /* fraction of total volume size */

#define ENABLE_RAYCAST 1 /* Set to zero to disable actual graphics. Useful for profiling/optimizing higher level loops & threads */
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

extern float calc_raydir_z( const Camera * );
extern float screen_uv_min[2];

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

/* Divide-And-Conquer version. Didn't turn out to be fast at all.
see oc_traverse2.c */
void oc_traverse_dac( const Octree oc[1],
	size_t ray_count,
	float const *ray_o[3],
	float const *ray_d[3],
	uint8 out_mat[],
	float out_depth[] );


void project_world_to_screen( float scr[2], const Camera *c, float px, float py, float pz, float res_x, float res_y );

#endif
