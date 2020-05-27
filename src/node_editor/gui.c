#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "gui.h"
#include "gui_config.h"

static int imin( int a, int b ) { return a < b ? a : b; }
static int imax( int a, int b ) { return a > b ? a : b; }

static int point_in_box( BBox *b, int x, int y )
{
	return x >= b->x0
	&& x <= b->x1
	&& y >= b->y0
	&& y <= b->y1;
}

static int box_outside_view( const BBox *box, const BBox *view )
{
	int n = 0;
	n |= ( box->x1 < view->x0 );
	n |= ( box->x0 > view->x1 );
	n |= ( box->y1 < view->y0 );
	n |= ( box->y0 > view->y1 );
	return n;
}

static void merge_bounds( BBox *c, BBox const *a, BBox const *b )
{
	c->x0 = imin( a->x0, b->x0 );
	c->y0 = imin( a->y0, b->y0 );
	c->x1 = imax( a->x1, b->x1 );
	c->y1 = imax( a->y1, b->y1 );
}

static void grow_bounds( BBox *b, int r )
{
	b->x0 -= r;
	b->y0 -= r;
	b->x1 += r;
	b->y1 += r;
}

/* The layout to use for root widgets (the parentless widgets) */
static const WidgetLayout ROOT_LAYOUT = {UI_PACK_REL,0,0,0,0};

/* Returns absolute position of given child widget */
static BBox get_child_box( Widget* const child, Widget* const prev, BBox* const prev_box, const WidgetLayout *layout, int parent_x0, int parent_y0 )
{
	WidgetPackType pack = layout->pack;
	int pad = prev ? layout->child_spacing : 0;
	int e = prev ? 0 : layout->edge_margin;
	BBox box;
	
	switch( pack )
	{
		case UI_PACK_E:
			box.x0 = prev ? ( prev_box->x1 + pad ) : ( prev_box->x0 + e );
			box.y0 = prev_box->y0;
			break;
		case UI_PACK_S:
			box.x0 = prev_box->x0;
			box.y0 = prev ? ( prev_box->y1 + pad ) : ( prev_box->y0 + e );
			break;
		case UI_PACK_REL:
			box.x0 = parent_x0 + child->layout.pos_x;
			box.y0 = parent_y0 + child->layout.pos_y;
			break;
	}
	
	box.x1 = box.x0 + child->size_x;
	box.y1 = box.y0 + child->size_y;
	return box;
}

/* Generates the layout for the widget tree. assumes w != NULL */
static BBox update_widget_bounds( Widget *parent, Widget *w, BBox box, const BBox *view )
{
	BBox parent_box = box;
	int total_com = 0;
	Widget *prev_w = NULL;
	const WidgetLayout *parent_layout = parent ? &parent->layout : &ROOT_LAYOUT;
	int parent_x0 = parent_box.x0;
	int parent_y0 = parent_box.y0;
	
	for( ; !w->is_terminator; prev_w=w, w=NEXT_WIDGET(w) )
	{
		if ( w->com & UI_DISABLED )
			continue;
		
		box = get_child_box( w, prev_w, &box, parent_layout, parent_x0, parent_y0 );
		
		w->tree_com = w->com;
		w->bounds = w->son ? update_widget_bounds( w, w->son, box, view ) : box;
		w->on_screen = !view || !box_outside_view( &w->bounds, view );
		total_com |= w->tree_com;
		
		if ( !( w->com & UI_NO_MERGE ) )
			merge_bounds( &parent_box, &parent_box, &w->bounds );
	}
	
	if ( parent ) {
		grow_bounds( &parent_box, parent_layout->edge_margin );
		parent->tree_com |= total_com;
	}
	
	return parent_box;
}

/* Recursively updates bounds of all widgets that are somehow linked to the given root widget */
static void update_widget_tree_bounds( Widget *w, const BBox *view )
{
	if ( w ) {
		update_widget_bounds( NULL, w, get_child_box( w, NULL, NULL, &ROOT_LAYOUT, 0, 0 ), view );
	}
}

static Widget *process_mouse_event( Widget *w, int const event_pos[2], UIMouseEvent m, Widget *the_one, Widget **signaller )
{
	Widget *prev_w = NULL;
	
	if ( !w )
		return w;
	
	for( ; !w->is_terminator; w=NEXT_WIDGET( prev_w=w ) )
	{
		int rel_x, rel_y;
		int in;
		
		if ( w->com & UI_DISABLED )
			continue;
		
		if ( w->son )
		{
			Widget *sig_src = NULL;
			Widget *new_one = process_mouse_event( w->son, event_pos, m, the_one, &sig_src );
			the_one = new_one;
			
			if ( sig_src )
				*signaller = sig_src;
		}
		
		if ( *signaller && w->fn.signaled ) {
			/* This widget has a signal handler so use it */
			w->fn.signaled( w, *signaller, prev_w );
			*signaller = NULL;
		}
		
		rel_x = event_pos[0] - w->bounds.x0;
		rel_y = event_pos[1] - w->bounds.y0;
		in = point_in_box( &w->bounds, event_pos[0], event_pos[1] );
		in |= ( w->com & UI_ALWAYS_INSIDE );
		w->hover = the_one ? 0 : in;
		
		if ( m == UI_MOUSE_UP && !WIDGET_IS_TEXTBOX(w) ) {
			w->focus = 0;
			w->pressed = 0;
		}
		
		if ( !the_one && !( w->com & UI_DECORATION ))
		{
			/* Consume the event */
			
			if ( WIDGET_IS_TEXTBOX(w) )
			{
				if ( m == UI_MOUSE_DOWN )
				{
					w->pressed = in;
					w->focus = in;
					
					/* Run textbox callback only when focus is lost (= the user stopped typing) */
					if ( !in && w->user_action )
						w->user_action( w->user_data );
					
					if ( in ) {
						the_one = w;
						
						/* Textbox gains focus. Clamp caret inside valid range */
						w->text.caret_pos = imin( imax( w->text.caret_pos, 0 ), strlen( w->text.s ));
					}
				}
			}
			else
			{
				if ( m == UI_MOUSE_DOWN )
				{
					w->focus = in;
					w->pressed = in;
					
					if ( in ) {
						the_one = w;
						
						if ( w->user_action )
							w->user_action( w->user_data );
						
						if ( w->fn.clicked )
							w->fn.clicked( w );
						
						if ( !*signaller && ( w->com & UI_SIGNALLER ) ) {
							/* Send a signal to ancestors */
							*signaller = w;
						}
					}
				}
			}
			
			if ( in ) {
				the_one = w;
				if ( w->fn.mouse_event )
					w->fn.mouse_event( w, rel_x, rel_y, m );
			}
		}
	}
	return the_one;
}

static void graph_connector_dragged( Widget *w, int x, int y, Widget *root )
{
	Widget *other;
	
	w->layout.pos_x = x;
	w->layout.pos_y = y;
	
	other = ui_get_widget_at( root, x, y, UI_GRAPH_CONNECTOR, UI_GRAPH_CONNECTOR );
	
	if ( other && other != w && ( other->gc.type == w->gc.type ) ) {
		if ( other->gc.pair )
			return; /* can't do weird 3-way connections */
		/* connected */
		w->gc.pair = other;
		other->gc.pair = w;
	} else if ( w->pressed ) {
		/* disconnected */
		if ( w->gc.pair )
			w->gc.pair->gc.pair = NULL;
		w->gc.pair = NULL;
	} else {
		return;
	}
	
	if ( w->user_action )
		w->user_action( w->user_data );
}

Widget *ui_mouse_event( Widget *w, int abs_x, int abs_y, UIMouseEvent m )
{
	static int x0 = 0, y0 = 0;
	static Widget *dw = NULL;
	int event_pos[2];
	Widget *nope=NULL;
	
	if ( !w )
		return NULL;
	
	update_widget_tree_bounds( w, NULL );
	
	if ( dw ) {
		
		if ( WIDGET_IS_GRAPH_CONNECTOR( dw ) && !( dw->com & UI_SIGNALLER ) )
			graph_connector_dragged( dw, abs_x, abs_y, w );
		
		if ( m == UI_MOUSE_MOVE ) {
			int dx = abs_x - x0;
			int dy = abs_y - y0;
			if ( dw->com & UI_DRAGGABLE ) {
				dw->layout.pos_x += dx;
				dw->layout.pos_y += dy;
			}
			if ( dw->fn.drag_event )
				dw->fn.drag_event( dw, dx, dy, abs_x, abs_y );
		} else if ( m == UI_MOUSE_UP ) {
			dw = NULL;
		}
	}
	
	event_pos[0] = abs_x;
	event_pos[1] = abs_y;
	w = process_mouse_event( w, event_pos, m, NULL, &nope );
	
	if ( m == UI_MOUSE_DOWN && w ) {
		dw = w;
	}
	
	x0 = abs_x;
	y0 = abs_y;
	
	return w;
}

void ui_key_event( Widget *w, int key, int mod )
{
	if ( w )
	for( ; !w->is_terminator; w=NEXT_WIDGET(w) )
	{
		if ( w->com & UI_DISABLED )
			continue;
		if ( w->fn.key_event )
			w->fn.key_event( w, key, mod );
		ui_key_event( w->son, key, mod );
	}
}

static void init_slider( WSliderParams *p )
{
	float x = p->out_f ? *p->out_f : ( p->out_i ? *p->out_i : 0 );
	p->pos = ( x - p->out_offset ) / p->out_scale;
	p->has_init = 1;
}

/* assumes widgets != NULL */
static void draw_all_widgets( Widget *widgets, void *data )
{
	size_t num, n;
	Widget *w, **p;
	
	/* Check how long the widget chain is */
	for( num=0,w=widgets; !w->is_terminator; w=NEXT_WIDGET(w),num++ );
	
	p = alloca( num * sizeof * p );
	n=0;
	
	/* Make a list of pointer to widgets but in reverse order  */
	for( w=widgets; !w->is_terminator; w=NEXT_WIDGET(w) )
		p[n++] = w;
	
	/* Draw widgets in reverse order */
	while( n ) {
		w = p[--n];
		
		if ( WIDGET_IS_SLIDER( w ) && !w->slider->has_init )
			init_slider( w->slider );
		
		if ( w->on_screen && !( w->com & UI_DISABLED )) {
			if ( w->fn.draw ) w->fn.draw( w, data );
			if ( w->son ) draw_all_widgets( w->son, data );
		}
	}
}

void ui_draw( Widget *w, void *data, const BBox *view )
{
	if ( !w )
		return;
	
	update_widget_tree_bounds( w, view );
	draw_all_widgets( w, data );
}

void _ui_textbox_key_event( Widget *w, int key, int mod )
{
	WTextBuf *t = &w->text;
	int cp = t->caret_pos;
	int len = strlen( t->s );
	cp = imin( imax( cp, 0 ), len );
	
	if ( !w->focus )
		return;
	
	switch( key )
	{
		case UI_TEXTBOX_KEY_LEFT:
			cp = ( mod & UI_TEXTBOX_MOD_CTRL ) ? 0 : cp - 1;
			break;
		case UI_TEXTBOX_KEY_RIGHT:
			cp = ( mod & UI_TEXTBOX_MOD_CTRL ) ? len : cp + 1;
			break;
		case UI_TEXTBOX_KEY_BACKSP:
			if ( mod & UI_TEXTBOX_MOD_CTRL ) {
				len -= t->caret_pos;
				t->s[len] = '\0';
				memmove( t->s, t->s + cp, len );
				cp = 0;
			} else if ( cp ) {
				t->s[--cp] = '\0';
			}
			break;
		case UI_TEXTBOX_KEY_DELETE:
			if ( mod & UI_TEXTBOX_MOD_CTRL ) {
				memset( t->s + cp, 0, len - cp );
			} else
			if ( cp < len ) {
				memmove( t->s + cp, t->s + cp + 1, len - cp + 1 );
				t->s[ --len ] = '\0';
			}
			break;
		case UI_TEXTBOX_KEY_ENTER:
			w->focus = 0;
			w->pressed = 0;
			if ( w->user_action )
				w->user_action( w->user_data );
			break;
		default:
			if ( isprint( key ) && len+1 < t->max )
			{
				memmove( t->s + cp + 1, t->s + cp, len - cp );
				t->s[cp++] = key;
				t->s[++len] = '\0';
			}
			break;
	}
	
	t->caret_pos = imin( imax( cp, 0 ), len );
}

Widget *ui_set_text( Widget *w, char *text )
{
	if ( text ) {
		int len = strlen( text );
		int nlc = 0;
		const char *p;
		
		for( p=text; *p != '\0'; p++ )
			nlc += ( *p == '\n' );
		
		w->size_y = UI_DEFAULT_BUTTON_H + nlc * UI_GLYPH_H;
		w->text.s = text;
		w->text.max = len + 1;
	} else {
		w->size_y = UI_DEFAULT_BUTTON_H;
		w->text.s = NULL;
		w->text.max = 0;
	}
	return w;
}

/* Used by checkboxes */
void _ui_toggle_bit( int *p )
{
	*p ^= 1;
}

void _ui_slider_dragged( Widget *w, int dx, int dy, int abs_x, int abs_y )
{
	float rail_len;
	float t;
	int i;
	int size_x = w->size_x;
	WSliderParams *par = w->slider;
	
	(void) abs_x;
	(void) abs_y;
	(void) dy;
	
	rail_len = size_x - UI_SLIDER_BUTTON_LEN;
	t = par->pos;
	t += dx / rail_len;
	t = t < 0 ? 0 : ( t > 1 ? 1 : t );
	par->pos = t;
	t = par->out_offset + par->out_scale * t;
	
	if ( par->out_f ) *par->out_f = t;
	if ( par->value_changed_f ) par->value_changed_f( t );
	
	i = t;
	if ( par->out_i ) *par->out_i = i;
	if ( par->value_changed_i ) par->value_changed_i( t );
}

void _ui_ignore_signal( Widget *a, Widget *b, Widget *c )
{
	(void) ( a - b + c );
}

void _ui_menu_clicked( Widget *menu )
{
	/* Open/close combo box */
	menu->son->com ^= UI_DISABLED;
}
void _ui_menu_signaled( Widget *menu, Widget *mbut, Widget *prev_of_menu )
{
	Widget *panel;
	
	panel = menu->son;
	panel->com |= UI_DISABLED; /* Hide the panel that contains menu buttons */
	
	/* don't mess with UII_INVISIBLE_MENU_LAUNCHER */
	if ( menu->style )
	{
		Widget *s;
		WidgetLayout lo;
		int sx, sy;
		
		/* Overwrite the imposter widget with the widget that was selected from the menu */
		
		s = prev_of_menu; /** NEXT_WIDGET( menu ); **/
		
		if ( WIDGET_IS_GRAPH_CONNECTOR( s ) && s->gc.pair && s->gc.pair->gc.pair ) {
			/* A graph connector can be embedded in a combo box. Disconnect when something else is selected */
			s->gc.pair->gc.pair = NULL;
		}
		
		lo = s->layout;
		sx = s->size_x;
		sy = s->size_y;
		
		*s = *mbut;
		s->com &= ~UI_SIGNALLER;
		s->size_x = sx;
		s->size_y = sy;
		s->layout = lo;
		s->detour = NULL;
	}
	
	mbut->focus = 0;
	mbut->pressed = 0;
	mbut->hover = 0;
}

/* Finds the widget that contains the given point
assumes w != NULL */
Widget *ui_get_widget_at( Widget *w, int abs_x, int abs_y, int com_mask, int com_bits )
{
	for( ; !w->is_terminator; w=NEXT_WIDGET(w) )
	{
		if ( w->com & UI_DISABLED )
			continue;
		
		if ( w->son && ( w->tree_com & com_mask ) == com_bits ) {
			Widget *c = ui_get_widget_at( w->son, abs_x, abs_y, com_mask, com_bits );
			if ( c && ( c->com & com_mask ) == com_bits )
				return c;
		}
		
		if ( ( w->com & com_mask ) != com_bits )
			continue;
		
		if ( point_in_box( &w->bounds, abs_x, abs_y ) || ( w->com & UI_ALWAYS_INSIDE ) )
			return w;
	}
	
	return NULL;
}

void ui_offset_user_data( Widget *w, ptrdiff_t offset )
{
	while( !w->is_terminator )
	{
		if ( w->son )
			ui_offset_user_data( w->son, offset );
		
		w->user_data = (char*) w->user_data + offset;
		w = NEXT_WIDGET( w );
	}
}

Widget *ui_deep_copy( const Widget *o )
{
	Widget *prev, *head, *w;
	
	if ( !o )
		return NULL;
	
	head = malloc( sizeof *head );
	w = head;
	prev = NULL;
	
	for( ;; )
	{
		if ( !w ) {
			if ( prev )
				prev->is_terminator = 1;
			break;
		}
		
		if ( prev )
			prev->detour = w;
		
		*w = *o;
		w->can_free = 1;
		w->gc.pair = NULL; /* sever these connections to avoid getting invalid pointers */
		
		if ( o->son )
			w->son = ui_deep_copy( o->son );
		
		if ( o->is_terminator )
			break;
		
		prev = w;
		o = NEXT_WIDGET( o );
		w = malloc( sizeof(Widget) );
	}
	
	return head;
}

void ui_free( Widget *w )
{
	while( w )
	{
		Widget *next;
		
		if ( w->son ) {
			ui_free( w->son );
			w->son = NULL;
		}
		
		next = w->is_terminator ? NULL : NEXT_WIDGET( w );
		
		if ( w->can_free )
			free( w );
		
		w = next;
	}
}
