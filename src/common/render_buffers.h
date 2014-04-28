#ifndef _RENDER_BUFFERS_H
#define _RENDER_BUFFERS_H
#include <stddef.h>
#include "types.h"

/* Dimensions of the buffers. render_resx is always a multiple of 16 */
extern size_t render_resx, render_resy;

/* Used by render_core.c */
extern uint8 *render_output_m; /* materials */
extern float *render_output_z; /* ray depth (distance to first intersection) */

/* Pixel buffers. The pointers are aligned to 16 bytes  */
extern uint32 *render_output_write; /* Write-mostly. This is the "back buffer" */
extern uint32 *render_output_rgba; /* Read-only. This is the "front buffer" */

/* Interchanges the 2 pointers above */
void swap_render_buffers( void );

/* (Re)Allocates memory. Deallocates memory if w*h == 0. w must be a multiple of 16. Should only be called by resize_render_output()
Returns zero on failure, nonzero on success. Also returns zero if w==0 or h==0 */
int resize_render_buffers( size_t w, size_t h );

#endif
