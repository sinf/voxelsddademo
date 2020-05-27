#ifndef _SPLAT_H
#define _SPLAT_H

/* todo:
- improve the interface to render buffers (render_core.c, render_threads.c)
- move the low-level raycasting loops into their own modules from render_core.c:render_part()
- eliminate global variables or group into structs
- simple rasterizer that can draw polygons, rectangles and circles (start with 32bit, add support for other bit depths later)
- treat voxels as cubes, project to screen space and draw as 4-6 sided polygons
- Splat the octree recursively in front-back order. Don't draw branch nodes, only depth test them. Just the leaf nodes need to be drawn
*/

#endif
