@file README.md		@brief µ window manager readme

Copyright (c) 2009 - 2011, 2021 by Lutz Sammer.  All Rights Reserved.

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

$Id: 26a94379bd0278ab32d3d3d672816c8e4b642893 $

@mainpage
									[TOC]

µwm - (uwm) µ Window Manager					{#mainpage}
============================
   Based on jwm from Joe Wingbermuehle, but rewritten from scratch.

about
-----
   µwm is a lightweight stacking window manager for the X11 Window System
   and is written in C and uses only libxcb at a minimum.  It has builtin
   menus/panels/buttons and other plugins.

overview
--------
   - stacking window manager
   - written in C (with many comments)
   - less dependencies (only XCB and X11 required)
   - doesn't use/need GNU autoconfigure and other auto-tools
   - executable less than 200k
   - less than 23k Source Lines of Code
   - low memory footprint during runtime ~ 1800k RES
   - low X11 resource usage ~ 200k (without desktop backgrounds)
   - low CPU usage

features
--------
   - configurable and themeable
   - no XML config file
   - builtin menu(s)
   - builtin panel(s) (other names are slit/bar/dock) with:
      - button
      - clock
      - netload
      - pager
      - swallow (dock)
      - systray
      - task-list
   - builtin tooltips
   - builtin background setter
   - composite support with xcompmgr (sample X compositing manager)
   - multiple desktops
   - multiple screen (xinerama)
   - 64-bit and 32-bit clean
   - little-endian and big-endian clean
   - compatible with musl, uclibc and libc6
   - compatible with GCC 4.5.3-11.2.0, Clang 2.9-13.0 and ekopath 4.0.11
   - many features can be compile time enabled / disabled

see @ref install

@page install
how to install µwm
------------------
   To compile you must have the 'requires' installed.

Quickstart:
-----------
   Just type make and use.

   To configure which modules to include, see [Makefile](Makefile)
   and [uwm.h](uwm.h)

   To configure the look&feel, see
   [contrib/uwmrc.example](contrib/uwmrc.example)

Install:
--------
   1. git
```
      git clone git://uwm.git.sourceforge.net/gitroot/uwm/uwm
      -or-
      git clone https://github.com/Johns-Q/uwm.git

      cd uwm
      # make should automatic pull the submodules from git.
      make
      ./xephyr.sh ./uwm -c contrib/uwmrc.example
```
   2. tarball

      *this versions are very outdated!*
      Download latest version from:
	 http://sourceforge.net/projects/uwm/files/
```
      tar vxf uwm-2011-*.tar.bz2
      cd uwm
      # the tarball contains the submodules
      make
      ./xephyr.sh ./uwm -c contrib/uwmrc.example
```
More:
-----
   - To build and see the source code documentation use:
```
      make doc
      firefox doc/html/index.html
```


Requires: {#requires}
---------
   - x11-libs/libxcb

      X C-language Bindings library
      http://xcb.freedesktop.org/
   - x11-libs/xcb-util,
   - x11-libs/xcb-util-wm,
   - x11-libs/xcb-util-image,
   - x11-libs/xcb-util-keysyms,
   - x11-libs/xcb-util-renderutil

      extra X C-language Bindings library
      http://xcb.freedesktop.org/

   - x11-libs/libX11 (for header files missing in xcb)

      X.Org X11 library
      http://xorg.freedesktop.org/
 
   - bsd-compat-headers

      for /usr/include/sys/queue.h and older musl

   - POSIX regex functions

      included in the GNU libc6 (also called glibc2) C library
      http://www.gnu.org/software/libc/libc.html

   - GNU Make 3.xx

      http://www.gnu.org/software/make/make.html

   - core-rc

      core runtime configuration.
      (included in source tarball) or pulled by the makefile from
      http://sourceforge.net/projects/core-rc/

   - core-array

      core general purpose associative array.
      (included in source tarball) or pulled by the makefile from
      http://sourceforge.net/projects/core-array/

### Optional:
   - media-libs/jpeg

      Library to load, handle and manipulate images in the JPEG
      format
      http://www.ijg.org/

   - media-libs/libpng

      Portable Network Graphics library
      http://www.libpng.org/

   - media-libs/tiff
      not yet supported!

      Library for manipulation of TIFF (Tag Image File Format) images
      http://www.remotesensing.org/libtiff/

   - media-fonts/dejavu

      DejaVu fonts, bitstream vera with ISO-8859-2 characters
      http://dejavu.sourceforge.net/

   - media-fonts/terminus-font

      A clean fixed font for the console and X11
      http://www.is-vn.bg/hamster/

   - media-fonts/font-alias

      X.Org font aliases
      http://xorg.freedesktop.org/

   - media-fonts/font-cursor-misc

      X.Org cursor font
      http://xorg.freedesktop.org/

   - media-fonts/font-misc-misc

      X.Org miscellaneous fonts
      http://xorg.freedesktop.org/

   - Doxygen Awesome

      Doxygen Awesome is a custom CSS theme for doxygen html-documentation
      with lots of customization parameters.
      https://jothepro.github.io/doxygen-awesome-css/

   - peg

      Parsing Expression Grammar - parser generator
      http://piumarta.com/software/peg
      needed for core-rc, if you like to modify the grammer

