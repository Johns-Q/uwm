///
///	@file property.c	@brief x11 property handler functions
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
///	@defgroup property The property module
///
///	This module handles property processing.
///
///	All property are collected here and distributed to the other moduls.
///
///	@todo FIXME: many properties are not yet watched and handled.
///
/// @{

#include <xcb/xcb.h>
#include "uwm.h"

#include <stdio.h>

#include <xcb/xcb_atom.h>
#include <xcb/xcb_icccm.h>

#include "queue.h"
#include "core-array/core-array.h"
#include "core-rc/core-rc.h"

#include "property.h"
#include "draw.h"
#include "image.h"
#include "client.h"
#include "border.h"
#include "hints.h"
#include "icon.h"
#include "menu.h"
#include "desktop.h"

#include "panel.h"
#include "plugin/task.h"

extern xcb_event_handlers_t EventHandlers;	///< xcb event handlers

// ------------------------------------------------------------------------ //
// Property
// ------------------------------------------------------------------------ //

    /// xcb property handlers
static xcb_property_handlers_t PropertyHandlers;

#ifdef DEBUG

/**
**	Handle default property change.
**
**	@param data	user data (unused).
**	@param conn	xcb connection
**	@param state	property state
**	@param window	window which property was changed
**	@param atom	atom of changed property
**	@param property	get property request of changed property
*/
static int HandlePropertyDefault( __attribute__ ((unused))
    void *data, __attribute__ ((unused)) xcb_connection_t * conn,
    __attribute__ ((unused)) uint8_t state,
    __attribute__ ((unused)) xcb_window_t window, xcb_atom_t atom,
    __attribute__ ((unused)) xcb_get_property_reply_t * property)
{
    Atom *temp;
    const char *name;
    int n;

    Debug(3, "%s: atom %x - ", __FUNCTION__, atom);

    // one of our atoms?
    for (temp = &Atoms.COMPOUND_TEXT; temp <= &Atoms.UWM_EXIT; ++temp) {
	if (temp->Atom == atom) {
	    Debug(3, "%s\n", temp->Name);
	    return 1;
	}
    }

    // xcb_get_atom_name_unchecked
    xcb_atom_get_name(Connection, atom, &name, &n);

    Debug(3, "%.*s\n", n, name);
    return 1;
}
#endif

/**
**	Handle WM hints property change.
**
**	@param data	user data (unused).
**	@param conn	xcb connection
**	@param state	property state
**	@param window	window which property was changed
**	@param atom	atom of changed property
**	@param property	get property request of changed property
**
**	@returns always true.
*/
static int HandlePropertyWMHints( __attribute__ ((unused))
    void *data, __attribute__ ((unused)) xcb_connection_t * conn,
    __attribute__ ((unused)) uint8_t state, xcb_window_t window,
    xcb_atom_t atom, xcb_get_property_reply_t * property)
{
    Client *client;

    Debug(3, "%s: atom %x\n", __FUNCTION__, atom);

    if ((client = ClientFindByChild(window))) {
	xcb_wm_hints_t wm_hints;

	if (xcb_get_wm_hints_from_reply(&wm_hints, property)
		&& wm_hints.flags & XCB_WM_HINT_STATE) {
	    switch (wm_hints.initial_state) {
		case XCB_WM_STATE_WITHDRAWN:
		case XCB_WM_STATE_NORMAL:
		    Debug(3, "%s: client %x mapped\n", __FUNCTION__, window);
		    //FIXME: see map request
#if 0
		    xcb_map_window(Connection, client->Window);
		    if (!(client->State & WM_STATE_MAPPED)) {
			client->State |= WM_STATE_MAPPED;
			client->State &= ~(WM_STATE_MINIMIZED
			    | WM_STATE_SHOW_DESKTOP | WM_STATE_HIDDEN
			    | WM_STATE_SHADED);
			if (!(client->State & WM_STATE_STICKY)) {
			    client->Desktop = DesktopCurrent;
			}
			xcb_map_window(Connection, client->Window);
			xcb_map_window(Connection, client->Parent);
			ClientRaise(client);
			ClientFocus(client);
			// Done by focus: TaskUpdate();
			// Done by focus: PagerUpdate();
		    }
		    ClientRestack();
#endif
		    break;
		case XCB_WM_STATE_ICONIC:
		    Debug(3, "%s: client %x minimized\n", __FUNCTION__, window);
		    //FIXME:
		    // client->State |= WM_STATE_MINIMIZED;
		    break;
	    }
	} else {			// defaults to mapped
	    Debug(3, "%s: client %x no state\n", __FUNCTION__, window);
	    //FIXME:
	}

	BorderDraw(client, NULL);
	TaskUpdate();
    }

    return 1;
}

/**
**	Handle normal hints property change.
**
**	@param data	user data (unused).
**	@param conn	xcb connection
**	@param state	property state
**	@param window	window which property was changed
**	@param atom	atom of changed property
**	@param property	get property request of changed property
**
**	@returns always true.
*/
static int HandlePropertyWMNormalHints( __attribute__ ((unused))
    void *data, __attribute__ ((unused)) xcb_connection_t * conn,
    __attribute__ ((unused)) uint8_t state,
    __attribute__ ((unused)) xcb_window_t window, xcb_atom_t atom,
    __attribute__ ((unused)) xcb_get_property_reply_t * property)
{
    Debug(3, "%s: atom %x\n", __FUNCTION__, atom);

    return 1;
}

/**
**	Handle WM_NAME/_NET_WM_NAME hints property change.
**
**	@param data	user data (unused).
**	@param conn	xcb connection
**	@param state	property state
**	@param window	window which property was changed
**	@param atom	atom of changed property
**	@param property	get property request of changed property
**
**	@returns always true.
**
**	@todo property argument isn't used.
*/
static int HandlePropertyWMName( __attribute__ ((unused))
    void *data, __attribute__ ((unused)) xcb_connection_t * conn,
    __attribute__ ((unused)) uint8_t state, xcb_window_t window,
    xcb_atom_t atom,
    __attribute__ ((unused)) xcb_get_property_reply_t * property)
{
    Client *client;

    Debug(3, "%s: atom %x %p\n", __FUNCTION__, atom, property);

    if ((client = ClientFindByChild(window))) {
	HintGetWMName(client);
	BorderDraw(client, NULL);
	TaskUpdate();
    }

    return 1;
}

/**
**	Initialize the property module.
**
**	@todo FIXME: must handle many more properties WM_TRANSIENT_FOR, ...
*/
void PropertyInit(void)
{
    xcb_property_handlers_init(&PropertyHandlers, &EventHandlers);

    // ICCCM atoms
    xcb_property_set_handler(&PropertyHandlers, WM_NAME, UINT32_MAX,
	HandlePropertyWMName, NULL);
    xcb_property_set_handler(&PropertyHandlers, WM_HINTS, UINT32_MAX,
	HandlePropertyWMHints, NULL);
    xcb_property_set_handler(&PropertyHandlers, WM_NORMAL_HINTS, UINT32_MAX,
	HandlePropertyWMNormalHints, NULL);
    // EWMH atoms
    xcb_property_set_handler(&PropertyHandlers, Atoms.NET_WM_NAME.Atom,
	UINT32_MAX, HandlePropertyWMName, NULL);
#if 0
    xcb_property_set_handler(&PropertyHandlers, Atoms.WM_TRANSIENT_FOR.Atom,
	UINT32_MAX, HandleProperty_wm_transient_for, NULL);
    xcb_property_set_handler(&PropertyHandlers, Atoms.WM_CLIENT_LEADER.Atom,
	UINT32_MAX, HandleProperty_wm_client_leader, NULL);
    xcb_property_set_handler(&PropertyHandlers, Atoms.WM_ICON_NAME.Atom,
	UINT32_MAX, HandleProperty_wm_icon_name, NULL);
    // ATOM_WM_COLORMAP_WINDOWS

    xcb_property_set_handler(&PropertyHandlers, Atoms._NET_WM_ICON_NAME.Atom,
	UINT32_MAX, HandleProperty_wm_icon_name, NULL);
    xcb_property_set_handler(&PropertyHandlers,
	Atoms._NET_WM_STRUT_PARTIAL.Atom, UINT32_MAX,
	HandleProperty_net_wm_strut_partial, NULL);
    xcb_property_set_handler(&PropertyHandlers, Atoms._NET_WM_ICON.Atom,
	UINT32_MAX, HandleProperty_net_wm_icon, NULL);

    // background change
    xcb_property_set_handler(&PropertyHandlers, Atoms._XROOTPMAP_ID.Atom, 1,
	HandleProperty_xrootpmap_id, NULL);
#endif
#ifdef DEBUG
    xcb_property_set_default_handler(&PropertyHandlers, UINT32_MAX,
	HandlePropertyDefault, NULL);
#endif
}

/**
**	Cleanup the property module.
*/
void PropertyExit(void)
{
    xcb_property_handlers_wipe(&PropertyHandlers);
}

/// @}
