/* pvm-val.c - Memory allocator for the PVM.  */

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
#include <gc/gc.h>

#include "pvm.h"
#include "pvm-val.h"

void *
pvm_alloc (size_t size)
{
  return GC_MALLOC (size);
}

void *
pvm_realloc (void *ptr, size_t size)
{
  return GC_REALLOC (ptr, size);
}

char *
pvm_alloc_strdup (const char *string)
{
  return GC_strdup (string);
}

static void
pvm_alloc_finalize_closure (void *object, void *client_data)
{
  /* XXX this causes a crash because of a cycle in the finalizers:
     routines of recursive PVM programs contain a reference to
     themselves, be it directly or indirectly.  */
  /* pvm_cls cls = (pvm_cls) object; */
  /*  pvm_destroy_program (PVM_VAL_CLS_PROGRAM (cls)); */
}

void *
pvm_alloc_cls (void)
{
  pvm_cls cls = pvm_alloc (sizeof (struct pvm_cls));

  GC_register_finalizer_no_order (cls, pvm_alloc_finalize_closure, NULL,
                                  NULL, NULL);
  return cls;
}

void
pvm_alloc_initialize ()
{
  /* Initialize the Boehm Garbage Collector.  */
  GC_INIT ();
}

void
pvm_alloc_finalize ()
{
  GC_gcollect ();
}

void
pvm_alloc_add_gc_roots (void *pointer, size_t nelems)
{
  GC_add_roots (pointer,
                ((char*) pointer) + sizeof (void*) * nelems);
}

void
pvm_alloc_remove_gc_roots (void *pointer, size_t nelems)
{
  GC_remove_roots (pointer,
                   ((char*) pointer) + sizeof (void*) * nelems);
}
