# GNU poke - GUI

# Copyright (C) 2020 Jose E. Marchesi

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

MOSTLYCLEANFILES =
CLEANFILES =
DISTCLEANFILES =
MAINTAINERCLEANFILES =

if GUI

guifilesdir = $(pkgdatadir)/gui

dist_guifiles_DATA = pk-main.tcl pk-gui.tcl pk-mi.tcl pk-mi-msg.tcl

bin_PROGRAMS = poke-gui

poke_gui_SOURCES = poke-gui.c

poke_gui_CPPFLAGS = -I$(top_builddir)/common -I$(top_srcdir)/gl-gui \
                    $(TCL_INCLUDE_SPEC) $(TK_INCLUDE_SPEC) \
                    -DPOKEGUIDIR=\"$(guifilesdir)\" \
                    -DLOCALEDIR=\"$(localedir)\"
poke_gui_CFLAGS = -Wall
poke_gui_LDADD = $(TCL_LIB_SPEC) $(TK_LIB_SPEC) \
                 $(top_builddir)/gl-gui/libgnu.la

endif
