#version 120

uniform float zNear;
uniform float zFar;

void main( void )
{
	mat4 mvpMatrix = gl_ProjectionMatrix * gl_ModelViewMatrix;
	
	gl_Position = mvpMatrix * gl_Vertex;
	gl_TexCoord[0] = gl_MultiTexCoord0;
	gl_FrontColor = gl_Color; //vec4( 1.0 );
}
