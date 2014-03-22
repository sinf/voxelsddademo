#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "trimesh.h"

static int char_count( const char *s, char c )
{
	int count = 0;
	while( *s != '\0' )
		count += ( *(s++) == c );
	return count;
}

Trimesh *load_wavefront_obj( const char filename[] )
{
	vec3f pos[MAX_MESH_VERTS];
	vec2f texc[MAX_MESH_VERTS];
	int n_pos = 0;
	int n_texc = 0;
	
	char *ext;
	char buf[256];
	FILE *file;
	Trimesh *mesh;
	
	printf( "Loading wavefront obj... %s\n", filename );
	file = fopen( filename, "r" );
	if ( !file )
	{
		printf( "Error: failed to open file\n" );
		abort();
	}
	
	mesh = calloc( 1, sizeof(Trimesh) );
	while( fgets( buf, sizeof(buf), file ) )
	{
		switch( buf[0] )
		{
			case 'v':
				switch( buf[1] )
				{
					case ' ':
						/* position (v) */
						sscanf( buf, "v %f %f %f", pos[n_pos], pos[n_pos]+1, pos[n_pos]+2 );
						n_pos++;
						break;
					
					case 't':
						/* texture coordinate (vt) */
						sscanf( buf, "vt %f %f %f", texc[n_texc], texc[n_texc]+1, texc[n_texc]+2 );
						n_texc++;
						break;
					
					default:
						/* garbage */
						break;
				}
				break;
			
			case 'f':
				if ( buf[1] == ' ' )
				{
					int slash_count = char_count( buf, '/' );
					int p[3], uv[3], n;
					
					if ( slash_count == 3 )
					{
						/* Triangle with UV coordinates */
						sscanf( buf, "f %d/%d %d/%d %d/%d", p, uv, p+1, uv+1, p+2, uv+2 );
					}
					else
					{
						/* UV not specified */
						sscanf( buf, "f %d %d %d", p, p+1, p+2 );
						uv[0] = uv[1] = uv[2] = 1;
					}
					
					for( n=0; n<3; n++ )
					{
						Vertex *v = mesh->vertices + mesh->num_verts;
						
						/* OBJ indexes begin with 1. ARGH! */
						p[n] -= 1;
						uv[n] -= 1;
						
						/* Clamp indexes to valid range */
						p[n] = clamp( p[n], 0, n_pos );
						uv[n] = clamp( uv[n], 0, n_texc );
						
						/* Copy from temporary buffers to mesh vertex */
						memcpy( v->pos, pos[p[n]], sizeof(vec3f) );
						memcpy( v->uv, texc[uv[n]], sizeof(vec2f) );
						
						mesh->num_verts++;
					}
				}
				/* Todo: support other face types? */
				break;
				
			default:
				/* unsupported feature */
				break;
		}
		
		if ( n_pos >= MAX_MESH_VERTS )
			break;
		
		if ( n_texc >= MAX_MESH_VERTS )
			break;
	}
	fclose( file );
	
	printf( "Vertices: %d/%d\n", mesh->num_verts, MAX_MESH_VERTS );
	printf( "Triangles: %d\n", mesh->num_verts / 3 );
	
	/* Load texture */
	strncpy( buf, filename, sizeof(buf)-4 );
	ext = strrchr( buf, '.' );
	if ( ext )
	{
		/* Replace .obj with .bmp */	
		strcpy( ext+1, "bmp" );
		mesh->tex = load_texture( buf, 0 );
	}
	
	return mesh;
}

void scale_mesh( Trimesh *m, double sx, double sy, double sz )
{
	int n;
	for( n=0; n<m->num_verts; n++ )
	{
		Vertex *vert = m->vertices + n;
		vert->pos[0] *= sx;
		vert->pos[1] *= sy;
		vert->pos[2] *= sz;
	}
}

static void create_mesh_vbo( Trimesh *m, GLenum usage_hint )
{
	GLuint vbo;
	
	glGenBuffers( 1, &vbo );
	glBindBuffer( GL_ARRAY_BUFFER, vbo );
	
	glBufferData( GL_ARRAY_BUFFER, sizeof(Vertex) * m->num_verts, m->vertices, usage_hint );
	
	glBindBuffer( GL_ARRAY_BUFFER, 0 );
	m->vbo = vbo;
}

static int num_verts_to_draw = 0;
void draw_mesh( void )
{
	glDrawArrays( GL_TRIANGLES, 0, num_verts_to_draw );
}

void select_mesh( Trimesh *m )
{
	if ( m )
	{
		const Vertex *vert = NULL;
		const void *pos_stride = &vert->pos;
		const void *uv_stride = &vert->uv;
		
		if ( m->vbo == 0 )
			create_mesh_vbo( m, GL_STATIC_DRAW );
		
		num_verts_to_draw = m->num_verts;
		glBindBuffer( GL_ARRAY_BUFFER, m->vbo );
	
		glEnableClientState( GL_VERTEX_ARRAY );
		glVertexPointer( 3, GL_FLOAT, sizeof(Vertex), pos_stride );
		
		glEnableClientState( GL_TEXTURE_COORD_ARRAY );
		glTexCoordPointer( 2, GL_FLOAT, sizeof(Vertex), uv_stride );
		
		glActiveTexture( GL_TEXTURE0 );
		glBindTexture( GL_TEXTURE_2D, m->tex->gl_tex_id );
	}
	else
	{
		glBindTexture( GL_TEXTURE_2D, 0 );
		
		glDisableClientState( GL_VERTEX_ARRAY );
		glDisableClientState( GL_TEXTURE_COORD_ARRAY );
		glBindBuffer( GL_ARRAY_BUFFER, 0 );
	}
}
