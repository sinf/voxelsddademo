#ifndef _GUI_CONFIG_H
#define _GUI_CONFIG_H

#include <SDL_keysym.h>
#include "text.h"

enum {
	UI_TEXTBOX_KEY_LEFT = SDLK_LEFT,
	UI_TEXTBOX_KEY_RIGHT = SDLK_RIGHT,
	UI_TEXTBOX_KEY_BACKSP = SDLK_BACKSPACE,
	UI_TEXTBOX_KEY_DELETE = SDLK_DELETE,
	UI_TEXTBOX_KEY_ENTER = SDLK_RETURN,
	UI_TEXTBOX_MOD_CTRL = KMOD_CTRL
};

enum {
	UI_GLYPH_W = GLYPH_W,
	UI_GLYPH_H = GLYPH_H,
	UI_DEFAULT_BUTTON_W = 200,
	UI_DEFAULT_BUTTON_H = 30,
	UI_DEFAULT_WIDGET_SPACING = 5,
	UI_DEFAULT_EDGE_MARGIN = 5,
	UI_CARET_CHARACTER = '|',
	UI_CARET_OFFSET_X = -GLYPH_W/2,
	UI_SLIDER_BUTTON_LEN = 40, /* slider handle button length */
	UI_SLIDER_BUTTON_TH = 13, /* slider handle button thickness */
	UI_SLIDER_RAIL_TH = 7, /* slider rail thickness */
	UI_SLIDER_RAIL_POS_Y = 22, /* rail distance from top edge of the slider widget */
	UI_SLIDER_LABEL_POS_Y = -1,
	UI_FOCUS_BORDER_R = 1, /* how thich edges to draw around highlighted items */
	UI_LABEL_OFFSET_X = 5, /* where to draw text on any buttons */
	UI_LABEL_OFFSET_Y = 5,
	UI_GC_LINE_DODGE = 15 /* how much the graph connector lines can be moved to dodge other widgets */
};

#define UI_CHECKBOX_YES "YES"
#define UI_CHECKBOX_NO " NO"

enum {
	UI_COLOR_DEBUG = 0xFF0000,
	UI_COLOR_FOCUS_BORDER = 0xFFFFFF,
	
	UI_COLOR_BG = 0x3a4041,
	UI_COLOR_BG_HOVER = 0x3a4041,
	
	UI_COLOR_BUTTON = 0x797979,
	UI_COLOR_BUTTON_HOVER = 0x8c9597,
	
	UI_COLOR_TEXTBOX = 0x7e78c3,
	UI_COLOR_TEXTBOX_HOVER = 0xa098f7,
	
	UI_COLOR_SLIDER_RAIL = 0x505050,
	UI_COLOR_SLIDER_BUTTON = 0xCCCCCC,
	
	/* graph connector */
	UI_COLOR_GC = 0xa3b842,
	UI_COLOR_GC_HOVER = 0xb8cf4b,
	UI_COLOR_GC_LINKED = 0xd09744,
	UI_COLOR_GC_LINKED_HOVER = 0xd9bd4e,
	UI_COLOR_GC_LINES = 0xFFFF00,
	UI_COLOR_GC_LINES_MOVING = 0xFFFFFF
};

#endif
