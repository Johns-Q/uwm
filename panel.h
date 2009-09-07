///
///	@file panel.h @brief panel header file
///
///	Copyright (c) 2009 by Johns.  All Rights Reserved.
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

/// addtogroup panel
/// @{

//////////////////////////////////////////////////////////////////////////////
//	Declares
//////////////////////////////////////////////////////////////////////////////

/**
**	Panel typedef.
*/
typedef struct _panel_ Panel;

/**
**	Panel plugin typedef.
*/
typedef struct _panel_plugin_ Plugin;

/**
**	Structure to hold common panel plugin data.
*/
struct _panel_plugin_
{
    STAILQ_ENTRY(_panel_plugin_) Next;	///< singly-linked panel list
    Panel *Panel;			///< owner of plugin

    void *Object;			///< plugin private data

    int16_t X;				///< x-coordinate in panel
    int16_t Y;				///< y-coordinate in panel

    uint16_t Width;			///< actual width
    uint16_t Height;			///< actual height

    int16_t ScreenX;			///< x-coordinate on screen
    int16_t ScreenY;			///< y-coordinate on screen

    uint16_t RequestedWidth;		///< requested width
    uint16_t RequestedHeight;		///< requested height

    unsigned Grabbed:1;			///< mouse was grabbed by plugin

    xcb_window_t Window;		///< content of window (plugin frees)
    xcb_pixmap_t Pixmap;		///< content of pixmap (plugin frees)

    /// callback to create new plugin
    void (*Create) (Plugin *);
    /// callback to delete plugin
    void (*Delete) (Plugin *);
    /// callback to set size known so far
    void (*SetSize) (Plugin *, unsigned, unsigned);
    /// callback to resize plugin
    void (*Resize) (Plugin *);
    /// callback to show tooltip of plugin
    void (*Tooltip) (Plugin *, int, int);
    /// callback for mouse presse event
    void (*HandleButtonPress) (Plugin *, int, int, int);
    /// callback for mouse release event
    void (*HandleButtonRelease) (Plugin *, int, int, int);
    /// callback for mouse motion event
    void (*HandleMotionNotify) (Plugin *, int, int, int);
    /// callback for timeout of plugin
    void (*Timeout) (Plugin *, uint32_t, int, int);
};

/**
**	Enumeration of panel layouts.
*/
typedef enum
{
    PANEL_LAYOUT_HORIZONTAL,		///< left-to-right
    PANEL_LAYOUT_VERTICAL		///< top-to-bottom
} PanelLayout;

/**
**	New Enumeration of panel alignments.
*/
typedef enum
{
    PANEL_GRAVITY_STATIC = 0,		///< static at user specified
    ///< x and y coordinates
    PANEL_GRAVITY_NORTH_WEST,		///< top-left aligned
    PANEL_GRAVITY_NORTH,		///< top-middle aligned
    PANEL_GRAVITY_NORTH_EAST,		///< top-right aligned
    PANEL_GRAVITY_WEST,			///< left-center aligned
    PANEL_GRAVITY_CENTER,		///< screen center aligned
    PANEL_GRAVITY_EAST,			///< right-center aligned
    PANEL_GRAVITY_SOUTH_WEST,		///< bottom-left aligned
    PANEL_GRAVITY_SOUTH,		///< bottom-middle aligned
    PANEL_GRAVITY_SOUTH_EAST,		///< bottom-right aligned
} PanelGravity;

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
**	Panel structure.
*/
struct _panel_
{
    SLIST_ENTRY(_panel_) Next;		///< singly-linked panel list

    int16_t X;				///< x-coordinate of panel
    int16_t Y;				///< y-coordinate of panel

    uint16_t Width;			///< actual width of panel
    uint16_t Height;			///< actual height of panel

    int16_t RequestedX;			///< requested x-coordinate of panel
    int16_t RequestedY;			///< requested y-coordinate of panel
    uint16_t RequestedWidth;		///< requested width of panel
    uint16_t RequestedHeight;		///< requested height of panel

    int8_t Border;			///< border size in pixels

    Layer OnLayer:8;			///< layer for panel

    unsigned Hidden:1;			///< true if hidden by autohide
    unsigned AutoHide:1;		///< true autohide panel
    unsigned MaximizeOver:1;		///< true if maximized over panel

    PanelLayout Layout:1;		///< layout
    PanelGravity Gravity:4;		///< placement and alignment on screen

    xcb_window_t Window;		///< panel window

    /// single tail queue list of plugins belonging of panel
     STAILQ_HEAD(_plugin_head_, _panel_plugin_) Plugins;
};

SLIST_HEAD(_panel_head_, _panel_);	///< panel head structure

//////////////////////////////////////////////////////////////////////////////
//	Variables
//////////////////////////////////////////////////////////////////////////////

extern struct _panel_head_ Panels;	/// list of all panels

//////////////////////////////////////////////////////////////////////////////
//	Prototypes
//////////////////////////////////////////////////////////////////////////////

    /// Create an empty panel plugin.
extern Plugin *PanelPluginNew(void);

    /// Clear panel plugin background with given color.
extern void PanelClearPluginBackgroundWithColor(const Plugin *,
    const uint32_t *);
    /// Clear panel plugin background.
extern void PanelClearPluginBackground(const Plugin *);

    /// Update plugin on a panel.
extern void PanelUpdatePlugin(const Panel *, const Plugin *);

    /// Default panel plugin create method.
extern void PanelPluginCreatePixmap(Plugin *);

    /// Panel plugin delete method.  Frees used pixmap.
extern void PanelPluginDeletePixmap(Plugin *);

    /// Resize a panel.
extern void PanelResize(Panel *);

    /// Handle a button press on a panel.
extern int PanelHandleButtonPress(const xcb_button_press_event_t *);

    /// Handle a button release on a panel.
extern int PanelHandleButtonRelease(const xcb_button_release_event_t *);

    /// Handle a motion notify event over a panel.
extern int PanelHandleMotionNotify(const xcb_motion_notify_event_t *);

    /// Handle a panel enter notify.
extern int PanelHandleEnterNotify(const xcb_enter_notify_event_t *);

    /// Handle a panel expose event.
extern int PanelHandleExpose(const xcb_expose_event_t *);

    /// Handle timeout for the panel.
extern void PanelTimeout(uint32_t, int, int);

extern void PanelsDraw(void);		///< Draw all panels.

extern void PanelInit(void);		///< Initialize panel support.
extern void PanelExit(void);		///< Cleanup panel support.
extern void PanelConfig(void);		///< Configure panel support.

/// @}
