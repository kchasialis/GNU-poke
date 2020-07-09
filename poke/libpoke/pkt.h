/* pkt.h - Terminal utilities for libpoke.  */

/* Copyright (C) 2020 Jose E. Marchesi */

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

#ifndef PKT_H
#define PKT_H

#include <config.h>

#include "libpoke.h"  /* For struct pk_term_if */

extern struct pk_term_if libpoke_term_if;

#define pk_puts libpoke_term_if.puts_fn
#define pk_printf libpoke_term_if.printf_fn
#define pk_term_flush libpoke_term_if.flush_fn
#define pk_term_indent libpoke_term_if.indent_fn
#define pk_term_class libpoke_term_if.class_fn
#define pk_term_end_class libpoke_term_if.end_class_fn
#define pk_term_hyperlink libpoke_term_if.hyperlink_fn
#define pk_term_end_hyperlink libpoke_term_if.end_hyperlink_fn

#endif /* ! PKT_H */
