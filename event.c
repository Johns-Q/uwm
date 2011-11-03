///
///	@file event.c		@brief x11 event handler functions
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
///	@defgroup event The event module
///
///	This module handles event processing.
///
///	All events are collected here and distributed to the other moduls.
///
///	@todo
///		FIXME: rewrite event handling, to support double click
///		and long click better.
///
/// @{

#include <xcb/xcb.h>
#include "uwm.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <poll.h>
#include <errno.h>
#include <string.h>

#include <xcb/xcb_event.h>
#include <xcb/xcb_atom.h>
#include <xcb/xcb_icccm.h>
#include <xcb/shape.h>

#include "queue.h"
#include "core-array/core-array.h"
#include "core-rc/core-rc.h"

#include "misc.h"
#include "event.h"
#include "property.h"
#include "client.h"
#include "moveresize.h"
#include "border.h"
#include "hints.h"

#include "draw.h"
#include "image.h"
#include "pointer.h"
#include "keyboard.h"
#include "tooltip.h"

#include "icon.h"
#include "menu.h"
#include "background.h"
#include "desktop.h"

#include "panel.h"
#include "plugin/pager.h"
#include "plugin/swallow.h"
#include "plugin/systray.h"
#include "plugin/task.h"

#include "dia.h"
#include "td.h"

//////////////////////////////////////////////////////////////////////////////

static xcb_generic_event_t *PushedEvent;	///< one event look ahead

    /// maximal movement to detect double click
int DoubleClickDelta;

    /// time to get second click to detect double click
int DoubleClickSpeed;

    /// last time of click
static xcb_timestamp_t DoubleClickLastTime;

    /// last x-coordinate of click
static int16_t DoubleClickLastX;

    /// last y-coordinate of click
static int16_t DoubleClickLastY;

    /// last button detail of click
static int8_t DoubleClickLastButton;

    /// flag double click active
static int8_t DoubleClickActive;

//////////////////////////////////////////////////////////////////////////////

/**
**	Handle key press event.
**
**	@param event	key press event
**
**	@returns true if event was handled, false otherwise.
*/
static inline int HandleKeyPress(const xcb_key_press_event_t * event)
{
    Debug(4, "key press state=%x, detail=%d child %x event %x\n", event->state,
	event->detail, event->child, event->event);

    KeyboardHandler(1, event);

    return 1;
}

/**
**	Handle key release event.
**
**	@param event	key release event
**
**	@returns true if event was handled, false otherwise.
*/
static inline int HandleKeyRelease(const xcb_key_release_event_t * event)
{
    Debug(4, "key release state=%x, detail=%d child %x event %x\n",
	event->state, event->detail, event->child, event->event);

    KeyboardHandler(0, (const xcb_key_press_event_t *)event);

    return 1;
}

/**
**	Handle button press event.
**
**	@param event	button press event
**
**	@returns true if event was handled, false otherwise.
**
**	@todo make button click on frame configurable
**	@todo rewrite the double-click handling with delay/timeout
*/
static inline int HandleButtonPress(xcb_button_press_event_t * event)
{
    Client *client;

    Debug(3, "button press state=%x, detail=%d child %x event %x\n",
	event->state, event->detail, event->child, event->event);
    PointerSetPosition(event->root_x, event->root_y);
    TooltipHide();

    //
    //	Double click detection
    //
    if (DoubleClickActive && event->detail == DoubleClickLastButton
	&& event->time >= DoubleClickLastTime
	&& event->time < DoubleClickLastTime + DoubleClickSpeed
	&& abs(event->root_x - DoubleClickLastX) <= DoubleClickDelta
	&& abs(event->root_y - DoubleClickLastY) <= DoubleClickDelta) {
	DoubleClickActive = 0;
	// trick17 add double click to state
	event->state |= (1 << 7) << event->detail;
	Debug(3, "double click of button %d %d ms\n", event->detail,
	    event->time - DoubleClickLastTime);
    } else {
	DoubleClickActive = 1;
	DoubleClickLastTime = event->time;
	DoubleClickLastX = event->root_x;
	DoubleClickLastY = event->root_y;
	DoubleClickLastButton = event->detail;
	// FIXME: must delay click!
    }

    //
    //	Frame
    //
    if ((client = ClientFindByFrame(event->event))) {
	Debug(3, "client frame click\n");
	// FIXME: auto-raise?
	ClientRaise(client);
	if (FocusModus == FOCUS_CLICK) {
	    ClientFocus(client);
	}
	// FIXME: make configurable
	switch (event->detail) {
	    case XCB_BUTTON_INDEX_1:
		BorderHandleButtonPress(client, event);
		break;
	    case XCB_BUTTON_INDEX_2:
		ClientMoveLoop(client, XCB_BUTTON_INDEX_2, event->event_x,
		    event->event_y);
		break;
	    case XCB_BUTTON_INDEX_3:
		BorderShowMenu(client, event->event_x, event->event_y);
		break;
	    case XCB_BUTTON_INDEX_4:
		// FIXME: check already shaded?
		ClientShade(client);
		// FIXME: Shade didn't update Pager!
		break;
	    case XCB_BUTTON_INDEX_5:
		// FIXME: check already unshaded?
		ClientUnshade(client);
		// FIXME: Shade didn't update Pager!
		break;
	}
	// not done by Shade/Unshade,...
	PagerUpdate();
	return 1;
    }
    //
    //	Root window
    //
    if (event->event == XcbScreen->root) {
	RootMenuHandleButtonPress(event);
	return 1;
    }
    if (DiaHandleButtonPress(event)) {
	return 1;
    }
#ifdef USE_TD
    if (TdHandleButtonPress(event)) {
	return 1;
    }
#endif
    //
    //	Client
    //
    if ((client = ClientFindByChild(event->event))) {
	int north;
	int south;
	int east;
	int west;

	Debug(3, "client window button press\n");
	if (DialogHandleButtonPress(event)) {
	    return 1;
	}
	switch (event->detail) {
	    case XCB_BUTTON_INDEX_1:
	    case XCB_BUTTON_INDEX_2:
		// FIXME: click raises?
		ClientRaise(client);
		if (FocusModus == FOCUS_CLICK) {
		    ClientFocus(client);
		}
		// wm standard ALT moves window
		// FIXME: don't ignore other MODS?
		if ((event->state & XCB_MOD_MASK_1)) {
		    BorderGetSize(client, &north, &south, &east, &west);

		    ClientMoveLoop(client, XCB_BUTTON_INDEX_2,
			event->event_x + west, event->event_y + north);
		}
		break;
	    case XCB_BUTTON_INDEX_3:
		// wm standard ALT resizes window
		// FIXME: don't ignore other MODS?
		Debug(3, "resize on alt %x\n", event->state);
		if (event->state & XCB_MOD_MASK_1) {
		    Debug(3, "resize loop %x\n", event->state);
		    BorderGetSize(client, &north, &south, &east, &west);
		    ClientResizeLoop(client, XCB_BUTTON_INDEX_3,
			BORDER_ACTION_RESIZE | BORDER_ACTION_RESIZE_E |
			BORDER_ACTION_RESIZE_S, event->event_x + west,
			event->event_y + north);
		} else {
		    ClientRaise(client);
		    if (FocusModus == FOCUS_CLICK) {
			ClientFocus(client);
		    }
		}
	    case XCB_BUTTON_INDEX_4:
	    case XCB_BUTTON_INDEX_5:
		break;
	}
	// FIXME: when handled don't replay!
	// send grabed event to client
	xcb_allow_events(Connection, XCB_ALLOW_REPLAY_POINTER,
	    XCB_CURRENT_TIME);
	// FIXME: should be done by functions PagerUpdate();
	return 1;
    }

    if (PanelHandleButtonPress(event)) {
	// FIXME: better let function handle calling update
	// FIXME: should be done by functions PagerUpdate();
	return 1;
    }
    return 0;
}

/**
**	Handle release button event.
**
**	@param event	button release event
**
**	@returns true if event was handled, false otherwise.
*/
static inline int HandleButtonRelease(const xcb_button_release_event_t * event)
{
    Debug(3, "button release detail = %d state = %d\n", event->detail,
	event->state);

    PointerSetPosition(event->root_x, event->root_y);

    if (DialogHandleButtonRelease(event)) {
	return 1;
    }
    if (DiaHandleButtonRelease(event)) {
	return 1;
    }
#ifdef USE_TD
    if (TdHandleButtonRelease(event)) {
	return 1;
    }
#endif
    if (PanelHandleButtonRelease(event)) {
	return 1;
    }
    return 0;
}

/**
**	Handle motion notify.
**
**	@param event	X11 motion notify event
**
**	@returns true if event was handled, false otherwise.
*/
static int HandleMotionNotify(const xcb_motion_notify_event_t * event)
{
    Client *client;

    Debug(4, "mouse moved %d, %d\n", event->root_x, event->root_y);
    // FIXME: can discard multiple motion notifies here

    PointerSetPosition(event->root_x, event->root_y);

    if ((client = ClientFindByFrame(event->event))) {
	if (client->Border & BORDER_OUTLINE) {
	    BorderAction action;
	    xcb_cursor_t cursor;

	    action = BorderGetAction(client, event->event_x, event->event_y);
	    cursor = BorderGetCursor(action);

	    // FIXME: can check if cursor remains same and reduce x11 calls
	    xcb_change_window_attributes(Connection, event->event,
		XCB_CW_CURSOR, &cursor);
	}
	return 1;
    }

    if (PanelHandleMotionNotify(event)) {
	return 1;
    }
    if (DiaHandleMotionNotify(event)) {
	return 1;
    }
#ifdef USE_TD
    if (TdHandleMotionNotify(event)) {
	return 1;
    }
#endif

    return 0;
}

/**
**	Handle enter notify.
**
**	@param event	enter notify event
**
**	@returns true if event was handled, false otherwise.
*/
static inline int HandleEnterNotify(const xcb_enter_notify_event_t * event)
{
    Client *client;

    Debug(3, "enter notify - event %x child %x\n", event->event, event->child);

    PointerSetPosition(event->root_x, event->root_y);

    if ((client = ClientFindByAny(event->event))) {
	if (!(client->State & WM_STATE_ACTIVE)
	    && (FocusModus == FOCUS_SLOPPY)) {
	    ClientFocus(client);
	}
	if (client->Parent == event->event) {	// frame entered
	    BorderAction action;
	    xcb_cursor_t cursor;

	    action = BorderGetAction(client, event->event_x, event->event_y);
	    cursor = BorderGetCursor(action);
	    xcb_change_window_attributes(Connection, event->event,
		XCB_CW_CURSOR, &cursor);
	} else {
	    // FIXME: too many X11 calls? can check if really need to reset
	    PointerSetDefaultCursor(client->Parent);
	}
	return 1;
    }
    if (PanelHandleEnterNotify(event)) {
	return 1;
    }
    return 0;
}

/**
**	Handle an expose.
**
**	@param event	X11 expose event
**
**	@returns true if event was handled, false otherwise.
*/
static inline int HandleExpose(const xcb_expose_event_t * event)
{
    Client *client;

    Debug(4, "expose - window %d\n", event->window);
    if ((client = ClientFindByAny(event->window))) {
	if (event->window == client->Parent) {
	    BorderDraw(client, event);
	    return 1;
	}
	// dialog window only if last expose
	if (event->window == client->Window
	    && (client->State & WM_STATE_WMDIALOG) && !event->count) {
	    return DialogHandleExpose(event);
	}
	// ignore other expose events
	return 1;
    }
    Debug(3, "expose - counter %d\n", event->count);
    if (event->count) {			// ignore this until last
	return 1;
    }

    if (PanelHandleExpose(event)) {
	return 1;
    }
    if (TooltipHandleExpose(event)) {
	return 1;
    }
    if (DiaHandleExpose(event)) {
	return 1;
    }
#ifdef USE_TD
    if (TdHandleExpose(event)) {
	return 1;
    }
#endif
    return 0;
}

/**
**	Handle destroy notify.
**
**	@param event	destroy notify event
**
**	@returns true if event was handled, false otherwise.
*/
static inline int HandleDestroyNotify(const xcb_destroy_notify_event_t * event)
{
    Client *client;

    Debug(3, "destroy notify - window %x\n", event->window);

    if ((client = ClientFindByChild(event->window))) {
	Debug(3, "destroy client %s\n", client->Name);
	if (client == ClientControlled) {
	    ClientController();		// stop, if move/resize
	}
	ClientDelWindow(client);
	return 1;
    } else if (SwallowHandleDestroyNotify(event)) {
	return 1;
    }
    return SystrayHandleDestroyNotify(event->window);
}

/**
**	Handle unmap notify.
**
**	@param event	unmap notify event
**
**	@returns true if event was handled, false otherwise.
*/
static inline int HandleUnmapNotify(const xcb_unmap_notify_event_t * event)
{
    Client *client;

    Debug(3, "unmap notify - event %x window %x %s\n", event->event,
	event->window, event->from_configure ? "(from configure)" : "");

    if ((client = ClientFindByChild(event->window))) {
	xcb_generic_event_t *peek;

	if ((peek = PeekWindowEvent(client->Window, XCB_DESTROY_NOTIFY))) {
	    Debug(3, "peek destory notify\n");
	    HandleDestroyNotify((xcb_destroy_notify_event_t *) peek);
	    free(peek);
	    return 1;
	}
	// ignore this unmap, produces only problems!!!!
	if (!XCB_EVENT_SENT(event)) {
	    return 1;
	}

	if (client == ClientControlled) {
	    ClientController();		// stop, if move/resize
	}

	if (client->State & WM_STATE_MAPPED) {
	    client->State &= ~WM_STATE_MAPPED;
	    xcb_unmap_window(Connection, client->Parent);

	    // FIXME: need only to set what has changed!
	    HintSetAllStates(client);
	    TaskUpdate();
	    PagerUpdate();
	}
    }

    return 1;
}

/**
**	Handle map request.
**
**	@param event	map request event
**
**	@returns true if event was handled, false otherwise.
*/
static inline int HandleMapRequest(const xcb_map_request_event_t * event)
{
    Client *client;

    Debug(3, "map request: window %x\n", event->window);

    // FIXME: swallow gets name, class which is done below again!
    if (SwallowHandleMapRequest(event)) {	// window to shallow
	return 1;
    }

    if ((client = ClientFindByAny(event->window))) {
	// known client
	if (!(client->State & WM_STATE_MAPPED)) {
	    client->State |= WM_STATE_MAPPED;
	    client->State &=
		~(WM_STATE_MINIMIZED | WM_STATE_SHOW_DESKTOP | WM_STATE_HIDDEN
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
    } else {
	xcb_get_window_attributes_cookie_t cookie;

	// FIXME: grab server? needed if interactive placement
	cookie =
	    xcb_get_window_attributes_unchecked(Connection, event->window);
	client = ClientAddWindow(event->window, cookie, 0, 1);
	if (client) {
	    if (FocusModus == FOCUS_CLICK) {
		ClientFocus(client);
	    }
	    // Done by restack: PagerUpdate();
	} else {			// not managed just map it
	    xcb_map_window(Connection, event->window);
	}
    }
    ClientRestack();
    return 1;
}

/**
**	Handle reparent notify.
**
**	@param event	reparent notify event
**
**	@returns true if event was handled, false otherwise.
*/
static inline int HandleReparentNotify(const xcb_reparent_notify_event_t *
    event)
{
    Debug(3, "reparent notify - window %x\n", event->event);
    SystrayHandleReparentNotify(event);

    return 1;
}

/**
**	Handle configure notify.
**
**	@param event	configure notify event
**
**	@returns true if event was handled, false otherwise.
*/
static inline int HandleConfigureNotify(const xcb_configure_notify_event_t *
    event)
{
    Debug(4, "configure notify - window %x\n", event->event);

    // FIXME: check if root window is reconfigured, need to redesign panels.

    if (SwallowHandleConfigureNotify(event)) {
	return 1;
    }

    return 0;
}

/**
**	Handle configure request.
**
**	@param event	configure request event
**
**	@returns true if event was handled, false otherwise.
*/
static inline int HandleConfigureRequest(const xcb_configure_request_event_t *
    event)
{
    Client *client;

    Debug(3, "configure request - window %x\n", event->window);

    // FIXME: check if root window is reconfigured, need to redesign panels.

    if (SystrayHandleConfigureRequest(event)) {
	return 1;
    }

    if ((client = ClientFindByChild(event->window))) {
	int north;
	int south;
	int east;
	int west;
	int changed;
	uint32_t values[5];

	// we own this window, make sure it's not trying to do something bad
	Debug(3, "%s: own window\n", __FUNCTION__);

	changed = 0;
	if ((event->value_mask & XCB_CONFIG_WINDOW_X)
	    && client->X != event->x) {
	    client->X = event->x;
	    changed = 1;
	}
	if ((event->value_mask & XCB_CONFIG_WINDOW_Y)
	    && client->Y != event->y) {
	    client->Y = event->y;
	    changed = 1;
	}
	if ((event->value_mask & XCB_CONFIG_WINDOW_WIDTH)
	    && client->Width != event->width) {
	    client->Width = event->width;
	    changed = 1;
	}
	if ((event->value_mask & XCB_CONFIG_WINDOW_HEIGHT)
	    && client->Height != event->height) {
	    client->Height = event->height;
	    changed = 1;
	}
	Debug(3, "changed %d,%d %dx%d\n", client->X, client->Y, client->Width,
	    client->Height);
	// border, sibling, stacking are ignored
	if (!changed) {			// nothing changed
	    return 1;
	}
	if (client == ClientControlled) {
	    ClientController();		// stop, if move/resize
	}

	ClientConstrainSize(client);
	client->State &= ~(WM_STATE_MAXIMIZED_HORZ | WM_STATE_MAXIMIZED_VERT);
	if (client->State & WM_STATE_SHADED) {
	    // FIXME: shaded? fullscreen
	    Debug(2, "loose shading?\n");
	}

	ClientUpdateShape(client);

	BorderGetSize(client, &north, &south, &east, &west);

	values[0] = client->X;
	values[1] = client->Y;
	values[2] = client->Width + east + west;
	values[3] = client->Height + north + south;
	xcb_configure_window(Connection, client->Parent,
	    XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH
	    | XCB_CONFIG_WINDOW_HEIGHT, values);
	values[0] = west;
	values[1] = north;
	values[2] = client->Width;
	values[3] = client->Height;
	xcb_configure_window(Connection, client->Window,
	    XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH
	    | XCB_CONFIG_WINDOW_HEIGHT, values);
    } else {
	uint32_t values[7];
	int i;

	// we don't know about this window, just let configure through

	i = 0;
	if (event->value_mask & XCB_CONFIG_WINDOW_X) {
	    values[i++] = event->x;
	}
	if (event->value_mask & XCB_CONFIG_WINDOW_Y) {
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

	xcb_configure_window(Connection, event->window, event->value_mask,
	    values);
    }

    return 0;
}

/**
**	Handle resize request.
**
**	@param event	resize request event
**
**	@returns true if event was handled, false otherwise.
*/
static inline int HandleResizeRequest(const xcb_resize_request_event_t * event)
{
    Debug(3, "resize request - window %x\n", event->window);

    if (SystrayHandleResizeRequest(event)) {
	return 1;
    }

    return 0;
}

/**
**	Handle property notify.
**
**	@param event	selection clear event
**
**	@returns true if event was handled, false otherwise.
*/
static inline int HandlePropertyNotify(const xcb_property_notify_event_t *
    event)
{
    PropertyHandler(event->state, event->window, event->atom);

    return 1;
}

/**
**	Handle selection clear.
**
**	@param event	selection clear event
**
**	@returns true if event was handled, false otherwise.
*/
static inline int HandleSelectionClear(const xcb_selection_clear_event_t *
    event)
{
    Debug(3, "selection clear - window %x\n", event->owner);

    return SystrayHandleSelectionClear(event);
}

#ifdef DEBUG

/**
**	Debug atom name.
**
**	@param atom	client messsage type atom
*/
static void DebugAtomName(uint32_t atom)
{
    xcb_get_atom_name_cookie_t cookie;
    xcb_get_atom_name_reply_t *reply;
    const char *name;
    int n;

    cookie = xcb_get_atom_name_unchecked(Connection, atom);
    reply = xcb_get_atom_name_reply(Connection, cookie, NULL);
    if (reply) {
	n = xcb_get_atom_name_name_length(reply);
	name = xcb_get_atom_name_name(reply);
	Debug(2, "unsupported client message atom '%.*s'\n", n, name);
	free(reply);
    } else {
	Debug(2, "unsupported client message atom #%d\n", atom);
    }
}
#endif

/**
**	Handle client message.
**
**	@param event	client message event
**
**	@returns true if event was handled, false otherwise.
*/
static inline int HandleClientMessage(const xcb_client_message_event_t * event)
{
    Client *client;

    Debug(3, "client message - window %x\n", event->window);

    // message for our root window
    if (event->window == XcbScreen->root) {
	if (event->type == Atoms.UWM_RESTART.Atom) {
	    KeepRunning = 1;
	    KeepLooping = 0;
	} else if (event->type == Atoms.UWM_EXIT.Atom) {
	    KeepLooping = 0;
	} else if (event->type == Atoms.NET_CURRENT_DESKTOP.Atom) {
	    DesktopChange(event->data.data32[0]);
	} else {
#ifdef DEBUG
	    DebugAtomName(event->type);
#endif
	}
	return 1;
    }
    // message for our client window
    if ((client = ClientFindByAny(event->window))) {
	if (event->type == Atoms.WM_CHANGE_STATE.Atom) {
	    if (client == ClientControlled) {
		ClientController();	// stop, if move/resize
	    }

	    switch (event->data.data32[0]) {
		case XCB_ICCCM_WM_STATE_WITHDRAWN:
		    ClientSetWithdrawn(client);
		    break;
		case XCB_ICCCM_WM_STATE_NORMAL:
		    ClientRestore(client, 1);
		    break;
		case XCB_ICCCM_WM_STATE_ICONIC:
		    ClientMinimize(client);
		    break;
		default:
		    break;
	    }
	} else if (event->type == Atoms.NET_ACTIVE_WINDOW.Atom) {
	    ClientRestore(client, 1);
	    ClientFocus(client);
	} else if (event->type == Atoms.NET_WM_DESKTOP.Atom) {
	    if (event->data.data32[0] == (uint32_t) ~ 0L) {
		ClientSetSticky(client, 1);
	    } else {
		if (client == ClientControlled) {
		    ClientController();	// stop, if move/resize
		}
		if (event->data.data32[0] < (uint32_t) DesktopN) {
		    ClientSetSticky(client, 0);
		    ClientSetDesktop(client, event->data.data32[0]);
		}
	    }
	} else if (event->type == Atoms.NET_CLOSE_WINDOW.Atom) {
	    ClientDelete(client);
	} else if (event->type == Atoms.NET_MOVERESIZE_WINDOW.Atom) {
	    HintNetMoveResizeWindow(client, event);
	} else if (event->type == Atoms.NET_WM_STATE.Atom) {
	    HintNetWmState(client, event);
	} else {
#ifdef DEBUG
	    DebugAtomName(event->type);
#endif
	}
	return 1;
    }

    if (event->type == Atoms.NET_SYSTEM_TRAY_OPCODE.Atom) {
	SystrayHandleClientMessageEvent(event);
	return 1;
    }

    return 0;
}

#ifdef USE_SHAPE			// {

/**
**	Handle shape notify.
**
**	@param event	shape notify event
**
**	@returns true if event was handled, false otherwise.
*/
static inline int HandleShapeNotify(const xcb_shape_notify_event_t * event)
{
    Client *client;

    Debug(3, "shape notify - window %x\n", event->affected_window);
    if ((client = ClientFindByAny(event->affected_window))) {
	// FIXME: shape on/off?
	ClientUpdateShape(client);
    }
    return 1;
}
#endif // } USE_SHAPE

#ifdef DEBUG

/**
**	Debug print event
**
**	@param event	generic event
*/
static inline int HandleDebugEvent(const xcb_generic_event_t * event)
{
    int send_event;
    int type;

    send_event = XCB_EVENT_SENT(event);
    if ((type = XCB_EVENT_RESPONSE_TYPE(event))) {
	Debug(3, "Event %s following sequence number %d%s\n",
	    xcb_event_get_label(type), event->sequence,
	    send_event ? " (from sent_event)" : "");
    } else {
	Debug(2, "Error %s on sequence number %d (%s)\n",
	    xcb_event_get_error_label(((xcb_request_error_t *)
		    event)->error_code), event->sequence,
	    xcb_event_get_request_label(((xcb_request_error_t *)
		    event)->major_opcode));
    }
    return 1;
}
#else
///	Dunmy debug print event
#define HandleDebugEvent(x)		/* x */
#endif

//////////////////////////////////////////////////////////////////////////////

/**
**	Handle timeout
**
**	Called before waiting on events and then periodic.
*/
static void HandleTimeout(void)
{
    uint32_t tick;
    static uint32_t last_tick;
    int x;
    int y;

    tick = GetMsTicks();
    // handle double-click timeout

    // only every 50ms
    if (last_tick <= tick && tick < last_tick + 50) {
	return;
    }
    last_tick = tick;

    PointerGetPosition(&x, &y);
    PanelTimeout(tick, x, y);
    TooltipTimeout(tick, x, y);
    DiaTimeout(tick, x, y);
#ifdef USE_TD
    TdTimeout(tick, x, y);
#endif
}

/**
**	Wait for event.
*/
void WaitForEvent(void)
{
    struct pollfd fds[1];
    int n;

    if (PushedEvent) {			// pushed event?
	return;
    }
    HandleTimeout();

    fds[0].fd = xcb_get_file_descriptor(Connection);
    fds[0].events = POLLIN | POLLPRI;

    while (KeepLooping) {
	// flush before blocking (and waiting for new events)
	xcb_flush(Connection);
#ifdef DEBUG
	fflush(NULL);
#endif
	// FIXME: need to configure the timeout base
	n = poll(fds, 1, 50);
	if (n < 0) {
	    Error("error poll %s\n", strerror(errno));
	    return;
	}
	if (xcb_connection_has_error(Connection)) {
	    Debug(2, "!!!! closed %d=%x\n", n, fds[0].revents);
	    KeepLooping = 0;
	    break;
	}
	if (n) {
	    if (fds[0].revents & POLLPRI) {
		Debug(2, "%d: error\n", fds[0].fd);
	    }
	    if (fds[0].revents & POLLIN) {
		break;
	    }
	}
	HandleTimeout();
    }
}

/**
**	Poll next event.
**
**	Look for next event.
**
**	@returns any pushed event or new event or NULL if none available.
*/
xcb_generic_event_t *PollNextEvent(void)
{
    xcb_generic_event_t *event;

    if ((event = PushedEvent)) {
	PushedEvent = NULL;
	return event;
    }
    return xcb_poll_for_event(Connection);
}

#if 0

/**
**	Look if there is an event available.
**
**	@todo FIXME: remove
*/
int IsNextEventAvail(void)
{
    if (PushedEvent) {
	return 1;
    }
    PushedEvent = xcb_poll_for_event(Connection);

    return PushedEvent != NULL;
}
#endif

/**
**	Peek window event.
**
**	@param window	window must match
**	@param type	type must match
**
**	@returns next event, if window and event type matches.
*/
xcb_generic_event_t *PeekWindowEvent(xcb_window_t window, int type)
{
    xcb_generic_event_t *event;

    if ((event = PollNextEvent())) {
	if (XCB_EVENT_RESPONSE_TYPE(event) == type && event->pad[1] == window) {
	    return event;
	}
	PushedEvent = event;
	return NULL;

    }
    return event;
}

/**
**	Discard further motion events on same window
**
**	@param[in,out] event	old event overwritten by last
**	@param window		update motion event for this window
*/
void DiscardMotionEvents(xcb_motion_notify_event_t ** event,
    xcb_window_t window)
{
    xcb_motion_notify_event_t *peek;

    if ((peek = *event)) {
	PointerSetPosition(peek->root_x, peek->root_y);
    }
    while ((peek = (xcb_motion_notify_event_t *) PollNextEvent())) {
	if (XCB_EVENT_RESPONSE_TYPE(peek) != XCB_MOTION_NOTIFY) {
	    PushedEvent = (xcb_generic_event_t *) peek;
	    break;
	}
	PointerSetPosition(peek->root_x, peek->root_y);
	Debug(4, "discarding motion event\n");
	if (peek->event == window) {
	    free(*event);
	    *event = peek;
	} else {
	    free(peek);
	}
    }
}

/**
**	Handle a global single event.
**
**	@param event	generic event
*/
void EventHandleEvent(xcb_generic_event_t * event)
{
    switch (XCB_EVENT_RESPONSE_TYPE(event)) {
	case 0:			// error code
	    HandleDebugEvent(event);
	    break;
	case XCB_KEY_PRESS:		// keyboard press
	    HandleKeyPress((xcb_key_press_event_t *) event);
	    break;
	case XCB_KEY_RELEASE:		// keyboard release
	    HandleKeyRelease((xcb_key_press_event_t *) event);
	    break;
	case XCB_BUTTON_PRESS:		// pointer button press
	    HandleButtonPress((xcb_button_press_event_t *) event);
	    break;
	case XCB_BUTTON_RELEASE:	// pointer button release
	    HandleButtonRelease((xcb_button_press_event_t *) event);
	    break;
	case XCB_MOTION_NOTIFY:	// pointer motion notify
	    HandleMotionNotify((xcb_motion_notify_event_t *) event);
	    break;
	case XCB_ENTER_NOTIFY:		// pointer enter notify
	    HandleEnterNotify((xcb_enter_notify_event_t *) event);
	    break;
	case XCB_EXPOSE:		// window redraw
	    HandleExpose((xcb_expose_event_t *) event);
	    break;
	case XCB_DESTROY_NOTIFY:	// window destroy notify
	    HandleDestroyNotify((xcb_destroy_notify_event_t *) event);
	    break;
	case XCB_UNMAP_NOTIFY:		// window unmap notify
	    HandleUnmapNotify((xcb_unmap_notify_event_t *) event);
	    break;
	case XCB_MAP_REQUEST:		// window map request
	    HandleMapRequest((xcb_map_request_event_t *) event);
	    break;
	case XCB_REPARENT_NOTIFY:	// window reparent notify
	    HandleReparentNotify((xcb_reparent_notify_event_t *) event);
	    break;
	case XCB_CONFIGURE_NOTIFY:	// window configure notify
	    HandleConfigureNotify((xcb_configure_notify_event_t *) event);
	    break;
	case XCB_CONFIGURE_REQUEST:	// window configure request
	    HandleConfigureRequest((xcb_configure_request_event_t *) event);
	    break;
	case XCB_RESIZE_REQUEST:	// window resize request
	    HandleResizeRequest((xcb_resize_request_event_t *) event);
	    break;
	case XCB_PROPERTY_NOTIFY:	// property change notify
	    HandlePropertyNotify((xcb_property_notify_event_t *) event);
	    break;
	case XCB_SELECTION_CLEAR:	// selection clear
	    HandleSelectionClear((xcb_selection_clear_event_t *) event);
	    break;
	case XCB_CLIENT_MESSAGE:	// client message
	    HandleClientMessage((xcb_client_message_event_t *) event);
	    break;
	default:
#ifdef USE_SHAPE
	    if (XCB_EVENT_RESPONSE_TYPE(event)
		== ShapeEvent + XCB_SHAPE_NOTIFY) {
		HandleShapeNotify((xcb_shape_notify_event_t *) event);
		break;
	    }
#endif
	    HandleDebugEvent(event);
	    break;
    }
}

/**
**	Main event loop.
*/
void EventLoop(void)
{
    xcb_generic_event_t *event;

    // process all events, while not exit requested
    do {
	while ((event = PollNextEvent())) {
	    EventHandleEvent(event);
	    free(event);
	}
	WaitForEvent();
    } while (KeepLooping);

    // give windows (swallow windows especially) time to map
    usleep(RESTART_DELAY);

    // process events and ignore them, one last time
    while ((event = PollNextEvent())) {
	free(event);
    }
    Debug(3, "end of event loop\n");
}

/**
**	Initialize the event module.
*/
void EventInit(void)
{
}

/// @}
