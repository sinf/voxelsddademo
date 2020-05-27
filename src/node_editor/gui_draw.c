#include <math.h>
#include <SDL.h>
#include "gui_draw.h"
#include "gui_config.h"
#include "gui.h"

int total_ui_boxes_drawn = 0;
int show_ui_widget_bounds = 0;

static void draw_box( SDL_Surface *screen, int x, int y, int w, int h, Uint32 color );
static void draw_horz_line( SDL_Surface *screen, int x0, int x1, int y, Uint32 color );
static void draw_vert_line( SDL_Surface *screen, int y0, int y1, int x, Uint32 color );
void draw_text( struct SDL_Surface *dst, int x, int y, const char text[] );

#if 0
/* Draws a box with beveled top-left and bottom-right corners */
static void draw_box( SDL_Surface *screen, int x, int y, int w, int h, Uint32 color )
{
	int j = 4;
	SDL_Rect r;
	int t;
	
	if ( h < j )
		j = h;
	
	for( t=0; t<j; t++ ) {
		r.x = x + j - t;
		r.y = y + t;
		r.w = w - j + t;
		r.h = 1;
		SDL_FillRect( screen, &r, color );
	}
	
	t = h - 2*j + 1;
	if ( t > 0 ) {
		r.x = x;
		r.y = y + j;
		r.w = w;
		r.h = t;
		SDL_FillRect( screen, &r, color );
	}
	
	
	total_ui_boxes_drawn += j + ( j - 1 ) + ( t > 0 );
	
	for( t=1; t<j; t++ ) {
		r.x = x;
		r.y = y + h - j + t;
		r.w = w - t;
		r.h =1;
		SDL_FillRect( screen, &r, color );
	}
}
#else
static void draw_box( SDL_Surface *screen, int x, int y, int w, int h, Uint32 color )
{
	SDL_Rect r;
	r.x = x;
	r.y = y;
	r.w = w;
	r.h = h;
	SDL_FillRect( screen, &r, color );
	total_ui_boxes_drawn++;
}
#endif

static void draw_horz_line( SDL_Surface *screen, int x0, int x1, int y, Uint32 color )
{
	SDL_Rect r;
	r.y = y;
	r.h = 1;
	if ( x0 < x1 ) {
		r.x = x0;
		r.w = x1 - x0;
	} else {
		r.x = x1;
		r.w = x0 - x1;
	}
	r.w++;
	SDL_FillRect( screen, &r, color );
	total_ui_boxes_drawn++;
}

static void draw_vert_line( SDL_Surface *screen, int y0, int y1, int x, Uint32 color )
{
	SDL_Rect r;
	r.x = x;
	r.w = 1;
	if ( y0 < y1 ) {
		r.y = y0;
		r.h = y1 - y0;
	} else {
		r.y = y1;
		r.h = y0 - y1;
	}
	r.h++;
	SDL_FillRect( screen, &r, color );
	total_ui_boxes_drawn++;
}

static void draw_graph_connector_lines( Widget *w, SDL_Surface *screen, const Uint32 color )
{
	const Widget *pair = w->gc.pair;
	const BBox *box = &w->bounds;
	const int k = UI_GC_LINE_DODGE;
	int px, py; /* final end point where to connect the line */
	int mx, my; /* midpoint of box */
	int ax, bx, bxl, bxr;
	
	mx = ( box->x0 + box->x1 ) >> 1;
	my = ( box->y0 + box->y1 ) >> 1;
	bxl = box->x0 - k;
	bxr = box->x1 + k;
	
	if ( pair ) {
		px = ( box->x0 + box->x1 + pair->bounds.x0 + pair->bounds.x1 ) >> 2;
		py = ( box->y0 + box->y1 + pair->bounds.y0 + pair->bounds.y1 ) >> 2;
	} else if ( w->pressed ) {
		px = ( mx + w->layout.pos_x ) >> 1;
		py = ( my + w->layout.pos_y ) >> 1;
	} else {
		return;
	}
	
	/* do a S-curve if the 2 buttons have the same X coords */
	px -= ( px == mx ) && ( w < pair );
	
	if ( px < mx ) {
		ax = box->x0;
		bx = bxl;
	} else {
		ax = box->x1;
		bx = bxr;
	}
	
	if ( px >= bxl && px <= bxr ) {
		draw_horz_line( screen, ax, bx, my, color );
		draw_horz_line( screen, bx, px, py, color );
		draw_vert_line( screen, my, py, bx, color );
	} else {
		draw_horz_line( screen, ax, px, my, color );
		draw_vert_line( screen, my, py, px, color );
	}
}

/* Almost equivalent to calling snprintf with the format string "%# .2e"
- but this one doesn't clutter the exponent with redudant zeros */
static int format_exp10( char buf[], size_t bufsize, unsigned num_decimal_places, float x )
{
	char fmt[64];
	int e = 0;
	float m = x;
	if ( x != 0.0 ) {
		e = log10f( fabsf( x ) );
		m = x * exp10f( 1 - e-- );
	}
	snprintf( fmt, sizeof(fmt), "%% 1.%ufe%%d", num_decimal_places );
	return snprintf( buf, bufsize, fmt, m, e );
}

void draw_widget( struct Widget *w, void *screen )
{
	static Uint32 const the_colors[] = {
		0,0,
		UI_COLOR_BG, UI_COLOR_BG_HOVER,
		UI_COLOR_BUTTON, UI_COLOR_BUTTON_HOVER,
		UI_COLOR_TEXTBOX, UI_COLOR_TEXTBOX_HOVER,
		UI_COLOR_GC, UI_COLOR_GC_HOVER,
		UI_COLOR_GC_LINKED, UI_COLOR_GC_LINKED_HOVER
	};
	
	int x = w->bounds.x0;
	int y = w->bounds.y0;
	int sx = w->bounds.x1 - w->bounds.x0;
	int sy = w->bounds.y1 - w->bounds.y0;
	
	int label_x = x + UI_LABEL_OFFSET_X;
	int label_y = y + ( WIDGET_IS_SLIDER(w) ? UI_SLIDER_LABEL_POS_Y : UI_LABEL_OFFSET_Y );
	
	if ( w->style )
	{
		int style;
		
		if ( w->pressed || ( WIDGET_IS_TEXTBOX(w) && w->focus ) )
		{
			int r = UI_FOCUS_BORDER_R;
			draw_box( screen, x-r, y-r, sx+2*r, sy+2*r, UI_COLOR_FOCUS_BORDER );
		}
		
		style = w->style;
		style += WIDGET_IS_GRAPH_CONNECTOR( w ) && w->gc.pair;
		style <<= 1;
		style += w->hover;
		
		draw_box( screen, x, y, sx, sy, the_colors[style] );
	}
	
	if ( w->text.s )
	{
		draw_text( screen, label_x, label_y, w->text.s );
		
		if ( WIDGET_IS_TEXTBOX(w) && w->focus )
		{
			char const s[] = {UI_CARET_CHARACTER,'\0'};
			draw_text( screen, label_x + w->text.caret_pos * GLYPH_W + UI_CARET_OFFSET_X, label_y, s );
		}
	}
	
	if ( WIDGET_IS_CHECKBOX( w ) )
	{
		int yes = *(int*) w->user_data;
		int len = sizeof( UI_CHECKBOX_YES ) / sizeof( char ) - 1;
		draw_text( screen, w->bounds.x1 - UI_LABEL_OFFSET_X - len * UI_GLYPH_W, label_y, yes ? UI_CHECKBOX_YES : UI_CHECKBOX_NO );
	}
	
	if ( WIDGET_IS_SLIDER( w ) )
	{
		int rail_len = w->size_x - UI_SLIDER_BUTTON_LEN;
		int rail_mid_y2 = ( y + UI_SLIDER_RAIL_POS_Y ) << 1;
		int rail_n = ( rail_mid_y2 - UI_SLIDER_RAIL_TH ) >> 1;
		int rail_w = ( w->bounds.x0 << 1 ) + UI_SLIDER_BUTTON_LEN >> 1;
		
		int but_n = ( rail_mid_y2 - UI_SLIDER_BUTTON_TH ) >> 1;
		int but_w = w->bounds.x0 + rail_len * w->slider->pos;
		
		char buf[32];
		int len;
		
		draw_box( screen, rail_w, rail_n, rail_len, UI_SLIDER_RAIL_TH, UI_COLOR_SLIDER_RAIL );
		draw_box( screen, but_w, but_n, UI_SLIDER_BUTTON_LEN, UI_SLIDER_BUTTON_TH, UI_COLOR_SLIDER_BUTTON );
		
		if ( w->slider->out_f || w->slider->value_changed_f )
			len = format_exp10( buf, sizeof( buf ), 1, *w->slider->out_f );
		else
			len = snprintf( buf, sizeof( buf ), "%d", *w->slider->out_i );
		
		draw_text( screen,
			w->bounds.x1 - UI_LABEL_OFFSET_X - len * UI_GLYPH_W,
			y + UI_SLIDER_LABEL_POS_Y, buf );
	}
	
	if ( WIDGET_IS_GRAPH_CONNECTOR( w ) )
	{
		Uint32 color = ( !w->gc.pair && w->pressed ) ? UI_COLOR_GC_LINES_MOVING : UI_COLOR_GC_LINES;
		draw_graph_connector_lines( w, screen, color );
		
		if ( w->gc.pair ) {
			/* Both parts of the line need to be drawn if even one of the pair is on screen */
			if ( !w->gc.pair->on_screen )
				draw_graph_connector_lines( w->gc.pair, screen, color );
		} else if ( w->pressed ) {
			/* Create a dummy widget at cursor and use it to draw lines when draggin */
			Widget t;
			t.bounds.x0 = t.bounds.x1 = w->layout.pos_x;
			t.bounds.y0 = t.bounds.y1 = w->layout.pos_y;
			t.layout.pos_x = ( w->bounds.x0 + w->bounds.x1 ) >> 1;
			t.layout.pos_y = ( w->bounds.y0 + w->bounds.y1 ) >> 1;
			t.gc.pair = NULL;
			t.pressed = 1;
			draw_graph_connector_lines( &t, screen, color );
		}
	}
	
	if ( show_ui_widget_bounds ) {
		/* Draw outlines of *ALL* widgets */
		Uint32 color = UI_COLOR_DEBUG;
		draw_horz_line( screen, w->bounds.x0, w->bounds.x1, w->bounds.y0, color );
		draw_horz_line( screen, w->bounds.x0, w->bounds.x1, w->bounds.y1, color );
		draw_vert_line( screen, w->bounds.y0, w->bounds.y1, w->bounds.x0, color );
		draw_vert_line( screen, w->bounds.y0, w->bounds.y1, w->bounds.x1, color );
	}
}

void draw_image_widget( struct Widget *w, void *screen )
{
	SDL_Surface *img, **img_p = w->user_data;
	SDL_Rect dst;
	
	if ( img_p && *img_p )
	{
		dst.x = w->bounds.x0;
		dst.y = w->bounds.y0;
		
		img = *img_p;
		w->size_x = img->w;
		w->size_y = img->h;
		
		SDL_BlitSurface( img, NULL, screen, &dst );
	}
}
