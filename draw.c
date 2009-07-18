///
///	@file draw.c	@brief drawing functions
///
///	Copyright (c) 2009 by Johns.  All Rights Reserved.
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

///	@defgroup draw The drawing module.
///
///	This modules contains all X11 drawing functions.
///
/// @{

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <xcb/xcb.h>
#include <xcb/xcbext.h>
#include <xcb/xcb_event.h>

#include "uwm.h"
#include "draw.h"

// ------------------------------------------------------------------------ //
// XCB
// ------------------------------------------------------------------------ //

// only in newer releases included
#ifndef XCB_EVENT_ERROR_BAD_IMPLEMENTATION

/**
* @brief Convert an event error type to a label.
* @param type The erro type.
* @return A string with the event name, or NULL if unknown or if the event is
* not an error.
*/
const char *xcb_event_get_error_label(uint8_t type);
#endif

/**
**	Draw poly text.
**
**	This a simple version of xcb_poly_text_8, which works only with
**	one string with less equal 254 characters.
**
**	Request:
**
**	- drawable: DRAWABLE
**	- gc: GCONTEXT
**	- x, y: INT16
**	- items: (LISTofTEXTITEM8)
**
**	- TEXTELT8: TEXTELT8: [delta: INT8 string: STRING8]
**
**	- FONT: 5 bytes 255 + FontID
**
**	- Len8 Delta8 String8 ...
**
*/
xcb_void_cookie_t xcb_poly_text_8_simple(xcb_connection_t * c,
    xcb_drawable_t drawable, xcb_gcontext_t gc, int16_t x, int16_t y,
    uint32_t len, const char *str)
{
    static const xcb_protocol_request_t xcb_req = {
	/* count */ 5,
	/* ext */ 0,
	/* opcode */ XCB_POLY_TEXT_8,
	/* isvoid */ 1
    };
    struct iovec xcb_parts[7];
    uint8_t xcb_lendelta[2];
    xcb_void_cookie_t xcb_ret;
    xcb_poly_text_8_request_t xcb_out;

    xcb_out.pad0 = 0;
    xcb_out.drawable = drawable;
    xcb_out.gc = gc;
    xcb_out.x = x;
    xcb_out.y = y;

    xcb_lendelta[0] = len;
    xcb_lendelta[1] = 0;

    xcb_parts[2].iov_base = (char *)&xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_parts[4].iov_base = xcb_lendelta;
    xcb_parts[4].iov_len = sizeof(xcb_lendelta);
    xcb_parts[5].iov_base = (char *)str;
    xcb_parts[5].iov_len = len * sizeof(char);

    xcb_parts[6].iov_base = 0;
    xcb_parts[6].iov_len = -(xcb_parts[4].iov_len + xcb_parts[5].iov_len) & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

// ------------------------------------------------------------------------ //
// Font
// ------------------------------------------------------------------------ //

///
///	@defgroup font The font module.
///
///	This module handles loading fonts and drawing text.
///
/// @{

FontTable Fonts = {			///< contains all our used fonts
    .Titlebar = {.ModuleName = "titlebar"}
    ,
    .Menu = {.ModuleName = "menu"}
    ,
    .Task = {.ModuleName = "task"}
    ,
    .Tooltip = {.ModuleName = "tooltip"}
    ,
    .Clock = {.ModuleName = "clock"}
    ,
    .Panel = {.ModuleName = "panel"}
    ,
    .PanelButton = {.ModuleName = "button"}
    ,
    .Pager = {.ModuleName = "pager"}
    ,
    .Fallback = {.ModuleName = "fallback"}
};
static xcb_gcontext_t FontGC;		///< font graphic context

/**
**	Send query for text extents of string.
**
**	Part 1 sends the request,
*/
xcb_query_text_extents_cookie_t FontQueryExtentsRequest(const Font * font,
    int len, const char *str)
{
    xcb_query_text_extents_cookie_t cookie;
    xcb_char2b_t *chars;
    int i;

    chars = alloca(len * 2);
    for (i = 0; i < len; ++i) {		// convert 8 -> 16
	chars[i].byte1 = 0;
	chars[i].byte2 = str[i];
    }

    cookie =
	xcb_query_text_extents_unchecked(Connection, font->Font, len, chars);
    return cookie;
}

/**
**	Get font width of string.
**
**	Part2 fetch the reply from FontQueryExtentsRequest().
*/
int FontGetTextWidth(xcb_query_text_extents_cookie_t cookie, unsigned *width)
{
    xcb_query_text_extents_reply_t *query_text_extents;

    query_text_extents =
	xcb_query_text_extents_reply(Connection, cookie, NULL);
    if (!query_text_extents) {
	Error("can't query text extents\n");
	return 1;
    }

    *width = query_text_extents->overall_width;
    free(query_text_extents);

    return 0;
}

#if 0

/**
**	Get font box of string.
**
**	Part1 fetch the reply,
*/
int FontGetStringBox1(xcb_query_text_extents_cookie_t cookie, unsigned *width,
    unsigned *height)
{
    xcb_query_text_extents_reply_t *query_text_extents;

    query_text_extents =
	xcb_query_text_extents_reply(Connection, cookie, NULL);
    if (!query_text_extents) {
	Error("can't query text extents\n");
	return 1;
    }

    if (width) {
	*width = query_text_extents->overall_width;
    }
    if (height) {
	*height =
	    query_text_extents->overall_ascent +
	    query_text_extents->overall_descent;
    }
    free(query_text_extents);
    return 0;
}
#endif

/**
**	Display a string.
**
**	@todo FIXME: clipping on region
*/
void FontDrawString(xcb_drawable_t drawable, Font * font, uint32_t pixel,
    int x, int y, unsigned width, xcb_rectangle_t * region, const char *str)
{
    size_t len;
    uint32_t mask;
    uint32_t values[2];
    xcb_rectangle_t rectangle;

    if (region) {
	Debug(0, "FIXME: %s didn't support region clipping yet\n",
	    __FUNCTION__);
    }
    if (!str) {
	return;
    }
    if (!(len = strlen(str))) {
	return;
    }

    if (region) {			// i hope don't need full region support
	int x1;
	int x2;
	int y1;
	int y2;

	x1 = MAX(x, region->x);
	y1 = MAX(y, region->y);
	x2 = MIN(x + width, region->x + (unsigned)region->width);
	y2 = MIN(y + UINT16_MAX, region->y + region->height);

	if (x1 >= x2 || y1 >= y2) {
	    return;			// nothing todo
	}
	rectangle.x = x1;
	rectangle.y = y1;
	rectangle.width = x2 - x1;
	rectangle.height = y2 - y1;
    } else {
	rectangle.x = x;
	rectangle.y = y;
	rectangle.width = width;
	rectangle.height = UINT16_MAX;	// clip on width
    }

    mask = XCB_GC_FOREGROUND | XCB_GC_FONT;
    values[0] = pixel;
    values[1] = font->Font;
    xcb_change_gc(Connection, FontGC, mask, values);

    xcb_set_clip_rectangles(Connection, XCB_CLIP_ORDERING_UNSORTED, FontGC, 0,
	0, 1, &rectangle);
    xcb_poly_text_8_simple(Connection, drawable, FontGC, x, y + font->Ascent,
	len, str);
}

/**
**	Initialize a font stage 0.
**
**	Send request to X11, don't wait for the result.
*/
static void FontInit0(Font * font)
{
    if (font->FontName) {
	font->Font = xcb_generate_id(Connection);
	font->Cookie =
	    xcb_open_font_checked(Connection, font->Font,
	    strlen(font->FontName), font->FontName);
    }
}

/**
**	Check a font initialize a font stage 1.
**
**	Check the result of the X11 requests.
*/
static void FontCheck1(Font * font)
{
    xcb_generic_error_t *error;

    if (font->FontName) {
	if (!(error = xcb_request_check(Connection, font->Cookie))) {
	    font->QCookie = xcb_query_font_unchecked(Connection, font->Font);
	    return;
	}
	Warning("could not load font '%s': %s\n", font->FontName,
	    xcb_event_get_error_label(error->error_code));
	free(error);
	// fallback fails, its fatal
	if (font == &Fonts.Fallback) {
	    FatalError("could not load the default font: '%s'\n",
		Fonts.Fallback.FontName);
	}
    }
    // use fallback font, if not configured or error
    font->Font = Fonts.Fallback.Font;
}

/**
**	Check a font initialize a font stage 2.
**
**	Check the result of the X11 requests.
*/
static void FontCheck2(Font * font)
{
    xcb_query_font_reply_t *reply;

    // only good fonts, send request
    if (font->Font != Fonts.Fallback.Font || font == &Fonts.Fallback) {
	reply = xcb_query_font_reply(Connection, font->QCookie, NULL);
	if (reply) {
	    font->Ascent = reply->font_ascent;
	    font->Height = reply->font_ascent + reply->font_descent;
	    Debug(3, "font %d + %d\n", reply->font_ascent,
		reply->font_descent);
	    free(reply);
	    return;
	}
	Warning("could not query font '%s'\n", font->FontName);
    }
    // use fallback font, if not configured or error
    font->Font = Fonts.Fallback.Font;
    font->Ascent = Fonts.Fallback.Ascent;
    font->Height = Fonts.Fallback.Height;
}

/**
**	Initialize font support.
*/
void FontInit(void)
{
    uint32_t mask;
    uint32_t values[2];

    Fonts.Fallback.FontName = strdup(DEFAULT_FONT);
    if (Fonts.Panel.FontName) {
	// as default use panel font, for panel plugins
	if (!Fonts.PanelButton.FontName) {
	    Fonts.PanelButton.FontName = strdup(Fonts.Panel.FontName);
	}
	if (!Fonts.Pager.FontName) {
	    Fonts.Pager.FontName = strdup(Fonts.Panel.FontName);
	}
	if (!Fonts.Task.FontName) {
	    Fonts.Task.FontName = strdup(Fonts.Panel.FontName);
	}
	if (!Fonts.Clock.FontName) {
	    Fonts.Clock.FontName = strdup(Fonts.Panel.FontName);
	}
    }
    // dispatch requests
    FontInit0(&Fonts.Fallback);
    FontInit0(&Fonts.Titlebar);
    FontInit0(&Fonts.Menu);
    FontInit0(&Fonts.Task);
    FontInit0(&Fonts.Tooltip);
    FontInit0(&Fonts.Clock);
    FontInit0(&Fonts.Panel);
    FontInit0(&Fonts.PanelButton);
    FontInit0(&Fonts.Pager);

    // create graphics context
    FontGC = xcb_generate_id(Connection);
    mask = XCB_GC_GRAPHICS_EXPOSURES;
    values[0] = 0;
    xcb_create_gc(Connection, FontGC, RootWindow, mask, values);

    FontCheck1(&Fonts.Fallback);
    FontCheck1(&Fonts.Titlebar);
    FontCheck1(&Fonts.Menu);
    FontCheck1(&Fonts.Task);
    FontCheck1(&Fonts.Tooltip);
    FontCheck1(&Fonts.Clock);
    FontCheck1(&Fonts.Panel);
    FontCheck1(&Fonts.PanelButton);
    FontCheck1(&Fonts.Pager);

    FontCheck2(&Fonts.Fallback);
    FontCheck2(&Fonts.Titlebar);
    FontCheck2(&Fonts.Menu);
    FontCheck2(&Fonts.Task);
    FontCheck2(&Fonts.Tooltip);
    FontCheck2(&Fonts.Clock);
    FontCheck2(&Fonts.Panel);
    FontCheck2(&Fonts.PanelButton);
    FontCheck2(&Fonts.Pager);
}

/**
**	Cleanup font support.
**
**	@warning Bug alert: FontTable.Titlebar must be first and
**	FontTable.Fallback the last element in #FontTable.
*/
void FontExit(void)
{
    Font *font;

    for (font = &Fonts.Titlebar; font <= &Fonts.Fallback; ++font) {
	xcb_close_font(Connection, font->Font);
	if (font->FontName) {
	    free(font->FontName);
	    font->FontName = NULL;
	}
	font->Font = XCB_NONE;
    }
    xcb_free_gc(Connection, FontGC);
    FontGC = XCB_NONE;
}

// ------------------------------------------------------------------------ //
// Config

/**
**	Set the font to use for a component.
*/
void FontSet(const char *module, const char *value)
{
    Font *font;

    if (!module) {
	Warning("empty module tag\n");
	return;
    }
    if (!value) {
	Warning("empty font tag\n");
	return;
    }

    for (font = &Fonts.Titlebar; font <= &Fonts.Fallback; ++font) {
	if (!strcasecmp(module, font->ModuleName)) {
	    if (font->FontName) {
		free(font->FontName);
	    }

	    font->FontName = strdup(value);
	    return;
	}
    }
    Warning("font module '%s' not found\n", module);
}

/// @}

/// @}
