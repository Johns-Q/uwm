@file README.txt		@brief µ window manager

Copyright (c) 2009, 2010 by Lutz Sammer.  All Rights Reserved.

Contributor(s):

License: AGPLv3

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as
published by the Free Software Foundation, either version 3 of the
License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

$Id$

Based on jwm from Joe Wingbermuehle, but rewritten from scratch.

To compile you must have the 'requires' installed.

Quickstart:
-----------

Just type make and use.

To configure which modules to include, see Makefile and uwm.h

To configure the look&feel, see contrib/uwmrc.example

Install:
--------
	1a) git

	git clone git://uwm.git.sourceforge.net/gitroot/uwm/uwm
	cd uwm
	# make should automatic pull the submodules from git.
	make
	./xephyr.sh ./uwm -c contrib/uwmrc.example

	2a) tarball

	Download latest version from:
	    http://sourceforge.net/projects/uwm/files/

	tar vxf uwm-2010-*.tar.bz2
	cd uwm
	# the tarball contains the submodules
	make
	./xephyr.sh ./uwm -c contrib/uwmrc.example

Requires:
---------
	x11-libs/libxcb and x11-libs/xcb-util
		X C-language Bindings library
		http://xcb.freedesktop.org/

	POSIX regex functions
		included in the GNU libc6 (also called glibc2) C library
		http://www.gnu.org/software/libc/libc.html

	GNU Make 3.xx
		http://www.gnu.org/software/make/make.html

	core-rc
		core runtime configuration.
		(included in source tarball) or
		http://sourceforge.net/projects/core-rc/

	core-array
		core general purpose associative array.
		(included in source tarball) or
		http://sourceforge.net/projects/core-array/

Optional:
	media-libs/jpeg
		Library to load, handle and manipulate images in the JPEG
		format
		http://www.ijg.org/

	media-libs/libpng
		Portable Network Graphics library
		http://www.libpng.org/

	media-libs/tiff
		Library for manipulation of TIFF (Tag Image File Format) images
		http://www.remotesensing.org/libtiff/

	media-fonts/dejavu
		DejaVu fonts, bitstream vera with ISO-8859-2 characters
		http://dejavu.sourceforge.net/

	media-fonts/terminus-font
		A clean fixed font for the console and X11
		http://www.is-vn.bg/hamster/

	media-fonts/font-misc-misc
		X.Org miscellaneous fonts
		http://xorg.freedesktop.org/
	peg
		Parsing Expression Grammar - parser generator
		http://piumarta.com/software/peg
		needed for core-rc, if you like to modify the grammer
