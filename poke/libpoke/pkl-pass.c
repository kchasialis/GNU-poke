/* pkl-pass.c - Support for compiler passes.  */

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

#include "pkl-pass.h"

#define PKL_CALL_PHASES(CLASS,ORDER,DISCR)                              \
  do                                                                    \
    {                                                                   \
      size_t i = 0;                                                     \
                                                                        \
      while (phases[i])                                                 \
        {                                                               \
          if (phases[i]->CLASS##_##ORDER##_handlers[(DISCR)])           \
            {                                                           \
              int restart;                                              \
                                                                        \
              node                                                      \
                = phases[i]->CLASS##_##ORDER##_handlers[(DISCR)] (compiler, \
                                                                  toplevel, \
                                                                  ast,  \
                                                                  node, \
                                                                  payloads[i], \
                                                                  &restart, \
                                                                  child_pos, \
                                                                  parent, \
                                                                  &dobreak, \
                                                                  payloads, \
                                                                  phases,\
                                                                  flags, \
                                                                  level); \
              *handlers_used += 1;                                      \
              if (dobreak)                                              \
                goto _exit;                                             \
                                                                        \
              if (restart)                                              \
                {                                                       \
                  /* Restart the subtree with the rest of the phases. */ \
                  node = pkl_do_pass_1 (compiler, toplevel, ast, node,  \
                                        child_pos,                      \
                                        parent,                         \
                                        payloads + i + 1,               \
                                        phases + i + 1,                 \
                                        flags,                          \
                                        level);                         \
                  /* goto restart */                                    \
                  goto restart;                                         \
                }                                                       \
            }                                                           \
          i++;                                                          \
        }                                                               \
    }                                                                   \
  while (0)

#define PKL_CALL_PHASES_SINGLE(what)                                    \
  do                                                                    \
    {                                                                   \
      size_t i = 0;                                                     \
                                                                        \
      while (phases[i])                                                 \
        {                                                               \
          if (phases[i]->what##_handler)                                \
            {                                                           \
              int restart;                                              \
              node                                                      \
                = phases[i]->what##_handler (compiler,                  \
                                             toplevel,                  \
                                             ast,                       \
                                             node,                      \
                                             payloads[i],               \
                                             &restart,                  \
                                             child_pos,                 \
                                             parent,                    \
                                             &dobreak,                  \
                                             payloads,                  \
                                             phases,                    \
                                             flags,                     \
                                             level);                    \
              if (dobreak)                                              \
                goto _exit;                                             \
                                                                        \
              if (restart)                                              \
                {                                                       \
                  /* Restart the subtree with the rest of the phases. */ \
                  node = pkl_do_pass_1 (compiler, toplevel, ast, node,  \
                                        child_pos,                      \
                                        parent,                         \
                                        payloads + i + 1,               \
                                        phases + i + 1,                 \
                                        flags,                          \
                                        level);                         \
                  /* goto restart */                                    \
                  goto restart;                                         \
                }                                                       \
            }                                                           \
          i++;                                                          \
        }                                                               \
    }                                                                   \
  while (0)

/* Forward prototype.  */
static pkl_ast_node pkl_do_pass_1 (pkl_compiler compiler,
                                   jmp_buf toplevel,
                                   pkl_ast ast,
                                   pkl_ast_node node,
                                   size_t child_pos,
                                   pkl_ast_node parent,
                                   void *payloads[], struct pkl_phase *phases[],
                                   int flags, int level);


#define PKL_PASS_PRE_ORDER 0
#define PKL_PASS_POST_ORDER 1

static inline pkl_ast_node
pkl_call_node_handlers (pkl_compiler compiler,
                        jmp_buf toplevel,
                        pkl_ast ast,
                        pkl_ast_node node,
                        void *payloads[],
                        struct pkl_phase *phases[],
                        int *handlers_used,
                        size_t child_pos,
                        pkl_ast_node parent,
                        int *_dobreak,
                        int order,
                        int flags,
                        int level)
{
  int node_code = PKL_AST_CODE (node);
  int dobreak = 0;

  if (order == PKL_PASS_POST_ORDER)
    {
      /* In post-order, less generic handlers should be executed
         first.  */

      /* Call the handlers defined for specific opcodes in the given
         order.  */
      if (node_code == PKL_AST_EXP)
        {
          int opcode = PKL_AST_EXP_CODE (node);

          switch (opcode)
            {
#define PKL_DEF_OP(ocode, str)                          \
              case ocode:                               \
                PKL_CALL_PHASES (op, ps, ocode);        \
                break;
#include "pkl-ops.def"
#undef PKL_DEF_OP
            default:
              /* Unknown operation code.  */
              assert (0);
            }

          /* The node may have been replaced by the handler above.
             Refresh the code.  */
          node_code = PKL_AST_CODE (node);
        }

      /* Call the phase handlers defined for specific types, in the given
         order.  */
      if (node_code == PKL_AST_TYPE)
        {
          int typecode = PKL_AST_TYPE_CODE (node);

          PKL_CALL_PHASES (type, ps, typecode);
          /* The node may have been replaced by the handler above.
             Refresh the code.  */
          node_code = PKL_AST_CODE (node);
        }

      /* Call the phase handlers defined for node codes, in the given
         order.  */
      PKL_CALL_PHASES (code, ps, node_code);

      /* Call the phase handlers defined as default.  */
      PKL_CALL_PHASES_SINGLE (default_ps);
    }
  else if (order == PKL_PASS_PRE_ORDER)
    {
      /* In pre-order, more generic handlers should be executed
         first.  */

      /* Call the phase handlers defined as default.  */
      PKL_CALL_PHASES_SINGLE (default_pr);

      /* Call the phase handlers defined for node codes, in the given
         order.  */
      PKL_CALL_PHASES (code, pr, node_code);

      /* Call the handlers defined for specific opcodes in the given
         order.  */
      if (node_code == PKL_AST_EXP)
        {
          int opcode = PKL_AST_EXP_CODE (node);

          switch (opcode)
            {
#define PKL_DEF_OP(ocode, str)                          \
              case ocode:                               \
                PKL_CALL_PHASES (op, pr, ocode);        \
                break;
#include "pkl-ops.def"
#undef PKL_DEF_OP
            default:
              /* Unknown operation code.  */
              assert (0);
            }

          /* The node may have been replaced by the handler above.
             Refresh the code.  */
          node_code = PKL_AST_CODE (node);
        }

      /* Call the phase handlers defined for specific types, in the given
         order.  */
      if (node_code == PKL_AST_TYPE)
        {
          int typecode = PKL_AST_TYPE_CODE (node);

          PKL_CALL_PHASES (type, pr, typecode);
          /* The node may have been replaced by the handler above.
             Refresh the code.  */
          node_code = PKL_AST_CODE (node);
        }
    }
  else
    assert (0);

 restart:
 _exit:
  *_dobreak = dobreak;
  return node;
}

#define PKL_PASS(CHILD)                                      \
  do                                                         \
    {                                                        \
      (CHILD) = pkl_do_pass_1 (compiler, toplevel, ast,      \
                               (CHILD), 0, node,             \
                               payloads, phases,             \
                               flags, level);                \
    }                                                        \
  while (0)

/* Note that in the following macro CHAIN should expand to an
   l-value.  */
#define PKL_PASS_CHAIN(CHAIN)                                   \
  do                                                            \
    {                                                           \
      pkl_ast_node elem, last, next;                            \
      size_t cpos = 0;                                          \
                                                                \
      /* Process first element in the chain.  */                \
      elem = (CHAIN);                                           \
      next = PKL_AST_CHAIN (elem);                              \
      CHAIN = pkl_do_pass_1 (compiler, toplevel, ast, elem, cpos++, node, \
                             payloads, phases, flags, level);           \
      last = (CHAIN);                                           \
      elem = next;                                              \
                                                                \
      /* Process rest of the chain.  */                         \
      while (elem)                                              \
        {                                                       \
          next = PKL_AST_CHAIN (elem);                          \
          PKL_AST_CHAIN (last) = pkl_do_pass_1 (compiler,       \
                                                toplevel,       \
                                                ast,            \
                                                elem,           \
                                                cpos++,         \
                                                node,           \
                                                payloads,       \
                                                phases,         \
                                                flags,          \
                                                level);         \
          last = PKL_AST_CHAIN (last);                          \
          elem = next;                                          \
        }                                                       \
    } while (0)

static pkl_ast_node
pkl_do_pass_1 (pkl_compiler compiler,
               jmp_buf toplevel,
               pkl_ast ast,
               pkl_ast_node node,
               size_t child_pos,
               pkl_ast_node parent,
               void *payloads[], struct pkl_phase *phases[],
               int flags, int level)
{
  pkl_ast_node node_orig = node;
  int node_code = PKL_AST_CODE (node);
  int handlers_used = 0;
  int dobreak = 0;

  /* If there are no passes then there is nothing to do. */
  if (phases == NULL)
    goto _exit;

  /* Check the COMPILED level in the node, and exit if the node
     doesn't need additional processing.  */
  if (level != 0 && PKL_AST_TYPE_COMPILED (node) >= level
      && PKL_AST_CODE (node) == PKL_AST_TYPE)
    goto _exit;

  /* Call the pre-order handlers from registered phases.  */
  node = pkl_call_node_handlers (compiler, toplevel, ast, node, payloads, phases,
                                 &handlers_used, child_pos, parent, &dobreak,
                                 PKL_PASS_PRE_ORDER, flags, level);
  if (dobreak)
    goto _exit;

  /* Process child nodes.  */

  if (flags & PKL_PASS_F_TYPES)
    {
      if (PKL_AST_TYPE (node))
        PKL_AST_TYPE (node)
          = pkl_do_pass_1 (compiler, toplevel, ast,
                           PKL_AST_TYPE (node), 0, node,
                           payloads, phases, flags, level);
    }

  switch (node_code)
    {
    case PKL_AST_EXP:
      PKL_PASS (PKL_AST_EXP_OPERAND (node, 0));
      if (PKL_AST_EXP_NUMOPS (node) == 2)
        PKL_PASS (PKL_AST_EXP_OPERAND (node, 1));

      break;
    case PKL_AST_PROGRAM:
      if (PKL_AST_PROGRAM_ELEMS (node))
        PKL_PASS_CHAIN (PKL_AST_PROGRAM_ELEMS (node));

      break;
    case PKL_AST_COND_EXP:
      PKL_PASS (PKL_AST_COND_EXP_COND (node));
      PKL_PASS (PKL_AST_COND_EXP_THENEXP (node));
      PKL_PASS (PKL_AST_COND_EXP_ELSEEXP (node));

      break;
    case PKL_AST_ARRAY:
      if (PKL_AST_ARRAY_INITIALIZERS (node))
        PKL_PASS_CHAIN (PKL_AST_ARRAY_INITIALIZERS (node));

      break;
    case PKL_AST_ARRAY_INITIALIZER:
      if (PKL_AST_ARRAY_INITIALIZER_INDEX (node))
        PKL_PASS (PKL_AST_ARRAY_INITIALIZER_INDEX (node));
      PKL_PASS (PKL_AST_ARRAY_INITIALIZER_EXP (node));

      break;
    case PKL_AST_TRIMMER:
      PKL_PASS (PKL_AST_TRIMMER_ENTITY (node));
      if (PKL_AST_TRIMMER_FROM (node))
        PKL_PASS (PKL_AST_TRIMMER_FROM (node));
      if (PKL_AST_TRIMMER_TO (node))
        PKL_PASS (PKL_AST_TRIMMER_TO (node));

      break;
    case PKL_AST_INDEXER:
      PKL_PASS (PKL_AST_INDEXER_ENTITY (node));
      PKL_PASS (PKL_AST_INDEXER_INDEX (node));

      break;
    case PKL_AST_STRUCT:
      if (PKL_AST_STRUCT_FIELDS (node))
        PKL_PASS_CHAIN (PKL_AST_STRUCT_FIELDS (node));
      break;
    case PKL_AST_STRUCT_FIELD:
      if (PKL_AST_STRUCT_FIELD_NAME (node))
        PKL_PASS (PKL_AST_STRUCT_FIELD_NAME (node));
      PKL_PASS (PKL_AST_STRUCT_FIELD_EXP (node));
      break;
    case PKL_AST_STRUCT_REF:
      PKL_PASS (PKL_AST_STRUCT_REF_STRUCT (node));
      PKL_PASS (PKL_AST_STRUCT_REF_IDENTIFIER (node));

      break;
    case PKL_AST_OFFSET:
      if (PKL_AST_OFFSET_MAGNITUDE (node))
        PKL_PASS (PKL_AST_OFFSET_MAGNITUDE (node));
      PKL_PASS (PKL_AST_OFFSET_UNIT (node));

      break;
    case PKL_AST_CAST:
      PKL_PASS (PKL_AST_CAST_TYPE (node));
      PKL_PASS (PKL_AST_CAST_EXP (node));

      break;

    case PKL_AST_ISA:
      PKL_PASS (PKL_AST_ISA_TYPE (node));
      PKL_PASS (PKL_AST_ISA_EXP (node));

      break;
    case PKL_AST_MAP:
      if (PKL_AST_MAP_IOS (node))
        PKL_PASS (PKL_AST_MAP_IOS (node));
      PKL_PASS (PKL_AST_MAP_OFFSET (node));
      PKL_PASS (PKL_AST_MAP_TYPE (node));

      break;
    case PKL_AST_SCONS:
      PKL_PASS (PKL_AST_SCONS_TYPE (node));
      PKL_PASS (PKL_AST_SCONS_VALUE (node));

      break;
    case PKL_AST_TYPE:
      {
        switch (PKL_AST_TYPE_CODE (node))
          {
          case PKL_TYPE_ARRAY:
            PKL_PASS (PKL_AST_TYPE_A_ETYPE (node));
            if (PKL_AST_TYPE_A_BOUND (node))
              PKL_PASS (PKL_AST_TYPE_A_BOUND (node));

            break;
          case PKL_TYPE_STRUCT:
            if (PKL_AST_TYPE_S_ELEMS (node))
              PKL_PASS_CHAIN (PKL_AST_TYPE_S_ELEMS (node));
            break;
          case PKL_TYPE_FUNCTION:
            if (PKL_AST_TYPE_F_ARGS (node))
              PKL_PASS_CHAIN (PKL_AST_TYPE_F_ARGS (node));
            PKL_PASS (PKL_AST_TYPE_F_RTYPE (node));
            break;
          case PKL_TYPE_OFFSET:
            PKL_PASS (PKL_AST_TYPE_O_BASE_TYPE (node));
            PKL_PASS (PKL_AST_TYPE_O_UNIT (node));

            break;
          case PKL_TYPE_INTEGRAL:
          case PKL_TYPE_STRING:
          case PKL_TYPE_VOID:
          case PKL_TYPE_ANY:
          case PKL_TYPE_NOTYPE:
            break;
          default:
            /* Unknown type code.  */
            assert (0);
          }
        break;
      }
    case PKL_AST_STRUCT_TYPE_FIELD:
      if (PKL_AST_STRUCT_TYPE_FIELD_NAME (node))
        PKL_PASS (PKL_AST_STRUCT_TYPE_FIELD_NAME (node));
      PKL_PASS (PKL_AST_STRUCT_TYPE_FIELD_TYPE (node));
      if (PKL_AST_STRUCT_TYPE_FIELD_CONSTRAINT (node))
        PKL_PASS (PKL_AST_STRUCT_TYPE_FIELD_CONSTRAINT (node));
      if (PKL_AST_STRUCT_TYPE_FIELD_INITIALIZER (node))
        PKL_PASS (PKL_AST_STRUCT_TYPE_FIELD_INITIALIZER (node));
      if (PKL_AST_STRUCT_TYPE_FIELD_OPTCOND (node))
        PKL_PASS (PKL_AST_STRUCT_TYPE_FIELD_OPTCOND (node));
      if (PKL_AST_STRUCT_TYPE_FIELD_LABEL (node))
        PKL_PASS (PKL_AST_STRUCT_TYPE_FIELD_LABEL (node));
      break;
    case PKL_AST_DECL:
      PKL_PASS (PKL_AST_DECL_INITIAL (node));

      break;
    case PKL_AST_FUNCALL:
      if (PKL_AST_FUNCALL_ARGS (node))
        PKL_PASS_CHAIN (PKL_AST_FUNCALL_ARGS (node));
      PKL_PASS (PKL_AST_FUNCALL_FUNCTION (node));
      break;
    case PKL_AST_FUNCALL_ARG:
      if (PKL_AST_FUNCALL_ARG_EXP (node))
        PKL_PASS (PKL_AST_FUNCALL_ARG_EXP (node));
      break;
    case PKL_AST_FUNC:
      if (PKL_AST_FUNC_RET_TYPE (node))
        PKL_PASS (PKL_AST_FUNC_RET_TYPE (node));
      if (PKL_AST_FUNC_ARGS (node))
        PKL_PASS_CHAIN (PKL_AST_FUNC_ARGS (node));
      PKL_PASS (PKL_AST_FUNC_BODY (node));

      break;
    case PKL_AST_FUNC_ARG:
      if (PKL_AST_FUNC_ARG_TYPE (node))
        PKL_PASS (PKL_AST_FUNC_ARG_TYPE (node));
      if (PKL_AST_FUNC_ARG_INITIAL (node))
        PKL_PASS (PKL_AST_FUNC_ARG_INITIAL (node));
      break;
    case PKL_AST_FUNC_TYPE_ARG:
      if (PKL_AST_FUNC_TYPE_ARG_TYPE (node))
        PKL_PASS (PKL_AST_FUNC_TYPE_ARG_TYPE (node));
      break;
    case PKL_AST_COMP_STMT:
      if (PKL_AST_COMP_STMT_STMTS (node))
        PKL_PASS_CHAIN (PKL_AST_COMP_STMT_STMTS (node));
      break;
    case PKL_AST_ASS_STMT:
      PKL_PASS (PKL_AST_ASS_STMT_EXP (node));
      PKL_PASS (PKL_AST_ASS_STMT_LVALUE (node));
      break;
    case PKL_AST_IF_STMT:
      PKL_PASS (PKL_AST_IF_STMT_EXP (node));
      PKL_PASS (PKL_AST_IF_STMT_THEN_STMT (node));
      if (PKL_AST_IF_STMT_ELSE_STMT (node))
        PKL_PASS (PKL_AST_IF_STMT_ELSE_STMT (node));
      break;
    case PKL_AST_LOOP_STMT:
      if (PKL_AST_LOOP_STMT_ITERATOR (node))
        PKL_PASS (PKL_AST_LOOP_STMT_ITERATOR (node));
      if (PKL_AST_LOOP_STMT_CONDITION (node))
        PKL_PASS (PKL_AST_LOOP_STMT_CONDITION (node));
      PKL_PASS (PKL_AST_LOOP_STMT_BODY (node));
      break;
    case PKL_AST_LOOP_STMT_ITERATOR:
      PKL_PASS (PKL_AST_LOOP_STMT_ITERATOR_CONTAINER (node));
      PKL_PASS (PKL_AST_LOOP_STMT_ITERATOR_DECL (node));
      break;
    case PKL_AST_BREAK_STMT:
      break;
    case PKL_AST_RETURN_STMT:
      if (PKL_AST_RETURN_STMT_EXP (node))
        PKL_PASS (PKL_AST_RETURN_STMT_EXP (node));
      break;
    case PKL_AST_EXP_STMT:
      PKL_PASS (PKL_AST_EXP_STMT_EXP (node));
      break;
    case PKL_AST_TRY_CATCH_STMT:
      PKL_PASS (PKL_AST_TRY_CATCH_STMT_CODE (node));
      PKL_PASS (PKL_AST_TRY_CATCH_STMT_HANDLER (node));
      if (PKL_AST_TRY_CATCH_STMT_ARG (node))
        PKL_PASS (PKL_AST_TRY_CATCH_STMT_ARG (node));
      if (PKL_AST_TRY_CATCH_STMT_EXP (node))
        PKL_PASS (PKL_AST_TRY_CATCH_STMT_EXP (node));
      break;
    case PKL_AST_TRY_UNTIL_STMT:
      PKL_PASS (PKL_AST_TRY_UNTIL_STMT_CODE (node));
      PKL_PASS (PKL_AST_TRY_UNTIL_STMT_EXP (node));
      break;
    case PKL_AST_RAISE_STMT:
      if (PKL_AST_RAISE_STMT_EXP (node))
        PKL_PASS (PKL_AST_RAISE_STMT_EXP (node));
      break;
    case PKL_AST_PRINT_STMT_ARG:
      if (PKL_AST_PRINT_STMT_ARG_EXP (node))
        PKL_PASS (PKL_AST_PRINT_STMT_ARG_EXP (node));
      break;
    case PKL_AST_PRINT_STMT:
      if (PKL_AST_PRINT_STMT_FMT (node))
        PKL_PASS (PKL_AST_PRINT_STMT_FMT (node));
      if (PKL_AST_PRINT_STMT_TYPES (node))
        PKL_PASS_CHAIN (PKL_AST_PRINT_STMT_TYPES (node));
      if (PKL_AST_PRINT_STMT_ARGS (node))
        PKL_PASS_CHAIN (PKL_AST_PRINT_STMT_ARGS (node));
      break;
    case PKL_AST_NULL_STMT:
    case PKL_AST_INTEGER:
    case PKL_AST_STRING:
    case PKL_AST_IDENTIFIER:
    case PKL_AST_ENUM:
    case PKL_AST_ENUMERATOR:
    case PKL_AST_VAR:
      /* These node types have no children.  */
      break;
    default:
      /* Unknown node code.  This kills the poke :'( */
      assert (0);
    }

  /* Call the post-order handlers from registered phases.  */
  node = pkl_call_node_handlers (compiler, toplevel, ast, node, payloads, phases,
                                 &handlers_used, child_pos, parent, &dobreak,
                                 PKL_PASS_POST_ORDER, flags, level);

  /* If no handler has been invoked, call the default handler of the
     registered phases in case they are defined.  */
  if (handlers_used == 0)
    PKL_CALL_PHASES_SINGLE(else);
 restart:

  /* If a new node was created to replace the incoming node, increase
     its reference counter.  This assumes that the node returned by
     this function will be stored in some other node (or the top-level
     AST structure).  */
  if (node != node_orig)
    ASTREF (node);

 _exit:
  if (level != 0
      && PKL_AST_CODE (node) == PKL_AST_TYPE)
    PKL_AST_TYPE_COMPILED (node) = level;
  return node;
}

int
pkl_do_subpass (pkl_compiler compiler,
                pkl_ast ast, pkl_ast_node node,
                struct pkl_phase *phases[], void *payloads[],
                int flags, int level)
{
  jmp_buf toplevel;

  switch (setjmp (toplevel))
    {
    case 0:
      ast->ast = pkl_do_pass_1 (compiler, toplevel, ast, node, 0,
                                NULL /* parent */,
                                payloads, phases, flags, level);
      break;
    case 1:
      /* Non-error non-local exit.  */
      break;
    case 2:
      /* Error in node handler.  */
      return 0;
      break;
    }

  return 1;
}

int
pkl_do_pass (pkl_compiler compiler,
             pkl_ast ast,
             struct pkl_phase *phases[], void *payloads[],
             int flags, int level)
{
  return pkl_do_subpass (compiler, ast, ast->ast, phases,
                         payloads, flags, level);
}
