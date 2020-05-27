#include <stdlib.h>
#include <SDL.h>
#include "gui.h"
#include "gui_config.h"
#include "gui_draw.h"
#include "text.h"

/*
static void root_mouse_func( Widget *w, int x, int y, UIMouseEvent e ) {
	printf( "Root mouse func: %d %d %d\n", x, y, e );
}
static void root_key_func( Widget *w, int key, int mod ) {
	printf( "Root key event: %d %d\n", key, mod );
}
*/

#define INITIAL_TEXTBOX_VALUE "please type"
static char textbox_buf[18] = INITIAL_TEXTBOX_VALUE;
static int checkbox_value = 0;
static SDL_Surface *my_image = NULL;

static float slider1_value = 0;
static int slider2_value = 0;
static WSliderParams slider1 = UII_SLIDER_PF( -0.1, 0.1, &slider1_value, NULL );
static WSliderParams slider2 = UII_SLIDER_PI( 0, 10000, &slider2_value, NULL );

static void print_user( Widget *w ) { puts( w->user_data ); }
static void update_window_title( void *s ) { SDL_WM_SetCaption( s, s ); }

static char total_boxes_label[64] = "";
static Widget box_count_label_widget[] = {
	UII_LABEL( 0, total_boxes_label ),
	UII_CHECKBOX( 0, "Show bounds", &show_ui_widget_bounds ),
	UII_TERMINATOR
};

static Widget menu_buttons[] = {
	UII_BUTTON( UI_SIGNALLER, "option 1", NULL, NULL ),
	UII_BUTTON( UI_SIGNALLER, "option 2", NULL, NULL ),
	UII_BUTTON( UI_SIGNALLER, "option 3", NULL, NULL ),
	UII_BUTTON( UI_SIGNALLER, "option 4", NULL, NULL ),
	UII_SLIDER( UI_SIGNALLER, "slider1", &slider1 ),
	UII_GRAPH_CONNECTOR( UI_SIGNALLER, "connector (3)", 3 ),
	UII_TERMINATOR
};
static Widget buttons[] = {
	UII_BUTTON( 0, "Button 1", print_user, "button 1 clicked" ),
	UII_BUTTON( 0, "Button 2", print_user, "button 2 clickde" ),
	UII_LABEL( 0, "this is a label" ),
	UII_TEXTBOX( 0, textbox_buf, sizeof(textbox_buf), update_window_title ),
	UII_CHECKBOX( 0, "check", &checkbox_value ),
	UII_SLIDER( 0, "slider2", &slider2 ),
	UII_GRAPH_CONNECTOR( 0, "connector (3)", 3 ),
	UII_TERMINATOR
};
static Widget buttons2[] = {
	UII_LABEL( 0, "A smaller panel" ),
	UII_GRAPH_CONNECTOR( 0, "connector (5)", 5 ),
	UII_MENU_LAUNCHER( buttons2+2, menu_buttons ),
	UII_LABEL( 0, "text ..." ),
	UII_TERMINATOR
};
static Widget buttons3[] = {
	UII_LABEL( 0, "Some connectors" ),
	UII_GRAPH_CONNECTOR( 0, "connector (3)", 3 ),
	UII_GRAPH_CONNECTOR( 0, "connector (3)", 3 ),
	UII_GRAPH_CONNECTOR( 0, "connector (3)", 3 ),
	UII_GRAPH_CONNECTOR( 0, "connector (5)", 5 ),
	UII_TERMINATOR
};

static Widget img_panel[] = {
	UII_SDL_SURFACE( 0, &my_image ),
	UII_TERMINATOR
};

static Widget panels[] = {
	UII_PANEL( UI_DRAGGABLE, box_count_label_widget ), /* invisible dummy widget */
	UII_PANEL( UI_DRAGGABLE, buttons ),
	UII_PANEL( UI_DRAGGABLE, buttons2 ),
	UII_PANEL( UI_DRAGGABLE, buttons3 ),
	UII_PANEL( UI_DRAGGABLE, img_panel ),
	UII_TERMINATOR
};

static void new_panel_button_clicked( Widget *button )
{
	static Widget my_buttons[] = {
		UII_LABEL( 0, "asdiasbn" ),
		UII_BUTTON( 0, "button", print_user, "button" ),
		UII_BUTTON( 0, "ggggggg", print_user, "gg" ),
		UII_GRAPH_CONNECTOR( 0, "a5", 5 ),
		UII_GRAPH_CONNECTOR( 0, "b5", 5 ),
		UII_GRAPH_CONNECTOR( 0, "c3", 3 ),
		UII_TERMINATOR
	};
	static Widget my_panel[] = {
		UII_PANEL( 0, my_buttons ),
		UII_TERMINATOR
	};
	Widget *new, *root = button->user_data;
	if ( root ) {
		new = ui_deep_copy( my_panel );
		new->detour = NEXT_WIDGET( root );
		
		SDL_GetMouseState( &new->layout.pos_x, &new->layout.pos_y );
		new->layout.pos_x -= root->bounds.x0;
		new->layout.pos_y -= root->bounds.y0;
		
		root->detour = new;
	}
}

static Widget rmb_menu_buts[] = {
	UII_PANEL( 0, rmb_menu_buts+2 ),
	UII_TERMINATOR,
	UII_LABEL( 0, "hello" ),
	UII_BUTTON( UI_SIGNALLER, "click me", print_user, "rmb menu 1" ),
	UII_BUTTON( UI_SIGNALLER, "asdasd e", print_user, "rmb menu 2" ),
	UII_BUTTON( UI_SIGNALLER, "new panel", new_panel_button_clicked, panels ),
	UII_BUTTON( UI_SIGNALLER | UI_ALWAYS_INSIDE, "cancel", NULL, NULL ),
	UII_TERMINATOR
};

static Widget hroot[] = {
	UII_INVISIBLE_MENU_LAUNCHER( 0, rmb_menu_buts ),
	UII_FRAME( UI_ALWAYS_INSIDE | UI_DRAGGABLE, panels ),
	UII_TERMINATOR
};

static void toggle_right_click_menu( Widget *hroot, int x, int y )
{
	/* todo */
	hroot[0].son->com ^= UI_DISABLED;
	hroot[0].son->layout.pos_x = x;
	hroot[0].son->layout.pos_y = y;
}

int main()
{
	const int video_flags = SDL_DOUBLEBUF|SDL_RESIZABLE|SDL_HWSURFACE;
	
	SDL_Surface *screen;
	
	if ( SDL_Init(SDL_INIT_VIDEO) < 0 )
		return 1;
	
	screen = SDL_SetVideoMode( 1024, 768, 0, video_flags );
	if ( !screen )
		return 2;
	
	my_image = SDL_LoadBMP( "data/icon.bmp" );
	if ( !my_image ) {
		printf( "%s\n", SDL_GetError() );
	} else {
		SDL_Surface *s = SDL_DisplayFormat( my_image );
		if ( s ) {
			SDL_FreeSurface( my_image );
			my_image = s;
		}
	}
	
	if ( !load_font() )
		return 3;
	
	SDL_EnableUNICODE( 1 );
	SDL_EnableKeyRepeat( 150, 50 );
	
	while( 42 )
	{
		SDL_Event e;
		BBox scr_bounds;
		
		while( SDL_PollEvent( &e ) )
		switch( e.type )
		{
			case SDL_QUIT:
				goto EXIT;
			
			case SDL_KEYDOWN:
				if ( e.key.keysym.sym == SDLK_ESCAPE )
					goto EXIT;
				if ( isprint( e.key.keysym.sym ) )
					ui_key_event( hroot, e.key.keysym.unicode, e.key.keysym.mod );
				else
					ui_key_event( hroot, e.key.keysym.sym, e.key.keysym.mod );
				break;
			
			case SDL_MOUSEBUTTONDOWN:
				if ( e.button.button == SDL_BUTTON_RIGHT )
					toggle_right_click_menu( hroot, e.button.x, e.button.y );
				else
					ui_mouse_event( hroot, e.button.x, e.button.y, UI_MOUSE_DOWN );
				break;
			
			case SDL_MOUSEBUTTONUP:
				if ( e.button.button != SDL_BUTTON_RIGHT )
					ui_mouse_event( hroot, e.button.x, e.button.y, UI_MOUSE_UP );
				break;
			
			case SDL_MOUSEMOTION:
				ui_mouse_event( hroot, e.motion.x, e.motion.y, UI_MOUSE_MOVE );
				break;
			
			case SDL_VIDEORESIZE:
				screen = SDL_SetVideoMode( e.resize.w, e.resize.h, 0, video_flags );
				break;
			
			default:
				break;
		}
		
		SDL_FillRect( screen, NULL, 0 );
		
		total_ui_boxes_drawn=1;
		scr_bounds.x0 = 0;
		scr_bounds.y0 = 0;
		scr_bounds.x1 = screen->w;
		scr_bounds.y1 = screen->h;
		
		ui_draw( hroot, screen, &scr_bounds );
		snprintf( total_boxes_label, sizeof(total_boxes_label), "Boxes drawn: %d", total_ui_boxes_drawn );
		
		SDL_Flip( screen );
	}
	
EXIT:;
	unload_font();
	if ( my_image ) SDL_FreeSurface( my_image );
	ui_free( hroot );
	SDL_Quit();
	return 0;
}
