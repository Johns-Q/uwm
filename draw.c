///
///	@file draw.c	@brief drawing functions
///
///	Copyright (c) 2009, 2010 by Lutz Sammer.  All Rights Reserved.
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
#include <xcb/xcb_aux.h>

#include "uwm.h"
#include "core-array/core-array.h"
#include "core-rc/core-rc.h"
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

#if 0

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
**	@param c	connection to x11 server
**	@param drawable	X11 drawable pixmap/window
**	@param gc	graphical context to draw the text
**	@param x	starting x-coordinate
**	@param y	starting y-coordinate
**
**	@todo not correct!
**
**	@param len	text length
**	@param str	string to draw
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
#endif

/**
**	Draw poly 16bit text.
**
**	This a simple version of xcb_poly_text_16, which works only with
**	one string with less equal 254 characters.
**
**	Request:
**
**	- drawable: DRAWABLE
**	- gc: GCONTEXT
**	- x, y: INT16
**	- items: (LISTofTEXTITEM16)
**
**	- TEXTELT16: TEXTELT16: [delta: INT8 string: STRING16]
**
**	- FONT: 5 bytes 255 + FontID
**
**	- Len8 Delta8 String16 ...
**
**	@param c	connection to x11 server
**	@param drawable	X11 drawable pixmap/window
**	@param gc	graphical context to draw the text
**	@param x	starting x-coordinate
**	@param y	starting y-coordinate
**
**	@todo not correct!
**
**	@param len	text length
**	@param str	string to draw
*/
xcb_void_cookie_t xcb_poly_text_16_simple(xcb_connection_t * c,
    xcb_drawable_t drawable, xcb_gcontext_t gc, int16_t x, int16_t y,
    uint32_t len, const xcb_char2b_t * str)
{
    static const xcb_protocol_request_t xcb_req = {
	/* count */ 5,
	/* ext */ 0,
	/* opcode */ XCB_POLY_TEXT_16,
	/* isvoid */ 1
    };
    struct iovec xcb_parts[7];
    uint8_t xcb_lendelta[2];
    xcb_void_cookie_t xcb_ret;
    xcb_poly_text_16_request_t xcb_out;

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
    xcb_parts[5].iov_len = len * sizeof(xcb_char2b_t);

    xcb_parts[6].iov_base = 0;
    xcb_parts[6].iov_len = -(xcb_parts[4].iov_len + xcb_parts[5].iov_len) & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

// ------------------------------------------------------------------------ //
// Color
// ------------------------------------------------------------------------ //

///
///	@defgroup color The color module.
///
///	This module handles loading colors.
///
///	Depends on xcb.
///
/// @{

#define COLOR_DELTA 45			///< for lighten / darken

/**
**	Color table with all used colors.
**
**	@see /usr/share/X11/rgb.txt for x11 color-names
*/
ColorTable Colors = {
    .TitleFG = {"title-fg", NULL, 0UL, "gray55"}
    ,
    .TitleBG1 = {"title-bg1", NULL, 0UL, "gray22"}
    ,
    .TitleBG2 = {"title-bg2", NULL, 0UL, "black"}
    ,

    .TitleActiveFG = {"title-active-fg", NULL, 0UL, "white"}
    ,
    .TitleActiveBG1 = {"title-active-bg1", NULL, 0UL, "gray22"}
    ,
    .TitleActiveBG2 = {"title-active-bg1", NULL, 0UL, "gray55"}
    ,

    .BorderLine = {"border-line", NULL, 0UL, "black"}
    ,
    .BorderActiveLine = {"border-active-line", NULL, 0UL, "gray22"}
    ,
    .BorderCorner = {"border-corner", NULL, 0UL, "gray33"}
    ,
    .BorderActiveCorner = {"border-active-corner", NULL, 0UL, "white"}
    ,

    .PanelFG = {"panel-fg", NULL, 0UL, "white"}
    ,
    .PanelBG = {"panel-bg", NULL, 0UL, "gray33"}
    ,

    .TaskFG = {"task-fg", NULL, 0UL, "gray55"}
    ,
    .TaskBG1 = {"task-bg1", NULL, 0UL, "gray22"}
    ,
    .TaskBG2 = {"task-bg2", NULL, 0UL, "black"}
    ,
    .TaskActiveFG = {"task-active-fg", NULL, 0UL, "white"}
    ,
    .TaskActiveBG1 = {"task-active-bg1", NULL, 0UL, "gray22"}
    ,
    .TaskActiveBG2 = {"task-active-bg2", NULL, 0UL, "gray55"}
    ,

    .PagerFG = {"pager-fg", NULL, 0UL, "gray33"}
    ,
    .PagerBG = {"pager-bg", NULL, 0UL, "gray22"}
    ,
    .PagerActiveFG = {"pager-active-fg", NULL, 0UL, "gray55"}
    ,
    .PagerActiveBG = {"pager-active-bg", NULL, 0UL, "gray66"}
    ,
    .PagerOutline = {"pager-outline", NULL, 0UL, "darkred"}
    ,
    .PagerText = {"pager-text", NULL, 0UL, "white"}
    ,

    .MenuFG = {"menu-fg", NULL, 0UL, "gray11"}
    ,
    .MenuBG = {"menu-bg", NULL, 0UL, "gray44"}
    ,
    .MenuActiveFG = {"menu-active-fg", NULL, 0UL, "white"}
    ,
    .MenuActiveBG1 = {"menu-active-bg1", NULL, 0UL, "gray33"}
    ,
    .MenuActiveBG2 = {"menu-active-bg2", NULL, 0UL, "gray66"}
    ,
    .MenuActiveOutline = {"menu-active-outline", NULL, 0UL, "black"}
    ,

    .TooltipFG = {"tooltip-fg", NULL, 0UL, "black"}
    ,
    .TooltipBG = {"tooltip-bg", NULL, 0UL, "gray55"}
    ,
    .TooltipOutline = {"tooltip-outline", NULL, 0UL, "darkred"}
    ,

    .PanelButtonFG = {"button-fg", NULL, 0UL, "gray55"}
    ,
    .PanelButtonBG = {"button-bg", NULL, 0UL, "gray22"}
    ,

    .ClockFG = {"clock-fg", NULL, 0UL, "gray55"}
    ,
    .ClockBG = {"clock-bg", NULL, 0UL, "gray22"}
    ,
};

static int RedShift;			///< red shift for x11 pixel
static int GreenShift;			///< green shift for x11 pixel
static int BlueShift;			///< blue shift for x11 pixel
static uint32_t RedMask;		///< red mask for x11 pixel
static uint32_t GreenMask;		///< green mask for x11 pixel
static uint32_t BlueMask;		///< blue mask for x11 pixel

    /// map a linear 8-bit RGB space to pixel values
static uint32_t *ColorRgb8Map;

    /// map 8-bit pixel values to a 24-bit linear RGB space
static uint32_t *ColorReverseMap;

/**
**	Compute the pixel value from RGB components.
**
**	xcb_coloritem_t.red + ... -> xcb_coloritem_t.pixel
**
**	@param c	X11 color item (rgb + pixel)
*/
static void ColorGetDirectPixel(xcb_coloritem_t * c)
{
    uint32_t red;
    uint32_t green;
    uint32_t blue;

    // normalise
    red = c->red << 16;
    green = c->green << 16;
    blue = c->blue << 16;

    // shift to the correct offsets and mask
    red = (red >> RedShift) & RedMask;
    green = (green >> GreenShift) & GreenMask;
    blue = (blue >> BlueShift) & BlueMask;

    // combine
    c->pixel = red | green | blue;
}

/**
**	Compute the pixel value from RGB components.
**
**	ColorRgb8Map -> xcb_coloritem_t.pixel
**
**	@param c	X11 color item (rgb + pixel)
*/
static void ColorGetMappedPixel(xcb_coloritem_t * c)
{
    ColorGetDirectPixel(c);
    c->pixel = ColorRgb8Map[c->pixel];
}

/**
**	Compute the pixel value from RGB components.
**
**	xcb_coloritem_t.red ... -> xcb_coloritem_t.pixel
**
**	@param c	X11 color item (rgb + pixel)
*/
void ColorGetPixel(xcb_coloritem_t * c)
{
    switch (RootVisualType->_class) {
	case XCB_VISUAL_CLASS_DIRECT_COLOR:
	case XCB_VISUAL_CLASS_TRUE_COLOR:
	    ColorGetDirectPixel(c);
	    return;
	default:
	    ColorGetMappedPixel(c);
	    return;
    }
}

/**
**	Look up a color by name.
**
**	@param color_name	color name
**	@param c		X11 color item (rgb + pixel)
**
**	@todo split request and reply
*/
static int ColorGetByName(const char *color_name, xcb_coloritem_t * c)
{
    xcb_lookup_color_reply_t *reply;
    xcb_lookup_color_cookie_t cookie;

    cookie =
	xcb_lookup_color_unchecked(Connection, XcbScreen->default_colormap,
	strlen(color_name), color_name);
    reply = xcb_lookup_color_reply(Connection, cookie, NULL);
    if (reply) {
	c->red = reply->exact_red;
	c->green = reply->exact_green;
	c->blue = reply->exact_blue;
	c->flags =
	    XCB_COLOR_FLAG_RED | XCB_COLOR_FLAG_GREEN | XCB_COLOR_FLAG_BLUE;

	free(reply);
	return 1;
    }
    return 0;
}

/**
**	Parse a color for a component.
**
**	@param value	color name or \#hex triple
**	@param[out] c	xcb color item
**
**	@todo ColorGetByName is used syncron.
*/
int ColorParse(const char *value, xcb_coloritem_t * c)
{
    if (!value) {
	return 0;
    }
    if (xcb_aux_parse_color((char *)value, &c->red, &c->green, &c->blue)) {
	c->flags =
	    XCB_COLOR_FLAG_RED | XCB_COLOR_FLAG_GREEN | XCB_COLOR_FLAG_BLUE;
    } else {
	if (!ColorGetByName(value, c)) {
	    Warning("bad color: \"%s\"\n", value);
	    return 0;
	}
    }
    ColorGetPixel(c);
    return 1;
}

/**
**	Compute the RGB components from an index into our RGB colormap.
**
**	@param c	X11 color item (rgb + pixel)
*/
static void ColorGetFromIndex(xcb_coloritem_t * c)
{
    uint32_t red;
    uint32_t green;
    uint32_t blue;

    red = (c->pixel & RedMask) << RedShift;
    green = (c->pixel & GreenMask) << GreenShift;
    blue = (c->pixel & BlueMask) << BlueShift;
    c->red = red >> 16;
    c->green = green >> 16;
    c->blue = blue >> 16;
}

/**
**	Get the RGB components from a pixel value.
**
**	@param c	X11 color item (rgb + pixel)
*/
void ColorGetFromPixel(xcb_coloritem_t * c)
{
    switch (RootVisualType->_class) {
	case XCB_VISUAL_CLASS_DIRECT_COLOR:
	case XCB_VISUAL_CLASS_TRUE_COLOR:
	    // nothing to do
	    break;
	default:
	    // convert from a pixel value to a linear RGB space.
	    c->pixel = ColorReverseMap[c->pixel & 255];
	    break;
    }

    // extract the RGB components from the linear RGB pixel value
    ColorGetFromIndex(c);
}

/**
**	Get a RGB pixel value from RGB components.
**
**	xcb_coloritem_t.red + ... -> xcb_coloritem_t.pixel
**
**	@param c	X11 color item (rgb + pixel)
*/
void ColorGetIndex(xcb_coloritem_t * c)
{
    ColorGetDirectPixel(c);
}

/**
**	Compute a color lighter than the input.
**
**	@param orig		orignal base color
**	@param[out] dest	lighter color result
*/
static void ColorLighten(Color * orig, Color * dest)
{
    xcb_coloritem_t temp;
    unsigned u;

    if (orig->Value) {
	ColorParse(orig->Value, &temp);
    } else if (orig->Default) {
	ColorParse(orig->Default, &temp);
    }
    // temp contains now r/g/b

    // * 1.45
    u = temp.red * (100 + COLOR_DELTA);
    temp.red = u > 6553500 ? 65535 : (u / 100);
    u = temp.green * (100 + COLOR_DELTA);
    temp.green = u > 6553500 ? 65535 : (u / 100);
    u = temp.blue * (100 + COLOR_DELTA);
    temp.blue = u > 6553500 ? 65535 : (u / 100);

    ColorGetPixel(&temp);
    dest->Pixel = temp.pixel;
}

/**
**	Compute a color darker than the input.
**
**	@param orig		orignal base color
**	@param[out] dest	darker color result
*/
static void ColorDarken(Color * orig, Color * dest)
{
    xcb_coloritem_t temp;
    unsigned u;

    if (orig->Value) {
	ColorParse(orig->Value, &temp);
    } else if (orig->Default) {
	ColorParse(orig->Default, &temp);
    }
    // temp contains now r/g/b

    // * 0.55
    u = temp.red * (100 - COLOR_DELTA);
    temp.red = u / 100;
    u = temp.green * (100 - COLOR_DELTA);
    temp.green = u / 100;
    u = temp.blue * (100 - COLOR_DELTA);
    temp.blue = u / 100;

    ColorGetPixel(&temp);
    dest->Pixel = temp.pixel;
}

/**
**	Compute the mask for computing colors in a linear RGB colormap.
*/
static void ColorShiftMask(uint32_t mask, int *shift)
{
    int i;

    //	Components are stored in 16 bits.
    //
    //	When computing pixels, we'll first shift left 16 bits
    //	so to the shift will be an offset from that 32 bit entity.
    //	shift = 16 - <shift-to-ones> + <shift-to-zeros>
    i = 0;
    while (mask && !(mask & (1 << 31))) {
	++i;
	mask <<= 1;
    }
    *shift = i;
}

/**
**	Intialize color module.
**
**	@warning BugAlert: ColorTable.TitleFG must be the first color and
**	ColorTable.MenuActiveDown the last color in #ColorTable.
**
**	@todo use xcb unsync!
*/
void ColorInit(void)
{
    int i;
    int red;
    int green;
    int blue;
    xcb_alloc_color_cookie_t cookies[256];
    xcb_coloritem_t c;
    Color *color;

    // determine how to convert between RGB triples and pixels
    switch (RootVisualType->_class) {
	case XCB_VISUAL_CLASS_DIRECT_COLOR:
	case XCB_VISUAL_CLASS_TRUE_COLOR:
	    ColorShiftMask(RedMask = RootVisualType->red_mask, &RedShift);
	    ColorShiftMask(GreenMask =
		RootVisualType->green_mask, &GreenShift);
	    ColorShiftMask(BlueMask = RootVisualType->blue_mask, &BlueShift);
	    break;
	default:
	    // attempt to get 256 colors, pretend it worked
	    ColorShiftMask(RedMask = 0xE0, &RedShift);
	    ColorShiftMask(GreenMask = 0x1C, &GreenShift);
	    ColorShiftMask(BlueMask = 0x03, &BlueShift);
	    ColorRgb8Map = malloc(sizeof(*ColorRgb8Map) * 256);

	    // RGB: 3, 3, 2
	    i = 0;
	    for (red = 0; red < 8; red++) {
		for (green = 0; green < 8; green++) {
		    for (blue = 0; blue < 4; blue++) {
			cookies[i++] =
			    xcb_alloc_color_unchecked(Connection,
			    XcbScreen->default_colormap,
			    (uint16_t) ((74898 * red) / 8),
			    (uint16_t) ((74898 * green) / 8),
			    (uint16_t) ((87381 * blue) / 4));
		    }
		}
	    }

	    // get alloc color replies and compute the reverse pixel mapping
	    // (pixel -> 24-bit RGB)
	    ColorReverseMap = malloc(sizeof(uint32_t) * 256);
	    for (i = 0; i < 256; i++) {
		xcb_alloc_color_reply_t *reply;

		reply = xcb_alloc_color_reply(Connection, cookies[i], NULL);
		if (reply) {
		    ColorRgb8Map[i] = reply->pixel;
		    c.red = reply->red;
		    c.green = reply->green;
		    c.blue = reply->blue;
		    c.flags =
			XCB_COLOR_FLAG_RED | XCB_COLOR_FLAG_GREEN |
			XCB_COLOR_FLAG_BLUE;

		    ColorGetDirectPixel(&c);
		    ColorReverseMap[i] = c.pixel;
		    free(reply);
		}
		// FIXME: alloc color can fail, what todo?
	    }
    }

    // allocate the colors
    for (color = &Colors.TitleFG; color <= &Colors.MenuActiveDown; ++color) {
	if (color->Value) {
	    ColorParse(color->Value, &c);
	    color->Pixel = c.pixel;
	} else if (color->Default) {
	    ColorParse(color->Default, &c);
	    color->Pixel = c.pixel;
	}
    }

    // inherit unset colors from the panel for panel plugins
    if (Colors.PanelBG.Value || Colors.PanelBG.Default) {
	if (!Colors.TaskBG1.Value && !Colors.TaskBG1.Value) {
	    Colors.TaskBG1.Pixel = Colors.PanelBG.Pixel;
	}
	if (!Colors.TaskBG2.Value && !Colors.TaskBG2.Value) {
	    Colors.TaskBG2.Pixel = Colors.PanelBG.Pixel;
	}

	if (!Colors.PanelButtonBG.Value && !Colors.PanelButtonBG.Value) {
	    Colors.PanelButtonBG.Pixel = Colors.PanelBG.Pixel;
	}
	if (!Colors.ClockBG.Value && !Colors.ClockBG.Value) {
	    Colors.ClockBG.Pixel = Colors.PanelBG.Pixel;
	}
    }
    if (Colors.PanelFG.Value || Colors.PanelFG.Default) {
	if (!Colors.TaskFG.Value && !Colors.TaskFG.Value) {
	    Colors.TaskFG.Pixel = Colors.PanelFG.Pixel;
	}
	if (!Colors.PanelButtonFG.Value && !Colors.PanelButtonFG.Value) {
	    Colors.PanelButtonFG.Pixel = Colors.PanelFG.Pixel;
	}
	if (!Colors.ClockFG.Value && !Colors.ClockFG.Value) {
	    Colors.ClockFG.Pixel = Colors.PanelFG.Pixel;
	}
    }

    ColorLighten(&Colors.PanelBG, &Colors.PanelUp);
    ColorDarken(&Colors.PanelBG, &Colors.PanelDown);
    ColorLighten(&Colors.TaskBG1, &Colors.TaskUp);
    ColorDarken(&Colors.TaskBG1, &Colors.TaskDown);
    ColorLighten(&Colors.TaskActiveBG1, &Colors.TaskActiveUp);
    ColorDarken(&Colors.TaskActiveBG1, &Colors.TaskActiveDown);
    ColorLighten(&Colors.MenuBG, &Colors.MenuUp);
    ColorDarken(&Colors.MenuBG, &Colors.MenuDown);
    ColorLighten(&Colors.MenuActiveBG1, &Colors.MenuActiveUp);
    ColorDarken(&Colors.MenuActiveBG1, &Colors.MenuActiveDown);

    // free the now unused color values
    for (color = &Colors.TitleFG; color <= &Colors.MenuActiveDown; ++color) {
	free(color->Value);
	color->Value = NULL;
    }
}

/**
**	Cleanup color module.
*/
void ColorExit(void)
{
    if (ColorRgb8Map) {
	xcb_free_colors(Connection, XcbScreen->default_colormap, 0, 256,
	    ColorRgb8Map);
	free(ColorRgb8Map);
	ColorRgb8Map = NULL;
	free(ColorReverseMap);
	ColorReverseMap = NULL;
    }
}

// ------------------------------------------------------------------------ //
// Config

#ifdef USE_RC				// {

/**
**	Parse configuration for color module
**
**	@param config	global config dictionary
*/
void ColorConfig(const Config * config)
{
    Color *color;

    // FIXME: get errors here when unconfigured!
    for (color = &Colors.TitleFG; color <= &Colors.MenuActiveDown; ++color) {
	const char *value;

	if (color->Name
	    && ConfigGetString(ConfigDict(config), &value, "color",
		color->Name, NULL)) {
	    color->Value = strdup(value);
	}
    }
}

#endif // } USE_RC

/// @}

// ------------------------------------------------------------------------ //
// Font
// ------------------------------------------------------------------------ //

///
///	@defgroup font The font module.
///
///	This module handles loading fonts and drawing text.
///
///	Depends on xcb.
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
**	Convert an utf-8 string to ucs-2 string.
**
**	@param utf8		input utf-8 encoded string
**	@param len		length of utf-8 string
**	@param[out] ucs2	output ucs-2 encoded string
**
**	@returns the length of the ucs-2 string.
*/
static size_t FontUtf8ToUcs2(const uint8_t * utf8, size_t len,
    xcb_char2b_t * ucs2)
{
    size_t in_pos;
    size_t out_pos;
    unsigned ch;

    in_pos = 0;
    out_pos = 0;
    while (in_pos < len) {
	if (utf8[in_pos] <= 0x7F) {
	    ucs2[out_pos].byte1 = 0;
	    ucs2[out_pos].byte2 = utf8[in_pos];
	    in_pos++;
	} else if ((utf8[in_pos] & 0300) != 0300) {
	    // out of sync
	    ucs2[out_pos].byte1 = 0xFF;
	    ucs2[out_pos].byte2 = 0xFD;
	    in_pos++;
	} else if (in_pos < len - 1 && utf8[in_pos] <= 0xDF
	    && utf8[in_pos + 1] <= 0xBF && utf8[in_pos + 1] >= 0x80) {
	    ch = ((utf8[in_pos] & 0x1F) << 6) + (utf8[in_pos + 1] & 0x3F);
	    ucs2[out_pos].byte1 = ch >> 8;
	    ucs2[out_pos].byte2 = ch;
	    // check for overlong sequence
	    if (ch < 0x80) {
		ucs2[out_pos].byte1 = 0xFF;
		ucs2[out_pos].byte2 = 0xFD;
	    }
	    in_pos += 2;
	} else if (in_pos < len - 2 && utf8[in_pos] <= 0xEF
	    && utf8[in_pos + 1] <= 0xBF && utf8[in_pos + 1] >= 0x80
	    && utf8[in_pos + 2] <= 0xBF && utf8[in_pos + 2] >= 0x80) {
	    ch = ((utf8[in_pos] & 0x0F) << 12) + ((utf8[in_pos +
			1] & 0x3F) << 6) + (utf8[in_pos + 2] & 0x3F);
	    ucs2[out_pos].byte1 = ch >> 8;
	    ucs2[out_pos].byte2 = ch;
	    // check for overlong sequence
	    if (ch < 0x800) {
		ucs2[out_pos].byte1 = 0xFF;
		ucs2[out_pos].byte2 = 0xFD;
	    }
	    in_pos += 3;
	} else {
	    // incomplete sequence
	    ucs2[out_pos].byte1 = 0xFF;
	    ucs2[out_pos].byte2 = 0xFD;
	    in_pos++;
	}
	out_pos++;
    }
    return out_pos;
}

/**
**	Send query for text extents of utf-8 string.
**
**	Part 1 sends the request,
**
**	@param font	font for string extents
**	@param len	lenght of string
**	@param str	text of string
**
**	@returns cookie to fetch reply.
**
**	@todo wrong for utf8
*/
xcb_query_text_extents_cookie_t FontQueryExtentsRequest(const Font * font,
    size_t len, const char *str)
{
    xcb_char2b_t *chars;
    size_t i;

    chars = alloca(len * sizeof(*chars));
    i = FontUtf8ToUcs2((const uint8_t *)str, len, chars);
    return xcb_query_text_extents_unchecked(Connection, font->Font, i, chars);
}

/**
**	Font width of string.
**
**	Part2 fetch the reply from FontQueryExtentsRequest().
**
**	@param cookie	cookie of xcb_query_text_extents request
**
**	@returns length of string.
*/
int FontTextWidthReply(xcb_query_text_extents_cookie_t cookie)
{
    int width;
    xcb_query_text_extents_reply_t *reply;

    if (!(reply = xcb_query_text_extents_reply(Connection, cookie, NULL))) {
	Error("query text extents failed\n");
	return 0;
    }

    width = reply->overall_width;
    free(reply);

    return width;
}

/**
**	Display an utf-8 string.
**
**	@param drawable	x11 drawable pixmap/window
**	@param font	our font definition
**	@param pixel	color to draw in x11 pixel format
**	@param x	x-coordinate to place the text
**	@param y	y-coordinate to place the text
**	@param region	region to draw
**	@param width	don't draw text beyond width
**	@param str	text string
**
**	@todo FIXME: clipping on region
*/
void FontDrawString(xcb_drawable_t drawable, const Font * font, uint32_t pixel,
    int x, int y, unsigned width, xcb_rectangle_t * region, const char *str)
{
    size_t i;
    size_t len;
    uint32_t values[2];
    xcb_rectangle_t rectangle;
    xcb_char2b_t *chars;

    if (region) {
	Debug(2, "FIXME: %s didn't support region clipping yet\n",
	    __FUNCTION__);
    }
    if (!str || !(len = strlen(str))) {
	return;
    }

    if (0 && region) {			// FIXME: we need full region support
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

    values[0] = pixel;
    values[1] = font->Font;
    xcb_change_gc(Connection, FontGC, XCB_GC_FOREGROUND | XCB_GC_FONT, values);
    xcb_set_clip_rectangles(Connection, XCB_CLIP_ORDERING_UNSORTED, FontGC, 0,
	0, 1, &rectangle);

    // UTF-8 to UCS-2 conversion
    chars = alloca(len * sizeof(*chars));
    i = FontUtf8ToUcs2((const uint8_t *)str, len, chars);
    /*
       Some one broke uft-8 support
       if (*str & 0x80) {
       printf("UTF-8 %02x+%02x\n", chars[0].byte1, chars[0].byte2);
       } else {
       printf("ASCII %02x+%02x\n", chars[0].byte1, chars[0].byte2);
       }
     */

    xcb_poly_text_16_simple(Connection, drawable, FontGC, x, y + font->Ascent,
	i, chars);
}

/**
**	Initialize a font stage 0.
**
**	Send request to X11, don't wait for the result.
**
**	@param font	send open request for this font
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
**	Send request for font query.
**
**	@param font	reply/request for this font
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
**
**	@param font	get query reply for this font
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
    uint32_t value[1];
    Font *font;

    if (!Fonts.Fallback.FontName) {
	Fonts.Fallback.FontName = strdup(DEFAULT_FONT);
    }
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
    value[0] = 0;
    xcb_create_gc(Connection, FontGC, XcbScreen->root,
	XCB_GC_GRAPHICS_EXPOSURES, value);

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

    // font names are no longer needed
    for (font = &Fonts.Titlebar; font <= &Fonts.Fallback; ++font) {
	free(font->FontName);
	font->FontName = NULL;
    }
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
	font->Font = XCB_NONE;
    }
    xcb_free_gc(Connection, FontGC);
    FontGC = XCB_NONE;
}

// ------------------------------------------------------------------------ //
// Config

#ifdef USE_RC				// {

/**
**	Parse configuration for font module
**
**	@param config	global config dictionary
*/
void FontConfig(const Config * config)
{
    Font *font;

    for (font = &Fonts.Titlebar; font <= &Fonts.Fallback; ++font) {
	const char *value;

	if (ConfigGetString(ConfigDict(config), &value, "font",
		font->ModuleName, NULL)) {
	    font->FontName = strdup(value);
	}
    }
}

#endif // } USE_RC

/// @}

// ------------------------------------------------------------------------ //
// Gradient
// ------------------------------------------------------------------------ //

///	@defgroup gradient The gradient drawing module.
///
///	This modules contains the gradient drawing functions.
///
/// @{

/**
**	Draw a horizontal gradient.
**
**	@param drawable		pixmap or window to get gradient
**	@param gc		graphic context to draw gradient
**	@param from_pixel	x11 pixel starting color
**	@param to_pixel		x11 pixel ending color
**	@param x		start x-coordinate of gradient
**	@param y		start y-coordinate of gradient
**	@param width		width of gradient
**	@param height		height of gradient
**
**	@todo rewrite float to fix point integer
*/
void GradientDrawHorizontal(xcb_drawable_t drawable, xcb_gcontext_t gc,
    uint32_t from_pixel, uint32_t to_pixel, int x, int y, unsigned width,
    unsigned height)
{
    xcb_point_t points[2];
    unsigned line;
    xcb_coloritem_t temp;
    double red;
    double green;
    double blue;
    double from_red;
    double from_green;
    double from_blue;
    double to_red;
    double to_green;
    double to_blue;
    double multiplier;
    double from_mult;
    double to_mult;

    // return if there's nothing to do
    if (!width || !height) {
	Debug(2, "Hey dude, check the caller can check this\n");
	return;
    }
    // here we assume that background was filled elsewhere
    if (from_pixel == to_pixel) {
	Debug(2, "Hey dude, check the caller can check this\n");
	return;
    }
    // load "from" color
    temp.pixel = from_pixel;
    ColorGetFromPixel(&temp);
    from_red = (double)temp.red / 65535.0;
    from_green = (double)temp.green / 65535.0;
    from_blue = (double)temp.blue / 65535.0;

    // load "to" color
    temp.pixel = to_pixel;
    ColorGetFromPixel(&temp);
    to_red = (double)temp.red / 65535.0;
    to_green = (double)temp.green / 65535.0;
    to_blue = (double)temp.blue / 65535.0;
    multiplier = 1.0 / height;

    // loop over each line
    points[0].x = x;
    points[1].x = x + width;
    for (line = 0; line < height; line++) {

	to_mult = line * multiplier;
	from_mult = 1.0 - to_mult;
	// determine color for this line
	red = from_red * from_mult + to_red * to_mult;
	green = from_green * from_mult + to_green * to_mult;
	blue = from_blue * from_mult + to_blue * to_mult;
	temp.red = (uint16_t) (red * 65535.9);
	temp.green = (uint16_t) (green * 65535.9);
	temp.blue = (uint16_t) (blue * 65535.9);
	ColorGetPixel(&temp);

	// draw line
	xcb_change_gc(Connection, gc, XCB_GC_FOREGROUND, &temp.pixel);
	points[0].y = y + line;
	points[1].y = y + line;
	xcb_poly_line(Connection, XCB_COORD_MODE_ORIGIN, drawable, gc, 2,
	    points);
    }
}

/// @}

/// @}
