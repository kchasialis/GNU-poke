/* pkl-pass.h - Support for compiler passes.  */

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

#ifndef PKL_PASS_H
#define PKL_PASS_H

#include <config.h>

#include <assert.h>
#include <setjmp.h>
#include <inttypes.h>
#include <stdarg.h>

#include "pkl.h"
#include "pkl-diag.h"
#include "pkl-ast.h"

/* A `pass' is a complete run over a given AST.  A `phase' is an
   analysis or a transformation performed over a subset of the nodes
   in the AST.  One pass may integrate several phases.

   Implementing a phase involves defining a struct pkl_phase variable
   and filling it up.  A pkl_phase struct contains:

   - CODE_PS_HANDLERS is a table indexed by node codes, which must be
     values in the `pkl_ast_code' enumeration defined in pkl-ast.h.
     For example, PKL_AST_ARRAY.  It maps codes to
     pkl_phase_handler_fn functions.

   - OP_PS_HANDLERS is a table indexed by operator codes, which must
     be values in the `pkl_ast_op' enumeration defined in pkl-ast.h.
     This enumeration is in turn generated from the operators defined
     in pkl-ops.def.  For example, PKL_AST_OP_ADD.  It maps codes to
     pkl_phase_handler_fn functions.

   - TYPE_PS_HANDLERS is a table indexed by type codes, which must be
     values in the `pkl_ast_type_code' enumeration defined in
     pkl-ast.h. For example, PKL_TYPE_STRING.  It maps codes to
     pkl_phase_handler_fn functions.

   The handlers defined in the tables above are invoked while
   traversing the AST in a post-order depth-first fashion.  Additional
   tables exist to define handlers that are executed while traversing
   the AST a pre-order depth-first fashion:

   - CODE_PR_HANDLERS
   - OP_PR_HANDLERS
   - TYPE_PR_HANDLERS

   A given phase can define handlers of both types: PR (pre-order) and
   PS (post-order).

   There are three additional handlers that the user can install:

   - DEFAULT_PR_HANDLER is invoked for every node in the AST, after the
     more particular ones, in pre-order.

   - DEFAULT_PS_HANDLER is invoked for every node in the AST, after
     the more particular ones, in post-order.

   - ELSE_HANDLER, if not NULL, is invoked for every node for which no
     other handler (PR or PS) has been invoked.

   Note that if a given node class falls in several categories as
   implemented in the handlers tables, the order in which the
   different kind of handlers are executed depend on the ordering:

   - Pre-order handlers are executed from more generic to more
     particular.  For example, for a PKL_AST_TYPE node with type code
     PKL_TYPE_ARRAY, the handler in `code_pr_handlers' will be invoked
     first, followed by the handler in `type_pr_handlers'.

   - Post-order handlers are executed from more particular to more
     generic.  For example, for a PKL_AST_TYPE node with the type code
     PKL_TYPE_ARRAY, the handler in `type_ps_handlers' will be invoked
     first, followed by the handler in `code_ps_handlers'.

   If the `else' handler is NULL and no other handler is executed,
   then no action is performed on a node other than traversing it.  */

struct pkl_phase; /* Forward declaration.  */

typedef pkl_ast_node (*pkl_phase_handler_fn) (pkl_compiler compiler,
                                              jmp_buf toplevel,
                                              pkl_ast ast,
                                              pkl_ast_node node,
                                              void *payload,
                                              int *restart,
                                              size_t child_pos,
                                              pkl_ast_node parent,
                                              int *dobreak,
                                              void *payloads[],
                                              struct pkl_phase *phases[],
                                              int flags,
                                              int level);

struct pkl_phase
{
  pkl_phase_handler_fn else_handler;

  pkl_phase_handler_fn default_ps_handler;
  pkl_phase_handler_fn code_ps_handlers[PKL_AST_LAST];
  pkl_phase_handler_fn op_ps_handlers[PKL_AST_OP_LAST];
  pkl_phase_handler_fn type_ps_handlers[PKL_TYPE_NOTYPE];

  pkl_phase_handler_fn default_pr_handler;
  pkl_phase_handler_fn code_pr_handlers[PKL_AST_LAST];
  pkl_phase_handler_fn op_pr_handlers[PKL_AST_OP_LAST];
  pkl_phase_handler_fn type_pr_handlers[PKL_TYPE_NOTYPE];
};

typedef struct pkl_phase *pkl_phase;

/* The following macros should be used in order to register handlers
   in a `struct pkl_phase'.  This allows changing the structure layout
   without impacting the phase definitions.  */

#define PKL_PHASE_ELSE_HANDLER(handler)      \
  .else_handler = handler

#define PKL_PHASE_PR_DEFAULT_HANDLER(handler)   \
  .default_pr_handler = handler
#define PKL_PHASE_PS_DEFAULT_HANDLER(handler)   \
  .default_ps_handler = handler

#define PKL_PHASE_PR_HANDLER(code, handler)     \
  .code_pr_handlers[code] = handler
#define PKL_PHASE_PS_HANDLER(code, handler)     \
  .code_ps_handlers[code] = handler

#define PKL_PHASE_PR_OP_HANDLER(code,handler)   \
  .op_pr_handlers[code] = handler
#define PKL_PHASE_PS_OP_HANDLER(code,handler)   \
  .op_ps_handlers[code] = handler

#define PKL_PHASE_PR_TYPE_HANDLER(code,handler) \
  .type_pr_handlers[code] = handler
#define PKL_PHASE_PS_TYPE_HANDLER(code,handler) \
  .type_ps_handlers[code] = handler

/* The following macros are available to be used in the body of a node
   handler:

   PKL_PASS_COMPILER expands to an l-value holding the compiler
   executing the pass.

   PKL_PASS_PAYLOAD expands to an l-value holding the data pointer
   passed to `pkl_do_pass'.

   PKL_PASS_AST expands to an l-value holding the pkl_ast value
   corresponding to the AST being processed.

   PKL_PASS_NODE expands to an l-value holding the pkl_ast_node for
   the node being processed.

   PKL_PASS_PARENT expands to an l-value holding the pkl_ast_node for
   the parent of the node being processed.  This is NULL for the
   top-level node in the AST.

   PKL_PASS_CHILD_POS expands to an l-value holding the position (zero
   based) of the node being processed in the parent's node chain.  If
   the node is not part of a chain, this holds 0.

   PKL_PASS_DONE finishes the execution of the node handler.  This is
   equivalent to reaching the end of the handler body.

   PKL_PASS_BREAK causes the pass manager to not process the children
   of the current node.  This should only be used in a PR handler.

   PKL_PASS_RESTART expands to an l-value that should be set to 1 if
   the handler modifies its subtree structure in any way, either
   creating new nodes or removing existing nodes.  This makes the pass
   machinery do the right thing (hopefully.)  By default its value is
   0.  This macro should _not_ be used as an r-value.  Also, setting
   PKL_PASS_RESTART to 1 should only be done in PS handlers.

   PKL_PASS_SUBPASS (NODE) starts a subpass that processes the subtree
   starting at NODE.  The subpass shares the same payloads than the
   current pass.  If the execution of the subpass returns an error
   then the expansion of this macro calls PKL_PASS_ERROR.

   PKL_PASS_EXIT can be used in order to interrupt the execution of
   the compiler pass, making `pkl_do_pass' to return a non-error code.

   PKL_PASS_ERROR can be used in order to interrupt the execution of
   the compiler pass, making `pkl_do_pass' to return an error code.

   If you use PKL_PASS_EXIT or PKL_PASS_ERROR, please make sure to
   delete any node you create unless they are linked to the AST.
   Otherwise you will leak memory.  */

#define PKL_PASS_COMPILER _compiler
#define PKL_PASS_PAYLOAD _payload
#define PKL_PASS_AST _ast
#define PKL_PASS_NODE _node
#define PKL_PASS_PARENT _parent
#define PKL_PASS_RESTART (*_restart)
#define PKL_PASS_CHILD_POS _child_pos

#define PKL_PASS_SUBPASS(NODE)                            \
  do                                                      \
    {                                                     \
      if (!pkl_do_subpass (PKL_PASS_COMPILER,             \
                           PKL_PASS_AST,                  \
                           (NODE),                        \
                           _phases,                       \
                           _payloads,                     \
                           _flags,                        \
                           _level))                       \
        PKL_PASS_ERROR;                                   \
    }                                                     \
  while (0)

#define PKL_PASS_DONE do { goto _exit; } while (0)
#define PKL_PASS_BREAK do { *_dobreak = 1; goto _exit; } while (0)

#define PKL_PASS_EXIT do { longjmp (_toplevel, 1); } while (0)
#define PKL_PASS_ERROR do { longjmp (_toplevel, 2); } while (0)

/* The following macros are used to define phase handlers, like
   follows:

   PKL_PHASE_BEGIN_HANDLER (handler_name)
      ... handler prologue ...
   {
      ... handler body ...
   }
   PKL_PHASE_END_HANDLER

   Only certain macros should be used as the handler prologue.  See
   below.  The handler body contains your C code.  Reaching the end of
   the body finishes the execution of the handler.  */

#define PKL_PHASE_BEGIN_HANDLER(name)                                   \
  static pkl_ast_node name (pkl_compiler _compiler, jmp_buf _toplevel,  \
                            pkl_ast _ast,                               \
                            pkl_ast_node _node, void *_payload,         \
                            int *_restart, size_t _child_pos,           \
                            pkl_ast_node _parent, int *_dobreak,        \
                            void *_payloads[],                          \
                            struct pkl_phase *_phases[],                \
                            int _flags,                                 \
                            int _level)                                 \
  {                                                                     \
  /* printf (#name " on node %" PRIu64 "\n", PKL_AST_UID (_node)); */   \
     PKL_PASS_RESTART = 0;

#define PKL_PHASE_END_HANDLER                        \
                                                     \
   goto _exit; /* To avoid compiler warning */       \
_exit:                                               \
   return PKL_PASS_NODE;                             \
  }

/* The PKL_PHASE_PARENT, PKL_PHASE_PARENT_OP and PKL_PHASE_PARENT_TYPE
   macros can be used in the prologue of a handler in order to delimit
   its execution to the case where the current node is a child of a
   node of the given node code/operation code/type code.  */

static inline int
pkl_phase_parent_in (pkl_ast_node parent,
                     int nc, ...)
{
  va_list valist;
  int i;

  if (parent == NULL)
    /* This happens with the top-level node of the AST, or the
       top-level node of a subpass.  */
    return 1;

  va_start (valist, nc);
  for (i = 0; i < nc; i++)
    {
      int code = va_arg (valist, int);
      if (PKL_AST_CODE (parent) == code)
        return 1;
    }
  va_end (valist);

  return 0;
}

#define PKL_PHASE_PARENT(NP,...)                                        \
  if (!pkl_phase_parent_in (PKL_PASS_PARENT, (NP), __VA_ARGS__))        \
    {                                                                   \
      goto _exit;                                                       \
    }

/* Traverse the given AST, applying the provided phases (or
   transformations) in sequence to each AST node.

   PHASES is a NULL-terminated array of pointers to node handlers.

   PAYLOADS is an array of pointers to payloads, which will be passed
   to the node handlers occupying the same position in the PHASES
   array.  There should be as much payloads as phases, and it is not
   needed to terminate this array with NULL.

   FLAGS is a set of ORed flags, that modify the behavior or
   pkl_do_pass.  Allowed flags are:

   PKL_PASS_F_TYPES
      Traverse type tags in AST nodes.

   Running several phases in "parallel" in the same pass is good for
   performance.  However, there is an important consideration: if a
   phase requires to process each AST nodes just once, no restarting
   phases must precede it in the pass.  This is the case of the code
   generation pass, for example.

   Return 0 if some error occurred during the pass execution.  Return
   1 otherwise.  */

#define PKL_PASS_F_TYPES 1

int pkl_do_pass (pkl_compiler compiler, pkl_ast ast,
                 struct pkl_phase *phases[], void *payloads[],
                 int flags, int level)
  __attribute__ ((visibility ("hidden")));

/* The following function is to be used by the PKL_PASS_SUBPASS macro
   defined above.  */

int pkl_do_subpass (pkl_compiler compiler, pkl_ast ast, pkl_ast_node node,
                    struct pkl_phase *phases[], void *payloads[],
                    int flags, int level)
  __attribute__ ((visibility ("hidden")));

/* Macros to emit a compilation error, a warning or an ICE from a
   phase handler.  Using them reduces verbosity by not passing the
   compiler and the AST arguments explicitly.  */

#define PKL_ERROR(...)                                                  \
  pkl_error (PKL_PASS_COMPILER, PKL_PASS_AST, __VA_ARGS__)

#define PKL_WARNING(...)                                                \
  pkl_warning (PKL_PASS_COMPILER, PKL_PASS_AST, __VA_ARGS__)

#define PKL_ICE(...)                                                    \
  pkl_ice (PKL_PASS_COMPILER, PKL_PASS_AST, __VA_ARGS__)

#endif /* PKL_PASS_H  */
