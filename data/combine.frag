#version 120
#extension GL_ARB_texture_rectangle: require

uniform ivec2 tex_resolution; // Size of input_... textures
uniform ivec2 resolution; // Screen resolution
uniform vec2 fov; // Field of view in radians
uniform float zNear;
uniform float zFar;

uniform sampler2DRect input_tex_z; // Linear depth in eye space
uniform sampler2DRect input_tex_m; // Material indexes
uniform sampler2DRect input_mat_table; // Material color table
uniform bool show_normals = false;

//out vec4 gl_FragColor;
//out float gl_FragDepth;

// Lighting setup
const vec3 light_dir = vec3( -0.57735027, 0.57735027, 0.57735027 );

// y coordinates for material texture
#define M_DIFFUSE 0
#define M_AMBIENT 1

// Converts linear eye space depth to screen depth [0,1]
float normalize_depth( in float z )
{
	return 0.5;
}

vec4 read_diffuse( in int material_id )
{
	ivec2 texel_co = ivec2( material_id, M_DIFFUSE );
	//return texelFetch( input_mat_table, texel_co );
	return texture2DRect( input_mat_table, vec2( texel_co ) );
}

vec4 read_ambient( in int material_id )
{
	ivec2 texel_co = ivec2( material_id, M_AMBIENT );
	//return texelFetch( input_mat_table, texel_co );
	return texture2DRect( input_mat_table, vec2( texel_co ) );
}

float read_depth( in ivec2 pos )
{
	// return texelFetch( input_tex_z, pos ).r;
	return texture2DRect( input_tex_z, vec2( pos ) ).r;
}

int read_material( in ivec2 pos )
{
	// return texelFetch( input_tex_m, pos ).r;
	return int( texture2DRect( input_tex_m, vec2( pos ) ).r * 255.0 );
}

void main( void )
{
	//ivec2 tex_size = textureSize( input_tex_m );
	ivec2 tex_size = tex_resolution;
	ivec2 tex_pos = ivec2(
		tex_size.x * int( gl_FragCoord.x ) / resolution.x,
		tex_size.y - tex_size.y * int( gl_FragCoord.y ) / resolution.y );
	
	int m_index = read_material( tex_pos );
	
	if ( m_index == 0 )
	{
		/* Ignore material #0 (transparency) */
		discard;
	}
	
	vec4 diffuse = read_diffuse( m_index );
	vec4 ambient = read_ambient( m_index );
	float z = read_depth( tex_pos );
	vec3 out_color = vec3( diffuse );
	
	// Compute screen space normal vector
	vec2 delta_z = vec2(
		read_depth( tex_pos + ivec2(1,0) ) - z,
		read_depth( tex_pos + ivec2(0,-1) ) - z );
	
	vec2 pixel_size = 2.0 * z * tan( fov * 0.5 ) / tex_size;
	vec3 nor = vec3( delta_z / pixel_size, 2.0 );
	nor = normalize( nor );
	
	if ( show_normals )
	{
		vec3 color = nor.xyz;
		color += vec3( 1.0 );
		color *= 0.5;
		gl_FragColor = vec4( color, 1.0 );
		return;
	}
	
	// Diffuse shading
	float dif = dot( nor, light_dir );
	dif = dif + ambient.r;
	dif = clamp( dif, 0.0, 1.0 );
	out_color = out_color * dif;
	
	// Done
	gl_FragColor = vec4( out_color, 1.0 );
	gl_FragDepth = normalize_depth( z );
}

