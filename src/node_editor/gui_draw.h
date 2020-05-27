#ifndef _UI_DRAW_H
#define _UI_DRAW_H

struct Widget;

/* reset to 0 manually before calling ui_draw() */
extern int total_ui_boxes_drawn;
extern int show_ui_widget_bounds;

/* Draws a generic widget */
void draw_widget( struct Widget *w, void *screen );

/* Blits widget's user_data as a SDL_Surface */
void draw_image_widget( struct Widget *w, void *screen );

/* Can be used to embed SDL surfaces in UIs */
#define UII_SDL_SURFACE(com,surfpp) UII_GRAPHICS_WIDGET( (com), draw_image_widget, (surfpp) )

#endif
