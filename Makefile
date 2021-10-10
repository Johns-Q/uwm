#
#	@file Makefile	@brief	make file
#
#	Copyright (c) 2009 - 2011, 2021 by Lutz Sammer.  All Rights Reserved.
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

#----------------------------------------------------------------------------
#	Config

#	See uwm.h for all user setable options
#
#	Usage:
#		make CONFIG="opt1 opt2 ... optn"
#
#CONFIG	+=	'-DDEFAULT_FONT="-*-luxi sans-*-*-*-*-*-*-*-*-*-*-*-*"'

#	global default uwm system configuration file
#CONFIG += '-DSYSTEM_CONFIG="/etc/system.uwmrc"'
#	support xdg config home
#CONFIG += -DXDG_CONFIG_HOME
#	enable/disable runtime configuration
CONFIG += -DUSE_RC -DUSE_CORE_RC_GET_STRINGS
#CONFIG += -DNO_RC
#	enable/disable outline window move/resize
#CONFIG += -DUSE_OUTLINE
#CONFIG += -DNO_OUTLINE
#	enable/disable move/resize status window
#CONFIG += -DUSE_STATUS
#CONFIG += -DNO_STATUS
#	enable/disable snap for move/resize
#CONFIG += -DUSE_SNAP
#CONFIG += -DNO_SNAP
#	enable/disable confirm dialog window
#CONFIG += -DUSE_DIALOG
#CONFIG += -DNO_DIALOG
#	enable/disable menu
#CONFIG += -DUSE_MENU
#CONFIG += -DNO_MENU
#
#		enable/disable directory menu (needs menu)
#CONFIG += -DUSE_MENU_DIR
#CONFIG += -DNO_MENU_DIR
#
#	enable/disable panel
#CONFIG += -DUSE_PANEL
#CONFIG += -DNO_PANEL
#
#		enable/disable panel button plugin (needs panel)
#CONFIG += -DUSE_BUTTON
#CONFIG += -DNO_BUTTON
#		enable/disable panel clock plugin (needs panel)
#CONFIG += -DUSE_CLOCK
#CONFIG += -DNO_CLOCK
#		enable/disable panel pager plugin (needs panel)
#CONFIG += -DUSE_PAGER
#CONFIG += -DNO_PAGER
#		enable/disable panel task plugin (needs panel)
#CONFIG += -DUSE_TASK
#CONFIG += -DNO_TASK
#		enable/disable panel swallow plugin (needs panel)
#CONFIG += -DUSE_SWALLOW
#CONFIG += -DNO_SWALLOW
#		enable/disable panel systray plugin (needs panel)
#CONFIG += -DUSE_SYSTRAY
#CONFIG += -DNO_SYSTRAY
#		enable/disable panel netload plugin (needs panel)
#CONFIG += -DUSE_NETLOAD
#CONFIG += -DNO_NETLOAD
#
#	enable/disable desktop background setting(s) (depends on ICON/JPEG/PNG)
#CONFIG += -DUSE_BACKGROUND
#CONFIG	+= -DNO_BACKGROUND
#		enable/disable support for XSETROOT_ID property (needs back..)
#CONDIF += -DUSE_XSETROOT_ID
#CONDIF += -DNO_XSETROOT_ID
#
#	enable/disable icon support
#CONFIG += -DUSE_ICON
#CONFIG	+= -DNO_ICON
#		enable/disable external jpeg support (needs icon)
CONFIG += -DUSE_JPEG
#CONFIG += -DNO_JPEG
#		enable/disable external png support (needs icon)
CONFIG += -DUSE_PNG
#CONFIG += -DNO_PNG
#		enable/disable external tiff support (needs icon)
#CONFIG += -DUSE_TIFF
#CONFIG += -DNO_TIFF
#		enable/disable internal xpm support (needs icon)
#CONFIG += -DUSE_XPM
#CONFIG += -DNO_XPM

#	enable/disable window rules
#CONFIG += -DUSE_RULE
#CONFIG += -DNO_RULE
#	enable/disable buildin DIA show application (image viewer/slide show)
#CONFIG += -DUSE_DIA
#CONFIG += -DNO_DIA
#	enable/disable buildin tower defense application
#CONFIG += -DUSE_TD
#CONFIG += -DNO_TD

#	use X Xinerama Extension
CONFIG += -DUSE_XINERAMA
#CONFIG += -DNO_XINERAMA
#	use X Rendering Extension
#CONFIG += -DUSE_RENDER
#CONFIG += -DNO_RENDER
#	use X Shape Extentsion
#CONFIG += -DUSE_SHAPE
#CONFIG += -DNO_SHAPE
#	use XMU emulation (needed for rounded corners)
#CONFIG += -DUSE_XMU
#CONFIG += -DNO_XMU
#	enable/disable colormap support
#CONFIG += -DUSE_COLORMAPS
#CONFIG += -DNO_COLORMAPS
#	enable/disable motif hints support
#CONFIG += -DUSE_MOTIF_HINTS
#CONFIG += -DNO_MOTIF_HINTS

#	enable/disable debug
#CONFIG += -DDEBUG
#CONFIG += -DNO_DEBUG

DEFS = $(CONFIG) #### $(addprefix -D, $(CONFIG))

#----------------------------------------------------------------------------

VERSION	=	"0.33"
GIT_REV =	$(shell git describe --always 2>/dev/null)

#CC=	gcc
#CC=	tcc
#CC=	clang
#CC=	/opt/ekopath/bin/pathcc

#MARCH=	-march=armv6j -mtune=arm1136jf-s -mfpu=vfp -mfloat-abi=softfp
#MARCH=	-march=native
#MARCH=	-muclibc
OPTIM=	-U_FORTIFY_SOURCE -D__OPTIMIZE__ -Os -fomit-frame-pointer
CFLAGS= $(MARCH) $(OPTIM) -pipe -g -W -Wall -Wextra -Winit-self \
	-Wdeclaration-after-statement
override CFLAGS+= -I. $(DEFS) -DVERSION='$(VERSION)' \
	$(if $(GIT_REV), -DGIT_REV='"$(GIT_REV)"') \
	$(if $(findstring DDEBUG,$(CONFIG)), -Werror)
ifeq ($(CC),tcc)
    LDFLAGS=
else
    LDFLAGS=	-Wl,--sort-common,--gc-sections,--as-needed
endif
LIBS=	`pkg-config --static --libs xcb-keysyms xcb-aux xcb-atom xcb-util \
	xcb-event xcb-icccm xcb-shape xcb-renderutil xcb-render xcb-image \
	xcb-shm xcb` \
	$(if $(findstring USE_XINERAMA,$(CONFIG)), \
	    `pkg-config --static --libs xcb-xinerama`) \
	$(if $(findstring USE_PNG,$(CONFIG)), \
	    `pkg-config --static --libs libpng`) \
	$(if $(findstring USE_JPEG,$(CONFIG)), -ljpeg) \
	$(if $(findstring USE_TIFF,$(CONFIG)), -ltiff) \
	-lpthread
OBJS	= uwm.o command.o pointer.o keyboard.o draw.o image.o icon.o \
	tooltip.o hints.o screen.o background.o desktop.o menu.o \
	rule.o border.o client.o moveresize.o event.o property.o misc.o \
	panel.o plugin/button.o plugin/pager.o plugin/task.o plugin/swallow.o \
	plugin/systray.o plugin/clock.o plugin/netload.o \
	dia.o td.o
SRCS	= $(OBJS:.o=.c)
HDRS	= uwm.h command.h pointer.h keyboard.h draw.h image.h icon.h \
	tooltip.h hints.h screen.h background.h desktop.h menu.h \
	rule.h border.h client.h moveresize.h event.h property.h misc.h \
	panel.h plugin/button.h plugin/pager.h plugin/task.h plugin/swallow.h \
	plugin/systray.h plugin/clock.h plugin/netload.h \
	readable_bitmap.h dia.h td.h uwm-config.h queue.h

FILES=	Makefile u.xpm contrib/uwm-helper.sh.in uwm.1 uwmrc.5 CODINGSTYLE.txt \
	README.txt ChangeLog contrib/uwm.doxyfile LICENSE.md AGPL-v3.0.md \
	contrib/uwm16x16.xpm contrib/x.xpm contrib/uwmrc.example

all:	uwm #udm

#	fix missing files
td.c:
	touch td.c

td.h:
	touch td.h

uwm-config.h:
	touch uwm-config.h

#----------------------------------------------------------------------------
#	Modules
#

core-rc/core-rc.mk:
	git submodule init core-rc
	git submodule update core-rc

core-array/core-array.mk:
	git submodule init core-array
	git submodule update core-array

ifeq ($(findstring -DNO_RC,$(CONFIG)),)
include core-rc/core-rc.mk
endif
include core-array/core-array.mk

#----------------------------------------------------------------------------

uwm:	$(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

udm:	udm.o image.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

$(OBJS):Makefile $(HDRS)

contrib/uwm-helper:	contrib/uwm-helper.sh.in
	cp $^ $@

#----------------------------------------------------------------------------
#	Developer tools

.PHONY: doc indent clean clobber distclean dist

doc:	$(SRCS) $(HDRS) contrib/uwm.doxyfile
	(cat contrib/uwm.doxyfile; \
	echo 'PROJECT_NUMBER=${VERSION} $(if $(GIT_REV), (GIT-$(GIT_REV)))') \
	| doxygen -

indent:
	for i in $(OBJS:.o=.c) $(HDRS); do \
		indent $$i; unexpand -a $$i > $$i.up; mv $$i.up $$i; \
	done

clean:
	-rm core *.o *~ plugin/*.o plugin/*~

clobber distclean:	clean
	-rm -rf uwm udm doc/html

dist:
	tar cjf uwm-`date +%F-%H`.tar.bz2 \
		--transform "s,^,uwm-`date +%F-%H`/," \
		$(FILES) $(HDRS) $(OBJS:.o=.c)

dist-version:
	tar cjf uwm-$(VERSION).tar.bz2 \
		--transform "s,^,uwm-$(VERSION)/," \
		$(FILES) $(HDRS) $(OBJS:.o=.c)

dist-git:
	git tag v$(VERSION)
	git archive --format=tar --prefix="uwm-$(VERSION)/" v$(VERSION) | \
		gzip > uwm-$(VERSION).tar.gz

DESTDIR=/usr/local

install: all contrib/uwm-helper uwm.1 uwmrc.5
	mkdir -p $(DESTDIR)/bin
	strip --strip-unneeded -R .comment uwm
	install -s uwm $(DESTDIR)/bin/
	chmod +x contrib/uwm-helper
	install contrib/uwm-helper $(DESTDIR)/bin/
	install -D uwm.1 $(DESTDIR)/share/man/man1/uwm.1
	install -D uwmrc.5 $(DESTDIR)/share/man/man5/uwmrc.5
	strip --strip-unneeded -R .comment udm
	install -s udm $(DESTDIR)/bin/
