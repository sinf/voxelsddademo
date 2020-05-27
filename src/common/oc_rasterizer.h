#ifndef _OCTREE_RASTERIZER_H
#define _OCTREE_RASTERIZER_H

struct Octree;
struct Camera;
struct SDL_Surface;

void rasterize_octree( struct Octree *tree, struct Camera *camera, struct SDL_Surface *screen );

#endif
