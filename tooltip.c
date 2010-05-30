///
///	@file tooltip.c		@brief tooltip functions.
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

///
///	@ingroup draw
///	@defgroup tooltip The tooltip module.
///
///	This module contains the tooltip functions.
///
///	Uses:
///		#TOOLTIP_DEFAULT_DELAY from config section
///		#TOOLTIP_MAXIMAL_MOVE from config section
///
/// @{

#include <xcb/xcb.h>
#include "uwm.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <xcb/xcb_aux.h>

#include "array.h"
#include "config.h"

#include "draw.h"
#include "screen.h"

extern Config *UwmConfig;		///< µwm config

//////////////////////////////////////////////////////////////////////////////

/**
**	Tooltip structure.
*/
typedef struct _tooltip_
{
    unsigned Active:1;			///< flag tooltip is shown
    //unsigned Timeout:1;			///< flag tooltip shown timeout
    int16_t X;				///< x-coordinate of tooltip window
    int16_t Y;				///< y-coordinate of tooltip window
    uint16_t Width;			///< tooltip window width
    uint16_t Height;			///< tooltip window height
    int16_t MouseX;			///< mouse x-coordinate as created
    int16_t MouseY;			///< mouse x-coordinate as created
    char *Text;				///< tooltip text
    xcb_window_t Window;		///< tooltip X11 window

/* FIXME: more general tooltips handling
    uint16_t LastX;			///< last mouse x-coordinate
    uint16_t LastY;			///< last mouse y-coordinate
    uint32_t LastTick;			///< last tooltip tick
    char *NewText;			///< new tooltip text
*/
} Tooltip;

static Tooltip TooltipVars[1];		///< current tooltip data

    /// tooltip delay in milliseconds
int TooltipDelay = TOOLTIP_DEFAULT_DELAY;
static int TooltipEnabled = 1;		///< flag tooltips are enabled

//////////////////////////////////////////////////////////////////////////////

/**
**	Draw the tooltip window.
*/
static void TooltipDraw(void)
{
    xcb_aux_clear_window(Connection, TooltipVars->Window);
    FontDrawString(TooltipVars->Window, &Fonts.Tooltip, Colors.TooltipFG.Pixel,
	4, 1, TooltipVars->Width, NULL, TooltipVars->Text);
}

/**
**	Show tooltip window.
**
**	@param x	x-coordinate of the tooltip window
**	@param y	y-coordinate of the tooltip window
**	@param text	text to display in the tooltip
**
**	@todo check if same tooltip is redawn.
**		support multiline tooltips.
*/
void TooltipShow(int x, int y, const char *text)
{
    xcb_query_text_extents_cookie_t cookie;
    int len;
    const Screen *screen;
    unsigned width;
    uint32_t values[5];

    if (!TooltipEnabled) {
	return;
    }
    free(TooltipVars->Text);
    TooltipVars->Text = NULL;

    if (!text || !(len = strlen(text))) {	// none or empty string
	return;
    }
    cookie = FontQueryExtentsRequest(&Fonts.Tooltip, len, text);

    screen = ScreenGetByXY(x, y);

    TooltipVars->Text = strdup(text);
    TooltipVars->Height = Fonts.Tooltip.Height;
    width = FontTextWidthReply(cookie);
    TooltipVars->Width = width;

    TooltipVars->Height += 2;
    TooltipVars->Width += 9;

    if (TooltipVars->Width > screen->Width) {
	TooltipVars->Width = screen->Width;
	// FIXME: make multiline tooltip
    }
    TooltipVars->X = x;
    TooltipVars->Y = y - TooltipVars->Height - 2;

    // make sure tooltip is shown as much as possible
    if (TooltipVars->X + TooltipVars->Width >= screen->Width) {
	TooltipVars->X = screen->Width - TooltipVars->Width - 2;
    }
    if (TooltipVars->Y < 0) {
	TooltipVars->Y = y + 2;
    }
    if (TooltipVars->Y + TooltipVars->Height >= screen->Height) {
	TooltipVars->Y = screen->Height - TooltipVars->Height - 2;
    }

    if (TooltipVars->Window) {		// move and resize the window
	values[0] = TooltipVars->X;
	values[1] = TooltipVars->Y;
	values[2] = TooltipVars->Width;
	values[3] = TooltipVars->Height;
	xcb_configure_window(Connection, TooltipVars->Window,
	    XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH
	    | XCB_CONFIG_WINDOW_HEIGHT, values);
    } else {				// create new window
	TooltipVars->Window = xcb_generate_id(Connection);
	values[0] = Colors.TooltipBG.Pixel;
	values[1] = Colors.TooltipOutline.Pixel;
	values[2] = 1;
	values[3] =
	    XCB_EVENT_MASK_POINTER_MOTION | XCB_EVENT_MASK_POINTER_MOTION_HINT
	    | XCB_EVENT_MASK_EXPOSURE;
	values[4] =
	    XCB_EVENT_MASK_POINTER_MOTION | XCB_EVENT_MASK_BUTTON_PRESS |
	    XCB_EVENT_MASK_BUTTON_RELEASE;
	xcb_create_window(Connection,	// connection
	    XCB_COPY_FROM_PARENT,	// depth (same as root)
	    TooltipVars->Window,	// window Id
	    RootWindow,			// parent window
	    TooltipVars->X, TooltipVars->Y,	// x, y
	    TooltipVars->Width, TooltipVars->Height,	// width, height
	    1,				// border_width
	    XCB_WINDOW_CLASS_INPUT_OUTPUT,	// class
	    XCB_COPY_FROM_PARENT,	// visual (same as root)
	    XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL | XCB_CW_SAVE_UNDER | XCB_CW_EVENT_MASK | XCB_CW_DONT_PROPAGATE, values);	// mask, values

#ifdef DEBUG
	if (0) {
	    xcb_get_window_attributes_reply_t *reply;

	    reply =
		xcb_get_window_attributes_reply(Connection,
		xcb_get_window_attributes_unchecked(Connection,
		    TooltipVars->Window), NULL);
	    Debug(3, "tooltip: save under %d\n", reply->save_under);
	    free(reply);
	}
#endif
    }
    TooltipVars->MouseX = x;
    TooltipVars->MouseY = y;

    if (TooltipVars->Active) {
	TooltipDraw();
    } else {
	// raise window
	values[0] = XCB_STACK_MODE_ABOVE;
	xcb_configure_window(Connection, TooltipVars->Window,
	    XCB_CONFIG_WINDOW_STACK_MODE, values);
	// map window on the screen
	xcb_map_window(Connection, TooltipVars->Window);
	TooltipVars->Active = 1;
	// FIXME: tooltip isn't shown, only empty window!
	Debug(3, "tooltip mapped, should get expose next\n");
    }
}

/**
**	Hide tooltip.
*/
void TooltipHide(void)
{
    if (TooltipVars->Active) {
	xcb_unmap_window(Connection, TooltipVars->Window);
	TooltipVars->Active = 0;
    }
}

/**
**	Handle an expose event on tooltip window.
**
**	@param event	X11 expose event
**
**	@returns true if the event was for the tooltip module, 0 otherwise.
**
**	@note multiple exposes are already handled by main event handler.
*/
int TooltipHandleExpose(const xcb_expose_event_t * event)
{
    Debug(3, "tooltip %d vs %d\n", event->window, TooltipVars->Window);
    if (TooltipVars->Active && event->window == TooltipVars->Window) {
	TooltipDraw();
	return 1;
    }
    return 0;
}

/**
**	Timeout tooltip (this is used to hide tooltips after movement).
**
**	@param tick	current tick in ms
**	@param x	current mouse x-coordinate
**	@param y	current mouse y-coordinate
*/
void TooltipTimeout(uint32_t __attribute__ ((unused)) tick, int x, int y)
{
    if (TooltipVars->Active
	&& (abs(TooltipVars->MouseX - x) > TOOLTIP_MAXIMAL_MOVE
	    || abs(TooltipVars->MouseY - y) > TOOLTIP_MAXIMAL_MOVE)) {
	Debug(3, "tooltip moved unmap\n");
	TooltipHide();
    }
}

#if 0

/**
**	Handle an motion notify event on tooltip window.
**
**	@param event	X11 motion notify event
**
**	@returns true if the event was for the tooltip module, 0 otherwise.
*/
int TooltipHandleMotionNotify(const xcb_motion_notify_event_t * event)
{
    if (TooltipVars->Active && event->window == TooltipVars->Window) {
	xcb_unmap_window(Connection, TooltipVars->Window);
	TooltipVars->Active = 0;
	return 1;
    }
    return 0;
}
#endif

/**
**	Cleanup tooltips.
*/
void TooltipExit(void)
{
    free(TooltipVars->Text);
    TooltipVars->Text = NULL;

    if (TooltipVars->Window) {
	xcb_destroy_window(Connection, TooltipVars->Window);
	TooltipVars->Window = XCB_NONE;
    }
}

// ------------------------------------------------------------------------ //
// Config

#ifdef USE_LUA

/**
**	Set whether or not tooltips are enabled.
**
**	@param enable	true to enable tooltips, false to disable tooltips
*/
void TooltipSetEnabled(int enable)
{
    TooltipEnabled = enable;
}

/**
**	Set the delay before showing tooltips.
**
**	@param delay	delay in milliseconds
*/
void TooltipSetDelay(int delay)
{
    if (delay < 0) {
	Warning("invalid tooltip delay specified: %d\n", delay);
    } else {
	TooltipDelay = delay;
    }
}

#else

/**
**	Get tooltip configuration.
*/
void TooltipConfig(void)
{
    ssize_t ival;

    if (ConfigGetInteger(ConfigDict(UwmConfig), &ival, "tooltip", "delay",
	    NULL)) {
	if (ival < 0) {
	    Warning("invalid tooltip delay specified: %zd\n", ival);
	} else {
	    TooltipDelay = ival;
	}
    }
    if (ConfigGetInteger(ConfigDict(UwmConfig), &ival, "tooltip", "enabled",
	    NULL)) {
	TooltipEnabled = ival != 0;
    }
}

#endif

/// @}
