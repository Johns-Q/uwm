///
///	@file desktop.c		@brief desktop management functions
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
///	@defgroup desktop The desktop module.
///
///	This module handles multiple desktops, which is our name for
///	virtual screens / workspace.
///
/// @{

#define _GNU_SOURCE	1		///< fix stpcpy strchrnul

#include <xcb/xcb.h>
#include "uwm.h"

#include <sys/queue.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <xcb/xcb_icccm.h>
#include <xcb/xcb_atom.h>

#include "core-array/core-array.h"
#include "core-rc/core-rc.h"

#include "client.h"
#include "hints.h"

#include "draw.h"
#include "image.h"
#include "icon.h"
#include "menu.h"
#include "background.h"
#include "desktop.h"

#include "panel.h"
#include "plugin/button.h"
#include "plugin/pager.h"
#include "plugin/task.h"

// ------------------------------------------------------------------------ //

/**
**	Desktop typedef.
*/
typedef struct _desktop_ Desktop;

/**
**	Desktop strukture.
*/
struct _desktop_
{
    char *Name;				///< name of desktop
    // FIXME: move background here?
};

static Desktop *Desktops;		///< all desktops
int DesktopN;				///< number of desktops
static int DesktopShowing;		///< desktop background shown
int DesktopCurrent;			///< current desktop

// ------------------------------------------------------------------------ //
// Commands

/**
**	Change to the specified desktop.
**
**	@param desktop	switch to this desktop number
*/
void DesktopChange(int desktop)
{
    Client *client;
    int layer;

    if (desktop >= DesktopN) {		// out of range
	return;
    }
    if (DesktopCurrent == desktop) {	// already on this desktop
	return;
    }
    /*
     ** Hide clients from the old desktop.
     ** Note that we show clients in a separate loop to prevent an issue
     ** with clients loosing focus.
     */
    for (layer = LAYER_BOTTOM; layer < LAYER_MAX; layer++) {
	TAILQ_FOREACH(client, &ClientLayers[layer], LayerQueue) {
	    if (client->State & WM_STATE_STICKY) {
		continue;
	    }
	    if (client->Desktop == DesktopCurrent) {
		ClientHide(client);
	    }
	}
    }

    // show clients on the new desktop
    for (layer = LAYER_BOTTOM; layer < LAYER_MAX; layer++) {
	TAILQ_FOREACH(client, &ClientLayers[layer], LayerQueue) {
	    if (client->State & WM_STATE_STICKY) {
		continue;
	    }
	    if (client->Desktop == desktop) {
		ClientShow(client);
	    }
	}
    }

    DesktopCurrent = desktop;

    AtomSetCardinal(RootWindow, &Atoms.NET_CURRENT_DESKTOP, DesktopCurrent);

    ClientRestack();
    TaskUpdate();
    PagerUpdate();
    DesktopUpdate();

    BackgroundLoad(desktop);
}

/**
**	Change to the next desktop.
*/
void DesktopNext(void)
{
    DesktopChange((DesktopCurrent + 1) % DesktopN);
}

/**
**	Change to the previous desktop.
*/
void DesktopPrevious(void)
{
    if (DesktopCurrent > 0) {
	DesktopChange(DesktopCurrent - 1);
    } else {
	DesktopChange(DesktopN - 1);
    }
}

// ------------------------------------------------------------------------ //

/**
**	Get the name of a desktop.
**
**	@param desktop	desktop number
**
**	@returns the desktop name as read-only string.
*/
const char *DesktopGetName(int desktop)
{
    if (Desktops && desktop < DesktopN && Desktops[desktop].Name) {
	return Desktops[desktop].Name;
    } else {
	return "";
    }
}

/**
**	Create a desktop menu.
**
**	@param mask	desktop mask
**
**	@returns menu to switch desktops.
*/
Menu *DesktopCreateMenu(unsigned mask)
{
    Menu *menu;
    int i;

    menu = MenuNew();
    for (i = 0; i < DesktopN; i++) {
	MenuItem *item;
	char *s;

	item = MenuNewItem(NULL, NULL);
	s = malloc(strlen(Desktops[i].Name) + 3);
	if (mask & (1 << i)) {
	    stpcpy(stpcpy(stpcpy(s, "["), Desktops[i].Name), "]");
	} else {
	    stpcpy(stpcpy(stpcpy(s, " "), Desktops[i].Name), " ");
	}
	item->Text = s;
	item->Command.Type = MENU_ACTION_SET_DESKTOP;
	item->Command.Integer = i;
	MenuAppendItem(menu, item);
    }

    return menu;
}

/**
**	Toggle the "show desktop" state.
*/
void DesktopToggleShow(void)
{
    Client *client;
    int layer;

    for (layer = LAYER_BOTTOM; layer < LAYER_MAX; layer++) {
	TAILQ_FOREACH(client, &ClientLayers[layer], LayerQueue) {
	    // skip "nolist" items
	    if (client->State & WM_STATE_NOLIST) {
		continue;
	    }
	    if (DesktopShowing) {
		if (client->State & WM_STATE_SHOW_DESKTOP) {
		    ClientRestore(client, 0);
		}
	    } else if (client->Desktop == DesktopCurrent
		|| (client->State & WM_STATE_STICKY)) {
		if (client->State & (WM_STATE_MAPPED | WM_STATE_SHADED)) {
		    ClientMinimize(client);
		    client->State |= WM_STATE_SHOW_DESKTOP;
		}
	    }
	}
    }
    DesktopShowing = !DesktopShowing;

    // _NET_SHOWING_DESKTOP
    AtomSetCardinal(RootWindow, &Atoms.NET_SHOWING_DESKTOP, DesktopShowing);

    ClientRestack();
}

/**
**	Toggle the "shade desktop" state.
*/
void DesktopToggleShade(void)
{
    Client *client;
    int layer;

    for (layer = LAYER_BOTTOM; layer < LAYER_MAX; layer++) {
	TAILQ_FOREACH(client, &ClientLayers[layer], LayerQueue) {
	    // skip "nolist" items
	    if (client->State & WM_STATE_NOLIST) {
		continue;
	    }
	    if (client->State & WM_STATE_SHADED) {
		ClientUnshade(client);
	    } else {
		ClientShade(client);
	    }
	}
    }
}

/**
**	Initialize desktop support.
*/
void DesktopInit(void)
{
    int i;
    int n;
    char buf[16];
    char *data;

    // allocate space, if not already done by config
    if (!Desktops) {
	if (!DesktopN) {		// 4 desktops default
	    DesktopN = DESKTOP_DEFAULT_COUNT;
	}
	Desktops = calloc(DesktopN, sizeof(*Desktops));
    }
    // set name of desktop, if not already done by config
    for (i = 0; i < DesktopN; i++) {
	if (!Desktops[i].Name) {
	    snprintf(buf, sizeof(buf), "desktop %d", i + 1);
	    Desktops[i].Name = strdup(buf);
	}
    }
    DesktopShowing = 0;

    //
    //	Update hints, which can't be set in AtomInit.
    //

    // _NET_SHOWING_DESKTOP
    AtomSetCardinal(RootWindow, &Atoms.NET_SHOWING_DESKTOP, DesktopShowing);

    // _NET_NUMBER_OF_DESKTOPS
    AtomSetCardinal(RootWindow, &Atoms.NET_NUMBER_OF_DESKTOPS, DesktopN);

    // _NET_DESKTOP_NAMES
    for (n = i = 0; i < DesktopN; i++) {
	n += strlen(Desktops[i].Name) + 1;
    }
    data = alloca(n);
    for (n = i = 0; i < DesktopN; i++) {
	strcpy(data + n, Desktops[i].Name);
	n += strlen(Desktops[i].Name) + 1;
    }
    xcb_change_property(Connection, XCB_PROP_MODE_REPLACE, RootWindow,
	Atoms.NET_DESKTOP_NAMES.Atom, Atoms.UTF8_STRING.Atom, 8, n, data);
}

/**
**	Cleanup desktop data.
*/
void DesktopExit(void)
{
    int i;

    for (i = 0; i < DesktopN; i++) {
	free(Desktops[i].Name);
    }
    free(Desktops);

    DesktopN = 0;
    Desktops = NULL;
    DesktopCurrent = 0;
    DesktopShowing = 0;
}

// ------------------------------------------------------------------------ //
// Config

/**
**	Parse configuration for desktop module.
**
**	@param config	global config dictionary
*/
void DesktopConfig(const Config * config)
{
    const ConfigObject *array;

    //
    //	array of desktop
    //
    if (ConfigGetArray(ConfigDict(config), &array, "desktop", NULL)) {
	ssize_t ival;
	const ConfigObject *index;
	const ConfigObject *value;

	if (ConfigGetInteger(array, &ival, "count", NULL)) {
	    if (ival < DESKTOP_MINIMAL_COUNT || ival > DESKTOP_MAXIMAL_COUNT) {
		Warning("invalid desktop count: \"%zd\"\n", ival);
	    } else {
		DesktopN = ival;
	    }
	}

	Desktops = calloc(DesktopN, sizeof(*Desktops));

	//
	//	array of buttons
	//
	index = NULL;
	value = ConfigArrayFirstFixedKey(array, &index);
	while (value) {
	    const char *sval;

	    if (ConfigCheckString(value, &sval)) {
		ConfigCheckInteger(index, &ival);
		if (0 <= ival && ival < DesktopN) {
		    Desktops[ival].Name = strdup(sval);
		}
	    } else {
		Warning("value in desktop ignored\n");
	    }
	    value = ConfigArrayNextFixedKey(array, &index);
	}
    }
}

/// @}
