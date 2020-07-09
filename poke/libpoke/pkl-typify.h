/* pkl-typify.c - Type handling phases for the poke compiler.  */

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

#ifndef PKL_TYPIFY_H
#define PKL_TYPIFY_H

#include <config.h>
#include <pkl-pass.h>

struct pkl_typify_payload
{
  int errors;
};

typedef struct pkl_typify_payload *pkl_typify_payload;

extern struct pkl_phase pkl_phase_typify1;
extern struct pkl_phase pkl_phase_typify2;

#endif /* PKL_TYPIFY_H */
