# Generated automatically from Makefile.in by configure.
#    Makefile template for Configure for the arm sim library.
#    Copyright (C) 1995, 1996, 1997 Free Software Foundation, Inc.
#    Written by Cygnus Support.
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

## COMMON_PRE_CONFIG_FRAG
## COMMON_PRE_CONFIG_FRAG

srcdir = .
prefix = /usr
exec_prefix = ${prefix}
bindir = ${exec_prefix}/bin

libdir = ${exec_prefix}/lib
tooldir = $(libdir)/$(target_alias)

datadir = ${prefix}/share
mandir = ${prefix}/man
man1dir = $(mandir)/man1
infodir = ${prefix}/info
includedir = ${prefix}/include

SHELL = /bin/sh

INSTALL = /usr/bin/ginstall -c
INSTALL_PROGRAM = ${INSTALL}
INSTALL_DATA = ${INSTALL} -m 644

CC = gcc
CC_FOR_BUILD = $(CC)
CFLAGS = -g -Og -ggdb -m32 -I/usr/X11R6/include -DDEFAULT_INLINE=0

AR = ar
AR_FLAGS = rc
RANLIB = ranlib
MAKEINFO = makeinfo

DEP = $(srcroot)/mkdep

OBJS = \
	armemu32.o \
	armcopro.o \
	arminit.o \
	armsupp.o \
	armvirt.o \
	bag.o \
	armmmu.o \
	armmem.o \
	armio.o \
	armlcd.o \
	psion.o 


LIBS = -lnsl  -L/usr/lib -L/usr/X11R6/lib -rdynamic -ldl -lXext -lX11 -lm
LDFLAGS = -ggdb -m32

all: psimulator

psimulator: $(OBJS)
	$(CC) $(CFLAGS) -o psimulator $(OBJS) $(LIBS) $(LDFLAGS)

# Support targets.

install: install-common

install-common: installdirs psimulator
	$(INSTALL_PROGRAM) psimulator$(EXEEXT) $(bindir)/psimulator$(EXEEXT)

installdirs:
	$(SHELL) mkinstalldirs $(bindir)

info:
clean-info:
install-info:

.NOEXPORT:
MAKEOVERRIDES=

tags etags: TAGS

# Macros like EXTERN_SIM_CORE confuse tags.
# And the sim-n-foo.h files create functions that can't be found either.
TAGS: force
	cd $(srcdir) && \
	etags --regex '/^\([a-z_]+\) (/\1/' --regex '/^\/[*] TAGS: .*/' \
		*.[ch] ../common/*.[ch]

clean:
	rm -f *.[oa] *~ core

distclean mostlyclean maintainer-clean realclean: clean
	rm -f TAGS
	rm -f Makefile config.cache config.log config.status .gdbinit
	rm -f tconfig.h config.h stamp-h
	rm -f targ-vals.def

.c.o:
	$(CC) -c $(CFLAGS) $<

# Dummy target to force execution of dependent targets.
#force:

#Makefile: Makefile.in config.status
#	CONFIG_HEADERS= $(SHELL) ./config.status

#config.status: configure
#	$(SHELL) ./config.status --recheck

#config.h: stamp-h ; @true
#stamp-h: config.in config.status
#	CONFIG_FILES= CONFIG_HEADERS=config.h:config.in $(SHELL) ./config.status

#.gdbinit: # config.status $(srccom)/gdbinit.in
#	CONFIG_FILES=$@:../common/gdbinit.in CONFIG_HEADERS= $(SHELL) ./config.status


## End COMMON_POST_CONFIG_FRAG


armcopro.o: armcopro.c armdefs.h armmmu.h armmem.h armio.h armlcd.h 

armemu26.o: armemu.c armdefs.h armmmu.h armmem.h armio.h armlcd.h armemu.h 
	$(CC) -c $< -o armemu26.o $(CFLAGS)

armemu32.o: armemu.c armdefs.h armmmu.h armmem.h armio.h armlcd.h armemu.h
	$(CC) -c $< -o armemu32.o -DMODE32 $(CFLAGS)

arminit.o: arminit.c armdefs.h armmmu.h armmem.h armio.h armlcd.h armemu.h

armrdi.o: armrdi.c armdefs.h armmmu.h armmem.h armio.h armlcd.h armemu.h armos.h dbg_cp.h dbg_conf.h dbg_rdi.h \
	dbg_hif.h communicate.h

armsupp.o: armsupp.c armdefs.h armmmu.h armmem.h armio.h armlcd.h armemu.h

bag.o: bag.c bag.h

armio.o:	armio.c armdefs.h armmmu.h armmem.h armio.h armlcd.h 
armmem.o:	armmem.c armdefs.h armmmu.h armmem.h armio.h armlcd.h 
armmmu.o:	armmmu.c armdefs.h armmmu.h armmem.h armio.h armlcd.h 
armvirt.o:	armvirt.c armdefs.h armmmu.h armmem.h armio.h armlcd.h 
wrapper.o:	wrapper.c armdefs.h armmmu.h armmem.h armio.h armlcd.h 

