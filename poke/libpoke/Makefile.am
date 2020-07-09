# GNU poke - Makefile.am for libpoke

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

dist_pkgdata_DATA = pkl-rt.pk std.pk

lib_LTLIBRARIES = libpoke.la

include_HEADERS = libpoke.h

libpoke_la_SOURCES = libpoke.h libpoke.c \
                     pk-val.c \
                     pkt.h \
                     pkl.h pkl.c \
                     pkl-ast.h pkl-ast.c \
                     pkl-env.h pkl-env.c \
                     pkl-pass.h pkl-pass.c \
                     pkl-promo.h pkl-promo.c \
                     pkl-fold.h pkl-fold.c \
                     pkl-typify.h pkl-typify.c \
                     pkl-anal.h pkl-anal.c \
                     pkl-trans.h pkl-trans.c \
                     pkl-tab.y pkl-lex.l \
                     pkl-gen.h pkl-gen.c \
                     pkl-asm.h pkl-asm.c \
                     pkl-diag.h pkl-diag.c \
                     pkl-parser.h pkl-parser.c \
                     pkl-gen.pks pkl-asm.pks \
                     pkl-gen.pkc pkl-asm.pkc \
                     pkl-insn.def pkl-ops.def pkl-attrs.def \
                     pvm.h pvm.c \
                     pvm-val.c pvm-val.h \
                     pvm-env.c \
                     pvm-alloc.h pvm-alloc.c \
                     pvm-program.h pvm-program.c \
                     pvm.jitter \
                     ios.c ios.h ios-dev.h \
                     ios-dev-file.c ios-dev-mem.c

libpoke_la_SOURCES += ../common/pk-utils.c ../common/pk-utils.h

if NBD
libpoke_la_SOURCES += ios-dev-nbd.c
endif NBD

.pks.pkc:
	{ srcdir=$(srcdir) $(AWK) -f $(srcdir)/ras $< > $@; } \
          || { rm -f $@ && false; }
pkl-gen.pkc pkl-asm.pkc: $(srcdir)/ras $(srcdir)/pkl-insn.def

# XXX Adding pkl-lex.h shouldn't be necessary?
EXTRA_DIST = pkl-lex.h ras

BUILT_SOURCES = pkl-tab.h pkl-lex.c \
                pkl-gen.pkc pkl-asm.pkc \
                pvm-vm.h pvm-vm1.c pvm-vm2.c

AM_YFLAGS = -d -t --report=state
MOSTLYCLEANFILES += pkl-tab.output

AM_LFLAGS = -d

# Libtool's library version information for libpoke.
# See the libtool documentation, section "Library interface versions".
# Before making a release, use gnulib/build-aux/libtool-next-version.
LTV_CURRENT=0
LTV_REVISION=0
LTV_AGE=0

libpoke_la_CPPFLAGS = -I$(top_builddir)/gl-libpoke -I$(top_srcdir)/gl-libpoke \
                      -I$(top_srcdir)/common \
                      -DPKL_DEBUG \
                      -DPKGDATADIR=\"$(pkgdatadir)\" \
                      -DPKGINFODIR=\"$(infodir)\" \
                      -DLOCALEDIR=\"$(localedir)\"
libpoke_la_CFLAGS = -Wall $(BDW_GC_CFLAGS) $(LIBNBD_CFLAGS)
libpoke_la_LIBADD = ../gl-libpoke/libgnu.la libpvmjitter.la \
                    $(BDW_GC_LIBS) \
                    $(LIBNBD_LIBS)
libpoke_la_LDFLAGS = -version-info $(LTV_CURRENT):$(LTV_REVISION):$(LTV_AGE)

# Integration with jitter.

noinst_LTLIBRARIES = libpvmjitter.la
libpvmjitter_la_SOURCES = pvm-vm.h pvm-vm1.c pvm-vm2.c
libpvmjitter_la_CPPFLAGS = $(libpoke_la_CPPFLAGS) $(JITTER_CPPFLAGS)
libpvmjitter_la_CFLAGS = $(libpoke_la_CFLAGS) $(JITTER_CFLAGS)
libpvmjitter_la_LDFLAGS = $(JITTER_LDFLAGS)
libpvmjitter_la_LIBADD = $(JITTER_LIBADD)

libpoke_la_CPPFLAGS += $(JITTER_CPPFLAGS)

BUILT_SOURCES += pvm-vm.h pvm-vm1.c pvm-vm2.c

pvm-vm.h pvm-vm1.c pvm-vm2.c: pvm.jitter
	$(JITTER) $(srcdir)/pvm.jitter --output $(builddir)

# End of Makefile.am
