///
///	@file client.h	@brief client header file
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

/// @addtogroup client
/// @{

//////////////////////////////////////////////////////////////////////////
//	Defines
//////////////////////////////////////////////////////////////////////////////

#define CLIENT_MOVE_DELTA 3		///< react on move after more moved

//////////////////////////////////////////////////////////////////////////////
//	Declares
//////////////////////////////////////////////////////////////////////////////

/**
**	Enumeration of focus models
*/
typedef enum
{
    FOCUS_SLOPPY,			///< sloppy focus
    FOCUS_CLICK,			///< click focus
} FocusModel;

/**
**	Window state flags.
*/
typedef enum
{
    WM_STATE_NONE = 0,			///< no special state

    WM_STATE_ACTIVE = 1 << 0,		///< client has focus
    WM_STATE_MAPPED = 1 << 1,		///< client is shown (on some desktop)
    WM_STATE_MAXIMIZED_HORZ = 1 << 2,	///< client is maximized horizonatally
    WM_STATE_MAXIMIZED_VERT = 1 << 3,	///< client is maximized vertically
    WM_STATE_HIDDEN = 1 << 4,		///< client is not on current desktop
    WM_STATE_STICKY = 1 << 5,		///< client is on all desktops
    WM_STATE_NOLIST = 1 << 6,		///< skip client in task list
    WM_STATE_NOPAGER = 1 << 7,		///< skip client on pager display
    WM_STATE_MINIMIZED = 1 << 8,	///< client is minimized
    WM_STATE_SHADED = 1 << 9,		///< client is shaded
    WM_STATE_WMDIALOG = 1 << 10,	///< client is a WM dialog window
    WM_STATE_PIGNORE = 1 << 11,		///< ignore program-specified position
    WM_STATE_SHAPE = 1 << 12,		///< client uses shape extension
    WM_STATE_SHOW_DESKTOP = 1 << 13,	///< client minimized to show desktop
    WM_STATE_FULLSCREEN = 1 << 14,	///< client wants to be full screen
    WM_STATE_OPACITY = 1 << 15		///< client has a fixed opacity
} WmState;

/**
**	Window border flags.
*/
typedef enum
{
    BORDER_NONE = 0,			///< window has no border

    BORDER_OUTLINE = 1 << 0,		///< window has a border
    BORDER_TITLE = 1 << 1,		///< window has a title bar
    BORDER_MINIMIZE = 1 << 2,		///< window supports minimize
    BORDER_STICKY = 1 << 3,		///< window supports sticky
    BORDER_CLOSE = 1 << 4,		///< window supports close
    BORDER_RESIZE = 1 << 5,		///< window supports resizing
    BORDER_LOWER = 1 << 6,		///< window supports lower
    BORDER_RAISE = 1 << 7,		///< window supports raise
    BORDER_MOVE = 1 << 8,		///< window supports moving
    BORDER_MAXIMIZE_VERT = 1 << 9,	///< " supports maximize vertically
    BORDER_MAXIMIZE_HORZ = 1 << 10,	///< " supports maximize horizontally
} WmBorder;

/**
**	Enumeration of our window layers.
*/
typedef enum
{
    LAYER_BOTTOM = 0,			///< lowest window layer
    LAYER_DESKTOP = 0,			///< desktop window layer
    LAYER_BELOW = 2,			///< normal window layer with below
    LAYER_NORMAL = 3,			///< normal window layer
    LAYER_ABOVE = 4,			///< normal window layer with above
    LAYER_PANEL_DEFAULT = 6,		///< default layer of panels
    LAYER_FULLSCREEN = 8,		///< fullscreeen windows
    LAYER_TOP = 9,			///< highest window layer
    LAYER_MAX = 10			///< maximal number of layers supported
} Layer;

/**
**	The default border flags.
*/
#define BORDER_DEFAULT \
    (BORDER_OUTLINE | BORDER_TITLE | BORDER_MINIMIZE | BORDER_STICKY \
    | BORDER_CLOSE | BORDER_RESIZE | BORDER_MOVE | BORDER_LOWER \
    | BORDER_RAISE | BORDER_MAXIMIZE_VERT | BORDER_MAXIMIZE_HORZ)

/**
**	Client window structure typedef.
*/
typedef struct _client_ Client;

/**
**	Client window structure.
*/
struct _client_
{
    SLIST_ENTRY(_client_) NetClient;	///< singly-linked list for net-client

    LIST_ENTRY(_client_) ChildList;	///< in list of childs
    LIST_ENTRY(_client_) FrameList;	///< in list of frames
    TAILQ_ENTRY(_client_) LayerQueue;	///< in list queue of layers

    int16_t Deleted;			///< client is already deleted
    int16_t RefCount;			///< reference counter

    xcb_window_t Window;		///< client window
    xcb_window_t Parent;		///< frame window
    xcb_window_t Owner;			///< owner window

    int16_t X;				///< x-location of window
    int16_t Y;				///< y-location of window
    uint16_t Width;			///< width of window
    uint16_t Height;			///< height of window

    int16_t OldX;			///< old x-coordinate (for maximize)
    int16_t OldY;			///< old y-coordinate (for maximize)
    uint16_t OldWidth;			///< old width (for maximize)
    uint16_t OldHeight;			///< old height (for maximize)

#ifdef USE_COLORMAP
    xcb_colormap_t Colormap;		///< window's colormap
    // ColormapNode *Colormaps;		///< colormaps assigned to this window
#endif

    //@{
    WmState State:16;			///< state bit mask
    WmBorder Border:16;			///< border bit mask
    uint8_t OnLayer;			///< window layer
    uint8_t Desktop;			///< desktop
    uint32_t Opacity;			///< opacity (0 - 0xFFFFFFFF)
    //@}

    // properties/hints cached
    char *Name;				///< name of window for display
    char *InstanceName;			///< name of window for properties
    char *ClassName;			///< name of window class
    xcb_size_hints_t SizeHints;		///< copy of xcb/x11 size hints.

#ifdef USE_ICON
    // FIXME: Icon *
    void *Icon;				///< icon assigned to this window
#endif
};

// *INDENT-OFF*
/**
**	typedef for clients in a net client list
*/
typedef SLIST_HEAD(_client_netlist_, _client_) ClientNetListHead;
/**
**	typedef for clients in a layer tail queue
*/
typedef TAILQ_HEAD(_client_layer_, _client_) ClientLayerHead;
// *INDENT-ON*

//////////////////////////////////////////////////////////////////////////////
//	Variables
//////////////////////////////////////////////////////////////////////////////

extern FocusModel FocusModus;		///< current focus model

    /// singly-linked List of all clients for _NET_CLIENT_LIST and task list
extern ClientNetListHead ClientNetList;

extern int ClientN;			///< number of clients managed

    /// table of double linkted tail queues of all clients in a layer
extern ClientLayerHead ClientLayers[LAYER_MAX];

extern void (*ClientController)(void);	///< callback to stop move/resize
extern Client *ClientControlled;	///< current controlled client

//////////////////////////////////////////////////////////////////////////////
//	Prototypes
//////////////////////////////////////////////////////////////////////////////

    /// Determine if a client is allowed focus.
extern int ClientShouldFocus(const Client *);

    /// Send a client message to a window.
extern void ClientSendMessage(xcb_window_t, xcb_atom_t, xcb_atom_t);

    /// Send WM_DELETE_WINDOW message to window.
extern void ClientSendDeleteWindow(xcb_window_t);

    /// Send a configure event to a client window.
extern void ClientSendConfigureEvent(const Client *);

    /// Restack clients.
extern void ClientRestack(void);

    /// Set keyboard focus to a client.
extern void ClientFocus(Client *);

    /// Set keyboard focus back to active client.
extern void ClientRefocus(void);

    /// Focus next client in stacking order.
extern void ClientFocusNextStacked(const Client *);

#ifdef USE_SHAPE
    /// Update shape of a client using shape extension.
extern void ClientUpdateShape(const Client *);
#else
    /// Dummy for Update shape of a client using shape extension.
#define ClientUpdateShape(client)
#endif

    /// Shade a client.
extern void ClientShade(Client *);

    /// Unshade a client.
extern void ClientUnshade(Client *);

    /// Set full screen status of a client.
extern void ClientSetFullscreen(Client *, int);

    /// Raise client to top of its layer.
extern void ClientRaise(Client *);

    /// Lower client to bottom of its layer.
extern void ClientLower(Client *);

    /// Set client's state to withdrawn.
extern void ClientSetWithdrawn(Client *);

    /// Minimize client window and all of its transients.
extern void ClientMinimize(Client *);

    /// Restore client window and its transients.
extern void ClientRestore(Client *, int);

    /// Maximize client window.
extern void ClientMaximize(Client *, int, int);

    /// Maximize client using its default maximize settings.
extern void ClientMaximizeDefault(Client *);

    /// Show hidden client.
extern void ClientShow(Client *);

    /// Hide client without unmapping.
extern void ClientHide(Client *);

    /// Set the client layer.
extern void ClientSetLayer(Client *, int);

    /// Set a client's desktop. This will update transients.
extern void ClientSetDesktop(Client *, int);

    /// Set client's sticky status.
extern void ClientSetSticky(Client *, int);

    /// Find a client by parent (frame) window.
extern Client *ClientFindByFrame(xcb_window_t);

    /// Find a client by child (client) window.
extern Client *ClientFindByChild(xcb_window_t);

    /// Find a client by (client) window or parent (frame) window.
extern Client *ClientFindByAny(xcb_window_t);

    /// Add window to management.
extern Client *ClientAddWindow(xcb_window_t,
    xcb_get_window_attributes_cookie_t, int, int);

    /// Remove client window from management.
extern void ClientDelWindow(Client *);

    /// Kill client window.
extern void ClientKill(Client *);

    /// Send a delete message to a client.
extern void ClientDelete(Client *);

/// FIXME:

    /// Prepare initialize the client module
extern void ClientPreInit(void);

    /// Initialize the client module
extern void ClientInit(void);

    /// Cleanup the client module
extern void ClientExit(void);

/// @addtogroup placement
/// @{

    /// Move window in specified direction for reparenting.
extern void ClientGravitate(Client *, int);

    /// Remove struts associated with client.
extern void ClientDelStrut(const Client *);

    /// Add client specified struts to our list.
extern void ClientGetStrut(const Client * client);

    /// Place client on screen.
extern void ClientPlace(Client *, int);

    /// Place maximized client on screen.
extern void ClientPlaceMaximized(Client *, int, int);

    /// Constrain size of client to available screen space.
extern void ClientConstrainSize(Client *);

    /// Initialize placement module.
extern void PlacementInit(void);

    /// Exit placement module.
extern void PlacementExit(void);

/// @}

/// @}
