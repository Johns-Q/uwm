///
///	@file clock.c	@brief clock panel plugin functions.
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

///
///	@ingroup panel
///	@defgroup clock_plugin	The clock panel plugin
///
///	This module add clocks to the panel. This module is only available
///	if compiled with #USE_CLOCK.
///
///	The clock is shown as text in the panel. A command can be executed on
///	click.
///
///	@todo Can add support for multiple commands (left, right, middle click).
///	or generic command handling, like buttons.
///
/// @{

#include <xcb/xcb.h>
#include "uwm.h"

#ifdef USE_CLOCK			// {

#include <sys/queue.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "draw.h"
#include "panel.h"

#include "array.h"
#include "config.h"

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

    // FIXME: more buttons?
    char *Command;			///< command to run when clicked
    char AsciiTime[80];			///< currently displayed time

    unsigned UserWidth:1;		///< user-specified clock width flag
};

    /// Clock plugin list head structure
SLIST_HEAD(_clock_head_, _clock_plugin_);
    //
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
static void ClockDraw(ClockPlugin * clock_plugin, const time_t * now)
{
    char buf[80];
    Plugin *plugin;
    Panel *panel;
    xcb_query_text_extents_cookie_t cookie_label;
    unsigned real_width;
    unsigned width;
    unsigned height;

    plugin = clock_plugin->Plugin;
    if (!(panel = plugin->Panel)) {
	Debug(0, "clock not inside a panel\n");
	return;
    }
    // draw only, if text changed
    strftime(buf, sizeof(buf), clock_plugin->ShortFormat, localtime(now));

    if (!strcmp(clock_plugin->AsciiTime, buf)) {
	return;
    }
    cookie_label = FontQueryExtentsRequest(&Fonts.Clock, strlen(buf), buf);

    strcpy(clock_plugin->AsciiTime, buf);

    // clear the background
    PanelClearPluginBackgroundWithColor(plugin, &Colors.ClockBG.Pixel);

    width = FontTextWidthReply(cookie_label);
    height = Fonts.Clock.Height;
    real_width = width + 2 * CLOCK_INNER_SPACE;
    // is clock right size?
    // FIXME: proportional font, too much resizes?
    if (real_width == plugin->RequestedWidth || clock_plugin->UserWidth) {
	// just draw the clock
	FontDrawString(plugin->Pixmap, &Fonts.Clock, Colors.ClockFG.Pixel,
	    plugin->Width / 2 - width / 2, plugin->Height / 2 - height / 2,
	    plugin->Width, NULL, buf);

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
    // create pixmap
    PanelPluginCreatePixmap(plugin);

    // clear the background
    PanelClearPluginBackgroundWithColor(plugin, &Colors.ClockBG.Pixel);

    // FIXME: shouldn't i draw here?
}

/**
**	Resize a clock panel plugin.
**
**	@param plugin	common panel plugin data of clock
*/
static void ClockResize(Plugin * plugin)
{
    ClockPlugin *clock_plugin;
    time_t now;

    PanelPluginDeletePixmap(plugin);
    PanelPluginCreatePixmap(plugin);

    clock_plugin = plugin->Object;
    clock_plugin->AsciiTime[0] = '\0';	// force redraw

    time(&now);
    ClockDraw(clock_plugin, &now);
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
    if (clock_plugin->Command) {
	CommandRun(clock_plugin->Command);
    }
}

/**
**	Show tooltip of clock panel plugin.
**
**	@param plugin	common panel plugin data of clock
**	@param x	current mouse x-coordinate
**	@param y	current mouse y-coordinate
*/
static void ClockTooltip(Plugin * plugin, int x, int y)
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
    time_t now;
    ClockPlugin *clock_plugin;

    // draw only every 1000ms
    if (ClockLastUpdateTick > tick || tick >= ClockLastUpdateTick + 1000) {
	ClockLastUpdateTick = tick;
	// all clocks are redrawn by first plugin
	SLIST_FOREACH(clock_plugin, &Clocks, Next) {
	    time(&now);
	    ClockDraw(clock_plugin, &now);
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
	xcb_query_text_extents_cookie_t cookie_label;
	unsigned width;
	unsigned height;

	strftime(buf, sizeof(buf), clock_plugin->ShortFormat, localtime(&now));
	cookie_label = FontQueryExtentsRequest(&Fonts.Clock, strlen(buf), buf);

	plugin = clock_plugin->Plugin;

	width = FontTextWidthReply(cookie_label);
	height = Fonts.Clock.Height;	// use max height

	if (!plugin->RequestedWidth) {
	    plugin->RequestedWidth = width + 2 * CLOCK_INNER_SPACE;
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

	if (clock_plugin->ShortFormat) {
	    free(clock_plugin->ShortFormat);
	}
	if (clock_plugin->LongFormat) {
	    free(clock_plugin->LongFormat);
	}
	if (clock_plugin->Command) {
	    free(clock_plugin->Command);
	}

	SLIST_REMOVE_HEAD(&Clocks, Next);
	free(clock_plugin);
    }
}

// ------------------------------------------------------------------------ //
// Config

#ifdef USE_LUA

/**
**	Create a new clock panel plugin.
**
**	@param short_format	format string to be displayed in clock panel
**				#CLOCK_DEFAULT_FORMAT if format is NULL
**	@param long_format	format string to be displayed in tooltip
**				#CLOCK_DEFAULT_LONG_FORMAT if format is NULL
**	@param command		command to be executed on click of clock
**	@param width		!=0 fixed width of clock panel
**	@param height		!=0 fixed height of clock panel
**
**	@returns created clock plugin.
**
**	@todo support multiple commands.
*/
Plugin *ClockNew(const char *short_format, const char *long_format,
    const char *command, unsigned width, unsigned height)
{
    Plugin *plugin;
    ClockPlugin *clock_plugin;

    clock_plugin = calloc(1, sizeof(*clock_plugin));
    SLIST_INSERT_HEAD(&Clocks, clock_plugin, Next);

    if (!short_format) {
	short_format = CLOCK_DEFAULT_FORMAT;
    }
    clock_plugin->ShortFormat = strdup(short_format);

    if (!long_format) {
	long_format = CLOCK_DEFAULT_LONG_FORMAT;
    }
    clock_plugin->LongFormat = strdup(long_format);

    if (command) {
	clock_plugin->Command = strdup(command);
    }

    plugin = PanelPluginNew();
    plugin->Object = clock_plugin;
    clock_plugin->Plugin = plugin;

    if (width > 0) {
	plugin->RequestedWidth = width;
	clock_plugin->UserWidth = 1;
    }
    plugin->RequestedHeight = height;

    plugin->Create = ClockCreate;
    plugin->Delete = PanelPluginDeletePixmap;
    plugin->Resize = ClockResize;
    plugin->Tooltip = ClockTooltip;
    plugin->HandleButtonPress = ClockHandleButtonPress;
    plugin->Timeout = ClockTimeout;

    return plugin;
}

#else

/**
**	Create a new clock panel plugin from config data.
**
**	@param array	configuration array for clock panel plugin
*/
Plugin * ClockConfig(ConfigObject* array)
{
    Plugin *plugin;
    ClockPlugin *clock_plugin;
    const char * sval;
    ssize_t ival;

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

    if (ConfigGetString(array, &sval, "command", NULL)) {
	clock_plugin->Command = strdup(sval);
    }

    plugin = PanelPluginNew();
    plugin->Object = clock_plugin;
    clock_plugin->Plugin = plugin;

    if (ConfigGetInteger(array, &ival, "width", NULL)) {
	if (ival > 0) {
	    plugin->RequestedWidth = ival;
	    clock_plugin->UserWidth = 1;
	}
    }
    if (ConfigGetInteger(array, &ival, "height", NULL)) {
	plugin->RequestedHeight = ival;
    }

    plugin->Create = ClockCreate;
    plugin->Delete = PanelPluginDeletePixmap;
    plugin->Resize = ClockResize;
    plugin->Tooltip = ClockTooltip;
    plugin->HandleButtonPress = ClockHandleButtonPress;
    plugin->Timeout = ClockTimeout;

    return plugin;
}

#endif

#endif // } USE_CLOCK

/// @}
