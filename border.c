///
///	@file border.c		@brief client window border functions
///
///	Copyright (c) 2009 - 2011 by Lutz Sammer.  All Rights Reserved.
///
///	Contributor(s):
///
///	License: AGPLv3
///
///	This program is free software: you can redistribute it and/or modify
///	it under the terms of the GNU Affero General Public License as
///	published by the Free Software Foundation, either version 3 of the
///	License.
///
///	This program is distributed in the hope that it will be useful,
///	but WITHOUT ANY WARRANTY; without even the implied warranty of
///	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
///	GNU Affero General Public License for more details.
///
///	$Id$
//////////////////////////////////////////////////////////////////////////////

///
///	@ingroup client
///	@defgroup border The window border module.
///
///	This module handles the window borders.
///
/// @{

#include <xcb/xcb.h>
#include "uwm.h"

#include <stdio.h>
#include <stdlib.h>

#include <xcb/xcb_icccm.h>
#include <xcb/xcb_image.h>
#ifdef USE_SHAPE
#include <xcb/shape.h>
#endif

#include "queue.h"
#include "core-array/core-array.h"
#include "core-rc/core-rc.h"

#include "misc.h"
#include "draw.h"
#include "image.h"
#include "pointer.h"
#include "client.h"
#include "moveresize.h"
#include "border.h"
#include "icon.h"
#include "menu.h"

#include "readable_bitmap.h"

// ------------------------------------------------------------------------ //
// Declares
// ------------------------------------------------------------------------ //

#define BORDER_TITLE_SPACE 3		///< space arround title icons

/**
**	Border icon typedef
*/
typedef struct _border_button_ BorderButton;

/**
**	Border icon structure
*/
struct _border_button_
{
    const char *Name;			///< name tag of border button icon
    char *FileName;			///< filename of external bitmap
    /// compiled/default internal bitmap
    uint8_t Bitmap[BORDER_BUTTON_WIDTH * BORDER_BUTTON_HEIGHT / 8];
    xcb_pixmap_t Pixmap;		///< pixmap on x11 server
};

/**
**	Border icon table.
*/
typedef struct _border_button_table_
{
    BorderButton Close;			///< close button
    BorderButton Minimize;		///< minimize button
    BorderButton Maximize;		///< maximize button
    BorderButton MaximizeActive;	///< maximize active button
    BorderButton Sticky;		///< sticky button
    BorderButton StickyActive;		///< sticky active button
} BorderButtonTable;

/**
**	Table of default border icons.
*/
static BorderButtonTable BorderButtons = {
    .Close = {"close", NULL, {
		    ________, ________,
		    ________, ________,
		    ________, ________,
		    ________, ________,
		    ________, ________,
		    ________, ________,
		    ____OO__, __OO____,
		    _____OO_, _OO_____,
		    ______OO, OO______,
		    _______O, O_______,
		    ______OO, OO______,
		    _____OO_, _OO_____,
		    ____OO__, __OO____,
		    ________, ________,
		    ________, ________,
	    ________, ________}, XCB_NONE},
    .Minimize = {"minimize", NULL, {
		    ________, ________,
		    ________, ________,
		    ________, ________,
		    ________, ________,
		    ________, ________,
		    ________, ________,
		    ________, ________,
		    ________, ________,
		    ________, ________,
		    ________, ________,
		    ________, ________,
		    ________, ________,
		    ____OOOO, OOOO____,
		    ____OOOO, OOOO____,
		    ________, ________,
	    ________, ________}, XCB_NONE},
    .Maximize = {"maximize", NULL, {
		    ________, ________,
		    ________, ________,
		    ________, ________,
		    ________, ________,
		    ________, ________,
		    ____OOOO, OOOO____,
		    ____O___, ___O____,
		    ____O___, ___O____,
		    ____O___, ___O____,
		    ____O___, ___O____,
		    ____O___, ___O____,
		    ____O___, ___O____,
		    ____OOOO, OOOO____,
		    ________, ________,
		    ________, ________,
	    ________, ________}, XCB_NONE},
    .MaximizeActive = {"maximize-active", NULL, {
		    ________, ________,
		    ________, ________,
		    ________, ________,
		    ________, ________,
		    ________, ________,
		    ______OO, OOOO____,
		    ______OO, OOOO____,
		    ________, ___O____,
		    ____OOOO, OO_O____,
		    ____OOOO, OO_O____,
		    ____O___, _O_O____,
		    ____O___, _O_O____,
		    ____O___, _O______,
		    ____OOOO, OO______,
		    ________, ________,
	    ________, ________}, XCB_NONE},
    .Sticky = {"sticky", NULL, {
		    ________, ________,
		    ________, ________,
		    ________, ________,
		    ________, ________,
		    ________, ________,
		    _____OOO, OOO_____,
		    ____OO__, __OO____,
		    ___OO___, ___OO___,
		    ___O____, O___O___,
		    ___O___O, ____O___,
		    ___OO___, ___OO___,
		    ____OO__, __OO____,
		    _____OOO, OOO_____,
		    ________, ________,
		    ________, ________,
	    ________, ________}, XCB_NONE},
    .StickyActive = {"sticky-active", NULL, {
		    ________, ________,
		    ________, ________,
		    ________, ________,
		    ________, ________,
		    ________, ________,
		    _____OOO, OOO_____,
		    ____OO__, __OO____,
		    ___OO__O, O__OO___,
		    ___O__OO, OO__O___,
		    ___O__OO, OO__O___,
		    ___OO__O, O__OO___,
		    ____OO__, __OO____,
		    _____OOO, OOO_____,
		    ________, ________,
		    ________, ________,
	    ________, ________}, XCB_NONE},
};

static xcb_gcontext_t BorderGC;		///< border graphic context

#if defined(USE_SHAPE) && defined(USE_XMU)
static xcb_pixmap_t ShapePixmap;	///< shape pixmap of border
static unsigned ShapePixmapWidth;	///< shape pixmap width
static unsigned ShapePixmapHeight;	///< shape pixmap height
static xcb_gcontext_t ShapeGC;		///< shape graphic context
#endif

    /// width of window border
static int BorderWidth;

    /// height of window title
static int BorderTitleHeight;

    /// width and height of resize corner
static int BorderCornerSize;

    /// border button icon width
static const int BorderButtonWidth = BORDER_BUTTON_WIDTH;

#ifdef USE_PIXMAN
    /// FIXME: not written yet
static pixman_region16_t BorderRegion;

static xcb_rectangle_t *BorderRectangles;	///< clip rectangles for border
static int BorderRectangleN;		///< number of clip rectangles

#else
static void *BorderRegion;		///< dummy update region
#endif

#if defined(USE_SHAPE) && defined(USE_XMU)	// {

/**
**	Clear shape mask of a window.
**
**	@param window	window to reset shape
*/
void ShapeRoundedRectReset(xcb_window_t window)
{
    xcb_shape_mask(Connection, XCB_SHAPE_SO_SET, XCB_SHAPE_SK_BOUNDING, window,
	0, 0, XCB_NONE);
}

/**
**	Set shape mask on window to give rounded border.
**
**	@param window	window to shape
**	@param width	width of window
**	@param height	height of window
*/
void ShapeRoundedRectWindow(xcb_window_t window, unsigned width,
    unsigned height)
{
    uint32_t pixel;
    xcb_rectangle_t rectangle;

    // need bigger pixmap for shape
    if (width > ShapePixmapWidth || height > ShapePixmapHeight) {
	if (ShapePixmap != XCB_NONE) {
	    xcb_free_pixmap(Connection, ShapePixmap);
	}
	ShapePixmap = xcb_generate_id(Connection);
	xcb_create_pixmap(Connection, 1, ShapePixmap, XcbScreen->root, width,
	    height);
	if (ShapeGC == XCB_NONE) {
	    ShapeGC = xcb_generate_id(Connection);
	    xcb_create_gc(Connection, ShapeGC, ShapePixmap, 0, NULL);
	}
    }

    pixel = 0;
    xcb_change_gc(Connection, ShapeGC, XCB_GC_FOREGROUND, &pixel);
    rectangle.x = 0;
    rectangle.y = 0;
    rectangle.width = width;
    rectangle.height = height;
    xcb_poly_fill_rectangle(Connection, ShapePixmap, ShapeGC, 1, &rectangle);

    pixel = 1;
    xcb_change_gc(Connection, ShapeGC, XCB_GC_FOREGROUND, &pixel);
    // corner bound radius -1 to allow slightly better outline drawing
    xmu_fill_rounded_rectangle(Connection, ShapePixmap, ShapeGC, 0, 0, width,
	height, CORNER_RADIUS - 1, CORNER_RADIUS - 1);

    xcb_shape_mask(Connection, XCB_SHAPE_SO_SET, XCB_SHAPE_SK_BOUNDING, window,
	0, 0, ShapePixmap);
}

/**
**	Subtract shape mask on window to give rounded border.
**
**	@param window	window to shape
**	@param width	width of window
**	@param height	height of window
*/
void ShapeRoundedRectSubtract(xcb_window_t window, unsigned width,
    unsigned height)
{
    uint32_t pixel;
    xcb_rectangle_t rectangle;

    // need bigger pixmap for shape
    if (width > ShapePixmapWidth || height > ShapePixmapHeight) {
	if (ShapePixmap != XCB_NONE) {
	    xcb_free_pixmap(Connection, ShapePixmap);
	}
	ShapePixmap = xcb_generate_id(Connection);
	xcb_create_pixmap(Connection, 1, ShapePixmap, XcbScreen->root, width,
	    height);
	if (ShapeGC == XCB_NONE) {
	    ShapeGC = xcb_generate_id(Connection);
	    xcb_create_gc(Connection, ShapeGC, ShapePixmap, 0, NULL);
	}
    }

    pixel = 1;
    xcb_change_gc(Connection, ShapeGC, XCB_GC_FOREGROUND, &pixel);
    rectangle.x = 0;
    rectangle.y = 0;
    rectangle.width = width;
    rectangle.height = height;
    xcb_poly_fill_rectangle(Connection, ShapePixmap, ShapeGC, 1, &rectangle);

    pixel = 0;
    xcb_change_gc(Connection, ShapeGC, XCB_GC_FOREGROUND, &pixel);
    // corner bound radius -1 to allow slightly better outline drawing
    xmu_fill_rounded_rectangle(Connection, ShapePixmap, ShapeGC, 0, 0, width,
	height, CORNER_RADIUS - 1, CORNER_RADIUS - 1);

    xcb_shape_mask(Connection, XCB_SHAPE_SO_SUBTRACT, XCB_SHAPE_SK_BOUNDING,
	window, 0, 0, ShapePixmap);
}

#else // }{ USE_SHAPE && USE_XMU

    /// Dummy for clear shape mask of a window.
#define ShapeRoundedRectReset(window)
    /// Dummy for set shape mask on window to give rounded border.
#define ShapeRoundedRectWindow(window, width, height)
    /// Dummy for subtract shape mask on window to give rounded border.
#define ShapeRoundedRectSubtract(window, width, height)

#endif // } !(USE_SHAPE && USE_XMU)

/**
**	Get size of icon to display in window title.
**
**	@returns the size in pixels (note that icons are square).
*/
static inline int BorderGetIconSize(void)
{
    return BorderTitleHeight - 2 * BORDER_TITLE_SPACE;
}

/**
**	Get size of borders for client.
**
**	@param client		client
**	@param[out] north	returns north border size
**	@param[out] south	returns south border size
**	@param[out] east	returns east border size
**	@param[out] west	returns west border size
*/
void BorderGetSize(const Client * client, int *north, int *south, int *east,
    int *west)
{
    *north = 0;
    *south = 0;
    *east = 0;
    *west = 0;				// default border size

    // full screen is a special case
    if (client->State & WM_STATE_FULLSCREEN) {
	return;
    }

    if (client->Border & BORDER_OUTLINE) {
	*north = BorderWidth;
	*south = BorderWidth;
	*east = BorderWidth;
	*west = BorderWidth;
    }

    if (client->Border & BORDER_TITLE) {
	*north = BorderTitleHeight;
    }

    if (client->State & WM_STATE_SHADED) {
	*south = 0;
    }
}

/**
**	Get the size of the window title border.
**
**	Only used for cascade window placement.
**
**	@returns the height in pixel of the default title bar.
*/
int BorderGetTitleSize(void)		// __attribute__ ((pure))
{
    return BorderWidth + BorderTitleHeight;
}

/**
**	Draw button icon of window title.
**
**	@param window	frame window.
**	@param gc	graphical context (border)
**	@param pixel	foreground color
**	@param pixmap	icon pixmap as clip mask
**	@param xoffset	x offset into frame window
**	@param yoffset	y offset into frame window
*/
static void BorderDrawTitleButton(xcb_drawable_t window, xcb_gcontext_t gc,
    uint32_t pixel, xcb_pixmap_t pixmap, int xoffset, int yoffset)
{
    uint32_t values[4];
    xcb_rectangle_t rectangle;

    values[0] = pixel;
    values[1] = xoffset + yoffset;
    values[2] = yoffset;
    values[3] = pixmap;
    xcb_change_gc(Connection, gc,
	XCB_GC_FOREGROUND | XCB_GC_CLIP_ORIGIN_X | XCB_GC_CLIP_ORIGIN_Y |
	XCB_GC_CLIP_MASK, values);

    rectangle.x = xoffset + yoffset;
    rectangle.y = yoffset;
    rectangle.width = BORDER_BUTTON_WIDTH;
    rectangle.height = BORDER_BUTTON_HEIGHT;
    xcb_poly_fill_rectangle(Connection, window, gc, 1, &rectangle);

    values[0] = XCB_NONE;
    xcb_change_gc(Connection, gc, XCB_GC_CLIP_MASK, values);
}

/**
**	Draw window border decoration.
**
**	Draw title bar with application icon, title text and minimize,
**	maximize and close button.
**	Draw resize corner and window border.
**
**	@param client		client to decorate
**	@param draw_icon	true draw window client icon
*/
static void BorderDrawBorder(const Client * client, int draw_icon)
{
    int icon_size;
    int north;
    int south;
    int east;
    int west;
    unsigned width;
    unsigned height;
    uint32_t text_pixel;
    uint32_t title_pixel1;
    uint32_t title_pixel2;
    uint32_t outline_pixel;
    uint32_t corner_pixel;
    xcb_rectangle_t rectangles[2];

    icon_size = BorderGetIconSize();
    BorderGetSize(client, &north, &south, &east, &west);

    width = client->Width + west + east;
    height = client->Height + north + south;

    // determine colors and gradients to use
    if (client->State & WM_STATE_ACTIVE) {
	text_pixel = Colors.TitleActiveFG.Pixel;
	title_pixel1 = Colors.TitleActiveBG1.Pixel;
	title_pixel2 = Colors.TitleActiveBG2.Pixel;
	outline_pixel = Colors.BorderActiveLine.Pixel;
	corner_pixel = Colors.BorderActiveCorner.Pixel;
    } else {
	text_pixel = Colors.TitleFG.Pixel;
	title_pixel1 = Colors.TitleBG1.Pixel;
	title_pixel2 = Colors.TitleBG2.Pixel;
	outline_pixel = Colors.BorderLine.Pixel;
	corner_pixel = Colors.BorderCorner.Pixel;
    }

    // set window background color (to reduce flickering)
    xcb_change_window_attributes(Connection, client->Parent, XCB_CW_BACK_PIXEL,
	&title_pixel2);

    // draw outside border (clear window with right color)
    xcb_change_gc(Connection, BorderGC, XCB_GC_FOREGROUND, &title_pixel2);
    rectangles[0].x = 0;
    rectangles[0].y = 0;
    rectangles[0].width = width;
    rectangles[0].height = height;
    xcb_poly_fill_rectangle(Connection, client->Parent, BorderGC, 1,
	rectangles);
    // FIXME: check if 4 rectangles are faster than 1 big?

    // windows without title have no icon, text and buttons
    if (client->Border & BORDER_TITLE) {
	int title_width;
	int yoffset;

	yoffset = BorderTitleHeight / 2 - BORDER_BUTTON_HEIGHT / 2;

	// draw buttons and determine how many pixels may be used for title
	// [space] [icon] [space] [title...] [space] [buttons...] [space]

	// caclulate starting position of rightmost title icon
	title_width = width - 1 - BORDER_TITLE_SPACE - BorderButtonWidth;

	// window close button
	if (title_width > BorderButtonWidth && (client->Border & BORDER_CLOSE)) {
	    BorderDrawTitleButton(client->Parent, BorderGC, text_pixel,
		BorderButtons.Close.Pixmap, title_width, yoffset);
	    title_width -= BorderButtonWidth;
	}
	// window maximize button
	if (title_width > BorderButtonWidth
	    && (client->Border & (BORDER_MAXIMIZE_VERT |
		    BORDER_MAXIMIZE_HORZ))) {
	    BorderDrawTitleButton(client->Parent, BorderGC, text_pixel,
		(client->State & (WM_STATE_MAXIMIZED_HORZ |
			WM_STATE_MAXIMIZED_VERT))
		? BorderButtons.MaximizeActive.Pixmap : BorderButtons.
		Maximize.Pixmap, title_width, yoffset);
	    title_width -= BorderButtonWidth;
	}
	// window minimize button
	if (title_width > BorderButtonWidth
	    && (client->Border & BORDER_MINIMIZE)) {
	    BorderDrawTitleButton(client->Parent, BorderGC, text_pixel,
		BorderButtons.Minimize.Pixmap, title_width, yoffset);
	    title_width -= BorderButtonWidth;
	}
	// window sticky button
	if (title_width > BorderButtonWidth
	    && (client->Border & BORDER_STICKY)) {
	    BorderDrawTitleButton(client->Parent, BorderGC, text_pixel,
		(client->State & WM_STATE_STICKY)
		? BorderButtons.StickyActive.Pixmap : BorderButtons.
		Sticky.Pixmap, title_width, yoffset);
	    title_width -= BorderButtonWidth;
	}

	title_width += 1 + BORDER_TITLE_SPACE + BorderButtonWidth;
	title_width -= icon_size + 4 * BORDER_TITLE_SPACE;

	// draw title bar
	if (title_pixel1 != title_pixel2) {
	    // keep 1 pixel border
	    GradientDrawHorizontal(client->Parent, BorderGC, title_pixel1,
		title_pixel2, 1, 1, width - 2, BorderTitleHeight - 2);
	}
#ifdef USE_ICON
	// draw client icon
	if (client->Icon && width >= (unsigned)BorderTitleHeight && draw_icon) {
	    IconDraw(client->Icon, client->Parent, BORDER_TITLE_SPACE,
		BorderTitleHeight / 2 - icon_size / 2, icon_size, icon_size);
	}
#endif
	// draw client name
	if (client->Name && client->Name[0] && title_width > 0) {
	    FontDrawString(client->Parent, &Fonts.Titlebar, text_pixel,
		icon_size + BORDER_TITLE_SPACE * 2,
		BorderTitleHeight / 2 - Fonts.Titlebar.Height / 2, title_width,
		BorderRegion, client->Name);
	}
    }
    //
    //	Draw resize area
    //	FIXME: what happens with BorderWidth = 1
    //	FIXME: support drawing no corners
    //	FIXME: draw multiple rectangles faster than one big?
    //	I draw a rectangle instead of a L.
    //
    if (client->Border & BORDER_RESIZE && !(client->State & WM_STATE_SHADED)
	&& client->Width >= BorderCornerSize * 2
	&& client->Height >= BorderCornerSize * 2) {

	xcb_change_gc(Connection, BorderGC, XCB_GC_FOREGROUND, &corner_pixel);

	// south west
	rectangles[0].x = 1;
	rectangles[0].y = height - BorderCornerSize;
	rectangles[0].width = BorderCornerSize - 1;
	rectangles[0].height = BorderCornerSize - 1;
	// south east
	rectangles[1].x = width - BorderCornerSize;
	rectangles[1].y = height - BorderCornerSize;
	rectangles[1].width = BorderCornerSize - 1;
	rectangles[1].height = BorderCornerSize - 1;
	xcb_poly_fill_rectangle(Connection, client->Parent, BorderGC, 2,
	    rectangles);

	// corner outline
	xcb_change_gc(Connection, BorderGC, XCB_GC_FOREGROUND, &outline_pixel);

	rectangles[0].x = 0;
	rectangles[0].y = height - BorderCornerSize - 1;
	rectangles[0].width = BorderCornerSize;
	rectangles[0].height = BorderCornerSize;

	rectangles[1].x = width - BorderCornerSize - 1;
	rectangles[1].y = height - BorderCornerSize - 1;
	rectangles[1].width = BorderCornerSize;
	rectangles[1].height = BorderCornerSize;
	xcb_poly_rectangle(Connection, client->Parent, BorderGC, 2,
	    rectangles);
    } else {
	// window outline
	xcb_change_gc(Connection, BorderGC, XCB_GC_FOREGROUND, &outline_pixel);
    }

#if defined(USE_SHAPE) && defined(USE_XMU)
    if (client->State & WM_STATE_SHADED) {
	xmu_draw_rounded_rectangle(Connection, client->Parent, BorderGC, 0, 0,
	    width - 1, north - 1, CORNER_RADIUS, CORNER_RADIUS);
    } else {
	xmu_draw_rounded_rectangle(Connection, client->Parent, BorderGC, 0, 0,
	    width - 1, height - 1, CORNER_RADIUS, CORNER_RADIUS);
    }
#else
    rectangles[0].x = 0;
    rectangles[0].y = 0;
    rectangles[0].width = width - 1;
    if (client->State & WM_STATE_SHADED) {
	rectangles[0].height = north - 1;
    } else {
	rectangles[0].height = height - 1;
    }
    xcb_poly_rectangle(Connection, client->Parent, BorderGC, 1, rectangles);
#endif

}

/**
**	Draw client window border.
**
**	@param client	client whose frame to draw
**	@param expose	X11 expose event causing redraw (or NULL)
*/
void BorderDraw(const Client * client, const xcb_expose_event_t * expose)
{
    int draw_icon;

    // don't draw any more, if we are exiting
    if (!KeepLooping
	// must be either mapped or shaded to have border
	|| !(client->State & (WM_STATE_MAPPED | WM_STATE_SHADED))
	// hidden and fullscreen windows don't get borders
	|| (client->State & (WM_STATE_HIDDEN | WM_STATE_FULLSCREEN))
	// there is no border to draw
	|| !(client->Border & (BORDER_TITLE | BORDER_OUTLINE))) {
	return;
    }

    if (expose) {
	// we return now if there are more expose events coming
	if (expose->count) {
	    return;
	}
	Debug(2, "%s: FIXME: expose not optimized\n", __FUNCTION__);

	//
	// FIXME:
	//	Use pixman to build a list of rectangles to update
	//	create a clipmask from this rectagle list
	//	draw the border.

	draw_icon = 1;
    } else {				// an expose event did not occur
	// redraw everything
	uint32_t value[1];

	draw_icon = 1;
	value[0] = XCB_NONE;
	xcb_change_gc(Connection, BorderGC, XCB_GC_CLIP_MASK, value);
    }

    // do actual drawing
    BorderDrawBorder(client, draw_icon);

    // we no longer need region, release it
    if (expose) {
	// FIXME: free pixmap rectangle list
	BorderRegion = NULL;
    }
}

// ------------------------------------------------------------------------ //
// Actions

/**
**	Get cursor for resizing on specified frame location.
**
**	@param action	get cursor for this resize action
**
**	@returns x11 cursor id for the action.
*/
static xcb_cursor_t BorderGetResizeCursor(BorderAction action)
{
    if (action & BORDER_ACTION_RESIZE_N) {
	if (action & BORDER_ACTION_RESIZE_E) {
	    return Cursors.NorthEast;
	}
	if (action & BORDER_ACTION_RESIZE_W) {
	    return Cursors.NorthWest;
	}
	return Cursors.North;
    }
    if (action & BORDER_ACTION_RESIZE_S) {
	if (action & BORDER_ACTION_RESIZE_E) {
	    return Cursors.SouthEast;
	}
	if (action & BORDER_ACTION_RESIZE_W) {
	    return Cursors.SouthWest;
	}
	return Cursors.South;
    }
    if (action & BORDER_ACTION_RESIZE_E) {
	return Cursors.East;
    }
    return Cursors.West;
}

/**
**	Get cursor for specified action on frame.
**
**	@param action	get cursor for this action
**
**	@returns x11 cursor id for the action.
*/
xcb_cursor_t BorderGetCursor(BorderAction action)
{
    switch (action & BORDER_ACTION_MASK) {
	case BORDER_ACTION_RESIZE:
	    return BorderGetResizeCursor(action);
	case BORDER_ACTION_CLOSE:
	case BORDER_ACTION_MAXIMIZE:
	case BORDER_ACTION_MINIMIZE:
	case BORDER_ACTION_MOVE:
	default:
	    break;
    }
    return Cursors.Default;
}

/**
**	Request pointer grab for resizing window.
**
**	@param action	grab cursor for this action
**
**	@returns cookie for grab request
**
**	@todo FIXME: move into pointer modul
*/
xcb_grab_pointer_cookie_t PointerGrabForResizeRequest(int action)
{
    return PointerGrabRequest(XcbScreen->root, BorderGetCursor(action));
}

/**
**	Determine border action to take given coordinates.
**
**	@param client	client
**	@param x	x-coordinate of mouse (frame relative)
**	@param y	y-coordinate of mouse (frame relative)
**
**	@returns action to take.
*/
BorderAction BorderGetAction(const Client * client, int x, int y)
{
    int north;
    int south;
    int east;
    int west;
    int offset;
    int width;
    int height;

    BorderGetSize(client, &north, &south, &east, &west);
    width = client->Width;

    // check title bar actions
    if (client->Border & BORDER_TITLE) {
	// check buttons on title bar
	if (y >= BORDER_TITLE_SPACE && y <= BorderTitleHeight) {
#ifdef USE_ICON
	    // menu button
	    if (client->Icon && width >= BorderTitleHeight) {
		// BorderTitleHeight = space icon-size space
		if (x >= BORDER_TITLE_SPACE && x <= BorderTitleHeight) {
		    return BORDER_ACTION_MENU;
		}
	    }
#endif
	    // close button
	    offset =
		width + west + east - BORDER_TITLE_SPACE - BorderButtonWidth;
	    if ((client->Border & BORDER_CLOSE) && offset > BorderButtonWidth) {
		if (x > offset && x < offset + BorderButtonWidth) {
		    return BORDER_ACTION_CLOSE;
		}
		offset -= BorderButtonWidth;
	    }
	    // maximize button
	    if ((client->Border & (BORDER_MAXIMIZE_VERT |
			BORDER_MAXIMIZE_HORZ))
		&& offset > BorderButtonWidth) {
		if (x > offset && x < offset + BorderButtonWidth) {
		    return BORDER_ACTION_MAXIMIZE;
		}
		offset -= BorderButtonWidth;
	    }
	    // minimize button
	    if ((client->Border & BORDER_MINIMIZE)
		&& offset > BorderButtonWidth) {
		if (x > offset && x < offset + BorderButtonWidth) {
		    return BORDER_ACTION_MINIMIZE;
		}
		offset -= BorderButtonWidth;
	    }
	    // sticky button
	    if ((client->Border & BORDER_STICKY)
		&& offset > BorderButtonWidth) {
		if (x > offset && x < offset + BorderButtonWidth) {
		    return BORDER_ACTION_STICKY;
		}
	    }
	}
	// check for move
	if (y >= BORDER_TITLE_SPACE && y <= BorderTitleHeight) {
	    if (x >= BORDER_TITLE_SPACE
		&& x < width + west + east - BORDER_TITLE_SPACE) {
		if (client->Border & BORDER_MOVE) {
		    return BORDER_ACTION_MOVE;
		} else {
		    return BORDER_ACTION_NONE;
		}
	    }
	}
    }
    // now we check resize actions.
    // there is no need to go further, if resizing isn't allowed
    if (!(client->Border & BORDER_RESIZE)) {
	return BORDER_ACTION_NONE;
    }

    height = client->Height;

    // FIXME: window to small, just use corner resize? (inverse of now!)
    // check south east/west and north east/west resizing
    if (width >= BorderCornerSize * 2 && height >= BorderCornerSize * 2) {
	if (y > height + north + south - BorderCornerSize) {
	    if (x < BorderCornerSize) {
		return BORDER_ACTION_RESIZE_S | BORDER_ACTION_RESIZE_W |
		    BORDER_ACTION_RESIZE;
	    } else if (x > width + west + east - BorderCornerSize) {
		return BORDER_ACTION_RESIZE_S | BORDER_ACTION_RESIZE_E |
		    BORDER_ACTION_RESIZE;
	    }
	} else if (y < BorderCornerSize) {
	    if (x < BorderCornerSize) {
		return BORDER_ACTION_RESIZE_N | BORDER_ACTION_RESIZE_W |
		    BORDER_ACTION_RESIZE;
	    } else if (x > width + west + east - BorderCornerSize) {
		return BORDER_ACTION_RESIZE_N | BORDER_ACTION_RESIZE_E |
		    BORDER_ACTION_RESIZE;
	    }
	}
    }
    // check east, west, north, and south resizing
    if (x <= west) {
	return BORDER_ACTION_RESIZE_W | BORDER_ACTION_RESIZE;
    } else if (x >= width + west) {
	return BORDER_ACTION_RESIZE_E | BORDER_ACTION_RESIZE;
    } else if (y >= height + north) {
	return BORDER_ACTION_RESIZE_S | BORDER_ACTION_RESIZE;
    } else if (y <= south) {
	return BORDER_ACTION_RESIZE_N | BORDER_ACTION_RESIZE;
    } else {
	return BORDER_ACTION_NONE;
    }
}

/**
**	Take appropriate action for click on client border.
**
**	@param client	client which border gets a button press
**	@param event	X11 button press event
*/
void BorderHandleButtonPress(Client * client,
    const xcb_button_press_event_t * event)
{
    BorderAction action;
    int border;

    action = BorderGetAction(client, event->event_x, event->event_y);
    Debug(3, "%s: action = %d\n", __FUNCTION__, action);
    switch (action & BORDER_ACTION_MASK) {
	case BORDER_ACTION_RESIZE:
	    ClientResizeLoop(client, event->detail, action, event->event_x,
		event->event_y);
	    break;
	case BORDER_ACTION_MOVE:
	    // double click
	    if (event->state & (1 << 7) << event->detail) {
		// FIXME: make configurable
		ClientMaximizeDefault(client);
	    } else {
		ClientMoveLoop(client, event->detail, event->event_x,
		    event->event_y);
	    }
	    break;
	case BORDER_ACTION_MENU:
	    if (client->Border & BORDER_OUTLINE) {
		border = BorderWidth;
	    } else {
		border = 0;
	    }
	    WindowMenuShow(NULL, client->X + event->event_x - border,
		client->Y + event->event_y - BorderTitleHeight - border,
		client);
	    break;
	case BORDER_ACTION_CLOSE:
	    ClientDelete(client);
	    break;
	case BORDER_ACTION_MAXIMIZE:
	    ClientMaximizeDefault(client);
	    break;
	case BORDER_ACTION_MINIMIZE:
	    ClientMinimize(client);
	    break;
	case BORDER_ACTION_STICKY:
	    if (client->State & WM_STATE_STICKY) {
		ClientSetSticky(client, 0);
	    } else {
		ClientSetSticky(client, 1);
	    }
	    break;
	default:
	    Debug(2, "unknown border action %d\n", action);
	    break;
    }
}

/**
**	Show window menu relative to border.
**
**	@param client	client of window menu
**	@param x	x-coordinate for menu (client relative)
**	@param y	y-coordinate for menu (client relative)
*/
void BorderShowMenu(Client * client, int x, int y)
{
    int north;
    int south;
    int east;
    int west;

    BorderGetSize(client, &north, &south, &east, &west);
    WindowMenuShow(NULL, client->X + x - west, client->Y + y - north, client);
}

// ---------------------------------------------------------------------------

/**
**	Initialize border module.
*/
void BorderInit(void)
{
    BorderButton *button;
    uint32_t value[1];

    if (!BorderTitleHeight) {		// config 0
#ifdef DEBUG
	if (!Fonts.Titlebar.Height) {
	    Debug(0, "border needs font to be initialized already\n");
	}
#endif
	BorderTitleHeight = Fonts.Titlebar.Height + 2 * BORDER_TITLE_SPACE;
    }

    for (button = &BorderButtons.Close; button <= &BorderButtons.StickyActive;
	++button) {
	if (button->FileName) {
	    Debug(2, "FIXME: should load '%s' for %s.\n", button->FileName,
		button->Name);
	    //
	    //	FIXME: read .xbm file format, convert it into bitmap.
	    //
	}
	button->Pixmap =
	    xcb_create_pixmap_from_bitmap_data(Connection, XcbScreen->root,
	    (uint8_t *) button->Bitmap, BORDER_BUTTON_WIDTH,
	    BORDER_BUTTON_HEIGHT, 1, 1, 0, NULL);
    }

    BorderGC = xcb_generate_id(Connection);
    value[0] = 0;
    xcb_create_gc(Connection, BorderGC, XcbScreen->root,
	XCB_GC_GRAPHICS_EXPOSURES, value);
}

/**
**	Cleanup border module.
*/
void BorderExit(void)
{
    BorderButton *button;

    for (button = &BorderButtons.Close; button <= &BorderButtons.StickyActive;
	++button) {
	free(button->FileName);
	button->FileName = NULL;
	if (button->Pixmap) {
	    xcb_free_pixmap(Connection, button->Pixmap);
	    button->Pixmap = XCB_NONE;
	}
    }

    xcb_free_gc(Connection, BorderGC);
    BorderGC = XCB_NONE;

#if defined(USE_SHAPE) && defined(USE_XMU)
    if (ShapePixmap != XCB_NONE) {
	xcb_free_pixmap(Connection, ShapePixmap);
	ShapePixmap = XCB_NONE;
    }
    if (ShapeGC != XCB_NONE) {
	xcb_free_pixmap(Connection, ShapeGC);
	ShapeGC = XCB_NONE;
    }
    ShapePixmapWidth = 0;
    ShapePixmapHeight = 0;
#endif
}

// ------------------------------------------------------------------------ //
// Config

#ifdef USE_RC				// {

/**
**	Parse window border configuration.
**
**	@param config	global config dictionary
**
**	@todo FIXME: configuration of corner-width
**	@todo FIXME: configuration of button icons and loading.
*/
void BorderConfig(const Config * config)
{
    ssize_t ival;

    // window border width
    BorderWidth = BORDER_DEFAULT_WIDTH;
    if (ConfigGetInteger(ConfigDict(config), &ival, "border", "width", NULL)) {
	if (ival < BORDER_MINIMAL_WIDTH || ival > BORDER_MAXIMAL_WIDTH) {
	    Warning("border width %zd out of range\n", ival);
	} else {
	    BorderWidth = ival;
	}
    }
    // resize corner size
    BorderCornerSize = BORDER_DEFAULT_TITLE_HEIGHT - 1;
    if (BorderCornerSize < BorderWidth) {
	// FIXME: when i draw ther corner as L, i can make it smaller
	BorderCornerSize = BorderWidth + 1;
    }
    // window title height
    BorderTitleHeight = BORDER_DEFAULT_TITLE_HEIGHT;
    if (ConfigGetInteger(ConfigDict(config), &ival, "border", "title-height",
	    NULL)) {
	// 0 is valid as default: use font height
	if (ival && (ival < BORDER_MINIMAL_TITLE_HEIGHT
		|| ival > BORDER_MAXIMAL_TITLE_HEIGHT)) {
	    Warning("border title height %zd out of range\n", ival);
	} else {
	    BorderTitleHeight = ival;
	}
    }
    // FIXME: look for user icon names.
}

#endif // } USE_RC

/// @}
