///
///	@file hints.h	@brief window manager hints header file
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

/// @addtogroup hint
/// @{

/// @addtogroup atom
/// @{

//////////////////////////////////////////////////////////////////////////////
//	Declares
//////////////////////////////////////////////////////////////////////////////

/**
**	Atom typedef
*/
typedef struct _atom_ Atom;

/**
**	Atom structure
*/
struct _atom_
{
    union
    {
	xcb_atom_t Atom;		///< x11 atom id
	xcb_intern_atom_cookie_t Cookie;	///< cookie for intern request
    };
    const char *Name;			///< atom name
};

/**
**	AtomTable typedef
*/
typedef struct _atoms_ AtomTable;

/**
**	AtomTable structure.
*/
struct _atoms_
{
    // misc
    Atom COMPOUND_TEXT;			///< type: compount text
    Atom UTF8_STRING;			///< type: uft8 tet

    Atom XROOTPMAP_ID;			///< root background pixmap

    Atom MANAGER;			///< manager message for systray

    // ICCCM atoms
    Atom WM_STATE;			///< window withdrawn/normal/iconic
    Atom WM_PROTOCOLS;			///< supported protocols list
    Atom WM_DELETE_WINDOW;		///< request delete window protocol
    Atom WM_TAKE_FOCUS;			///< assignment of input focus protocol
    Atom WM_CHANGE_STATE;		///< change state client message
#ifdef USE_COLORMAP
    Atom WM_COLORMAP_WINDOWS;		///< list of window ids with special
#endif

    // EWMH atoms
    Atom NET_SUPPORTED;			///< which hints do we support
    Atom NET_SUPPORTING_WM_CHECK;	///< for alive window manager check
    Atom NET_NUMBER_OF_DESKTOPS;	///< indicate number of virtual desktops
    Atom NET_DESKTOP_NAMES;		///< names of all virtual desktops
    Atom NET_DESKTOP_GEOMETRY;		///< common size of all desktops
    Atom NET_DESKTOP_VIEWPORT;		///< top left corner of each viewport
    Atom NET_CURRENT_DESKTOP;		///< index of current desktop
    Atom NET_ACTIVE_WINDOW;		///< ID of currently active window
    Atom NET_WORKAREA;			///< work area for each desktop
    Atom NET_FRAME_EXTENTS;		///< extents of the window's frame
    Atom NET_WM_DESKTOP;		///< desktop window is in
    Atom NET_SHOWING_DESKTOP;		///< "showing desktop" mode

    Atom NET_WM_STATE;			///< hints list describing window state
    Atom NET_WM_STATE_MODAL;		///< is modal dialog box
    Atom NET_WM_STATE_STICKY;		///< keep position fixed on screen
    Atom NET_WM_STATE_MAXIMIZED_VERT;	///< window is vertically maximized
    Atom NET_WM_STATE_MAXIMIZED_HORZ;	///< window is horizontally maximized
    Atom NET_WM_STATE_SHADED;		///< window is shaded
    Atom NET_WM_STATE_SKIP_TASKBAR;	///< not be included on a taskbar
    Atom NET_WM_STATE_SKIP_PAGER;	///< not be included on a pager
    Atom NET_WM_STATE_HIDDEN;		///< not be visible on screen
    Atom NET_WM_STATE_FULLSCREEN;	///< should fill entire screen
    Atom NET_WM_STATE_ABOVE;		///< on top of most windows
    Atom NET_WM_STATE_BELOW;		///< be below most windows
    Atom NET_WM_STATE_DEMANDS_ATTENTION;	///< action in or with window happened

    Atom NET_WM_ALLOWED_ACTIONS;	///< allowed action on window
    Atom NET_WM_ACTION_MOVE;		///< window may be moved around screen
    Atom NET_WM_ACTION_RESIZE;		///< window may be resized
    Atom NET_WM_ACTION_MINIMIZE;	///< window may be iconified
    Atom NET_WM_ACTION_SHADE;		///< window may be shaded
    Atom NET_WM_ACTION_STICK;		///< may have its sticky state toggled
    Atom NET_WM_ACTION_MAXIMIZE_HORZ;	///< may be maximized horizontally
    Atom NET_WM_ACTION_MAXIMIZE_VERT;	///< may be maximized vertically
    Atom NET_WM_ACTION_FULLSCREEN;	///< may be brought to fullscreen state
    Atom NET_WM_ACTION_CHANGE_DESKTOP;	///< may be moved between desktops
    Atom NET_WM_ACTION_CLOSE;		///< window may be closed
    Atom NET_WM_ACTION_ABOVE;		///< may placed in "above" layer
    Atom NET_WM_ACTION_BELOW;		///< may placed in "below" layer

    Atom NET_CLOSE_WINDOW;		///< pagers wanting to close a window
    Atom NET_MOVERESIZE_WINDOW;		///< pagers wanting to move or resize
    // NET_WM_MOVERESIZE
    // NET_RESTACK_WINDOW
    // NET_REQUEST_FRAME_EXTENTS

    Atom NET_WM_NAME;			///< title of window in utf-8
    Atom NET_WM_ICON;			///< EWMH CARDINAL[][2+n]/32
    Atom NET_WM_WINDOW_TYPE;		///< functional type of window
    Atom NET_WM_WINDOW_TYPE_DESKTOP;	///< desktop feature
    Atom NET_WM_WINDOW_TYPE_DOCK;	///< dock or panel feature
    Atom NET_WM_WINDOW_TYPE_TOOLBAR;	///< toolbar window
    Atom NET_WM_WINDOW_TYPE_MENU;	///< pinnable menu window
    Atom NET_WM_WINDOW_TYPE_UTILITY;	///< small persistent utility window
    Atom NET_WM_WINDOW_TYPE_SPLASH;	///< splash screen application starting
    Atom NET_WM_WINDOW_TYPE_DIALOG;	///< dialog window
    Atom NET_WM_WINDOW_TYPE_NORMAL;	///< normal ordinary window

    Atom NET_CLIENT_LIST;		///< array contain all windows managed
    Atom NET_CLIENT_LIST_STACKING;	///< bottom-to-top stacking order

    Atom NET_WM_STRUT_PARTIAL;		///< FIXME:
    Atom NET_WM_STRUT;			///< FIXME:

    Atom NET_SYSTEM_TRAY_OPCODE;	///< FIXME:
    Atom NET_SYSTEM_TRAY_ORIENTATION;	///< system tray plugin orientation

    Atom NET_WM_WINDOW_OPACITY;		///< opacity of window for composite

#ifdef USE_MOTIF_HINTS
    // MWM atoms
    Atom MOTIF_WM_HINTS;		///< motif window manager hints
#endif

    // µWM-specific atoms
    Atom UWM_RESTART;			///< private, restart window manager
    Atom UWM_EXIT;			///< private, exit window manager
};

//////////////////////////////////////////////////////////////////////////////
//	Variables
//////////////////////////////////////////////////////////////////////////////

extern AtomTable Atoms;			///< all atoms

//////////////////////////////////////////////////////////////////////////////
//	Prototypes
//////////////////////////////////////////////////////////////////////////////

    /// Send a cardinal atom get request.
extern xcb_get_property_cookie_t AtomCardinalRequest(xcb_window_t,
    const Atom *);
    /// Read a cardinal atom get reply.
extern int AtomGetCardinal(xcb_get_property_cookie_t, uint32_t *);

    /// Set a cardinal atom.
extern void AtomSetCardinal(xcb_window_t, const Atom *, uint32_t);

    /// Set a window atom.
extern void AtomSetWindow(xcb_window_t, const Atom *, uint32_t);

    /// Set pixmap atom.
extern void AtomSetPixmap(xcb_window_t, const Atom *, xcb_pixmap_t);

    /// Prepare initialize atoms.
extern void AtomPreInit(void);

    /// Initialize atoms.
extern void AtomInit(void);

    /// Cleanup atom module.
extern void AtomExit(void);

/// @}

//////////////////////////////////////////////////////////////////////////////
//	Prototypes
//////////////////////////////////////////////////////////////////////////////

    /// Send request for current desktop.
extern xcb_get_property_cookie_t HintNetCurrentDesktopRequest(void);

    /// Determine current desktop.
extern void HintGetNetCurrentDesktop(xcb_get_property_cookie_t);

    /// Get client protocols/hints.
extern void HintGetClientProtocols(Client *);

    /// Get client's name.
extern void HintGetWMName(Client *);

    /// Maintain _NET_WORKAREA property of root window.
extern void HintSetNetWorkarea(void);

    /// Maintain _NET_CLIENT_LIST[_STACKING] properties of root window.
extern void HintSetNetClientList(void);

    /// Set _NET_WM_DESKTOP ewmh hint for client.
extern void HintSetNetWMDesktop(const Client *);

    /// Set window state hints for client.
extern void HintSetAllStates(const Client *);

    /// Handle client message event _NET_MOVERESIZE_WINDOW.
extern void HintNetMoveResizeWindow(Client *,
    const xcb_client_message_event_t *);

    /// Handle client message event _NET_WM_STATE.
extern void HintNetWmState(Client *, const xcb_client_message_event_t *);

/// @}
