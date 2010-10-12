///
///	@file panel.h @brief panel header file
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

/// @addtogroup panel
/// @{

//////////////////////////////////////////////////////////////////////////////
//	Declares
//////////////////////////////////////////////////////////////////////////////

/**
**	Panel typedef.
*/
typedef struct _panel_ Panel;

/**
**	Panel common panel plugin data typedef.
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

    unsigned UserWidth:1;		///< user-specified width flag
    unsigned UserHeight:1;		///< user-specified width flag
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
    int8_t HiddenSize;			///< hidden size in pixels

    Layer OnLayer:8;			///< layer for panel

    unsigned Hidden:1;			///< true if hidden by autohide
    unsigned AutoHide:1;		///< true autohide panel
    unsigned MaximizeOver:1;		///< true if maximized over panel

    PanelLayout Layout:1;		///< layout
    Gravity Gravity:4;			///< placement and alignment on screen

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

extern void PanelsDraw(void);		///< Draw all panels.

    /// Hide/show/toggle panel (for commands).
extern void PanelToggle(int, int);

    /// Execute pointer button command
extern void PanelExecuteButton(const Plugin *, MenuButton *, int);

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

    /// Default panel plugin delete method.
extern void PanelPluginDeletePixmap(Plugin *);

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

    /// Resize a panel.
extern void PanelResize(Panel *);

extern void PanelInit(void);		///< Initialize panel support.
extern void PanelExit(void);		///< Cleanup panel support.

    /// Parse a panel plugin size configuration.
extern void PanelPluginConfigSize(const ConfigObject *, Plugin *);

    /// Configure panel support.
extern void PanelConfig(const Config *);

/// @}
