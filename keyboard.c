///
///	@file keyboard.c	@brief keyboard functions.
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
///	@defgroup keyboard The keyboard module.
///
///	This module handles keyboard input.
///
///	@todo parse keyboard config, grab keys.
///
/// @{

#include <xcb/xcb.h>
#include "uwm.h"

#include <sys/queue.h>

#include <stdio.h>
#include <stdlib.h>

#include <xcb/xcb_icccm.h>
#include <xcb/xcb_keysyms.h>
#include <X11/keysym.h>			// keysym XK_

#include "array.h"
#include "config.h"

#include "client.h"

extern Config *UwmConfig;		///< µwm config

//////////////////////////////////////////////////////////////////////////////

static xcb_key_symbols_t *XcbKeySymbols;	///< Keyboard symbols
static uint16_t NumLockMask;		///< mod mask for num-lock
static uint16_t ShiftLockMask;		///< mod mask for shift-lock
static uint16_t CapsLockMask;		///< mod mask for caps-lock
static uint16_t ModeSwitchMask;		///< mod mask for mode-switch

/**
**	Grab keyboard, send request.
**
**	@param window	window grabbed
**
**	@returns cookie of grab request.
*/
xcb_grab_keyboard_cookie_t KeyboardGrabRequest(xcb_window_t window)
{
    return xcb_grab_keyboard_unchecked(Connection, 0, window, XCB_CURRENT_TIME,
	XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
}

/**
**	Grab keyboard, read reply.
**
**	@param cookie	cookie from grab request
**
**	@returns true, if grabbed successfull, false otherwise.
*/
int KeyboardGrabReply(xcb_grab_keyboard_cookie_t cookie)
{
    xcb_grab_keyboard_reply_t *reply;

    if ((reply = xcb_grab_keyboard_reply(Connection, cookie, NULL))) {
	int status;

	Debug(3, "  grab keyboard %d\n", reply->status);
	status = reply->status == XCB_GRAB_STATUS_SUCCESS;
	free(reply);
	return status;
    }
    return 0;
}

/**
**	Get keysym from event.
**
**	@param keycode	keycode from keyboard event
**	@param modifier	modifier from keyboard event
**
**	@returns keyboard symbol mapped on keycode.
**
**	Somebody: bugs here, I, GNU? xcb?
**	NoSymbol returned for any key, check Xephyr!
*/
xcb_keysym_t KeyboardGet(xcb_keycode_t keycode, unsigned modifier)
{
    xcb_keysym_t ks0;
    xcb_keysym_t ks1;

    Debug(3, "key %x:%x ", modifier, keycode);

    // XCB_MOD_MASK_SHIFT XCB_MOD_MASK_LOCK XCB_MOD_MASK_CONTROL
    // XCB_MOD_MASK_1 .. XCB_MOD_MASK_5

    // handle mode-switch
    if (modifier & ModeSwitchMask) {
	ks0 = xcb_key_symbols_get_keysym(XcbKeySymbols, keycode, 2);
	ks1 = xcb_key_symbols_get_keysym(XcbKeySymbols, keycode, 3);
    } else {
	ks0 = xcb_key_symbols_get_keysym(XcbKeySymbols, keycode, 0);
	ks1 = xcb_key_symbols_get_keysym(XcbKeySymbols, keycode, 1);
    }

    // use first keysym, if second keysym didn't exists
    if (ks1 == XCB_NO_SYMBOL) {
	ks1 = ks0;
    }
    // see xcb-util-0.3.6/keysyms/keysyms.c:
    if (!(modifier & XCB_MOD_MASK_SHIFT) && !(modifier & XCB_MOD_MASK_LOCK)) {
	Debug(3, " %x\n", ks0);
	return ks0;
    }
    // FIXME: more cases

    Debug(3, " %x\n", ks0);

    return ks0;
}

/**
**	Grab our key bindings on client window.
**
**	@param client	window client
*/
void KeyboardGrabBindings(Client * client)
{
    Debug(0, "%s: FIXME: %p\n", __FUNCTION__, client);
}

/**
**	Initialize the keyboard module.
*/
void KeyboardInit(void)
{
    xcb_get_modifier_mapping_cookie_t cookie;
    xcb_get_modifier_mapping_reply_t *reply;
    xcb_keycode_t *modmap;
    xcb_keycode_t *num_lock;
    xcb_keycode_t *shift_lock;
    xcb_keycode_t *caps_lock;
    xcb_keycode_t *mode_switch;

    cookie = xcb_get_modifier_mapping_unchecked(Connection);

    XcbKeySymbols = xcb_key_symbols_alloc(Connection);
    if (!XcbKeySymbols) {
	Error("Can't read key symbols\n");
	return;
    }
    //
    //	find lock mask for NUM-LOCK, SHIFT-LOCK, CAPS-LOCK, MODE-SWITCH
    //
#ifndef XCB_EVENT_ERROR_SUCESS
    num_lock = NULL;
    shift_lock = NULL;
    caps_lock = NULL;
#else
    // xcb-utils changed xcb_keycode_t to xcb_keycode_t*
    num_lock = xcb_key_symbols_get_keycode(XcbKeySymbols, XK_Num_Lock);
    shift_lock = xcb_key_symbols_get_keycode(XcbKeySymbols, XK_Shift_Lock);
    caps_lock = xcb_key_symbols_get_keycode(XcbKeySymbols, XK_Caps_Lock);
    mode_switch = xcb_key_symbols_get_keycode(XcbKeySymbols, XK_Mode_switch);
#endif

    NumLockMask = ShiftLockMask = CapsLockMask = ModeSwitchMask = 0;

    reply = xcb_get_modifier_mapping_reply(Connection, cookie, NULL);
    if (reply) {
	int i;
	int j;

	modmap = xcb_get_modifier_mapping_keycodes(reply);
	for (i = 0; i < 8; ++i) {
	    for (j = 0; j < reply->keycodes_per_modifier; ++j) {
		xcb_keycode_t key_code;
		int k;

		key_code = modmap[i * reply->keycodes_per_modifier + j];
		// unused slot?
		if (key_code == XCB_NO_SYMBOL) {
		    continue;
		}
		if (num_lock) {
		    for (k = 0; num_lock[k] != XCB_NO_SYMBOL; ++k) {
			if (num_lock[k] == key_code) {
			    Debug(3, "Found num-lock %d\n", 1 << i);
			    NumLockMask = 1 << i;
			    break;
			}
		    }
		}
		if (shift_lock) {
		    for (k = 0; shift_lock[k] != XCB_NO_SYMBOL; ++k) {
			if (shift_lock[k] == key_code) {
			    Debug(3, "Found shift-lock %d\n", 1 << i);
			    ShiftLockMask = 1 << i;
			    break;
			}
		    }
		}
		if (caps_lock) {
		    for (k = 0; caps_lock[k] != XCB_NO_SYMBOL; ++k) {
			if (caps_lock[k] == key_code) {
			    Debug(3, "Found caps-lock %d\n", 1 << i);
			    CapsLockMask = 1 << i;
			    break;
			}
		    }
		}
		if (mode_switch) {
		    for (k = 0; mode_switch[k] != XCB_NO_SYMBOL; ++k) {
			if (mode_switch[k] == key_code) {
			    Debug(3, "Found mode_switch %d\n", 1 << i);
			    ModeSwitchMask = 1 << i;
			    break;
			}
		    }
		}
	    }
	}
	free(reply);
    }
    Debug(3, "xcb mod mask lock: %d\n", XCB_MOD_MASK_LOCK);

    free(num_lock);
    free(shift_lock);
    free(caps_lock);
    free(mode_switch);
}

/**
**	Cleanup the keyboard module.
*/
void KeyboardExit(void)
{
    xcb_key_symbols_free(XcbKeySymbols);
    XcbKeySymbols = NULL;
}

/**
**	Parse keyboard/command configuration.
*/
void KeyboardConfig(void)
{
    const ConfigObject *array;

    if (ConfigGetArray(ConfigDict(UwmConfig), &array, "key-bindings", NULL)) {
	//MenuButtonsConfig(array, &RootButtons);
	Debug(0, "FIXME: parse key bindings\n");
    }
}

/// @}
