# Copyright (C) 2019, 2020 Jose E. Marchesi
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3, or (at your option)
# any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

EXTRA_DIST = config lib poke.cmd poke.color poke.id3v1 poke.map poke.pkl \
             poke.repl poke.rgb24 poke.std

AUTOMAKE_OPTIONS = dejagnu

check-DEJAGNU: site.exp
	@srcdir=`cd $(srcdir) && pwd`; export srcdir; \
	r=`pwd`; export r; \
	LC_ALL=C; export LC_ALL; \
	EXPECT=$(EXPECT); export EXPECT; \
	runtest=$(RUNTEST); \
	if $(SHELL) -c "$$runtest --version" > /dev/null 2>&1; then \
	  CC_FOR_TARGET="$(CC_FOR_TARGET)" CFLAGS_FOR_TARGET="$(CFLAGS)" \
	  HAVE_LIBTEXTSTYLE="$(HAVE_LIBTEXTSTYLE)" \
	  NBDKIT="$(NBDKIT)" \
          POKESTYLESDIR="$(top_srcdir)/etc" \
          POKEPICKLESDIR="$(top_srcdir)/pickles" \
          POKEDATADIR="$(top_srcdir)/libpoke" \
          POKECMDSDIR="$(top_srcdir)/poke" \
          POKEDOCDIR="$(top_builddir)/doc" \
          POKE_LOAD_PATH="$(top_srcdir)/poke" \
		$$runtest --tool $(DEJATOOL) --srcdir $${srcdir} --objdir $(builddir) \
                        SHELL="$(SHELL)" \
			$(RUNTESTFLAGS); \
	else \
	  >&2 echo "WARNING: could not find \`runtest'"; \
	  exit 77; :;\
	fi
