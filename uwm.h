///
///	@file uwm.h	@brief uwm main header file
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

//////////////////////////////////////////////////////////////////////////////
//	Config
//////////////////////////////////////////////////////////////////////////////

//
//	Which modules to build.
//
#if !defined(NO_OUTLINE) && !defined(USE_OUTLINE)
#define USE_OUTLINE			///< include outline window move/resize
#endif
#if !defined(NO_STATUS) && !defined(USE_STATUS)
#define USE_STATUS			///< include move/resize status window
#endif
#if !defined(NO_SNAP) && !defined(USE_SNAP)
#define USE_SNAP			///< include snap for move/resize
#endif
#if !defined(NO_DIALOG) && !defined(USE_DIALOG)
#define USE_DIALOG			///< include confirm dialog window
#endif
#if !defined(NO_MENU) && !defined(USE_MENU)
#define USE_MENU			///< include menu
#endif
#if !defined(NO_PANEL) && !defined(USE_PANEL)
#define USE_PANEL			///< include panel
#endif

#ifdef USE_PANEL			// only possible if panel enabled
#if !defined(NO_BUTTON) && !defined(USE_BUTTON)
#define USE_BUTTON			///< include panel button plugin
#endif
#if !defined(NO_CLOCK) && !defined(USE_CLOCK)
#define USE_CLOCK			///< include panel clock plugin
#endif
#if !defined(NO_PAGER) && !defined(USE_PAGER)
#define USE_PAGER			///< include panel pager plugin
#endif
#if !defined(NO_TASK) && !defined(USE_TASK)
#define USE_TASK			///< include panel task plugin
#endif
#if !defined(NO_SWALLOW) && !defined(USE_SWALLOW)
#define USE_SWALLOW			///< include panel swallow plugin
#endif
#if !defined(NO_SYSTRAY) && !defined(USE_SYSTRAY)
#define NO_SYSTRAY			///< include panel systray plugin
#endif
#endif

#if !defined(NO_BACKGROUND) && !defined(USE_BACKGROUND)
#define USE_BACKGROUND			///< include background support
#endif
#if !defined(NO_ICON) && !defined(USE_ICON)
#define USE_ICON			///< include icon support
#endif

#ifdef USE_ICON				// loading icons are supported for:
#if !defined(NO_JPEG) && !defined(USE_JPEG)
#define USE_JPEG			///< include external jpeg support
#endif
#if !defined(NO_PNG) && !defined(USE_PNG)
#define USE_PNG				///< include external png support
#endif
#if !defined(NO_TIFF) && !defined(USE_TIFF)
#define USE_TIFF			///< include external tiff support
#endif
#if !defined(NO_XPM) && !defined(USE_XPM)
#define USE_XPM				///< include internal xpm support
#endif
#endif

#if !defined(NO_RULE) && !defined(USE_RULE)
#define USE_RULE			///< include window rules
#endif

#if !defined(NO_DIA) && !defined(USE_DIA)
#define NO_DIA				///< include dia-show application
#endif

#if !defined(NO_TD) && !defined(USE_TD)
#define NO_TD				///< include tower-defense application
#endif

	// x11/xcb features
#if !defined(NO_XINERMA) && !defined(USE_XINERMA)
#define USE_XINERMA			///< xinerama support
#endif
#if !defined(NO_SHAPE) && !defined(USE_SHAPE)
#define USE_SHAPE			///< shape support
#endif
#if !defined(NO_RENDER) && !defined(USE_RENDER)
#define USE_RENDER			///< render support
#endif
#if !defined(NO_XMU) && !defined(USE_XMU)
#define USE_XMU				///< xmu emulation support
#endif
#if !defined(NO_COLORMAPS) && !defined(USE_COLORMAPS)
#define NO_COLORMAPS			///< x11 colormaps support
#endif

#if !defined(NO_MOTIF_HINTS) && !defined(USE_MOTIF_HINTS)
#define NO_MOTIF_HINTS			///< support motif hints
#endif

#if !defined(NO_DEBUG) && !defined(USE_DEBUG)
#define NO_DEBUG			///< generate debug code
#endif

//
//	General tuneable parameters.
//
#ifndef SYSTEM_CONFIG
    /// global default uwm system configuration file
#define SYSTEM_CONFIG "/usr/local/etc/system.uwmrc"
#endif
#ifndef DEFAULT_CONFIG
    /// default uwm user configuration file
#define DEFAULT_CONFIG "~/.uwm/uwmrc"
#endif

#ifndef SHELL
#define SHELL	"/bin/sh"		///< default shell
#endif

#define RESTART_DELAY 50000		///< restart delay in microsecond

#ifndef DEFAULT_FONT
#define DEFAULT_FONT "variable"		///< default/fallback font name
#endif

#ifndef CURSOR_FONT
#define CURSOR_FONT "cursor"		///< default cursor font
#endif

#define CORNER_RADIUS 4			///< radius of rounded windows

#define MENU_INNER_SPACE 2		///< inner border of menu items

#define PANEL_DEFAULT_WIDTH 32		///< default panel width
#define PANEL_DEFAULT_HEIGHT 32		///< default panel height
#define PANEL_MINIMAL_BORDER 0		///< mimimal panel border width
#define PANEL_DEFAULT_BORDER 1		///< default border width
#define PANEL_MAXIMAL_BORDER 32		///< maximal panel border width
#define PANEL_DEFAULT_HIDE_SIZE 1	///< default size while hideing

#define PANEL_INNER_SPACE 1		///< panel inner space

#define CLOCK_INNER_SPACE 2		///< inner border of clock plugin
#ifndef CLOCK_DEFAULT_FORMAT
#define CLOCK_DEFAULT_FORMAT "%I:%M %p"	///< default time format
#endif
#ifndef CLOCK_DEFAULT_LONG_FORMAT
#define CLOCK_DEFAULT_LONG_FORMAT "%c"	///< default long time format
#endif

#define TASK_INNER_SPACE 2		///< task inner spacing

#define TOOLTIP_DEFAULT_DELAY 500	///< tooltip delay
#define TOOLTIP_MAXIMAL_MOVE 2		///< maximum movement of mouse

#define DESKTOP_MINIMAL_COUNT 1		///< minimum 1 desktop needed
#define DESKTOP_DEFAULT_COUNT 4		///< number of desktops to create
#define DESKTOP_MAXIMAL_COUNT 9		///< maximal 9 desktops supported

#define BORDER_MINIMAL_WIDTH 1		///< minimum client border width
#define BORDER_DEFAULT_WIDTH 4		///< default client border width
#define BORDER_MAXIMAL_WIDTH 32		///< maximal client border width
#define BORDER_MINIMAL_TITLE_HEIGHT 2	///< minimum client title height
#define BORDER_DEFAULT_TITLE_HEIGHT 20	///< default client title height
#define BORDER_MAXIMAL_TITLE_HEIGHT 64	///< maximal client title height

#define DOUBLE_CLICK_MINIMAL_DELTA 0	///< minimal movement for double click
#define DOUBLE_CLICK_DEFAULT_DELTA 2	///< default movement for double click
#define DOUBLE_CLICK_MAXIMAL_DELTA 32	///< maximal movement for double click

#define DOUBLE_CLICK_MINIMAL_SPEED 1	///< minimal time for double click
#define DOUBLE_CLICK_DEFAULT_SPEED 250	///< default time for double click
#define DOUBLE_CLICK_MAXIMAL_SPEED 2000	///< maximal time for double click

#define SNAP_MINIMAL_DISTANCE 1		///< minimum snap distance
#define SNAP_DEFAULT_DISTANCE 5		///< default snap distance
#define SNAP_MAXIMAL_DISTANCE 32	///< maximal snap distance

//
//	Nothing to edit behind this point
// ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

//////////////////////////////////////////////////////////////////////////////
//	Defines
//////////////////////////////////////////////////////////////////////////////

#ifndef DEBUG
    /// No debug: only errors and warnings
#define DebugLevel 2
#endif

/**
**	Debug output function.
**
**	- 0	fatal errors and errors
**	- 1	warnings
**	- 2	important debug and fixme's
*/
#define Debug(level, fmt...) \
    do { if (level<DebugLevel) { printf(fmt); fflush(stdout); } } while (0)

#ifdef DEBUG
	/// include debug only code
#define IfDebug(code) do { code } while(0)
#else
	/// didn't include debug only code
#define IfDebug(code)
#endif

/**
**	Show fatal error.
*/
#define FatalError(fmt...) \
     do { Debug(0, fmt); exit(-1); } while (0)

/**
**	Show error.
*/
#define Error(fmt...) Debug(0, fmt)

/**
**	Show warning.
*/
#define Warning(fmt...) Debug(1, fmt)

    /// Return the minimum of two values.
#define MIN( i, j ) ( (i) > (j) ? (j) : (i) )

    /// Return the maximum of two values.
#define MAX( i, j ) ( (i) > (j) ? (i) : (j) )

#ifdef DEBUG
    /// disable compiler warning about uninitialized variables
#define NO_WARNING(x) memset(&x, 0, sizeof(x))
#else
    /// disable compiler warning about uninitialized variables
#define NO_WARNING(x)
#endif

//////////////////////////////////////////////////////////////////////////////
//	Types
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
//	Variables
//////////////////////////////////////////////////////////////////////////////

#ifdef DEBUG
extern int DebugLevel;			///< debug level
#endif
extern const char *DisplayString;	///< X11 display string
extern xcb_connection_t *Connection;	///< connection to X11 server
extern xcb_screen_t *XcbScreen;		///< our screen

#define RootWidth (XcbScreen->width_in_pixels)	///< FIXME: quick fix
#define RootHeight (XcbScreen->height_in_pixels)	///< FIXME: quick fix
extern xcb_window_t RootWindow;		///< our root window
extern unsigned RootDepth;		///< root window depth
extern xcb_gcontext_t RootGC;		///< root graphic context
extern xcb_visualtype_t *RootVisualType;	///< root visual type

#ifdef USE_SHAPE
extern int HaveShape;			///< shape extension found
extern int ShapeEvent;			///< first shape event code
#endif
#ifdef USE_RENDER
extern int HaveRender;			///< xrender extension found
#endif

extern char KeepRunning;		///< keep running
extern char KeepLooping;		///< keep looping

//////////////////////////////////////////////////////////////////////////////
//	Prototypes
//////////////////////////////////////////////////////////////////////////////

    /// Execute an external program. @ingroup commands
extern void CommandRun(const char *);

    /// Parse gravity.
extern int ParseGravity(const char *, const char *);

    /// Signal desktop change
#define DesktopUpdate() \
    PanelButtonDesktopUpdate()
