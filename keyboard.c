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
#include <string.h>

#include <xcb/xcb_icccm.h>
#include <xcb/xcb_keysyms.h>
#include <X11/keysym.h>			// keysym XK_

#include "core-array/core-array.h"
#include "core-rc/core-rc.h"

#include "draw.h"
#include "image.h"
#include "pointer.h"
#include "client.h"
#include "keyboard.h"
#include "icon.h"
#include "menu.h"

//////////////////////////////////////////////////////////////////////////////

/**
**	Keyboard key.
*/
typedef struct _keyboard_key_
{
    uint16_t Modifier;			///< modifier
    xcb_keysym_t KeySym;		///< keyboard symbol
} KeyboardKey;

/**
**	Keyboard binding.
*/
typedef struct _keyboard_binding_
{
    KeyboardKey Key;			///< key sequence for command
    MenuCommand Command;		///< command to execute
} KeyboardBinding;

static xcb_key_symbols_t *XcbKeySymbols;	///< Keyboard symbols
static uint16_t NumLockMask;		///< mod mask for num-lock
static uint16_t ShiftLockMask;		///< mod mask for shift-lock
static uint16_t CapsLockMask;		///< mod mask for caps-lock
static uint16_t ModeSwitchMask;		///< mod mask for mode-switch

    /// table of keyboard bindings
static KeyboardBinding *KeyboardBindings;

    /// number of keyboard bindings in table
static int KeyboardBindingN;

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
**	To fix keyboard with Xephyr 1.8.1 use "-keybd ephyr,,,xkbmodel=evdev".
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

    Debug(3, " %x+%x\n", ks0, ks1);

    // use first keysym, if second keysym didn't exists
    if (ks1 == XCB_NO_SYMBOL) {
	ks1 = ks0;
    }
    // see xcb-util-0.3.6/keysyms/keysyms.c:
    if (!(modifier & XCB_MOD_MASK_SHIFT) && !(modifier & XCB_MOD_MASK_LOCK)) {
	return ks0;
    }
    // FIXME: more cases

    return ks0;
}

/**
**	Grab a key on client window.
**
**	@param client		window client
**	@param modifiers	X11 modifiers
**	@param keysym		X11 keycode
*/
static void KeyboardGrabKey(Client * client, unsigned modifiers,
    unsigned keysym)
{
    xcb_keycode_t *keycodes;

    //
    //	FIXME: grab key with all lock modifiers
    //

    keycodes = xcb_key_symbols_get_keycode(XcbKeySymbols, keysym);
    if (keycodes) {
	xcb_keycode_t *key_code;
	xcb_keycode_t last;

	last = 0;
	for (key_code = keycodes; *key_code; key_code++) {
	    Debug(3, "grab keycode %x %x\n", modifiers, *key_code);
	    if (*key_code == last) {	// ignore simple duplicates
		Debug(3, "double keycodes\n");
		continue;
	    }
	    last = *key_code;
	    xcb_grab_key(Connection, 1, client->Window, modifiers, *key_code,
		XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
	}
	free(keycodes);
    }
}

/**
**	Grab our key bindings on client window.
**
**	@param client	window client
*/
void KeyboardGrabBindings(Client * client)
{
    int i;

    //
    //	go through all bindings
    //
    for (i = 0; i < KeyboardBindingN; ++i) {
	Debug(3, "grab-bindings: %x %x\n", KeyboardBindings[i].Key.Modifier,
	    KeyboardBindings[i].Key.KeySym);
	KeyboardGrabKey(client, KeyboardBindings[i].Key.Modifier,
	    KeyboardBindings[i].Key.KeySym);
    }
}

// ------------------------------------------------------------------------ //

#ifdef DEBUG

/**
**	Make string from modifier mask.
**
**	@param mask	keyboard modifier mask of keyboard event
**
**	@returns readable string for mask.
*/
static const char *KeyboardMask2String(uint16_t mask)
{
    switch (mask) {
	case XCB_KEY_BUT_MASK_SHIFT:
	    return "Shift";
	case XCB_KEY_BUT_MASK_LOCK:
	    return "Lock";
	case XCB_KEY_BUT_MASK_CONTROL:
	    return "Control";
	case XCB_KEY_BUT_MASK_MOD_1:
	    return "Mod1";
	case XCB_KEY_BUT_MASK_MOD_2:
	    return "Mod2";
	case XCB_KEY_BUT_MASK_MOD_3:
	    return "Mod3";
	case XCB_KEY_BUT_MASK_MOD_4:
	    return "Mod4";
	case XCB_KEY_BUT_MASK_MOD_5:
	    return "Mod5";
	case XCB_KEY_BUT_MASK_BUTTON_1:
	    return "Button1";
	case XCB_KEY_BUT_MASK_BUTTON_2:
	    return "Button2";
	case XCB_KEY_BUT_MASK_BUTTON_3:
	    return "Button3";
	case XCB_KEY_BUT_MASK_BUTTON_4:
	    return "Button4";
	case XCB_KEY_BUT_MASK_BUTTON_5:
	    return "Button5";
	default:
	    return "Unknown";
    }
}

#endif

/**
**	Key pressed or released.
**
**	@param pressed	true, key was pressed; false, key was released.
**	@param key	key
**	@param state	state
**
**	@todo pressing keysym simultaneous for a command, isn't supported.
**	@todo pressing keysym in sequence for a command, isn't supported.
**	@todo check shift-lock num-lock
*/
void KeyboardHandler(int pressed, unsigned key, unsigned state)
{
    int i;
    xcb_keysym_t keysym;

#ifdef DEBUG
    Debug(4, "%s: %d, %d, %d ", __FUNCTION__, pressed, key, state);
    if (state) {
	Debug(5, "modifier: ");
	for (i = 0; i < 16; ++i) {
	    if (state & (1 << i)) {
		Debug(5, "%s ", KeyboardMask2String(state & (1 << i)));
	    }
	}
    }
#endif

    if (!pressed) {			// for now ignore any release
	return;
    }
    // convert keycode into keysym
    keysym = KeyboardGet(key, state);
    Debug(3, "keysym %#010x\n", keysym);

    //
    //	search mapping for the key
    //
    for (i = 0; i < KeyboardBindingN; ++i) {
	if (KeyboardBindings[i].Key.Modifier == state
	    && KeyboardBindings[i].Key.KeySym == keysym) {
	    int x;
	    int y;

	    Debug(4, "found key with command %d\n",
		KeyboardBindings[i].Command.Type);

	    PointerGetPosition(&x, &y);
	    MenuCommandExecute(&KeyboardBindings[i].Command, x, y);
	    break;
	}
    }

    // alert
    //xcb_bell(Connection, 100);
}

// ------------------------------------------------------------------------ //

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
    int i;

    xcb_key_symbols_free(XcbKeySymbols);
    XcbKeySymbols = NULL;

    //
    //	free memory used by keyboard bindings
    //
    for (i = 0; i < KeyboardBindingN; ++i) {
	MenuCommandDel(&KeyboardBindings[i].Command);
    }
}

// ------------------------------------------------------------------------ //
// Config

#ifdef USE_RC				// {

/**
**	Parse keyboard key-list configuration.
**
**	@param keylist	list of keysym and modifier
**	@param[out] key	key sequence
*/
static void KeyboardKeylistConfig(const ConfigObject * keylist,
    KeyboardKey * key)
{
    const ConfigObject *index;
    const ConfigObject *value;
    uint16_t modifier;
    xcb_keysym_t keysym;

    modifier = 0;
    keysym = 0;

    //
    //	array of keys
    //
    index = NULL;
    value = ConfigArrayFirstFixedKey(keylist, &index);
    while (value) {
	ssize_t ival;

	if (ConfigCheckInteger(value, &ival)) {
	    Debug(4, "\t\tkey %#010zx\n", ival);
	    if (ival & 0x20000000) {	// modifier
		if (modifier & ival) {
		    Warning("double modifier in keylist ignored\n");
		}
		modifier |= ival;
	    } else {			// keyboard symbol
		if (keysym) {
		    Warning("double keysym in keylist ignored\n");
		}
		keysym = ival;
	    }
	} else {
	    Warning("value in key list ignored\n");
	}
	value = ConfigArrayNextFixedKey(keylist, &index);
    }

    if (!modifier && !keysym) {
	Warning("no key list defined\n");
    }
    Debug(4, "\t\t modifier %#04x %#010x\n", modifier, keysym);

    key->Modifier = modifier;
    key->KeySym = keysym;
}

/**
**	Parse keyboard binding configuration.
**
**	@param bindings	keyboard bindings array
*/
static void KeyboardBindingConfig(const ConfigObject * binding)
{
    const ConfigObject *index;
    const ConfigObject *value;
    int keyn;
    KeyboardKey key;
    MenuCommand command;

    NO_WARNING(key);
    keyn = 0;

    //
    //	array of keys
    //
    index = NULL;
    value = ConfigArrayFirstFixedKey(binding, &index);
    while (value) {
	const ConfigObject *aval;

	if (ConfigCheckArray(value, &aval)) {
	    KeyboardKeylistConfig(aval, &key);
	    ++keyn;
	} else {
	    Warning("value in key-binding ignored\n");
	}
	value = ConfigArrayNextFixedKey(binding, &index);
    }
    if (!keyn) {
	Warning("no key list given\n");
    } else if (keyn != 1) {
	Warning("only single key list support\n");
    }
    //
    //	parse command of key list
    //
    MenuCommandConfig(binding, &command);

    //
    //	insert binding into table
    //
    KeyboardBindings =
	realloc(KeyboardBindings,
	(KeyboardBindingN + 1) * sizeof(*KeyboardBindings));
    KeyboardBindings[KeyboardBindingN].Key = key;
    KeyboardBindings[KeyboardBindingN].Command = command;
    ++KeyboardBindingN;
}

/**
**	Parse keyboard/command configuration.
**
**	@param config	global config dictionary
*/
void KeyboardConfig(const Config * config)
{
    const ConfigObject *array;

    if (ConfigGetArray(ConfigDict(config), &array, "key-binding", NULL)) {
	const ConfigObject *index;
	const ConfigObject *value;

	//MenuButtonsConfig(array, &RootButtons);
	//
	//	array of key bindings
	//
	index = NULL;
	value = ConfigArrayFirstFixedKey(array, &index);
	while (value) {
	    const ConfigObject *aval;

	    if (ConfigCheckArray(value, &aval)) {
		KeyboardBindingConfig(aval);
	    } else {
		Warning("value in key-binding ignored\n");
	    }
	    value = ConfigArrayNextFixedKey(array, &index);
	}
    }
}

#endif // } USE_RC

/// @}
