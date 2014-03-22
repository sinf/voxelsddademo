#version 120

uniform sampler2D colormap;

void main( void )
{
	gl_FragDepth = gl_FragCoord.z;
	gl_FragColor = texture2D( colormap, gl_TexCoord[0].st );
}

todo
