///
///	@file menu.h	@brief menu header file
///
///	Copyright (c) 2009, 2010, 2021 by Lutz Sammer.	All Rights Reserved.
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

/// @addtogroup menu
/// @{

// ------------------------------------------------------------------------ //
// Dialog
// ------------------------------------------------------------------------ //

/// @addtogroup dialog
/// @{

#ifdef USE_DIALOG			// {

//////////////////////////////////////////////////////////////////////////////
//	Variables
//////////////////////////////////////////////////////////////////////////////

extern char ShowKillConfirmation;	///< show dialog for kill

//////////////////////////////////////////////////////////////////////////////
//	Prototypes
//////////////////////////////////////////////////////////////////////////////

    /// Show a confirm dialog.
extern void DialogShowConfirm(Client *, void (*)(Client *), ...);

    /// Handle an expose event.
extern int DialogHandleExpose(const xcb_expose_event_t *);

    /// Handle a mouse button release event.
extern int DialogHandleButtonPress(const xcb_button_press_event_t *);

    /// Handle a button release on dialog.
extern int DialogHandleButtonRelease(const xcb_button_release_event_t *);

    /// Initialize dialog.
#define DialogInit()
    /// Cleanup dialog.
extern void DialogExit(void);

#else // }{ USE_DIALOG

#define ShowKillConfirmation	0	///< Dummy for Show dialog for kill

    /// Dummy for Show a confirm dialog.
#define DialogShowConfirm(client, action, ...) action(client)

    /// Dummy for Handle an expose event.
#define DialogHandleExpose(event) 0
    /// Dummy for Handle a mouse button release event.
#define DialogHandleButtonPress(event)	0
    /// Dummy for Handle a button release on dialog.
#define DialogHandleButtonRelease(event) 0

    /// Dummy for Initialize dialog.
#define DialogInit()
    /// Dummy for Cleanup dialog.
#define DialogExit()

#endif // } !USE_DIALOG

/// @}

// ------------------------------------------------------------------------ //
// Label
// ------------------------------------------------------------------------ //

/// @addtogroup label
/// @{

//////////////////////////////////////////////////////////////////////////////
//	Defines
//////////////////////////////////////////////////////////////////////////////

#define LABEL_BORDER 1			///< label border
#define LABEL_INNER_SPACE 2		///< label inner spacing

//////////////////////////////////////////////////////////////////////////////
//	Declares
//////////////////////////////////////////////////////////////////////////////

/**
**	Label types.
*/
typedef enum
{
    LABEL_MENU,				///< menu item (default: must be 0)
    LABEL_MENU_ACTIVE,			///< active menu item
    LABEL_MENU_LABEL,			///< just menu label
    LABEL_TASK,				///< item in the task list
    LABEL_TASK_ACTIVE,			///< active item in the task list
    LABEL_PANEL,			///< item in the panel
    LABEL_PANEL_ACTIVE			///< active item in the panel
} LabelType;

/**
**	Alignment of content in a label.
*/
typedef enum
{
    LABEL_ALIGN_LEFT,			///< left align label (default)
    LABEL_ALIGN_CENTER			///< center align label
} LabelAlign;

/**
**	Used for drawing a label.
*/
typedef struct
{
    LabelType Type:8;			///< type of label to draw
    LabelAlign Alignment:1;		///< alignment of label content
    unsigned NoBackground:1;		///< don't draw background

    int16_t TextOffset;			///< offset of text to align labels

    int16_t X;				///< X coordinate to render the label
    int16_t Y;				///< Y coordinate to render the label
    uint16_t Width;			///< width of the label
    uint16_t Height;			///< height of the label

    xcb_drawable_t Drawable;		///< destination of label
    xcb_gcontext_t GC;			///< graphics context used for drawing

#ifdef USE_ICON
    Icon *Icon;				///< icon used in the label
#endif
    const Font *Font;			///< font for label text
    const char *Text;			///< text used in the label
} Label;

//////////////////////////////////////////////////////////////////////////////
//	Prototypes
//////////////////////////////////////////////////////////////////////////////

    /// Draw a label.
extern void LabelDraw(const Label *);

    /// Reset label with default values.
extern void LabelReset(Label *, xcb_drawable_t, xcb_gcontext_t);

/// @}

// ------------------------------------------------------------------------ //
// Menu
// ------------------------------------------------------------------------ //

//////////////////////////////////////////////////////////////////////////////
//	Declares
//////////////////////////////////////////////////////////////////////////////

///	Menu runtime structure typedef.
typedef struct _runtime_ Runtime;

/**
**	Menu or submenu typedef.
*/
typedef struct _menu_ Menu;

/**
**	Menu item typedef.
*/
typedef struct _menu_item_ MenuItem;

/**
**	Menu button typedef.
*/
typedef struct _menu_button_ MenuButton;

/**
**	Menu command typedef.
*/
typedef struct _menu_command_ MenuCommand;

/**
**	Enumeration of menu action types.
*/
typedef enum
{
    MENU_ACTION_NONE,			///< nothing
    // window actions
    MENU_ACTION_TOGGLE_STICKY,		///< toggle sticky flag of client
    MENU_ACTION_TOGGLE_MAXIMIZE,	///< toggle maximize client
    MENU_ACTION_MAXIMIZE_HORZ,		///< maximize client only horizontal
    MENU_ACTION_MAXIMIZE_VERT,		///< maximize client only vertical
    MENU_ACTION_MAXIMIZE_TILE,		///< maximize client tiled
    MENU_ACTION_MINIMIZE,		///< minimize client (iconify)
    MENU_ACTION_RESTORE,		///< restore client old size/position
    MENU_ACTION_TOGGLE_SHADE,		///< toggle shade of client
    MENU_ACTION_MOVE,			///< interactive move client
    MENU_ACTION_RESIZE,			///< interactive resize client
    MENU_ACTION_RAISE,			///< raise client
    MENU_ACTION_LOWER,			///< lower client
    MENU_ACTION_CLOSE,			///< close client
    MENU_ACTION_KILL,			///< kill client

    // global actions
    MENU_ACTION_RESTART,		///< restart µwm
    MENU_ACTION_EXIT,			///< exit µwm, optional command
    MENU_ACTION_EXECUTE,		///< execute shell command
    MENU_ACTION_FILE,			///< execute through wrapper
    MENU_ACTION_SET_LAYER,		///< set layer of client
    MENU_ACTION_SET_DESKTOP,		///< change current desktop
    MENU_ACTION_NEXT_DESKTOP,		///< next desktop
    MENU_ACTION_PREV_DESKTOP,		///< previous desktop
    MENU_ACTION_SENDTO_DESKTOP,		///< send client to desktop
    MENU_ACTION_ROOT_MENU,		///< show root menu
    MENU_ACTION_TOGGLE_SHOW_DESKTOP,	///< toggle show desktop
    MENU_ACTION_TOGGLE_SHADE_DESKTOP,	///< toggle shade desktop

    // keyboard actions
    MENU_ACTION_TASK_NEXT_WINDOW,	///< next window, in task order
    MENU_ACTION_TASK_PREV_WINDOW,	///< previous window, in task order
    MENU_ACTION_TASK_FOCUS_WINDOW,	///< focus window # in task order
    MENU_ACTION_HIDE_PANEL,		///< hide panel #
    MENU_ACTION_SHOW_PANEL,		///< show panel #
    MENU_ACTION_TOGGLE_PANEL,		///< toggle panel #

    MENU_ACTION_DIA_SHOW,		///< start dia-show plugin
    MENU_ACTION_PLAY_TD,		///< play td plugin

    // menu actions
    MENU_ACTION_SUBMENU,		///< menu item is sub - menu
    MENU_ACTION_DESKTOP,		///< autogen desktop sub - menu
    MENU_ACTION_WINDOW,			///< FIXME: autogen window sub - menu
    MENU_ACTION_SENDTO,			///< autogen sendto sub - menu
    MENU_ACTION_TILE,			///< autogen tile sub - menu
    MENU_ACTION_LAYER,			///< autogen layer sub - menu
    MENU_ACTION_DIR,			///< autogen dir sub - menu
    MENU_ACTION_DIR_PREPARED,		///< prepared dir sub - menu
    // don't add new action here, otherwise it is shown as sub menu
} MenuAction;

/**
**	Menu command structure.
*/
struct _menu_command_
{
    MenuAction Type:8;			///< menu action type
    union
    {
	int Integer;			///< integer parameter
	char *String;			///< string parameter
	Menu *Submenu;			///< sub menu parameter
    };
} __attribute__((packed));

/**
**	Menu button structure.
*/
struct _menu_button_
{
    uint32_t Buttons;			///< buttons population
    MenuCommand Commands[1];		///< button commands
};

/**
**	Menu item structure.
*/
struct _menu_item_
{
#ifdef USE_ICON
    union
    {
	char *IconName;			///< icon name (or NULL)
	Icon *Icon;			///< loaded icon to display
    };
#endif
    char *Text;				///< text to display (or NULL)

    int16_t OffsetY;			///< y-offset of menu item
#ifdef USE_ICON
    unsigned IconLoaded:1;		///< flag icon loaded
    unsigned IconOrText:1;		///< flag display icon or text
#endif

    MenuCommand Command;		///< command of menu item
} __attribute__((packed));

//////////////////////////////////////////////////////////////////////////////
//	Variables
//////////////////////////////////////////////////////////////////////////////

extern int MenuShown;			///< flag menu is shown

//////////////////////////////////////////////////////////////////////////////
//	Prototypes
//////////////////////////////////////////////////////////////////////////////

    /// Create a new empty menu.
extern Menu *MenuNew(void);

    /// Delete menu.
extern void MenuDel(Menu *);

    /// Execute a menu command.
extern void MenuCommandExecute(const MenuCommand *, int, int, void *);

    /// Free memory of menu command.
extern void MenuCommandDel(MenuCommand *);

    /// Execute commands of menu button.
extern void MenuButtonExecute(MenuButton *, int, int, int, void *);

    /// Free memory of menu button
extern void MenuButtonDel(MenuButton *);

    /// Create a new menu item.
extern MenuItem *MenuNewItem(const char *, const char *);

    /// Delete menu item.
extern void MenuDelItem(MenuItem *);

    /// Append a menu item to menu.
extern void MenuAppendItem(Menu *, MenuItem *);

    /// Parse menu command configuration.
extern void MenuCommandConfig(const ConfigObject *, MenuCommand *);

    /// Parse menu pointer buttons configuration.
extern void MenuButtonsConfig(const ConfigObject *, MenuButton **);

    /// Parse menu-item configuration.
extern MenuItem *MenuItemConfig(const ConfigObject *);

    /// Parse menu configuration.
extern void MenuConfig(const Config *);

// ------------------------------------------------------------------------ //
// Root menu
// ------------------------------------------------------------------------ //

/// @addtogroup rootmenu
/// @{

//////////////////////////////////////////////////////////////////////////////
//	Prototypes
//////////////////////////////////////////////////////////////////////////////

    /// Handle root menu button press.
extern void RootMenuHandleButtonPress(const xcb_button_press_event_t *);

    /// Build menu from directory.
extern Menu *RootMenuFromDirectory(char *);

    /// Initialize the root menu module.
extern void RootMenuInit(void);

    /// Cleanup the root menu module.
extern void RootMenuExit(void);

    /// Parse root menu configuration.
extern void RootMenuConfig(const Config *);

/// @}

// ------------------------------------------------------------------------ //
// Window menu
// ------------------------------------------------------------------------ //

/// @addtogroup windowmenu
/// @{

//////////////////////////////////////////////////////////////////////////////
//	Prototypes
//////////////////////////////////////////////////////////////////////////////

    /// Get the size of a window menu.
extern Runtime *WindowMenuGetSize(const Client *, unsigned *, unsigned *);

    /// Show a window menu.
extern void WindowMenuShow(Runtime *, int, int, Client *);

/// @}
/// @}
