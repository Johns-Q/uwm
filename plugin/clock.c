///
///	@file clock.c	@brief clock panel plugin functions.
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
///	@ingroup panel
///	@defgroup clock_plugin	The clock panel plugin
///
///	This module add clocks to the panel. This module is only available
///	if compiled with #USE_CLOCK.
///
///	The clock is shown as single or double line text in the panel.
///	Commands (external or window manager) can be executed on pointer
///	button click.
///
///	@todo drawing two lines isn't the best code
///
/// @{

#include <xcb/xcb.h>
#include "uwm.h"

#ifdef USE_CLOCK			// {

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <xcb/xcb_icccm.h>

#include "queue.h"
#include "core-array/core-array.h"
#include "core-rc/core-rc.h"

#include "draw.h"
#include "tooltip.h"
#include "client.h"

#include "image.h"
#include "icon.h"
#include "menu.h"

#include "panel.h"
#include "plugin/clock.h"

/**
**	Clock plugin typedef.
*/
typedef struct _clock_plugin_ ClockPlugin;

/**
**	Structure with clock panel plugin private data.
*/
struct _clock_plugin_
{
    SLIST_ENTRY(_clock_plugin_) Next;	///< singly-linked clock list
    Plugin *Plugin;			///< common plugin data

    char *ShortFormat;			///< time format for panel
    char *LongFormat;			///< time format for tooltip

    MenuButton *Buttons;		///< commands to run on click

    char AsciiTime[80];			///< currently displayed time
};

    /// Clock plugin list head structure
SLIST_HEAD(_clock_head_, _clock_plugin_);

    /// list of all clocks of the plugin
static struct _clock_head_ Clocks = SLIST_HEAD_INITIALIZER(Clocks);

static uint32_t ClockLastUpdateTick;	///< tick of last clock update

// ------------------------------------------------------------------------ //
// Draw

/**
**	Draw a clock panel plugin.
**
**	@param clock_plugin	clock plugin private data
**	@param now		current time
*/
static void ClockDraw(ClockPlugin * clock_plugin)
{
    time_t now;
    char buf[80];
    char *s;
    int l;
    Plugin *plugin;
    Panel *panel;
    xcb_query_text_extents_cookie_t cookie_label1;
    xcb_query_text_extents_cookie_t cookie_label2;
    unsigned real_width;
    unsigned width1;
    unsigned width2;
    unsigned height;

    plugin = clock_plugin->Plugin;
    if (!(panel = plugin->Panel)) {
	Debug(2, "clock not inside a panel\n");
	return;
    }
    time(&now);
    l = strftime(buf, sizeof(buf), clock_plugin->ShortFormat, localtime(&now));

    // draw only, if text changed
    if (!strcmp(clock_plugin->AsciiTime, buf)) {
	return;
    }
    // handle only one or two lines
    s = strchr(buf, '\n');
    if (s) {
	s++;
	cookie_label1 =
	    FontQueryExtentsRequest(&Fonts.Clock, (s - buf) - 1, buf);
	cookie_label2 =
	    FontQueryExtentsRequest(&Fonts.Clock, l - (s - buf), s);
	height = Fonts.Clock.Height * 2 + CLOCK_INNER_SPACE;

    } else {
	cookie_label1 = FontQueryExtentsRequest(&Fonts.Clock, l, buf);
	height = Fonts.Clock.Height;
    }

    strcpy(clock_plugin->AsciiTime, buf);

    // clear the background
    PanelClearPluginBackgroundWithColor(plugin, &Colors.ClockBG.Pixel);

    width1 = FontTextWidthReply(cookie_label1);
    NO_WARNING(width2);
    real_width = width1 + 2 * CLOCK_INNER_SPACE;
    if (s) {
	width2 = FontTextWidthReply(cookie_label2);
	if (width2 > width1) {
	    real_width = width2 + 2;
	}
    }
    // is clock right size?
    // FIXME: proportional font, too much resizes?
    if (real_width == plugin->RequestedWidth || plugin->UserWidth) {
	// just draw the clock

	if (s) {
	    s[-1] = '\0';
	    FontDrawString(plugin->Pixmap, &Fonts.Clock, Colors.ClockFG.Pixel,
		plugin->Width / 2 - width1 / 2,
		plugin->Height / 2 - height / 2, plugin->Width, NULL, buf);
	    FontDrawString(plugin->Pixmap, &Fonts.Clock, Colors.ClockFG.Pixel,
		plugin->Width / 2 - width2 / 2,
		plugin->Height / 2 - height / 2 + Fonts.Clock.Height,
		plugin->Width, NULL, s);
	} else {
	    FontDrawString(plugin->Pixmap, &Fonts.Clock, Colors.ClockFG.Pixel,
		plugin->Width / 2 - width1 / 2,
		plugin->Height / 2 - height / 2, plugin->Width, NULL, buf);
	}

	PanelUpdatePlugin(panel, plugin);
    } else {				// wrong size, request resize
	plugin->RequestedWidth = real_width;
	PanelResize(panel);
    }
}

// ------------------------------------------------------------------------ //
// Callbacks

/**
**	Create/initialize a clock panel plugin.
**
**	@param plugin	common panel plugin data of clock
*/
static void ClockCreate(Plugin * plugin)
{
    ClockPlugin *clock_plugin;

    // create pixmap
    PanelPluginCreatePixmap(plugin);

    clock_plugin = plugin->Object;
    clock_plugin->AsciiTime[0] = '\0';	// force redraw

    ClockDraw(clock_plugin);
}

/**
**	Resize a clock panel plugin.
**
**	@param plugin	common panel plugin data of clock
*/
static void ClockResize(Plugin * plugin)
{
    PanelPluginDeletePixmap(plugin);
    ClockCreate(plugin);
}

/**
**	Handle a click event on a clock panel plugin.
**
**	Runs command associated with clock.
**
**	@param plugin	common panel plugin data of clock
**	@param x	x-coordinate of button press
**	@param y	y-coordinate of button press
**	@param mask	button mask
*/
static void ClockHandleButtonPress(Plugin * plugin, int
    __attribute__ ((unused)) x, int __attribute__ ((unused)) y, int
    __attribute__ ((unused)) mask)
{
    ClockPlugin *clock_plugin;

    clock_plugin = plugin->Object;
    PanelExecuteButton(plugin, clock_plugin->Buttons, mask);
}

/**
**	Show tooltip of clock panel plugin.
**
**	@param plugin	common panel plugin data of clock
**	@param x	current mouse x-coordinate
**	@param y	current mouse y-coordinate
*/
static void ClockTooltip(const Plugin * plugin, int x, int y)
{
    const ClockPlugin *clock_plugin;
    time_t now;
    char buf[128];

    clock_plugin = plugin->Object;
    time(&now);
    strftime(buf, sizeof(buf), clock_plugin->LongFormat, localtime(&now));
    TooltipShow(x, y, buf);
}

/**
**	Clock panel plugin timeout method.
**
**	@param plugin	common panel plugin data of clock
**	@param tick	current tick in ms
**	@param x	current mouse x-coordinate
**	@param y	current mouse y-coordinate
*/
static void ClockTimeout(Plugin
    __attribute__ ((unused)) * plugin, uint32_t tick, int
    __attribute__ ((unused)) x, int __attribute__ ((unused)) y)
{
    ClockPlugin *clock_plugin;

    // draw only every 1000ms
    if (ClockLastUpdateTick > tick || tick >= ClockLastUpdateTick + 1000) {
	ClockLastUpdateTick = tick;
	// all clocks are redrawn by first plugin
	SLIST_FOREACH(clock_plugin, &Clocks, Next) {
	    ClockDraw(clock_plugin);
	}
    }
}

// ------------------------------------------------------------------------ //

/**
**	Initialize clock(s).
**
**	@todo FIXME: can send request for all clocks first, than handle
**	requests.
*/
void ClockInit(void)
{
    ClockPlugin *clock_plugin;
    time_t now;

    time(&now);
    SLIST_FOREACH(clock_plugin, &Clocks, Next) {
	Plugin *plugin;
	char buf[80];
	const char *s;
	int l;
	xcb_query_text_extents_cookie_t cookie_label1;
	xcb_query_text_extents_cookie_t cookie_label2;
	unsigned width1;
	unsigned width2;
	unsigned height;

	// FIXME: combine with draw
	l = strftime(buf, sizeof(buf), clock_plugin->ShortFormat,
	    localtime(&now));
	s = strchr(buf, '\n');
	if (s) {
	    ++s;
	    cookie_label1 =
		FontQueryExtentsRequest(&Fonts.Clock, (s - buf) - 1, buf);
	    cookie_label2 =
		FontQueryExtentsRequest(&Fonts.Clock, l - (s - buf), s);
	    height = Fonts.Clock.Height * 2;

	} else {
	    cookie_label1 = FontQueryExtentsRequest(&Fonts.Clock, l, buf);
	    height = Fonts.Clock.Height;	// use max height
	}

	plugin = clock_plugin->Plugin;

	// FIXME TWO lines clock format

	width1 = FontTextWidthReply(cookie_label1);
	if (s) {
	    width2 = FontTextWidthReply(cookie_label2);
	    if (width2 > width1) {
		width1 = width2;
	    }

	}

	if (!plugin->RequestedWidth) {
	    plugin->RequestedWidth = width1 + 2 * CLOCK_INNER_SPACE;
	}
	if (!plugin->RequestedHeight) {
	    plugin->RequestedHeight = height + 2 * CLOCK_INNER_SPACE;
	}
    }
}

/**
**	Cleanup clock(s).
*/
void ClockExit(void)
{
    ClockPlugin *clock_plugin;

    while (!SLIST_EMPTY(&Clocks)) {	// list deletion
	clock_plugin = SLIST_FIRST(&Clocks);

	free(clock_plugin->ShortFormat);
	free(clock_plugin->LongFormat);

	MenuButtonDel(clock_plugin->Buttons);

	SLIST_REMOVE_HEAD(&Clocks, Next);
	free(clock_plugin);
    }
}

// ------------------------------------------------------------------------ //
// Config

#ifdef USE_RC				// {

/**
**	Create a new clock panel plugin from config data.
**
**	@param array	configuration array for clock panel plugin
**
**	@returns created clock panel plugin.
*/
Plugin *ClockConfig(const ConfigObject * array)
{
    Plugin *plugin;
    ClockPlugin *clock_plugin;
    const char *sval;

    clock_plugin = calloc(1, sizeof(*clock_plugin));
    SLIST_INSERT_HEAD(&Clocks, clock_plugin, Next);

    if (!ConfigGetString(array, &sval, "format", NULL)) {
	sval = CLOCK_DEFAULT_FORMAT;
    }
    clock_plugin->ShortFormat = strdup(sval);

    if (!ConfigGetString(array, &sval, "tooltip", NULL)) {
	sval = CLOCK_DEFAULT_LONG_FORMAT;
    }
    clock_plugin->LongFormat = strdup(sval);

    // common config of pointer buttons to commands
    MenuButtonsConfig(array, &clock_plugin->Buttons);

    plugin = PanelPluginNew();
    plugin->Object = clock_plugin;
    clock_plugin->Plugin = plugin;

    // common config of plugin size
    PanelPluginConfigSize(array, plugin);

    plugin->Create = ClockCreate;
    plugin->Delete = PanelPluginDeletePixmap;
    plugin->Resize = ClockResize;
    plugin->Tooltip = ClockTooltip;
    plugin->HandleButtonPress = ClockHandleButtonPress;
    plugin->Timeout = ClockTimeout;

    return plugin;
}

#endif // } USE_RC

#endif // } USE_CLOCK

/// @}
