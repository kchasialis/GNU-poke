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


dist_pkgdata_DATA = pkl-rt.pk std.pk \
                    pk-cmd.pk pk-dump.pk pk-save.pk pk-copy.pk

bin_PROGRAMS = poke
poke_SOURCES = poke.c poke.h \
               ios.c ios.h ios-dev.h \
               ios-dev-file.c ios-dev-mem.c \
               pk-term.c pk-term.h \
               pk-repl.c pk-repl.h \
               pk-cmd.c pk-cmd.h \
               pk-utils.h pk-utils.c \
               pk-ios.c \
               pk-info.c pk-misc.c pk-help.c pk-vm.c \
               pk-def.c pk-set.c pk-editor.c \
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
               pkl-parser.h pkl-parser.c \
               pvm.h pvm.c \
               pvm-alloc.h pvm-alloc.c \
               pvm-val.h pvm-val.c \
               pvm-env.h pvm-env.c \
               pvm.jitter \
               pkl-gen.pks pkl-asm.pks \
               pkl-gen.pkc pkl-asm.pkc \
               pkl-insn.def pkl-ops.def pkl-attrs.def

if NBD
poke_SOURCES += ios-dev-nbd.c
endif NBD

if HSERVER
  poke_SOURCES += pk-hserver.h pk-hserver.c
endif

.pks.pkc:
	srcdir=$(srcdir) $(AWK) -f $(srcdir)/ras $< > $@

pkl-gen.pkc pkl-asm.pkc: $(srcdir)/ras $(srcdir)/pkl-insn.def

# XXX this shouldn't be necessary?
EXTRA_DIST = pkl-lex.h ras

BUILT_SOURCES = pkl-tab.h pkl-lex.c \
                pkl-gen.pkc pkl-asm.pkc \
                pvm-vm.h pvm-vm1.c pvm-vm2.c
AM_YFLAGS = -d -t --report=state
AM_LFLAGS = -d

poke_CPPFLAGS = -I$(top_builddir)/gl -I$(top_srcdir)/gl \
                -DPKL_DEBUG \
                -DPKGDATADIR=\"$(pkgdatadir)\" \
                -DPKGINFODIR=\"$(infodir)\" \
                -DJITTER_VERSION=\"$(JITTER_VERSION)\" \
                -DLOCALEDIR=\"$(localedir)\"
poke_CFLAGS = -Wall $(BDW_GC_CFLAGS) $(LIBNBD_CFLAGS)
poke_LDADD = $(top_builddir)/gl/libgnu.la \
             libpvmjitter.a \
             $(LIBNBD_LIBS) \
             $(LTLIBREADLINE) $(BDW_GC_LIBS) $(LIBTEXTSTYLE)
poke_LDFLAGS =

if HSERVER
  poke_LDADD += -lpthread
endif


if HJSON
  poke_CFLAGS += $(JSONC_CFLAGS)
  poke_LDADD += $(JSONC_LIBS)
endif

# Integration with jitter.

noinst_LIBRARIES = libpvmjitter.a
libpvmjitter_a_SOURCES = pvm-vm.h pvm-vm1.c pvm-vm2.c
libpvmjitter_a_CPPFLAGS = $(poke_CPPFLAGS)
libpvmjitter_a_CFLAGS = $(poke_CFLAGS) $(JITTER_CFLAGS)

poke_CPPFLAGS += $(JITTER_CPPFLAGS)
poke_LDADD += $(JITTER_LDADD)
poke_LDFLAGS += $(JITTER_LDFLAGS)

BUILT_SOURCES += pvm-vm.h pvm-vm1.c pvm-vm2.c

pvm-vm.h pvm-vm1.c pvm-vm2.c: pvm.jitter
	$(JITTER) $(srcdir)/pvm.jitter --output $(builddir)

# End of Makefile.am
