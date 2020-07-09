/* pvm.h - Poke Virtual Machine.  */

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

#include <config.h>

#include <string.h>
#include <assert.h>
#include <signal.h>

#include "pkl.h"
#include "pvm.h"

#include "pvm-alloc.h"
#include "pvm-program.h"
#include "pvm-vm.h"

/* The following struct defines a Poke Virtual Machine.  */

#define PVM_STATE_RESULT_VALUE(PVM)                     \
  ((PVM)->pvm_state.pvm_state_backing.result_value)
#define PVM_STATE_EXIT_CODE(PVM)                        \
  ((PVM)->pvm_state.pvm_state_backing.exit_code)
#define PVM_STATE_VM(PVM)                               \
  ((PVM)->pvm_state.pvm_state_backing.vm)
#define PVM_STATE_ENV(PVM)                              \
  ((PVM)->pvm_state.pvm_state_runtime.env)
#define PVM_STATE_ENDIAN(PVM)                           \
  ((PVM)->pvm_state.pvm_state_runtime.endian)
#define PVM_STATE_NENC(PVM)                             \
  ((PVM)->pvm_state.pvm_state_runtime.nenc)
#define PVM_STATE_PRETTY_PRINT(PVM)                     \
  ((PVM)->pvm_state.pvm_state_runtime.pretty_print)
#define PVM_STATE_OMODE(PVM)                            \
  ((PVM)->pvm_state.pvm_state_runtime.omode)
#define PVM_STATE_OBASE(PVM)                            \
  ((PVM)->pvm_state.pvm_state_runtime.obase)
#define PVM_STATE_OMAPS(PVM)                            \
  ((PVM)->pvm_state.pvm_state_runtime.omaps)
#define PVM_STATE_ODEPTH(PVM)                           \
  ((PVM)->pvm_state.pvm_state_runtime.odepth)
#define PVM_STATE_OINDENT(PVM)                          \
  ((PVM)->pvm_state.pvm_state_runtime.oindent)
#define PVM_STATE_OACUTOFF(PVM)                         \
  ((PVM)->pvm_state.pvm_state_runtime.oacutoff)

struct pvm
{
  /* Note that the contents of the struct pvm_state are defined in the
     state-struct-backing-c and state-struct-runtime-c entries in
     pvm.jitter.  */
  struct pvm_state pvm_state;

  /* If not NULL, this is the compiler to be used when the PVM needs
     to build programs.  */
  pkl_compiler compiler;
};

pvm
pvm_init (void)
{
  pvm apvm = calloc (1, sizeof (struct pvm));
  if (!apvm)
    return NULL;

  /* Initialize the memory allocation subsystem.  */
  pvm_alloc_initialize ();

  /* Initialize the VM subsystem.  */
  pvm_initialize ();

  /* Initialize the VM state.  */
  pvm_state_initialize (&apvm->pvm_state);

  /* Register GC roots.  */
  pvm_alloc_add_gc_roots (&PVM_STATE_ENV (apvm), 1);
  pvm_alloc_add_gc_roots
    (apvm->pvm_state.pvm_state_backing.jitter_stack_stack_backing.memory,
     apvm->pvm_state.pvm_state_backing.jitter_stack_stack_backing.element_no);
  pvm_alloc_add_gc_roots
    (apvm->pvm_state.pvm_state_backing.jitter_stack_returnstack_backing.memory,
     apvm->pvm_state.pvm_state_backing.jitter_stack_returnstack_backing.element_no);
  pvm_alloc_add_gc_roots
    (apvm->pvm_state.pvm_state_backing.jitter_stack_exceptionstack_backing.memory,
     apvm->pvm_state.pvm_state_backing.jitter_stack_exceptionstack_backing.element_no);

  /* Initialize the global environment.  Note we do this after
     registering GC roots, since we are allocating memory.  */
  PVM_STATE_ENV (apvm) = pvm_env_new (0 /* hint */);
  PVM_STATE_VM (apvm) = apvm;

  return apvm;
}

pvm_env
pvm_get_env (pvm apvm)
{
  return PVM_STATE_ENV (apvm);
}

enum pvm_exit_code
pvm_run (pvm apvm, pvm_program program, pvm_val *res)
{
  sighandler_t previous_handler;
  pvm_routine routine = pvm_program_routine (program);

  PVM_STATE_RESULT_VALUE (apvm) = PVM_NULL;
  PVM_STATE_EXIT_CODE (apvm) = PVM_EXIT_OK;

  previous_handler = signal (SIGINT, pvm_handle_signal);
  pvm_execute_routine (routine, &apvm->pvm_state);
  signal (SIGINT, previous_handler);

  if (res != NULL)
    *res = PVM_STATE_RESULT_VALUE (apvm);

  return PVM_STATE_EXIT_CODE (apvm);
}

void
pvm_shutdown (pvm apvm)
{
  /* Deregister GC roots.  */
  pvm_alloc_remove_gc_roots (&PVM_STATE_ENV (apvm), 1);
  pvm_alloc_remove_gc_roots
    (apvm->pvm_state.pvm_state_backing.jitter_stack_stack_backing.memory,
     apvm->pvm_state.pvm_state_backing.jitter_stack_stack_backing.element_no);
  pvm_alloc_remove_gc_roots
    (apvm->pvm_state.pvm_state_backing.jitter_stack_returnstack_backing.memory,
     apvm->pvm_state.pvm_state_backing.jitter_stack_returnstack_backing.element_no);
  pvm_alloc_remove_gc_roots
    (apvm->pvm_state.pvm_state_backing.jitter_stack_exceptionstack_backing.memory,
     apvm->pvm_state.pvm_state_backing.jitter_stack_exceptionstack_backing.element_no);

  /* Finalize the VM state.  */
  pvm_state_finalize (&apvm->pvm_state);

  /* Finalize the VM subsystem.  */
  pvm_finalize ();

  free (apvm);

  /* Finalize the memory allocator.  */
  pvm_alloc_finalize ();
}

enum ios_endian
pvm_endian (pvm apvm)
{
  return PVM_STATE_ENDIAN (apvm);
}

void
pvm_set_endian (pvm apvm, enum ios_endian endian)
{
  PVM_STATE_ENDIAN (apvm) = endian;
}

enum ios_nenc
pvm_nenc (pvm apvm)
{
  return PVM_STATE_NENC (apvm);
}

void
pvm_set_nenc (pvm apvm, enum ios_nenc nenc)
{
  PVM_STATE_NENC (apvm) = nenc;
}

int
pvm_pretty_print (pvm apvm)
{
  return PVM_STATE_PRETTY_PRINT (apvm);
}

void
pvm_set_pretty_print (pvm apvm, int flag)
{
  PVM_STATE_PRETTY_PRINT (apvm) = flag;
}

enum pvm_omode
pvm_omode (pvm apvm)
{
  return PVM_STATE_OMODE (apvm);
}

void
pvm_set_omode (pvm apvm, enum pvm_omode omode)
{
  PVM_STATE_OMODE (apvm) = omode;
}

int
pvm_obase (pvm apvm)
{
  return PVM_STATE_OBASE (apvm);
}

void
pvm_set_obase (pvm apvm, int obase)
{
  PVM_STATE_OBASE (apvm) = obase;
}

int
pvm_omaps (pvm apvm)
{
  return PVM_STATE_OMAPS (apvm);
}

void
pvm_set_omaps (pvm apvm, int omaps)
{
  PVM_STATE_OMAPS (apvm) = omaps;
}

unsigned int
pvm_oindent (pvm apvm)
{
  return PVM_STATE_OINDENT (apvm);
}

void
pvm_set_oindent (pvm apvm, unsigned int oindent)
{
  PVM_STATE_OINDENT (apvm) = oindent;
}

unsigned int
pvm_odepth (pvm apvm)
{
  return PVM_STATE_ODEPTH (apvm);
}

void
pvm_set_odepth (pvm apvm, unsigned int odepth)
{
  PVM_STATE_ODEPTH (apvm) = odepth;
}

unsigned int
pvm_oacutoff (pvm apvm)
{
  return PVM_STATE_OACUTOFF (apvm);
}

void
pvm_set_oacutoff (pvm apvm, unsigned int cutoff)
{
  PVM_STATE_OACUTOFF (apvm) = cutoff;
}

pkl_compiler
pvm_compiler (pvm apvm)
{
  return apvm->compiler;
}

void
pvm_set_compiler (pvm apvm, pkl_compiler compiler)
{
  apvm->compiler = compiler;
}

void
pvm_assert (int expression)
{
  assert (expression);
}
