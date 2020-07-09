/* pvm-alloc.h - Memory allocator for the PVM.  */

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

#ifndef PVM_ALLOC_H
#define PVM_ALLOC_H

#include <config.h>
#include <gc.h>

/* This file provides memory allocation services to the PVM code.  */

/* Functions to initialize and finalize the allocator, respectively.
   At finalization time all allocated memory is fred.  No pvm_alloc_*
   services shall be used once finalized, unless pvm_alloc_init is
   invoked again.  */

void pvm_alloc_initialize (void)
    __attribute__ ((visibility ("hidden")));
void pvm_alloc_finalize (void)
    __attribute__ ((visibility ("hidden")));

/* Register/unregister NELEM pointers at POINTER as roots for the
   garbage-collector.  */

void pvm_alloc_add_gc_roots (void *pointer, size_t nelems)
  __attribute__ ((visibility ("hidden")));
void pvm_alloc_remove_gc_roots (void *pointer, size_t nelems)
  __attribute__ ((visibility ("hidden")));

/* Allocate SIZE bytes and return a pointer to the allocated memory.
   SIZE has the same semantics as in malloc(3).  On error, return
   NULL.  */

void *pvm_alloc (size_t size)
  __attribute__ ((malloc))
  __attribute__ ((alloc_size (1)))
  __attribute__ ((visibility ("hidden")));

/* Reallocate the given pointer to occupy SIZE bytes and return a
   pointer to the allocated memory.  SIZE has the same semantics as in
   realloc(3).  On error, return NULL.  */

void *pvm_realloc (void *ptr, size_t size)
  __attribute__ ((malloc))
  __attribute__ ((alloc_size (2)))
  __attribute__ ((visibility ("hidden")));

/* Allocate a pvm_cls struct and return a pointer to the allocated
   memory.  This type-specific allocator is needed because the GC
   needs additional information to free these structs.  */

void *pvm_alloc_cls (void)
  __attribute__ ((malloc))
  __attribute__ ((visibility ("hidden")));

/* Allocate and return a copy of the given STRING.  This call has the
   same semantics than strdup(3).  */

char *pvm_alloc_strdup (const char *string)
  __attribute__ ((malloc))
  __attribute__ ((visibility ("hidden")));

/* Forced collection.  */

void pvm_alloc_gc (void)
  __attribute__ ((visibility ("hidden")));

#endif /* ! PVM_ALLOC_H */
