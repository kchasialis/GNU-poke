# GNU poke

# Copyright (C) 2019, 2020 Jose E. Marchesi

# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

AUTOMAKE_OPTIONS = subdir-objects

MOSTLYCLEANFILES =
CLEANFILES =
DISTCLEANFILES =
MAINTAINERCLEANFILES =

dist_pkgdata_DATA = pk-cmd.pk pk-dump.pk pk-save.pk pk-copy.pk \
                    pk-extract.pk poke.pk

bin_PROGRAMS = poke
poke_SOURCES = poke.c poke.h \
               pk-term.c pk-term.h \
               pk-repl.c pk-repl.h \
               pk-cmd.c pk-cmd.h \
               pk-cmd-ios.c pk-cmd-info.c pk-cmd-misc.c \
               pk-cmd-help.c pk-cmd-def.c pk-cmd-vm.c \
               pk-cmd-set.c pk-cmd-editor.c pk-cmd-map.c \
               pk-ios.c pk-ios.h \
               pk-map.c pk-map.h pk-map-parser.h \
               pk-map-tab.y pk-map-lex.l

poke_SOURCES += ../common/pk-utils.c ../common/pk-utils.h

poke_CPPFLAGS = -I$(top_builddir)/gl -I$(top_srcdir)/gl \
                -I$(top_srcdir)/common \
                -I$(top_srcdir)/libpoke -I$(top_builddir)/libpoke \
                -DJITTER_VERSION=\"$(JITTER_VERSION)\" \
                -DPKGDATADIR=\"$(pkgdatadir)\" \
                -DPKGINFODIR=\"$(infodir)\" \
                -DLOCALEDIR=\"$(localedir)\"
poke_CFLAGS = -Wall
poke_LDADD = $(top_builddir)/gl/libgnu.la \
             $(top_builddir)/libpoke/libpoke.la \
             $(LTLIBREADLINE) \
             $(LTLIBTEXTSTYLE)
poke_LDFLAGS =

AM_YFLAGS = -d -t --report=state
AM_LFLAGS = -d

EXTRA_DIST = pk-map-lex.h

BUILT_SOURCES = pk-map-tab.h pk-map-lex.c

# Machine interface
if POKE_MI
poke_SOURCES += pk-mi.c pk-mi.h \
                pk-mi-msg.c pk-mi-msg.h \
                pk-mi-json.c pk-mi-json.h
poke_CFLAGS += $(JSON_C_CFLAGS)
poke_LDADD += $(JSON_C_LIBS)
endif

if HSERVER
  poke_SOURCES += pk-hserver.h pk-hserver.c
  poke_LDADD += -lpthread
endif

# Created by doc/Makefile.am.
MOSTLYCLEANFILES += nodelist

# End of Makefile.am
