#
#	@file Makefile	@brief	make file
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

#----------------------------------------------------------------------------
#	Config

#	See uwm.h for all user setable options
#
#	Usage:
#		make CONFIG="opt1 opt2 ... optn"
#
#CONFIG	+=	'-DDEFAULT_FONT="-*-luxi sans-*-*-*-*-*-*-*-*-*-*-*-*"'

#	support desktop background setting(s) (depends on ICON/JPEG/PNG)
#CONFIG	+= -DNO_BACKGROUND
#CONFIG  += -DUSE_BACKGROUND

#	use X Rendering Extension (disabled due BUG (64bit crash!))
CONFIG += -DNO_RENDER
#	use X Shape Extentsion
#CONFIG += -DNO_SHAPE
#	use XMU emulation (needed for rounded corners)
CONFIG += -DNO_XMU
#	use buildin DIA show (image viewer/slide show)
#CONFIG += -DUSE_DIA
#	diable buildin DIA show (image viewer/slide show)
#CONFIG += -DNO_DIA

#	enable debug
#CONFIG += -DDEBUG
#	disable debug
#CONFIG += -DNO_DEBUG

DEFS = $(CONFIG) #### $(addprefix -D, $(CONFIG))

#----------------------------------------------------------------------------

VERSION	=	"0.22"
GIT_REV =	$(shell git describe --always 2>/dev/null)

CC=	gcc

#MARCH=	-march=armv6j -mtune=arm1136jf-s -mfpu=vfp -mfloat-abi=softfp
#MARCH=	-march=native
#MARCH=	-muclibc
#
OPTIM=	-U_FORTIFY_SOURCE -D__OPTIMIZE__ -O0 # -Os -fomit-frame-pointer
CFLAGS= $(MARCH) $(OPTIM) -W -Wall -Wextra -Werror -g -pipe \
	-I. $(DEFS) -DVERSION='$(VERSION)' \
	$(if $(GIT_REV), -DGIT_REV='"$(GIT_REV)"') \
	$(if $(findstring -DDEBUG,$(CONFIG)), -Werror)
LIBS=	`pkg-config --static --libs xcb-keysyms xcb-aux xcb-atom xcb-property \
	xcb-event xcb-icccm xcb-shape xcb-renderutil xcb-render xcb-image \
	xcb-shm xcb` `pkg-config --static --libs libpng` -ljpeg -ltiff -lpthread

OBJS	= uwm.o command.o pointer.o keyboard.o draw.o image.o icon.o \
	tooltip.o hints.o screen.o background.o desktop.o menu.o \
	rule.o border.o client.o moveresize.o event.o \
	panel.o plugin/button.o plugin/pager.o plugin/task.o plugin/swallow.o \
	plugin/systray.o plugin/clock.o \
	array.o config.o dia.o td.o
SRCS	= $(OBJS:.o=.c)
HDRS	= uwm.h command.h pointer.h keyboard.h draw.h image.h icon.h \
	tooltip.h hints.h screen.h background.h desktop.h menu.h \
	rule.h border.h client.h moveresize.h event.h \
	panel.h plugin/button.h plugin/pager.h plugin/task.h plugin/swallow.h \
	plugin/systray.h plugin/clock.h \
	array.h config.h readable_bitmap.h dia.h td.h

FILES=	Makefile u.xpm uwmrc.peg uwmrc.c.in contrib/uwm.doxyfile \
	contrib/uwm16x16.xpm contrib/uwmrc.example

all:	uwm #udm

uwm:	$(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

udm:	udm.o array.o config.o image.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

#
#	array standalone to debug it
#
array:	array.c
	$(CC) $(CFLAGS) -DTEST -DDEBUG $(LDFLAGS) -o $@ $^ $(LIBS)

array2:	array2.c
	$(CC) $(CFLAGS) -DTEST -DDEBUG $(LDFLAGS) -o $@ $^ $(LIBS)

#uslab:	uslab.c
#	$(CC) $(CFLAGS) -DTEST -DDEBUG $(LDFLAGS) -o $@ $^ $(LIBS)

$(OBJS):Makefile $(HDRS)

config.o: uwmrc.c

uwmrc.c: uwmrc.peg
	peg -o uwmrc.c uwmrc.peg || cp uwmrc.c.in uwmrc.c

#----------------------------------------------------------------------------
#	Developer tools

.PHONY: doc indent clean clobber dist

doc:	$(SRCS) $(HDRS) contrib/uwm.doxyfile
	(cat contrib/uwm.doxyfile; \
	echo 'PROJECT_NUMBER=${VERSION} $(if $(GIT_REV), (GIT-$(GIT_REV)))') \
	| doxygen -

indent:
	for i in $(OBJS:.o=.c) $(HDRS); do \
		indent $$i; unexpand -a $$i > $$i.up; mv $$i.up $$i; \
	done

clean:
	-rm *.o *~ plugin/*.o plugin/*~

clobber:	clean
	-rm -rf uwm udm doc/html

dist:
	tar cjCf .. uwm-`date +%F-%H`.tar.bz2 \
		$(addprefix uwm/, $(FILES) $(HDRS) $(OBJS:.o=.c))

dist-git:
	git tag v$(VERSION)
	git archive --format=tar --prefix="uwm-$(VERSION)/" v$(VERSION) | \
		gzip > uwm-$(VERSION).tar.gz

DESTDIR=/usr/local

install: all
	mkdir -p $(DESTDIR)/bin
	strip --strip-unneeded -R .comment uwm
	install -s uwm $(DESTDIR)/bin/
	strip --strip-unneeded -R .comment udm
	install -s udm $(DESTDIR)/bin/
