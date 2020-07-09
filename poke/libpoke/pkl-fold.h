/* pkl-fold.h - Constant folding phase for the poke compiler. */

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

#ifndef PKL_FOLD_H
#define PKL_FOLD_H

#include <config.h>

struct pkl_fold_payload
{
  int errors;
};

typedef struct pkl_fold_payload *pkl_fold_payload;

extern struct pkl_phase pkl_phase_fold;

#endif /* PKL_FOLD_H */
