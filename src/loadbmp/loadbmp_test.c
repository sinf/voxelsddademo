#include <stdlib.h>
#include <stdio.h>
#include <SDL/SDL.h>
#include "loadbmp.h"

SDL_Surface *load_bmp_surf( const char *filename, int flags )
{
	int errcode;
	int w, h;
	uint32_t *pixels;
	SDL_Surface *surf;
	SDL_Surface *surf2;
	Uint32 rmask, gmask, bmask, amask;
	int row;
	
	errcode = load_bmp( filename, &pixels, &w, &h, flags );
	if ( errcode > 0 )
	{
		printf( "ERROR: %s\n", BMP_ERROR_MESSAGES[errcode] );
		return NULL;
	}
	
	amask = 0xFF000000;
	rmask = 0x00FF0000;
	gmask = 0x0000FF00;
	bmask = 0x000000FF;
	
	surf = SDL_CreateRGBSurface( 0, w, h, 32, rmask, gmask, bmask, 0 );
	SDL_LockSurface( surf );
	
	for( row=0; row<h; row++ )
		memcpy( (char*) surf->pixels + row * surf->pitch, pixels + row * w, w << 2 );
	
	free( pixels );
	SDL_UnlockSurface( surf );
	surf2 = SDL_DisplayFormat( surf );
	if ( surf2 )
	{
		SDL_FreeSurface( surf );
		surf = surf2;
	}
	
	return surf;
}

int main( int argc, char **argv )
{
	char *img_file;
	SDL_Surface *image;
	SDL_Surface *screen;
	SDL_Event event;
	int running;
	uint32_t flags = 0;
	
	if ( argc < 2 )
	{
		puts( "No loadable image specified" );
		return 1;
	}
	
	for( argc--; argc>1; argc-- )
		flags |= strtol( argv[argc], NULL, 0 );
	
	img_file = argv[1];
	printf( "Flags: 0x%08x\n", flags );
	printf( "Image: %s\n", img_file );
	
	if ( SDL_Init(SDL_INIT_VIDEO) < 0 )
		return 2;
	
	puts( "Loading..." );
	image = load_bmp_surf( img_file, flags );
	if ( !image )
		return 3;
	
	screen = SDL_SetVideoMode( image->w, image->h, 32, 0 );
	if ( !screen )
		return 4;
	
	SDL_BlitSurface( image, NULL, screen, NULL );
	SDL_Flip( screen );
	running = 1;
	
	while( running )
	{
		SDL_WaitEvent( &event );
		switch( event.type )
		{
			case SDL_QUIT:
				running = 0;
				break;
			
			case SDL_VIDEOEXPOSE:
				SDL_BlitSurface( image, NULL, screen, NULL );
				SDL_Flip( screen );
				break;
			
			default:
				break;
		}
	}
	
	SDL_Quit();
	return 0;
}
