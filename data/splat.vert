#version 120

asdasd

void main( void )
{
	const mat4 mvpMatrix = gl_ProjectionMatrix * gl_ModelViewMatrix;
	
	gl_Position = mvpMatrix * gl_Vertex;
	gl_FrontColor = gl_Color; //vec4( 1.0 );
}
