/* pkl-anal.h - Analysis phases for the poke compiler.  */

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

#ifndef PKL_ANAL_H
#define PKL_ANAL_H

#include <config.h>
#include "pkl-pass.h"

/* The following struct defines the payload of the analysis phases.

   ERRORS is the number of errors detected while running the phase.

   CONTEXT is a stack of mutually-exclusive contexts, that encode
   context information for some of the handlers.

   If NEXT_CONTEXT is 0 then we are not in any particular context
   (NO_CONTEXT) otherwise NEXT_CONTEXT - 1 is the index of the current
   context in the array CONTEXT.  */

#define PKL_ANAL_MAX_CONTEXT_NEST 32

#define PKL_ANAL_NO_CONTEXT 0
#define PKL_ANAL_CONTEXT_STRUCT_TYPE 1
#define PKL_ANAL_CONTEXT_METHOD 2

struct pkl_anal_payload
{
  int errors;
  int context[PKL_ANAL_MAX_CONTEXT_NEST];
  int next_context;
};

typedef struct pkl_anal_payload *pkl_anal_payload;

extern struct pkl_phase pkl_phase_anal1;
extern struct pkl_phase pkl_phase_anal2;
extern struct pkl_phase pkl_phase_analf;

static inline void
pkl_anal_init_payload (pkl_anal_payload payload)
{
  memset (payload, 0, sizeof (struct pkl_anal_payload));
}

#endif /* PKL_ANAL_H */
