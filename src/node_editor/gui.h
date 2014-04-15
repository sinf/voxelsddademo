#ifndef _GUI_H
#define _GUI_H
#include <stddef.h>

/* UI component bits */
enum {
	UI_DECORATION=1, /* Ignore all events and pass to parent instead */
	UI_ALWAYS_INSIDE=2, /* The widget extends to infinity and always captures mouse events */
	UI_GRAPH_CONNECTOR=4,
	UI_DISABLED=8, /* Pretend the widget doesn't exist */
	UI_SIGNALLER=16, /* activates the 'signaled' callback of the first ancestor who has that callback */
	UI_DRAGGABLE=32,
	UI_NO_MERGE=64 /* Don't enlarge the bounds of the parent widget */
};

typedef enum {
	UI_MOUSE_DOWN,
	UI_MOUSE_UP,
	UI_MOUSE_MOVE
} UIMouseEvent;

typedef enum {
	UI_PACK_S, /* add child widget to south side of the previous child widget */
	UI_PACK_E, /* add child widget to east side of previous child widget */
	UI_PACK_REL /* treat (pos_x, pos_y) of child widgets as relative to parent's top-left corner */
} WidgetPackType;

typedef struct {
	WidgetPackType pack;
	int edge_margin;
	int child_spacing;
	int pos_x, pos_y;
} WidgetLayout;

typedef struct {
	int x0, y0, x1, y1;
} BBox;

struct Widget;
typedef struct Widget Widget;

typedef struct {
	void (*draw)( Widget *, void *gfx_data ); /* use the 'bounds' field to draw the widget! */
	void (*mouse_event)( Widget *, int x, int y, UIMouseEvent evt ); /* x,y relative to widget */
	void (*drag_event)( Widget *, int dx, int dy, int abs_x, int abs_y );
	void (*key_event)( Widget *, int key, int mod );
	void (*clicked)( Widget * );
	void (*signaled)( Widget *w, Widget *signaler, Widget *prev_of_w );
} WidgetCalls;

typedef struct {
	char *s;
	int max; /* maximum length, includes NUL */
	int caret_pos;
} WTextBuf;

typedef struct {
	/* Slider */
	float pos; /* in range [0,1] */
	float out_scale; /* The final output value is computed as follows: */
	float out_offset; /* slider_out_offset + slider_pos * slider_out_scale */
	float *out_f;
	int *out_i;
	void (*value_changed_f)( float );
	void (*value_changed_i)( int );
	int has_init; /* WSliderParams.pos will be initialized on first call to ui_draw() because the initializer macros can't dereference pointers at compile time */
} WSliderParams;

#define UII_SLIDER_PF(low,high,p,func) {0,(high)-(low),(low),(p),NULL,(func),NULL,0}
#define UII_SLIDER_PI(low,high,p,func) {0,(high)-(low),(low),NULL,(p),NULL,(func),0}

typedef struct {
	int type; /* Only widgets with the same con_type can be connected */
	Widget *pair; /* The widget that is connected or NULL */
} WGraphConnector;

struct Widget
{
	Widget *son; /* pointer to beginning of child widget chain (or NULL) */
	int com; /* component enable mask */
	int style; /* affects color and other graphic stuff */
	
	/* used styles:
	0 = the widget has no background
	1 = dark background used by panels and docks
	2 = clickable buttons
	3 = text fields
	4 = graph connector
	5 = graph connector (when linked)
	*/
	
	WidgetLayout layout;
	int size_x, size_y; /* Widget dimensions. Should be zero for containers */
	
	void *user_data; /* should not ever point to a widget */
	void (*user_action)( void *user_data );
	
	WidgetCalls fn; /* User callbacks */
	WTextBuf text;
	WSliderParams *slider;
	WGraphConnector gc;
	
	BBox bounds; /* absolute coords. includes the bounds of all child nodes. read-only */
	int tree_com; /* OR'd component masks from all of the nodes in the tree */
	
	unsigned
	is_terminator : 1, /* Only nonzero for a "terminator" widget that ends a widget chain. The terminator isn't rendered and doesn't do anything */
	hover : 1, /* True when mouse is over the widget */
	focus : 1, /* True when the widget has focus */
	pressed : 1, /* True when the widget has been clicked but mouse button hasn't been released */
	on_screen : 1, /* Whether the widget is on screen or not. Updated by draw_widgets */
	can_free : 1; /* Whether this widget should be free'd or not when it is no longer needed */
	
	Widget *detour; /* Address of the next sibling widget is normally widget+1 but detour when detour != NULL */
};

/* Returns the next sibling widget (check for is_terminator first!) */
#define NEXT_WIDGET(w) (((w)->detour ? (w)->detour : (w)+1))

typedef void (*UICallback)( void * );
extern void draw_widget( Widget *w, void *screen );

Widget *ui_mouse_event( Widget *root, int x, int y, UIMouseEvent m ); /* (x,y) are relative to parent widget. Returns the widget that got the event */
void ui_key_event( Widget *root, int key, int mod ); /* Every single widget gets the event */
void ui_draw( Widget *root, void *gfx_data, const BBox *viewport ); /* Only widgets inside viewport are drawn, unless viewport is NULL */

void ui_offset_user_data( Widget *w, ptrdiff_t offset ); /* Recursively adds offset to user_data. Assumes w != NULL. To rebase, set offset = new_base - old_base */
Widget *ui_deep_copy( const Widget *w ); /* May return NULL or an incomplete tree if malloc fails. Makes a deep copy */
void ui_free( Widget *w ); /* Recursively frees all widgets whose can_free is set */

Widget *ui_get_widget_at( Widget *w, int abs_x, int abs_y, int com_mask, int com_bits );
Widget *ui_set_text( Widget *w, char *text );

/* Widget callbacks (try to not use/access these directly) */
void _ui_textbox_key_event( Widget *, int, int );
void _ui_toggle_bit( int *x );
void _ui_slider_dragged( Widget *, int, int, int, int );
void _ui_menu_clicked( Widget *menu );
void _ui_menu_signaled( Widget *menu, Widget *menu_but, Widget *prev );

/* Checks for widget type */
#define WIDGET_IS_CHECKBOX(w) ((w)->user_action==(UICallback)_ui_toggle_bit)
#define WIDGET_IS_SLIDER(w) ((w)->slider != NULL)
#define WIDGET_IS_TEXTBOX(w) ((w)->fn.key_event==_ui_textbox_key_event)
#define WIDGET_IS_GRAPH_CONNECTOR(w) (((w)->com & UI_GRAPH_CONNECTOR) != 0)

/* Widget initializer helpers */
#define UII_DEFAULT_LAYOUT(pack) {pack,UI_DEFAULT_EDGE_MARGIN,UI_DEFAULT_WIDGET_SPACING,0}
#define UII_DEFAULT_CALLS {draw_widget,NULL}
#define UII_STRING_LIT(s) {(s),sizeof(s),sizeof(s)}
#define UII_BUTTON_SIZE UI_DEFAULT_BUTTON_W,UI_DEFAULT_BUTTON_H
#define UII_BUTTON_CALLS(fn) {draw_widget,NULL,NULL,NULL,(fn)}
#define UII_EMPTY_TAILER NULL,{0},{0},0
#define UII_DETOUR_TAILER(det) NULL,{0},{0},0,0,0,0,0,0,0,(det)

/* The terminator widget. Must be present at the end of all widget arrays */
#define UII_TERMINATOR {NULL,0,0,{0},0,0,NULL,NULL,{NULL},{0},UII_EMPTY_TAILER,1}

/* Widget struct initializers */

#define UII_FRAME(com,son) { \
	(son),(com),0, \
	{UI_PACK_REL,0}, \
	0,0,NULL,NULL, \
	UII_DEFAULT_CALLS, \
	{0}, \
	UII_EMPTY_TAILER}

#define UII_PANEL(com,son) { \
	(son),(com)|UI_DRAGGABLE,1, \
	UII_DEFAULT_LAYOUT( UI_PACK_S ), \
	0,0,NULL,NULL, \
	UII_DEFAULT_CALLS, \
	{0}, \
	UII_EMPTY_TAILER}

#define UII_LABEL(com,s) { \
	NULL, \
	(com)|UI_DECORATION, \
	0, \
	UII_DEFAULT_LAYOUT( UI_PACK_S ), \
	UII_BUTTON_SIZE, \
	NULL,NULL, \
	UII_DEFAULT_CALLS, \
	UII_STRING_LIT(s), \
	UII_EMPTY_TAILER}

#define UII_BUTTON(com,title,callback,data) { \
	NULL, \
	(com), \
	2, \
	UII_DEFAULT_LAYOUT( UI_PACK_S ), \
	UII_BUTTON_SIZE, \
	data,NULL, \
	UII_BUTTON_CALLS(callback), \
	UII_STRING_LIT(title), \
	UII_EMPTY_TAILER}

#define UII_BUTTON2(com,title,callback,data) { \
	NULL, \
	(com), \
	2, \
	UII_DEFAULT_LAYOUT( UI_PACK_S ), \
	UII_BUTTON_SIZE, \
	data,callback, \
	UII_DEFAULT_CALLS, \
	UII_STRING_LIT(title), \
	UII_EMPTY_TAILER}

#define UII_TEXTBOX(com,buffer,max_len,callback) { \
	NULL, \
	(com), \
	3, \
	UII_DEFAULT_LAYOUT( UI_PACK_S ), \
	UII_BUTTON_SIZE, \
	buffer, callback, \
	{draw_widget,NULL,NULL,_ui_textbox_key_event}, \
	{buffer,max_len,max_len}, \
	UII_EMPTY_TAILER}

#define UII_CHECKBOX(com,title,int_pointer) { \
	NULL, \
	(com), \
	2, \
	UII_DEFAULT_LAYOUT( UI_PACK_S ), \
	UII_BUTTON_SIZE, \
	int_pointer, (UICallback) _ui_toggle_bit, \
	UII_DEFAULT_CALLS, \
	UII_STRING_LIT(title), \
	UII_EMPTY_TAILER}

#define UII_SLIDER(com,title,params) { \
	NULL, \
	(com), \
	2, \
	UII_DEFAULT_LAYOUT( UI_PACK_S ), \
	UII_BUTTON_SIZE, \
	NULL,NULL, \
	{draw_widget,NULL,_ui_slider_dragged,NULL}, \
	UII_STRING_LIT(title), \
	(params), \
	{0},{0},0}

/* common_ancestor must not be NULL */
#define UII_GRAPH_CONNECTOR(com,title,type) { \
	NULL, \
	(com)|UI_GRAPH_CONNECTOR, \
	4, \
	UII_DEFAULT_LAYOUT( UI_PACK_S ), \
	UII_BUTTON_SIZE, \
	NULL,NULL, \
	UII_DEFAULT_CALLS, \
	UII_STRING_LIT(title), \
	NULL, \
	{type,NULL}, \
	{0},0}

#define __UI_MENU_IMP_W ((UI_DEFAULT_BUTTON_W - UI_DEFAULT_BUTTON_H - UI_DEFAULT_WIDGET_SPACING))
#define UII_MENU_LAUNCHER(self,first_menu_button) \
	{ /* A frame for 2 buttons */ \
		(self)+1, \
		UI_DECORATION, \
		0, \
		{ UI_PACK_E, 0, UI_DEFAULT_WIDGET_SPACING, 0}, \
		UII_BUTTON_SIZE, \
		NULL,NULL, \
		{draw_widget,NULL}, \
		{0}, \
		UII_DETOUR_TAILER( (self)+6 ) \
	}, { /* A button that shows the selected menu widget on top of itself */ \
		NULL, \
		0, \
		2, \
		{UI_PACK_S,0}, \
		__UI_MENU_IMP_W, UI_DEFAULT_BUTTON_H, \
		NULL,NULL, \
		UII_DEFAULT_CALLS, \
		{0}, \
		UII_EMPTY_TAILER \
	}, { /* A button that opens the combo box */ \
		(self)+4, \
		0, \
		2, \
		{UI_PACK_REL,0}, \
		UI_DEFAULT_BUTTON_H, UI_DEFAULT_BUTTON_H, \
		NULL,NULL, \
		{draw_widget,NULL,NULL,NULL,_ui_menu_clicked,_ui_menu_signaled}, \
		UII_STRING_LIT(".."), \
		UII_EMPTY_TAILER \
	}, \
	UII_TERMINATOR, /* Terminate the list of 2 buttons (above) */ \
	{ /* Panel that contains the menu buttons */ \
		(first_menu_button), \
		UI_NO_MERGE | UI_DISABLED, \
		0, \
		{UI_PACK_S,0,0,-UI_DEFAULT_WIDGET_SPACING - __UI_MENU_IMP_W, UI_DEFAULT_BUTTON_H + UI_DEFAULT_WIDGET_SPACING}, \
		UII_BUTTON_SIZE, \
		NULL,NULL, \
		UII_DEFAULT_CALLS, \
		{0}, \
		UII_EMPTY_TAILER}, \
	UII_TERMINATOR /* Terminate the list of 1 panel (above) */

#define UII_INVISIBLE_MENU_LAUNCHER(com,menu_panel) { \
	(menu_panel), \
	(com), \
	0, \
	{UI_PACK_REL,0}, \
	0,0, \
	NULL,NULL, \
	{draw_widget,NULL,NULL,NULL,NULL,_ui_menu_signaled}, \
	{0}, \
	UII_EMPTY_TAILER}

#define UII_GRAPHICS_WIDGET(com,draw_call,data) { \
	NULL, \
	(com)|UI_DECORATION, \
	0, \
	UII_DEFAULT_LAYOUT( UI_PACK_S ), \
	0,0, \
	data, NULL, \
	{draw_call,NULL}, \
	{0}, \
	UII_EMPTY_TAILER}

#endif
