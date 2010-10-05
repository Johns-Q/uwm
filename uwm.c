///
///	@file uwm.c	@brief µwm µ window manager main file
///
///	Copyright (c) 2009, 2010 by Lutz Sammer.  All Rights Reserved.
///
///	Contributor(s):
///		based on jwm from Joe Wingbermuehle
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

/**
**	@mainpage
**
**	@section about
**
**	µwm is a lightweight stacking window manager for the X11 Window System
**	and is written in C and uses only libxcb at a minimum.	It has builtin
**	menus/panels/buttons and other plugins.
**
**	@section overview
**
**	- stacking window manager
**	- written in C (with many comments)
**	- less dependencies (only XCB and X11 required)
**	- doesn't use/need GNU autoconfigure and other auto-tools
**	- executable less than 200k
**	- less than 20k Source Lines of Code
**	- low memory footprint during runtime ~ 1800k RES
**	- low X11 resource usage ~ 200k (without desktop backgrounds)
**	- low CPU usage
**
**	@section features
**
**	- configurable and themeable
**	- no XML config file
**	- builtin menu(s)
**	- builtin panel(s) (other names are slit/bar/dock) with:
**		- button
**		- pager
**		- task-list
**		- swallow (dock)
**		- systray (planned)
**		- clock
**	- builtin tooltips
**	- builtin background setter
**	- composite support with xcompmgr (sample X compositing manager)
**	- multiple desktops
**	- multiple screen (xinerama)
**	- 64-bit and 32-bit clean
**	- little-endian and big-endian clean
**	- many features can be compile time enabled / disabled
**
**	see <a href="modules.html">µwm list of modules</a>.
*/

///
///	@defgroup uwm The uwm main module.
///
///	This module contains main, init, config and exit functions.
///
/// @{

#include <xcb/xcb.h>
#include "uwm.h"

#include <sys/queue.h>
#include <sys/wait.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <xcb/xcb_atom.h>
#include <xcb/xcb_aux.h>
#include <xcb/xcb_icccm.h>
#ifdef USE_SHAPE
#include <xcb/shape.h>
#endif
#ifdef USE_RENDER
#include <xcb/xcb_renderutil.h>
#include <xcb/render.h>
#endif

#include "core-array/core-array.h"
#include "core-rc/core-rc.h"

#include "misc.h"
#include "event.h"
#include "property.h"
#include "command.h"
#include "draw.h"
#include "image.h"
#include "screen.h"
#include "pointer.h"
#include "client.h"
#include "border.h"
#include "moveresize.h"
#include "keyboard.h"
#include "hints.h"
#include "tooltip.h"
#include "icon.h"
#include "menu.h"
#include "background.h"
#include "desktop.h"
#include "rule.h"

#include "panel.h"
#include "plugin/button.h"
#include "plugin/clock.h"
#include "plugin/pager.h"
#include "plugin/swallow.h"
#include "plugin/systray.h"
#include "plugin/task.h"

#include "dia.h"
#include "td.h"

// ------------------------------------------------------------------------ //
// Variables
// ------------------------------------------------------------------------ //

#ifdef DEBUG
int DebugLevel = 3;			///< debug level
#endif

const char *DisplayString;		///< X11 display string
xcb_connection_t *Connection;		///< connection to X11 server
xcb_screen_t *XcbScreen;		///< our screen

// FIXME: this is all in XcbScreen->
xcb_window_t RootWindow;		///< our root window
unsigned RootDepth;			///< root window depth

// Prepared
xcb_gcontext_t RootGC;			///< root graphic context
xcb_visualtype_t *RootVisualType;	///< root visual type

#ifdef USE_SHAPE
int HaveShape;				///< shape extension found
int ShapeEvent;				///< first shape event code
#endif
#ifdef USE_RENDER
int HaveRender;				///< xrender extension found
#endif

char KeepRunning;			///< keep running
char KeepLooping;			///< keep looping

//static char Initializing;		///< init state

// ------------------------------------------------------------------------ //

/**
**	Redraw borders on current desktop.
**
**	This should be done after loading clients since stacking order
**	may cause borders on current desktop to become visible after moving
**	clients to their assigned desktops.
**
**	@todo need to draw only clients on current desktop
*/
static void RedrawCurrentDesktop(void)
{
    Client *client;
    int layer;

    for (layer = LAYER_BOTTOM; layer < LAYER_MAX; layer++) {
	TAILQ_FOREACH(client, &ClientLayers[layer], LayerQueue) {
	    if (!(client->State & (WM_STATE_HIDDEN | WM_STATE_MINIMIZED))) {
		BorderDraw(client, NULL);
	    }
	}
    }
}

/**
**	Initialize modules.
*/
static void ModulesInit(void)
{
    xcb_get_property_cookie_t net_current_desktop_cookie;

    KeepLooping = 1;
//    Initializing = 1;

    CommandInit();			// must be first (runs programs)
    KeepRunning = 0;			// needed by command init for restart

    AtomPreInit();			// send requests early
    PointerPreInit();
    ClientPreInit();

    AtomInit();
    net_current_desktop_cookie = HintNetCurrentDesktopRequest();

    ColorInit();
    IconInit();
    BackgroundInit();			// needs atom + color + icon
    FontInit();
    BorderInit();
    ScreenInit();
    PointerInit();
    KeyboardInit();
    DesktopInit();
    DialogInit();
    RuleInit();
    //Hints();
    //StatusInit();
    OutlineInit();
    PlacementInit();
    TooltipInit();
    RootMenuInit();
    ClockInit();
    PanelButtonInit();
    PagerInit();			// need client
    TaskInit();				// need client
    SystrayInit();
    PanelInit();
    ClientInit();			// need task, pager
    SwallowInit();			// needs client

//    Initializing = 0;

    PointerSetDefaultCursor(RootWindow);
    DesktopCurrent = -1;		// force update
    HintGetNetCurrentDesktop(net_current_desktop_cookie);

    // allow clients to do their thing
    xcb_aux_sync(Connection);
    xcb_ungrab_server(Connection);

    // draw all panels
    PanelsDraw();

    // draw border of all clients on current desktop
    RedrawCurrentDesktop();
}

/**
**	Cleanup modules.
*/
static void ModulesExit(void)
{
    BorderExit();
    ClientExit();
    DesktopExit();
    DialogExit();
    FontExit();
    RuleExit();
    // Hints();
    OutlineExit();
    //StatusExit();
    PlacementExit();
    TooltipExit();

    PanelExit();			// panel exit befor plugin exit
    SystrayExit();
    SwallowExit();
    TaskExit();
    PagerExit();
    PanelButtonExit();
    ClockExit();

    RootMenuExit();
    IconExit();				// menu/panel frees icons self

    ScreenExit();
    BackgroundExit();
    KeyboardExit();
    PointerExit();
    AtomExit();
    ColorExit();

    CommandExit();			// must be last (runs programs)
}

// ------------------------------------------------------------------------ //
// Config

#ifdef USE_LUA

/**
**	Set focus model.
**
**	@param model	focus model keyword
*/
void SetFocusModel(const char *model)
{
    FocusModus = FOCUS_SLOPPY;
    if (!model || !strcasecmp(model, "sloppy")) {
    } else if (!strcasecmp(model, "click")) {
	FocusModus = FOCUS_CLICK;
    } else {
	Warning("invalid focus model: '%s'\n", model);
    }
}

#else

/**
**	Parse gravity.
**
**	@param keyword	gravity type (static, north, ...)
**	@param error	error info for failure message
**
**	@returns PanelGravity for given keyword.
**
**	@todo can move into rule module.
**	@todo PANEL_GRAVITY_... can become GRAVITY_...
*/
int ParseGravity(const char *keyword, const char *error)
{
    if (!strcasecmp(keyword, "static")) {
	return PANEL_GRAVITY_STATIC;
    } else if (!strcasecmp(keyword, "north")) {
	return PANEL_GRAVITY_NORTH;
    } else if (!strcasecmp(keyword, "south")) {
	return PANEL_GRAVITY_SOUTH;
    } else if (!strcasecmp(keyword, "west")) {
	return PANEL_GRAVITY_WEST;
    } else if (!strcasecmp(keyword, "east")) {
	return PANEL_GRAVITY_EAST;
    } else if (!strcasecmp(keyword, "center")) {
	return PANEL_GRAVITY_CENTER;
    } else if (!strcasecmp(keyword, "north-west")) {
	return PANEL_GRAVITY_NORTH_WEST;
    } else if (!strcasecmp(keyword, "north-east")) {
	return PANEL_GRAVITY_NORTH_EAST;
    } else if (!strcasecmp(keyword, "south-west")) {
	return PANEL_GRAVITY_SOUTH_WEST;
    } else if (!strcasecmp(keyword, "south-east")) {
	return PANEL_GRAVITY_SOUTH_EAST;
    } else {
	Warning("invalid %s gravity: \"%s\"\n", error, keyword);
	return -1;
    }
}

/**
**	Get global configuration.
**
**	@param config	global config dictionary
*/
void GlobalConfig(const Config * config)
{
    const char *sval;
    ssize_t ival;

    Debug(2, "%s: FIXME:\n", __FUNCTION__);
    // FIXME: global configs Placement Snap DoubleClick ...

    if (ConfigGetString(ConfigDict(config), &sval, "focus-model", NULL)) {
	if (!strcasecmp(sval, "sloppy")) {
	    FocusModus = FOCUS_SLOPPY;
	} else if (!strcasecmp(sval, "click")) {
	    FocusModus = FOCUS_CLICK;
	} else {
	    FocusModus = FOCUS_SLOPPY;
	    Warning("invalid focus model: '%s'\n", sval);
	}
    }
    // DoubleClick
    if (ConfigGetInteger(ConfigDict(config), &ival, "double-click", "delta",
	    NULL)) {
	if (DOUBLE_CLICK_MINIMAL_DELTA <= ival
	    && ival <= DOUBLE_CLICK_MAXIMAL_DELTA) {
	    DoubleClickDelta = ival;
	} else {
	    DoubleClickDelta = DOUBLE_CLICK_DEFAULT_DELTA;
	    Warning("double-click delta %zd out of range\n", ival);
	}
    }
    if (ConfigGetInteger(ConfigDict(config), &ival, "double-click", "speed",
	    NULL)) {
	if (DOUBLE_CLICK_MINIMAL_SPEED <= ival
	    && ival <= DOUBLE_CLICK_MAXIMAL_SPEED) {
	    DoubleClickSpeed = ival;
	} else {
	    DoubleClickSpeed = DOUBLE_CLICK_DEFAULT_SPEED;
	    Warning("double-click speed %zd out of range\n", ival);
	}
    }
}

#endif

// ------------------------------------------------------------------------ //

/**
**	Open connection to X server.
*/
static void ConnectionOpen(void)
{
    xcb_connection_t *connection;
    int screen_nr;
    const xcb_setup_t *setup;
    xcb_screen_iterator_t iter;
    int i;
    xcb_screen_t *screen;
    uint32_t mask;
    uint32_t values[3];

    // open connection to X server.
    // use DISPLAY environment variable as default display name
    connection = xcb_connect(DisplayString, &screen_nr);
    if (!connection || xcb_connection_has_error(connection)) {
	FatalError("Can't connect to X11 server on %s\n",
	    DisplayString ? DisplayString : getenv("DISPLAY"));
    }

    Debug(3, "Use screen %d\n", screen_nr);

    // get screen whose number is screen_nr
    setup = xcb_get_setup(connection);
    iter = xcb_setup_roots_iterator(setup);
    for (i = 0; i < screen_nr; ++i) {
	xcb_screen_next(&iter);
    }
    screen = iter.data;

    Connection = connection;
    XcbScreen = screen;
    // FIXME: should i really copy this?
    RootWindow = screen->root;
    RootDepth = screen->root_depth;

    // emulate DefaultGC
    RootGC = xcb_generate_id(connection);
    mask = XCB_GC_FOREGROUND | XCB_GC_BACKGROUND | XCB_GC_GRAPHICS_EXPOSURES;
    values[0] = screen->white_pixel;
    values[1] = screen->black_pixel;
    values[2] = 0;
    xcb_create_gc(connection, RootGC, RootWindow, mask, values);

    RootVisualType = xcb_aux_find_visual_by_id(screen, screen->root_visual);

#ifdef USE_COLORMAP
    colormapCount =
	MaxCmapsOfScreen(ScreenOfConnection(Connection, rootScreen));
#endif
}

/**
**	Close X server connection.
*/
static void ConnectionClose(void)
{
    xcb_flush(Connection);		// send out remaining commands

    xcb_disconnect(Connection);
    Connection = 0;
}

/**
**	Signal handler.
*/
static void SignalHandler(int __attribute__ ((unused)) signum)
{
    signal(SIGTERM, SignalHandler);
    signal(SIGINT, SignalHandler);
    signal(SIGHUP, SignalHandler);
    KeepLooping = 0;
}

/**
**	Initialize X server connection.
*/
static void ConnectionInit(void)
{
    const xcb_query_extension_reply_t *query_extension_reply;
    uint32_t value[1];

    ConnectionOpen();

    // grab server to delay any events until we enter event-loop
    xcb_grab_server(Connection);
    // prefetch extensions
#ifdef USE_SHAPE
    xcb_prefetch_extension_data(Connection, &xcb_shape_id);
#endif
#ifdef USE_RENDER
    xcb_prefetch_extension_data(Connection, &xcb_render_id);
#endif

    // Set events we want for root window.
    // Note that asking for SUBSTRUCTURE_REDIRECT will fail
    // if another window manager is already running.
    value[0] =
	XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_KEY_RELEASE |
	XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE |
	XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_POINTER_MOTION |
	XCB_EVENT_MASK_POINTER_MOTION_HINT | XCB_EVENT_MASK_STRUCTURE_NOTIFY |
	XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT | XCB_EVENT_MASK_PROPERTY_CHANGE |
	XCB_EVENT_MASK_COLOR_MAP_CHANGE;
    xcb_change_window_attributes(Connection, RootWindow, XCB_CW_EVENT_MASK,
	value);

    signal(SIGTERM, SignalHandler);
    signal(SIGINT, SignalHandler);
    signal(SIGHUP, SignalHandler);

#ifdef USE_SHAPE
    query_extension_reply = xcb_get_extension_data(Connection, &xcb_shape_id);
    if ((HaveShape = query_extension_reply->present)) {
	Debug(2, "shape extension enabled\n");

	// Shape notify 0 = window has changed its shape
	ShapeEvent = query_extension_reply->first_event;
	Debug(4, "shape event code %d\n", ShapeEvent);

    } else {
	Debug(2, "shape extension disabled\n");
    }
#endif
#ifdef USE_RENDER
    query_extension_reply = xcb_get_extension_data(Connection, &xcb_render_id);
    if ((HaveRender = query_extension_reply->present)) {
	Debug(2, "render extension enabled\n");
	if (RootDepth < 24) {
	    Warning("root depth is %d, icon alpha channel disabled\n",
		RootDepth);
	    HaveRender = 0;
	}
    } else {
	Debug(2, "render extension disabled\n");
    }
#endif

    //	Setup event handlers
    EventInit();

    //	Setup property handlers
    PropertyInit();
}

/**
**	Exit X server connection
*/
static void ConnectionExit(void)
{
    EventExit();
    PropertyExit();
#ifdef USE_RENDER
    xcb_render_util_disconnect(Connection);
#endif

    ConnectionClose();
}

// ------------------------------------------------------------------------ //
// LUA
// ------------------------------------------------------------------------ //

///
///	@defgroup lua The lua module.
///
///	This module handles loading the configuration.
///
///	This module is only available, if compiled with #USE_LUA.
///
/// @{

#ifdef USE_LUA				// {

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

/**
**	Version of us.
*/
const char *Version(void)
{
    return VERSION;
}

//
//	FUN with cpp, to make LUA->C api quick&dirty
//

    /// LUA->C: integer argument
#define LUA_ARG_I(idx) \
    luaL_checkint(L, idx)
    /// LUA->C: long argument
#define LUA_ARG_L(idx) \
    luaL_checknumber(L, idx)
    /// LUA->C: string argument
#define LUA_ARG_S(idx) \
    luaL_checkstring(L, idx)
    /// LUA->C: opt string argument
#define LUA_ARG_OS(idx) \
    luaL_optstring(L, idx, NULL)
    /// LUA->C: userdata argument
#define LUA_ARG_U(idx) \
    lua_touserdata(L, idx)

    /// LUA->C: void return
#define LUA_RET( x ) \
    x; return 0
    /// LUA->C: string return
#define LUA_RET_S( x ) \
    lua_pushstring(L, x); return 1
    /// LUA->C: userdata return
#define LUA_RET_U( x ) \
    lua_pushlightuserdata(L, x); return 1
    /// LUA->C: opt userdata return (FIXME: not working!)
#define LUA_RET_OU( x ) \
    lua_pushlightuserdata(L, x); return 1

    /// LUA->C: build glue function
#define LUA_TO_C_X(name, narg, args, ret) \
    static int Lua##name(lua_State *L) \
    { \
	Debug(4, "%s(%d)\n", #name, lua_gettop(L)); \
	if (lua_gettop(L) != narg) { \
	    return luaL_error(L, "Wrong number of arguments"); \
	} \
	ret( name args ); \
 \
    }

/// defines lua bindings 1 .. n arguments
//@{
#define LUA_TO_C_0(name, ret) \
    LUA_TO_C_X(name, 0, (), ret)
#define LUA_TO_C_1(name, a1, ret) \
    LUA_TO_C_X(name, 1, (a1(1)), ret)
#define LUA_TO_C_2(name, a1, a2, ret) \
    LUA_TO_C_X(name, 2, (a1(1), a2(2)), ret)
#define LUA_TO_C_3(name, a1, a2, a3, ret) \
    LUA_TO_C_X(name, 3, (a1(1), a2(2), a3(3)), ret)
#define LUA_TO_C_4(name, a1, a2, a3, a4, ret) \
    LUA_TO_C_X(name, 4, (a1(1), a2(2), a3(3), a4(4)), ret)
#define LUA_TO_C_5(name, a1, a2, a3, a4, a5, ret) \
    LUA_TO_C_X(name, 5, (a1(1), a2(2), a3(3), a4(4), a5(5)), ret)
#define LUA_TO_C_6(name, a1, a2, a3, a4, a5, a6, ret) \
    LUA_TO_C_X(name, 6, (a1(1), a2(2), a3(3), a4(4), a5(5), a6(6)), ret)
//@}

/*
**	General
*/
    /// lua -> C: Version.
LUA_TO_C_0(Version, LUA_RET_S);

/*
**	Command
*/
    /// lua -> C: CommandAddStartup.
LUA_TO_C_1(CommandAddStartup, LUA_ARG_S, LUA_RET);

    /// lua -> C: CommandAddRestart.
LUA_TO_C_1(CommandAddRestart, LUA_ARG_S, LUA_RET);
    /// lua -> C: CommandAddExiting.
LUA_TO_C_1(CommandAddExiting, LUA_ARG_S, LUA_RET);

/*
**	Color
*/
    /// lua -> C: ColorSet.
LUA_TO_C_2(ColorSet, LUA_ARG_S, LUA_ARG_S, LUA_RET);

#ifdef USE_BACKGROUND

/*
**	Background
*/
    /// lua -> C: BackgroundSet.
LUA_TO_C_3(BackgroundSet, LUA_ARG_I, LUA_ARG_S, LUA_ARG_S, LUA_RET);
#endif

/*
**	Font
*/
    /// lua -> C: FontSet.
LUA_TO_C_2(FontSet, LUA_ARG_S, LUA_ARG_S, LUA_RET);

#ifdef USE_ICON

/*
**	Icon
*/
    /// lua -> C: IconAddPath.
LUA_TO_C_1(IconAddPath, LUA_ARG_S, LUA_RET);
#endif

/*
**	Border
*/
    /// lua -> C: BorderSetTitleHeight
LUA_TO_C_1(BorderSetTitleHeight, LUA_ARG_I, LUA_RET);
    /// lua -> C: BorderSetWidth
LUA_TO_C_1(BorderSetWidth, LUA_ARG_I, LUA_RET);

/*
**	Tooltip
*/
    /// lua -> C: TooltipSetEnabled.
LUA_TO_C_1(TooltipSetEnabled, LUA_ARG_I, LUA_RET);
    /// lua -> C: TooltipSetDelay.
LUA_TO_C_1(TooltipSetDelay, LUA_ARG_I, LUA_RET);

/*
**	Menu
*/
    /// lua -> C: MenuNewItem.
LUA_TO_C_2(MenuNewItem, LUA_ARG_OS, LUA_ARG_OS, LUA_RET_U);
    /// lua -> C: MenuSetSubmenu.
LUA_TO_C_2(MenuSetSubmenu, LUA_ARG_U, LUA_ARG_U, LUA_RET);
    /// lua -> C: MenuSetAction.
LUA_TO_C_3(MenuSetAction, LUA_ARG_U, LUA_ARG_S, LUA_ARG_OS, LUA_RET);

    /// lua -> C: MenuInsertItem.
LUA_TO_C_2(MenuInsertItem, LUA_ARG_U, LUA_ARG_U, LUA_RET);
    /// lua -> C: MenuAppendItem.
LUA_TO_C_2(MenuAppendItem, LUA_ARG_U, LUA_ARG_U, LUA_RET);
    /// lua -> C: MenuNew.
LUA_TO_C_0(MenuNew, LUA_RET_U);
    /// lua -> C: MenuSetLabel.
LUA_TO_C_2(MenuSetLabel, LUA_ARG_U, LUA_ARG_S, LUA_RET);
    /// lua -> C: MenuSetUserHeight.
LUA_TO_C_2(MenuSetUserHeight, LUA_ARG_U, LUA_ARG_I, LUA_RET);

    /// lua -> C: RootSetMenu.
LUA_TO_C_2(RootSetMenu, LUA_ARG_I, LUA_ARG_U, LUA_RET);

    /// lua -> C: DesktopSetCount.
LUA_TO_C_1(DesktopSetCount, LUA_ARG_I, LUA_RET);
    /// lua -> C: DesktopSetName.
LUA_TO_C_2(DesktopSetName, LUA_ARG_I, LUA_ARG_OS, LUA_RET);

/*
**	Panel
*/
    /// lua -> C: PanelSetOpacity.
LUA_TO_C_1(PanelSetOpacity, LUA_ARG_L, LUA_RET);
    /// lua -> C: PanelNew.
LUA_TO_C_0(PanelNew, LUA_RET_U);
    /// lua -> C: PanelSetAutoHide.
LUA_TO_C_2(PanelSetAutoHide, LUA_ARG_U, LUA_ARG_I, LUA_RET);
    /// lua -> C: PanelSetMaximizeOver.
LUA_TO_C_2(PanelSetMaximizeOver, LUA_ARG_U, LUA_ARG_I, LUA_RET);
    /// lua -> C: PanelSetPosition.
LUA_TO_C_3(PanelSetPosition, LUA_ARG_U, LUA_ARG_I, LUA_ARG_I, LUA_RET);
    /// lua -> C: PanelSetWidth.
LUA_TO_C_2(PanelSetWidth, LUA_ARG_U, LUA_ARG_I, LUA_RET);
    /// lua -> C: PanelSetHeight.
LUA_TO_C_2(PanelSetHeight, LUA_ARG_U, LUA_ARG_I, LUA_RET);
    /// lua -> C: PanelSetBorder.
LUA_TO_C_2(PanelSetBorder, LUA_ARG_U, LUA_ARG_I, LUA_RET);
    /// lua -> C: PanelSetLayout.
LUA_TO_C_2(PanelSetLayout, LUA_ARG_U, LUA_ARG_OS, LUA_RET);
    /// lua -> C: PanelSetLayer.
LUA_TO_C_2(PanelSetLayer, LUA_ARG_U, LUA_ARG_I, LUA_RET);
    /// lua -> C: PanelSetGravity.
LUA_TO_C_2(PanelSetGravity, LUA_ARG_U, LUA_ARG_OS, LUA_RET);
    /// lua -> C: PanelAddPlugin.
LUA_TO_C_2(PanelAddPlugin, LUA_ARG_U, LUA_ARG_U, LUA_RET);

#ifdef USE_CLOCK

/*
**	Clock panel plugin
*/
    /// lua -> C: ClockNew.
LUA_TO_C_5(ClockNew, LUA_ARG_OS, LUA_ARG_OS, LUA_ARG_OS, LUA_ARG_I, LUA_ARG_I,
    LUA_RET_U);
#endif

    /// lua -> C: PanelButtonNew.
LUA_TO_C_4(PanelButtonNew, LUA_ARG_U, LUA_ARG_OS, LUA_ARG_I, LUA_ARG_I,
    LUA_RET_OU);

#ifdef USE_PAGER

/*
**	Pager panel plugin
*/
    /// lua -> C: PagerNew.
LUA_TO_C_2(PagerNew, LUA_ARG_I, LUA_ARG_I, LUA_RET_U);
#endif

/*
**	Task panel plugin
*/
    /// lua -> C: TaskSetInsertMode.
LUA_TO_C_1(TaskSetInsertMode, LUA_ARG_OS, LUA_RET);
    /// lua -> C: TaskNew.
LUA_TO_C_0(TaskNew, LUA_RET_U);
    /// lua -> C: TaskSetMaxItemWidth.
LUA_TO_C_2(TaskSetMaxItemWidth, LUA_ARG_U, LUA_ARG_I, LUA_RET);

    /// lua -> C: SwallowNew.
LUA_TO_C_4(SwallowNew, LUA_ARG_OS, LUA_ARG_OS, LUA_ARG_I, LUA_ARG_I,
    LUA_RET_U);

#ifdef USE_SYSTRAY

/*
**	Systray panel plugin
*/
    /// lua -> C: SystrayNew.
LUA_TO_C_0(SystrayNew, LUA_RET_U);
#endif

/*
**	rule
*/
    /// lua -> C: RuleNew.
LUA_TO_C_0(RuleNew, LUA_RET_U);
    /// lua -> C: RuleAddName.
LUA_TO_C_2(RuleAddName, LUA_ARG_U, LUA_ARG_S, LUA_RET);
    /// lua -> C: RuleAddClass.
LUA_TO_C_2(RuleAddClass, LUA_ARG_U, LUA_ARG_S, LUA_RET);
    /// lua -> C: RuleAddTitle.
LUA_TO_C_2(RuleAddTitle, LUA_ARG_U, LUA_ARG_S, LUA_RET);
    /// lua -> C: RuleAddOption.
LUA_TO_C_3(RuleAddOption, LUA_ARG_U, LUA_ARG_S, LUA_ARG_OS, LUA_RET);

/*
**	global config
*/
    /// lua -> C: SetFocusModel.
LUA_TO_C_1(SetFocusModel, LUA_ARG_S, LUA_RET);

/**
**	lua -> C: functions
*/
static const struct luaL_reg LuaUwm[] = {
    {
	"Version", LuaVersion}, {

	"CommandAddStartup", LuaCommandAddStartup}, {
	"CommandAddRestart", LuaCommandAddRestart}, {
	"CommandAddExiting", LuaCommandAddExiting}, {

	"ColorSet", LuaColorSet}, {

#ifdef USE_BACKGROUND
	"BackgroundSet", LuaBackgroundSet}, {
#endif

	"FontSet", LuaFontSet}, {

#ifdef USE_ICON
	"IconAddPath", LuaIconAddPath}, {
#endif
	"BorderSetTitleHeight", LuaBorderSetTitleHeight}, {
	"BorderSetWidth", LuaBorderSetWidth}, {

	"TooltipSetEnabled", LuaTooltipSetEnabled}, {
	"TooltipSetDelay", LuaTooltipSetDelay}, {

	"MenuNewItem", LuaMenuNewItem}, {
	"MenuSetSubmenu", LuaMenuSetSubmenu}, {
	"MenuSetAction", LuaMenuSetAction}, {

	"MenuNew", LuaMenuNew}, {
	"MenuSetLabel", LuaMenuSetLabel}, {
	"MenuSetUserHeight", LuaMenuSetUserHeight}, {

	"MenuInsertItem", LuaMenuInsertItem}, {
	"MenuAppendItem", LuaMenuAppendItem}, {
	"RootSetMenu", LuaRootSetMenu}, {

	"DesktopSetCount", LuaDesktopSetCount}, {
	"DesktopSetName", LuaDesktopSetName}, {

	"PanelSetOpacity", LuaPanelSetOpacity}, {
	"PanelNew", LuaPanelNew}, {
	"PanelSetAutoHide", LuaPanelSetAutoHide}, {
	"PanelSetMaximizeOver", LuaPanelSetMaximizeOver}, {
	"PanelSetPosition", LuaPanelSetPosition}, {
	"PanelSetWidth", LuaPanelSetWidth}, {
	"PanelSetHeight", LuaPanelSetHeight}, {
	"PanelSetBorder", LuaPanelSetBorder}, {
	"PanelSetLayout", LuaPanelSetLayout}, {
	"PanelSetLayer", LuaPanelSetLayer}, {
	"PanelSetGravity", LuaPanelSetGravity}, {
	"PanelAddPlugin", LuaPanelAddPlugin}, {
#ifdef USE_CLOCK
	"ClockNew", LuaClockNew}, {
#endif
#ifdef USE_BUTTON
	"PanelButtonNew", LuaPanelButtonNew}, {
#endif
#ifdef USE_PAGER
	"PagerNew", LuaPagerNew}, {
#endif
#ifdef USE_TASK
	"TaskSetInsertMode", LuaTaskSetInsertMode}, {
	"TaskNew", LuaTaskNew}, {
	"TaskSetMaxItemWidth", LuaTaskSetMaxItemWidth}, {
#endif
#ifdef USE_SWALLOW
	"SwallowNew", LuaSwallowNew}, {
#endif
#ifdef USE_SYSTRAY
	"SystrayNew", LuaSystrayNew}, {
#endif

#ifdef USE_RULE
	"RuleNew", LuaRuleNew}, {
	"RuleAddName", LuaRuleAddName}, {
	"RuleAddClass", LuaRuleAddClass}, {
	"RuleAddTitle", LuaRuleAddTitle}, {
	"RuleAddOption", LuaRuleAddOption}, {
#endif

	"SetFocusModel", LuaSetFocusModel}, {

	NULL, NULL}
};

/**
**	Initialize lua
*/
static void LuaInit(const char *filename)
{
    lua_State *L;			// Lua interpreter
    int err;
    char *name;

    // initialize Lua
    L = lua_open();

    // load Lua base libraries
    luaL_openlibs(L);

    // register our functions
    luaL_register(L, "uwm", LuaUwm);

    // run script
    name = ExpandPath(filename);
    if ((err = luaL_dofile(L, name))) {
	Error("error parsing config file (%d) %s\n", err, lua_tostring(L, -1));
	Debug(2, "FIXME: %s not written load system config\n", __FUNCTION__);
	exit(-1);
    }
    free(name);
    // cleanup Lua
    lua_close(L);
}

/**
**	Parse configuration file.
*/
void ParseConfig(const char *file)
{
    LuaInit(file);
}

#else // }{ USE_LUA

    /// Dummy for parse configuration file.
#define NoParseConfig(file)

#endif // } !USE_LUA

/// @}

/**
**	Parse configuration file.
**
**	@param filename	config file name
*/
static void ParseConfig(const char *filename)
{
    char *name;
    Config *config;
    ConfigImport import[1];

    //
    //	build our constants passed to core-rc
    //
    import[0].Index = "UWM-VERSION";
    import[0].Value = VERSION;

    name = ExpandPath(filename);
    config = ConfigReadFile(1, import, name);
    free(name);

    if (!config) {
	Error("error parsing user config file '%s'\n", filename);
	config = ConfigReadFile(1, import, SYSTEM_CONFIG);
	if (!config) {
	    Error("error parsing system config file '%s'\n", SYSTEM_CONFIG);
	    return;
	}
	Debug(2, "Config '%s' loaded\n", SYSTEM_CONFIG);
    } else {
	Debug(2, "Config '%s' loaded\n", filename);
    }

    GlobalConfig(config);
    CommandConfig(config);
    ColorConfig(config);
    FontConfig(config);
    TooltipConfig(config);
    DesktopConfig(config);
    BackgroundConfig(config);
    RuleConfig(config);
    BorderConfig(config);
    StatusConfig(config);
    OutlineConfig(config);
    SnapConfig(config);
    // MoveResizeConfig(config);
    KeyboardConfig(config);
    IconConfig(config);
    MenuConfig(config);
    RootMenuConfig(config);
    //WindowMenuConfig(config);
    PanelConfig(config);
    DiaConfig(config);

    ConfigFreeMem(config);
}

// ------------------------------------------------------------------------ //

/**
**	Send message to root window.
**
**	@param string	Message string
**
**	@note when this function is called, no module is available
*/
static void SendClientMessage(const char *string)
{
    xcb_client_message_event_t event;

    ConnectionOpen();

    memset(&event, 0, sizeof(event));
    event.response_type = XCB_CLIENT_MESSAGE;
    event.format = 32;
    event.window = RootWindow;
    event.type = xcb_atom_get(Connection, string);

    xcb_send_event(Connection, XCB_SEND_EVENT_DEST_POINTER_WINDOW, RootWindow,
	XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT, (void *)&event);

    ConnectionClose();
}

/**
**	Send _UWM_RESTART to root window.
*/
static void SendRestart(void)
{
    SendClientMessage("_UWM_RESTART");
}

/**
**	Send _UWM_EXIT to root window.
*/
static void SendExit(void)
{
    SendClientMessage("_UWM_EXIT");
}

// ------------------------------------------------------------------------ //

/**
**	Print version.
*/
static void PrintVersion(void)
{
    printf("µwm Version " VERSION
#ifdef GIT_REV
	"(GIT-" GIT_REV ")"
#endif
	", (c) 2009, 2010 by Lutz Sammer\n"
	"\tLicense AGPLv3: GNU Affero General Public License version 3\n");
}

/**
**	Print usage.
*/
static void PrintUsage(void)
{
    printf("Usage: µwm [-?|-h] [-c config] [-d X] [-e] [-p] [-r] [-v]\n"
	"\t-c config\tload configuration from config\n"
	"\t-d X\tset the X display to use\n"
	"\t-e\texit µWM (send _UWM_EXIT to the root window)\n"
	"\t-r\trestart µWM (send _UWM_RESTART to the root window)\n"
	"\t-p\tparse the configuration file and exit\n"
	"\t-? -h\tdisplay this message\n" "\t-v\tdisplay version information\n"
	"Only idiots print usage on stderr!\n");
}

/**
**	Main entry point.
**
**	@param argc	number of arguments
**	@param argv	arguments vector
**
**	@returns -1 on failures, 0 clean exit.
*/
int main(int argc, char *const argv[])
{
    const char *config_filename;

    config_filename = DEFAULT_CONFIG;
    //
    //	Parse command line arguments
    //
    for (;;) {
	switch (getopt(argc, argv, "hv?-c:d:epr")) {
	    case 'c':			// config file
		config_filename = optarg;
		continue;
	    case 'd':			// x11 display
		DisplayString = optarg;
		continue;
	    case 'e':			// send exit
		SendExit();
		return 0;
	    case 'r':			// send restart
		SendRestart();
		return 0;
	    case 'p':			// parse config
		ParseConfig(config_filename);
		return 0;

	    case EOF:
		break;
	    case 'v':			// print version
		PrintVersion();
		return 0;
	    case '?':
	    case 'h':			// help usage
		PrintVersion();
		PrintUsage();
		return 0;
	    case '-':
		PrintVersion();
		PrintUsage();
		fprintf(stderr, "\nWe need no long options\n");
		return -1;
	    case ':':
		PrintVersion();
		fprintf(stderr, "Missing argument for option '%c'\n", optopt);
		return -1;
	    default:
		PrintVersion();
		fprintf(stderr, "Unkown option '%c'\n", optopt);
		return -1;
	}
	break;
    }
    if (optind < argc) {
	PrintVersion();
	while (optind < argc) {
	    fprintf(stderr, "Unhandled argument '%s'\n", argv[optind++]);
	}
	return -1;
    }
    //
    //	  main loop
    //
    ConnectionInit();

    do {
	// parse configuration file
	ParseConfig(config_filename);

	// initialize modules
	ModulesInit();

	// main event loop
	EventLoop();

	// restore start state
	ModulesExit();

    } while (KeepRunning);

    ConnectionExit();

    //
    //	 If we have command to execute on shutdown, run it now
    //
    if (ExitCommand) {
	const char *display;

	// FIXME: use global function
	// prepare environment
	display = DisplayString ? DisplayString : getenv("DISPLAY");
	if (display && *display) {
	    char *str;

	    str = alloca(9 + strlen(display));
	    stpcpy(stpcpy(str, "DISPLAY="), display);
	    putenv(str);
	}

	execl(Shell, Shell, "-c", ExitCommand, NULL);
	Warning("exec failed: (%s) %s\n", Shell, ExitCommand);
	return -1;
    }

    return 0;
}

/// @}
