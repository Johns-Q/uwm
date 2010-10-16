///
///	@file netload.c @brief netload panel plugin functions.
///
///	Copyright (c) 2010 by Lutz Sammer.  All Rights Reserved.
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
///	@defgroup netload_plugin	The netload panel plugin
///
///	This module add netloads to the panel. This module is only available
///	if compiled with #USE_NETLOAD.
///
///	The netload is shown as graph in the panel.  Commands (external or
///	window manager) can be executed on pointer button click.
///
///	The tooltip show the average RX/TX traffic and the maximal traffic.
///
/// @{

#include <xcb/xcb.h>
#include "uwm.h"

#ifdef USE_NETLOAD			// {

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/queue.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include <xcb/xcb_icccm.h>

#include "core-array/core-array.h"
#include "core-rc/core-rc.h"

#include "draw.h"
#include "tooltip.h"
#include "client.h"

#include "image.h"
#include "icon.h"
#include "menu.h"

#include "panel.h"
#include "plugin/netload.h"

#define NETLOAD_INNER_SPACE	2	///< space around graph

/**
**	Netload plugin typedef.
*/
typedef struct _netload_plugin_ NetloadPlugin;

/**
**	Structure with netload panel plugin private data.
*/
struct _netload_plugin_
{
    SLIST_ENTRY(_netload_plugin_) Next;	///< singly-linked netload list
    Plugin *Plugin;			///< common plugin data

    const char *Interface;		///< network interface to monitor

    uint32_t *History;			///< rx/tx bytes history
    uint32_t AverageTX;			///< average tx
    uint32_t AverageRX;			///< average rx
    uint32_t LastTX;			///< last tx
    uint32_t LastRX;			///< last rx
    uint32_t MaxTX;			///< max tx
    uint32_t MaxRX;			///< max rx

    MenuButton *Buttons;		///< commands to run on click
};

    /// Netload plugin list head structure
SLIST_HEAD(_netload_head_, _netload_plugin_);

    /// list of all netloads of the plugin
static struct _netload_head_ Netloads = SLIST_HEAD_INITIALIZER(Netloads);

static uint32_t NetloadLastUpdateTick;	///< tick of last netload update

// ------------------------------------------------------------------------ //
// Proc

#define PROC_NET_DEV	"/proc/net/dev"	///< network statistics

/**
**	Collect network data.
*/
static void ProcReadNet(void)
{
    int fd;
    char buf[8192];
    int n;
    unsigned long rx;
    unsigned long tx;
    NetloadPlugin *netload_plugin;

    fd = open(PROC_NET_DEV, O_RDONLY);
    if (fd < 0) {
	return;
    }
    n = read(fd, buf, sizeof(buf));
    if (n > 0) {
	const char *s;

	buf[n] = '\0';
	if (!(s = strchr(buf, '\n'))) {	// skip first line
	    close(fd);
	    return;
	}
	++s;
	for (;;) {
	    char name[512];
	    int n;

	    if (!(s = strchr(s, '\n'))) {	// skip to end of line
		break;
	    }
	    ++s;

	    rx = 0;
	    tx = 0;
	    n = sscanf(s, "%s %lu %*s %*s %*s %*s %*s %*s %*s %lu", name, &rx,
		&tx);
	    if (n != 3) {
		continue;
	    }
	    n = strlen(name);		// remove trailing :
	    if (name[n - 1] == ':') {
		name[n - 1] = '\0';
	    }
	    Debug(3, "%s %d %lu %lu\n", name, n, rx, tx);

	    //
	    //	search plugin
	    //
	    SLIST_FOREACH(netload_plugin, &Netloads, Next) {
		//
		//	interface not specified, use first usefull network
		//	device.
		//
		if (!netload_plugin->Interface) {
		    if (!strncmp(name, "lo", 3) || !strncmp(name, "dummy", 5)
			|| !strncmp(name, "irda", 4)) {
			continue;
		    }
		    Debug(0, "found %s\n", name);
		    netload_plugin->Interface = strdup(name);
		}
		//
		//	there can be multiple plugins with the same interface
		//
		if (!strcmp(name, netload_plugin->Interface)) {
		    int size;
		    unsigned delta_rx;
		    unsigned delta_tx;

		    Debug(3, "found plugin %s\n", name);

		    // round about
		    if (rx < netload_plugin->LastRX) {
			netload_plugin->LastRX = rx;
		    }
		    if (tx < netload_plugin->LastTX) {
			netload_plugin->LastTX = tx;
		    }
		    // add values to history
		    size =
			netload_plugin->Plugin->Width -
			NETLOAD_INNER_SPACE * 2;
		    delta_rx = rx - netload_plugin->LastRX;
		    delta_tx = tx - netload_plugin->LastTX;
		    netload_plugin->History[size * 2 - 2] = delta_rx;
		    netload_plugin->History[size * 2 - 1] = delta_tx;

		    // greatest seen value
		    if (netload_plugin->MaxRX < delta_rx) {
			netload_plugin->MaxRX = delta_rx;
		    }
		    if (netload_plugin->MaxTX < delta_tx) {
			netload_plugin->MaxTX = delta_tx;
		    }
		    // average value
		    netload_plugin->AverageRX =
			(netload_plugin->AverageRX + delta_rx) / 2;
		    netload_plugin->AverageTX =
			(netload_plugin->AverageTX + delta_tx) / 2;

		    // last value for next round
		    netload_plugin->LastRX = rx;
		    netload_plugin->LastTX = tx;
		}
	    }
	}
	close(fd);
    }
}

/**
**	Collect the network statistics.
*/
void NetloadCollect(void)
{
    ProcReadNet();
}

// ------------------------------------------------------------------------ //
// Draw

/**
**	Draw a netload panel plugin.
**
**	@param netload_plugin	netload plugin private data
*/
static void NetloadDraw(NetloadPlugin * netload_plugin)
{
    Plugin *plugin;
    Panel *panel;
    int x;
    unsigned size;

    plugin = netload_plugin->Plugin;
    if (!(panel = plugin->Panel)) {
	Debug(2, "netload not inside a panel\n");
	return;
    }

    size = plugin->Height - NETLOAD_INNER_SPACE * 2;
    for (x = 0; x < plugin->Width - NETLOAD_INNER_SPACE * 2; ++x) {
	xcb_point_t points[2];
	unsigned rx;
	unsigned tx;
	unsigned u;

	rx = netload_plugin->History[x * 2 + 0];
	tx = netload_plugin->History[x * 2 + 1];

	// fit tx + rx into area
	u = netload_plugin->MaxRX + netload_plugin->MaxTX;
	rx = ((unsigned long)rx * size) / u;
	tx = ((unsigned long)tx * size) / u;

	rx = MIN(size, rx);
	tx = MIN(size, tx);

	points[0].x = x + NETLOAD_INNER_SPACE;
	points[1].x = x + NETLOAD_INNER_SPACE;

	if (rx + tx < size) {		// no overlap clear
	    xcb_change_gc(Connection, RootGC, XCB_GC_FOREGROUND,
		&Colors.NetloadBG.Pixel);
	    points[0].y = NETLOAD_INNER_SPACE + tx;
	    points[1].y = size - rx;
	    xcb_poly_line(Connection, XCB_COORD_MODE_ORIGIN, plugin->Pixmap,
		RootGC, 2, points);
	}

	if (tx) {
	    xcb_change_gc(Connection, RootGC, XCB_GC_FOREGROUND,
		&Colors.NetloadTX.Pixel);
	    points[0].y = NETLOAD_INNER_SPACE;
	    points[1].y = NETLOAD_INNER_SPACE + tx;
	    xcb_poly_line(Connection, XCB_COORD_MODE_ORIGIN, plugin->Pixmap,
		RootGC, 2, points);
	}

	if (rx) {
	    xcb_change_gc(Connection, RootGC, XCB_GC_FOREGROUND,
		&Colors.NetloadRX.Pixel);
	    points[0].y = size - rx;
	    points[1].y = size;
	    xcb_poly_line(Connection, XCB_COORD_MODE_ORIGIN, plugin->Pixmap,
		RootGC, 2, points);
	}
    }

    PanelUpdatePlugin(panel, plugin);
}

// ------------------------------------------------------------------------ //
// Callbacks

/**
**	Create/initialize a netload panel plugin.
**
**	@param plugin	common panel plugin data of netload
*/
static void NetloadCreate(Plugin * plugin)
{
    NetloadPlugin *netload_plugin;
    int size;

    // create pixmap
    PanelPluginCreatePixmap(plugin);
    // clear the background
    PanelClearPluginBackgroundWithColor(plugin, &Colors.NetloadBG.Pixel);

    netload_plugin = plugin->Object;
    Debug(3, "width %d\n", plugin->Width);

    // reallocate history buffer
    size = plugin->Width - NETLOAD_INNER_SPACE * 2;
    free(netload_plugin->History);
    netload_plugin->History =
	calloc(sizeof(*netload_plugin->History), 2 * size);
    if (!netload_plugin->LastRX) {
	netload_plugin->LastRX = UINT32_MAX;
	netload_plugin->LastTX = UINT32_MAX;
    }
    if (netload_plugin->MaxRX < size) {
	netload_plugin->MaxRX = size;
    }
    if (netload_plugin->MaxTX < size) {
	netload_plugin->MaxTX = size;
    }
    // FIXME: is redraw needed?
    // NetloadDraw(netload_plugin);
}

/**
**	Resize a netload panel plugin.
**
**	@param plugin	common panel plugin data of netload
*/
static void NetloadResize(Plugin * plugin)
{
    PanelPluginDeletePixmap(plugin);
    NetloadCreate(plugin);
}

/**
**	Handle a click event on a netload panel plugin.
**
**	Runs command associated with netload.
**
**	@param plugin	common panel plugin data of netload
**	@param x	x-coordinate of button press
**	@param y	y-coordinate of button press
**	@param mask	button mask
*/
static void NetloadHandleButtonPress(Plugin * plugin, int
    __attribute__ ((unused)) x, int __attribute__ ((unused)) y, int
    __attribute__ ((unused)) mask)
{
    NetloadPlugin *netload_plugin;

    netload_plugin = plugin->Object;
    PanelExecuteButton(plugin, netload_plugin->Buttons, mask);
}

/**
**	Scale value.
**
**	@param value		value to scale
**	@param[out] scale	scale string
**
**	@returns scaled value.
*/
static uint32_t NetloadScale(uint32_t value, const char **scale)
{
    if (value > (1 << 30) - 1) {
	*scale = "GiB";
	return value >> 30;
    }
    if (value > (1 << 20) - 1) {
	*scale = "MiB";
	return value >> 20;
    }
    if (value > (1 << 10) - 1) {
	*scale = "KiB";
	return value >> 10;
    }
    *scale = " B ";
    return value;
}

/**
**	Show tooltip of netload panel plugin.
**
**	@param plugin	common panel plugin data of netload
**	@param x	current mouse x-coordinate
**	@param y	current mouse y-coordinate
*/
static void NetloadTooltip(Plugin * plugin, int x, int y)
{
    const NetloadPlugin *netload_plugin;
    char buf[128];
    uint32_t avg_rx;
    const char *avg_rx_scale;
    uint32_t avg_tx;
    const char *avg_tx_scale;
    uint32_t max_rx;
    const char *max_rx_scale;
    uint32_t max_tx;
    const char *max_tx_scale;

    netload_plugin = plugin->Object;
    avg_rx = NetloadScale(netload_plugin->AverageRX * 2, &avg_rx_scale);
    avg_tx = NetloadScale(netload_plugin->AverageTX * 2, &avg_tx_scale);
    max_rx = NetloadScale(netload_plugin->MaxRX * 2, &max_rx_scale);
    max_tx = NetloadScale(netload_plugin->MaxTX * 2, &max_tx_scale);

    snprintf(buf, sizeof(buf), "%s: rx:%4u%s<%4u%s/s tx:%4u%s<%4u%s/s",
	netload_plugin->Interface, avg_rx, avg_rx_scale, max_rx, max_rx_scale,
	avg_tx, avg_tx_scale, max_tx, max_tx_scale);
    TooltipShow(x, y, buf);
}

/**
**	Netload panel plugin timeout method.
**
**	@param plugin	common panel plugin data of netload
**	@param tick	current tick in ms
**	@param x	current mouse x-coordinate
**	@param y	current mouse y-coordinate
*/
static void NetloadTimeout(Plugin
    __attribute__ ((unused)) * plugin, uint32_t tick, int
    __attribute__ ((unused)) x, int __attribute__ ((unused)) y)
{
    // draw only every 500ms
    if (NetloadLastUpdateTick > tick || tick >= NetloadLastUpdateTick + 500) {
	NetloadPlugin *netload_plugin;

	NetloadLastUpdateTick = tick;
	NetloadCollect();
	// all netloads are redrawn by first plugin
	SLIST_FOREACH(netload_plugin, &Netloads, Next) {
	    int size;

	    NetloadDraw(netload_plugin);
	    size = netload_plugin->Plugin->Width - NETLOAD_INNER_SPACE * 2;
	    memmove(netload_plugin->History, netload_plugin->History + 2,
		(size - 1) * sizeof(*netload_plugin->History) * 2);

	}
    }
}

// ------------------------------------------------------------------------ //

/**
**	Initialize netload panel plugin.
*/
void NetloadInit(void)
{
    NetloadPlugin *netload_plugin;
    time_t now;

    time(&now);
    SLIST_FOREACH(netload_plugin, &Netloads, Next) {
	Plugin *plugin;
	unsigned width;
	unsigned height;

	plugin = netload_plugin->Plugin;
	width = 56;
	height = 16;

	if (!plugin->RequestedWidth) {
	    plugin->RequestedWidth = width + 2 * NETLOAD_INNER_SPACE;
	}
	if (!plugin->RequestedHeight) {
	    plugin->RequestedHeight = height + 2 * NETLOAD_INNER_SPACE;
	}
    }
}

/**
**	Cleanup netload panel plugin.
*/
void NetloadExit(void)
{
    NetloadPlugin *netload_plugin;

    while (!SLIST_EMPTY(&Netloads)) {	// list deletion
	netload_plugin = SLIST_FIRST(&Netloads);

	free(netload_plugin->Interface);
	free(netload_plugin->History);

	MenuButtonDel(netload_plugin->Buttons);

	SLIST_REMOVE_HEAD(&Netloads, Next);
	free(netload_plugin);
    }
}

// ------------------------------------------------------------------------ //
// Config

#ifdef USE_RC				// {

/**
**	Create a new netload panel plugin from config data.
**
**	@param array	configuration array for netload panel plugin
**
**	@returns created netload panel plugin.
*/
Plugin *NetloadConfig(const ConfigObject * array)
{
    Plugin *plugin;
    NetloadPlugin *netload_plugin;
    const char *sval;

    netload_plugin = calloc(1, sizeof(*netload_plugin));
    SLIST_INSERT_HEAD(&Netloads, netload_plugin, Next);

    // user specified network interface
    if (ConfigGetString(array, &sval, "interface", NULL)) {
	netload_plugin->Interface = strdup(sval);
    }
    // common config of pointer buttons to commands
    MenuButtonsConfig(array, &netload_plugin->Buttons);

    plugin = PanelPluginNew();
    plugin->Object = netload_plugin;
    netload_plugin->Plugin = plugin;

    // common config of plugin size
    PanelPluginConfigSize(array, plugin);

    plugin->Create = NetloadCreate;
    plugin->Delete = PanelPluginDeletePixmap;
    plugin->Resize = NetloadResize;
    plugin->Tooltip = NetloadTooltip;
    plugin->HandleButtonPress = NetloadHandleButtonPress;
    plugin->Timeout = NetloadTimeout;

    return plugin;
}

#endif // } USE_RC

#endif // } USE_NETLOAD

/// @}
