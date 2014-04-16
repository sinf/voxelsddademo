common_flags=\
"-Wall -Wextra -std=c99 -pedantic -pthread -march=native "+\
"-Wno-missing-field-initializers "+\
"-Wno-char-subscripts "+\
"-Wno-parentheses"

# mode : (cc flags, linker flags)
special_flags={ \
"debug" : ("-O2 -g",""),
"fast" : ("-O4 -ffast-math -funsafe-math-optimizations -ffinite-math-only -ftree-vectorizer-verbose=3",""),
"prof" : ("-O2 -g -pg","-pg"),
}

def build_stuff( mode ):
	print("mode="+mode)
	base=Environment()
	base.ParseConfig( "sdl-config --libs --cflags" );
	base.ParseConfig( "pkg-config --libs --cflags glee" );
	base.Append( LIBS=Split("rt m pthread") )
	base.Append( CPPPATH=["../common"] )
	base.Append( LIBPATH=[".."] )
	base.Append( RPATH=["."] )
	base.Append( CCFLAGS=Split(common_flags) )
	if mode in special_flags:
		f=special_flags[mode]
		base.Append(CCFLAGS=Split(f[0]))
		base.Append(LINKFLAGS=Split(f[1]))
	for d in ["common", "rays", "node_editor"]:
		SConscript( "src/"+d+"/SConscript", variant_dir="build/"+mode+"/"+d, duplicate=0, exports=["base"] )

modes=ARGUMENTS.get( "x", None )
modes=special_flags.keys() if modes is None else Split(modes)
for m in modes:
	build_stuff(m)

# Mahd. SDL korvaajia:
#  GLFW
#  Cpw
#  Allegro
#  SDL 2.0
