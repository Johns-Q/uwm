#!/bin/sh
#
#	@file xephyr.sh		@brief	Xephyr start script
#
#	Copyright (c) 2009, 2010 by Lutz Sammer.  All Rights Reserved.
#
#	Contributor(s):
#
#	License: AGPLv3
#
#	This program is free software: you can redistribute it and/or modify
#	it under the terms of the GNU Affero General Public License as
#	published by the Free Software Foundation, either version 3 of the
#	License.
#
#	This program is distributed in the hope that it will be useful,
#	but WITHOUT ANY WARRANTY; without even the implied warranty of
#	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#	GNU Affero General Public License for more details.
#
#	$Id$
#----------------------------------------------------------------------------

#	Usage:
#		./xephyr ./uwm [uwm-options]
#

#not working :(
#trap 'echo sigusr1' SIGUSR1

# fix font path of Xephyr
FONTS="`xset q | grep usr/share`"

# this didn't work (left is Super_R and up PRINT)
#KB="-keybd ephyr,,,xkbmodel=evdev,xkblayout=de"
#KB="-keybd ephyr,,,xkbmodel=evdev"

# fix keyboard of Xephyr part 1
xmodmap -pke >/tmp/xephyr.xmodmap.$$

# -reset x8 -host-cursor
Xephyr :1 -fp $FONTS -ac -wr -screen 800x480 $KB +xinerama -terminate &
export DISPLAY=:1.0
# wait for server started
#pidof Xephyr
sleep 3s
exec "$@" &
# fix Xephyr keyboard part 2
xmodmap /tmp/xephyr.xmodmap.$$; rm /tmp/xephyr.xmodmap.$$
