///
///	@file systray.c		@brief systray panel plugin functions.
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
///	@defgroup systray_plugin	The systray panel plugin
///
///	This module adds system tray to the panel.
///
///	http://standards.freedesktop.org/systemtray-spec/latest/
///
///	@todo A tray icon must support the "client" or "plug" side of the
///	XEMBED specification. XEMBED is a protocol for cross-toolkit widget
///	embedding.
///
///	Alternatives:
///		- x11-misc/peksystray
///			http://peksystray.sourceforge.net/
///		- x11-plugins/wmsystray
///			http://kai.vm.bytemark.co.uk/~arashi/wmsystray/
///		- x11-misc/stalonetray
///			http://stalonetray.sourceforge.net/
/// @{

#include <xcb/xcb.h>
#include "uwm.h"

#ifdef USE_SYSTRAY			// {

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <xcb/xcb_atom.h>
#include <xcb/xcb_icccm.h>

#include "queue.h"
#include "core-array/core-array.h"
#include "core-rc/core-rc.h"

#include "client.h"
#include "hints.h"

#include "draw.h"
#include "image.h"
#include "icon.h"
#include "menu.h"

#include "panel.h"
#include "plugin/systray.h"

// ------------------------------------------------------------------------ //

/**
**	System tray window typedef.
*/
typedef struct _systray_window_ SystrayWindow;

/**
**	Structure to represent a docked window in system tray.
*/
struct _systray_window_
{
    SLIST_ENTRY(_systray_window_) Next;	///< singly-linked list

    xcb_window_t Window;		///< docked window self
    unsigned NeedsReparent:1;		///< flag docked window must reparent
};

/**
**	System tray plugin typedef.
*/
typedef struct _systray_plugin_ SystrayPlugin;

/**
**	Structure with system tray panel plugin private data.
*/
struct _systray_plugin_
{
    Plugin *Plugin;			///< common plugin data

    xcb_window_t Window;		///< systray window
    xcb_atom_t Atom;			///< _NET_SYSTEM_TRAY_Sx atom
    unsigned Owner:1;			///< we are owner of selection
    unsigned Orientation:1;		///< orientation of systray

    /// list of windows docked in system tray
     SLIST_HEAD(_systray_head_, _systray_window_) Docked;
};

    /// client initiates docking
#define SYSTEM_TRAY_REQUEST_DOCK    0
    /// begin ballon message
#define SYSTEM_TRAY_BEGIN_MESSAGE   1
    /// canel previous ballon message
#define SYSTEM_TRAY_CANCEL_MESSAGE  2

    /// orientation of icons in systray
#define _NET_SYSTEM_TRAY_ORIENTATION_HORZ 0
    /// orientation of icons in systray
#define _NET_SYSTEM_TRAY_ORIENTATION_VERT 1

static SystrayPlugin *Systray;		///< only one systray plugin

// ------------------------------------------------------------------------ //

/**
**	Layout items on systray.
**
**	@todo FIXME: split request/reply
*/
static void SystrayUpdate(void)
{
    int item_width;
    int item_height;
    SystrayWindow *docked;
    int x;
    int y;

    Debug(3, "%s: %dx%d\n", __FUNCTION__, Systray->Plugin->Width,
	Systray->Plugin->Height);

    // determine size of items in systray.
    if (Systray->Orientation == _NET_SYSTEM_TRAY_ORIENTATION_HORZ) {
	item_width = item_height = Systray->Plugin->Height;
    } else {
	item_width = item_height = Systray->Plugin->Width;
    }

    Debug(3, "\titem: %dx%d\n", item_width, item_height);

    x = 0;
    y = 0;
    SLIST_FOREACH(docked, &Systray->Docked, Next) {
	int xoffset;
	int yoffset;
	int width;
	int height;
	xcb_get_geometry_cookie_t cookie;
	xcb_get_geometry_reply_t *geom_reply;
	uint32_t values[4];
	xcb_configure_notify_event_t event;

	xoffset = 0;
	yoffset = 0;

	width = item_width;
	height = item_height;

	cookie = xcb_get_geometry_unchecked(Connection, docked->Window);
	geom_reply = xcb_get_geometry_reply(Connection, cookie, NULL);
	if (geom_reply) {
	    int ratio;

	    Debug(3, "\twindow %dx%d\n", geom_reply->width,
		geom_reply->height);
	    // keep window aspect ratio
	    ratio = (geom_reply->width * 65535) / geom_reply->height;

	    if (ratio > 65535) {
		if (width > geom_reply->width) {
		    width = geom_reply->width;
		}
		height = (width * 65535) / ratio;
	    } else {
		if (height > geom_reply->height) {
		    height = geom_reply->height;
		}
		width = (height * ratio) / 65535;
	    }

	    xoffset = (item_width - width) / 2;
	    yoffset = (item_height - height) / 2;

	    free(geom_reply);
	}

	Debug(3, "\t -> %dx%d%+d%+d\n", width, height, x + xoffset,
	    y + yoffset);

	values[0] = x + xoffset;
	values[1] = y + yoffset;
	values[2] = width;
	values[3] = height;
	xcb_configure_window(Connection, docked->Window,
	    XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH
	    | XCB_CONFIG_WINDOW_HEIGHT, values);

	//
	//	send configure notify event
	//
	event.response_type = XCB_CONFIGURE_NOTIFY;
	event.event = docked->Window;
	event.window = docked->Window;

	event.x = x + xoffset;
	event.y = y + yoffset;
	event.width = width;
	event.height = height;

	event.border_width = 0;
	event.above_sibling = XCB_NONE;
	event.override_redirect = 0;

	xcb_send_event(Connection, XCB_SEND_EVENT_DEST_POINTER_WINDOW,
	    docked->Window, XCB_EVENT_MASK_STRUCTURE_NOTIFY, (void *)&event);

	if (docked->NeedsReparent) {
	    xcb_reparent_window(Connection, docked->Window,
		Systray->Plugin->Window, x + xoffset, y + yoffset);
	}

	if (Systray->Orientation == _NET_SYSTEM_TRAY_ORIENTATION_HORZ) {
	    x += item_width;
	} else {
	    y += item_height;
	}
    }
}

/**
**	Add a window to systray.
**
**	@param window	window id of docking request
*/
static void SystrayAddWindow(xcb_window_t window)
{
    SystrayWindow *docked;
    uint32_t value;

    Debug(3, "%s: %x\n", __FUNCTION__, window);

    if (!Systray) {			// if no systray is running, just return
	Debug(2, "shouldn't get the message without systray\n");
	return;
    }
    if (!window) {			// add only valid windows
	return;
    }
    // if this window is already in systray, ignore it
    SLIST_FOREACH(docked, &Systray->Docked, Next) {
	if (docked->Window == window) {
	    Debug(2, "window is already docked in systray\n");
	    return;
	}
    }

    // add window to our list
    docked = calloc(1, sizeof(*docked));
    SLIST_INSERT_HEAD(&Systray->Docked, docked, Next);
    docked->Window = window;

    xcb_change_save_set(Connection, XCB_SET_MODE_INSERT, window);

    value =
	XCB_EVENT_MASK_POINTER_MOTION | XCB_EVENT_MASK_POINTER_MOTION_HINT |
	XCB_EVENT_MASK_STRUCTURE_NOTIFY | XCB_EVENT_MASK_RESIZE_REDIRECT;
    xcb_change_window_attributes(Connection, window, XCB_CW_EVENT_MASK,
	&value);

    // It's safe to reparent at (0, 0) since we call
    // PanelResize which will invoke SystrayResize callback
    xcb_reparent_window(Connection, window, Systray->Plugin->Window, 0, 0);
    // raise window
    value = XCB_STACK_MODE_ABOVE;
    xcb_configure_window(Connection, window, XCB_CONFIG_WINDOW_STACK_MODE,
	&value);
    // map window on screen
    xcb_map_window(Connection, window);

    // update requested size
    if (Systray->Orientation == _NET_SYSTEM_TRAY_ORIENTATION_HORZ) {
	if (Systray->Plugin->RequestedWidth > 1) {
	    Systray->Plugin->RequestedWidth += Systray->Plugin->Height;
	} else {
	    Systray->Plugin->RequestedWidth = Systray->Plugin->Height;
	}
    } else {
	if (Systray->Plugin->RequestedHeight > 1) {
	    Systray->Plugin->RequestedHeight += Systray->Plugin->Width;
	} else {
	    Systray->Plugin->RequestedHeight = Systray->Plugin->Width;
	}
    }

    // update panel containing systray
    PanelResize(Systray->Plugin->Panel);
}

// ------------------------------------------------------------------------ //
// Events

/**
**	Handle a resize request event.
**
**	Window docked in systray request a resize, update panel.
**
**	@param event	X11 resize request event
**
**	@returns 1 if handled, 0 otherwise.
*/
int SystrayHandleResizeRequest(const xcb_resize_request_event_t * event)
{
    SystrayWindow *docked;

    if (!Systray) {			// no systray ready
	return 0;
    }

    SLIST_FOREACH(docked, &Systray->Docked, Next) {
	// look if this window is docked
	if (docked->Window == event->window) {
	    uint32_t values[2];

	    // resize window
	    values[0] = event->width;
	    values[1] = event->height;
	    xcb_configure_window(Connection, docked->Window,
		XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, values);

	    // FIXME: this reresizes the window
	    SystrayUpdate();
	    return 1;
	}
    }
    return 0;
}

/**
**	Handle a configure request.
**
**	@param event	X11 configure request event
**
**	@returns 1 if handled, 0 otherwise.
*/
int SystrayHandleConfigureRequest(const xcb_configure_request_event_t * event)
{
    SystrayWindow *docked;

    if (!Systray) {			// no systray ready
	return 0;
    }

    SLIST_FOREACH(docked, &Systray->Docked, Next) {
	// look if this window is docked
	if (docked->Window == event->window) {
	    uint32_t values[7];
	    int i;

	    i = 0;
	    // send configure to docked client
	    if (event->value_mask & XCB_CONFIG_WINDOW_X) {
		values[i++] = event->x;
	    }
	    if (event->value_mask & XCB_CONFIG_WINDOW_X) {
		values[i++] = event->y;
	    }
	    if (event->value_mask & XCB_CONFIG_WINDOW_WIDTH) {
		values[i++] = event->width;
	    }
	    if (event->value_mask & XCB_CONFIG_WINDOW_HEIGHT) {
		values[i++] = event->height;
	    }
	    if (event->value_mask & XCB_CONFIG_WINDOW_BORDER_WIDTH) {
		values[i++] = event->border_width;
	    }
	    if (event->value_mask & XCB_CONFIG_WINDOW_SIBLING) {
		values[i++] = event->sibling;
	    }
	    if (event->value_mask & XCB_CONFIG_WINDOW_STACK_MODE) {
		values[i++] = event->stack_mode;
	    }

	    xcb_configure_window(Connection, docked->Window, event->value_mask,
		values);

	    SystrayUpdate();
	    return 1;
	}
    }
    return 0;
}

/**
**	Handle a reparent notify event.
**
**	@param event	X11 reparent notify event
**
**	@returns 1 if handled, 0 otherwise.
*/
int SystrayHandleReparentNotify(const xcb_reparent_notify_event_t * event)
{
    SystrayWindow *docked;

    if (!Systray) {			// no systray ready
	return 0;
    }

    SLIST_FOREACH(docked, &Systray->Docked, Next) {
	// look if this window is docked
	if (docked->Window == event->window) {
	    if (event->parent != Systray->Plugin->Window) {
		Debug(1, "docked window reparents\n");
		docked->NeedsReparent = 1;
	    }
	    // reparent it back
	    SystrayUpdate();
	    return 1;
	}
    }
    return 0;
}

/**
**	Handle a destroy event.
**
**	@param window	window that was destroyed
**
**	@returns 1 if handled, 0 otherwise.
*/
int SystrayHandleDestroyNotify(xcb_window_t window)
{
    SystrayWindow *docked;

    if (!Systray) {			// no systray ready
	return 0;
    }

    SLIST_FOREACH(docked, &Systray->Docked, Next) {
	// look if this window is docked
	if (docked->Window == window) {
	    // remove from list and free
	    SLIST_REMOVE(&Systray->Docked, docked, _systray_window_, Next);
	    free(docked);

	    //
	    //	Update requested size.
	    //
	    if (Systray->Orientation == _NET_SYSTEM_TRAY_ORIENTATION_HORZ) {
		Systray->Plugin->RequestedWidth -= Systray->Plugin->Height;
		if (Systray->Plugin->RequestedWidth <= 0) {
		    Systray->Plugin->RequestedWidth = 1;
		}
	    } else {
		Systray->Plugin->RequestedHeight -= Systray->Plugin->Width;
		if (Systray->Plugin->RequestedHeight <= 0) {
		    Systray->Plugin->RequestedHeight = 1;
		}
	    }

	    // resize panel.
	    PanelResize(Systray->Plugin->Panel);

	    return 1;
	}
    }
    return 0;
}

/**
**	Handle a selection clear event.
**
**	@param event	X11 selection clear event
++
**	@returns 1 if handled, 0 otherwise.
*/
int SystrayHandleSelectionClear(const xcb_selection_clear_event_t * event)
{
    if (event->selection == Systray->Atom) {
	Debug(1, "lost _NET_SYSTEM_TRAY selection");
	Systray->Owner = 0;
	return 1;
    }

    return 0;
}

/**
**	Handle a client message sent to systray window.
**
**	_NET_SYSTEM_TRAY_OPCODE
**
**	@param event	X11 client message event
**
**	@todo _NET_SYSTEM_TRAY_MESSAGE_DATA
*/
void SystrayHandleClientMessageEvent(const xcb_client_message_event_t * event)
{
    Debug(3, "%s: %x\n", __FUNCTION__, event->data.data32[1]);

    switch (event->data.data32[1]) {
	case SYSTEM_TRAY_REQUEST_DOCK:
	    SystrayAddWindow(event->data.data32[2]);
	    break;
	case SYSTEM_TRAY_BEGIN_MESSAGE:
	    Debug(3, "%s: FIXME: SYSTEM_TRAY_BEGIN_MESSAGE\n", __FUNCTION__);
	    break;
	case SYSTEM_TRAY_CANCEL_MESSAGE:
	    Debug(3, "%s: FIXME: SYSTEM_TRAY_CANCEL_MESSAGE\n", __FUNCTION__);
	    break;
	default:
	    Warning("invalid opcode in systray event\n");
	    break;
    }
}

// ------------------------------------------------------------------------ //
// Callback

/**
**	Set size of a systray panel plugin.
**
**	@param plugin	common panel plugin data of systray
**	@param width	width of plugin
**	@param height	height of plugin
*/
static void SystraySetSize(Plugin * plugin, unsigned width, unsigned height)
{
    int n;
    SystrayWindow *docked;

    n = 0;
    SLIST_FOREACH(docked, &Systray->Docked, Next) {
	++n;
    }

    if (!width) {
	if (n > 0) {
	    plugin->RequestedWidth = plugin->Width = n * height;
	} else {
	    plugin->RequestedWidth = plugin->Width = 1;
	}
    } else if (!height) {
	if (n > 0) {
	    plugin->RequestedHeight = plugin->Height = n * width;
	} else {
	    plugin->RequestedHeight = plugin->Height = 1;
	}
    }

    Debug(3, "%s: %dx%d\n", __FUNCTION__, plugin->RequestedWidth,
	plugin->RequestedHeight);
}

/**
**	Initialize a systray panel plugin.
**
**	@param plugin	common panel plugin data of systray
*/
static void SystrayCreate(Plugin * plugin)
{
    uint32_t values[3];

    if (plugin->Window) {		// map systray window.
	// resize window
	values[0] = plugin->Width;
	values[1] = plugin->Height;
	// raise window
	values[2] = XCB_STACK_MODE_ABOVE;
	xcb_configure_window(Connection, Systray->Window,
	    XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT |
	    XCB_CONFIG_WINDOW_STACK_MODE, values);
	// map window on screen
	xcb_map_window(Connection, Systray->Window);
    }

    Systray->Orientation =
	plugin->Height ==
	1 ? _NET_SYSTEM_TRAY_ORIENTATION_VERT :
	_NET_SYSTEM_TRAY_ORIENTATION_HORZ;
    // set _NET_SYSTEM_TRAY_ORIENTATION
    AtomSetCardinal(Systray->Window, &Atoms.NET_SYSTEM_TRAY_ORIENTATION,
	Systray->Orientation);

    //
    // Get selection if we don't already own it.
    // If we did already own it, getting it again would cause problems
    // with some clients due to way restarts are handled.
    //
    if (!Systray->Owner) {
	xcb_client_message_event_t event;

	// fill event
	memset(&event, 0, sizeof(event));
	event.response_type = XCB_CLIENT_MESSAGE;
	event.format = 32;
	event.window = XcbScreen->root;
	event.type = Atoms.MANAGER.Atom;
	event.data.data32[0] = XCB_CURRENT_TIME;
	event.data.data32[1] = Systray->Atom;
	event.data.data32[2] = Systray->Window;

	xcb_set_selection_owner(Connection, Systray->Window, Systray->Atom,
	    XCB_CURRENT_TIME);

	xcb_send_event(Connection, XCB_SEND_EVENT_DEST_POINTER_WINDOW,
	    XcbScreen->root, 0xFFFFFF, (void *)&event);

	Systray->Owner = 1;
	Debug(3, "selection prepared\n");
    }
}

/**
**	Resize a systray panel plugin.
**
**	@param plugin	common panel plugin data of systray
*/
static void SystrayResize(Plugin * plugin)
{
    uint32_t values[2];

    // resize window
    values[0] = plugin->Width;
    values[1] = plugin->Height;
    xcb_configure_window(Connection, Systray->Window,
	XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, values);

    SystrayUpdate();
}

// ---------------------------------------------------------------------------

/**
**	Initialize system tray plugin data.
**
**	@note system tray is kept during restart!
*/
void SystrayInit(void)
{
    if (!Systray) {			// no systray has been requested
	return;
    }
    if (!Systray->Plugin) {
	// systray item has been removed from configuration
	xcb_destroy_window(Connection, Systray->Window);
	free(Systray);
	Systray = NULL;
	return;
    }

    if (!Systray->Window) {		// no systray yet
	const xcb_setup_t *setup;
	xcb_screen_iterator_t iter;
	int i;
	char *atom_name;
	xcb_intern_atom_cookie_t cookie;
	xcb_intern_atom_reply_t *reply;

	// get screen number of our screen
	setup = xcb_get_setup(Connection);
	iter = xcb_setup_roots_iterator(setup);
	for (i = 0; i < xcb_setup_roots_length(setup); ++i) {
	    if (XcbScreen == iter.data) {
		break;
	    }
	    xcb_screen_next(&iter);
	}
	// FIXME: perhaps there are better ways to get screen number

	if (!(atom_name = xcb_atom_name_by_screen("_NET_SYSTEM_TRAY", i))) {
	    Warning("error getting systray atom\n");
	    return;
	}
	Debug(4, "our try '%s'\n", atom_name);

	cookie =
	    xcb_intern_atom_unchecked(Connection, 0, strlen(atom_name),
	    atom_name);
	free(atom_name);

	Systray->Window = xcb_generate_id(Connection);
	xcb_create_window(Connection, XCB_COPY_FROM_PARENT, Systray->Window,
	    XcbScreen->root, -1, -1, 1, 1, 0, XCB_COPY_FROM_PARENT,
	    XCB_COPY_FROM_PARENT, XCB_CW_BACK_PIXEL, &Colors.PanelBG.Pixel);

	// FIXME: can delay reply until create
	if (!(reply = xcb_intern_atom_reply(Connection, cookie, NULL))) {
	    Warning("error getting systray atom\n");
	    return;
	}
	Systray->Atom = reply->atom;
	free(reply);

	Debug(3, "systray window %x\n", Systray->Window);
    }
    Systray->Plugin->Window = Systray->Window;
}

/**
**	Cleanup the system tray plugin.
*/
void SystrayExit(void)
{
    if (Systray) {
	if (KeepRunning) {
	    // If restarting we just reparent systray window to root window.
	    // We need to keep systray around and visible so that
	    // we don't cause problems with windows in systray.
	    // It seems every application handles docking differently...
	    xcb_reparent_window(Connection, Systray->Window, XcbScreen->root,
		0, 0);
	    Systray->Plugin = NULL;
	} else {
	    // free memory for docked window list
	    while (!SLIST_EMPTY(&Systray->Docked)) {	// list deletion
		SystrayWindow *docked;

		docked = SLIST_FIRST(&Systray->Docked);

		xcb_reparent_window(Connection, docked->Window,
		    XcbScreen->root, 0, 0);

		SLIST_REMOVE_HEAD(&Systray->Docked, Next);
		free(docked);
	    }

	    // release selection
	    if (Systray->Owner) {
		xcb_set_selection_owner(Connection, Systray->Atom, XCB_NONE,
		    XCB_CURRENT_TIME);
	    }
	    // destroy systray window.
	    xcb_destroy_window(Connection, Systray->Window);
	    free(Systray);
	}
    }
}

// ------------------------------------------------------------------------ //
// Config

#ifdef USE_RC				// {

/**
**	Create a new systray panel plugin from config data.
**
**	@param array	configuration array for systray panel plugin
**
**	@returns created sysstray panel plugin.
*/
Plugin *SystrayConfig( __attribute__ ((unused))
    const ConfigObject * array)
{
    Plugin *plugin;

    if (Systray && Systray->Plugin) {
	Warning("only one systray allowed\n");
	return NULL;
    }

    if (!Systray) {			// systray kept over restart
	Systray = calloc(1, sizeof(*Systray));
	SLIST_INIT(&Systray->Docked);
    }

    plugin = PanelPluginNew();
    plugin->Object = Systray;
    Systray->Plugin = plugin;

    plugin->RequestedWidth = 1;
    plugin->RequestedHeight = 1;

    plugin->SetSize = SystraySetSize;
    plugin->Create = SystrayCreate;
    plugin->Resize = SystrayResize;

    Debug(3, "systray %p created\n", plugin);

    return plugin;
}

#endif // } USE_RC

#endif // } USE_SYSTRAY

/// @}
