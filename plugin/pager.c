///
///	@file pager.c	@brief pager panel plugin functions.
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
///	@defgroup pager_plugin	The pager panel plugin
///
///	This module adds pager to the panel.
///
///	@todo sticky flag didn't work
///
/// @{

#define _GNU_SOURCE	1		///< fix stpcpy strchrnul

#include <xcb/xcb.h>
#include "uwm.h"

#ifdef USE_PAGER			// {

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/queue.h>

#include <xcb/xcb_icccm.h>
#include <xcb/xcb_event.h>

#include "core-array/core-array.h"
#include "core-rc/core-rc.h"

#include "event.h"
#include "draw.h"
#include "tooltip.h"
#include "pointer.h"
#include "image.h"
#include "client.h"
#include "moveresize.h"
#include "border.h"

#include "icon.h"
#include "menu.h"
#include "desktop.h"
#include "panel.h"
#include "plugin/pager.h"

extern xcb_event_handlers_t EventHandlers;	///< xcb event handlers

/**
**	Pager plugin typedef.
*/
typedef struct _pager_plugin_ PagerPlugin;

/**
**	Structure to represent a pager panel plugin.
*/
struct _pager_plugin_
{
    SLIST_ENTRY(_pager_plugin_) Next;	///< singly-linked pager list
    Plugin *Plugin;			///< common panel plugin data

    PanelLayout Layout:2;		///< layout of the desktops
    unsigned Labeled:1;			///< set to label the pager
    unsigned Sticky:1;			///< sticky windows are also on pager

    uint16_t DeskWidth;			///< width of a single desktop
    uint16_t DeskHeight;		///< height of a single desktop

    int32_t ScaleX;			///< horizontal scale factor
    int32_t ScaleY;			///< vertical scale factor
};

    /// Pager plugin list head structure
SLIST_HEAD(_pager_head_, _pager_plugin_);

    /// Linked list of all pager plugins
static struct _pager_head_ Pagers = SLIST_HEAD_INITIALIZER(Pagers);

/**
**	Get desktop for a pager given a set of coordinates.
**
**	@param pager_plugin	pager plugin to use
**	@param x		x-coordinate relative to plugin
**	@param y		y-coordinate relative to plugin
**
**	@returns desktop under @a x and @a y.
*/
static int PagerGetDesktop(PagerPlugin * pager_plugin, int x, int y)
{
    if (pager_plugin->Layout == PANEL_LAYOUT_HORIZONTAL) {
	return x / (pager_plugin->DeskWidth + 1);
    } else {
	return y / (pager_plugin->DeskHeight + 1);
    }
}

/**
**	Get client for a pager given a set of coordinates.
**
**	@param pager_plugin	pager plugin to use
**	@param x		x-coordinate relative to plugin
**	@param y		y-coordinate relative to plugin
**
**	@returns client which contains point @a x @a y.
*/
static Client *PagerGetClient(PagerPlugin * pager_plugin, int x, int y)
{
    int desktop;
    int layer;

    // determine the selected desktop
    desktop = PagerGetDesktop(pager_plugin, x, y);
    if (pager_plugin->Layout == PANEL_LAYOUT_HORIZONTAL) {
	x -= (pager_plugin->DeskWidth + 1) * desktop;
    } else {
	y -= (pager_plugin->DeskHeight + 1) * desktop;
    }

    // find client under the specified coordinates
    for (layer = LAYER_TOP; layer >= LAYER_BOTTOM; layer--) {
	Client *client;

	TAILQ_FOREACH(client, &ClientLayers[layer], LayerQueue) {
	    int mini_x;
	    int mini_y;
	    unsigned mini_w;
	    unsigned mini_h;

	    // skip this client if it isn't mapped
	    if (!(client->State & WM_STATE_MAPPED)) {
		continue;
	    }
	    // client shouldn't be shown on pager
	    if (client->State & WM_STATE_NOPAGER) {
		continue;
	    }
	    // skip this client if it isn't on the selected desktop
	    if (client->State & WM_STATE_STICKY) {
		if (!pager_plugin->Sticky && DesktopCurrent != desktop) {
		    continue;
		}
	    } else if (client->Desktop != desktop) {
		continue;
	    }
	    // get the offset and size of client on the pager desktop
	    mini_x = (client->X * pager_plugin->ScaleX + 65536) / 65536;
	    mini_y = (client->Y * pager_plugin->ScaleY + 65536) / 65536;
	    mini_w = (client->Width * pager_plugin->ScaleX) / 65536;
	    mini_h = (client->Height * pager_plugin->ScaleY) / 65536;

	    // normalize the offset and size
	    if (mini_x + mini_w > pager_plugin->DeskWidth) {
		mini_w = pager_plugin->DeskWidth - mini_x;
	    }
	    if (mini_y + mini_h > pager_plugin->DeskHeight) {
		mini_h = pager_plugin->DeskHeight - mini_y;
	    }
	    if (mini_x < 0) {
		mini_w += mini_x;
		mini_x = 0;
	    }
	    if (mini_y < 0) {
		mini_h += mini_y;
		mini_y = 0;
	    }
	    // skip client if we are no longer in bounds
	    if (mini_w <= 0 || mini_h <= 0) {
		continue;
	    }
	    // check the y-coordinate
	    if (y < mini_y || y > mini_y + (signed)mini_h) {
		continue;
	    }
	    // check the x-coordinate
	    if (x < mini_x || x > mini_x + (signed)mini_w) {
		continue;
	    }
	    // found it. exit
	    return client;
	}
    }
    return NULL;
}

// ------------------------------------------------------------------------ //
// Client window move in pager

/**
**	Client-terminated pager move.
*/
static void PagerMoveController(void)
{
    xcb_ungrab_pointer(Connection, XCB_CURRENT_TIME);
    xcb_ungrab_keyboard(Connection, XCB_CURRENT_TIME);
    ClientFinishAction = 1;
}

/**
**	Stop an active pager move.
**
**	@param client	client moved arround
**	@param do_move	true, client was moved
**	@param hmax	client was horizontally maximized
**	@param vmax	client was vertically maximized
*/
static void PagerStopMove(Client * client, int do_move, int hmax, int vmax)
{
    int north;
    int south;
    int east;
    int west;
    uint32_t values[2];

    // release grabs
    client->Controller();
    client->Controller = ClientDefaultController;

    if (do_move) {
	// THIS is only needed, if moved with outline
	BorderGetSize(client, &north, &south, &east, &west);

	values[0] = client->X - west;
	values[1] = client->Y - north;
	xcb_configure_window(Connection, client->Parent,
	    XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, values);
	ClientSendConfigureEvent(client);

	// restore the maximized state of client
	if (hmax || vmax) {
	    ClientMaximize(client, hmax, vmax);
	}
	// redraw the pager
	PagerUpdate();
    }
}

/**
**	Start a pager move operation.
**
**	@param plugin	common panel plugin data of pager
**	@param startx	mouse x-coordinate
**	@param starty	mouse y-coordinate
**
**	@returns true if the client moved, false otherwise.
**
**	@todo disable panel autohide during pager move.
**	@todo scale mouse movement down.
*/
int PagerMoveLoop(Plugin * plugin, int startx, int starty)
{
    PagerPlugin *pager_plugin;
    Client *client;
    xcb_grab_pointer_cookie_t cookie;
    int do_move;
    int hmax;
    int vmax;
    int north;
    int south;
    int east;
    int west;
    int olddesktop;

    pager_plugin = plugin->Object;

    // client wasn't found, just return
    if (!(client = PagerGetClient(pager_plugin, startx, starty))) {
	return 0;
    }
    // is movement allowed
    if (!(client->Border & BORDER_MOVE)) {
	return 0;
    }
    cookie = PointerGrabForMoveRequest();

    BorderGetSize(client, &north, &south, &east, &west);

    hmax = 0;
    vmax = 0;
    do_move = 0;

    ClientFinishAction = 0;
    client->Controller = PagerMoveController;

    olddesktop = client->Desktop;

    PointerGrabReply(cookie);
    // FIXME: what if grab failed?
    if (!(PointerGetButtonMask() & (XCB_BUTTON_MASK_1 | XCB_BUTTON_MASK_3))) {
	Debug(3, "only client clicked, leave early\n");
	PagerStopMove(client, 0, 0, 0);
	return 0;
    }
    for (;;) {
	for (;;) {
	    xcb_generic_event_t *event;

	    if (ClientFinishAction || !KeepLooping) {
		client->Controller = ClientDefaultController;
		return do_move;
	    }
	    if (!(event = PollNextEvent())) {
		break;
	    }
	    switch (XCB_EVENT_RESPONSE_TYPE(event)) {
		    uint32_t values[2];
		    int desktop;
		    int x;
		    int y;

		case XCB_BUTTON_RELEASE:
		    // button released stop movement
		    if (((xcb_button_press_event_t *) event)->detail ==
			XCB_BUTTON_INDEX_1
			|| ((xcb_button_press_event_t *) event)->detail ==
			XCB_BUTTON_INDEX_3) {
			PagerStopMove(client, do_move, hmax, vmax);
			free(event);
			return do_move;
		    }
		case XCB_BUTTON_PRESS:
		    break;

		case XCB_MOTION_NOTIFY:
		    DiscardMotionEvents((xcb_motion_notify_event_t **) & event,
			plugin->Panel->Window);

		    x = ((xcb_motion_notify_event_t *) event)->root_x;
		    y = ((xcb_motion_notify_event_t *) event)->root_y;

		    // only the first time moved
		    if (!do_move) {
			if (abs(startx - x) > CLIENT_MOVE_DELTA
			    || abs(starty - y) > CLIENT_MOVE_DELTA) {

			    // if client is maximized, unmaximize it
			    if (client->State & WM_STATE_MAXIMIZED_HORZ) {
				hmax = 1;
			    }
			    if (client->State & WM_STATE_MAXIMIZED_VERT) {
				vmax = 1;
			    }
			    if (hmax || vmax) {
				ClientMaximize(client, 0, 0);
			    }

			    do_move = 1;
			} else {
			    break;
			}
		    }
		    // get the mouse position on pager
		    x -= plugin->ScreenX;
		    y -= plugin->ScreenY;

		    // don't move if we are off of the pager
		    if (x < 0 || x > plugin->Width) {
			break;
		    }
		    if (y < 0 || y > plugin->Height) {
			break;
		    }
		    // determine the new client desktop
		    desktop = PagerGetDesktop(pager_plugin, x, y);
		    if (pager_plugin->Layout == PANEL_LAYOUT_HORIZONTAL) {
			// Johns: +1
			x -= (pager_plugin->DeskWidth + 1) * desktop;
		    } else {
			y -= (pager_plugin->DeskHeight + 1) * desktop;
		    }

		    // if this client isn't sticky and now on a different
		    //	desktop, change client's desktop
		    if (!(client->State & WM_STATE_STICKY)) {
			if (desktop != olddesktop) {
			    ClientSetDesktop(client, desktop);
			    olddesktop = desktop;
			}
		    }
		    // get new client coordinates
		    x = startx + (x - startx);
		    x = (x * 65536) / pager_plugin->ScaleX;
		    x -= (client->Width + east + west) / 2;
		    y = starty + (y - starty);
		    y = (y * 65536) / pager_plugin->ScaleY;
		    y -= (client->Height + north + south) / 2;

		    // move window
		    client->X = x;
		    client->Y = y;

		    ClientSnap(client);

		    // FIXME: same outline like normal window move
		    values[0] = client->X - west;
		    values[1] = client->Y - north;
		    xcb_configure_window(Connection, client->Parent,
			XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, values);
		    ClientSendConfigureEvent(client);
		    PagerUpdate();
		    break;
		default:
		    xcb_event_handle(&EventHandlers, event);
		    break;
	    }
	    free(event);
	}
	WaitForEvent();
    }
}

// ------------------------------------------------------------------------ //
// Draw

/**
**	Draw a client on the pager.
**
**	@param pager_plugin	pager plugin to draw
**	@param client		client to check and to draw
*/
static void PagerDrawClient(const PagerPlugin * pager_plugin,
    const Client * client)
{
    xcb_rectangle_t rectangle;
    int desk_offset;

    // don't draw client if it isn't mapped
    if (!(client->State & WM_STATE_MAPPED)) {
	return;
    }
    // client shouldn't be shown on pager
    if (client->State & WM_STATE_NOPAGER) {
	return;
    }
    // determine the desktop for client
    // FIXME: sticky clients, should be shown on all desktop (with option)
    if (client->State & WM_STATE_STICKY) {
	desk_offset = DesktopCurrent;
    } else {
	desk_offset = client->Desktop;
    }
    if (pager_plugin->Layout == PANEL_LAYOUT_HORIZONTAL) {
	desk_offset *= pager_plugin->DeskWidth + 1;
    } else {
	desk_offset *= pager_plugin->DeskHeight + 1;
    }

    // determine the location and size of client on the pager
    rectangle.x = (client->X * pager_plugin->ScaleX + 65536) / 65536;
    rectangle.y = (client->Y * pager_plugin->ScaleY + 65536) / 65536;
    rectangle.width = (client->Width * pager_plugin->ScaleX) / 65536;
    rectangle.height = (client->Height * pager_plugin->ScaleY) / 65536;

    // stay on pager desktop, fix size and offset
    if (rectangle.x + rectangle.width > pager_plugin->DeskWidth) {
	rectangle.width = pager_plugin->DeskWidth - rectangle.x;
    }
    if (rectangle.y + rectangle.height > pager_plugin->DeskHeight) {
	rectangle.height = pager_plugin->DeskHeight - rectangle.y;
    }
    if (rectangle.x < 0) {
	rectangle.width += rectangle.x;
	rectangle.x = 0;
    }
    if (rectangle.y < 0) {
	rectangle.height += rectangle.y;
	rectangle.y = 0;
    }
    // return if there's nothing to do
    if (rectangle.width <= 0 || rectangle.height <= 0) {
	return;
    }
    // move to the correct desktop on the pager
    if (pager_plugin->Layout == PANEL_LAYOUT_HORIZONTAL) {
	rectangle.x += desk_offset;
    } else {
	rectangle.y += desk_offset;
    }

    // draw client outline
    xcb_change_gc(Connection, RootGC, XCB_GC_FOREGROUND,
	&Colors.PagerOutline.Pixel);
    xcb_poly_rectangle(Connection, pager_plugin->Plugin->Pixmap, RootGC, 1,
	&rectangle);

    // fill client if there's room
    if (rectangle.width > 1 && rectangle.height > 1) {
	uint32_t pixel;

	if ((client->State & WM_STATE_ACTIVE)
	    && (client->Desktop == DesktopCurrent
		|| (client->State & WM_STATE_STICKY))) {
	    pixel = Colors.PagerActiveFG.Pixel;
	} else {
	    pixel = Colors.PagerFG.Pixel;
	}
	xcb_change_gc(Connection, RootGC, XCB_GC_FOREGROUND, &pixel);

	rectangle.x++;
	rectangle.y++;
	rectangle.width--;
	rectangle.height--;
	xcb_poly_fill_rectangle(Connection, pager_plugin->Plugin->Pixmap,
	    RootGC, 1, &rectangle);
    }
}

/**
**	Draw pager label.
**
**	@param pager_plugin	pager plugin to draw
*/
static void PagerDrawLabel(const PagerPlugin * pager_plugin)
{
    int i;
    int x;
    int y;
    const Plugin *plugin;
    xcb_pixmap_t pixmap;
    unsigned width;
    unsigned height;
    unsigned desk_width;
    unsigned desk_height;
    unsigned text_width;
    unsigned text_height;

    plugin = pager_plugin->Plugin;
    pixmap = plugin->Pixmap;
    width = plugin->Width;
    height = plugin->Height;
    desk_width = pager_plugin->DeskWidth;
    desk_width = pager_plugin->DeskWidth;
    desk_height = pager_plugin->DeskHeight;

    text_height = Fonts.Pager.Height;
    if (text_height < desk_height) {
	for (i = 0; i < DesktopN; i++) {
	    const char *name;
	    xcb_query_text_extents_cookie_t cookie;

	    name = DesktopGetName(i);
	    cookie = FontQueryExtentsRequest(&Fonts.Pager, strlen(name), name);
	    text_width = FontTextWidthReply(cookie);

	    if (text_width < desk_width) {
		if (pager_plugin->Layout == PANEL_LAYOUT_HORIZONTAL) {
		    x = i * (desk_width + 1) + desk_width / 2 - text_width / 2;
		    y = height / 2 - text_height / 2;
		} else {
		    x = width / 2 - text_width / 2;
		    y = i * (desk_height + 1) + desk_height / 2 -
			text_height / 2;
		}
		FontDrawString(pixmap, &Fonts.Pager, Colors.PagerText.Pixel, x,
		    y, desk_width, NULL, name);
	    }
	}
    }
}

/**
**	Update all pager plugin(s).
*/
void PagerUpdate(void)
{
    const PagerPlugin *pager_plugin;

    if (!KeepLooping) {			// no work if exiting
	return;
    }
#ifdef DEBUG
    if (SLIST_EMPTY(&Pagers)) {
	Debug(2, "FIXME: pagers missing init %s or no pager\n", __FUNCTION__);
    }
#endif
    if (!ClientLayers[0].tqh_first && !ClientLayers[0].tqh_last) {
	Debug(2, "FIXME: clients missing init %s\n", __FUNCTION__);
	return;
    }

    SLIST_FOREACH(pager_plugin, &Pagers, Next) {
	xcb_pixmap_t pixmap;
	xcb_rectangle_t rectangle;
	const Plugin *plugin;
	unsigned desk_width;
	unsigned desk_height;
	unsigned width;
	unsigned height;
	int i;

	plugin = pager_plugin->Plugin;
	// draw background
	PanelClearPluginBackgroundWithColor(plugin, &Colors.PagerBG.Pixel);

	pixmap = plugin->Pixmap;
	width = plugin->Width;
	height = plugin->Height;
	desk_width = pager_plugin->DeskWidth;
	desk_height = pager_plugin->DeskHeight;

	// highlight current desktop
	xcb_change_gc(Connection, RootGC, XCB_GC_FOREGROUND,
	    &Colors.PagerActiveBG.Pixel);
	if (pager_plugin->Layout == PANEL_LAYOUT_HORIZONTAL) {
	    rectangle.x = DesktopCurrent * (desk_width + 1);
	    rectangle.y = 0;
	    rectangle.width = desk_width;
	    rectangle.height = height;
	} else {
	    rectangle.x = 0;
	    rectangle.y = DesktopCurrent * (desk_height + 1);
	    rectangle.width = width;
	    rectangle.height = desk_height;
	}
	xcb_poly_fill_rectangle(Connection, pixmap, RootGC, 1, &rectangle);

	// draw clients (bottom to top)
	for (i = LAYER_BOTTOM; i <= LAYER_TOP; i++) {
	    const Client *client;

	    TAILQ_FOREACH_REVERSE(client, &ClientLayers[i], _client_layer_,
		LayerQueue) {
		PagerDrawClient(pager_plugin, client);
	    }
	}

	// draw labels
	if (pager_plugin->Labeled) {
	    PagerDrawLabel(pager_plugin);
	}
	// draw desktop dividers
	xcb_change_gc(Connection, RootGC, XCB_GC_FOREGROUND,
	    &Colors.PagerFG.Pixel);
	for (i = 1; i < DesktopN; i++) {
	    xcb_point_t points[2];

	    if (pager_plugin->Layout == PANEL_LAYOUT_HORIZONTAL) {
		points[0].x = (desk_width + 1) * i - 1;
		points[0].y = 0;
		points[1].x = (desk_width + 1) * i - 1;
		points[1].y = height;
	    } else {
		points[0].x = 0;
		points[0].y = (desk_height + 1) * i - 1;
		points[1].x = width;
		points[1].y = (desk_height + 1) * i - 1;
	    }
	    xcb_poly_line(Connection, XCB_COORD_MODE_ORIGIN, pixmap, RootGC, 2,
		points);
	}
	PanelUpdatePlugin(plugin->Panel, plugin);
    }
}

// ------------------------------------------------------------------------ //
// Callbacks

/**
**	Resize pager panel plugin.
**
**	@param plugin	panel plugin
**
**	@todo can use default method.
*/
static void PagerResize(Plugin * plugin)
{
    PanelPluginDeletePixmap(plugin);
    PanelPluginCreatePixmap(plugin);
    // FIXME: only current needs a redraw
    PagerUpdate();
}

/**
**	Set size of a pager panel plugin.
**
**	@param plugin	panel plugin of pager
**	@param width	new width
**	@param height	new height
*/
static void PagerSetSize(Plugin * plugin, unsigned width, unsigned height)
{
    PagerPlugin *pager_plugin;

    pager_plugin = plugin->Object;

    Debug(3, "%s (%d,%d)\n", __FUNCTION__, width, height);
    if (width) {			// vertical pager,
	// compute height from width.
	pager_plugin->DeskHeight = (width * RootHeight) / RootWidth;
	pager_plugin->DeskWidth = width;
	pager_plugin->Layout = PANEL_LAYOUT_VERTICAL;
	plugin->Width = width;
	plugin->Height = (pager_plugin->DeskHeight + 1) * DesktopN;
    } else if (height) {		// horizontal pager
	// compute width from height
	pager_plugin->DeskWidth = (height * RootWidth) / RootHeight;
	pager_plugin->DeskHeight = height;
	pager_plugin->Layout = PANEL_LAYOUT_HORIZONTAL;
	plugin->Width = (pager_plugin->DeskWidth + 1) * DesktopN;
	plugin->Height = height;
    } else {
	Debug(0, "oops don't know what todo\n");
    }

    pager_plugin->ScaleX = ((pager_plugin->DeskWidth - 2)
	* 65536) / RootWidth;
    pager_plugin->ScaleY = ((pager_plugin->DeskHeight - 2)
	* 65536) / RootHeight;
}

/**
**	Show tooltip of pager plugin.
**
**	"client-name" @ "desktop-name"
**
**	@param plugin	panel plugin of pager
**	@param x	current mouse x-coordinate
**	@param y	current mouse y-coordinate
*/
static void PagerTooltip(Plugin * plugin, int x, int y)
{
    const char *desktop_name;
    const Client *client;
    char *tooltip;

    desktop_name =
	DesktopGetName(PagerGetDesktop(plugin->Object, x - plugin->ScreenX,
	    y - plugin->ScreenY));
    tooltip = (char *)desktop_name;
    if ((client =
	    PagerGetClient(plugin->Object, x - plugin->ScreenX,
		y - plugin->ScreenY)) && client->Name) {
	tooltip = alloca(strlen(desktop_name) + strlen(client->Name) + 4);
	stpcpy(stpcpy(stpcpy(tooltip, client->Name), " @ "), desktop_name);
    }
    TooltipShow(x, y, tooltip);
}

/**
**	Handle a button event on a pager panel plugin.
**
**	@param plugin	common panel plugin data
**	@param x	x-coordinate of button press
**	@param y	y-coordinate of button press
**	@param mask	button mask
*/
static void PagerHandleButtonPress(Plugin * plugin, int x, int y, int mask)
{
    switch (mask) {
	case XCB_BUTTON_INDEX_1:	// move client window or
	    if (!PagerMoveLoop(plugin, x, y)) {
	case XCB_BUTTON_INDEX_2:	// change to selected desktop
		DesktopChange(PagerGetDesktop(plugin->Object, x, y));
	    }
	    break;

	case XCB_BUTTON_INDEX_3:	// move a client and possibly change its desktop
	    PagerMoveLoop(plugin, x, y);
	    break;

	case XCB_BUTTON_INDEX_4:	// change to previous desktop
	    DesktopPrevious();
	    break;

	case XCB_BUTTON_INDEX_5:	// change to next desktop
	    DesktopNext();
	    break;

	default:
	    break;
    }
}

// ------------------------------------------------------------------------ //

#if 0

/**
**	Initialize the pager panel plugin.
*/
void PagerInit(void)
{
}
#endif

/**
**	Cleanup the pager panel plugin.
*/
void PagerExit(void)
{
    PagerPlugin *pager_plugin;

    while (!SLIST_EMPTY(&Pagers)) {	// list deletion
	pager_plugin = SLIST_FIRST(&Pagers);

	SLIST_REMOVE_HEAD(&Pagers, Next);
	free(pager_plugin);
    }
}

// ------------------------------------------------------------------------ //
// Config

/**
**	Create a new pager panel plugin from config data.
**
**	@param array	configuration array for pager panel plugin
**
**	@returns created pager panel plugin.
*/
Plugin *PagerConfig(const ConfigObject * array)
{
    PagerPlugin *pager_plugin;
    Plugin *plugin;

    pager_plugin = calloc(1, sizeof(*pager_plugin));
    SLIST_INSERT_HEAD(&Pagers, pager_plugin, Next);

    if (ConfigGetBoolean(array, "labeled", NULL) > 0) {
	pager_plugin->Labeled = 1;
    }
    if (ConfigGetBoolean(array, "sticky", NULL) > 0) {
	pager_plugin->Sticky = 1;
    }

    plugin = PanelPluginNew();
    plugin->Object = pager_plugin;
    pager_plugin->Plugin = plugin;

    plugin->Create = PanelPluginCreatePixmap;
    plugin->Delete = PanelPluginDeletePixmap;
    plugin->Resize = PagerResize;
    plugin->SetSize = PagerSetSize;
    plugin->Tooltip = PagerTooltip;
    plugin->HandleButtonPress = PagerHandleButtonPress;

    return plugin;
}

#endif // } USE_PAGER

/// @}
