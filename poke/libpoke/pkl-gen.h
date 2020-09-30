/* pkl-gen.h - Code generation pass for the poke compiler.  */

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

#ifndef PKL_GEN_H
#define PKL_GEN_H

#include <config.h>

#include <stdlib.h>
#include <string.h>

#include "pkl.h"
#include "pkl-ast.h"
#include "pkl-pass.h"
#include "pkl-asm.h"
#include "pkl-asm.h"
#include "pvm.h"

/* It would be very unlikely to have more than 24 declarations nested
   in a poke program... if it ever happens, we will just increase
   this, thats a promise :P (don't worry, there is an assertion that
   will fire if this limit is surpassed.) */

#define PKL_GEN_MAX_PASM 25

/* The following struct defines the payload of the code generation
   phase.

   COMPILER is the Pkl compiler driving the compilation.

   PASM and PASM2 are stacks of macro-assemblers.  Assemblers in PASM
   are used for assembling the main program, struct mappers, and
   functions.  Assemblers in PASM2 are used for compiling struct
   constructors.

   CUR_PASM and CUR_PASM2 are the pointers to the top of PASM and
   PASM2, respectively.

   PROGRAM is the main PVM program being compiled.  When the phase is
   completed, the program is "finished" (in PVM parlance) and ready
   to be used.

   IN_STRUCT_DECL is 1 when a struct declaration is being generated.
   0 otherwise.

   IN_MAPPER is 1 when a mapper function for an array or a struct type
   is being generated.  0 otherwise.

   IN_WRITER is 1 when a writer function for an array or a struct type
   is begin generated.  0 otherwise.  This context is also used for
   struct integrators.

   IN_CONSTRUCTOR is 1 when a struct constructor is being generated.
   0 otherwise.

   IN_COMPARATOR is 1 when a struct comparator is being generated.
   0 otherwise.

   IN_LVALUE is 1 in a sub-tree corresponding to the l-value of an
   assignment statement.  0 otherwise.

   IN_ARRAY_BOUNDER is 1 when an array bounder function is being
   generated.  0 otherwise.

   ENDIAN is the endianness to be used when mapping and writing
   integral types.

   MAPPER_DEPTH and CONSTRUCTOR_DEPTH are used in the array mapper and
   constructor generation handlers.  */

struct pkl_gen_payload
{
  pkl_compiler compiler;
  pkl_asm pasm[PKL_GEN_MAX_PASM];
  pkl_asm pasm2[PKL_GEN_MAX_PASM];
  int cur_pasm;
  int cur_pasm2;
  pvm_program program;
  int in_struct_decl;
  int in_mapper;
  int in_constructor;
  int in_writer;
  int in_valmapper;
  int in_lvalue;
  int in_comparator;
  int in_array_bounder;
  int in_method_arguments;
  int in_funcall;
  int generating_pvm_struct_type;
  int endian;
  int constructor_depth;
  int mapper_depth;
};

typedef struct pkl_gen_payload *pkl_gen_payload;

extern struct pkl_phase pkl_phase_gen;

static inline void
pkl_gen_init_payload (pkl_gen_payload payload, pkl_compiler compiler)
{
  memset (payload, 0, sizeof (struct pkl_gen_payload));
  payload->compiler = compiler;
}


#endif /* !PKL_GEN_H  */
