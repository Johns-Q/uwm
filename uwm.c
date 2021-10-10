///
///	@file uwm.c	@brief µwm µ window manager main file
///
///	Copyright (c) 2009 - 2011, 2021 by Lutz Sammer.	 All Rights Reserved.
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
**		- clock
**		- netload
**		- pager
**		- swallow (dock)
**		- systray
**		- task-list
**	- builtin tooltips
**	- builtin background setter
**	- composite support with xcompmgr (sample X compositing manager)
**	- multiple desktops
**	- multiple screen (xinerama)
**	- 64-bit and 32-bit clean
**	- little-endian and big-endian clean
**	- compatible with uclibc and libc6
**	- compatible with GCC 4.5.3, Clang 2.9 and ekopath 4.0.11
**	- many features can be compile time enabled / disabled
**
**	see <a href="modules.html">list of µwm modules</a>.
*/

///
///	@defgroup uwm The uwm main module.
///
///	This module contains main, init, config and exit functions.
///
/// @{

#define _GNU_SOURCE	1		///< fix stpcpy strchrnul

#include <xcb/xcb.h>
#include "uwm.h"

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
#include <xcb/bigreq.h>

#include "queue.h"
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
#include "plugin/netload.h"
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
*/
static void RedrawCurrentDesktop(void)
{
    Client *client;
    int layer;

    for (layer = LAYER_BOTTOM; layer < LAYER_MAX; layer++) {
	TAILQ_FOREACH(client, &ClientLayers[layer], LayerQueue) {
	    // draw only clients on current desktop which are visible
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
    BackgroundPreInit();

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
    NetloadInit();
    PanelInit();
    ClientInit();			// need task, pager
    SwallowInit();			// needs client

//    Initializing = 0;

    PointerSetDefaultCursor(XcbScreen->root);
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
    NetloadExit();
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

    // emulate DefaultGC
    RootGC = xcb_generate_id(connection);
    mask = XCB_GC_FOREGROUND | XCB_GC_BACKGROUND | XCB_GC_GRAPHICS_EXPOSURES;
    values[0] = screen->white_pixel;
    values[1] = screen->black_pixel;
    values[2] = 0;
    xcb_create_gc(connection, RootGC, XcbScreen->root, mask, values);

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
static void SignalHandler(int __attribute__((unused)) signum)
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
    xcb_prefetch_extension_data(Connection, &xcb_big_requests_id);

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
    xcb_change_window_attributes(Connection, XcbScreen->root,
	XCB_CW_EVENT_MASK, value);

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
	if (XcbScreen->root_depth < 24) {
	    Warning("root depth is %d, icon alpha channel disabled\n",
		XcbScreen->root_depth);
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

    xcb_prefetch_maximum_request_length(Connection);
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
// Configuration
// ------------------------------------------------------------------------ //

#ifdef USE_RC				// {

#if defined(USE_RULE) || defined(USE_PANEL)	// {

/**
**	Parse gravity.
**
**	@param keyword	gravity type (static, north, ...)
**	@param error	error info for failure message
**
**	@returns gravity for given keyword.
*/
Gravity ParseGravity(const char *keyword, const char *error)
{
    if (!strcasecmp(keyword, "static")) {
	return GRAVITY_STATIC;
    } else if (!strcasecmp(keyword, "north")) {
	return GRAVITY_NORTH;
    } else if (!strcasecmp(keyword, "south")) {
	return GRAVITY_SOUTH;
    } else if (!strcasecmp(keyword, "west")) {
	return GRAVITY_WEST;
    } else if (!strcasecmp(keyword, "east")) {
	return GRAVITY_EAST;
    } else if (!strcasecmp(keyword, "center")) {
	return GRAVITY_CENTER;
    } else if (!strcasecmp(keyword, "north-west")) {
	return GRAVITY_NORTH_WEST;
    } else if (!strcasecmp(keyword, "north-east")) {
	return GRAVITY_NORTH_EAST;
    } else if (!strcasecmp(keyword, "south-west")) {
	return GRAVITY_SOUTH_WEST;
    } else if (!strcasecmp(keyword, "south-east")) {
	return GRAVITY_SOUTH_EAST;
    } else {
	Warning("invalid %s gravity: \"%s\"\n", error, keyword);
	return -1;
    }
}

#endif // } defined(USE_RULE) || defined(USE_PANEL)

/**
**	Get global configuration.
**
**	@param config	global config dictionary
*/
static void GlobalConfig(const Config * config)
{
    const char *sval;
    ssize_t ival;

    FocusModus = FOCUS_SLOPPY;
    if (ConfigStringsGetString(ConfigDict(config), &sval, "focus-model", NULL)) {
	if (!strcasecmp(sval, "sloppy")) {
	    // default: FocusModus = FOCUS_SLOPPY;
	} else if (!strcasecmp(sval, "click")) {
	    FocusModus = FOCUS_CLICK;
	} else {
	    Warning("invalid focus model: '%s'\n", sval);
	}
    }
    // DoubleClick
    DoubleClickDelta = DOUBLE_CLICK_DEFAULT_DELTA;
    if (ConfigStringsGetInteger(ConfigDict(config), &ival, "double-click",
	    "delta", NULL)) {
	if (DOUBLE_CLICK_MINIMAL_DELTA <= ival
	    && ival <= DOUBLE_CLICK_MAXIMAL_DELTA) {
	    DoubleClickDelta = ival;
	} else {
	    Warning("double-click delta %zd out of range\n", ival);
	}
    }
    DoubleClickSpeed = DOUBLE_CLICK_DEFAULT_SPEED;
    if (ConfigStringsGetInteger(ConfigDict(config), &ival, "double-click",
	    "speed", NULL)) {
	if (DOUBLE_CLICK_MINIMAL_SPEED <= ival
	    && ival <= DOUBLE_CLICK_MAXIMAL_SPEED) {
	    DoubleClickSpeed = ival;
	} else {
	    Warning("double-click speed %zd out of range\n", ival);
	}
    }
}

/**
**	Parse configuration file.
**
**	@param filename config file name
*/
static void ParseConfig(const char *filename)
{
    char *name;
    Config *config;

    //
    //	build our constants passed to core-rc
    //
    config = ConfigNewConfig(NULL);
    ConfigDefine(config, ConfigNewString("UWM-VERSION"),
	ConfigNewString(VERSION));

    name = ExpandPath(filename);
    config = ConfigReadFile2(config, name);
    free(name);

    if (!config) {
	Error("error parsing user config file '%s'\n", filename);
	config = ConfigReadFile2(config, SYSTEM_CONFIG);
	if (!config) {
	    Error("error parsing system config file '%s'\n", SYSTEM_CONFIG);
	    // return;
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
    KeyboardConfig(config);
    IconConfig(config);
    MenuConfig(config);
    RootMenuConfig(config);
    //WindowMenuConfig(config);
    PanelConfig(config);
    DiaConfig(config);

    ConfigFreeMem(config);
}

#else // }{ USE_RC

    /// Dummy for Parse configuration file.
#define ParseConfig(filename)

#endif // } !USE_RC

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
    xcb_intern_atom_cookie_t cookies;
    xcb_intern_atom_reply_t *reply;

    ConnectionOpen();
    cookies = xcb_intern_atom_unchecked(Connection, 0, strlen(string), string);

    memset(&event, 0, sizeof(event));
    event.response_type = XCB_CLIENT_MESSAGE;
    event.format = 32;
    event.window = XcbScreen->root;

    if ((reply = xcb_intern_atom_reply(Connection, cookies, NULL))) {
	event.type = reply->atom;
	free(reply);

	xcb_send_event(Connection, XCB_SEND_EVENT_DEST_POINTER_WINDOW,
	    XcbScreen->root, XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT,
	    (void *)&event);
    } else {
	FatalError("Can't send client message '%s'\n", string);
    }

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
	", (c) 2009 - 2011, 2021 by Lutz Sammer\n"
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
#ifdef DEBUG
	"\t-D\tincrease debug level (more and verbose output)\n"
#endif
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

#ifdef XDG_CONFIG_HOME
    const char *xdg_config_home;
    const char *config_dir;
    char *config_pathname;

    //
    //	Parse config dir.
    //
    xdg_config_home = getenv("XDG_CONFIG_HOME");
    if (xdg_config_home) {
	config_dir = xdg_config_home;
    } else {
	config_dir = "~/.config";
    }
    config_pathname = malloc(strlen(config_dir) + strlen(DEFAULT_CONFIG) + 2);
    strcpy(config_pathname, config_dir);
    config_pathname[strlen(config_dir)] = '/';
    strcpy(config_pathname + strlen(config_dir) + 1, DEFAULT_CONFIG);
    config_filename = config_pathname;
#else
    config_filename = DEFAULT_CONFIG;
#endif
    //
    //	Parse command line arguments
    //
    for (;;) {
	switch (getopt(argc, argv, "hv?-c:d:eprD")) {
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
	    case 'p':			// parse configuration only
		ConnectionOpen();
		ParseConfig(config_filename);
		ConnectionClose();
		return 0;

	    case 'D':			// increase debug level
#ifdef DEBUG
		++DebugLevel;
		continue;
#else
		fprintf(stderr, "\nCompiled without debug support\n");
		return -1;
#endif

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
	CommandPrepareEnv();
	execl(Shell, Shell, "-c", ExitCommand, NULL);
	Warning("exec failed: (%s) %s\n", Shell, ExitCommand);
	return -1;
    }

    return 0;
}

/// @}
