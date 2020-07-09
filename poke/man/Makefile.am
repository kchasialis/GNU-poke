# Makefile.am for poke/man

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

man_MANS = poke.1

EXTRA_DIST = $(man_MANS)
MAINTAINERCLEANFILES = $(man_MANS)

# The man pages depend on the version number.
common_mandeps = $(top_srcdir)/configure.ac

# Depend on the source, not the binary; we don't need to regenerate
# the binary when any source file changes, only the main one.
# Use -o so that the `missing' program can infer the output file.

poke.1: $(top_srcdir)/poke/poke.c $(common_mandeps)
	$(MAKE) $(AM_MAKEFLAGS) $(builddir)/../poke/poke$(EXEEXT)
	$(top_builddir)/run \
           $(HELP2MAN) -p poke --name="The GNU extensible binary editor" \
		$(builddir)/../poke/poke$(EXEEXT) -o $@
