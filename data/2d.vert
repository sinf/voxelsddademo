#version 120

uniform ivec2 resolution;

void main( void )
{
	vec4 vert = gl_Vertex;
	vert.y = resolution.y - vert.y;
	
	gl_Position = vec4(
		( vert.xy / resolution - vec2(0.5) ) * 2.0,
		0.0,
		1.0
	);
	
	gl_TexCoord[0] = gl_MultiTexCoord0;
}
