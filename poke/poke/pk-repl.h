/* pk-repl.h - A REPL ui for poke.  */

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

#ifndef PK_REPL_H
#define PK_REPL_H

#include <config.h>

#include "poke.h" /* For poke_quiet_p  */

/* Enter the REPL!  :) */

void pk_repl (void);

/* Display stuff before the REPL line currently being edited, which is
   preserved.  */

void pk_repl_display_begin (void);
void pk_repl_display_end (void);

/* Insert a string at the current cursor position in the line being
   edited.  */

void pk_repl_insert (const char *str);

#endif /* ! PK_REPL_H */
