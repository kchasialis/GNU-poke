/* poke.h - Interactive editor for binary files.  */

/* Copyright (C) 2019, 2020 Jose E. Marchesi */

/* This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef POKE_H
#define POKE_H

#include <config.h>

#include <stdlib.h> /* EXIT_FAILURE */
#include <gettext.h>
#define _(str) dgettext (PACKAGE, str)

#include "pk-term.h"
#include "libpoke.h"

extern int poke_interactive_p;
extern int poke_quiet_p;
extern int poke_exit_p;
#if HAVE_HSERVER
extern int poke_hserver_p;
#endif
extern int poke_exit_code;
extern pk_compiler poke_compiler;
extern char *poke_datadir;
extern char *poke_infodir;
extern char *poke_picklesdir;
extern char *poke_mapsdir;
extern char *poke_docdir;
extern char *poke_cmdsdir;
extern char *poke_doc_viewer;
extern int poke_auto_map_p;
extern int poke_prompt_maps_p;

void pk_print_version (void);
void pk_fatal (const char *errmsg) __attribute__ ((noreturn));

static inline void
pk_assert_alloc (const void *m)
{
  if (!m)
    {
      pk_printf (_("out of memory\n"));
      exit (EXIT_FAILURE);
    }
}

#endif /* !POKE_H */
