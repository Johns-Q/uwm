///
///	@file property.c	@brief x11 property handler functions
///
///	Copyright (c) 2009 - 2011, 2021 by Lutz Sammer.	 All Rights Reserved.
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
///< @{

#include <xcb/xcb.h>
#include "uwm.h"

#include <stdio.h>
#include <stdlib.h>

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

// ------------------------------------------------------------------------ //
// Property
// ------------------------------------------------------------------------ //

/**
**	Handle WM_NAME/_NET_WM_NAME hints property change.
**
**	@param state	property state
**	@param window	window which property was changed
**	@param atom	atom of changed property
**	@param property	get property request of changed property
**
**	@returns always true.
**
**	@todo property argument isn't used.
*/
static int HandlePropertyWMName(
    __attribute__((unused)) uint8_t state, xcb_window_t window,
    xcb_atom_t atom,
    __attribute__((unused)) xcb_get_property_reply_t * property)
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

#ifdef DEBUG

/**
**	Handle WM hints property change.
**
**	@param state	property state
**	@param window	window which property was changed
**	@param atom	atom of changed property
**	@param property	get property request of changed property
**
**	@returns always true.
*/
static int HandlePropertyWMHints(
    __attribute__((unused)) uint8_t state, xcb_window_t window,
    xcb_atom_t atom, xcb_get_property_reply_t * property)
{
    Client *client;

    Debug(3, "%s: atom #%x\n", __FUNCTION__, atom);

    if ((client = ClientFindByChild(window))) {
	xcb_icccm_wm_hints_t wm_hints;

	if (xcb_icccm_get_wm_hints_from_reply(&wm_hints, property)
	    && wm_hints.flags & XCB_ICCCM_WM_HINT_STATE) {
	    switch (wm_hints.initial_state) {
		case XCB_ICCCM_WM_STATE_WITHDRAWN:
		case XCB_ICCCM_WM_STATE_NORMAL:
		    Debug(3, "%s: client %x mapped\n", __FUNCTION__, window);
		    //FIXME: see map request
#if 0
		    xcb_map_window(Connection, client->Window);
		    if (!(client->State & WM_STATE_MAPPED)) {
			client->State |= WM_STATE_MAPPED;
			client->State &=
			    ~(WM_STATE_MINIMIZED | WM_STATE_SHOW_DESKTOP |
			    WM_STATE_HIDDEN | WM_STATE_SHADED);
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
		case XCB_ICCCM_WM_STATE_ICONIC:
		    Debug(3, "%s: client %x minimized\n", __FUNCTION__,
			window);
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
**	@param state	property state
**	@param window	window which property was changed
**	@param atom	atom of changed property
**	@param property	get property request of changed property
**
**	@returns always true.
*/
static int HandlePropertyWMNormalHints(
    __attribute__((unused)) uint8_t state,
    __attribute__((unused)) xcb_window_t window, xcb_atom_t atom,
    __attribute__((unused)) xcb_get_property_reply_t * property)
{
    Debug(3, "%s: atom %x\n", __FUNCTION__, atom);

    return 1;
}

#endif

#ifdef DEBUG

/**
**	Handle debug property change.
**
**	@param state	property state
**	@param window	window which property was changed
**	@param atom	atom of changed property
**	@param property	get property request of changed property
*/
static int HandlePropertyDebug(uint8_t state, xcb_window_t window,
    xcb_atom_t atom,
    __attribute__((unused)) xcb_get_property_reply_t * property)
{
    Atom *temp;
    const char *name;
    int n;
    xcb_get_atom_name_cookie_t cookie;
    xcb_get_atom_name_reply_t *reply;

    Debug(3, "%s: state %d window %x atom %x - ", __FUNCTION__, state, window,
	atom);
#if 0
    // removed from libxcb-util
    // predefined atom?
    if ((name = xcb_atom_get_name_predefined(atom))) {
	Debug(3, "'%s'\n", name);
	return 1;
    }
#endif
    // one of our atoms?
    for (temp = &Atoms.COMPOUND_TEXT; temp <= &Atoms.UWM_EXIT; ++temp) {
	if (temp->Atom == atom) {
	    Debug(3, "'%s'\n", temp->Name);
	    return 1;
	}
    }

    cookie = xcb_get_atom_name_unchecked(Connection, atom);
    reply = xcb_get_atom_name_reply(Connection, cookie, NULL);
    if (reply) {
	n = xcb_get_atom_name_name_length(reply);
	name = xcb_get_atom_name_name(reply);
	Debug(3, "atom '%.*s'\n", n, name);
	free(reply);
    } else {
	Debug(3, "atom #%d\n", atom);
    }

    return 1;
}

#endif

/**
**	Get property value.
**
**	@param state	property state
**	@param window	window which property was changed
**	@param atom	atom of changed property
**	@param len	supported length of value
**
**	@returns reply of get any property, NULL if property was deleted
*/
static xcb_get_property_reply_t *PropertyGet(uint8_t state,
    xcb_window_t window, xcb_atom_t atom, uint32_t len)
{
    xcb_get_property_cookie_t cookie;
    xcb_get_property_reply_t *reply;

    if (state != XCB_PROPERTY_DELETE) {
	cookie =
	    xcb_get_property_unchecked(Connection, 0, window, atom,
	    XCB_GET_PROPERTY_TYPE_ANY, 0U, len);
	reply = xcb_get_property_reply(Connection, cookie, NULL);

	return reply;
    }
    return NULL;
}

/**
**	Property change handler.
**
**	@param state	property state
**	@param window	window which property was changed
**	@param atom	atom of changed property
*/
void PropertyHandler(int state, xcb_window_t window, xcb_atom_t atom)
{
    xcb_get_property_reply_t *reply;

    Debug(4, "%s: %d window %x atom %x\n", __FUNCTION__, state, window, atom);

    reply = NULL;
    switch (atom) {			// handle predefined atoms
#ifdef DEBUG
	case XCB_ATOM_WM_HINTS:
	    reply = PropertyGet(state, window, atom, UINT32_MAX);
	    HandlePropertyWMHints(state, window, atom, reply);
	    break;
#endif
	case XCB_ATOM_WM_NAME:
	    reply = PropertyGet(state, window, atom, UINT32_MAX);
	    HandlePropertyWMName(state, window, atom, reply);
	    break;
#ifdef DEBUG
	case XCB_ATOM_WM_NORMAL_HINTS:
	    reply = PropertyGet(state, window, atom, UINT32_MAX);
	    HandlePropertyWMNormalHints(state, window, atom, reply);
	    break;
#endif
	default:			// handle own/ewmh atoms
	    if (atom == Atoms.NET_WM_NAME.Atom) {
		reply = PropertyGet(state, window, atom, UINT32_MAX);
		// FIXME: use NetWmName
		HandlePropertyWMName(state, window, atom, reply);
		break;
	    }
#ifdef DEBUG
	    HandlePropertyDebug(state, window, atom, NULL);
#endif
	    break;
    }
#if 0
    // FIXME:
    Atoms.WM_TRANSIENT_FOR.Atom,
	HandleProperty_wm_transient_for Atoms.WM_CLIENT_LEADER.Atom,
	HandleProperty_wm_client_leader Atoms.WM_ICON_NAME.Atom,
	HandleProperty_wm_icon_name
	// ATOM_WM_COLORMAP_WINDOWS
	Atoms._NET_WM_ICON_NAME.Atom,
	HandleProperty_wm_icon_name Atoms._NET_WM_STRUT_PARTIAL.Atom,
	HandleProperty_net_wm_strut_partial Atoms._NET_WM_ICON.Atom,
	HandleProperty_net_wm_icon
	// background change
	Atoms._XROOTPMAP_ID.Atom, 1, HandleProperty_xrootpmap_id
#endif
	free(reply);
}

/**
**	Initialize the property module.
**
**	@todo FIXME: must handle many more properties WM_TRANSIENT_FOR, ...
*/
void PropertyInit(void)
{
}

/**
**	Cleanup the property module.
*/
void PropertyExit(void)
{
}

/// @}
