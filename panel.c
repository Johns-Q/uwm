///
///	@file panel.c		@brief panel functions
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
///	@defgroup panel The panel module.
///
///	This module handles panels.  An unlimited number of panels are
///	supported.  Other names for panels or parts of it are slit, toolbar or
///	dock.  Panels can be placed everywhere, can also be abused as desktop
///	buttons or desktop widget.
///
///	A panel can contain buttons, task-list, pager, docked applications,
///	systray or clock.
///
///	Alternatives:
///		- lxde-base/lxpanel
///			@n Lightweight X11 desktop panel
///			@n http://lxde.sf.net
///		- x11-misc/fbpanel
///			@n light-weight X11 desktop panel
///			@n http://fbpanel.sourceforge.net
///< @{

#include <xcb/xcb.h>
#include "uwm.h"

#ifdef USE_PANEL			// {

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <xcb/xcb_atom.h>
#include <xcb/xcb_icccm.h>

#include "queue.h"
#include "core-array/core-array.h"
#include "core-rc/core-rc.h"

#include "draw.h"
#include "tooltip.h"
#include "screen.h"
#include "pointer.h"
#include "image.h"
#include "client.h"
#include "hints.h"

#include "icon.h"
#include "menu.h"
#include "panel.h"
#include "plugin/button.h"
#include "plugin/clock.h"
#include "plugin/netload.h"
#include "plugin/pager.h"
#include "plugin/swallow.h"
#include "plugin/systray.h"
#include "plugin/task.h"

// ------------------------------------------------------------------------ //

    /// list of all panels
struct _panel_head_ Panels = SLIST_HEAD_INITIALIZER(Panels);

static uint32_t PanelOpacity;		///< panel window transparency

// ------------------------------------------------------------------------ //

/**
**	Get the panel plugin under the given coordinates.
**
**	@param panel	prove all plugins of this panel
**	@param x	x-coordinate (panel relative)
**	@param y	y-coordinate (panel relative)
**
**	@returns panel plugin which handles the x, y.
*/
static Plugin *PanelGetPluginByXY(const Panel * panel, int x, int y)
{
    Plugin *plugin;
    int xoffset;
    int yoffset;

    xoffset = panel->Border;
    yoffset = panel->Border;
    STAILQ_FOREACH(plugin, &panel->Plugins, Next) {
	int width;
	int height;

	if (!(width = plugin->Width) || !(height = plugin->Height)) {
	    continue;
	}
	if (x >= xoffset && x - xoffset < width && y >= yoffset
	    && y - yoffset < height) {
	    return plugin;
	}
	if (panel->Layout == PANEL_LAYOUT_HORIZONTAL) {
	    xoffset += width;
	} else {
	    yoffset += height;
	}
    }
    return NULL;
}

// ------------------------------------------------------------------------ //
// Draw

/**
**	Clear panel plugin background with given color.
**
**	Generic function for plugins to clear its background.
**
**	@param plugin	common data of plugin to be cleared
**	@param pixel	x11 pixel of background color
*/
void PanelClearPluginBackgroundWithColor(const Plugin * plugin,
    const uint32_t * pixel)
{
    xcb_rectangle_t rectangle;

    xcb_change_gc(Connection, RootGC, XCB_GC_FOREGROUND, pixel);
    rectangle.x = 0;
    rectangle.y = 0;
    rectangle.width = plugin->Width;
    rectangle.height = plugin->Height;
    xcb_poly_fill_rectangle(Connection, plugin->Pixmap, RootGC, 1, &rectangle);
}

/**
**	Clear panel plugin background.
**
**	Generic function for plugins to clear its background with default color.
**
**	@param plugin	common data of plugin to be cleared
*/
void PanelClearPluginBackground(const Plugin * plugin)
{
    PanelClearPluginBackgroundWithColor(plugin, &Colors.PanelBG.Pixel);
}

/**
**	Update plugin on a panel.
**
**	@param panel	panel which owns the plugin (window)
**	@param plugin	update content of this plugin on screen
*/
void PanelUpdatePlugin(const Panel * panel, const Plugin * plugin)
{
    if (plugin->Pixmap && KeepLooping) {
	xcb_copy_area(Connection, plugin->Pixmap, panel->Window, RootGC, 0, 0,
	    plugin->X, plugin->Y, plugin->Width, plugin->Height);
    }
}

/**
**	Draw a panel.
**
**	@param panel	panel which plugins are drawn
**
**	@todo FIXME: panel hidden flickers!
*/
static void PanelDraw(const Panel * panel)
{
    const Plugin *plugin;
    int i;

    // draw al plugins
    STAILQ_FOREACH(plugin, &panel->Plugins, Next) {
	PanelUpdatePlugin(panel, plugin);
    }

    // draw border around panel
    for (i = 0; i < panel->Border; i++) {
	xcb_point_t points[3];

	xcb_change_gc(Connection, RootGC, XCB_GC_FOREGROUND,
	    &Colors.PanelUp.Pixel);
	// TOP + LEFT
	points[0].x = i;
	points[0].y = panel->Height - i - 1;
	points[1].x = i;
	points[1].y = i;
	points[2].x = panel->Width - i - 1;
	points[2].y = i;
	xcb_poly_line(Connection, XCB_COORD_MODE_ORIGIN, panel->Window, RootGC,
	    3, points);

	// BOTTOM + RIGHT
	xcb_change_gc(Connection, RootGC, XCB_GC_FOREGROUND,
	    &Colors.PanelDown.Pixel);
	points[0].x = panel->Width - i - 1;
	points[0].y = i + 1;
	points[1].x = panel->Width - i - 1;
	points[1].y = panel->Height - i - 1;
	points[2].x = i + 1;
	points[2].y = panel->Height - i - 1;
	xcb_poly_line(Connection, XCB_COORD_MODE_ORIGIN, panel->Window, RootGC,
	    3, points);
    }
}

/**
**	Draw all panels.
*/
void PanelsDraw(void)
{
    if (KeepLooping) {			// not shuting down
	const Panel *panel;

	SLIST_FOREACH(panel, &Panels, Next) {
	    // FIXME: should hidden panels be drawn?
	    PanelDraw(panel);
	}
    }
}

/**
**	Hide a panel (for autohide).
**
**	@param panel	panel to hide
**
**	@todo should be swallows and docked apps be unmapped?
*/
static void PanelHide(Panel * panel)
{
    int x;
    int y;
    uint32_t values[2];

    panel->Hidden = 1;

    x = panel->X;
    y = panel->Y;
    // FIXME: use gravity to hide panel
    switch (panel->Gravity) {
	case GRAVITY_STATIC:
	    // FIXME: should also hide, need to resize window size
	    Debug(2, "FIXME: should hide static panel\n");
	    break;
	case GRAVITY_NORTH_WEST:
	    if (panel->Layout == PANEL_LAYOUT_HORIZONTAL) {
		x = panel->HiddenSize - panel->Width;
	    } else {
		y = panel->HiddenSize - panel->Height;
	    }
	    break;
	case GRAVITY_NORTH:
	    y = panel->HiddenSize - panel->Height;
	    break;
	case GRAVITY_NORTH_EAST:
	    if (panel->Layout == PANEL_LAYOUT_HORIZONTAL) {
		x = XcbScreen->width_in_pixels - panel->HiddenSize;
	    } else {
		y = panel->HiddenSize - panel->Height;
	    }
	    break;
	case GRAVITY_WEST:
	    x = panel->HiddenSize - panel->Width;
	    break;
	case GRAVITY_CENTER:
	    break;
	case GRAVITY_EAST:
	    x = XcbScreen->width_in_pixels - panel->HiddenSize;
	    break;
	case GRAVITY_SOUTH_WEST:
	    if (panel->Layout == PANEL_LAYOUT_HORIZONTAL) {
		x = panel->HiddenSize - panel->Width;
	    } else {
		y = XcbScreen->height_in_pixels - panel->HiddenSize;
	    }
	    break;
	case GRAVITY_SOUTH:
	    y = XcbScreen->height_in_pixels - panel->HiddenSize;
	    break;
	case GRAVITY_SOUTH_EAST:
	    if (panel->Layout == PANEL_LAYOUT_HORIZONTAL) {
		x = XcbScreen->width_in_pixels - panel->HiddenSize;
	    } else {
		y = XcbScreen->height_in_pixels - panel->HiddenSize;
	    }
	    break;
    }

#if 0
    // determine where to move the panel
    if (panel->Layout == PANEL_LAYOUT_HORIZONTAL) {
	x = panel->X;

	// move to top or bottom side?
	if (panel->Y >= (signed)(XcbScreen->height_in_pixels / 2)) {
	    y = XcbScreen->height_in_pixels - panel->HiddenSize;
	} else {
	    y = panel->HiddenSize - panel->Height;
	}
    } else {
	y = panel->Y;

	// move to left or right side?
	if (panel->X >= (signed)(XcbScreen->width_in_pixels / 2)) {
	    x = XcbScreen->width_in_pixels - panel->HiddenSize;
	} else {
	    x = panel->HiddenSize - panel->Width;
	}
    }
#endif

    // move it.
    values[0] = x;
    values[1] = y;
    xcb_configure_window(Connection, panel->Window,
	XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, values);
}

/**
**	Display a panel (for autohide).
**
**	@param panel	panel to show
*/
static void PanelShow(Panel * panel)
{
    if (panel->Hidden) {
	uint32_t values[2];

	panel->Hidden = 0;

	values[0] = panel->X;
	values[1] = panel->Y;
	xcb_configure_window(Connection, panel->Window,
	    XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, values);

	// FIXME: why query pointer? (can generate enter window events?)
	// PointerQuery();
    }
}

/**
**	Hide/show/toggle panel (for commands).
**
**	@param panel_nr	panel number (should be in configuration order!)
**	@param toggle	0: hide, 1: show, -1: toggle.
*/
void PanelToggle(int panel_nr, int toggle)
{
    Panel *panel;

    SLIST_FOREACH(panel, &Panels, Next) {
	if (panel_nr-- <= 0) {
	    break;
	}
    }
    if (!panel) {
	Warning("wrong panel number configured\n");
	return;
    }
    Debug(4, "switch panel %p %+d%+d %d\n", panel, panel->X, panel->Y, toggle);
    switch (toggle) {
	case 0:
	    if (!panel->Hidden) {
		PanelHide(panel);
	    }
	    break;
	case 1:
	    if (panel->Hidden) {
		PanelShow(panel);
	    }
	    break;
	case -1:
	    if (panel->Hidden) {
		PanelShow(panel);
	    } else {
		PanelHide(panel);
	    }
	    break;
    }
}

/**
**	Execute pointer button command.
**
**	Generic function to execute pointer button clicks.
**
**	@param plugin	common plugin data
**	@param button	pointer button commands to be executed
**	@param mask	pointer button number
*/
void PanelExecuteButton(const Plugin * plugin, MenuButton * button, int mask)
{
    const Screen *screen;
    int x;
    int y;

    //
    //	Calculate menu hot-spot
    //
    screen = ScreenGetByXY(plugin->ScreenX, plugin->ScreenY);
    if (plugin->Panel->Layout == PANEL_LAYOUT_HORIZONTAL) {
	x = plugin->ScreenX;
	if (plugin->ScreenY + (int)plugin->Height / 2 <
	    screen->Y + (int)screen->Height / 2) {
	    y = plugin->ScreenY + plugin->Height;
	} else {
	    y = -plugin->ScreenY;
	}
    } else {
	y = plugin->ScreenY;
	if (plugin->ScreenX + (int)plugin->Width / 2 <
	    screen->X + (int)screen->Width / 2) {
	    x = plugin->ScreenX + plugin->Width;
	} else {
	    x = -plugin->ScreenX;
	}
    }

    Debug(3, "menu at %d,%d\n", x, y);
    MenuButtonExecute(button, mask, x, y, NULL);
}

// ------------------------------------------------------------------------ //
// Events
// ------------------------------------------------------------------------ //

/**
**	Find panel by window.
**
**	@param window	X11 window id
**
**	@returns panel pointer if found, NULL otherwise.
*/
static Panel *PanelByWindow(xcb_window_t window)
{
    Panel *panel;

    SLIST_FOREACH(panel, &Panels, Next) {
	if (window == panel->Window) {
	    return panel;
	}
    }
    return NULL;
}

/**
**	Get the panel under the given coordinates.
**
**	@param x	x-coordinate (panel relative)
**	@param y	y-coordinate (panel relative)
**
**	@returns panel which handles the x, y.
*/
static Panel *PanelByXY(int x, int y)
{
    Panel *panel;

    SLIST_FOREACH(panel, &Panels, Next) {
	if (!panel->Hidden && x >= panel->X && x < panel->X + panel->Width
	    && y >= panel->Y && y < panel->Y + panel->Height) {
	    return panel;
	}
    }
    return NULL;
}

/**
**	Handle a button press on a panel.
**
**	@param event	X11 button press event
**
**	@returns true if event is handled by any panel.
*/
int PanelHandleButtonPress(const xcb_button_press_event_t * event)
{
    Panel *panel;

    if ((panel = PanelByWindow(event->event))) {
	Plugin *plugin;

	plugin = PanelGetPluginByXY(panel, event->event_x, event->event_y);
	if (plugin && plugin->HandleButtonPress) {
	    plugin->HandleButtonPress(plugin, event->event_x - plugin->X,
		event->event_y - plugin->Y, event->detail);
	}
	return 1;
    }
    return 0;
}

/**
**	Handle a button release on a panel.
**
**	@param event	X11 button release event
**
**	@returns true if event is handled by any panel.
*/
int PanelHandleButtonRelease(const xcb_button_release_event_t * event)
{
    Panel *panel;

    if ((panel = PanelByWindow(event->event))) {
	Plugin *plugin;

	// first inform any plugins that have a grab
	STAILQ_FOREACH(plugin, &panel->Plugins, Next) {
	    if (plugin->Grabbed) {
		plugin->HandleButtonRelease(plugin, event->event_x - plugin->X,
		    event->event_y - plugin->Y, event->detail);
		xcb_ungrab_pointer(Connection, XCB_CURRENT_TIME);
		plugin->Grabbed = 0;
		return 1;
	    }
	}

	// normal release: find plugin and send it
	plugin = PanelGetPluginByXY(panel, event->event_x, event->event_y);
	if (plugin && plugin->HandleButtonRelease) {
	    plugin->HandleButtonRelease(plugin, event->event_x - plugin->X,
		event->event_y - plugin->Y, event->detail);
	}
	return 1;
    }
    return 0;
}

/**
**	Callback used by tooltip handler to show the tooltip.
**
**	@param x	current mouse x-coordinate
**	@param y	current mouse y-coordinate
*/
static void PanelTooltip(int x, int y)
{
    const Panel *panel;
    const Plugin *plugin;

    if ((panel = PanelByXY(x, y))
	&& (plugin = PanelGetPluginByXY(panel, x - panel->X, y - panel->Y))) {
	plugin->Tooltip(plugin, x, y);
    }
}

/**
**	Handle a motion notify event over a panel.
**
**	@param event	X11 motion notify event
**
**	@returns true if event is handled by any panel, false otherwise.
*/
int PanelHandleMotionNotify(const xcb_motion_notify_event_t * event)
{
    Panel *panel;

    // remember last motion for tooltip
    TooltipRegister(event->root_x, event->root_y, PanelTooltip);

    if ((panel = PanelByWindow(event->event))) {
	Plugin *plugin;

	plugin = PanelGetPluginByXY(panel, event->event_x, event->event_y);
	if (plugin && plugin->HandleMotionNotify) {
	    plugin->HandleMotionNotify(plugin, event->event_x - plugin->X,
		event->event_y - plugin->Y, event->detail);
	}
	return 1;
    }
    return 0;
}

/**
**	Handle a panel enter notify (for autohide).
**
**	@param event	X11 enter notify event
**
**	@returns true if event is handled by any panel, false otherwise.
*/
int PanelHandleEnterNotify(const xcb_enter_notify_event_t * event)
{
    Panel *panel;

    if ((panel = PanelByWindow(event->event))) {
	PanelShow(panel);
	return 1;
    }
    /*
       // FIXME: moved from global event handler
       if (SwallowHandleEnterNotify(event)) {
       return 1;
       }
     */
    return 0;
}

/**
**	Handle a panel expose event.
**
**	@param event	X11 expose event
**
**	@returns true if event is handled by any panel, false otherwise.
*/
int PanelHandleExpose(const xcb_expose_event_t * event)
{
    Panel *panel;

    if ((panel = PanelByWindow(event->window))) {
	PanelDraw(panel);
	return 1;
    }
    return 0;
}

/**
**	Handle timeout for the panel (needed for autohide and tooltip).
**
**	@param tick	current tick in ms
**	@param x	current mouse x-coordinate
**	@param y	current mouse y-coordinate
**
**	@todo write more general tooltip handling
**	@todo delay of autohide?
*/
void PanelTimeout(uint32_t tick, int x, int y)
{
    Panel *panel;
    Plugin *plugin;

    SLIST_FOREACH(panel, &Panels, Next) {
	// FIXME: delay hiding?
	if (panel->AutoHide && !panel->Hidden && !MenuShown) {
	    // mouse outside of the panel
	    if (x < panel->X || x >= panel->X + panel->Width || y < panel->Y
		|| y >= panel->Y + panel->Height) {
		PanelHide(panel);
	    }
	}
	// call timeout of each plugin
	STAILQ_FOREACH(plugin, &panel->Plugins, Next) {
	    plugin->Timeout(plugin, tick, x, y);
	}
    }
}

// ------------------------------------------------------------------------ //

/**
**	Compute the total width of a panel.
**
**	@param panel	panel to get its minimum width
**
**	@returns the total panel width of all fixed size plugins
*/
static int PanelComputeTotalWidth(const Panel * panel)
{
    const Plugin *plugin;
    int result;

    result = 2 * panel->Border;
    STAILQ_FOREACH(plugin, &panel->Plugins, Next) {
	if (plugin->Width > 0) {
	    result += plugin->Width;
	}
    }

    return result;
}

/**
**	Compute the total height of a panel.
**
**	@param panel	panel to get its minimum height
**
**	@returns the total panel height of all fixed size plugins
*/
static int PanelComputeTotalHeight(const Panel * panel)
{
    const Plugin *plugin;
    int result;

    result = 2 * panel->Border;
    STAILQ_FOREACH(plugin, &panel->Plugins, Next) {
	if (plugin->Height > 0) {
	    result += plugin->Height;
	}
    }

    return result;
}

/**
**	Check if the panel fills the screen horizontally.
**
**	@param panel	panel to check it has fixed width
**
**	@returns true if one plugin has variable size.
*/
static int PanelCheckHorizontalFill(const Panel * panel)
{
    const Plugin *plugin;

    STAILQ_FOREACH(plugin, &panel->Plugins, Next) {
	if (!plugin->Width) {
	    return 1;
	}
    }
    return 0;
}

/**
**	Check if the panel fills the screen vertically.
**
**	@param panel	panel to check it has fixed height
**
**	@returns true if one plugin has variable size.
*/
static int PanelCheckVerticalFill(const Panel * panel)
{
    const Plugin *plugin;

    STAILQ_FOREACH(plugin, &panel->Plugins, Next) {
	if (!plugin->Height) {
	    return 1;
	}
    }
    return 0;
}

/**
**	Compute the maximal plugin width.
**
**	@param panel	panel to find its maximal width
**
**	@returns panel maximal width.
*/
static int PanelComputeMaxWidth(const Panel * panel)
{
    const Plugin *plugin;
    int result;

    result = 0;
    STAILQ_FOREACH(plugin, &panel->Plugins, Next) {
	int temp;

	if ((temp = plugin->Width) > 0) {
	    temp += 2 * panel->Border;
	    if (temp > result) {
		result = temp;
	    }
	}
    }

    return result;
}

/**
**	Compute the maximal plugin height.
**
**	@param panel	panel to find its maximal height
**
**	@returns panel maximal height.
*/
static int PanelComputeMaxHeight(const Panel * panel)
{
    const Plugin *plugin;
    int result;

    result = 0;
    STAILQ_FOREACH(plugin, &panel->Plugins, Next) {
	int temp;

	if ((temp = plugin->Height) > 0) {
	    temp += 2 * panel->Border;
	    if (temp > result) {
		result = temp;
	    }
	}
    }

    return result;
}

/**
**	Compute the size and position of a panel.
**
**	@param panel	panel update widths and heights
**
**	panel->Height and panel->Width are set with RequestedWidth
*/
static void PanelComputeSize(Panel * panel)
{
    Plugin *plugin;

    // determine the first dimension
    if (panel->Layout == PANEL_LAYOUT_HORIZONTAL) {
	if (!panel->Height) {
	    panel->Height = PanelComputeMaxHeight(panel);
	    if (!panel->Height) {
		panel->Height = PANEL_DEFAULT_HEIGHT;
	    }
	}
    } else {
	if (!panel->Width) {
	    panel->Width = PanelComputeMaxWidth(panel);
	    if (!panel->Width) {
		panel->Width = PANEL_DEFAULT_WIDTH;
	    }
	}
    }

    // now at least one size is known. handle the plugins
    STAILQ_FOREACH(plugin, &panel->Plugins, Next) {
	if (plugin->SetSize) {
	    if (panel->Layout == PANEL_LAYOUT_HORIZONTAL) {
		plugin->SetSize(plugin, 0, panel->Height - 2 * panel->Border);
	    } else {
		plugin->SetSize(plugin, panel->Width - 2 * panel->Border, 0);
	    }
	}
    }

    // determine the missing dimension
    if (panel->Layout == PANEL_LAYOUT_HORIZONTAL) {
	if (!panel->Width) {
	    if (PanelCheckHorizontalFill(panel)) {
		Debug(2, "FIXME: gravity error\n");
		panel->Width = XcbScreen->width_in_pixels;
	    } else {
		panel->Width = PanelComputeTotalWidth(panel);
	    }
	    if (!panel->Width) {
		panel->Width = PANEL_DEFAULT_WIDTH;
	    }
	}
    } else {
	if (!panel->Height) {
	    if (PanelCheckVerticalFill(panel)) {
		Debug(2, "FIXME: gravity error\n");
		panel->Height = XcbScreen->height_in_pixels;
	    } else {
		panel->Height = PanelComputeTotalHeight(panel);
	    }
	    if (!panel->Height) {
		panel->Height = PANEL_DEFAULT_HEIGHT;
	    }
	}
    }

    // compute the panel location
    panel->X = panel->RequestedX;
    panel->Y = panel->RequestedY;
    switch (panel->Gravity) {
	case GRAVITY_STATIC:
	    if (panel->X < 0) {
		panel->X += XcbScreen->width_in_pixels - panel->Width + 1;
	    }
	    if (panel->Y < 0) {
		panel->Y += XcbScreen->height_in_pixels - panel->Height + 1;
	    }
	    break;
	case GRAVITY_NORTH_WEST:
	    // panel->X += 0;
	    // panel->Y += 0;
	    break;
	case GRAVITY_NORTH:
	    panel->X += XcbScreen->width_in_pixels / 2 - panel->Width / 2;
	    // panel->Y += 0;
	    break;
	case GRAVITY_NORTH_EAST:
	    panel->X += XcbScreen->width_in_pixels - panel->Width;
	    // panel->Y += 0;
	    break;
	case GRAVITY_WEST:
	    // panel->X += 0;
	    panel->Y += XcbScreen->height_in_pixels / 2 - panel->Height / 2;
	    break;
	case GRAVITY_CENTER:
	    panel->X += XcbScreen->width_in_pixels / 2 - panel->Width / 2;
	    panel->Y += XcbScreen->height_in_pixels / 2 - panel->Height / 2;
	    break;
	case GRAVITY_EAST:
	    panel->X += XcbScreen->width_in_pixels - panel->Width;
	    panel->Y += XcbScreen->height_in_pixels / 2 - panel->Height / 2;
	    break;
	case GRAVITY_SOUTH_WEST:
	    // panel->X += 0;
	    panel->Y += XcbScreen->height_in_pixels - panel->Height;
	    break;
	case GRAVITY_SOUTH:
	    panel->X += XcbScreen->width_in_pixels / 2 - panel->Width / 2;
	    panel->Y += XcbScreen->height_in_pixels - panel->Height;
	    break;
	case GRAVITY_SOUTH_EAST:
	    panel->X += XcbScreen->width_in_pixels - panel->Width;
	    panel->Y += XcbScreen->height_in_pixels - panel->Height;
	    break;
    }
}

/**
**	Layout panel plugins on a panel.
**
**	@param panel			panel to be layouted
**	@param[out] variable_size	size of a variable sized plugin
**	@param[out] variable_remainder	remainding pixels for variable
*/
static void PanelPrepareLayout(Panel * panel, int *variable_size,
    int *variable_remainder)
{
    Plugin *plugin;
    int variable_count;
    int width;
    int height;
    int temp;

    panel->Width = panel->RequestedWidth;
    panel->Height = panel->RequestedHeight;

    // requested size (can be 0), is default for size
    STAILQ_FOREACH(plugin, &panel->Plugins, Next) {
	plugin->Width = plugin->RequestedWidth;
	plugin->Height = plugin->RequestedHeight;
    }

    PanelComputeSize(panel);

    // get the remaining size after setting fixed size plugins
    // also, keep track of the number of variable size plugins
    width = panel->Width - 2 * panel->Border;
    height = panel->Height - 2 * panel->Border;
    variable_count = 0;
    STAILQ_FOREACH(plugin, &panel->Plugins, Next) {
	if (panel->Layout == PANEL_LAYOUT_HORIZONTAL) {
	    // FIXME: not used swallow should have 0x0 size, see swallow.c
	    temp = plugin->Width;
	    if (temp > 0) {
		width -= temp;
	    } else if (!temp) {
		++variable_count;
	    }
	} else {
	    temp = plugin->Height;
	    if (temp > 0) {
		height -= temp;
	    } else if (!temp) {
		++variable_count;
	    }
	}
    }

    // distribute excess size among variable size plugins
    // if there are no variable size plugins, shrink the panel
    // if we are out of room, just give them a size of one
    *variable_size = 1;
    *variable_remainder = 0;
    if (panel->Layout == PANEL_LAYOUT_HORIZONTAL) {
	if (variable_count) {
	    if (width >= variable_count) {
		*variable_size = width / variable_count;
		*variable_remainder = width % variable_count;
	    }
	} else if (width > 0) {
	    panel->Width -= width;
	}
    } else {
	if (variable_count) {
	    if (height >= variable_count) {
		*variable_size = height / variable_count;
		*variable_remainder = height % variable_count;
	    }
	} else if (height > 0) {
	    panel->Height -= height;
	}
    }
}

/**
**	Resize a panel.
**
**	@param panel	a panel which must be resized
**
**	@todo combine Init+Resize
*/
void PanelResize(Panel * panel)
{
    int variable_size;
    int variable_remainder;
    int xoffset;
    int yoffset;
    Plugin *plugin;
    uint32_t values[4];

    PanelPrepareLayout(panel, &variable_size, &variable_remainder);

    // reposition items on the panel
    xoffset = panel->Border;
    yoffset = panel->Border;

    STAILQ_FOREACH(plugin, &panel->Plugins, Next) {
	unsigned width;
	unsigned height;

	plugin->X = xoffset;
	plugin->Y = yoffset;
	plugin->ScreenX = panel->X + xoffset;
	plugin->ScreenY = panel->Y + yoffset;

	// is plugin resizeable?
	if (plugin->Resize) {
	    if (panel->Layout == PANEL_LAYOUT_HORIZONTAL) {
		height = panel->Height - 2 * panel->Border;
		width = plugin->Width;
		if (!width) {
		    width = variable_size;
		    if (variable_remainder) {
			++width;
			--variable_remainder;
		    }
		}
	    } else {
		width = panel->Width - 2 * panel->Border;
		height = plugin->Height;
		if (!height) {
		    height = variable_size;
		    if (variable_remainder) {
			++height;
			--variable_remainder;
		    }
		}
	    }
	    plugin->Width = width;
	    plugin->Height = height;
	    plugin->Resize(plugin);
	}

	if (plugin->Window) {		// move plugin window
	    // FIXME: can add move to resize?
	    values[0] = xoffset;
	    values[1] = yoffset;
	    xcb_configure_window(Connection, plugin->Window,
		XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, values);
	}

	if (panel->Layout == PANEL_LAYOUT_HORIZONTAL) {
	    xoffset += plugin->Width;
	} else {
	    yoffset += plugin->Height;
	}
    }

    if (panel->Hidden) {		// hide moves window
	PanelHide(panel);
	// FIXME: should combine hide=move and resize here?
	values[0] = panel->Width;
	values[1] = panel->Height;
	xcb_configure_window(Connection, panel->Window,
	    XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, values);
    } else {
	values[0] = panel->X;
	values[1] = panel->Y;
	values[2] = panel->Width;
	values[3] = panel->Height;
	xcb_configure_window(Connection, panel->Window,
	    XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH
	    | XCB_CONFIG_WINDOW_HEIGHT, values);
    }

    // FIXME: draw if hidden? unneccessary or?
    TaskUpdate();
    PanelDraw(panel);
}

/**
**	Initialize panels.
*/
void PanelInit(void)
{
    Panel *panel;

    SLIST_FOREACH(panel, &Panels, Next) {
	int variable_size;
	int variable_remainder;
	uint32_t values[4];
	int xoffset;
	int yoffset;
	Plugin *plugin;

	PanelPrepareLayout(panel, &variable_size, &variable_remainder);

	//
	//	Create the panel window
	//
	panel->Window = xcb_generate_id(Connection);

	values[0] = Colors.PanelBG.Pixel;
	values[1] = 1;
	values[2] =
	    XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_KEY_RELEASE |
	    XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE |
	    XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_POINTER_MOTION |
	    XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_STRUCTURE_NOTIFY;
	values[3] = Cursors.Default;

	xcb_create_window(Connection, XCB_COPY_FROM_PARENT, panel->Window,
	    XcbScreen->root, panel->X, panel->Y, panel->Width, panel->Height,
	    0, XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT,
	    XCB_CW_BACK_PIXEL | XCB_CW_OVERRIDE_REDIRECT | XCB_CW_EVENT_MASK |
	    XCB_CW_CURSOR, values);

	if (PanelOpacity != UINT32_MAX) {
	    AtomSetCardinal(panel->Window, &Atoms.NET_WM_WINDOW_OPACITY,
		PanelOpacity);
	}
	xoffset = panel->Border;
	yoffset = panel->Border;

	// create and layout plugins of this panel
	STAILQ_FOREACH(plugin, &panel->Plugins, Next) {
	    if (plugin->Create) {
		unsigned width;
		unsigned height;

		if (panel->Layout == PANEL_LAYOUT_HORIZONTAL) {
		    height = panel->Height - 2 * panel->Border;
		    width = plugin->Width;
		    if (!width) {
			width = variable_size;
			if (variable_remainder) {
			    ++width;
			    --variable_remainder;
			}
		    }
		} else {
		    width = panel->Width - 2 * panel->Border;
		    height = plugin->Height;
		    if (!height) {
			height = variable_size;
			if (variable_remainder) {
			    ++height;
			    --variable_remainder;
			}
		    }
		}
		plugin->Width = width;
		plugin->Height = height;
		plugin->Create(plugin);
	    }

	    plugin->X = xoffset;
	    plugin->Y = yoffset;
	    plugin->ScreenX = panel->X + xoffset;
	    plugin->ScreenY = panel->Y + yoffset;

	    if (plugin->Window) {
		xcb_reparent_window(Connection, plugin->Window, panel->Window,
		    xoffset, yoffset);
	    }

	    if (panel->Layout == PANEL_LAYOUT_HORIZONTAL) {
		xoffset += plugin->Width;
	    } else {
		yoffset += plugin->Height;
	    }
	}
	// show the panel
	xcb_map_window(Connection, panel->Window);
    }

#if 0
    // Task + Pager needs client module, which isn't up yet.
    TaskUpdate();
    PagerUpdate();
#endif
}

/**
**	Cleanup panel data.
*/
void PanelExit(void)
{
    Panel *panel;
    Plugin *plugin;

    while (!SLIST_EMPTY(&Panels)) {	// singly-linked list deletion
	panel = SLIST_FIRST(&Panels);

	// all plugins of the panel
	plugin = STAILQ_FIRST(&panel->Plugins);
	while (plugin) {		// faster trailq deletion
	    Plugin *temp;

	    temp = STAILQ_NEXT(plugin, Next);

	    plugin->Delete(plugin);

	    free(plugin);
	    plugin = temp;
	}
	xcb_destroy_window(Connection, panel->Window);

	SLIST_REMOVE_HEAD(&Panels, Next);
	free(panel);
    }
}

// ------------------------------------------------------------------------ //
// Init

/**
**	Default panel plugin create method.
**
**	Creates a pixmap of panel plugin size.
**
**	@param plugin	new common plugin data to be be created
*/
void PanelPluginCreatePixmap(Plugin * plugin)
{
    plugin->Pixmap = xcb_generate_id(Connection);
    xcb_create_pixmap(Connection, XcbScreen->root_depth, plugin->Pixmap,
	XcbScreen->root, plugin->Width, plugin->Height);
}

/**
**	Default panel plugin delete method.  Frees used pixmap.
**
**	@param plugin	common plugin data to be deleted
*/
void PanelPluginDeletePixmap(Plugin * plugin)
{
    if (plugin->Pixmap != XCB_NONE) {
	xcb_free_pixmap(Connection, plugin->Pixmap);
	plugin->Pixmap = XCB_NONE;
    }
}

/**
**	Default panel plugin delete method.
**
**	@param plugin	common plugin data to be deleted
*/
static void PanelPluginDelete(Plugin * __attribute__((unused)) plugin)
{
    Debug(3, "plugin delete %p\n", plugin);
}

/**
**	Default panel plugin timeout method.
**
**	@param plugin	common plugin data
**	@param tick	current tick in ms
**	@param x	current mouse x-coordinate
**	@param y	current mouse y-coordinate
*/
static void PanelPluginTimeout(Plugin
    __attribute__((unused)) * plugin, uint32_t
    __attribute__((unused)) tick, int __attribute__((unused)) x, int
    __attribute__((unused)) y)
{
}

/**
**	Default panel plugin tooltip method.
**
**	@param plugin	common plugin data
**	@param x	current mouse x-coordinate
**	@param y	current mouse y-coordinate
*/
static void PanelPluginTooltip(const Plugin
    __attribute__((unused)) * plugin, int __attribute__((unused)) x, int
    __attribute__((unused)) y)
{
}

/**
**	Create an empty panel plugin.
**
**	@returns common plugin data, methods set to default
*/
Plugin *PanelPluginNew(void)
{
    Plugin *plugin;

    plugin = calloc(1, sizeof(*plugin));
    plugin->Delete = PanelPluginDelete;
    plugin->Timeout = PanelPluginTimeout;
    plugin->Tooltip = PanelPluginTooltip;

    return plugin;
}

/**
**	Create a new empty panel.
**
**	@returns empty panel, preset with default values.
*/
Panel *PanelNew(void)
{
    Panel *panel;
    Panel *temp;

    panel = calloc(1, sizeof(*panel));
    panel->Y = -1;
    panel->Border = PANEL_DEFAULT_BORDER;
    panel->HiddenSize = PANEL_DEFAULT_HIDE_SIZE;
    panel->OnLayer = LAYER_PANEL_DEFAULT;

    temp = SLIST_FIRST(&Panels);
    if (temp) {				// insert at tail, but keep simple list
	Panel *next;

	while ((next = SLIST_NEXT(temp, Next))) {
	    temp = next;
	}
	SLIST_INSERT_AFTER(temp, panel, Next);
    } else {
	SLIST_INSERT_HEAD(&Panels, panel, Next);
    }

    STAILQ_INIT(&panel->Plugins);

    return panel;
}

// ------------------------------------------------------------------------ //
// Config

#ifdef USE_RC				// {

/**
**	Parse a panel plugin size configuration.
**
**	@param array		configuration array for panel plugin
**	@param[out] plugin	panel plugin
*/
void PanelPluginConfigSize(const ConfigObject * array, Plugin * plugin)
{
    ssize_t ival;

    if (ConfigStringsGetInteger(array, &ival, "width", NULL)) {
	plugin->RequestedWidth = ival;
	plugin->UserWidth = 1;
    }
    if (ConfigStringsGetInteger(array, &ival, "height", NULL)) {
	plugin->RequestedHeight = ival;
	plugin->UserHeight = 1;
    }
}

/**
**	Parse a single panel configuration.
**
**	@param array	config array of panel with plugins
*/
static void PanelConfigPanel(const ConfigObject * array)
{
    Panel *panel;
    ssize_t ival;
    const char *sval;
    const ConfigObject *index;
    const ConfigObject *value;

    panel = PanelNew();

    //
    //	request-x
    //
    if (ConfigStringsGetInteger(array, &ival, "x", NULL)) {
	panel->RequestedX = ival;
    }
    //
    //	request-y
    //
    if (ConfigStringsGetInteger(array, &ival, "y", NULL)) {
	panel->RequestedY = ival;
    }
    //
    //	request-width
    //
    if (ConfigStringsGetInteger(array, &ival, "width", NULL)) {
	panel->RequestedWidth =
	    ival <
	    0 ? ((ssize_t) XcbScreen->width_in_pixels * -ival) / 100 : ival;
    }
    //
    //	request-height
    //
    if (ConfigStringsGetInteger(array, &ival, "height", NULL)) {
	panel->RequestedHeight =
	    ival <
	    0 ? ((ssize_t) XcbScreen->height_in_pixels * -ival) / 100 : ival;
    }
    //
    //	border
    //
    if (ConfigStringsGetInteger(array, &ival, "border", NULL)) {
	if (ival < PANEL_MINIMAL_BORDER || ival > PANEL_MAXIMAL_BORDER) {
	    Warning("invalid panel border: %zd\n", ival);
	} else {
	    panel->Border = ival;
	}
    }
    //
    //	hidden-size
    //
    if (ConfigStringsGetInteger(array, &ival, "hidden-size", NULL)) {
	if (ival < 1 || ival > 256) {
	    Warning("invalid panel hidden size: %zd\n", ival);
	} else {
	    panel->HiddenSize = ival;
	}
    }
    //
    //	layer
    //
    if (ConfigStringsGetInteger(array, &ival, "layer", NULL)) {
	if (ival < LAYER_BOTTOM || ival > LAYER_TOP) {
	    Warning("invalid panel layer: %zd\n", ival);
	} else {
	    panel->OnLayer = ival;
	}
    }
    //
    //	gravity
    //
    if (ConfigStringsGetString(array, &sval, "gravity", NULL)) {
	int i;

	if ((i = ParseGravity(sval, "panel"))) {
	    panel->Gravity = i;
	}
    }
    //
    //	layout
    //
    sval = NULL;
    if (ConfigStringsGetString(array, &sval, "layout", NULL)) {
	if (!strcasecmp(sval, "auto")) {
	    sval = NULL;
	} else if (!strcasecmp(sval, "horizontal")) {
	    panel->Layout = PANEL_LAYOUT_HORIZONTAL;
	} else if (!strcasecmp(sval, "vertical")) {
	    panel->Layout = PANEL_LAYOUT_VERTICAL;
	} else {
	    Warning("invalid panel layout: \"%s\"\n", sval);
	    sval = NULL;
	}
    }
    // prefer horizontal layout, but use vertical if width
    // is finite and height is larger than width or infinite
    if (!sval) {
	if (panel->RequestedWidth > 0 && (!panel->RequestedHeight
		|| panel->RequestedHeight > panel->RequestedWidth)) {
	    panel->Layout = PANEL_LAYOUT_VERTICAL;
	} else {
	    panel->Layout = PANEL_LAYOUT_HORIZONTAL;
	}
    }
    //
    //	auto-hide
    //
    if (ConfigStringsGetBoolean(array, "auto-hide", NULL) > 0) {
	panel->AutoHide = 1;
    }
    //
    //	maximize-over
    //
    if (ConfigStringsGetBoolean(array, "maximize-over", NULL) > 0) {
	panel->MaximizeOver = 1;
    }
    //
    //	plugins
    //
    index = NULL;
    value = ConfigArrayFirstFixedKey(array, &index);
    while (value) {			// only integer keys
	const ConfigObject *p_array;

	if (ConfigCheckArray(value, &p_array)) {

	    if (ConfigStringsGetString(p_array, &sval, "type", NULL)) {
		Plugin *plugin;

		if (!strcasecmp(sval, "button")) {
		    plugin = PanelButtonConfig(p_array);
		} else if (!strcasecmp(sval, "pager")) {
		    plugin = PagerConfig(p_array);
		} else if (!strcasecmp(sval, "task")) {
		    plugin = TaskConfig(p_array);
		} else if (!strcasecmp(sval, "swallow")) {
		    plugin = SwallowConfig(p_array);
		} else if (!strcasecmp(sval, "systray")) {
		    plugin = SystrayConfig(p_array);
		} else if (!strcasecmp(sval, "clock")) {
		    plugin = ClockConfig(p_array);
		} else if (!strcasecmp(sval, "netload")) {
		    plugin = NetloadConfig(p_array);
		} else {
		    Warning("panel plugin '%s' not supported\n", sval);
		    plugin = NULL;
		}
		if (plugin) {		// disabled plugins return NULL
		    plugin->Panel = panel;

		    STAILQ_INSERT_TAIL(&panel->Plugins, plugin, Next);
		}
	    } else {
		Warning("missing type in panel plugin config\n");
	    }

	} else {
	    Warning("value in panel config ignored\n");
	}
	value = ConfigArrayNextFixedKey(array, &index);
    }
}

/**
**	Parse panel config
**
**	@param config	global config dictionary
*/
void PanelConfig(const Config * config)
{
    const ConfigObject *array;
    double opacity;

    PanelOpacity = UINT32_MAX;

    //
    //	array of panels
    //
    if (ConfigStringsGetArray(ConfigDict(config), &array, "panel", NULL)) {
	const ConfigObject *index;
	const ConfigObject *value;

	//
	//	opacity
	//
	if (ConfigStringsGetDouble(array, &opacity, "opacity", NULL)) {
	    if (opacity <= 0.0 || opacity > 1.0) {
		Warning("invalid panel opacity: %g\n", opacity);
		opacity = 1.0;
	    }
	    PanelOpacity = UINT32_MAX * opacity;
	}
	//
	//	panels
	//
	index = NULL;
	value = ConfigArrayFirstFixedKey(array, &index);
	while (value) {
	    const ConfigObject *panel;

	    if (ConfigCheckArray(value, &panel)) {
		PanelConfigPanel(panel);
	    } else {
		Warning("value in panel config ignored\n");
	    }
	    value = ConfigArrayNextFixedKey(array, &index);
	}
    }
}

#endif // } USE_RC

#endif // } USE_PANEL

/// @}
