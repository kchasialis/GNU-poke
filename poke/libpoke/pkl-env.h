/* pkl-env.h - Compile-time lexical environments for Poke.  */

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

#ifndef PKL_ENV_H
#define PKL_ENV_H

#include <config.h>

#include "pkl.h"

/* The poke compiler maintains a data structure called the
   compile-time environment.  This structure keeps track of which
   variables will be at which position in which frames in the run-time
   environment when a particular variable-access operation is
   executed.  Conceptually, the compile-time environment is a list of
   "frames", each containing a list of declarations of variables,
   types and functions.

   The purpose of building this data structure is twofold:
   - When the parser finds a name, its meaning (particularly its type)
     can be found by searching the environment from the current frame
     out to the global one.

   - To aid in the determination of lexical addresses in variable
     references and assignments.  Lexical addresses are known
     at-compile time, and avoid the need of performing expensive
     lookups at run-time.

     The compile-time environment effectively mimics the corresponding
     run-time environments that will happen at run-time when a given
     lambda is created.

     For more details on this technique, see the Wizard Book (SICP)
     section 3.2, "The Environment model of Evaluation".  */

/* An environment consists on a stack of frames, each frame containing
   a set of declarations, which in effect are PKL_AST_DECL nodes.

   There are no values bound to the entities being declared, as values
   are not generally available at compile-time.  However, the type
   information is always available at compile-time.  */

typedef struct pkl_env *pkl_env;  /* Struct defined in pkl-env.c */

/* Get an empty environment.  */

pkl_env pkl_env_new (void)
  __attribute__ ((visibility ("hidden")));

/* Destroy ENV, freeing all resources.  */

void pkl_env_free (pkl_env env)
  __attribute__ ((visibility ("hidden")));

/* Push a new frame to ENV and return the modified environment.  The
   new frame is empty.  */

pkl_env pkl_env_push_frame (pkl_env env)
  __attribute__ ((visibility ("hidden")));

/* Pop a frame from ENV and return the modified environment.  The
   contents of the popped frame are disposed.  Trying to pop the
   top-level frame is an error.  */

pkl_env pkl_env_pop_frame (pkl_env env)
  __attribute__ ((visibility ("hidden")));

/* Register the declaration DECL in the current frame under NAME in
   the given NAMESPACE.  Return 1 if the declaration was properly
   registered.  Return 0 if there is already a declaration with the
   given name in the current frame and namespace.  */

int pkl_env_register (pkl_env env,
                      int namespace,
                      const char *name,
                      pkl_ast_node decl)
  __attribute__ ((visibility ("hidden")));

/* Return 1 if the given ENV contains only one frame.  Return 0
   otherwise.  */

int pkl_env_toplevel_p (pkl_env env)
  __attribute__ ((visibility ("hidden")));

/* Return a copy of ENV.  Note this only works for top-level
   environments.  */

pkl_env pkl_env_dup_toplevel (pkl_env env)
  __attribute__ ((visibility ("hidden")));

/* Declarations in Poke live in two different, separated name spaces:

   The `main' namespace, shared by types, variables and functions.
   The `units' namespace, for offset units.  */

#define PKL_ENV_NS_MAIN 0
#define PKL_ENV_NS_UNITS 1

/* Search in the environment ENV for a declaration with name NAME in
   the given NAMESPACE, put the lexical address of the first match in
   BACK and OVER if these are not NULL.  Return the declaration node.

   BACK is the number of frames back the declaration is located.  It
   is 0-based.

   OVER indicates its position in the list of declarations in the
   resulting frame.  It is 0-based.  */

pkl_ast_node pkl_env_lookup (pkl_env env, int namespace,
                             const char *name,
                             int *back, int *over)
  __attribute__ ((visibility ("hidden")));

/* The following iterators work on the main namespace.  */

struct pkl_ast_node_iter
{
  int bucket;        /* The bucket in which this node resides.  */
  pkl_ast_node node; /* A pointer to the node itself.  */
};


void pkl_env_iter_begin (pkl_env env, struct pkl_ast_node_iter *iter)
  __attribute__ ((visibility ("hidden")));

void pkl_env_iter_next (pkl_env env, struct pkl_ast_node_iter *iter)
  __attribute__ ((visibility ("hidden")));

bool pkl_env_iter_end (pkl_env env, const struct pkl_ast_node_iter *iter)
  __attribute__ ((visibility ("hidden")));

char *pkl_env_get_next_matching_decl (pkl_env env,
                                      struct pkl_ast_node_iter *iter,
                                      const char *name, size_t len)
  __attribute__ ((visibility ("hidden")));

/* Map over the declarations defined in the top-level compile-time
   environment, executing a handler.  */

#define PKL_MAP_DECL_TYPES PKL_AST_DECL_KIND_TYPE
#define PKL_MAP_DECL_VARS  PKL_AST_DECL_KIND_VAR

typedef void (*pkl_map_decl_fn) (pkl_ast_node decl, void *data);

void pkl_env_map_decls (pkl_env env,
                        int what,
                        pkl_map_decl_fn cb,
                        void *data)
  __attribute__ ((visibility ("hidden")));

#endif /* !PKL_ENV_H  */
