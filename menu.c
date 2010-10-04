///
///	@file menu.c	@brief menu functions
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
///	@defgroup menu The menu module.
///
///	This module contains the menu display and handling functions.
///
///	@todo move menu-item complete into menu
///	@todo add string pool to each menu
///	@todo support of double clicks
///
/// @{

#include <xcb/xcb.h>
#include "uwm.h"

#ifdef USE_MENU				// {

#include <sys/queue.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dirent.h>

#include <xcb/xcb_icccm.h>
#include <xcb/xcb_atom.h>
#include <xcb/xcb_image.h>
#include <xcb/xcb_bitops.h>
#include <X11/keysym.h>			// keysym XK_

#include "core-array/core-array.h"
#include "core-rc/core-rc.h"

#include "misc.h"
#include "event.h"
#include "command.h"
#include "draw.h"
#include "image.h"
#include "screen.h"
#include "pointer.h"
#include "client.h"
#include "border.h"
#include "hints.h"
#include "keyboard.h"
#include "icon.h"
#include "menu.h"
#include "desktop.h"

#include "panel.h"
#include "plugin/task.h"

#include "dia.h"
#include "td.h"

#include "readable_bitmap.h"

#ifndef XCB_EVENT_ERROR_SUCESS
    /// Function not defined until xcb-util-0.3.4.
extern const char *xcb_event_get_label(uint8_t);
#endif

// ------------------------------------------------------------------------ //
// Dialog
// ------------------------------------------------------------------------ //

///
///	@ingroup menu
///	@defgroup dialog The dialog module.
///
///	This module contains the confirm dialog display and handling functions.
///
///	These functions and all dependencies are only available if compiled
///	widh #USE_DIALOG.
///
/// @{

#ifdef USE_DIALOG			// {

#include <stdarg.h>

/**
**	Dialog state enumeration.
*/
typedef enum
{
    DIALOG_STATE_NORMAL,		///< normal, nothing selected
    DIALOG_STATE_OK,			///< ok button selected
    DIALOG_STATE_CANCEL			///< cancel button selected
} DialogState;

/**
**	Dialog typedef.
*/
typedef struct _dialog_ Dialog;

/**
**	Dialog structure.
*/
struct _dialog_
{
    LIST_ENTRY(_dialog_) Node;		///< list of dialogs

    void (*Action) (Client *);		///< action called on OK click
    Client *Client;			///< client argument of action

    int LineN;				///< number of text lines
    char **Lines;			///< text lines

    int16_t X;				///< x-coordinate
    int16_t Y;				///< y-coordinate

    uint16_t Width;			///< width of dialog window
    uint16_t Height;			///< height of dialog window

    Client *Self;			///< dialog window

    DialogState State;			///< current state

    uint16_t ButtonWidth;		///< cached button width
};

    /// structure for dialog head
LIST_HEAD(_dialog_head_, _dialog_);

    /// list of all dialog
static struct _dialog_head_ Dialogs = LIST_HEAD_INITIALIZER(&Dialogs);

static char ShowExitConfirmation = 1;	///< show dialog for exit
char ShowKillConfirmation = 1;		///< show dialog for kill

/**
**	Display the message on the dialog window.
**
**	@param dialog	menu confirmation dialog
*/
static void DialogDrawMessage(const Dialog * dialog)
{
    int yoffset;
    int i;

    yoffset = 4;
    for (i = 0; i < dialog->LineN; i++) {
	FontDrawString(dialog->Self->Window, &Fonts.Menu, Colors.PanelFG.Pixel,
	    4, yoffset, dialog->Width, NULL, dialog->Lines[i]);
	yoffset += Fonts.Menu.Height;
    }
}

/**
**	Draw the buttons on the dialog window.
**
**	@param dialog	menu confirmation dialog
*/
static void DialogDrawButtons(const Dialog * dialog)
{
    Label label;

    LabelReset(&label, dialog->Self->Window, RootGC);

    label.Alignment = LABEL_ALIGN_CENTER;
    label.Font = &Fonts.Menu;

    label.Width = dialog->ButtonWidth;
    label.Height = Fonts.Menu.Height + 4;

    if (dialog->State == DIALOG_STATE_OK) {
	label.Type = LABEL_TASK_ACTIVE;
    } else {
	label.Type = LABEL_TASK;
    }
    label.Text = "Ok";
    label.X = dialog->Width / 3 - dialog->ButtonWidth / 2;
    label.Y = dialog->Height - Fonts.Menu.Height - Fonts.Menu.Height / 2;
    LabelDraw(&label);

    if (dialog->State == DIALOG_STATE_CANCEL) {
	label.Type = LABEL_TASK_ACTIVE;
    } else {
	label.Type = LABEL_TASK;
    }
    label.Text = "Cancel";
    label.X = 2 * dialog->Width / 3 - dialog->ButtonWidth / 2;
    LabelDraw(&label);
}

/**
**	Draw a confirm dialog.
**
**	@param dialog	menu confirmation dialog
*/
static void DialogDrawConfirm(const Dialog * dialog)
{
    DialogDrawMessage(dialog);
    DialogDrawButtons(dialog);
}

/**
**	Compute the size of a dialog window.
**
**	@param dialog	menu confirmation dialog
*/
static void DialogSetupSize(Dialog * dialog)
{
    xcb_query_text_extents_cookie_t cookie_cancel;
    xcb_query_text_extents_cookie_t cookie_ok;
    xcb_query_text_extents_cookie_t *cookies;
    unsigned width;
    unsigned height;
    int i;

    // request size of buttons
    cookie_cancel =
	FontQueryExtentsRequest(&Fonts.Menu, sizeof("Cancel") - 1, "Cancel");
    cookie_ok = FontQueryExtentsRequest(&Fonts.Menu, sizeof("Ok") - 1, "Ok");

    // request size of message
    cookies = alloca(dialog->LineN * sizeof(*cookies));
    for (i = 0; i < dialog->LineN; ++i) {
	cookies[i] =
	    FontQueryExtentsRequest(&Fonts.Menu, strlen(dialog->Lines[i]),
	    dialog->Lines[i]);
    }

    // calculate minimal window width
    dialog->Width = FontTextWidthReply(cookie_cancel);
    width = FontTextWidthReply(cookie_ok);
    dialog->Width = MAX(dialog->Width, width);
    dialog->Width += 16;

    dialog->ButtonWidth = dialog->Width;

    dialog->Width *= 3;

    for (i = 0; i < dialog->LineN; ++i) {
	width = FontTextWidthReply(cookies[i]);
	dialog->Width = MAX(dialog->Width, width);
    }
    dialog->Width += 4 * 2;

    height = Fonts.Menu.Height;
    dialog->Height = (dialog->LineN + 2) * height;

    if (dialog->Client) {
	int north;
	int south;
	int east;
	int west;

	BorderGetSize(dialog->Client, &north, &south, &east, &west);

	dialog->X =
	    dialog->Client->X + dialog->Client->Width / 2 - dialog->Width / 2;
	if (dialog->X < 0) {
	    dialog->X = 0;
	}
	if (dialog->X + (unsigned)dialog->Width >= RootWidth) {
	    dialog->X = RootWidth - dialog->Width - east - west;
	}

	dialog->Y =
	    dialog->Client->Y + dialog->Client->Height / 2 -
	    dialog->Height / 2;
	if (dialog->Y < 0) {
	    dialog->Y = 0;
	}
	if (dialog->Y + (unsigned)dialog->Height >= RootHeight) {
	    dialog->Y = RootHeight - dialog->Height - north - south;
	}
    } else {				// place in the middle of screen
	const Screen *screen;

	screen = ScreenGetPointer();

	dialog->X = screen->Width / 2 - dialog->Width / 2 + screen->X;
	dialog->Y = screen->Height / 2 - dialog->Height / 2 + screen->Y;
    }
}

/**
**	Show a confirm dialog.
**
**	@param client	client window associated with dialog
**	@param action	callback to run if "OK" is clicked
**	@param ...	list of message strings
*/
void DialogShowConfirm(Client * client, void (*action) (Client *), ...)
{
    Dialog *dialog;
    va_list ap;
    int i;
    uint32_t values[2];
    xcb_window_t window;
    xcb_size_hints_t size_hints;

    dialog = calloc(1, sizeof(*dialog));
    LIST_INSERT_HEAD(&Dialogs, dialog, Node);

    dialog->Client = client;
    dialog->Action = action;

    // get the number of lines.
    va_start(ap, action);
    while (va_arg(ap, char *))
    {
	dialog->LineN++;
    }
    va_end(ap);

    dialog->Lines = malloc(dialog->LineN * sizeof(*dialog->Lines));
    va_start(ap, action);
    for (i = 0; i < dialog->LineN; ++i) {
	dialog->Lines[i] = strdup(va_arg(ap, char *));
    }
    va_end(ap);

    DialogSetupSize(dialog);
    dialog->State = DIALOG_STATE_NORMAL;

    window = xcb_generate_id(Connection);

    values[0] = Colors.PanelBG.Pixel;
    values[1] =
	XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE |
	XCB_EVENT_MASK_EXPOSURE;
    xcb_create_window(Connection, XCB_COPY_FROM_PARENT, window, RootWindow,
	dialog->X, dialog->Y, dialog->Width, dialog->Height, 0,
	XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT,
	XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK, values);

    size_hints.flags = 0;
    xcb_size_hints_set_position(&size_hints, 0, dialog->X, dialog->Y);
    xcb_set_wm_size_hints(Connection, window, WM_NORMAL_HINTS, &size_hints);
    xcb_set_wm_name(Connection, window, STRING, sizeof("Confirm") - 1,
	"Confirm");

    dialog->Self =
	ClientAddWindow(window, xcb_get_window_attributes_unchecked(Connection,
	    window), 0, 0);

    dialog->Self->State |= WM_STATE_WMDIALOG;
    if (client) {
	dialog->Self->Owner = client->Window;
    }

    ClientFocus(dialog->Self);

    xcb_grab_button(Connection, 1, window,
	XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE,
	XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, XCB_NONE, XCB_NONE,
	XCB_GRAB_ANY, XCB_BUTTON_MASK_ANY);
}

/**
**	Destroy a confirm dialog.
**
**	@param dialog	menu confirmation dialog
*/
static void DialogDelConfirm(Dialog * dialog)
{
    int i;

    // remove frame
    ClientDelWindow(dialog->Self);

    // free message
    for (i = 0; i < dialog->LineN; ++i) {
	free(dialog->Lines[i]);
    }
    free(dialog->Lines);

    LIST_REMOVE(dialog, Node);
    free(dialog);
}

// ------------------------------------------------------------------------ //
// Events
// ------------------------------------------------------------------------ //

/**
**	Find a dialog by window.
**
**	@param window	window of dialog to search
**
**	@returns dialog if found, otherwise NULL.
*/
static Dialog *DialogFindByWindow(xcb_window_t window)
{
    Dialog *dialog;

    LIST_FOREACH(dialog, &Dialogs, Node) {
	if (dialog->Self->Window == window) {
	    return dialog;
	}
    }

    return NULL;
}

/**
**	Handle an expose event.
**
**	@param event	X11 button expose event
**
**	@returns true if event was handled, false otherwise.
*/
int DialogHandleExpose(const xcb_expose_event_t * event)
{
    Dialog *dialog;

    if ((dialog = DialogFindByWindow(event->window))) {
	Debug(4, "expose - dialog %p (%d)\n", dialog, event->count);
	DialogDrawConfirm(dialog);
	return 1;
    }
    return 0;
}

/**
**	Handle a mouse button press event.
**
**	@param event	X11 button press event
**
**	@returns true if event was handled, false otherwise.
*/
int DialogHandleButtonPress(const xcb_button_press_event_t * event)
{
    Dialog *dialog;
    int y;
    int state;

    // event belongs to any dialogs?
    if ((dialog = DialogFindByWindow(event->event))) {
	state = DIALOG_STATE_NORMAL;
	y = dialog->Height - Fonts.Menu.Height - Fonts.Menu.Height / 2;
	// button line
	if (event->event_y >= y && event->event_y < y + Fonts.Menu.Height + 4) {
	    int x;

	    // which button is pressed?
	    x = dialog->Width / 3 - dialog->ButtonWidth / 2;
	    if (event->event_x > x
		&& event->event_x <= x + dialog->ButtonWidth) {
		state = DIALOG_STATE_OK;
	    } else {
		x = 2 * dialog->Width / 3 - dialog->ButtonWidth / 2;
		if (event->event_x > x
		    && event->event_x <= x + dialog->ButtonWidth) {
		    state = DIALOG_STATE_CANCEL;
		}
	    }
	}

	dialog->State = state;
	DialogDrawButtons(dialog);
	return 1;
    }
    return 0;
}

/**
**	Handle a button release on dialog.
**
**	@param event	X11 button release event
**
**	@returns true if event was handled, false otherwise.
*/
int DialogHandleButtonRelease(const xcb_button_release_event_t * event)
{
    Dialog *dialog;
    int y;
    DialogState state;

    // event belongs to any dialogs?
    if ((dialog = DialogFindByWindow(event->event))) {
	state = DIALOG_STATE_NORMAL;
	y = dialog->Height - Fonts.Menu.Height - Fonts.Menu.Height / 2;
	// button line
	if (event->event_y >= y && event->event_y < y + Fonts.Menu.Height + 4) {
	    int x;

	    // which button is released?
	    x = dialog->Width / 3 - dialog->ButtonWidth / 2;
	    if (event->event_x >= x
		&& event->event_x < x + dialog->ButtonWidth) {
		state = DIALOG_STATE_OK;
	    } else {
		x = 2 * dialog->Width / 3 - dialog->ButtonWidth / 2;
		if (event->event_x >= x
		    && event->event_x < x + dialog->ButtonWidth) {
		    state = DIALOG_STATE_CANCEL;
		}
	    }
	}
	// press + relase not on same button, ignore it
	if (state != dialog->State) {
	    state = DIALOG_STATE_NORMAL;
	}

	if (state == DIALOG_STATE_OK) {
	    dialog->Action(dialog->Client);
	}
	if (state) {
	    DialogDelConfirm(dialog);
	} else {
	    dialog->State = state;
	    DialogDrawButtons(dialog);
	}
	return 1;
    }

    LIST_FOREACH(dialog, &Dialogs, Node) {
	if (dialog->State != DIALOG_STATE_NORMAL) {
	    dialog->State = DIALOG_STATE_NORMAL;
	    DialogDrawButtons(dialog);
	}
    }

    return 0;
}

// ---------------------------------------------------------------------------

/*
**	Initialize dialog.
*/
// void DialogInit(void) { }

/**
**	Cleanup dialog.
*/
void DialogExit(void)
{
    Dialog *dialog;

    dialog = LIST_FIRST(&Dialogs);	// faster list deletion
    while (dialog) {
	Dialog *temp;

	temp = LIST_NEXT(dialog, Node);
	DialogDelConfirm(dialog);
	dialog = temp;
    }
    LIST_INIT(&Dialogs);
}

#endif // } USE_DIALOG

/// @}

// ------------------------------------------------------------------------ //
// Label
// ------------------------------------------------------------------------ //

///
///	@ingroup menu
///	@defgroup label The generic label module.
///
///	This module contains the label display and handling functions.
///
/// @{

#ifdef USE_ICON

/**
**	Get the scaled size of an icon for a label.
**
**	@param icon		icon scaled to fit in menu keeping aspect ratio
**	@param maxsize		maximal size available in menu
**	@param[out] width	width of scaled icon
**	@param[out] height	height of scaled icon
*/
static void MenuScaledIconSize(const Icon * icon, int maxsize, unsigned *width,
    unsigned *height)
{
    unsigned long ratio;

    // width to height
    ratio = (unsigned long)(icon->Image->Width * 65536) / icon->Image->Height;
    if (icon->Image->Width > icon->Image->Height) {	// compute size wrt width
	*width = (maxsize * ratio) / 65536;
	*height = (*width * 65536) / ratio;
    } else {				// compute size wrt height
	*height = (maxsize * 65536) / ratio;
	*width = (*height * ratio) / 65536;
    }

}
#endif

/**
**	Draw a label.
**
**	Used for menus and panel plugins.
**
**	@param label	label to draw
*/
void LabelDraw(const Label * label)
{
    xcb_drawable_t drawable;
    xcb_gcontext_t gc;
    int x;
    int y;
    unsigned width;
    unsigned height;
    uint32_t fg_pixel;
    uint32_t bg1_pixel;
    uint32_t bg2_pixel;
    uint32_t outline_pixel;
    uint32_t top_pixel;
    uint32_t bottom_pixel;
    xcb_rectangle_t rectangle;
    unsigned icon_width;
    unsigned icon_height;
    int text_offset;
    unsigned text_width;
    unsigned text_height;
    int xoffset;
    int yoffset;
    xcb_query_text_extents_cookie_t cookie_text;

    NO_WARNING(cookie_text);
    NO_WARNING(text_offset);

    // send request early FIXME: cache text length?
    if (label->Text) {
	cookie_text =
	    FontQueryExtentsRequest(label->Font, strlen(label->Text),
	    label->Text);
    }

    drawable = label->Drawable;
    gc = label->GC;
    x = label->X;
    y = label->Y;
    width = label->Width;
    height = label->Height;

    //
    //	Determine the colors to use
    //
    switch (label->Type) {
	case LABEL_MENU_LABEL:
	    fg_pixel = Colors.MenuFG.Pixel;
	    bg1_pixel = Colors.MenuBG.Pixel;
	    bg2_pixel = Colors.MenuBG.Pixel;
	    outline_pixel = Colors.MenuBG.Pixel;
	    top_pixel = Colors.MenuBG.Pixel;
	    bottom_pixel = Colors.MenuBG.Pixel;
	    break;
	case LABEL_MENU_ACTIVE:
	    fg_pixel = Colors.MenuActiveFG.Pixel;
	    bg1_pixel = Colors.MenuActiveBG1.Pixel;
	    bg2_pixel = Colors.MenuActiveBG2.Pixel;
	    if (bg1_pixel == bg2_pixel) {
		outline_pixel = Colors.MenuActiveOutline.Pixel;
	    } else {
		outline_pixel = Colors.MenuActiveDown.Pixel;
	    }
	    top_pixel = Colors.MenuActiveUp.Pixel;
	    bottom_pixel = Colors.MenuActiveDown.Pixel;
	    break;
	case LABEL_TASK:
	    fg_pixel = Colors.TaskFG.Pixel;
	    bg1_pixel = Colors.TaskBG1.Pixel;
	    bg2_pixel = Colors.TaskBG2.Pixel;
	    top_pixel = Colors.TaskUp.Pixel;
	    bottom_pixel = Colors.TaskDown.Pixel;
	    outline_pixel = bottom_pixel;
	    break;
	case LABEL_TASK_ACTIVE:
	    fg_pixel = Colors.TaskActiveFG.Pixel;
	    bg1_pixel = Colors.TaskActiveBG1.Pixel;
	    bg2_pixel = Colors.TaskActiveBG2.Pixel;
	    top_pixel = Colors.TaskActiveDown.Pixel;
	    bottom_pixel = Colors.TaskActiveUp.Pixel;
	    outline_pixel = bottom_pixel;
	    break;
	case LABEL_PANEL:
	    fg_pixel = Colors.PanelButtonFG.Pixel;
	    bg1_pixel = Colors.TaskBG1.Pixel;
	    bg2_pixel = Colors.TaskBG2.Pixel;
	    top_pixel = Colors.TaskUp.Pixel;
	    bottom_pixel = Colors.TaskDown.Pixel;
	    outline_pixel = bottom_pixel;
	    break;
	case LABEL_PANEL_ACTIVE:
	    fg_pixel = Colors.PanelButtonFG.Pixel;
	    bg1_pixel = Colors.TaskActiveBG1.Pixel;
	    bg2_pixel = Colors.TaskActiveBG2.Pixel;
	    top_pixel = Colors.TaskActiveDown.Pixel;
	    bottom_pixel = Colors.TaskActiveUp.Pixel;
	    outline_pixel = bottom_pixel;
	    break;
	case LABEL_MENU:
	default:
	    fg_pixel = Colors.MenuFG.Pixel;
	    bg1_pixel = Colors.MenuBG.Pixel;
	    bg2_pixel = Colors.MenuBG.Pixel;
	    outline_pixel = Colors.MenuDown.Pixel;
	    top_pixel = Colors.MenuUp.Pixel;
	    bottom_pixel = Colors.MenuDown.Pixel;
	    break;
    }

    //
    //	Draw the background
    //
    switch (label->Type) {
	case LABEL_TASK:
	case LABEL_PANEL:
	    // flat taskbuttons (only icon) for width < 48
	    if (width < 48) {
		break;
	    }
	default:
	    // draw the label background
	    xcb_change_gc(Connection, gc, XCB_GC_FOREGROUND, &bg1_pixel);
	    if (bg1_pixel == bg2_pixel) {	// single color
		rectangle.x = x + LABEL_BORDER;
		rectangle.y = y + LABEL_BORDER;
		rectangle.width = width - LABEL_BORDER;
		rectangle.height = height - LABEL_BORDER;
		xcb_poly_fill_rectangle(Connection, drawable, gc, 1,
		    &rectangle);
	    } else {			// gradient
		GradientDrawHorizontal(drawable, gc, bg1_pixel, bg2_pixel,
		    x + LABEL_BORDER, y + LABEL_BORDER, width - LABEL_BORDER,
		    height - LABEL_BORDER);
	    }

	    // draw the outline
	    xcb_change_gc(Connection, gc, XCB_GC_FOREGROUND, &outline_pixel);
	    rectangle.x = x;
	    rectangle.y = y;
	    rectangle.width = width;
	    rectangle.height = height;
	    xcb_poly_rectangle(Connection, drawable, gc, 1, &rectangle);
	    break;
    }

    // determine the size of the icon (if any) to display
    icon_width = 0;
    icon_height = 0;
#ifdef USE_ICON
    if (label->Icon) {
	if (width < height) {
	    MenuScaledIconSize(label->Icon,
		width - 2 * LABEL_INNER_SPACE - LABEL_BORDER, &icon_width,
		&icon_height);
	} else {
	    MenuScaledIconSize(label->Icon,
		height - 2 * LABEL_INNER_SPACE - LABEL_BORDER, &icon_width,
		&icon_height);
	}
    }
#endif

    // determine how much room is left for text
    text_width = 0;
    text_height = 0;
    if (label->Text) {
	if (label->TextOffset) {
	    text_offset = label->TextOffset;
	} else {
	    text_offset = icon_width;
	    if (icon_width) {
		text_offset += LABEL_INNER_SPACE;
	    }
	}

	text_height = label->Font->Height;
	text_width = FontTextWidthReply(cookie_text);

	if (text_width + text_offset + 2 * LABEL_INNER_SPACE +
	    2 * LABEL_BORDER > width) {
	    if (width <
		text_offset + 2U * LABEL_INNER_SPACE + 2 * LABEL_BORDER) {
		text_width = 0;
	    } else {
		text_width =
		    width - text_offset - 2 * LABEL_INNER_SPACE +
		    2 * LABEL_BORDER;
	    }
	}
    }
    // determine the offset of the icon and/or text in the label
    switch (label->Alignment) {
	case LABEL_ALIGN_CENTER:
	    xoffset = width / 2 - (icon_width + text_width) / 2;
	    if (xoffset < 0) {
		xoffset = 0;
	    }
	    break;
	case LABEL_ALIGN_LEFT:
	default:
	    xoffset = LABEL_BORDER + LABEL_INNER_SPACE;
	    break;
    }

#ifdef USE_ICON
    // display the icon
    if (label->Icon) {
	yoffset = height / 2 - icon_height / 2;
	IconDraw(label->Icon, drawable, x + xoffset, y + yoffset, icon_width,
	    icon_height);
    }
#endif
    // display the label
    if (label->Text && text_width) {
	xoffset += text_offset;
	yoffset = height / 2 - icon_height / 2;
	yoffset = height / 2 - text_height / 2;
	FontDrawString(drawable, label->Font, fg_pixel, x + xoffset,
	    y + yoffset, text_width, NULL, label->Text);
    }
}

/**
**	Reset label with default values.
**
**	@param label	structure to reset
**	@param drawable drawable window/pixmap to use
**	@param gc	graphics context to use
*/
void LabelReset(Label * label, xcb_drawable_t drawable, xcb_gcontext_t gc)
{
    memset(label, 0, sizeof(*label));

    label->Type = LABEL_MENU;
    label->Width = LABEL_BORDER;
    label->Height = LABEL_BORDER;
    label->Drawable = drawable;
    label->GC = gc;
    label->Font = &Fonts.Panel;
}

/// @}

// ------------------------------------------------------------------------ //
// Menu
// ------------------------------------------------------------------------ //

/**
**	Menu runtime structure.
*/
struct _runtime_
{
    Menu *Menu;				///< static menu data

    xcb_window_t Window;		///< menu window
    Runtime *Parent;			///< the parent menu (NULL for none)
    int16_t ParentOffset;		///< y-offset of this menu wrt parent
    int16_t X;				///< x-coordinate of the menu
    int16_t Y;				///< y-coordinate of the menu
    uint16_t Width;			///< width of the menu
    uint16_t Height;			///< height of the menu
    int16_t TextOffset;			///< x-offset of text in the menu
    uint8_t ItemHeight;			///< menu item height

    int16_t CurrentIndex;		///< current menu selection
    int16_t LastIndex;			///< last menu selection

    // FIXME: int16_t OffsetY[1];	///< y-offset of menu item
};

/**
**	Menu or submenu structure.
**
**	@todo allocate table for menu items at end of menu structure.
*/
struct _menu_
{
    char *Label;			///< menu label (NULL for no label)

    uint8_t UserHeight;			///< menu user item height

    int16_t ItemCount;			///< number of menu items in table
    MenuItem **ItemTable;		///< menu item table
};

int MenuShown;				///< flag menu is shown

static const Client *MenuClient;	///< client of menu
static MenuCommand MenuCommandSelected;	///< selected menu command
static uint32_t MenuOpacity = UINT32_MAX;	///< menu window transparency
static Menu **Menus;			///< table of menus
static int MenuN;			///< number of menus in table

static xcb_pixmap_t MenuArrowPixmap;	///< cached sub-menu arrow pixmap

/**
**	Submenu arrow, 4x7 pixels
**@{
*/
#define SUB_MENU_ARROW_WIDTH 4
#define SUB_MENU_ARROW_HEIGHT 7
static const uint8_t MenuSubmenuArrowBitmap[SUB_MENU_ARROW_HEIGHT] = {
// *INDENT-OFF*
    O_______,
    OO______,
    OOO_____,
    OOOO____,
    OOO_____,
    OO______,
    O_______,
// *INDENT-ON*
};

//@}

///	locale forward definitions
///@{

static int WindowMenuUserHeight;

static Runtime *MenuPrepareRuntime(Menu *);
static void MenuCleanupRuntime(Runtime *);
static int MenuExecuteRuntime(Runtime *, Runtime *, int, int);

static void MenuCommandPrepare(MenuCommand *);
static void MenuCommandCleanup(MenuCommand *);
static void MenuCommandCopy(MenuCommand *, const MenuCommand *);

static void RootMenuExecute(void
    __attribute__ ((unused)) *, const MenuCommand *);

static void RootMenuShow(int, int, int);

static void WindowMenuChoose(const MenuCommand *);
static Menu *WindowMenuCreateLayer(int);

static Menu *MenuConfigMenu(const ConfigObject *);

///@}

// ---------------------------------------------------------------------------

/**
**	Determine if a menu is valid (and can be shown).
**
**	Menus containing no name or icon are considerate as invalid.
**	Only the first level is checked.
**
**	@param menu	menu to be checked
**
**	@returns true if valid menu, false if invalid.
*/
static int MenuIsValid(const Menu * menu)
{
    if (menu) {
	int i;

	for (i = 0; i < menu->ItemCount; ++i) {
	    if (menu->ItemTable[i]->Text
#ifdef USE_ICON
		|| menu->ItemTable[i]->IconName
#endif
		) {
		return 1;
	    }
	}
    }
    return 0;
}

/**
**	Get the menu item associated with an index.
**
**	@param runtime	menu runtime to get element from
**	@param index	get element at this index
**
**	@returns menu item at index or last, NULL if index is negative.
*/
static MenuItem *MenuGetItem(const Runtime * runtime, int index)
{
    return index >= 0 ? runtime->Menu->ItemTable[index] : NULL;
}

/**
**	Get the index of previous item in the menu.
**
**	Skips separators.
**
**	@param runtime	menu runtime
**
**	@returns previous valid index, last-index if not found.
*/
static int MenuGetPreviousIndex(const Runtime * runtime)
{
    int i;

    for (i = runtime->CurrentIndex - 1; i >= 0; i--) {
	const MenuItem *item;

	item = runtime->Menu->ItemTable[i];
	if (item->Text
#ifdef USE_ICON
	    || item->Icon
#endif
	    ) {
	    return i;			// valid entry
	}
    }
    // FIXME: what if last entry is a separator?
    return runtime->Menu->ItemCount - 1;
}

/**
**	Get the index of next item in the menu.
**
**	Skips separators.
**
**	@param runtime	menu runtime
**
**	@returns next valid index, 0 if not found.
*/
static int MenuGetNextIndex(const Runtime * runtime)
{
    int i;

    for (i = runtime->CurrentIndex + 1; i < runtime->Menu->ItemCount; i++) {
	const MenuItem *item;

	item = runtime->Menu->ItemTable[i];
	if (item->Text
#ifdef USE_ICON
	    || item->Icon
#endif
	    ) {
	    return i;			// valid entry
	}
    }
    // FIXME: what if first entry is a separator?
    return 0;
}

/**
**	Get the item in the menu given a y-coordinate.
**
**	@param runtime	menu runtime
**	@param y	y-coordinate to find index
**
**	@returns the menu index under y, -1 if label, 0 if out of range.
*/
static int MenuGetIndexByY(const Runtime * runtime, int y)
{
    int i;

    if (runtime->Menu->ItemCount) {	// none empty menu
	if (y < runtime->Menu->ItemTable[0]->OffsetY) {
	    return -1;			// label
	}
	for (i = 0; i < runtime->Menu->ItemCount - 1; i++) {
	    if (y >= runtime->Menu->ItemTable[i]->OffsetY
		&& y < runtime->Menu->ItemTable[i + 1]->OffsetY) {
		return i;
	    }
	}
	return i;			// last entry
    }
    return 0;
}

/**
**	Set the active menu item.
**
**	@param runtime	menu runtime
**	@param index	new active menu item
*/
static void MenuSetPosition(Runtime * runtime, int index)
{
    int y;
    int updated;

    // middle of menu item
    y = runtime->Menu->ItemTable[index]->OffsetY + runtime->ItemHeight / 2;

    //
    //	Menu bigger than screen, must scroll.
    //
    if (runtime->Height >= RootHeight) {
	updated = 0;
	while (runtime->Y + y < runtime->ItemHeight / 2) {
	    runtime->Y += runtime->ItemHeight;
	    updated = 1;
	}
	while (runtime->Y + y > (int)RootHeight) {
	    runtime->Y -= runtime->ItemHeight;
	    updated = -1;
	}

	// move window to have selected item visible
	if (updated) {
	    uint32_t values[1];

	    values[0] = runtime->Y;
	    xcb_configure_window(Connection, runtime->Window,
		XCB_CONFIG_WINDOW_Y, values);
	    // move out of scroll area
	    y += updated;
	}
    }
    // we need to do this twice so the event gets registered on the submenu,
    // if one exists.
    // FIXME: don't wrap x
    PointerWrap(runtime->Window, 6, y);
    PointerWrap(runtime->Window, 6, y);
}

// ------------------------------------------------------------------------ //
// Drawing

/**
**	Create window and map menu window.
**
**	@param runtime	menu runtime to draw
**	@param x	x-coordinate of menu
**	@param y	y-coordinate of menu
**
**	@todo don't create window for all entries, if bigger than screen-size.
*/
static void MenuCreateWindow(Runtime * runtime, int x, int y)
{
    uint32_t values[3];

    runtime->LastIndex = -1;
    runtime->CurrentIndex = -1;

    //
    //	 Check if menu fits on screen
    //
    if (x + runtime->Width > (int)RootWidth) {
	if (runtime->Parent) {
	    x = runtime->Parent->X - runtime->Width;
	} else {
	    x = RootWidth - runtime->Width;
	}
    }
    runtime->ParentOffset = y;
    if (y + runtime->Height > (int)RootHeight) {
	y = RootHeight - runtime->Height;
    }
    if (y < 0) {
	y = 0;
    }
    runtime->X = x;
    runtime->Y = y;
    runtime->ParentOffset -= y;

    runtime->Window = xcb_generate_id(Connection);

    values[0] = Colors.MenuBG.Pixel;
    values[1] = 1;
    values[2] = XCB_EVENT_MASK_EXPOSURE;
    xcb_create_window(Connection, XCB_COPY_FROM_PARENT, runtime->Window,
	RootWindow, x, y, runtime->Width, runtime->Height, 0,
	XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT,
	XCB_CW_BACK_PIXEL | XCB_CW_SAVE_UNDER | XCB_CW_EVENT_MASK, values);

    if (MenuOpacity != UINT32_MAX) {
	AtomSetCardinal(runtime->Window, &Atoms.NET_WM_WINDOW_OPACITY,
	    MenuOpacity);
    }
    // raise window
    values[0] = XCB_STACK_MODE_ABOVE;
    xcb_configure_window(Connection, runtime->Window,
	XCB_CONFIG_WINDOW_STACK_MODE, values);
    // map window on the screen
    xcb_map_window(Connection, runtime->Window);
}

/**
**	Destroy menu window.
**
**	@param runtime	menu runtime
*/
static inline void MenuDestroyWindow(Runtime * runtime)
{
    xcb_destroy_window(Connection, runtime->Window);
}

/**
**	Draw menu label.
**
**	@param runtime	menu runtime which contains the menu label
*/
static void MenuDrawLabel(const Runtime * runtime)
{
    Label label;

    if (runtime->Menu->Label) {
	LabelReset(&label, runtime->Window, RootGC);

	label.Type = LABEL_MENU_LABEL;
	label.Alignment = LABEL_ALIGN_CENTER;

	label.X = MENU_INNER_SPACE;
	label.Y = MENU_INNER_SPACE;
	label.Width = runtime->Width - MENU_INNER_SPACE * 2 - 1;
	label.Height = runtime->ItemHeight - MENU_INNER_SPACE;

	label.Font = &Fonts.Menu;
	label.Text = runtime->Menu->Label;
	LabelDraw(&label);
    }
}

/**
**	Draw sub-menu arrow.
**
**	@param runtime	menu runtime data
**	@param item	menu item with sub-menu arrow
**	@param pixel	color of sub-menu arrow
*/
static void MenuDrawArrow(const Runtime * runtime, const MenuItem * item,
    uint32_t pixel)
{
    uint32_t values[4];
    xcb_rectangle_t rectangle;

    values[0] = pixel;
    values[1] = runtime->Width - SUB_MENU_ARROW_WIDTH - MENU_INNER_SPACE * 2;
    values[2] =
	item->OffsetY + runtime->ItemHeight / 2 - SUB_MENU_ARROW_HEIGHT / 2;
    values[3] = MenuArrowPixmap;
    xcb_change_gc(Connection, RootGC,
	XCB_GC_FOREGROUND | XCB_GC_CLIP_ORIGIN_X | XCB_GC_CLIP_ORIGIN_Y |
	XCB_GC_CLIP_MASK, values);

    rectangle.x = runtime->Width - SUB_MENU_ARROW_WIDTH - MENU_INNER_SPACE * 2;
    rectangle.y =
	item->OffsetY + runtime->ItemHeight / 2 - SUB_MENU_ARROW_HEIGHT / 2;
    rectangle.width = SUB_MENU_ARROW_WIDTH;
    rectangle.height = SUB_MENU_ARROW_HEIGHT;
    xcb_poly_fill_rectangle(Connection, runtime->Window, RootGC, 1,
	&rectangle);

    values[0] = XCB_NONE;
    xcb_change_gc(Connection, RootGC, XCB_GC_CLIP_MASK, values);
}

/**
**	Draw an unselected menu item.
**
**	@param runtime	menu runtime which contains the menu item
**	@param item	menu item to draw (NULL to draw label)
*/
static void MenuDrawItem(const Runtime * runtime, const MenuItem * item)
{
    Label label;

    if (item->Text
#ifdef USE_ICON
	|| item->Icon
#endif
	) {				// used item
	LabelReset(&label, runtime->Window, RootGC);

	label.Type = LABEL_MENU_LABEL;

	label.TextOffset = runtime->TextOffset;
	label.X = MENU_INNER_SPACE;
	label.Y = item->OffsetY;
	label.Width = runtime->Width - MENU_INNER_SPACE * 2 - 1;
	label.Height = runtime->ItemHeight;

#ifdef USE_ICON
	label.Icon = item->Icon;
#endif
	label.Font = &Fonts.Menu;
	label.Text = item->Text;
	LabelDraw(&label);
    } else {				// separator
	xcb_point_t points[2];

	xcb_change_gc(Connection, RootGC, XCB_GC_FOREGROUND,
	    &Colors.MenuDown.Pixel);
	points[0].x = MENU_INNER_SPACE * 2;
	points[1].x = runtime->Width - MENU_INNER_SPACE * 4;
	points[0].y = item->OffsetY + MENU_INNER_SPACE;
	points[1].y = item->OffsetY + MENU_INNER_SPACE;
	xcb_poly_line(Connection, XCB_COORD_MODE_ORIGIN, runtime->Window,
	    RootGC, 2, points);

	xcb_change_gc(Connection, RootGC, XCB_GC_FOREGROUND,
	    &Colors.MenuUp.Pixel);
	points[0].y = item->OffsetY + MENU_INNER_SPACE + 1;
	points[1].y = item->OffsetY + MENU_INNER_SPACE + 1;
	xcb_poly_line(Connection, XCB_COORD_MODE_ORIGIN, runtime->Window,
	    RootGC, 2, points);
    }

    //
    //	Draw arrow for submenu
    //
    if (item->Command.Type >= MENU_ACTION_SUBMENU) {
	MenuDrawArrow(runtime, item, Colors.MenuFG.Pixel);
    }
}

/**
**	Draw a selected menu item.
**
**	@param runtime	menu runtime which contains the menu item
**	@param item	menu item to draw
*/
static void MenuDrawSelectedItem(const Runtime * runtime,
    const MenuItem * item)
{
    if (item && (
#ifdef USE_ICON
	    item->Icon ||
#endif
	    item->Text)) {		// no separator
	Label label;

	LabelReset(&label, runtime->Window, RootGC);
	label.Type = LABEL_MENU_ACTIVE;

	label.TextOffset = runtime->TextOffset;
	label.X = MENU_INNER_SPACE;
	label.Y = item->OffsetY;
	label.Width = runtime->Width - MENU_INNER_SPACE * 2 - 1;
	label.Height = runtime->ItemHeight;

#ifdef USE_ICON
	label.Icon = item->Icon;
#endif
	label.Font = &Fonts.Menu;
	label.Text = item->Text;
	LabelDraw(&label);

	//
	//	Draw arrow of submenu
	//
	if (item->Command.Type >= MENU_ACTION_SUBMENU) {
	    MenuDrawArrow(runtime, item, Colors.MenuActiveFG.Pixel);
	}
    }
}

/**
**	Update the menu selection.
**
**	@param runtime	menu runtime to update
*/
static void MenuUpdate(const Runtime * runtime)
{
    // clear the old selection
    if (runtime->LastIndex >= 0) {
	MenuDrawItem(runtime, MenuGetItem(runtime, runtime->LastIndex));
    }
    // highlight the new selection
    MenuDrawSelectedItem(runtime, MenuGetItem(runtime, runtime->CurrentIndex));
}

/**
**	Draw a menu.
**
**	@param runtime	menu runtime to draw
*/
static void MenuDraw(const Runtime * runtime)
{
    int i;
    xcb_point_t points[3];
    const Menu *menu;

    menu = runtime->Menu;
    if (menu->Label) {
	MenuDrawLabel(runtime);
    }
    for (i = 0; i < menu->ItemCount; ++i) {
	if (i == runtime->CurrentIndex) {
	    MenuDrawSelectedItem(runtime, menu->ItemTable[i]);
	} else {
	    MenuDrawItem(runtime, menu->ItemTable[i]);
	}
    }

    //
    //	Draw border around menu
    //
    xcb_change_gc(Connection, RootGC, XCB_GC_FOREGROUND, &Colors.MenuUp.Pixel);
    points[0].x = 0;
    points[0].y = runtime->Height - 1;
    points[1].x = 0;
    points[1].y = 0;
    points[2].x = runtime->Width - 1;
    points[2].y = 0;
    xcb_poly_line(Connection, XCB_COORD_MODE_ORIGIN, runtime->Window, RootGC,
	3, points);
    // FIXME: poly_segments better?
    points[0].x = 1;
    points[0].y = runtime->Height - 2;
    points[1].x = 1;
    points[1].y = 1;
    points[2].x = runtime->Width - 2;
    points[2].y = 1;
    xcb_poly_line(Connection, XCB_COORD_MODE_ORIGIN, runtime->Window, RootGC,
	3, points);

    xcb_change_gc(Connection, RootGC, XCB_GC_FOREGROUND,
	&Colors.MenuDown.Pixel);
    points[0].x = 1;
    points[0].y = runtime->Height - 1;
    points[1].x = runtime->Width - 1;
    points[1].y = runtime->Height - 1;
    points[2].x = runtime->Width - 1;
    points[2].y = 1;
    xcb_poly_line(Connection, XCB_COORD_MODE_ORIGIN, runtime->Window, RootGC,
	3, points);
    points[0].x = 2;
    points[0].y = runtime->Height - 2;
    points[1].x = runtime->Width - 2;
    points[1].y = runtime->Height - 2;
    points[2].x = runtime->Width - 2;
    points[2].y = 2;
    xcb_poly_line(Connection, XCB_COORD_MODE_ORIGIN, runtime->Window, RootGC,
	3, points);
}

/**
**	Draw a menu and all its parents.
**
**	@param runtime	menu runtime data
*/
static void MenuDrawTree(const Runtime * runtime)
{
    if (runtime->Parent) {		// start with lowest = root
	MenuDrawTree(runtime->Parent);
    }

    MenuDraw(runtime);
}

// ------------------------------------------------------------------------ //
// Loop

/**
**	Determine the action to take on key press.
**
**	@param runtime	menu runtime variables
**	@param event	x11 key press event
**
**	@returns -1 leave, 0 no selection, 1 sub-menu.
**
**	@todo use configurable key bindings.
*/
static int MenuHandleKey(Runtime * runtime,
    const xcb_key_press_event_t * event)
{
    int i;
    Runtime *parent;

    if (runtime->CurrentIndex >= 0 || !runtime->Parent) {
	parent = runtime;
    } else {
	parent = runtime->Parent;
    }

    i = -1;
    switch (KeyboardGet(event->detail, event->state)) {
	case XK_Up:			// select previous menu item
	    i = MenuGetPreviousIndex(parent);
	    break;
	case XK_Down:			// select next menu item
	    i = MenuGetNextIndex(parent);
	    break;
	case XK_Right:			// select sub-menu item
	    parent = runtime;
	    i = 0;
	    break;
	case XK_Left:			// select parent menu
	    if (parent->Parent) {
		parent = parent->Parent;
		if (parent->CurrentIndex >= 0) {
		    i = parent->CurrentIndex;
		} else {
		    i = 0;
		}
	    }
	    break;
	case XK_Escape:		// leave menu without selection
	    return -1;
	case XK_Return:		// select menu item
	    // item is parent => enter sub-menu
	    if (parent != runtime) {
		parent = runtime;
		i = 0;
		break;
	    }
	    // valid menu entry selected, copy it for execute
	    if (parent->CurrentIndex >= 0) {
		MenuCommandCopy(&MenuCommandSelected, &MenuGetItem(parent,
			parent->CurrentIndex)->Command);
	    }
	    return 1;
	default:
	    break;
    }

    if (i >= 0) {			// something selected
	MenuSetPosition(parent, i);
    }
    return 0;
}

/**
**	Determine the action to take on button press.
**
**	@param runtime	menu runtime, which got a button press
**	@param event	button press event for menu
*/
static void MenuHandleButtonPress(Runtime * runtime,
    const xcb_button_press_event_t * event)
{
    int i;
    Runtime *parent;

    if (runtime->CurrentIndex >= 0 || !runtime->Parent) {
	parent = runtime;
    } else {
	parent = runtime->Parent;
    }

    i = -1;
    switch (event->detail) {
	case XCB_BUTTON_INDEX_4:	// scroll-wheel up
	    i = MenuGetPreviousIndex(parent);
	    break;
	case XCB_BUTTON_INDEX_5:	// scroll-wheel down
	    i = MenuGetNextIndex(parent);
	    break;
    }

    if (i >= 0) {
	MenuSetPosition(parent, i);
    }

    return;
}

/**
**	Determine the action to take given an event.
**
**	@param runtime	menu runtime of current handled menu
**	@param event	X11 motion notify event
**
**	@returns -1 leave, 0 no selection, 1 sub-menu.
*/
int MenuHandleMotionNotify(Runtime * runtime,
    xcb_motion_notify_event_t * event)
{
    int x;
    int y;
    xcb_window_t child;
    MenuItem *item;

    DiscardMotionEvents(&event, runtime->Window);

    child = event->child;
    x = event->root_x - runtime->X;
    y = event->root_y - runtime->Y;

    //
    //	update the selection of the current menu
    //
    if (x > 0 && y > 0 && x < runtime->Width && y < runtime->Height) {
	runtime->CurrentIndex = MenuGetIndexByY(runtime, y);
    } else {
	Runtime *parent;

	if ((parent = runtime->Parent)) {
	    if (child == parent->Window) {
		// leave if over the parent, but not on this selection
		if (y < runtime->ParentOffset
		    || y > parent->ItemHeight + runtime->ParentOffset) {
		    return -1;
		}
	    } else {
		// leave if over a menu window
		for (parent = parent->Parent; parent; parent = parent->Parent) {
		    if (parent->Window == child) {
			return -1;
		    }
		}
	    }
	}
	runtime->CurrentIndex = -1;
    }
    // scroll the menu if needed
    if (runtime->Height > RootHeight && runtime->CurrentIndex >= 0) {
	// if near the top, shift down
	if (runtime->Y + y < runtime->ItemHeight / 2) {
	    if (runtime->CurrentIndex > 0) {
		MenuSetPosition(runtime, --runtime->CurrentIndex);
	    }
	}
	// if near the bottom, shift up
	if (runtime->Y + y + runtime->ItemHeight / 2 > (int)RootHeight) {
	    if (runtime->CurrentIndex + 1 < runtime->Menu->ItemCount) {
		MenuSetPosition(runtime, ++runtime->CurrentIndex);
	    }
	}
    }
    // redraw if selected item changed
    if (runtime->LastIndex != runtime->CurrentIndex) {
	MenuUpdate(runtime);
	runtime->LastIndex = runtime->CurrentIndex;
    }
    // if the selected item is a submenu, show it
    item = MenuGetItem(runtime, runtime->CurrentIndex);
    if (item && item->Command.Type >= MENU_ACTION_SUBMENU) {
	Runtime *submenu;
	int status;

	MenuCommandPrepare(&item->Command);
	if (MenuIsValid(item->Command.Submenu)) {
	    submenu = MenuPrepareRuntime(item->Command.Submenu);
	    // FIXME: align menu better with selected item (label...)
	    status =
		MenuExecuteRuntime(submenu, runtime,
		runtime->X + runtime->Width,
		runtime->Y +
		runtime->Menu->ItemTable[runtime->CurrentIndex]->OffsetY);
	    MenuCleanupRuntime(submenu);
	    MenuCommandCleanup(&item->Command);

	    if (status) {		// item selected; destroy the menu tree
		return status;
	    }
	    MenuUpdate(runtime);
	}
    }

    return 0;
}

/**
**	Menu event loop.
**
**	@param runtime	menu runtime
**
**	@returns 0 if nothing selected or 1 if something selected.
*/
static int MenuLoop(Runtime * runtime)
{
    int enter_x;
    int enter_y;
    int press_x;
    int press_y;
    int moved;
    xcb_generic_event_t *event;
    const xcb_button_press_event_t *bpe;
    const xcb_button_release_event_t *bre;

    press_x = -100;
    press_y = -100;
    PointerGetPosition(&enter_x, &enter_y);
    moved = 0;

    while (KeepLooping) {
	while ((event = PollNextEvent())) {

	    switch (XCB_EVENT_RESPONSE_TYPE(event)) {
		case XCB_EXPOSE:
		    // FIXME: look here, only if not menu window?
		    EventHandleEvent(event);
		    // ignore this until last
		    if (!((const xcb_expose_event_t *)event)->count) {
			MenuDrawTree(runtime);
		    }
		    break;

		case XCB_BUTTON_PRESS:
		    bpe = (const xcb_button_press_event_t *)event;
		    enter_x = -100;
		    enter_y = -100;
		    moved = 1;
		    MenuHandleButtonPress(runtime, bpe);
		    break;

		case XCB_BUTTON_RELEASE:
		    bre = (const xcb_button_release_event_t *)event;
		    // FIXME: buttons should be configurable
		    if (bre->detail == XCB_BUTTON_INDEX_4) {
			break;
		    }
		    if (bre->detail == XCB_BUTTON_INDEX_5) {
			break;
		    }
		    if (!moved) {
			break;
		    }
		    // pointer only a little moved
		    if (abs(bre->root_x - enter_x) < DoubleClickDelta
			&& abs(bre->root_y - enter_y) < DoubleClickDelta) {
			break;
		    }
		    Debug(4, "button release\n");
		    // entry selected and no sub directory
		    if (runtime->CurrentIndex >= 0) {
			// no action on sub-menus for touch-screens
			if (runtime->Menu->ItemTable[runtime->CurrentIndex]
			    ->Command.Type >= MENU_ACTION_SUBMENU) {
			    Debug(3, "sub-menu %d\n", runtime->CurrentIndex);
			    break;
			}
			MenuCommandCopy(&MenuCommandSelected,
			    &runtime->Menu->ItemTable[runtime->CurrentIndex]
			    ->Command);
		    } else {
			Debug(4, "no valid menu-entry\n");
			if (runtime->Parent
			    && runtime->Parent->Window == bre->child) {
			    Debug(3, "have parent\n");
			    break;
			}
		    }
		    free(event);
		    return 1;

		case XCB_KEY_PRESS:
		    moved = 1;
		    switch (MenuHandleKey(runtime,
			    (const xcb_key_press_event_t *)event)) {
			case 0:	// nothing
			    break;
			case -1:	// mouse left the menu
			    free(event);
			    return 0;
			default:	// selection made
			    // FIXME: but back event?
			    xcb_allow_events(Connection,
				XCB_ALLOW_REPLAY_KEYBOARD, XCB_CURRENT_TIME);
			    free(event);
			    return 1;
		    }
		    break;

		case XCB_MOTION_NOTIFY:
		    moved = 1;
		    switch (MenuHandleMotionNotify(runtime,
			    (xcb_motion_notify_event_t *) event)) {
			case 0:	// nothing
			    break;
			case -1:	// mouse left the menu
			    free(event);
			    return 0;
			default:	// selection made
			    // FIXME: but back event?
			    xcb_allow_events(Connection,
				XCB_ALLOW_REPLAY_POINTER, XCB_CURRENT_TIME);
			    free(event);
			    return 1;
		    }
		    break;

		default:
		    EventHandleEvent(event);
		    break;
	    }
	    free(event);
	}
	WaitForEvent();
    }
    return 0;
}

// ------------------------------------------------------------------------ //
// Runtime

/**
**	Prepare menu to be shown.
**
**	@param menu	static menu data
**
**	@returns runtime structure for menu.
*/
static Runtime *MenuPrepareRuntime(Menu * menu)
{
    int submenu_offset;
    int i;
    xcb_query_text_extents_cookie_t cookie_label;
    xcb_query_text_extents_cookie_t *cookies;
    Runtime *runtime;

    NO_WARNING(cookie_label);
    NO_WARNING(cookies);

    if (!menu) {			// shouldn't happen
	abort();
    }

    runtime = calloc(1, sizeof(*runtime));
    runtime->Menu = menu;
    // FIXME: y-offsets

    // send label size request
    if (menu->Label) {
	cookie_label =
	    FontQueryExtentsRequest(&Fonts.Menu, strlen(menu->Label),
	    menu->Label);
    }

    cookies = alloca(menu->ItemCount * sizeof(*cookies));
    //
    //	Add icons to menu and send extents requests
    //
    for (i = 0; i < menu->ItemCount; ++i) {
	MenuItem *item;
	char *name;

	item = menu->ItemTable[i];
#ifdef USE_ICON
	// icon no already loaded
	if (!item->IconLoaded && (name = item->IconName)) {
	    item->Icon = IconLoadNamed(name);
	    if (!item->Icon) {
		Warning("could not load menu icon: \"%s\"\n", name);
	    }
	    item->IconLoaded = 1;
	    free(name);			// icon and name share same slot
	}
	// icon loaded
	if (item->Icon) {
	    if (!menu->UserHeight) {
		if (runtime->ItemHeight < item->Icon->Image->Height) {
		    runtime->ItemHeight = item->Icon->Image->Height;
		}
		if (runtime->TextOffset <
		    item->Icon->Image->Width + LABEL_INNER_SPACE * 2) {
		    runtime->TextOffset =
			item->Icon->Image->Width + LABEL_INNER_SPACE * 2;
		}
	    }
	}
#endif
	// FIXME: or-text
	if (item->Text) {
	    cookies[i] =
		FontQueryExtentsRequest(&Fonts.Menu, strlen(item->Text),
		item->Text);
	}
    }

    //
    //	Item height
    //
    if (menu->UserHeight) {
	// FIXME: above item-height not set if user-height
	if (runtime->ItemHeight) {	// align if any icon
	    runtime->TextOffset = runtime->ItemHeight + LABEL_INNER_SPACE * 2;
	}
	runtime->ItemHeight = menu->UserHeight;
    } else {
	if (Fonts.Menu.Height > runtime->ItemHeight) {
	    runtime->ItemHeight = Fonts.Menu.Height;
	    // FIXME: text offset is wrong, icons are now scaled bigger!
	    if (runtime->TextOffset
		&& runtime->TextOffset <
		runtime->ItemHeight + LABEL_INNER_SPACE * 2) {
		runtime->TextOffset =
		    runtime->ItemHeight + LABEL_INNER_SPACE * 2;
	    }
	}
    }
    runtime->ItemHeight += LABEL_INNER_SPACE * 2 + LABEL_BORDER;

    //
    //	Calculate menu size
    //
    runtime->Height = 1;
    runtime->Width = 5;
    if (menu->Label) {
	unsigned width;

	width = FontTextWidthReply(cookie_label);
	if (width > runtime->Width) {
	    runtime->Width = width;
	}
	runtime->Height += runtime->ItemHeight;
    }
    // nothing else to do, if there is nothing in the menu.
    if (!menu->ItemCount) {
	return runtime;
    }
    //
    //	Prepare y-offsets of menu items and largest text width
    //
    submenu_offset = 0;
    for (i = 0; i < menu->ItemCount; ++i) {
	MenuItem *item;

	item = menu->ItemTable[i];
	item->OffsetY = runtime->Height;
	if (item->Text) {
	    unsigned width;

	    width = FontTextWidthReply(cookies[i]) + LABEL_INNER_SPACE;
	    if (width > runtime->Width) {
		runtime->Width = width;
	    }
	    runtime->Height += runtime->ItemHeight;
#ifdef USE_ICON
	} else if (item->Icon) {
	    runtime->Height += runtime->ItemHeight;
#endif
	} else {			// separator
	    runtime->Height += 5;
	}

	if (item->Command.Type >= MENU_ACTION_SUBMENU) {
	    submenu_offset = SUB_MENU_ARROW_WIDTH + MENU_INNER_SPACE;
	}
    }
    runtime->Width +=
	MENU_INNER_SPACE * 2 + LABEL_INNER_SPACE * 2 + LABEL_BORDER * 2 +
	submenu_offset + runtime->TextOffset;
    runtime->Height += MENU_INNER_SPACE * 2;

    return runtime;
}

/**
**	Cleanup menu runtime after display.
**
**	@param runtime	menu runtime variables
*/
static void MenuCleanupRuntime(Runtime * runtime)
{
    free(runtime);
}

/**
**	Show a submenu.
**
**	@param runtime	menu runtime to draw
**	@param parent	parent of the menu
**	@param x	x-coordinate of menu position
**	@param y	y-coordinate of menu position
**
**	@returns 0 if nothing selected or 1 if something selected.
*/
static int MenuExecuteRuntime(Runtime * runtime, Runtime * parent, int x,
    int y)
{
    int status;

    runtime->Parent = parent;
    MenuCreateWindow(runtime, x, y);
    MenuDraw(runtime);

    ++MenuShown;
    status = MenuLoop(runtime);
    --MenuShown;

    MenuDestroyWindow(runtime);

    return status;
}

/**
**	Show a menu.
**
**	@param runtime	menu runtime to show and handle
**	@param x	menu x-coordinate
**	@param y	menu y-coordinate
**	@param execute	function to excute selected menu item
**	@param opaque	private data for execute
*/
static void MenuShowRuntime(Runtime * runtime, int x, int y,
    void (*execute) (void *, const MenuCommand *), void *opaque)
{
    xcb_grab_pointer_cookie_t pointer_cookie;
    xcb_grab_keyboard_cookie_t keyboard_cookie;
    int mouse_grabbed;
    int keyboard_grabbed;

    pointer_cookie = PointerGrabDefaultRequest(RootWindow);
    keyboard_cookie = KeyboardGrabRequest(RootWindow);

    mouse_grabbed = PointerGrabReply(pointer_cookie);
    keyboard_grabbed = KeyboardGrabReply(keyboard_cookie);

    if (!mouse_grabbed || !keyboard_grabbed) {
	Debug(3, "can't grab mouse or keyboard\n");
	xcb_ungrab_keyboard(Connection, XCB_CURRENT_TIME);
	xcb_ungrab_pointer(Connection, XCB_CURRENT_TIME);
	return;
    }

    MenuExecuteRuntime(runtime, NULL, x, y);

    xcb_ungrab_keyboard(Connection, XCB_CURRENT_TIME);
    xcb_ungrab_pointer(Connection, XCB_CURRENT_TIME);
    ClientRefocus();

    // execute selected command (copied, menu can already be freeed)
    if (MenuCommandSelected.Type) {
	execute(opaque, &MenuCommandSelected);
	MenuCommandDel(&MenuCommandSelected);
	MenuCommandSelected.Type = MENU_ACTION_NONE;
    }
}

// ------------------------------------------------------------------------ //
// Execute

/**
**	Exit callback for the exit dialog.
**
**	@param client	dummy argument for other callbacks.
*/
void ExitHandler(Client __attribute__ ((unused)) * client)
{
    Debug(3, "--- starting exit:\n");
    KeepLooping = 0;
}

/**
**	Restart callback for the restart menu item.
*/
static void DoRestart(void)
{
    Debug(3, "--- starting restart:\n");
    KeepRunning = 1;
    KeepLooping = 0;
}

/**
**	Exit with optional confirmation.
*/
static void DoExit(void)
{
    if (ShowExitConfirmation) {
	DialogShowConfirm(NULL, ExitHandler, "Exit µWM!", "Are you sure?",
	    NULL);
    } else {
	ExitHandler(NULL);
    }
}

/**
**	Prepare command to be executed.
**
**	@param command	menu command to be prepared
*/
static void MenuCommandPrepare(MenuCommand * command)
{
    Menu *submenu;
    int i;

    // handle generated sub-menus
    switch (command->Type) {
	case MENU_ACTION_DESKTOP:
	case MENU_ACTION_SENDTO:
	    if (MenuClient) {
		if (MenuClient->State & WM_STATE_STICKY) {
		    i = -1;
		} else {
		    i = 1 << MenuClient->Desktop;
		}
	    } else {
		i = 1 << DesktopCurrent;
	    }
	    command->Submenu = DesktopCreateMenu(i);
	    command->Submenu->UserHeight = WindowMenuUserHeight;
	    break;
	case MENU_ACTION_LAYER:
	    if (MenuClient) {
		i = MenuClient->OnLayer;
	    } else {
		i = -1;
	    }
	    command->Submenu = WindowMenuCreateLayer(i);
	    break;
	case MENU_ACTION_DIR:
	    if ((submenu = RootMenuFromDirectory(command->String))) {
		command->Submenu = submenu;
		// @warning path is overwritten by sub-menu!
		command->Type = MENU_ACTION_DIR_PREPARED;
	    }
	    break;
	    // FIXME: must generate other menus
    }
}

/**
**	Cleanup command after executed.
**
**	@param command	menu command to be cleaned
*/
static void MenuCommandCleanup(MenuCommand * command)
{
    char *path;

    switch (command->Type) {
	case MENU_ACTION_DESKTOP:
	case MENU_ACTION_WINDOW:
	case MENU_ACTION_SENDTO:
	case MENU_ACTION_LAYER:
	    MenuDel(command->Submenu);
	    command->Submenu = NULL;
	    break;
	case MENU_ACTION_DIR_PREPARED:
	    path = command->Submenu->Label;
	    command->Submenu->Label = NULL;
	    MenuDel(command->Submenu);
	    command->Type = MENU_ACTION_DIR;
	    command->String = path;
	    break;
    }
}

/**
**	Execute a menu command.
**
**	@param command	menu command action and parameter
**	@param x	x-coordinate of hot-spot for visual commands
**	@param y	x-coordinate of hot-spot for visual commands
**
**	@todo opaque argument?
*/
void MenuCommandExecute(const MenuCommand * command, int x, int y)
{
    char *s;
    char *p;

    switch (command->Type) {
	case MENU_ACTION_NONE:
	    break;
	case MENU_ACTION_SET_DESKTOP:
	    DesktopChange(command->Integer);
	    break;
	case MENU_ACTION_NEXT_DESKTOP:
	    DesktopNext();
	    break;
	case MENU_ACTION_PREV_DESKTOP:
	    DesktopPrevious();
	    break;

	case MENU_ACTION_EXECUTE:
	    xcb_flush(Connection);	// required for xprop, ...
	    CommandRun(command->String);
	    break;
	case MENU_ACTION_FILE:
	    xcb_flush(Connection);	// required for xprop, ...
	    p = command->String;
	    s = alloca(strlen(p) + 12);
	    sprintf(s, "uwm-helper %s", p);
	    CommandRun(s);
	    break;
	case MENU_ACTION_RESTART:
	    DoRestart();
	    break;
	case MENU_ACTION_EXIT:
	    free(ExitCommand);
	    // menu are freeed before execution
	    p = command->String;
	    ExitCommand = p ? strdup(p) : NULL;
	    DoExit();
	    break;
	case MENU_ACTION_TOGGLE_SHOW_DESKTOP:
	    DesktopToggleShow();
	    break;
	case MENU_ACTION_TOGGLE_SHADE_DESKTOP:
	    DesktopToggleShade();
	    break;

	case MENU_ACTION_TASK_NEXT_WINDOW:
	    TaskFocusNext();
	    break;
	case MENU_ACTION_TASK_PREV_WINDOW:
	    TaskFocusPrevious();
	    break;
	case MENU_ACTION_HIDE_PANEL:
	    PanelToggle(command->Integer, 0);
	    break;
	case MENU_ACTION_SHOW_PANEL:
	    PanelToggle(command->Integer, 1);
	    break;
	case MENU_ACTION_TOGGLE_PANEL:
	    PanelToggle(command->Integer, -1);
	    break;

	case MENU_ACTION_DIA_SHOW:
	    DiaCreate(command->String);
	    break;
	case MENU_ACTION_PLAY_TD:
#ifdef USE_TD
	    TdCreate(command->String);
#endif
	    break;

	case MENU_ACTION_SENDTO_DESKTOP:
	case MENU_ACTION_SET_LAYER:
	case MENU_ACTION_TOGGLE_MAXIMIZE:
	case MENU_ACTION_MINIMIZE:
	case MENU_ACTION_RESTORE:
	case MENU_ACTION_TOGGLE_SHADE:
	case MENU_ACTION_MOVE:
	case MENU_ACTION_RESIZE:
	case MENU_ACTION_RAISE:
	case MENU_ACTION_LOWER:
	case MENU_ACTION_CLOSE:
	case MENU_ACTION_KILL:
	    WindowMenuChoose(command);
	    break;

	case MENU_ACTION_ROOT_MENU:
	    RootMenuShow(command->Integer, x, y);
	    break;
	case MENU_ACTION_DESKTOP:
	case MENU_ACTION_WINDOW:
	case MENU_ACTION_SENDTO:
	case MENU_ACTION_LAYER:
	case MENU_ACTION_DIR_PREPARED:
	    Debug(3, "action for %d\n", command->Type);
	    if (MenuIsValid(command->Submenu)) {
		Runtime *runtime;

		runtime = MenuPrepareRuntime(command->Submenu);

		if (x < 0 || y < 0) {	// other side hot-spot
		    if (x < 0) {
			x = -x - runtime->Width;
		    }
		    if (y < 0) {
			y = -y - runtime->Height;
		    }
		}

		MenuShowRuntime(runtime, x, y, RootMenuExecute, NULL);

		MenuCleanupRuntime(runtime);
	    }
	    break;

	case MENU_ACTION_DIR:
	default:
	    Debug(2, "invalid menu command: %d\n", command->Type);
	    break;
    }
}

/**
**	Copy menu command.
**
**	@param dst	menu command destination
**	@param src	menu command source
*/
static void MenuCommandCopy(MenuCommand * dst, const MenuCommand * src)
{
    *dst = *src;
    switch (dst->Type) {
	case MENU_ACTION_EXECUTE:
	case MENU_ACTION_FILE:
	case MENU_ACTION_EXIT:
	case MENU_ACTION_DIR:
	case MENU_ACTION_DIA_SHOW:
	case MENU_ACTION_PLAY_TD:
	    if (src->String) {
		dst->String = strdup(src->String);
	    }
	default:
	    break;
    }
}

/**
**	Free memory of menu command.
**
**	@param command	menu command action and parameter
*/
void MenuCommandDel(MenuCommand * command)
{
    switch (command->Type) {
	case MENU_ACTION_EXECUTE:
	case MENU_ACTION_FILE:
	case MENU_ACTION_EXIT:
	case MENU_ACTION_DIR:
	case MENU_ACTION_DIA_SHOW:
	case MENU_ACTION_PLAY_TD:
	    free(command->String);
	    break;
	case MENU_ACTION_SUBMENU:
	case MENU_ACTION_DESKTOP:
	case MENU_ACTION_WINDOW:
	case MENU_ACTION_SENDTO:
	case MENU_ACTION_LAYER:
	case MENU_ACTION_DIR_PREPARED:
	    if (command->Submenu) {
		MenuDel(command->Submenu);
	    }
	default:
	    break;
    }
}

/**
**	Execute commands of menu button.
**
**	@param button	pointer button commands
**	@param mask	pointer button number
**	@param x	x-coordinate
**	@param y	y-coordinate
**	@param opaque	user data pointer
*/
void MenuButtonExecute(MenuButton * button, int mask, int x, int y, void
    __attribute__ ((unused)) * opaque)
{
    int b;

    Debug(3, "execute %d\n", mask);

    b = mask;
    if (b > 16) {
	Warning("only 16 buttons supported\n");
	return;
    }
    // FIXME: double click
#if 0
    b = event->detail;
    if (event->state & ((1 << 7) << b)) {
	Debug(3, " double click %d\n", b);
	b += 16;
	// FIXME: only if double click is used, otherwise click/click
    }
#endif

    b--;
    if (button && button->Buttons & (1 << b)) {
	MenuCommand *command;

	Debug(3, " have command\n");
	command =
	    button->Commands + xcb_popcount(button->Buttons & xcb_mask(b));
	MenuCommandPrepare(command);
	MenuCommandExecute(command, x, y);
	MenuCommandCleanup(command);
    } else {
	Debug(3, " button %d undefined\n", b);
    }
}

/**
**	Free memory of menu button.
**
**	@param button	pointer button commands
*/
void MenuButtonDel(MenuButton * button)
{
    int i;
    int n;

    for (n = i = 0; i < 32; ++i) {
	if (button->Buttons & (1 << i)) {
	    MenuCommandDel(button->Commands + n);
	    ++n;
	}
    }
    free(button);
}

/**
**	Create a new menu item.
**
**	@param name	name of icon displayed for this menu item (can be NULL)
**	@param text	text displayed for this menu item (can be NULL)
**
**	@returns malloced menu item structure.
*/
MenuItem *MenuNewItem(const char *name, const char *text)
{
    MenuItem *item;

    item = calloc(1, sizeof(*item));
#ifdef USE_ICON
    if (name) {
	item->IconName = strdup(name);
    }
#endif
    if (text) {
	item->Text = strdup(text);
    }

    return item;
}

/**
**	Delete menu item.
**
**	@param item	menu item to destroy
*/
void MenuDelItem(MenuItem * item)
{
    // text and string points to same text, don't free twice
    if (item->Text != item->Command.String) {
	MenuCommandDel(&item->Command);
    }
    free(item->Text);
#ifdef USE_ICON
    if (!item->IconLoaded) {		// icon + name share the same slot
	free(item->IconName);
    } else if (item->Icon) {
	IconDel(item->Icon);
    }
#endif
    free(item);
}

/**
**	Append a menu item to menu.
**
**	@param menu	menu
**	@param item	item appended to menu
*/
void MenuAppendItem(Menu * menu, MenuItem * item)
{
    int n;

    n = menu->ItemCount++;
    menu->ItemTable =
	realloc(menu->ItemTable, menu->ItemCount * sizeof(*menu->ItemTable));
    menu->ItemTable[n] = item;
}

/**
**	Create a new empty menu.
**
**	@returns malloced menu structure.
*/
Menu *MenuNew(void)
{
    Menu *menu;

    menu = calloc(1, sizeof(*menu));

    return menu;
}

/**
**	Delete menu.
**
**	@param menu	menu to destroy
*/
void MenuDel(Menu * menu)
{
    if (menu) {
	int i;

	// free all items
	for (i = 0; i < menu->ItemCount; ++i) {
	    MenuDelItem(menu->ItemTable[i]);
	}
	free(menu->ItemTable);
	free(menu->Label);
	free(menu);
    }
}

// ------------------------------------------------------------------------ //
// Config

#ifdef USE_LUA

/**
**	Set label of menu.
**
**	@param menu	menu to change label
**	@param label	label of menu
*/
void MenuSetLabel(Menu * menu, const char *label)
{
    free(menu->Label);
    menu->Label = label ? strdup(label) : NULL;
}

/**
**	Set user height of menu items.
**
**	@param menu	menu to change label
**	@param height	user height of menu items
*/
void MenuSetUserHeight(Menu * menu, int height)
{
    menu->UserHeight = height;
}

/**
**	Set sub-menu of menu item.
**
**	@param item	change type of this menu item to sub-menu
**	@param menu	make this menu the new sub-menu
*/
void MenuSetSubmenu(MenuItem * item, Menu * menu)
{
    if (!menu) {
	Warning("null sub menu added\n");
    }
    if (item->Type) {
	Warning("menu has already action defined\n");
    }
    if (item->Command.Submenu) {
	Warning("can loose memory, item has already a sub menu\n");
	// FIXME: MenuDel(Submenu); if it was menu
    }
    item->Type = MENU_ACTION_SUBMENU;
    item->Command.Submenu = menu;
}

/**
**	Set action of the menu.
*/
void MenuSetAction(MenuItem * item, const char *action, const char *value)
{
    if (!strcasecmp("none", action)) {
	item->Type = MENU_ACTION_NONE;
    } else if (!strcasecmp("desktop", action)) {
	item->Type = MENU_ACTION_DESKTOP;
    } else if (!strcasecmp("window", action)) {
	item->Type = MENU_ACTION_WINDOW;
    } else if (!strcasecmp("set-desktop", action)) {
	item->Type = MENU_ACTION_SET_DESKTOP;
	item->Command.Integer = value ? atoi(value) : 0;
    } else if (!strcasecmp("next-desktop", action)) {
	item->Type = MENU_ACTION_NEXT_DESKTOP;
    } else if (!strcasecmp("prev-desktop", action)) {
	item->Type = MENU_ACTION_PREV_DESKTOP;
    } else if (!strcasecmp("sendto", action)) {
	item->Type = MENU_ACTION_SENDTO;
    } else if (!strcasecmp("sendto-desktop", action)) {
	item->Type = MENU_ACTION_SENDTO_DESKTOP;
	item->Command.Integer = value ? atoi(value) : 0;
    } else if (!strcasecmp("layer", action)) {
	item->Type = MENU_ACTION_LAYER;
    } else if (!strcasecmp("set-layer", action)) {
	item->Type = MENU_ACTION_SET_LAYER;
	item->Command.Integer = value ? atoi(value) : 0;
    } else if (!strcasecmp("stick", action)) {
	item->Type = MENU_ACTION_TOGGLE_STICKY;
    } else if (!strcasecmp("toggle-maximize", action)) {
	item->Type = MENU_ACTION_TOGGLE_MAXIMIZE;
    } else if (!strcasecmp("maximize-horizontal", action)) {
	item->Type = MENU_ACTION_MAXIMIZE_HORZ;
    } else if (!strcasecmp("maximize-vertical", action)) {
	item->Type = MENU_ACTION_MAXIMIZE_VERT;
    } else if (!strcasecmp("minimize", action)) {
	item->Type = MENU_ACTION_MINIMIZE;
    } else if (!strcasecmp("restore", action)) {
	item->Type = MENU_ACTION_RESTORE;
    } else if (!strcasecmp("shade", action)) {
	item->Type = MENU_ACTION_TOGGLE_SHADE;
    } else if (!strcasecmp("move", action)) {
	item->Type = MENU_ACTION_MOVE;
    } else if (!strcasecmp("resize", action)) {
	item->Type = MENU_ACTION_RESIZE;
    } else if (!strcasecmp("raise", action)) {
	item->Type = MENU_ACTION_RAISE;
    } else if (!strcasecmp("lower", action)) {
	item->Type = MENU_ACTION_LOWER;
    } else if (!strcasecmp("close", action)) {
	item->Type = MENU_ACTION_CLOSE;
    } else if (!strcasecmp("kill", action)) {
	item->Type = MENU_ACTION_KILL;
    } else if (!strcasecmp("execute", action)) {
	item->Type = MENU_ACTION_EXECUTE;
	item->Command.String = value ? strdup(value) : NULL;
    } else if (!strcasecmp("exit", action)) {
	item->Type = MENU_ACTION_EXIT;
	item->Command.String = value ? strdup(value) : NULL;
    } else if (!strcasecmp("restart", action)) {
	item->Type = MENU_ACTION_RESTART;
    } else if (!strcasecmp("root-menu", action)) {
	item->Type = MENU_ACTION_ROOT_MENU;
	item->Command.Integer = value ? atoi(value) : 0;
    } else if (!strcasecmp("toggle-show-desktop", action)) {
	item->Type = MENU_ACTION_TOGGLE_SHOW_DESKTOP;
    } else if (!strcasecmp("dir", action)) {
	item->Type = MENU_ACTION_DIR;
	item->Command.String = value ? strdup(value) : NULL;
    } else {
	Warning("unsupported action '%s'(%s)\n", action, value);
    }
}

#else

/**
**	Parse menu command configuration.
**
**	@param array		configuration array for menu action
**	@param[out] command	command action parsed
*/
void MenuCommandConfig(const ConfigObject * array, MenuCommand * command)
{
    const char *sval;
    ssize_t ival;
    const ConfigObject *oval;

    //
    //	actions:
    //
    if (ConfigGetObject(array, &oval, "none", NULL)) {
	command->Type = MENU_ACTION_NONE;

	//
	//  window client commands
	//
    } else if (ConfigGetObject(array, &oval, "toggle-sticky", NULL)) {
	command->Type = MENU_ACTION_TOGGLE_STICKY;
    } else if (ConfigGetObject(array, &oval, "toggle-maximize", NULL)) {
	command->Type = MENU_ACTION_TOGGLE_MAXIMIZE;
    } else if (ConfigGetObject(array, &oval, "maximize-horizontal", NULL)) {
	command->Type = MENU_ACTION_MAXIMIZE_HORZ;
    } else if (ConfigGetObject(array, &oval, "maximize-vertical", NULL)) {
	command->Type = MENU_ACTION_MAXIMIZE_VERT;
    } else if (ConfigGetObject(array, &oval, "minimize", NULL)) {
	command->Type = MENU_ACTION_MINIMIZE;
    } else if (ConfigGetObject(array, &oval, "restore", NULL)) {
	command->Type = MENU_ACTION_RESTORE;
    } else if (ConfigGetObject(array, &oval, "toggle-shade", NULL)) {
	command->Type = MENU_ACTION_TOGGLE_SHADE;
    } else if (ConfigGetObject(array, &oval, "move", NULL)) {
	command->Type = MENU_ACTION_MOVE;
    } else if (ConfigGetObject(array, &oval, "resize", NULL)) {
	command->Type = MENU_ACTION_RESIZE;
    } else if (ConfigGetObject(array, &oval, "raise", NULL)) {
	command->Type = MENU_ACTION_RAISE;
    } else if (ConfigGetObject(array, &oval, "lower", NULL)) {
	command->Type = MENU_ACTION_LOWER;
    } else if (ConfigGetObject(array, &oval, "close", NULL)) {
	command->Type = MENU_ACTION_CLOSE;
    } else if (ConfigGetObject(array, &oval, "kill", NULL)) {
	command->Type = MENU_ACTION_KILL;
	//
	//  global commands
	//
    } else if (ConfigGetObject(array, &oval, "restart", NULL)) {
	command->Type = MENU_ACTION_RESTART;
    } else if (ConfigGetObject(array, &oval, "exit", NULL)) {
	command->Type = MENU_ACTION_EXIT;
	command->String = NULL;		// exit string is optional
	if (ConfigCheckString(oval, &sval)) {
	    command->String = strdup(sval);
	}
    } else if (ConfigGetString(array, &sval, "execute", NULL)) {
	command->Type = MENU_ACTION_EXECUTE;
	command->String = strdup(sval);
    } else if (ConfigGetString(array, &sval, "file", NULL)) {
	command->Type = MENU_ACTION_FILE;
	command->String = strdup(sval);

    } else if (ConfigGetInteger(array, &ival, "set-layer", NULL)) {
	command->Type = MENU_ACTION_SET_LAYER;
	command->Integer = ival;
    } else if (ConfigGetInteger(array, &ival, "set-desktop", NULL)) {
	command->Type = MENU_ACTION_SET_DESKTOP;
	command->Integer = ival;
    } else if (ConfigGetObject(array, &oval, "next-desktop", NULL)) {
	command->Type = MENU_ACTION_NEXT_DESKTOP;
    } else if (ConfigGetObject(array, &oval, "prev-desktop", NULL)) {
	command->Type = MENU_ACTION_PREV_DESKTOP;
    } else if (ConfigGetInteger(array, &ival, "sendto-desktop", NULL)) {
	command->Type = MENU_ACTION_SENDTO_DESKTOP;
	command->Integer = ival;
    } else if (ConfigGetInteger(array, &ival, "root-menu", NULL)) {
	command->Type = MENU_ACTION_ROOT_MENU;
	command->Integer = ival;

    } else if (ConfigGetObject(array, &oval, "toggle-show-desktop", NULL)) {
	command->Type = MENU_ACTION_TOGGLE_SHOW_DESKTOP;
    } else if (ConfigGetObject(array, &oval, "toggle-shade-desktop", NULL)) {
	command->Type = MENU_ACTION_TOGGLE_SHADE_DESKTOP;

    } else if (ConfigGetObject(array, &oval, "task-next-window", NULL)) {
	command->Type = MENU_ACTION_TASK_NEXT_WINDOW;
    } else if (ConfigGetObject(array, &oval, "task-prev-window", NULL)) {
	command->Type = MENU_ACTION_TASK_PREV_WINDOW;
    } else if (ConfigGetInteger(array, &ival, "hide-panel", NULL)) {
	command->Type = MENU_ACTION_HIDE_PANEL;
	command->Integer = ival;
    } else if (ConfigGetInteger(array, &ival, "show-panel", NULL)) {
	command->Type = MENU_ACTION_SHOW_PANEL;
	command->Integer = ival;
    } else if (ConfigGetInteger(array, &ival, "toggle-panel", NULL)) {
	command->Type = MENU_ACTION_TOGGLE_PANEL;
	command->Integer = ival;

    } else if (ConfigGetObject(array, &oval, "dia-show", NULL)) {
	command->Type = MENU_ACTION_DIA_SHOW;
	// string is optional
	command->String = NULL;
	if (ConfigCheckString(oval, &sval)) {
	    command->String = strdup(sval);
	}
    } else if (ConfigGetObject(array, &oval, "play-td", NULL)) {
	command->Type = MENU_ACTION_PLAY_TD;
	// string is optional
	command->String = NULL;
	if (ConfigCheckString(oval, &sval)) {
	    command->String = strdup(sval);
	}

    } else if (ConfigGetArray(array, &oval, "menu", NULL)) {
	command->Type = MENU_ACTION_SUBMENU;
	command->Submenu = MenuConfigMenu(oval);
    } else if (ConfigGetObject(array, &oval, "desktop", NULL)) {
	command->Type = MENU_ACTION_DESKTOP;
	command->Submenu = NULL;
    } else if (ConfigGetObject(array, &oval, "window", NULL)) {
	command->Type = MENU_ACTION_WINDOW;
	command->Submenu = NULL;
    } else if (ConfigGetObject(array, &oval, "sendto", NULL)) {
	command->Type = MENU_ACTION_SENDTO;
	command->Submenu = NULL;
    } else if (ConfigGetObject(array, &oval, "layer", NULL)) {
	command->Type = MENU_ACTION_LAYER;
	command->Submenu = NULL;
    } else if (ConfigGetString(array, &sval, "dir", NULL)) {
	// FIXME: path is optional
	command->Type = MENU_ACTION_DIR;
	command->String = strdup(sval);

    } else {
	Warning("unsupported or missing action for menu-command\n");
	command->Type = MENU_ACTION_NONE;
    }
}

/**
**	Parse single menu pointer button configuration.
**
**	@param array		array of config values for pointer button
**	@param[in,out] button	button config result
*/
static void MenuButtonConfig(const ConfigObject * array, MenuButton ** button)
{
    MenuCommand command;
    ssize_t ival;
    int b;
    int s;
    MenuButton *menu_button;

    //
    //	Config of pointer button
    //
    b = 1;
    if (ConfigGetInteger(array, &ival, "button", NULL)) {
	if (1 > ival || ival > 16) {
	    Warning("only button 1-16 supported\n");
	    ival = 1;
	}
	b = ival;
    } else if (ConfigGetInteger(array, &ival, "double-click", NULL)) {
	if (1 > ival || ival > 16) {
	    Warning("only button 1-16 supported\n");
	    ival = 1;
	}
	b = ival + 16;
    }
    // parse general command config
    MenuCommandConfig(array, &command);
    if (command.Type == MENU_ACTION_NONE) {
	Warning("\tfor button %d\n", b);
    }
    // append button to buttons
    if ((menu_button = *button)) {
	uint32_t mask;
	int n;
	int i;

	// FIXME: overwrite buttons is not supported.
	if (menu_button->Buttons & 1 << (b - 1)) {
	    Warning("button %d already defined\n", b);
	    return;
	}
	n = xcb_popcount(menu_button->Buttons);
	menu_button =
	    realloc(menu_button,
	    sizeof(*menu_button) + n * sizeof(MenuCommand));
	// copy values
	mask = xcb_mask(b);
	s = xcb_popcount(menu_button->Buttons & mask);
	for (i = s; i < n; ++i) {
	    menu_button->Commands[i + 1] = menu_button->Commands[i];
	}
	menu_button->Buttons |= 1 << (b - 1);
    } else {
	menu_button = malloc(sizeof(*menu_button));
	menu_button->Buttons = 1 << (b - 1);
	s = 0;
    }
    menu_button->Commands[s] = command;
    *button = menu_button;
}

/**
**	Parse menu pointer buttons configuration.
**
**	@param array		array of config values for buttons
**	@param[in,out] button	button config result
*/
void MenuButtonsConfig(const ConfigObject * array, MenuButton ** button)
{
    const ConfigObject *aval;
    const ConfigObject *index;
    const ConfigObject *value;

    //
    //	array of buttons
    //
    index = NULL;
    value = ConfigArrayFirstFixedKey(array, &index);
    while (value) {
	if (ConfigCheckArray(value, &aval)) {
	    MenuButtonConfig(aval, button);
	} else {
	    Warning("value in buttons ignored\n");
	}
	value = ConfigArrayNextFixedKey(array, &index);
    }
}

/**
**	Parse menu-item configuration.
**
**	@param array	configuration array for menu item
**
**	@returns menu item created for given configuration.
*/
MenuItem *MenuItemConfig(const ConfigObject * array)
{
    MenuItem *item;
    const char *sval;
    ssize_t ival;

    item = MenuNewItem(NULL, NULL);
    // separator ignore all other parameters
    if (ConfigGetInteger(array, &ival, "separator", NULL) && ival) {
	// item->Text = NULL && item->IconName flags separator
	return item;
    }
#ifdef USE_ICON
    if (ConfigGetString(array, &sval, "icon", NULL)) {
	item->IconName = strdup(sval);
    }
    if (ConfigGetInteger(array, &ival, "icon-or-text", NULL)) {
	item->IconOrText = ival != 0;
    }
#endif
    if (ConfigGetString(array, &sval, "text", NULL)) {
	item->Text = strdup(sval);
    }
    //
    //	actions:
    //
    MenuCommandConfig(array, &item->Command);
    if (item->Command.Type == MENU_ACTION_NONE) {
	Warning("\tfor command '%s'-'%s'\n", item->IconName, item->Text);
    }

    return item;
}

/// prepare new string handling in menus
#define MenuStrdup(menu, str)	strdup(str)

/**
**	Parse the menu config of a single menu or sub menu.
**
**	@param array	config table of a single menu
*/
static Menu *MenuConfigMenu(const ConfigObject * array)
{
    Menu *menu;
    const ConfigObject *aval;
    const char *sval;
    ssize_t ival;
    const ConfigObject *index;
    const ConfigObject *value;

    menu = MenuNew();

    if (ConfigGetString(array, &sval, "label", NULL)) {
	menu->Label = MenuStrdup(menu, sval);
    }
    if (ConfigGetInteger(array, &ival, "height", NULL)) {
	menu->UserHeight = ival;
    }
    //
    //	array of menu items
    //
    index = NULL;
    value = ConfigArrayFirstFixedKey(array, &index);
    while (value) {
	if (ConfigCheckArray(value, &aval)) {
	    MenuAppendItem(menu, MenuItemConfig(aval));
	} else {
	    Warning("value in menu item ignored\n");
	}
	value = ConfigArrayNextFixedKey(array, &index);
    }

    return menu;
}

/**
**	Parse menu configuration.
**
**	@param config	global config dictionary
*/
void MenuConfig(const Config * config)
{
    const ConfigObject *array;
    ssize_t ival;

    // FIXME: use GetBoolean
    if (ConfigGetInteger(ConfigDict(config), &ival, "show-exit-confirmation",
	    NULL)) {
	ShowExitConfirmation = ival != 0;
    }
    if (ConfigGetInteger(ConfigDict(config), &ival, "show-kill-confirmation",
	    NULL)) {
	ShowKillConfirmation = ival != 0;
    }
    if (ConfigGetInteger(ConfigDict(config), &ival, "window-menu-user-height",
	    NULL)) {
	WindowMenuUserHeight = ival;
    }
    //
    //	array of menus
    //
    if (ConfigGetArray(ConfigDict(config), &array, "root-menu", NULL)) {
	const ConfigObject *index;
	const ConfigObject *value;
	double opacity;

	//
	//	opacity
	//
	if (ConfigGetDouble(array, &opacity, "opacity", NULL)) {
	    if (opacity <= 0.0 || opacity > 1.0) {
		Warning("invalid menu opacity: %g\n", opacity);
		opacity = 1.0;
	    }
	    MenuOpacity = UINT32_MAX * opacity;
	    Debug(3, "opacity %x\n", MenuOpacity);
	}
	//
	//	menus
	//
	index = NULL;
	value = ConfigArrayFirstFixedKey(array, &index);
	while (value) {
	    const ConfigObject *aval;

	    if (ConfigCheckArray(value, &aval)) {
		Menus = realloc(Menus, (MenuN + 1) * sizeof(*Menus));
		Menus[MenuN++] = MenuConfigMenu(aval);
	    } else {
		Warning("value in menu config ignored\n");
	    }
	    value = ConfigArrayNextFixedKey(array, &index);
	}
    }
}

#endif

// ------------------------------------------------------------------------ //
// Root menu
// ------------------------------------------------------------------------ //

///
///	@ingroup menu
///	@defgroup rootmenu The root menu module.
///
///	This module handles root (window) menus and commands.
///
/// @{

static MenuButton *RootButtons;		///< root window buttons

/**
**	Root menu callback.  Execute selected menu item command.
**
**	@param opaque	private data
**	@param command	menu command to execute
*/
static void RootMenuExecute(void
    __attribute__ ((unused)) * opaque, const MenuCommand * command)
{
    int x;
    int y;

    PointerGetPosition(&x, &y);
    MenuCommandExecute(command, x, y);
}

/**
**	Show/handle a root menu.
**
**	@param index	menu number
**	@param x	X position = click x
**	@param y	Y position = click y
*/
static void RootMenuShow(int index, int x, int y)
{
    Runtime *runtime;

    // check, if we have enough menus.
    if (0 > index || index >= MenuN) {
	Debug(3, "index out of range %d\n", index);
	return;
    }

    MenuClient = NULL;
    runtime = MenuPrepareRuntime(Menus[index]);

    if (x < 0 || y < 0) {		// other side hot-spot
	if (x < 0) {
	    x = -x - runtime->Width;
	}
	if (y < 0) {
	    y = -y - runtime->Height;
	}
    }

    MenuShowRuntime(runtime, x, y, RootMenuExecute, NULL);

    MenuCleanupRuntime(runtime);

    return;
}

/**
**	Handle root menu button press.
**
**	@param event	X11 button press event
**
**	@todo double-click isn't working perfect get click, double-click
*/
void RootMenuHandleButtonPress(const xcb_button_press_event_t * event)
{
    MenuButtonExecute(RootButtons, event->detail, event->root_x, event->root_y,
	NULL);
}

/**
**	Build menu from directory.
**
**	@param path	directory path
**
**	@returns submenu for directory, NULL if failure.
**
**	@todo FIXME: disable directoy support / write documentation
*/
Menu *RootMenuFromDirectory(char *path)
{
    struct dirent **namelist;
    int n;
    Menu *submenu;
    char *name;

    submenu = MenuNew();
    submenu->Label = path;		// ugly hack save path in label
    //submenu->UserHeight = WindowMenuUserHeight;
    name = ExpandPath(path);
    n = scandir(name, &namelist, NULL, alphasort);
    free(name);
    if (n > 0) {
	int i;
	int path_len;

	path_len = strlen(path);
	if (path[path_len - 1] == '/') {	// remove trailing slash
	    --path_len;
	}

	for (i = 0; i < n; ++i) {
	    MenuItem *item;

	    if (strcmp(namelist[i]->d_name, ".")
		&& strcmp(namelist[i]->d_name, "..")) {
		char *s;

		item = MenuNewItem(NULL, namelist[i]->d_name);
		if (namelist[i]->d_type == DT_REG) {	// regular file
		    item->Command.Type = MENU_ACTION_FILE;
		} else if (namelist[i]->d_type == DT_DIR) {	// directory
		    item->Command.Type = MENU_ACTION_DIR;
		}
		if (item->Command.Type != MENU_ACTION_NONE) {
		    s = malloc(path_len + strlen(item->Text) + 2);
		    item->Command.String = s;
		    memcpy(s, path, path_len);
		    s[path_len] = '/';
		    strcpy(s + path_len + 1, item->Text);
		}
		MenuAppendItem(submenu, item);
	    }
	    free(namelist[i]);
	}
	free(namelist);
    } else {
	MenuAppendItem(submenu, MenuNewItem(NULL, "empty or can't read"));
	Warning("Can't scan dir '%s'\n", path);
    }
    return submenu;
}

// ------------------------------------------------------------------------ //

/**
**	Initialize the root menu module.
*/
void RootMenuInit(void)
{
    MenuArrowPixmap =
	xcb_create_pixmap_from_bitmap_data(Connection, RootWindow,
	(uint8_t *) MenuSubmenuArrowBitmap, SUB_MENU_ARROW_WIDTH,
	SUB_MENU_ARROW_HEIGHT, 1, 1, 0, NULL);
}

/**
**	Cleanup the root menu module.
*/
void RootMenuExit(void)
{
    int i;

    for (i = 0; i < MenuN; ++i) {
	MenuDel(Menus[i]);
    }
    free(Menus);
    Menus = NULL;
    MenuN = 0;
    MenuButtonDel(RootButtons);
    RootButtons = NULL;
    xcb_free_pixmap(Connection, MenuArrowPixmap);
    MenuArrowPixmap = XCB_NONE;
}

// ------------------------------------------------------------------------ //
// Config

#ifdef USE_LUA

/**
**	Set a root menu.
**
**	@param index	change the root menu at index
**	@param menu	new menu value for root menu with number index
*/
void RootSetMenu(int index, Menu * menu)
{
    if (index < 0 || (unsigned)index > sizeof(RootMenu) / sizeof(*RootMenu)) {
	Warning("invalid root menu %d\n", index);
	return;
    }
    // b
    // check if overwriting existing value
    if (RootMenu[index] && RootMenu[index] != menu) {
	unsigned u;

	Debug(2, "FIXME: free overwritten root menu\n");

	// check if overwriting this value will cause an orphan
	for (u = 0; u < sizeof(RootMenu) / sizeof(*RootMenu); u++) {
	    if ((unsigned)index != u && RootMenu[index] == RootMenu[u]) {
		break;
	    }
	}

	if (u == sizeof(RootMenu) / sizeof(*RootMenu)) {
	    // we have an orphan, delete it
	    MenuDel(RootMenu[index]);
	}

    }
    RootMenu[index] = menu;
}

#else

/**
**	Parse root menu/command configuration.
**
**	@param config	global config dictionary
*/
void RootMenuConfig(const Config * config)
{
    const ConfigObject *array;

    if (ConfigGetArray(ConfigDict(config), &array, "root", NULL)) {
	MenuButtonsConfig(array, &RootButtons);
    }
}

#endif

/// @}

// ------------------------------------------------------------------------ //
// Window menu
// ------------------------------------------------------------------------ //

///
///	@ingroup menu
///	@defgroup windowmenu The window menu module.
///
///	This module handles the menu of a client window.
///
///	@todo	support of user configurable menu
///	@todo	icons, perhaps this can be combined with above.
///
/// @{

static int WindowMenuUserHeight;	///< user height of menus

/**
**	Append an item to the current window menu.
**
**	@param menu	menu to which the item is appended at the end
**	@param icon	icon to show
**	@param text	text to show (icon+text NULL is separator)
**	@param type	type/action of menu item
**	@param data	data of the menu action
*/
static void WindowMenuAppend(Menu * menu, const char *icon, const char *text,
    MenuAction type, int data)
{
    MenuItem *item;

    item = MenuNewItem(icon, text);
    item->Command.Type = type;
    item->Command.Integer = data;

    MenuAppendItem(menu, item);
}

/**
**	Append a sub-menu to the current window menu.
**
**	@param menu	menu to which the item is appended at the end
**	@param icon	icon to show
**	@param text	text to show (icon+text NULL is separator)
**	@param type	type/action of menu item
**	@param data	data of the menu action
*/
static void WindowMenuAppendMenu(Menu * menu, const char *icon,
    const char *text, MenuAction type, Menu * data)
{
    MenuItem *item;

    item = MenuNewItem(icon, text);
    item->Command.Type = type;
    item->Command.Submenu = data;

    MenuAppendItem(menu, item);
}

/**
**	Create a window layer submenu.
**
**	@param on_layer	client current layer
**
**	@returns for client created layer menu.
*/
static Menu *WindowMenuCreateLayer(int on_layer)
{
    Menu *submenu;
    char buf[5];
    int layer;
    const char *str;

    submenu = MenuNew();
    submenu->UserHeight = WindowMenuUserHeight;

    str = on_layer == LAYER_BOTTOM ? "[Bottom]" : "Bottom";
    WindowMenuAppend(submenu, NULL, str, MENU_ACTION_SET_LAYER, LAYER_BOTTOM);

    buf[4] = 0;
    for (layer = LAYER_BOTTOM + 1; layer < LAYER_TOP; layer++) {
	switch (layer) {
	    case LAYER_BELOW:
		str = on_layer == layer ? "[Below]" : "Below";
		break;
	    case LAYER_NORMAL:
		str = on_layer == layer ? "[Normal]" : "Normal";
		break;
	    case LAYER_ABOVE:
		str = on_layer == layer ? "[Above]" : "Above";
		break;
	    default:
		if (on_layer == layer) {
		    buf[0] = '[';
		    buf[3] = ']';
		} else {
		    buf[0] = ' ';
		    buf[3] = ' ';
		}
		if (layer < 10) {
		    buf[1] = ' ';
		} else {
		    buf[1] = (layer / 10) + '0';
		}
		buf[2] = (layer % 10) + '0';
		str = buf;
		break;
	}
	WindowMenuAppend(submenu, NULL, str, MENU_ACTION_SET_LAYER, layer);
    }

    str = on_layer == LAYER_TOP ? "[Top]" : "Top";
    WindowMenuAppend(submenu, NULL, str, MENU_ACTION_SET_LAYER, LAYER_TOP);

    return submenu;
}

/**
**	Create a new window menu.
**
**	@param client	menu for this client
**
**	@todo add icons to menu
**	@todo use configuration for window menu
*/
static Menu *WindowMenuCreate(const Client * client)
{
    Menu *menu;

    menu = MenuNew();
    menu->UserHeight = WindowMenuUserHeight;

    if ((client->Border & BORDER_MAXIMIZE)
	&& (client->State & WM_STATE_MAPPED)) {

	if (!(client->State & (WM_STATE_MAXIMIZED_HORZ |
		    WM_STATE_MAXIMIZED_VERT))) {
	    WindowMenuAppend(menu, NULL, "Maximize-y",
		MENU_ACTION_MAXIMIZE_VERT, 0);
	}

	if (!(client->State & (WM_STATE_MAXIMIZED_HORZ |
		    WM_STATE_MAXIMIZED_VERT))) {
	    WindowMenuAppend(menu, NULL, "Maximize-x",
		MENU_ACTION_MAXIMIZE_HORZ, 0);
	}

	if ((client->State & (WM_STATE_MAXIMIZED_HORZ |
		    WM_STATE_MAXIMIZED_VERT))) {
	    WindowMenuAppend(menu, NULL, "Unmaximize",
		MENU_ACTION_TOGGLE_MAXIMIZE, 0);
	} else {
	    WindowMenuAppend(menu, NULL, "Maximize",
		MENU_ACTION_TOGGLE_MAXIMIZE, 0);
	}
    }

    if (client->Border & BORDER_MINIMIZE) {
	if (client->State & WM_STATE_MINIMIZED) {
	    WindowMenuAppend(menu, NULL, "Restore", MENU_ACTION_RESTORE, 0);
	} else {
	    if (client->State & WM_STATE_SHADED) {
		WindowMenuAppend(menu, NULL, "Unshade",
		    MENU_ACTION_TOGGLE_SHADE, 0);
	    } else {
		WindowMenuAppend(menu, NULL, "Shade", MENU_ACTION_TOGGLE_SHADE,
		    0);
	    }
	    WindowMenuAppend(menu, NULL, "Minimize", MENU_ACTION_MINIMIZE, 0);
	}
    }
    // dialog isn't allowed to be moved arround
    if (!(client->State & WM_STATE_WMDIALOG)) {
	if (!(client->State & WM_STATE_STICKY)) {
	    WindowMenuAppend(menu, NULL, "Send to", MENU_ACTION_DESKTOP, 0);
	}

	if (client->State & WM_STATE_STICKY) {
	    WindowMenuAppend(menu, NULL, "Unstick", MENU_ACTION_TOGGLE_STICKY,
		0);
	} else {
	    WindowMenuAppend(menu, NULL, "Stick", MENU_ACTION_TOGGLE_STICKY,
		0);
	}

	WindowMenuAppendMenu(menu, NULL, "Layer", MENU_ACTION_LAYER, 0);
    }

    if (client->State & (WM_STATE_MAPPED | WM_STATE_SHADED)) {
	if ((client->Border & BORDER_RAISE)
	    && TAILQ_FIRST(&ClientLayers[client->OnLayer]) != client) {
	    WindowMenuAppend(menu, NULL, "Raise", MENU_ACTION_RAISE, 0);
	}
	if ((client->Border & BORDER_LOWER)
	    && TAILQ_LAST(&ClientLayers[client->OnLayer],
		_client_layer_) != client) {
	    WindowMenuAppend(menu, NULL, "Lower", MENU_ACTION_LOWER, 0);
	}
	if (client->Border & BORDER_MOVE) {
	    WindowMenuAppend(menu, NULL, "Move", MENU_ACTION_MOVE, 0);
	}
	if (client->Border & BORDER_RESIZE) {
	    WindowMenuAppend(menu, NULL, "Resize", MENU_ACTION_RESIZE, 0);
	}
    }
    // dialog isn't allowed to be closed/killed
    if (!(client->State & WM_STATE_WMDIALOG)) {
	WindowMenuAppend(menu, NULL, NULL, MENU_ACTION_NONE, 0);
	WindowMenuAppend(menu, NULL, "Close", MENU_ACTION_CLOSE, 0);
	WindowMenuAppend(menu, NULL, "Kill", MENU_ACTION_KILL, 0);
    }

    return menu;
}

/**
**	Window menu action callback.
**
**	@param client	private data
**	@param command	menu command to execute
*/
static void WindowMenuExecute(void *client, const MenuCommand * command)
{
    switch (command->Type) {
	case MENU_ACTION_TOGGLE_STICKY:
	    if (((Client *) client)->State & WM_STATE_STICKY) {
		ClientSetSticky(client, 0);
	    } else {
		ClientSetSticky(client, 1);
	    }
	    break;
	case MENU_ACTION_TOGGLE_MAXIMIZE:
	    ClientMaximize(client, 1, 1);
	    break;
	case MENU_ACTION_MAXIMIZE_HORZ:
	    ClientMaximize(client, 1, 0);
	    break;
	case MENU_ACTION_MAXIMIZE_VERT:
	    ClientMaximize(client, 0, 1);
	    break;
	case MENU_ACTION_MINIMIZE:
	    ClientMinimize(client);
	    break;
	case MENU_ACTION_RESTORE:
	    ClientRestore(client, 1);
	    break;
	    // FIXME: DeskopCreateMenu creates only SET_DESKTOP.
	case MENU_ACTION_SET_DESKTOP:
	case MENU_ACTION_SENDTO_DESKTOP:
	    ClientSetDesktop(client, command->Integer);
	    break;
	case MENU_ACTION_TOGGLE_SHADE:
	    if (((Client *) client)->State & WM_STATE_SHADED) {
		ClientUnshade(client);
	    } else {
		ClientShade(client);
	    }
	    break;
	case MENU_ACTION_MOVE:
	    ClientMoveKeyboard(client);
	    break;
	case MENU_ACTION_RESIZE:
	    ClientResizeKeyboard(client);
	    break;
	case MENU_ACTION_RAISE:
	    ClientRaise(client);
	    break;
	case MENU_ACTION_LOWER:
	    ClientLower(client);
	    break;

	case MENU_ACTION_CLOSE:
	    ClientDelete(client);
	    break;
	case MENU_ACTION_KILL:
	    ClientKill(client);
	    break;
	case MENU_ACTION_SET_LAYER:
	    ClientSetLayer(client, command->Integer);
	    break;
	default:
	    Debug(2, "unknown window command: %d\n", command->Type);
	    break;
	case MENU_ACTION_DESKTOP:
	case MENU_ACTION_SENDTO:
	case MENU_ACTION_LAYER:
	case MENU_ACTION_DIR:
	    Debug(2, "wrong window command: %d\n", command->Type);
	    break;
    }
}

/**
**	Select a window for performing an action.
**
**	Grab the mouse to select a window.
**
**	@param command	memu command to be executed after choose.
*/
static void WindowMenuChoose(const MenuCommand * command)
{
    xcb_grab_pointer_cookie_t cookie;
    xcb_generic_event_t *event;

    cookie = PointerGrabForChooseRequest();

    if (PointerGrabReply(cookie)) {
	Debug(3, "grabbed mouse\n");
    }
    while (KeepLooping) {
	if ((event = PollNextEvent())) {
	    switch (XCB_EVENT_RESPONSE_TYPE(event)) {
		case XCB_BUTTON_PRESS:
		    if (((xcb_button_press_event_t *) event)->detail ==
			XCB_BUTTON_INDEX_1) {
			Client *client;

			if ((client =
				ClientFindByAny(((const
					    xcb_button_press_event_t *)
					event)->child))) {
			    WindowMenuExecute(client, command);
			} else {
			    Debug(2, "no client selected\n");
			}
		    }
		case XCB_KEY_PRESS:
		    free(event);
		    goto out;
		default:
		    EventHandleEvent(event);
		    break;
	    }
	    free(event);
	}
	WaitForEvent();
    }
  out:
    xcb_ungrab_pointer(Connection, XCB_CURRENT_TIME);
}

/**
**	Get the size of a window menu.
**
**	@param client		client for window menu
**	@param[out] width	width return
**	@param[out] height	height return
**
**	@returns the created and prepared menu, to show it.
*/
Runtime *WindowMenuGetSize(const Client * client, unsigned *width,
    unsigned *height)
{
    Runtime *runtime;

    MenuClient = client;
    runtime = MenuPrepareRuntime(WindowMenuCreate(client));
    *width = runtime->Width;
    *height = runtime->Height;

    return runtime;
}

/**
**	Show a window menu.
**
**	@param runtime	window menu runtime of client
**	@param x	x-coordinate of menu (root relative)
**	@param y	y-coordinate of menu (root relative)
**	@param client	client for window menu
*/
void WindowMenuShow(Runtime * runtime, int x, int y, Client * client)
{
    Menu *menu;

    MenuClient = client;
    if (!runtime) {
	runtime = MenuPrepareRuntime(WindowMenuCreate(client));
    }
    MenuShowRuntime(runtime, x, y, WindowMenuExecute, client);
    menu = runtime->Menu;
    MenuCleanupRuntime(runtime);
    MenuDel(menu);
}

/// @}

#endif // } USE_MENU

/// @}
