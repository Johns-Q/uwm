///
///	@file hints.c	@brief window manager hints functions.
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
///	@defgroup hint The hint module.
///
///	This module contains Extended Window Manager Hints reading/writing
///	functions.
///
///	(EWMH) Extended Window Manager Hints +
///	(ICCCM) Inter-Client Communication Conventions Manual
///
///	@see http://standards.freedesktop.org/wm-spec/wm-spec-latest.html
///	@see http://www.tronche.com/gui/x/icccm/
///
///	When compiled with #USE_MOTIF_HINTS, _MOTIF_WM_HINTS is also supported.
///	@see Motif Programming Manual
///
///	@todo
///		- _NET_WM_STATE_MODAL isn't supported
///		- _NET_WM_STATE_DEMANDS_ATTENTION isn't supported
///
/// @{

#include <xcb/xcb.h>
#include "uwm.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/queue.h>

#include <xcb/xcb_icccm.h>
#include <xcb/xcb_atom.h>

#include "core-array/core-array.h"
#include "core-rc/core-rc.h"

#include "draw.h"
#include "client.h"
#include "hints.h"
#include "image.h"
#include "icon.h"
#include "menu.h"
#include "desktop.h"
#include "border.h"

// ------------------------------------------------------------------------ //
// Atom
// ------------------------------------------------------------------------ //

///
///	@defgroup atom The atom module.
///
///	This module contains the lowlevel atom reading/writing functions.
///
/// @{

/**
**	Table of all atom used in the windowmanger, which are not defined in
**	/usr/include/xcb/xcb_atom.h.
*/
AtomTable Atoms = {
// *INDENT-OFF*	indent didn't support GNU dot syntax
    .COMPOUND_TEXT = {.Name = "COMPOUND_TEXT"},
    .UTF8_STRING = {.Name = "UTF8_STRING"},
    .XROOTPMAP_ID = {.Name = "_XROOTPMAP_ID"},
    .MANAGER = {.Name = "MANAGER"},

    .WM_STATE = {.Name = "WM_STATE"},
    .WM_PROTOCOLS = {.Name = "WM_PROTOCOLS"},
    .WM_DELETE_WINDOW = {.Name = "WM_DELETE_WINDOW"},
    .WM_TAKE_FOCUS = {.Name = "WM_TAKE_FOCUS"},
    .WM_CHANGE_STATE = {.Name = "WM_CHANGE_STATE"},
#ifdef USE_COLORMAP
    .WM_COLORMAP_WINDOWS = {.Name = "WM_COLORMAP_WINDOWS"},
#endif

    // first atom for _NET_SUPPORTED
#define ATOM_NET_FIRST NET_SUPPORTED
    .NET_SUPPORTED = {.Name = "_NET_SUPPORTED"},
    .NET_SUPPORTING_WM_CHECK = {.Name = "_NET_SUPPORTING_WM_CHECK"},
    .NET_NUMBER_OF_DESKTOPS = {.Name = "_NET_NUMBER_OF_DESKTOPS"},
    .NET_DESKTOP_NAMES = {.Name = "_NET_DESKTOP_NAMES"},
    .NET_DESKTOP_GEOMETRY = {.Name = "_NET_DESKTOP_GEOMETRY"},
    .NET_DESKTOP_VIEWPORT = {.Name = "_NET_DESKTOP_VIEWPORT"},
    .NET_CURRENT_DESKTOP = {.Name = "_NET_CURRENT_DESKTOP"},
    .NET_ACTIVE_WINDOW = {.Name = "_NET_ACTIVE_WINDOW"},
    .NET_WORKAREA = {.Name = "_NET_WORKAREA"},
    .NET_FRAME_EXTENTS = {.Name = "_NET_FRAME_EXTENTS"},
    .NET_WM_DESKTOP = {.Name = "_NET_WM_DESKTOP"},
    .NET_SHOWING_DESKTOP = {.Name = "_NET_SHOWING_DESKTOP"},

    .NET_WM_STATE = {.Name = "_NET_WM_STATE"},
    .NET_WM_STATE_MODAL = {.Name = "_NET_WM_STATE_MODAL"},
    .NET_WM_STATE_STICKY = {.Name = "_NET_WM_STATE_STICKY"},
    .NET_WM_STATE_MAXIMIZED_VERT = {.Name = "_NET_WM_STATE_MAXIMIZED_VERT"},
    .NET_WM_STATE_MAXIMIZED_HORZ = {.Name = "_NET_WM_STATE_MAXIMIZED_HORZ"},
    .NET_WM_STATE_SHADED = {.Name = "_NET_WM_STATE_SHADED"},
    .NET_WM_STATE_SKIP_TASKBAR = {.Name = "_NET_WM_STATE_SKIP_TASKBAR"},
    .NET_WM_STATE_SKIP_PAGER = {.Name = "_NET_WM_STATE_SKIP_PAGER"},
    .NET_WM_STATE_HIDDEN = {.Name = "_NET_WM_STATE_HIDDEN"},
    .NET_WM_STATE_FULLSCREEN = {.Name = "_NET_WM_STATE_FULLSCREEN"},
    .NET_WM_STATE_ABOVE = {.Name = "_NET_WM_STATE_ABOVE"},
    .NET_WM_STATE_BELOW = {.Name = "_NET_WM_STATE_BELOW"},
    .NET_WM_STATE_DEMANDS_ATTENTION =
	{.Name ="_NET_WM_STATE_DEMANDS_ATTENTION"},

    .NET_WM_ALLOWED_ACTIONS = {.Name = "_NET_WM_ALLOWED_ACTIONS"},
    .NET_WM_ACTION_MOVE = {.Name = "_NET_WM_ACTION_MOVE"},
    .NET_WM_ACTION_RESIZE = {.Name = "_NET_WM_ACTION_RESIZE"},
    .NET_WM_ACTION_MINIMIZE = {.Name = "_NET_WM_ACTION_MINIMIZE"},
    .NET_WM_ACTION_SHADE = {.Name = "_NET_WM_ACTION_SHADE"},
    .NET_WM_ACTION_STICK = {.Name = "_NET_WM_ACTION_STICK"},
    .NET_WM_ACTION_MAXIMIZE_HORZ = {.Name = "_NET_WM_ACTION_MAXIMIZE_HORZ"},
    .NET_WM_ACTION_MAXIMIZE_VERT = {.Name = "_NET_WM_ACTION_MAXIMIZE_VERT"},
    .NET_WM_ACTION_FULLSCREEN = {.Name = "_NET_WM_ACTION_FULLSCREEN"},
    .NET_WM_ACTION_CHANGE_DESKTOP = {.Name = "_NET_WM_ACTION_CHANGE_DESKTOP"},
    .NET_WM_ACTION_CLOSE = {.Name = "_NET_WM_ACTION_CLOSE"},
    .NET_WM_ACTION_ABOVE = {.Name = "_NET_WM_ACTION_ABOVE"},
    .NET_WM_ACTION_BELOW = {.Name = "_NET_WM_ACTION_BELOW"},

    .NET_CLOSE_WINDOW = {.Name = "_NET_CLOSE_WINDOW"},
    .NET_MOVERESIZE_WINDOW = {.Name = "_NET_MOVERESIZE_WINDOW"},
    .NET_WM_NAME = {.Name = "_NET_WM_NAME"},
    .NET_WM_ICON = {.Name = "_NET_WM_ICON"},
    .NET_WM_WINDOW_TYPE = {.Name = "_NET_WM_WINDOW_TYPE"},
    .NET_WM_WINDOW_TYPE_DESKTOP = {.Name = "_NET_WM_WINDOW_TYPE_DESKTOP"},
    .NET_WM_WINDOW_TYPE_DOCK = {.Name = "_NET_WM_WINDOW_TYPE_DOCK"},
    .NET_WM_WINDOW_TYPE_TOOLBAR = {.Name = "_NET_WM_WINDOW_TYPE_TOOLBAR"},
    .NET_WM_WINDOW_TYPE_MENU = {.Name = "_NET_WM_WINDOW_TYPE_MENU"},
    .NET_WM_WINDOW_TYPE_UTILITY = {.Name = "_NET_WM_WINDOW_TYPE_UTILITY"},
    .NET_WM_WINDOW_TYPE_SPLASH = {.Name = "_NET_WM_WINDOW_TYPE_SPLASH"},
    .NET_WM_WINDOW_TYPE_DIALOG = {.Name = "_NET_WM_WINDOW_TYPE_DIALOG"},
    .NET_WM_WINDOW_TYPE_NORMAL = {.Name = "_NET_WM_WINDOW_TYPE_NORMAL"},

    .NET_CLIENT_LIST = {.Name = "_NET_CLIENT_LIST"},
    .NET_CLIENT_LIST_STACKING = {.Name = "_NET_CLIENT_LIST_STACKING"},
    .NET_WM_STRUT_PARTIAL = {.Name = "_NET_WM_STRUT_PARTIAL"},
    .NET_WM_STRUT = {.Name = "_NET_WM_STRUT"},
    .NET_SYSTEM_TRAY_OPCODE = {.Name = "_NET_SYSTEM_TRAY_OPCODE"},
    .NET_SYSTEM_TRAY_ORIENTATION = {.Name = "_NET_SYSTEM_TRAY_ORIENTATION"},
    .NET_WM_WINDOW_OPACITY = {.Name = "_NET_WM_WINDOW_OPACITY"},
    // last atom for _NET_SUPPORTED
#define ATOM_NET_LAST NET_WM_WINDOW_OPACITY

#ifdef USE_MOTIF_HINTS
    .MOTIF_WM_HINTS = {.Name = "_MOTIF_WM_HINTS"},
#endif

    .UWM_RESTART = {.Name = "_UWM_RESTART"},
    .UWM_EXIT = {.Name = "_UWM_EXIT"}
    // DON'T add here see warnings in AtomInit()
// *INDENT-ON*
};

/**
**	@def ATOM_NET_FIRST
**	first atom for _NET_SUPPORTED
*/

/**
**	@def ATOM_NET_LAST
**	last atom for _NET_SUPPORTED
**
*/

    /// window for NET_SUPPORTING_WM_CHECK
static xcb_window_t AtomSupportingWindow;

/**
**	_NET_WM_STATE client message.
*/
enum
{
    _NET_WM_STATE_REMOVE = 0,		///< remove/unset property
    _NET_WM_STATE_ADD = 1,		///< add/set property
    _NET_WM_STATE_TOGGLE = 2,		///< toggle property
};

/**
**	Send cardinal atom get request.
**
**	@param window	window
**	@param atom	atom to read
**
**	@returns cookie to fetch reply.
*/
xcb_get_property_cookie_t AtomCardinalRequest(xcb_window_t window,
    const Atom * atom)
{
    return xcb_get_property_unchecked(Connection, 0, window, atom->Atom,
	CARDINAL, 0, UINT32_MAX);
}

/**
**	Read cardinal atom get reply.
**
**	@param cookie	cookie of get property request
**	@param value	pointer to location to save atom
**
**	@returns 1 on success, 0 on failure.
*/
int AtomGetCardinal(xcb_get_property_cookie_t cookie, uint32_t * value)
{
    xcb_get_property_reply_t *reply;

    reply = xcb_get_property_reply(Connection, cookie, NULL);
    if (reply) {
	if (xcb_get_property_value_length(reply) == sizeof(uint32_t)) {
	    *value = *(uint32_t *) xcb_get_property_value(reply);
	    free(reply);
	    return 1;
	}
	free(reply);
    }
    return 0;
}

/**
**	Set cardinal atom.
**
**	@param window	window owning atom
**	@param atom	atom to set
**	@param value	value of atom
*/
void AtomSetCardinal(xcb_window_t window, const Atom * atom, uint32_t value)
{
    xcb_change_property(Connection, XCB_PROP_MODE_REPLACE, window, atom->Atom,
	CARDINAL, 32, 1, &value);
}

/**
**	Set window atom.
**
**	@param window	window
**	@param atom	atom to set
**	@param value	value of atom
*/
void AtomSetWindow(xcb_window_t window, const Atom * atom, uint32_t value)
{
    xcb_change_property(Connection, XCB_PROP_MODE_REPLACE, window, atom->Atom,
	WINDOW, 32, 1, &value);
}

/**
**	Set pixmap atom.
**
**	@param window	window
**	@param atom	atom to set
**	@param value	value of atom
*/
void AtomSetPixmap(xcb_window_t window, const Atom * atom, xcb_pixmap_t value)
{
    xcb_change_property(Connection, XCB_PROP_MODE_REPLACE, window, atom->Atom,
	PIXMAP, 32, 1, &value);
}

/**
**
**	Prepare initialize atoms.
**
**	@warning BugAlert: AtomTable.COMPOUND_TEXT must be first atom and
**	AtomTable.UWM_EXIT last atom in #AtomTable.
*/
void AtomPreInit(void)
{
    Atom *atom;

    // send requests
    for (atom = &Atoms.COMPOUND_TEXT; atom <= &Atoms.UWM_EXIT; ++atom) {
	IfDebug(if (!atom->Name) {
	    Debug(2, "missing atom %td\n", atom - &Atoms.COMPOUND_TEXT);
		continue;}
	) ;

	atom->Cookie =
	    xcb_intern_atom_unchecked(Connection, 0, strlen(atom->Name),
	    atom->Name);
    }
}

/**
**	Initialize atoms.
**
**	@warning BugAlert: AtomTable.COMPOUND_TEXT must be first atom and
**	AtomTable.UWM_EXIT last atom in #AtomTable.
*/
void AtomInit(void)
{
    Atom *atom;
    int i;
    uint32_t *values;
    xcb_window_t father;

    //
    //	Intern all atoms
    //
    for (atom = &Atoms.COMPOUND_TEXT; atom <= &Atoms.UWM_EXIT; ++atom) {
	xcb_intern_atom_reply_t *reply;

	if ((reply = xcb_intern_atom_reply(Connection, atom->Cookie, NULL))) {
	    atom->Atom = reply->atom;
	    free(reply);
	}
    }

    //
    //	_NET_SUPPORTED
    //
    //	This property MUST be set by Window Manager to indicate which
    //	hints it supports.
    values = alloca((&Atoms.UWM_EXIT - &Atoms.COMPOUND_TEXT + 1)
	* sizeof(*values));
    for (i = 0, atom = &Atoms.ATOM_NET_FIRST; atom <= &Atoms.ATOM_NET_LAST;
	++i, ++atom) {
	values[i] = atom->Atom;
    }
    xcb_change_property(Connection, XCB_PROP_MODE_REPLACE, XcbScreen->root,
	Atoms.NET_SUPPORTED.Atom, ATOM, 32, i, values);

    //
    //	_NET_SUPPORTING_WM_CHECK
    //
    //	Window Manager MUST set this property on root window to be
    //	ID of child window created by himself
    //

    // create our own window
    father = xcb_generate_id(Connection);
    xcb_create_window(Connection, XCB_COPY_FROM_PARENT, father,
	XcbScreen->root, -1, -1, 1, 1, 0, XCB_COPY_FROM_PARENT,
	XCB_COPY_FROM_PARENT, XCB_CW_BACK_PIXEL, &Colors.PanelBG.Pixel);

    xcb_change_property(Connection, XCB_PROP_MODE_REPLACE, XcbScreen->root,
	Atoms.NET_SUPPORTING_WM_CHECK.Atom, WINDOW, 32, 1, &father);

    xcb_change_property(Connection, XCB_PROP_MODE_REPLACE, father,
	Atoms.NET_SUPPORTING_WM_CHECK.Atom, WINDOW, 32, 1, &father);

    // set window manager name
    xcb_change_property(Connection, XCB_PROP_MODE_REPLACE, father,
	Atoms.NET_WM_NAME.Atom, Atoms.UTF8_STRING.Atom, 8, 4, "µwm");
    AtomSupportingWindow = father;

    // _NET_DESKTOP_GEOMETRY
    values[0] = XcbScreen->width_in_pixels;
    values[1] = XcbScreen->height_in_pixels;
    xcb_change_property(Connection, XCB_PROP_MODE_REPLACE, XcbScreen->root,
	Atoms.NET_DESKTOP_GEOMETRY.Atom, CARDINAL, 32, 2, values);

    // _NET_DESKTOP_VIEWPORT
    values[0] = 0;
    values[1] = 0;
    xcb_change_property(Connection, XCB_PROP_MODE_REPLACE, XcbScreen->root,
	Atoms.NET_DESKTOP_VIEWPORT.Atom, CARDINAL, 32, 2, values);

    // _NET_CLIENT_LIST and _NET_CLIENT_LIST_STACKING set later.

    // _NET_SHOWING_DESKTOP, _NET_NUMBER_OF_DESKTOPS, _NET_DESKTOP_NAMES
    // and _NET_CURRENT_DESKTOP set by desktop module

    Debug(2, "FIXME: more default atom setting\n");
    // NET_VIRTUAL_ROOTS, NET_DESKTOP_LAYOUT
}

/**
**	Cleanup atom module.
*/
void AtomExit(void)
{
    xcb_destroy_window(Connection, AtomSupportingWindow);
}

/// @}

// ------------------------------------------------------------------------ //
// Hint
// ------------------------------------------------------------------------ //

#ifdef USE_MOTIF_HINTS

/**
**	MWM Defines for _MOTIF_WM_HINTS
**@{
*/
#define MWM_HINTS_FUNCTIONS   (1L << 0)
#define MWM_HINTS_DECORATIONS (1L << 1)
#define MWM_HINTS_INPUT_MODE  (1L << 2)
#define MWM_HINTS_STATUS      (1L << 3)

#define MWM_FUNC_ALL	  (1L << 0)
#define MWM_FUNC_RESIZE   (1L << 1)
#define MWM_FUNC_MOVE	  (1L << 2)
#define MWM_FUNC_MINIMIZE (1L << 3)
#define MWM_FUNC_MAXIMIZE (1L << 4)
#define MWM_FUNC_CLOSE	  (1L << 5)

#define MWM_DECOR_ALL	   (1L << 0)
#define MWM_DECOR_BORDER   (1L << 1)
#define MWM_DECOR_RESIZEH  (1L << 2)
#define MWM_DECOR_TITLE    (1L << 3)
#define MWM_DECOR_MENU	   (1L << 4)
#define MWM_DECOR_MINIMIZE (1L << 5)
#define MWM_DECOR_MAXIMIZE (1L << 6)

#define MWM_INPUT_MODELESS		    0
#define MWM_INPUT_PRIMARY_APPLICATION_MODAL 1
#define MWM_INPUT_SYSTEM_MODAL		    2
#define MWM_INPUT_FULL_APPLICATION_MODAL    3

#define MWM_TEAROFF_WINDOW (1L << 0)
//@}

/**
**	Motif window manager hints structure.
*/
typedef struct
{
    uint32_t Flags;			///< flags which values are present
    uint32_t Functions;			///< function bitmask
    uint32_t Decorations;		///< decorations bitmask
    int32_t InputMode;			///< input mode value
    uint32_t Status;			///< status value
} MotifWmHints;

#endif

// ------------------------------------------------------------------------ //
// Getting hints
// ------------------------------------------------------------------------ //

/**
**	Send request for current desktop.
**
**	@returns cookie for later HintGetNetCurrentDesktop.
*/
xcb_get_property_cookie_t HintNetCurrentDesktopRequest(void)
{
    return AtomCardinalRequest(XcbScreen->root, &Atoms.NET_CURRENT_DESKTOP);
}

/**
**	Determine current desktop.
**
**	Changes the current desktop to 0, if not found.
**	Used to import current desktop on startup.
**
**	@param cookie	cookie of the HintNetCurrentDesktopRequest()
**
**	_NET_CURRENT_DESKTOP(CARDINAL)
*/
void HintGetNetCurrentDesktop(xcb_get_property_cookie_t cookie)
{
    uint32_t temp;

    if (AtomGetCardinal(cookie, &temp)) {
	DesktopChange(temp);
    } else {
	DesktopChange(0);
    }
}

/**
**	Get client's name.  Client name should be shown in window
**	title.
**
**	@param client	client for the property
**
**	_NET_WM_NAME
**	WM_NAME[TEXT] TEXT: STRING|COMPOUND_TEXT
**
**	@todo split request/reply
*/
void HintGetWMName(Client * client)
{
    xcb_get_property_cookie_t cookie;
    xcb_get_property_reply_t *reply;
    xcb_get_text_property_reply_t prop;
    int n;

    cookie =
	xcb_get_any_property_unchecked(Connection, 0, client->Window,
	Atoms.NET_WM_NAME.Atom, UINT32_MAX);

    free(client->Name);
    client->Name = NULL;

    reply = xcb_get_property_reply(Connection, cookie, NULL);
    if (reply) {
	if ((n = xcb_get_property_value_length(reply))) {
	    client->Name = malloc(n + 1);
	    memcpy(client->Name, xcb_get_property_value(reply), n);
	    client->Name[n] = '\0';

	    Debug(3, "UTF-8 name '%s'\n", client->Name);
	    free(reply);
	    return;
	}
	free(reply);
    }
    Debug(3, "NET_WM_NAME failed\n");

    cookie = xcb_get_wm_name_unchecked(Connection, client->Window);
    if (xcb_get_wm_name_reply(Connection, cookie, &prop, NULL)) {
	client->Name = malloc(prop.name_len + 1);
	memcpy(client->Name, prop.name, prop.name_len);
	client->Name[prop.name_len] = '\0';
	xcb_get_text_property_reply_wipe(&prop);
	return;
    }
    Debug(3, "WM_NAME failed\n");
}

/**
**	Get window class for client.
**
**	@param client	client for the property
**
**	WM_CLASS
**
**	@todo split request/reply
*/
static void HintGetWMClass(Client * client)
{
    xcb_get_property_cookie_t cookie;
    xcb_get_wm_class_reply_t prop;

    cookie = xcb_get_wm_class_unchecked(Connection, client->Window);
    if (xcb_get_wm_class_reply(Connection, cookie, &prop, NULL)) {
	client->InstanceName = strdup(prop.instance_name);
	client->ClassName = strdup(prop.class_name);
	xcb_get_wm_class_reply_wipe(&prop);
	Debug(3, "Class %s.%s\n", client->InstanceName, client->ClassName);
    }
}

/**
**	Request size hints for client.
**
**	WM_NORMAL_HINTS(WM_SIZE_HINTS)
**
**	@param client	client for the property
**
**	@returns cookie for later HintGetWMNormal.
*/
static xcb_get_property_cookie_t HintRequestWMNormal(Client * client)
{
    return xcb_get_wm_normal_hints_unchecked(Connection, client->Window);
}

/**
**	Get size hints for client.
**
**	WM_NORMAL_HINTS(WM_SIZE_HINTS)
**
**	@param cookie	cookie of HintRequestWMNormal
**	@param client	client for the property
*/
static void HintGetWMNormal(xcb_get_property_cookie_t cookie, Client * client)
{
    if (!xcb_get_wm_normal_hints_reply(Connection, cookie, &client->SizeHints,
	    NULL)) {
	Debug(3, "no normal size hints\n");
	client->SizeHints.flags = 0;
    }
    // setup default values for missing hints
    if (!(client->SizeHints.flags & XCB_SIZE_HINT_P_RESIZE_INC)) {
	client->SizeHints.width_inc = 1;
	client->SizeHints.height_inc = 1;
    }
    if (!(client->SizeHints.flags & XCB_SIZE_HINT_P_MIN_SIZE)) {
	client->SizeHints.min_width = 1;
	client->SizeHints.min_height = 1;
    }
    if (!(client->SizeHints.flags & XCB_SIZE_HINT_P_MAX_SIZE)) {
	client->SizeHints.max_width = XcbScreen->width_in_pixels;
	client->SizeHints.max_height = XcbScreen->height_in_pixels;
    }
    if (!(client->SizeHints.flags & XCB_SIZE_HINT_BASE_SIZE)) {
	client->SizeHints.base_width = client->SizeHints.min_width;
	client->SizeHints.base_height = client->SizeHints.min_height;
    }
    if (!(client->SizeHints.flags & XCB_SIZE_HINT_P_ASPECT)) {
	client->SizeHints.min_aspect_num = 0;
	client->SizeHints.min_aspect_den = 1;
	client->SizeHints.max_aspect_num = 0;
	client->SizeHints.max_aspect_den = 1;
    }
    if (!(client->SizeHints.flags & XCB_SIZE_HINT_P_WIN_GRAVITY)) {
	client->SizeHints.win_gravity = XCB_GRAVITY_NORTH_WEST;
    }
    // FIXME: p/us size/position aren't used yet
    if (client->SizeHints.flags & XCB_SIZE_HINT_US_SIZE) {
	Debug(3, "us_position %+d%+d\n", client->SizeHints.x,
	    client->SizeHints.y);
    }
    if (client->SizeHints.flags & XCB_SIZE_HINT_US_POSITION) {
	Debug(3, "us_size %dx%d\n", client->SizeHints.width,
	    client->SizeHints.height);
    }
    if (client->SizeHints.flags & XCB_SIZE_HINT_P_POSITION) {
	Debug(3, "p_size %dx%d\n", client->SizeHints.width,
	    client->SizeHints.height);
    }
    if (client->SizeHints.flags & XCB_SIZE_HINT_P_SIZE) {
	Debug(3, "p_position %+d%+d\n", client->SizeHints.x,
	    client->SizeHints.y);
    }

    Debug(3, "minsize %dx%d\n", client->SizeHints.min_width,
	client->SizeHints.min_height);
    Debug(3, "maxsize %dx%d\n", client->SizeHints.max_width,
	client->SizeHints.max_height);
}

#ifdef USE_COLORMAP

/**
**	Get colormap information for client.
**
**	@param client	client to fetch colormaps
**
**	WM_COLORMAP_WINDOWS[WINDOW]
**
**	@todo not written, should be called, if the property changes of the
**	root window.
*/
static void HintGetWMColormaps(Client * client)
{
    Debug(2, "%s: FIXME: for %p\n", __FUNCTION__, client);
}
#endif

/**
**	Send WM hints request for window.
**
**	WM_HINTS(WM_HINTS)
**
**	@param client	client for the property
**
**	@returns the cookie of get property request
*/
static xcb_get_property_cookie_t HintWMHintsRequest(Client * client)
{
    return xcb_get_wm_hints_unchecked(Connection, client->Window);
}

/**
**	Get WM hints for window.
**
**	WM_HINTS(WM_HINTS)
**
**	Only initial state is used.
**
**	@param cookie	cookie of HintWMHintsRequest
**	@param client	client for the property
**
**	@todo split request / reply
*/
static void HintGetWMHints(xcb_get_property_cookie_t cookie, Client * client)
{
    xcb_wm_hints_t wm_hints;

    if (xcb_get_wm_hints_reply(Connection, cookie, &wm_hints, NULL)
	&& wm_hints.flags & XCB_WM_HINT_STATE) {
	switch (wm_hints.initial_state) {
	    case XCB_WM_STATE_WITHDRAWN:
	    case XCB_WM_STATE_NORMAL:
		if (!(client->State & WM_STATE_MINIMIZED)) {
		    client->State |= WM_STATE_MAPPED;
		}
		break;
	    case XCB_WM_STATE_ICONIC:
		client->State |= WM_STATE_MINIMIZED;
		break;
	}
    } else {				// defaults to mapped
	client->State |= WM_STATE_MAPPED;
    }
}

/**
**	Get WM transient for property of window.
**
**	@param client	client for the property
**
**	@todo split request / reply
*/
static void HintGetWMTransientFor(Client * client)
{
    xcb_get_property_cookie_t cookie;

    cookie = xcb_get_wm_transient_for_unchecked(Connection, client->Window);
    if (xcb_get_wm_transient_for_reply(Connection, cookie, &client->Owner,
	    NULL)) {
	return;
    }
    client->Owner = XCB_NONE;
}

#ifdef USE_MOTIF_HINTS

/**
**	Get motif window manager hints (_MOTIF_WM_HINTS).
**
**	This hint is included for compatibility with some applications
**
**	@param client	client for the property
**
**	@todo split request / reply
**	should this stone age old motif hints be still supported?
*/
static void HintGetMotifHints(Client * client)
{
    MotifWmHints *motif_hints;
    xcb_get_property_cookie_t cookie;
    xcb_get_property_reply_t *reply;

    cookie =
	xcb_get_property_unchecked(Connection, 0, client->Window,
	Atoms.MOTIF_WM_HINTS.Atom, Atoms.MOTIF_WM_HINTS.Atom, 0,
	sizeof(*motif_hints));

    reply = xcb_get_property_reply(Connection, cookie, NULL);
    if (!reply) {
	return;
    }
    if (xcb_get_property_value_length(reply) != sizeof(*motif_hints)) {
	free(reply);
	return;
    }

    motif_hints = xcb_get_property_value(reply);
    if ((motif_hints->Flags & MWM_HINTS_FUNCTIONS)
	&& !(motif_hints->Functions & MWM_FUNC_ALL)) {

	if (!(motif_hints->Functions & MWM_FUNC_RESIZE)) {
	    client->Border &= ~BORDER_RESIZE;
	}
	if (!(motif_hints->Functions & MWM_FUNC_MOVE)) {
	    client->Border &= ~BORDER_MOVE;
	}
	if (!(motif_hints->Functions & MWM_FUNC_MINIMIZE)) {
	    client->Border &= ~BORDER_MINIMIZE;
	}
	if (!(motif_hints->Functions & MWM_FUNC_MAXIMIZE)) {
	    client->Border &= ~BORDER_MAXIMIZE;
	}
	if (!(motif_hints->Functions & MWM_FUNC_CLOSE)) {
	    client->Border &= ~BORDER_CLOSE;
	}
    }

    if ((motif_hints->Flags & MWM_HINTS_DECORATIONS)
	&& !(motif_hints->Decorations & MWM_DECOR_ALL)) {

	if (!(motif_hints->Decorations & MWM_DECOR_BORDER)) {
	    client->Border &= ~BORDER_OUTLINE;
	}
	if (!(motif_hints->Decorations & MWM_DECOR_TITLE)) {
	    client->Border &= ~BORDER_TITLE;
	}
	if (!(motif_hints->Decorations & MWM_DECOR_MINIMIZE)) {
	    client->Border &= ~BORDER_MINIMIZE;
	}
	if (!(motif_hints->Decorations & MWM_DECOR_MAXIMIZE)) {
	    client->Border &= ~BORDER_MAXIMIZE;
	}
    }

    free(reply);
}

#else
#endif

/**
**	Process _NET_WM_STATE atoms.
**
**	@param cookie	cookie of xcb_get_property_reply
**	@param client	client which get _NET_WM_STATE.
*/
static void HintGetNetWmState(xcb_get_property_cookie_t cookie,
    Client * client)
{
    xcb_get_property_reply_t *reply;

    if ((reply = xcb_get_property_reply(Connection, cookie, NULL))) {
	// check if reply is valid
	if (reply->value_len && reply->format == 32) {
	    xcb_atom_t *atoms;
	    int i;
	    int n;

	    atoms = xcb_get_property_value(reply);
	    n = xcb_get_property_value_length(reply) / sizeof(xcb_atom_t);
	    for (i = 0; i < n; ++i) {
		// FIXME: _NET_WM_STATE_MODAL
		if (atoms[i] == Atoms.NET_WM_STATE_STICKY.Atom) {
		    // indicates that the Window Manager SHOULD keep the
		    // window's position fixed on the screen, even when the
		    // virtual desktop scrolls.
		    client->State |= WM_STATE_STICKY;
		} else if (atoms[i] == Atoms.NET_WM_STATE_MAXIMIZED_VERT.Atom) {
		    // indicates that the window is vertically maximized.
		    client->State |= WM_STATE_MAXIMIZED_VERT;
		} else if (atoms[i] == Atoms.NET_WM_STATE_MAXIMIZED_HORZ.Atom) {
		    // indicates that the window is horizontally maximized.
		    client->State |= WM_STATE_MAXIMIZED_HORZ;
		} else if (atoms[i] == Atoms.NET_WM_STATE_SHADED.Atom) {
		    // indicates that the window is shaded.
		    client->State |= WM_STATE_SHADED;
		} else if (atoms[i] == Atoms.NET_WM_STATE_SKIP_TASKBAR.Atom) {
		    // indicates that the window should not be included on a
		    // taskbar.
		    client->State |= WM_STATE_NOLIST;
		} else if (atoms[i] == Atoms.NET_WM_STATE_SKIP_PAGER.Atom) {
		    // indicates that the window should not be included on a
		    // pager.
		    client->State |= WM_STATE_NOPAGER;
		} else if (atoms[i] == Atoms.NET_WM_STATE_HIDDEN.Atom) {
		    // should be set by the Window Manager to indicate that a
		    // window would not be visible on the screen if its
		    // desktop/viewport were active and its coordinates were
		    // within the screen bounds. The canonical example is that
		    // minimized windows should be in the _NET_WM_STATE_HIDDEN
		    // state.
		    client->State |= WM_STATE_HIDDEN;
		} else if (atoms[i] == Atoms.NET_WM_STATE_FULLSCREEN.Atom) {
		    // indicates that the window should fill the entire screen
		    // and have no window decorations. Additionally the Window
		    // Manager is responsible for restoring the original
		    // geometry after a switch from fullscreen back to normal
		    // window.
		    client->State |= WM_STATE_FULLSCREEN;
		    client->OnLayer = LAYER_FULLSCREEN;
		} else if (atoms[i] == Atoms.NET_WM_STATE_ABOVE.Atom) {
		    // indicates that the window should be on top of most
		    // windows
		    client->OnLayer = LAYER_ABOVE;
		} else if (atoms[i] == Atoms.NET_WM_STATE_BELOW.Atom) {
		    // indicates that the window should be below most windows
		    // windows
		    client->OnLayer = LAYER_BELOW;
		    // FIXME: _NET_WM_STATE_DEMANDS_ATTENTION
		} else {
		    Warning("_NET_WM_STATE=%u of client %#x unsupported\n",
			atoms[i], client->Window);
		}
	    }
	}
	free(reply);
    }
}

/**
**	Process _NET_WM_WINDOW_TYPE atoms.
**
**	@param cookie	cookie of xcb_get_property_reply
**	@param client	client which get _NET_WINDOW_TYPE.
*/
static void HintGetNetWmWindowType(xcb_get_property_cookie_t cookie,
    Client * client)
{
    xcb_get_property_reply_t *reply;

    if ((reply = xcb_get_property_reply(Connection, cookie, NULL))) {
	// check if reply is valid
	if (reply->value_len && reply->format == 32) {
	    xcb_atom_t *atoms;
	    int i;
	    int n;

	    atoms = xcb_get_property_value(reply);
	    n = xcb_get_property_value_length(reply) / sizeof(xcb_atom_t);
	    for (i = 0; i < n; ++i) {

		if (atoms[i] == Atoms.NET_WM_WINDOW_TYPE_DESKTOP.Atom) {
		    // desktop windows should appear on all desktops
		    client->State |=
			WM_STATE_STICKY | WM_STATE_NOLIST | WM_STATE_NOPAGER;
		    // are not manipulated by window manager
		    client->Border = BORDER_NONE;
		    client->OnLayer = LAYER_DESKTOP;
		    break;
		}
		if (atoms[i] == Atoms.NET_WM_WINDOW_TYPE_DOCK.Atom) {
		    // indicates dock or panel feature.  Typically Window
		    // Manager would keep such windows on top of all other
		    // windows.
		    client->State |= WM_STATE_NOLIST | WM_STATE_NOPAGER;
		    client->Border = BORDER_NONE;
		    client->OnLayer = LAYER_PANEL_DEFAULT;
		    break;
		}
		if (atoms[i] == Atoms.NET_WM_WINDOW_TYPE_NORMAL.Atom) {
		    // indicates that this is a normal, top-level window,
		    // either managed or override-redirect.
		    break;
		}

		Warning("_NET_WM_WINDOW_TYPE=%u of client %#x unsupported\n",
		    atoms[i], client->Window);

		/*
		   Debug(2, "%s: FIXME: not written %p\n", __FUNCTION__, client);

		   _NET_WM_WINDOW_TYPE_TOOLBAR
		   _NET_WM_WINDOW_TYPE_MENU
		   _NET_WM_WINDOW_TYPE_UTILITY
		   _NET_WM_WINDOW_TYPE_SPLASH
		   _NET_WM_WINDOW_TYPE_DIALOG
		   _NET_WM_WINDOW_TYPE_DROPDOWN_MENU
		   _NET_WM_WINDOW_TYPE_POPUP_MENU
		   _NET_WM_WINDOW_TYPE_TOOLTIP
		   _NET_WM_WINDOW_TYPE_NOTIFICATION
		   _NET_WM_WINDOW_TYPE_COMBO
		   _NET_WM_WINDOW_TYPE_DND
		 */
	    }
	}
	free(reply);
    }
}

/**
**	Process _NET_WM_WINDOW_OPACITY atom.
**
**	@param cookie	cookie of xcb_get_property_reply
**	@param client	client which get _NET_WINDOW_OPACITY.
**
**	_NET_WM_WINDOW_OPACITY(CARDINAL)
*/
static void HintGetNetWmWindowOpacity(xcb_get_property_cookie_t cookie,
    Client * client)
{
    uint32_t temp;

    if (AtomGetCardinal(cookie, &temp)) {
	client->Opacity = temp;
    }
}

/**
**	Get all hints needed to determine current window state.
**
**	@param client	client to fetch all ICCCM and EWMH hints
**
**	@todo split the request and reports
*/
void HintGetState(Client * client)
{
    uint32_t temp;
    xcb_get_property_cookie_t cookie1;
    xcb_get_property_cookie_t cookie2;
    xcb_get_property_cookie_t cookie3;
    xcb_get_property_cookie_t cookie4;
    xcb_get_property_cookie_t cookie5;

    // default values
    client->OnLayer = LAYER_NORMAL;
    client->Border = BORDER_DEFAULT;
    client->Desktop = DesktopCurrent;
    client->Opacity = UINT32_MAX;

    // WM_HINTS
    cookie1 = HintWMHintsRequest(client);

#ifdef USE_MOTIF_HINTS
    HintGetMotifHints(client);
#endif

    // _NET_WM_DESKTOP
    cookie2 = AtomCardinalRequest(client->Window, &Atoms.NET_WM_DESKTOP);
    // _NET_WM_WINDOW_TYPE
    cookie3 =
	xcb_get_property_unchecked(Connection, 0, client->Window,
	Atoms.NET_WM_WINDOW_TYPE.Atom, ATOM, 0, UINT32_MAX);
    // _NET_WM_STATE
    cookie4 =
	xcb_get_property_unchecked(Connection, 0, client->Window,
	Atoms.NET_WM_STATE.Atom, ATOM, 0, UINT32_MAX);
    // _NET_WM_WINDOW_OPACITY
    cookie5 =
	AtomCardinalRequest(client->Window, &Atoms.NET_WM_WINDOW_OPACITY);

    HintGetWMHints(cookie1, client);

    // HintGetNetWMDesktop(cookie2, client);
    if (AtomGetCardinal(cookie2, &temp)) {
	if (temp == UINT32_MAX) {
	    client->State |= WM_STATE_STICKY;
	} else if (temp < (unsigned)DesktopN) {
	    client->Desktop = temp;
	} else {
	    client->Desktop = DesktopN - 1;
	}
    }

    HintGetNetWmWindowType(cookie3, client);
    HintGetNetWmState(cookie4, client);
    HintGetNetWmWindowOpacity(cookie5, client);
}

/**
**	Get client protocols/hints.
**
**	This is called while client is being added to management.
**
**	@param client	new client to be managed
**
**	@todo split requests and replies.
*/
void HintGetClientProtocols(Client * client)
{
    Client *owner;

    // FIXME: split request, reply
    HintGetWMName(client);
    HintGetWMClass(client);
    HintGetWMNormal(HintRequestWMNormal(client), client);
    HintGetWMTransientFor(client);
    HintGetState(client);

    // disable resize, if min=max
    if (client->SizeHints.min_width == client->SizeHints.max_width
	&& client->SizeHints.min_height == client->SizeHints.max_height) {
	client->Border &= ~BORDER_RESIZE;
    }
    // set client to same layer as its owner
    if (client->Owner != XCB_NONE
	&& (owner = ClientFindByChild(client->Owner))) {
	client->OnLayer = owner->OnLayer;
    }
}

// ------------------------------------------------------------------------ //
// Setting hints
// ------------------------------------------------------------------------ //

/**
**	Maintain _NET_WORKAREA property of root window.
**
**	@todo The Window Manager calculate this space by taking the current
**	page minus space occupied by dock and panel windows, as indicated by
**	the _NET_WM_STRUT or _NET_WM_STRUT_PARTIAL properties set on client
**	windows.
*/
void HintSetNetWorkarea(void)
{
    int i;
    uint32_t values[DesktopN * 4];

    Debug(2, "%s: FIXME: only dummy function written\n", __FUNCTION__);

    // _NET_WORKAREA
    for (i = 0; i < DesktopN; i++) {
	values[i * 4 + 0] = 0;
	values[i * 4 + 1] = 0;
	values[i * 4 + 2] = XcbScreen->width_in_pixels;
	values[i * 4 + 3] = XcbScreen->height_in_pixels;
    }
    xcb_change_property(Connection, XCB_PROP_MODE_REPLACE, XcbScreen->root,
	Atoms.NET_WORKAREA.Atom, CARDINAL, 32, DesktopN * 4, values);
}

/**
**	Maintain _NET_CLIENT_LIST[_STACKING] properties of root window.
*/
void HintSetNetClientList(void)
{
    xcb_window_t *window;
    int count;
    Client *client;
    int layer;

    window = alloca(ClientN * sizeof(*window));

    // set _NET_CLIENT_LIST
    // has initial mapping order, starting with oldest window
    count = 0;
    SLIST_FOREACH(client, &ClientNetList, NetClient) {
	window[count++] = client->Window;
    }
    IfDebug(if (count != ClientN) {
	Debug(0, "lost windows\n");}
    ) ;
    xcb_change_property(Connection, XCB_PROP_MODE_REPLACE, XcbScreen->root,
	Atoms.NET_CLIENT_LIST.Atom, WINDOW, 32, count, window);

    // set _NET_CLIENT_LIST_STACKING
    // has bottom-to-top stacking order
    count = 0;
    for (layer = LAYER_TOP; layer >= LAYER_BOTTOM; --layer) {
	TAILQ_FOREACH(client, &ClientLayers[layer], LayerQueue) {
	    window[count++] = client->Window;
	}
    }
    IfDebug(if (count != ClientN) {
	Debug(0, "lost windows\n");}
    ) ;
    xcb_change_property(Connection, XCB_PROP_MODE_REPLACE, XcbScreen->root,
	Atoms.NET_CLIENT_LIST_STACKING.Atom, WINDOW, 32, count, window);
}

/**
**	Set WM_STATE icccm hint for client.
**
**	@param client	set property of client
*/
static void HintSetWMState(const Client * client)
{
    uint32_t values[2];

    if (client->State & WM_STATE_MAPPED) {
	values[0] = XCB_WM_STATE_NORMAL;
    } else if (client->State & WM_STATE_MINIMIZED) {
	values[0] = XCB_WM_STATE_ICONIC;
    } else {
	Debug(3, "set withdrawn %x\n", client->Window);
	values[0] = XCB_WM_STATE_WITHDRAWN;
	values[0] = XCB_WM_STATE_NORMAL;
    }
    values[1] = XCB_NONE;

    xcb_change_property(Connection, XCB_PROP_MODE_REPLACE, client->Window,
	Atoms.WM_STATE.Atom, Atoms.WM_STATE.Atom, 32, 2, values);
}

/**
**	Set _NET_WM_STATE ewmh hint for client.
**
**	@param client	set property of client
*/
static void HintSetNetWMState(const Client * client)
{
    uint32_t values[11];
    int i;

    i = 0;
    // _NET_WM_STATE_MODAL
    if (client->State & WM_STATE_STICKY) {
	values[i++] = Atoms.NET_WM_STATE_STICKY.Atom;
    }
    if (client->State & WM_STATE_MAXIMIZED_HORZ) {
	values[i++] = Atoms.NET_WM_STATE_MAXIMIZED_HORZ.Atom;
    }
    if (client->State & WM_STATE_MAXIMIZED_VERT) {
	values[i++] = Atoms.NET_WM_STATE_MAXIMIZED_VERT.Atom;
    }
    if (client->State & WM_STATE_SHADED) {
	values[i++] = Atoms.NET_WM_STATE_SHADED.Atom;
    }
    if (client->State & WM_STATE_NOLIST) {
	values[i++] = Atoms.NET_WM_STATE_SKIP_TASKBAR.Atom;
    }
    if (client->State & WM_STATE_NOPAGER) {
	values[i++] = Atoms.NET_WM_STATE_SKIP_PAGER.Atom;
    }
    if (client->State & WM_STATE_HIDDEN || client->State & WM_STATE_MINIMIZED) {
	values[i++] = Atoms.NET_WM_STATE_HIDDEN.Atom;
    }
    if (client->State & WM_STATE_FULLSCREEN) {
	values[i++] = Atoms.NET_WM_STATE_FULLSCREEN.Atom;
    }
    if (client->OnLayer == LAYER_ABOVE) {
	values[i++] = Atoms.NET_WM_STATE_ABOVE.Atom;
    }
    if (client->OnLayer == LAYER_BELOW) {
	values[i++] = Atoms.NET_WM_STATE_BELOW.Atom;
    }
    // _NET_WM_STATE_DEMANDS_ATTENTION

    xcb_change_property(Connection, XCB_PROP_MODE_REPLACE, client->Window,
	Atoms.NET_WM_STATE.Atom, ATOM, 32, i, &values);
}

/**
**	Set _NET_FRAME_EXTENTS ewmh hint for client.
**
**	@param client	set property of client
*/
static void HintSetNetFrameExtents(const Client * client)
{
    uint32_t values[4];
    int north;
    int south;
    int east;
    int west;

    BorderGetSize(client, &north, &south, &east, &west);

    // left, right, top, bottom
    values[0] = west;
    values[1] = east;
    values[2] = north;
    values[3] = south;

    xcb_change_property(Connection, XCB_PROP_MODE_REPLACE, client->Window,
	Atoms.NET_FRAME_EXTENTS.Atom, CARDINAL, 32, 4, &values);
}

/**
**	Set NET_WM_ALLOWED_ACTIONS emwh hint for client.
**
**	@param client	set property of client
*/
static void HintSetNetAllowed(const Client * client)
{
    uint32_t values[12];
    int i;

    i = 0;
    if (client->Border & BORDER_MOVE) {
	values[i++] = Atoms.NET_WM_ACTION_MOVE.Atom;
    }
    if (client->Border & BORDER_RESIZE) {
	values[i++] = Atoms.NET_WM_ACTION_RESIZE.Atom;
    }
    if (client->Border & BORDER_MINIMIZE) {
	values[i++] = Atoms.NET_WM_ACTION_MINIMIZE.Atom;
    }
    if (client->Border & BORDER_TITLE) {
	values[i++] = Atoms.NET_WM_ACTION_SHADE.Atom;
    }
    values[i++] = Atoms.NET_WM_ACTION_STICK.Atom;
    if (client->Border & BORDER_MAXIMIZE) {
	if (client->Border & BORDER_MAXIMIZE_HORZ) {
	    values[i++] = Atoms.NET_WM_ACTION_MAXIMIZE_HORZ.Atom;
	}
	if (client->Border & BORDER_MAXIMIZE_VERT) {
	    values[i++] = Atoms.NET_WM_ACTION_MAXIMIZE_VERT.Atom;
	}
    }
    values[i++] = Atoms.NET_WM_ACTION_FULLSCREEN.Atom;
    if (!(client->State & WM_STATE_STICKY)) {
	values[i++] = Atoms.NET_WM_ACTION_CHANGE_DESKTOP.Atom;
    }
    if (client->Border & BORDER_CLOSE) {
	values[i++] = Atoms.NET_WM_ACTION_CLOSE.Atom;
    }
    values[i++] = Atoms.NET_WM_ACTION_ABOVE.Atom;
    values[i++] = Atoms.NET_WM_ACTION_BELOW.Atom;

    xcb_change_property(Connection, XCB_PROP_MODE_REPLACE, client->Window,
	Atoms.NET_WM_ALLOWED_ACTIONS.Atom, ATOM, 32, i, &values);
}

/**
**	Set _NET_WM_DESKTOP ewmh hint for client.
**
**	@param client	set property of client
*/
void HintSetNetWMDesktop(const Client * client)
{
    if (client->State & WM_STATE_STICKY) {
	AtomSetCardinal(client->Window, &Atoms.NET_WM_DESKTOP, UINT32_MAX);
    } else {
	AtomSetCardinal(client->Window, &Atoms.NET_WM_DESKTOP,
	    client->Desktop);
    }
}

/**
**	Set window state hints for client.
**
**	@param client	set all window properties of client
*/
void HintSetAllStates(const Client * client)
{
    HintSetWMState(client);
    HintSetNetWMState(client);
    HintSetNetFrameExtents(client);
    HintSetNetAllowed(client);
    HintSetNetWMDesktop(client);

    // write opacity
    if (client->Opacity == UINT32_MAX) {
	xcb_delete_property(Connection, client->Parent,
	    Atoms.NET_WM_WINDOW_OPACITY.Atom);
    } else {
	AtomSetCardinal(client->Parent, &Atoms.NET_WM_WINDOW_OPACITY,
	    client->Opacity);
    }
}

// ------------------------------------------------------------------------ //
// Client message
// ------------------------------------------------------------------------ //

/**
**	Handle client message event _NET_MOVERESIZE_WINDOW.
**
**	@param client	client getting the event.
**	@param event	xcb client message event.
**
**	@todo FIXME: function not written
*/
void HintNetMoveResizeWindow(Client * client,
    const xcb_client_message_event_t * event)
{
    Debug(2, "FIXME: %s(%p,%p) not written\n", __FUNCTION__, client, event);
}

/**
**	Handle client message event _NET_WM_STATE.
**
**	@param client	client getting the event.
**	@param event	xcb client message event.
**
**	@todo FIXME: combine with #GetNetWmState.
*/
void HintNetWmState(Client * client, const xcb_client_message_event_t * event)
{
    int i;
    unsigned state;

    state = 0;
    // Get the state to change, upto two per event
    for (i = 1; i <= 2; i++) {
	if (event->data.data32[i] == Atoms.NET_WM_STATE_STICKY.Atom) {
	    state |= WM_STATE_STICKY;
	} else if (event->data.data32[i] ==
	    Atoms.NET_WM_STATE_MAXIMIZED_VERT.Atom) {
	    state |= WM_STATE_MAXIMIZED_VERT;
	} else if (event->data.data32[i] ==
	    Atoms.NET_WM_STATE_MAXIMIZED_HORZ.Atom) {
	    state |= WM_STATE_MAXIMIZED_HORZ;
	} else if (event->data.data32[i] == Atoms.NET_WM_STATE_SHADED.Atom) {
	    state |= WM_STATE_SHADED;
	} else if (event->data.data32[i] == Atoms.NET_WM_STATE_FULLSCREEN.Atom) {
	    state |= WM_STATE_FULLSCREEN;
	}
    }

    switch (event->data.data32[0]) {
	case _NET_WM_STATE_REMOVE:	// remove/unset property
	    if (state & WM_STATE_STICKY) {
		ClientSetSticky(client, 0);
	    }
	    if (state & (WM_STATE_MAXIMIZED_VERT | WM_STATE_MAXIMIZED_HORZ)) {
		// ignore if not maximized
		if (client->State & (WM_STATE_MAXIMIZED_HORZ |
			WM_STATE_MAXIMIZED_VERT)) {
		    ClientMaximize(client, 0, 0);
		}
	    }
	    if (state & WM_STATE_SHADED) {
		ClientUnshade(client);
	    }
	    if (state & WM_STATE_FULLSCREEN) {
		ClientSetFullscreen(client, 0);
	    }
	    break;
	case _NET_WM_STATE_ADD:	// add/set property
	    if (state & WM_STATE_STICKY) {
		ClientSetSticky(client, 1);
	    }
	    if (state & (WM_STATE_MAXIMIZED_VERT | WM_STATE_MAXIMIZED_HORZ)) {
		// ignore if already maximized
		if (!(client->
			State & (WM_STATE_MAXIMIZED_VERT |
			    WM_STATE_MAXIMIZED_HORZ))) {
		    ClientMaximize(client, state & WM_STATE_MAXIMIZED_HORZ,
			state & WM_STATE_MAXIMIZED_VERT);
		}
	    }
	    if (state & WM_STATE_SHADED) {
		ClientShade(client);
	    }
	    if (state & WM_STATE_FULLSCREEN) {
		ClientSetFullscreen(client, 1);
	    }
	    break;
	case _NET_WM_STATE_TOGGLE:	// toggle property
	    if (state & WM_STATE_STICKY) {
		if (client->State & WM_STATE_STICKY) {
		    ClientSetSticky(client, 0);
		} else {
		    ClientSetSticky(client, 1);
		}
	    }
	    if (state & (WM_STATE_MAXIMIZED_VERT | WM_STATE_MAXIMIZED_HORZ)) {
		// FIXME: does ClientMaximize toggle?
		ClientMaximize(client, state & WM_STATE_MAXIMIZED_HORZ,
		    state & WM_STATE_MAXIMIZED_VERT);
	    }
	    if (state & WM_STATE_SHADED) {
		if (client->State & WM_STATE_SHADED) {
		    ClientUnshade(client);
		} else {
		    ClientShade(client);
		}
	    }
	    if (state & WM_STATE_FULLSCREEN) {
		if (client->State & WM_STATE_FULLSCREEN) {
		    ClientSetFullscreen(client, 0);
		} else {
		    ClientSetFullscreen(client, 1);
		}
	    }
	    break;
	default:
	    Debug(2, "bad _NET_WM_STATE action: %u", event->data.data32[0]);
	    break;
    }
}

/// @}
