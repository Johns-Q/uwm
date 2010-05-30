///
///	@file swallow.c		@brief swallow panel plugin functions.
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
///	@ingroup panel
///	@defgroup swallow_plugin	The swallow panel plugin
///
///	This module add swallows to the panel.	Also called dock or slit in
///	other window managers.
///
/// @{

#include <xcb/xcb.h>
#include "uwm.h"

#ifdef USE_SWALLOW			// {

#include <sys/queue.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <xcb/xcb_icccm.h>

#include "array.h"
#include "config.h"

#include "client.h"
#include "hints.h"

#include "draw.h"
#include "image.h"
#include "icon.h"
#include "menu.h"

#include "panel.h"
#include "plugin/swallow.h"

/**
**	Swallow plugin typedef.
*/
typedef struct _swallow_plugin_ SwallowPlugin;

/**
**	Structure with clock panel plugin private data.
*/
struct _swallow_plugin_
{
    SLIST_ENTRY(_swallow_plugin_) Next;	///< singly-linked clock list
    Plugin *Plugin;			///< common plugin data

    char *Name;				///< window instance name for swallow
    char *Class;			///< window class name for swallow
    char *Command;			///< command to execute

    uint8_t Border;			///< size of swallow window border
    unsigned UserWidth:1;		///< flag user width of plugin
    unsigned UserHeight:1;		///< flag user height of plugin
    unsigned UseOld:1;			///< flag use old clients
};

    /// Swallow plugin list head structure
SLIST_HEAD(_swallow_head_, _swallow_plugin_);
    //
    /// list of all swallows of plugin
static struct _swallow_head_ Swallows = SLIST_HEAD_INITIALIZER(Swallows);

// ------------------------------------------------------------------------ //
// Callbacks

/**
**	Destroy a swallow panel plugin.
**
**	@param plugin	common panel plugin data
*/
static void SwallowDelete(Plugin * plugin)
{
    // destroy window if there is one
    if (plugin->Window) {
	xcb_get_property_cookie_t cookie;
	xcb_get_wm_protocols_reply_t protocols;
	SwallowPlugin *swallow_plugin;

	xcb_reparent_window(Connection, plugin->Window, RootWindow, 0, 0);
	xcb_change_save_set(Connection, XCB_SET_MODE_DELETE, plugin->Window);

	// kill only swallows which we own!
	swallow_plugin = plugin->Object;
	if (swallow_plugin->UseOld || !swallow_plugin->Command) {
	    return;
	}

	cookie =
	    xcb_get_wm_protocols_unchecked(Connection, plugin->Window,
	    Atoms.WM_PROTOCOLS.Atom);

	// FIXME: move into functions...
	// check if client supports WM_DELETE_WINDOW
	if (xcb_get_wm_protocols_reply(Connection, cookie, &protocols, NULL)) {
	    unsigned u;

	    for (u = 0; u < protocols.atoms_len; ++u) {
		if (protocols.atoms[u] == Atoms.WM_DELETE_WINDOW.Atom) {
		    ClientSendDeleteWindow(plugin->Window);
		    xcb_get_wm_protocols_reply_wipe(&protocols);
		    return;
		}
	    }
	    xcb_get_wm_protocols_reply_wipe(&protocols);
	}
	xcb_kill_client(Connection, plugin->Window);
    }
}

/**
**	Handle a panel resize.
**
**	@param plugin	common panel plugin data
*/
static void SwallowResize(Plugin * plugin)
{
    // clear area behind swallows
    xcb_clear_area(Connection, 0, plugin->Panel->Window, plugin->X, plugin->Y,
	plugin->Width, plugin->Height);

    if (plugin->Window) {
	SwallowPlugin *swallow_plugin;
	uint32_t values[2];

	swallow_plugin = plugin->Object;

	// FIXME: too many resize request send
	Debug(3, "swallow '%s' resize to %dx%d\n", swallow_plugin->Name,
	    plugin->Width, plugin->Height);
	values[0] = plugin->Width - swallow_plugin->Border * 2;
	values[1] = plugin->Height - swallow_plugin->Border * 2;
	xcb_configure_window(Connection, plugin->Window,
	    XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, values);
    }
}

/**
**	Determine if a window should be swallowed.
**
**	@param already_mapped	flag true window was already mapped
**	@param window		x11 window id.
**
**	@returns true if window was swallowd, false otherwise.
**
**	@todo support auto swallow, swallow WITHDRAWN windows in special
**	area.
*/
int SwallowTryWindow(int already_mapped, xcb_window_t window)
{
    SwallowPlugin *swallow_plugin;
    xcb_get_property_cookie_t cookie;
    xcb_get_wm_class_reply_t prop;

    cookie.sequence = 0;
    SLIST_FOREACH(swallow_plugin, &Swallows, Next) {
	if (swallow_plugin->Plugin->Window) {
	    continue;			// already filled slot
	}
	if (already_mapped && swallow_plugin->Command
	    && !swallow_plugin->UseOld) {
	    continue;			// don't use old clients
	}
	// request class hints, if not already done
	if (!cookie.sequence) {
	    cookie = xcb_get_wm_class_unchecked(Connection, window);
	    if (!xcb_get_wm_class_reply(Connection, cookie, &prop, NULL)) {
		return 0;		// can't get hints, give up
	    }
	}
	if ((!swallow_plugin->Name
		|| !strcmp(prop.instance_name, swallow_plugin->Name))
	    && (!swallow_plugin->Class
		|| !strcmp(prop.class_name, swallow_plugin->Class))) {
	    uint32_t value[1];
	    xcb_get_geometry_cookie_t cookie;
	    xcb_get_geometry_reply_t *geom;

	    cookie = xcb_get_geometry_unchecked(Connection, window);

	    Debug(3, "found swallow '%s'\n", prop.instance_name);

	    // swallow window
	    value[0] =
		XCB_EVENT_MASK_STRUCTURE_NOTIFY |
		XCB_EVENT_MASK_RESIZE_REDIRECT;
	    xcb_change_window_attributes(Connection, window, XCB_CW_EVENT_MASK,
		value);
	    xcb_change_save_set(Connection, XCB_SET_MODE_INSERT, window);
	    xcb_change_window_attributes(Connection, window,
		XCB_CW_BORDER_PIXEL, &Colors.PanelBG.Pixel);
	    xcb_reparent_window(Connection, window,
		swallow_plugin->Plugin->Panel->Window, 0, 0);
	    // raise window
	    // FIXME: didn't must be above panel?
	    value[0] = XCB_STACK_MODE_ABOVE;
	    xcb_configure_window(Connection, window,
		XCB_CONFIG_WINDOW_STACK_MODE, value);
	    // map window on screen
	    xcb_map_window(Connection, window);

	    swallow_plugin->Plugin->Window = window;

	    // update size (FIXME: only if not all user)
	    geom = xcb_get_geometry_reply(Connection, cookie, NULL);
	    if (geom) {
		Debug(3, "swallow '%s' %dx%d border %d\n", prop.instance_name,
		    geom->width, geom->height, geom->border_width);
		swallow_plugin->Border = geom->border_width;
		if (!swallow_plugin->UserWidth) {
		    swallow_plugin->Plugin->RequestedWidth =
			geom->width + 2 * swallow_plugin->Border;
		}
		if (!swallow_plugin->UserHeight) {
		    swallow_plugin->Plugin->RequestedHeight =
			geom->height + 2 * swallow_plugin->Border;
		}
		free(geom);
	    } else {
		Warning("Can't get geometry, expect errors\n");
	    }
	    PanelResize(swallow_plugin->Plugin->Panel);

	    xcb_get_wm_class_reply_wipe(&prop);
	    return 1;
	}
    }
    if (cookie.sequence) {
	xcb_get_wm_class_reply_wipe(&prop);
    }
    return 0;
}

/**
**	Determine if a map event was for a window that should be swallowed.
**
**	@param event	map event
**
**	@returns 1 if this window should be swallowed, 0 if not.
*/
int SwallowHandleMapRequest(const xcb_map_request_event_t * event)
{
    return SwallowTryWindow(0, event->window);
}

/**
**	Handle a destroy notify.
**
**	@param event	destroy notify event
*/
int SwallowHandleDestroyNotify(const xcb_destroy_notify_event_t * event)
{
    SwallowPlugin *swallow_plugin;

    Debug(4, "swallow search %x - %x\n", event->window, event->event);
    SLIST_FOREACH(swallow_plugin, &Swallows, Next) {
	if (event->window == swallow_plugin->Plugin->Window) {
	    Debug(3, "found destroy swallow '%s'\n", swallow_plugin->Name);

	    swallow_plugin->Plugin->Window = XCB_NONE;
	    // FIXME: try 0, 0 here: 0,0 is dynamic size
	    swallow_plugin->Plugin->RequestedWidth = 1;
	    swallow_plugin->Plugin->RequestedHeight = 1;
	    PanelResize(swallow_plugin->Plugin->Panel);
	    return 1;
	}
    }
    return 0;
}

/**
**	Handle a configure notify.
**
**	@param event	configure notify event
**
**	@returns true if event was handled, false otherwise.
*/
int SwallowHandleConfigureNotify(const xcb_configure_notify_event_t * event)
{
    SwallowPlugin *swallow_plugin;

    Debug(4, "swallow search %x - %x\n", event->window, event->event);
    SLIST_FOREACH(swallow_plugin, &Swallows, Next) {
	if (event->window == swallow_plugin->Plugin->Window) {
	    unsigned width;
	    unsigned height;

	    Debug(3, "found configure swallow %s\n", swallow_plugin->Name);

	    width = event->width + swallow_plugin->Border * 2;
	    height = event->height + swallow_plugin->Border * 2;
	    if (width != swallow_plugin->Plugin->RequestedWidth
		|| height != swallow_plugin->Plugin->RequestedHeight) {
		swallow_plugin->Plugin->RequestedWidth = width;
		swallow_plugin->Plugin->RequestedHeight = height;
		Debug(3, "swallow resize request\n");
		PanelResize(swallow_plugin->Plugin->Panel);
	    }
	    return 1;
	}
    }
    return 0;
}

/**
**	Handle a resize request.
**
**	@param event	resize request event
**
**	@returns true if event was handled, false otherwise.
**
**	@todo not used yet!!!
*/
int SwallowHandleResizeRequest(const xcb_resize_request_event_t * event)
{
    SwallowPlugin *swallow_plugin;

    Debug(4, "swallow search %x\n", event->window);
    SLIST_FOREACH(swallow_plugin, &Swallows, Next) {
	if (event->window == swallow_plugin->Plugin->Window) {
	    unsigned width;
	    unsigned height;

	    Debug(3, "found resize swallow %s\n", swallow_plugin->Name);

	    width = event->width + swallow_plugin->Border * 2;
	    height = event->height + swallow_plugin->Border * 2;
	    if (width != swallow_plugin->Plugin->RequestedWidth
		|| height != swallow_plugin->Plugin->RequestedHeight) {
		swallow_plugin->Plugin->RequestedWidth = width;
		swallow_plugin->Plugin->RequestedHeight = height;
		Debug(3, "swallow resize request\n");
		PanelResize(swallow_plugin->Plugin->Panel);
	    }
	    return 1;
	}
    }
    return 0;
}

// ---------------------------------------------------------------------------

/**
**	Initialize swallow plugin.
*/
void SwallowInit(void)
{
    SwallowPlugin *swallow_plugin;

#ifdef DEBUG
    // clients need to be initialized before this plugin!
    if (!ClientLayers[0].tqh_first && !ClientLayers[0].tqh_last) {
	FatalError("FIXME: clients missing init %s\n", __FUNCTION__);
    }
#endif

    SLIST_FOREACH(swallow_plugin, &Swallows, Next) {
	// not already swallowed, execute command
	if (swallow_plugin->Command && !swallow_plugin->Plugin->Window) {
	    CommandRun(swallow_plugin->Command);
	}
    }
}

/**
**	Cleanup Swallow plugin.
*/
void SwallowExit(void)
{
    SwallowPlugin *swallow_plugin;

    while (!SLIST_EMPTY(&Swallows)) {	// list deletion
	swallow_plugin = SLIST_FIRST(&Swallows);

	free(swallow_plugin->Name);
	free(swallow_plugin->Command);

	SLIST_REMOVE_HEAD(&Swallows, Next);
	free(swallow_plugin);
    }
}

// ------------------------------------------------------------------------ //
// Config

#ifdef USE_LUA

/**
**	Create a swallowed application panel plugin.
**
**	@param name	name of application to swallow
**	@param command	command used to start swallowed application
**	@param width	width to use (0 for default)
**	@param height	height to use (0 for default)
*/
Plugin *SwallowNew(const char *name, const char *command, unsigned width,
    unsigned height)
{
    Plugin *plugin;
    SwallowPlugin *swallow_plugin;

    if (!name) {
	Warning("cannot swallow a client with no name\n");
	return NULL;
    }
    // make sure this name isn't already used.
    SLIST_FOREACH(swallow_plugin, &Swallows, Next) {
	if (!strcmp(swallow_plugin->Name, name)) {
	    Warning("cannot swallow the same client multiple times\n");
	    return NULL;
	}
    }

    swallow_plugin = calloc(1, sizeof(SwallowPlugin));
    SLIST_INSERT_HEAD(&Swallows, swallow_plugin, Next);

    swallow_plugin->Name = strdup(name);
    swallow_plugin->Command = command ? strdup(command) : NULL;

    plugin = PanelPluginNew();
    plugin->Object = swallow_plugin;
    swallow_plugin->Plugin = plugin;

    plugin->Delete = SwallowDelete;
    plugin->Resize = SwallowResize;

    plugin->RequestedWidth = 1;
    if (width) {
	plugin->RequestedWidth = width;
	swallow_plugin->UserWidth = 1;
    }
    plugin->RequestedHeight = 1;
    if (height) {
	plugin->RequestedHeight = height;
	swallow_plugin->UserHeight = 1;
    }

    return plugin;
}

#else

/**
**	Create a new swallow panel plugin from config data.
**
**	@param array	configuration array for swallow panel plugin
*/
Plugin *SwallowConfig(const ConfigObject * array)
{
    Plugin *plugin;
    SwallowPlugin *swallow_plugin;
    const char *name;
    const char *sval;
    ssize_t ival;

    name = NULL;
    sval = NULL;
    if (!ConfigGetString(array, &name, "name", NULL)
	&& !ConfigGetString(array, &sval, "sval", NULL)) {
	Warning("cannot swallow a client with no name\n");
	return NULL;
    }
    // make sure this name/class isn't already used.
    SLIST_FOREACH(swallow_plugin, &Swallows, Next) {
	if ((name && swallow_plugin->Name
		&& !strcmp(swallow_plugin->Name, name))
	    || (sval && swallow_plugin->Class
		&& !strcmp(swallow_plugin->Class, sval))) {
	    Warning("cannot swallow the same name/class multiple times\n");
	    return NULL;
	}
    }

    swallow_plugin = calloc(1, sizeof(SwallowPlugin));
    SLIST_INSERT_HEAD(&Swallows, swallow_plugin, Next);

    if (name) {
	swallow_plugin->Name = strdup(name);
    }
    if (sval) {
	swallow_plugin->Class = strdup(sval);
    }
    if (ConfigGetString(array, &sval, "execute", NULL)) {
	swallow_plugin->Command = strdup(sval);
    }
    if (ConfigGetInteger(array, &ival, "use-old", NULL)) {
	swallow_plugin->UseOld = ival != 0;
    }
    if (ConfigGetInteger(array, &ival, "border", NULL)) {
	if (0 <= ival && ival <= 32) {
	    swallow_plugin->Border = ival;
	}
    }

    plugin = PanelPluginNew();
    plugin->Object = swallow_plugin;
    swallow_plugin->Plugin = plugin;

    plugin->Delete = SwallowDelete;
    plugin->Resize = SwallowResize;

    plugin->RequestedWidth = 1;
    if (ConfigGetInteger(array, &ival, "width", NULL)) {
	plugin->RequestedWidth = ival;
	swallow_plugin->UserWidth = 1;
    }
    plugin->RequestedHeight = 1;
    if (ConfigGetInteger(array, &ival, "height", NULL)) {
	plugin->RequestedHeight = ival;
	swallow_plugin->UserHeight = 1;
    }

    return plugin;
}

#endif

#endif // } USE_SWALLOW

/// @}
