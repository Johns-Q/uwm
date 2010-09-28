///
///	@file task.c	@brief task panel plugin functions.
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
///	@defgroup task_plugin	The task panel plugin
///
///	This module adds task list (or task bar) to the panel.
///
///	Alternatives:
///		- gnome-extra/avant-window-navigator
///			Fully customisable dock-like window navigator
///			http://launchpad.net/awn
///
/// @{

#include <xcb/xcb.h>
#include "uwm.h"

#ifdef USE_TASK				// {

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <sys/queue.h>

#include <xcb/xcb_icccm.h>
#include <xcb/xcb_atom.h>
#include <xcb/xcb_event.h>
#include <xcb/xcb_image.h>

#include "array.h"
#include "core-rc/core-rc.h"

#include "draw.h"
#include "tooltip.h"
#include "pointer.h"
#include "screen.h"
#include "image.h"
#include "client.h"
#include "hints.h"
#include "icon.h"
#include "menu.h"
#include "desktop.h"

#include "panel.h"
#include "plugin/task.h"

#include "readable_bitmap.h"

/**
**	Panel task plugin typedef.
*/
typedef struct _task_plugin_ TaskPlugin;

/**
**	Panel task plugin structure.
*/
struct _task_plugin_
{
    SLIST_ENTRY(_task_plugin_) Next;	///< singly-linked task list
    Plugin *Plugin;			///< common plugin data

    PanelLayout Layout:2;		///< layout
    unsigned DynamicSize:1;		///< task list is dynamic size

    int16_t ItemHeight;			///< common height of items
    uint16_t MaxItemWidth;		///< maximal item width
};

    /// Task plugin list head structure
SLIST_HEAD(_task_head_, _task_plugin_);

    /// Linked list of all task plugins
static struct _task_head_ Tasks = SLIST_HEAD_INITIALIZER(Tasks);

    /// Client double linked tail queue head structure
TAILQ_HEAD(_client_task_, _client_);

    /// Double linked tail queue of all clients in tasks
static struct _client_task_ TaskClientList =
TAILQ_HEAD_INITIALIZER(TaskClientList);

/**
**	Minimized arrow, 5x5 pixels
*@{
*/
#define TASK_MINIMIZED_WIDTH 5
#define TASK_MINIMIZED_HEIGHT 5
static const uint8_t MinimizedBitmap[TASK_MINIMIZED_HEIGHT] = {
// *INDENT-OFF*
	O_______,
	OO______,
	OOO_____,
	OOOO____,
	OOOOO___,
// *INDENT-ON*
};

//@}

static xcb_pixmap_t MinimizedPixmap;	///< cached pixmap for bitmap

/**
**	Enumeration of task insert mode.
*/
typedef enum
{
    TASK_INSERT_LEFT,			///< insert new client left
    TASK_INSERT_RIGHT			///< insert new client right
} TaskInsert;

static TaskInsert TaskInsertMode;	///< task insert mode left/right

/**
**	Show menu associated with a task list item.
**
**	@param task_plugin	task plugin private data
**	@param client		client selected
**
**	@todo rewrite show menu code with general function.
*/
static void TaskShowWindowMenu(const TaskPlugin * task_plugin, Client * client)
{
    int x;
    int y;
    const Screen *screen;
    unsigned menu_width;
    unsigned menu_height;
    Runtime *runtime;

    runtime = WindowMenuGetSize(client, &menu_width, &menu_height);

    // FIXME: same code in PanelExecuteButton
    PointerGetPosition(&x, &y);
    screen = ScreenGetByXY(x, y);

    if (task_plugin->Layout == PANEL_LAYOUT_HORIZONTAL) {
	if (task_plugin->Plugin->ScreenY + task_plugin->Plugin->Height / 2 <
	    screen->Y + screen->Height / 2) {
	    y = task_plugin->Plugin->ScreenY + task_plugin->Plugin->Height;
	} else {
	    y = task_plugin->Plugin->ScreenY - menu_height;
	}
	x -= menu_width / 2;
    } else {
	if (task_plugin->Plugin->ScreenX + task_plugin->Plugin->Width / 2 <
	    screen->X + screen->Width / 2) {
	    x = task_plugin->Plugin->ScreenX + task_plugin->Plugin->Width;
	} else {
	    x = task_plugin->Plugin->ScreenX - menu_width;
	}
	y -= menu_height / 2;
    }

    WindowMenuShow(runtime, x, y, client);
}

/**
**	Focus next client in task.
*/
void TaskFocusNext(void)
{
    Client *client;

    // find active window in task
    TAILQ_FOREACH(client, &TaskClientList, TaskQueue) {
	if (ClientShouldFocus(client) && client->State & WM_STATE_ACTIVE) {
	    client = TAILQ_NEXT(client, TaskQueue);
	    break;
	}
    }
    if (!client) {
	client = TAILQ_FIRST(&TaskClientList);
    }
    // skip all non-focus clients
    while (client && !ClientShouldFocus(client)) {
	client = TAILQ_NEXT(client, TaskQueue);
    }
    // round about
    if (!client) {
	client = TAILQ_FIRST(&TaskClientList);
	while (client && !ClientShouldFocus(client)) {
	    client = TAILQ_NEXT(client, TaskQueue);
	}
    }

    if (client) {
	ClientRestore(client, 1);
	ClientFocus(client);
    }
}

/**
**	Focus previous client in task.
*/
void TaskFocusPrevious(void)
{
    Client *client;

    // find active window in task
    TAILQ_FOREACH_REVERSE(client, &TaskClientList, _client_task_, TaskQueue) {
	if (ClientShouldFocus(client) && client->State & WM_STATE_ACTIVE) {
	    client = TAILQ_PREV(client, _client_task_, TaskQueue);
	    break;
	}
    }
    if (!client) {
	client = TAILQ_LAST(&TaskClientList, _client_task_);
    }
    // skip all non-focus clients
    while (client && !ClientShouldFocus(client)) {
	client = TAILQ_PREV(client, _client_task_, TaskQueue);
    }
    // round about
    if (!client) {
	client = TAILQ_LAST(&TaskClientList, _client_task_);
	while (client && !ClientShouldFocus(client)) {
	    client = TAILQ_PREV(client, _client_task_, TaskQueue);
	}
    }

    if (client) {
	ClientRestore(client, 1);
	ClientFocus(client);
    }
}

/**
**	Determine if a client should be shown on task.
**
**	@param client	client in task list
**
**	@retval false	should not be shown in task bar/list.
**	@retval true	should not be shown in task bar/list.
**
**	@todo config what task should be in task list
*/
static int TaskShouldShowItem(const Client * client)
{
    if (client->Desktop != DesktopCurrent
	&& !(client->State & WM_STATE_STICKY)) {
	return 0;
    }
    if (client->State & WM_STATE_NOLIST) {
	return 0;
    }
    if (client->Owner != XCB_NONE) {
	return 0;
    }
    if (!(client->State & WM_STATE_MAPPED)
	&& !(client->State & (WM_STATE_MINIMIZED | WM_STATE_SHADED))) {
	return 0;
    }
    return 1;
}

/**
**	Get number of items on task.
**
**	@returns number of clients shown in task list.
*/
static int TaskGetTaskCount(void)
{
    int n;
    const Client *client;

    n = 0;
    TAILQ_FOREACH(client, &TaskClientList, TaskQueue) {
	if (TaskShouldShowItem(client)) {
	    ++n;
	}
    }
    return n;
}

/**
**	Get width of an item in task.
**
**	@param task_plugin	task plugin private data
**	@param n		number of tasks to display
**
**	@returns item width in task panel.
**
**	Width is limited to TaskPlugin.MaxItemWidth.
*/
static unsigned TaskGetItemWidth(const TaskPlugin * task_plugin, int n)
{
    unsigned item_width;

    item_width = task_plugin->Plugin->Width - TASK_INNER_SPACE;

    if (!n) {				// 0 full size
	return item_width;
    }

    item_width /= n;

    if (!item_width) {			// minimal 1 pixel
	item_width = 1;
    }
    // limited?
    if (item_width > task_plugin->MaxItemWidth) {
	item_width = task_plugin->MaxItemWidth;
    }

    return item_width;
}

/**
**	Get client associated with a xy-coordinate on task.
**
**	@param task_plugin	search in this task plugin
**	@param xy		x or y coordinate to search
**
**	@returns client in task list at x or y coordinate.
*/
static Client *TaskGetClient(TaskPlugin * task_plugin, int xy)
{
    Client *client;
    int remainder;
    int n;
    unsigned item_width;
    int index;
    int stop;
    unsigned width;

    index = TASK_INNER_SPACE;
    n = TaskGetTaskCount();

    if (task_plugin->Layout == PANEL_LAYOUT_HORIZONTAL) {
	width = task_plugin->Plugin->Width - index;
	item_width = TaskGetItemWidth(task_plugin, n);
	remainder = width - item_width * n;

	TAILQ_FOREACH(client, &TaskClientList, TaskQueue) {
	    if (TaskShouldShowItem(client)) {
		if (remainder) {
		    stop = index + item_width + 1;
		    --remainder;
		} else {
		    stop = index + item_width;
		}
		if (xy >= index && xy < stop) {
		    return client;
		}
		index = stop;
	    }
	}

    } else {
	TAILQ_FOREACH(client, &TaskClientList, TaskQueue) {
	    if (TaskShouldShowItem(client)) {
		stop = index + task_plugin->ItemHeight;
		if (xy >= index && xy < stop) {
		    return client;
		}
		index = stop;
	    }
	}
    }

    return NULL;
}

/**
**	Draw a specific task.
**
**	@param task_plugin	task plugin data
*/
static void TaskDraw(TaskPlugin * task_plugin)
{
    Panel *panel;
    Plugin *plugin;
    int n;
    int x;
    int y;
    unsigned width;
    unsigned height;
    unsigned item_width;
    int remainder;
    Label label;
    Client *client;
    char buf[128];

    plugin = task_plugin->Plugin;
    if (!(panel = plugin->Panel)) {
	Debug(0, "task not inside a panel\n");
	return;
    }

    PanelClearPluginBackground(plugin);

    if (!(n = TaskGetTaskCount())) {	// no tasks ready
	PanelUpdatePlugin(panel, plugin);
	return;
    }

    width = plugin->Width;
    height = plugin->Height;
    x = TASK_INNER_SPACE;
    width -= x;
    y = PANEL_INNER_SPACE;

    if (task_plugin->Layout == PANEL_LAYOUT_HORIZONTAL) {
	item_width = TaskGetItemWidth(task_plugin, n);
	remainder = width - item_width * n;
    } else {
	item_width = width;
	remainder = 0;
    }

#if 0
    // FIXME: global!
    IconSize = task_plugin->ItemHeight - 2 * TASK_INNER_SPACE - 4;
#endif

    LabelReset(&label, plugin->Pixmap, RootGC);
    label.Font = &Fonts.Task;

    TAILQ_FOREACH(client, &TaskClientList, TaskQueue) {
	if (TaskShouldShowItem(client)) {
	    if (client->State & WM_STATE_ACTIVE) {
		label.Type = LABEL_TASK_ACTIVE;
	    } else {
		label.Type = LABEL_TASK;
	    }

	    label.Width = item_width - TASK_INNER_SPACE - 1;
	    label.Height = task_plugin->ItemHeight - 1;
	    if (remainder) {
		if (task_plugin->Layout == PANEL_LAYOUT_HORIZONTAL) {
		    label.Width = item_width - TASK_INNER_SPACE;
		} else {
		    label.Height = task_plugin->ItemHeight;
		}
	    }

	    label.X = x;
	    label.Y = y;
#ifdef USE_ICON
	    label.Icon = client->Icon;
#endif

	    if (client->State & WM_STATE_MINIMIZED) {
		if (client->Name) {
		    snprintf(buf, sizeof(buf), "[%s]", client->Name);
		    label.Text = buf;
		} else {
		    label.Text = "[]";
		}
	    } else {
		label.Text = client->Name;
	    }
	    LabelDraw(&label);

	    if (client->State & WM_STATE_MINIMIZED) {
		uint32_t values[4];
		xcb_rectangle_t rectangle;

		values[0] = Colors.TaskFG.Pixel;
		values[1] = x + 3;
		values[2] =
		    y + task_plugin->ItemHeight - TASK_MINIMIZED_HEIGHT - 3;
		values[3] = MinimizedPixmap;
		xcb_change_gc(Connection, RootGC,
		    XCB_GC_FOREGROUND | XCB_GC_CLIP_ORIGIN_X |
		    XCB_GC_CLIP_ORIGIN_Y | XCB_GC_CLIP_MASK, values);
		rectangle.x = x + 3;
		rectangle.y =
		    y + task_plugin->ItemHeight - TASK_MINIMIZED_HEIGHT - 3;
		rectangle.width = TASK_MINIMIZED_WIDTH;
		rectangle.height = TASK_MINIMIZED_HEIGHT;
		xcb_poly_fill_rectangle(Connection, plugin->Pixmap, RootGC, 1,
		    &rectangle);
		values[0] = XCB_NONE;
		xcb_change_gc(Connection, RootGC, XCB_GC_CLIP_MASK, values);
	    }

	    if (task_plugin->Layout == PANEL_LAYOUT_HORIZONTAL) {
		x += item_width;
		if (remainder) {
		    ++x;
		    --remainder;
		}
	    } else {
		y += task_plugin->ItemHeight;
		if (remainder) {
		    ++y;
		    --remainder;
		}
	    }
	}
    }
    PanelUpdatePlugin(panel, plugin);
}

/**
**	Update all task plugin(s).
**
**	@todo write dynamic horizontal layout.
*/
void TaskUpdate(void)
{
    TaskPlugin *task_plugin;

    if (!KeepLooping) {			// no work if exiting
	return;
    }
#ifdef DEBUG
    if (SLIST_EMPTY(&Tasks)) {
	Debug(0, "FIXME: tasks missing init %s\n", __FUNCTION__);
    }
#endif
    if (!ClientLayers[0].tqh_first && !ClientLayers[0].tqh_last) {
	Debug(0, "FIXME: clients missing init %s\n", __FUNCTION__);
	return;
    }

    SLIST_FOREACH(task_plugin, &Tasks, Next) {
	// keep task variable or dynamic sized
	if (task_plugin->DynamicSize) {
	    if (task_plugin->Layout == PANEL_LAYOUT_VERTICAL) {
		int last_height;

		last_height = task_plugin->Plugin->RequestedHeight;
		task_plugin->Plugin->RequestedHeight =
		    (Fonts.Task.Height + 12) * TaskGetTaskCount() + 2;
		if (last_height != task_plugin->Plugin->RequestedHeight) {
		    PanelResize(task_plugin->Plugin->Panel);
		}
	    } else {
		Debug(0, "FIXME: %s for horizontal not yet written\n",
		    __FUNCTION__);
	    }
	}
	TaskDraw(task_plugin);
    }
}

/**
**	Add a client to task(s).
**
**	@param client	client to add
*/
void TaskAddClient(Client * client)
{
    if (TaskInsertMode == TASK_INSERT_RIGHT) {
	TAILQ_INSERT_TAIL(&TaskClientList, client, TaskQueue);
    } else {
	TAILQ_INSERT_HEAD(&TaskClientList, client, TaskQueue);
    }

    TaskUpdate();
    HintSetNetClientList();
}

/**
**	Remove a client from task(s).
**
**	@param client	client to remove
*/
void TaskDelClient(Client * client)
{
    TAILQ_REMOVE(&TaskClientList, client, TaskQueue);

    TaskUpdate();
    HintSetNetClientList();
}

// ------------------------------------------------------------------------ //
// Callbacks

/**
**	Initialize a task panel plugin.
**
**	@param plugin	panel plugin
*/
static void TaskCreate(Plugin * plugin)
{
    TaskPlugin *task_plugin;

    // create pixmap
    PanelPluginCreatePixmap(plugin);
    // clear pixmap
    PanelClearPluginBackground(plugin);

    task_plugin = plugin->Object;
    if (task_plugin->Layout == PANEL_LAYOUT_HORIZONTAL) {
	task_plugin->ItemHeight = plugin->Height - TASK_INNER_SPACE;
    } else {
	task_plugin->ItemHeight =
	    Fonts.Task.Height + LABEL_INNER_SPACE * 2 + LABEL_BORDER +
	    TASK_INNER_SPACE;
    }
}

/**
**	Set size of a task panel plugin.
**
**	@param plugin	common panel plugin data of task list
**	@param width	width of plugin
**	@param height	height of plugin
*/
static void TaskSetSize(Plugin * plugin, unsigned width, unsigned height)
{
    TaskPlugin *task_plugin;

    task_plugin = plugin->Object;

    if (!width) {
	task_plugin->Layout = PANEL_LAYOUT_HORIZONTAL;
    } else if (!height) {
	task_plugin->Layout = PANEL_LAYOUT_VERTICAL;
    } else if (width > height) {
	task_plugin->Layout = PANEL_LAYOUT_HORIZONTAL;
    } else {
	task_plugin->Layout = PANEL_LAYOUT_VERTICAL;
    }
}

/**
**	Resize a task panel plugin.
**
**	@param plugin	panel plugin
**
**	@todo use default resize plugin.
*/
void TaskResize(Plugin * plugin)
{
    // free old pixmap
    PanelPluginDeletePixmap(plugin);
    // create new size pixmap
    TaskCreate(plugin);
}

/**
**	Handle a task list button event.
**
**	@param plugin	common panel plugin data
**	@param x	x-coordinate of button press
**	@param y	y-coordinate of button press
**	@param mask	button mask
**
**	@todo make buttons on list configurable
*/
static void TaskHandleButtonPress(Plugin * plugin, int x, int y, int mask)
{
    TaskPlugin *task_plugin;
    Client *client;

    task_plugin = plugin->Object;
    if (task_plugin->Layout == PANEL_LAYOUT_HORIZONTAL) {
	client = TaskGetClient(task_plugin, x);
    } else {
	client = TaskGetClient(task_plugin, y);
    }

    if (client) {
	switch (mask) {
	    case XCB_BUTTON_INDEX_1:
		if (client->State & WM_STATE_ACTIVE
		    && TAILQ_FIRST(&ClientLayers[client->OnLayer]) == client) {
		    ClientMinimize(client);
		} else {
		    ClientRestore(client, 1);
		    ClientFocus(client);
		}
		break;
	    case XCB_BUTTON_INDEX_2:
		if (client->State & WM_STATE_SHADED) {
		    ClientUnshade(client);
		} else {
		    ClientShade(client);
		}
		break;
	    case XCB_BUTTON_INDEX_3:
		TaskShowWindowMenu(task_plugin, client);
		break;
	    case XCB_BUTTON_INDEX_4:
		TaskFocusPrevious();
		break;
	    case XCB_BUTTON_INDEX_5:
		TaskFocusNext();
		break;
	    default:
		break;
	}
    }
}

/**
**	Show tooltip of task list panel plugin.
**
**	@param plugin	common panel plugin data of task list
**	@param x	current mouse x-coordinate
**	@param y	current mouse y-coordinate
*/
static void TaskTooltip(Plugin * plugin, int x, int y)
{
    TaskPlugin *task_plugin;
    Client *client;

    task_plugin = plugin->Object;
    if (task_plugin->Layout == PANEL_LAYOUT_HORIZONTAL) {
	client = TaskGetClient(task_plugin, x - task_plugin->Plugin->ScreenX);
    } else {
	client = TaskGetClient(task_plugin, y - task_plugin->Plugin->ScreenY);
    }
    if (client) {
	TooltipShow(x, y, client->Name);
    }
}

// ---------------------------------------------------------------------------

/**
**	Initialize task panel plugin data.
*/
void TaskInit(void)
{
    MinimizedPixmap =
	xcb_create_pixmap_from_bitmap_data(Connection, RootWindow,
	(uint8_t *) MinimizedBitmap, TASK_MINIMIZED_WIDTH,
	TASK_MINIMIZED_HEIGHT, 1, 1, 0, NULL);
}

/**
**	Cleanup task panel plugin.
*/
void TaskExit(void)
{
    TaskPlugin *task_plugin;

    while (!SLIST_EMPTY(&Tasks)) {	// list deletion
	task_plugin = SLIST_FIRST(&Tasks);

	SLIST_REMOVE_HEAD(&Tasks, Next);
	free(task_plugin);
    }

    xcb_free_pixmap(Connection, MinimizedPixmap);
    MinimizedPixmap = XCB_NONE;
}

/**
**	@ingroup hints
**
**	Maintain _NET_CLIENT_LIST[_STACKING] properties of root window.
**
**	@todo FIXME: belongs into hints
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
    if (TaskInsertMode == TASK_INSERT_RIGHT) {
	TAILQ_FOREACH(client, &TaskClientList, TaskQueue) {
	    window[count++] = client->Window;
	}
    } else {
	TAILQ_FOREACH_REVERSE(client, &TaskClientList, _client_task_,
	    TaskQueue) {
	    window[count++] = client->Window;
	}
    }

    xcb_change_property(Connection, XCB_PROP_MODE_REPLACE, RootWindow,
	Atoms.NET_CLIENT_LIST.Atom, WINDOW, 32, count, window);

    // set _NET_CLIENT_LIST_STACKING
    // has bottom-to-top stacking order
    count = 0;
    for (layer = LAYER_TOP; layer >= LAYER_BOTTOM; --layer) {
	TAILQ_FOREACH(client, &ClientLayers[layer], LayerQueue) {
	    window[count++] = client->Window;
	}
    }

    xcb_change_property(Connection, XCB_PROP_MODE_REPLACE, RootWindow,
	Atoms.NET_CLIENT_LIST_STACKING.Atom, WINDOW, 32, count, window);
}

// ------------------------------------------------------------------------ //
// Config

#ifdef USE_LUA

/**
**	Set insertion mode for tasks.
**
**	@param mode	insertion mode (either right or left)
*/
void TaskSetInsertMode(const char *mode)
{
    if (!mode) {
	TaskInsertMode = TASK_INSERT_RIGHT;
	return;
    }

    if (!strcasecmp(mode, "right")) {
	TaskInsertMode = TASK_INSERT_RIGHT;
    } else if (!strcasecmp(mode, "left")) {
	TaskInsertMode = TASK_INSERT_LEFT;
    } else {
	Warning("invalid insert mode: \"%s\"", mode);
	TaskInsertMode = TASK_INSERT_RIGHT;
    }

}

/**
**	Create a new task panel plugin.
**
**	@returns common panel plugin data of task list
*/
Plugin *TaskNew(void)
{
    Plugin *plugin;
    TaskPlugin *task_plugin;

    task_plugin = calloc(1, sizeof(*task_plugin));
    SLIST_INSERT_HEAD(&Tasks, task_plugin, Next);

    task_plugin->Layout = PANEL_LAYOUT_HORIZONTAL;
    task_plugin->MaxItemWidth = UINT16_MAX;

    plugin = PanelPluginNew();
    plugin->Object = task_plugin;
    task_plugin->Plugin = plugin;

    plugin->Create = TaskCreate;
    plugin->Delete = PanelPluginDeletePixmap;
    plugin->SetSize = TaskSetSize;
    plugin->Resize = TaskResize;
    plugin->Tooltip = TaskTooltip;
    plugin->HandleButtonPress = TaskHandleButtonPress;

    return plugin;
}

/**
**	Set maximum width of an item in task.
**
**	@param plugin	common panel plugin data of task list
**	@param value	maximum width
*/
void TaskSetMaxItemWidth(Plugin * plugin, int value)
{
    if (value < 0) {
	Warning("invalid maxwidth for task item: %d\n", value);
	return;
    }
    ((TaskPlugin *) plugin->Object)->MaxItemWidth = value;
}

#else

/**
**	Create a new task panel plugin from config data.
**
**	@param array	configuration array for task panel plugin
**
**	@returns created task panel plugin.
*/
Plugin *TaskConfig(const ConfigObject * array)
{
    Plugin *plugin;
    TaskPlugin *task_plugin;
    const char *sval;
    ssize_t ival;

    task_plugin = calloc(1, sizeof(*task_plugin));
    SLIST_INSERT_HEAD(&Tasks, task_plugin, Next);

    if (ConfigGetString(array, &sval, "insert-mode", NULL)) {
	if (!strcasecmp(sval, "right")) {
	    TaskInsertMode = TASK_INSERT_RIGHT;
	} else if (!strcasecmp(sval, "left")) {
	    TaskInsertMode = TASK_INSERT_LEFT;
	} else {
	    Warning("invalid insert mode: \"%s\"", sval);
	    TaskInsertMode = TASK_INSERT_LEFT;
	}
    }
    if (ConfigGetInteger(array, &ival, "max-item-width", NULL)) {
	task_plugin->MaxItemWidth = ival;
    } else {
	task_plugin->MaxItemWidth = UINT16_MAX;
    }
    task_plugin->Layout = PANEL_LAYOUT_HORIZONTAL;

    plugin = PanelPluginNew();
    plugin->Object = task_plugin;
    task_plugin->Plugin = plugin;

    plugin->Create = TaskCreate;
    plugin->Delete = PanelPluginDeletePixmap;
    plugin->SetSize = TaskSetSize;
    plugin->Resize = TaskResize;
    plugin->Tooltip = TaskTooltip;
    plugin->HandleButtonPress = TaskHandleButtonPress;

    return plugin;
}

#endif

#endif // } USE_TASK

/// @}
