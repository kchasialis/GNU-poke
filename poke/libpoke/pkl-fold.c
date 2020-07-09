/* pkl-fold.c - Constant folding phase for the poke compiler. */

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

/* This file implements a constant folding phase.  */

#include <config.h>

#include <gettext.h>
#define _(str) gettext (str)
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include "xalloc.h"

#include "pk-utils.h"

#include "pkl.h"
#include "pkl-diag.h"
#include "pkl-ast.h"
#include "pkl-pass.h"
#include "pkl-fold.h"

/* Roll out our own GCD from gnulib.  */
#define WORD_T uint64_t
#define GCD fold_gcd
#include <gcd.c>

#define PKL_FOLD_PAYLOAD ((pkl_fold_payload) PKL_PASS_PAYLOAD)

/* The following handler is used in the folding phase to avoid
   re-folding already processed AST type nodes.  */

PKL_PHASE_BEGIN_HANDLER (pkl_fold_pr_type)
{
  if (PKL_AST_TYPE_COMPILED (PKL_PASS_NODE))
    PKL_PASS_BREAK;
}
PKL_PHASE_END_HANDLER

/* Emulation routines.

   The letter-codes after EMUL_ specify the number and kind of
   arguments that the operations receive and return.  The type of the
   returned value comes last.

   So, for example, EMUL_III declares an int64 OP int64 -> int64
   operation, whereas EMUL_SSI declares a string OP string -> int64
   operation.  */

#define EMUL_UNA_PROTO(OP,SIGN,TYPE,RTYPE)              \
  static inline RTYPE emul_##SIGN##_##OP (TYPE op)

#define EMUL_BIN_PROTO(OP,SIGN,TYPE,RTYPE)                      \
  static inline RTYPE emul_##SIGN##_##OP (TYPE op1, TYPE op2)

#define EMUL_II(OP)                       \
  EMUL_UNA_PROTO (OP,s,int64_t,int64_t)
#define EMUL_UU(OP)                       \
  EMUL_UNA_PROTO (OP,u,uint64_t,uint64_t)
#define EMUL_III(OP)                      \
  EMUL_BIN_PROTO (OP,s,int64_t,int64_t)
#define EMUL_UUU(OP)                      \
  EMUL_BIN_PROTO (OP,u,uint64_t,uint64_t)
#define EMUL_UUI(OP)                      \
  EMUL_BIN_PROTO (OP,u,uint64_t,int64_t)
#define EMUL_SSI(OP)                          \
  EMUL_BIN_PROTO (OP,s,const char *,int64_t)
#define EMUL_SIS(OP)                            \
  static inline char * emul_##OP (const char *op1, uint64_t op2)

EMUL_II (neg) { return -op; }
EMUL_UU (neg) { return -op; }
EMUL_II (pos) { return op; }
EMUL_UU (pos) { return op; }
EMUL_II (not) { return !op; }
EMUL_UU (not) { return !op; }
EMUL_II (bnot) { return ~op; }
EMUL_UU (bnot) { return ~op; }

EMUL_UUU (or) { return op1 || op2; }
EMUL_III (or) { return op1 || op2; }
EMUL_UUU (ior) { return op1 | op2; }
EMUL_III (ior) { return op1 | op2; }
EMUL_UUU (xor) { return op1 ^ op2; }
EMUL_III (xor) { return op1 ^ op2; }
EMUL_UUU (and) { return op1 && op2; }
EMUL_III (and) { return op1 && op2; }
EMUL_UUU (band) { return op1 & op2; }
EMUL_III (band) { return op1 & op2; }
EMUL_UUU (eq) { return op1 == op2; }
EMUL_III (eq) { return op1 == op2; }
EMUL_UUU (ne) { return op1 != op2; }
EMUL_III (ne) { return op1 != op2; }
EMUL_UUU (add) { return op1 + op2; }
EMUL_III (add) { return op1 + op2; }
EMUL_UUU (sub) { return op1 - op2; }
EMUL_III (sub) { return op1 - op2; }
EMUL_UUU (mul) { return op1 * op2; }
EMUL_III (mul) { return op1 * op2; }
EMUL_UUU (div) { return op1 / op2; }
EMUL_III (div) { return op1 / op2; }
EMUL_UUU (cdiv) { return (op1 - 1 + op2) / op2; }
EMUL_III (cdiv) { return (op1 - 1 + op2) / op2; }
EMUL_UUU (mod) { return op1 % op2; }
EMUL_III (mod) { return op1 % op2; }
EMUL_III (pow) { return pk_ipow (op1, op2); }
EMUL_UUU (pow) { return pk_upow (op1, op2); }
EMUL_UUU (lt) { return op1 < op2; }
EMUL_III (lt) { return op1 < op2; }
EMUL_UUU (gt) { return op1 > op2; }
EMUL_III (gt) { return op1 > op2; }
EMUL_UUU (le) { return op1 <= op2; }
EMUL_III (le) { return op1 <= op2; }
EMUL_UUU (ge) { return op1 >= op2; }
EMUL_III (ge) { return op1 >= op2; }

EMUL_UUU (gcd) { return fold_gcd (op1, op2); }
EMUL_III (gcd) { assert (0); return 0; }

EMUL_UUU (sl) { return op1 << op2; }
EMUL_III (sl) { return op1 << op2; }
EMUL_UUU (sr) { return op1 >> op2; }
EMUL_III (sr) { return op1 >> op2; }

EMUL_SSI (eqs) { return (STREQ (op1, op2)); }
EMUL_SSI (nes) { return (STRNEQ (op1, op2)); }
EMUL_SSI (gts) { return (strcmp (op1, op2) > 0); }
EMUL_SSI (lts) { return (strcmp (op1, op2) < 0); }
EMUL_SSI (les) { return (strcmp (op1, op2) <= 0); }
EMUL_SSI (ges) { return (strcmp (op1, op2) >= 0); }

EMUL_SIS (muls)
{
  char *res = xmalloc (strlen (op1) * op2 + 1);
  size_t i;

  *res = '\0';
  for (i = 0; i < op2; ++i)
    strcat (res, op1);

  return res;
}

/* The following emulation routines work on offset magnitudes
   normalized to bits.  */
EMUL_UUI (eqo) { return op1 == op2; }
EMUL_UUI (neo) { return op1 != op2; }
EMUL_UUI (gto) { return op1 > op2; }
EMUL_UUI (lto) { return op1 < op2; }
EMUL_UUI (leo) { return op1 <= op2; }
EMUL_UUI (geo) { return op1 >= op2; }
EMUL_III (eqo) { return op1 == op2; }
EMUL_III (neo) { return op1 != op2; }
EMUL_III (gto) { return op1 > op2; }
EMUL_III (lto) { return op1 < op2; }
EMUL_III (leo) { return op1 <= op2; }
EMUL_III (geo) { return op1 >= op2; }
EMUL_UUU (addo) { return op1 + op2; }
EMUL_III (addo) { return op1 + op2; }
EMUL_UUU (subo) { return op1 - op2; }
EMUL_III (subo) { return op1 - op2; }
EMUL_UUU (mulo) { return op1 * op2; }
EMUL_III (mulo) { return op1 * op2; }
EMUL_UUU (divo) { return op1 / op2; }
EMUL_III (divo) { return op1 / op2; }
EMUL_UUU (cdivo) { return (op1 - 1 + op2) / op2; }
EMUL_III (cdivo) { return (op1 - 1 + op2) / op2; }
EMUL_UUU (modo) { return op1 % op2; }
EMUL_III (modo) { return op1 % op2; }
EMUL_UUU (ioro) { return op1 | op2; }
EMUL_III (ioro) { return op1 | op2; }
EMUL_UUU (xoro) { return op1 ^ op2; }
EMUL_III (xoro) { return op1 ^ op2; }
EMUL_UUU (bando) { return op1 & op2; }
EMUL_III (bando) { return op1 & op2; }
EMUL_UUU (slo) { return op1 << op2; }
EMUL_III (slo) { return op1 << op2; }
EMUL_UUU (sro) { return op1 >> op2; }
EMUL_III (sro) { return op1 >> op2; }
EMUL_III (powo) { return pk_ipow (op1, op2); }
EMUL_UUU (powo) { return pk_upow (op1, op2); }
EMUL_II (poso) { return op; }
EMUL_UU (poso) { return op; }
EMUL_II (nego) { return -op; }
EMUL_UU (nego) { return -op; }
EMUL_II (bnoto) { return ~op; }
EMUL_UU (bnoto) { return ~op; }

/* Auxiliary macros used in the handlers below.  */

#define OP_UNARY_II(OP)                         \
  do                                                                    \
    {                                                                   \
      pkl_ast_node type = PKL_AST_TYPE (PKL_PASS_NODE);                 \
      pkl_ast_node op = PKL_AST_EXP_OPERAND (PKL_PASS_NODE, 0);         \
                                                                        \
      if (PKL_AST_TYPE_CODE (type) == PKL_TYPE_INTEGRAL)                \
        {                                                               \
          pkl_ast_node new;                                             \
          uint64_t result;                                              \
                                                                        \
          if (PKL_AST_CODE (op) != PKL_AST_INTEGER)                     \
            /* We cannot fold this expression.  */                      \
            PKL_PASS_DONE;                                              \
                                                                        \
          if (PKL_AST_TYPE_I_SIGNED (type))                             \
            result = emul_s_##OP (PKL_AST_INTEGER_VALUE (op));          \
          else                                                          \
            result = emul_u_##OP (PKL_AST_INTEGER_VALUE (op));          \
                                                                        \
          new = pkl_ast_make_integer (PKL_PASS_AST, result);            \
          PKL_AST_TYPE (new) = ASTREF (type);                           \
          PKL_AST_LOC (new) = PKL_AST_LOC (PKL_PASS_NODE);              \
                                                                        \
          pkl_ast_node_free (PKL_PASS_NODE);                            \
          PKL_PASS_NODE = new;                                          \
          PKL_PASS_DONE;                                                \
        }                                                               \
    }                                                                   \
  while (0)

#define OP_UNARY_OO(OP)                                                 \
  do                                                                    \
    {                                                                   \
      pkl_ast_node type = PKL_AST_TYPE (PKL_PASS_NODE);                 \
      pkl_ast_node op = PKL_AST_EXP_OPERAND (PKL_PASS_NODE, 0);         \
                                                                        \
      if (PKL_AST_TYPE_CODE (type) == PKL_TYPE_OFFSET)                  \
        {                                                               \
          pkl_ast_node new;                                             \
          uint64_t result;                                              \
          pkl_ast_node magnitude;                                       \
          pkl_ast_node op_magnitude;                                    \
          pkl_ast_node op_unit;                                         \
          pkl_ast_node type_base_type = PKL_AST_TYPE_O_BASE_TYPE (type); \
          pkl_ast_node type_unit = PKL_AST_TYPE_O_UNIT (type);          \
                                                                        \
          if (PKL_AST_CODE (op) != PKL_AST_OFFSET)                      \
            /* We cannot fold this expression.  */                      \
            PKL_PASS_DONE;                                              \
                                                                        \
          op_magnitude = PKL_AST_OFFSET_MAGNITUDE (op);                 \
          op_unit = PKL_AST_OFFSET_UNIT (op);                           \
                                                                        \
          if (PKL_AST_CODE (op_magnitude) != PKL_AST_INTEGER            \
              || PKL_AST_CODE (op_unit) != PKL_AST_INTEGER)             \
            /* We cannot fold this expression.  */                      \
            PKL_PASS_DONE;                                              \
                                                                        \
          if (PKL_AST_TYPE_I_SIGNED (type))                             \
            result = emul_s_##OP (PKL_AST_INTEGER_VALUE (op_magnitude));\
          else                                                          \
            result = emul_u_##OP (PKL_AST_INTEGER_VALUE (op_magnitude));\
                                                                        \
          magnitude = pkl_ast_make_integer (PKL_PASS_AST, result);      \
          PKL_AST_TYPE (magnitude) = ASTREF (type_base_type);           \
          PKL_AST_LOC (magnitude) = PKL_AST_LOC (PKL_PASS_NODE);        \
                                                                        \
          new = pkl_ast_make_offset (PKL_PASS_AST, magnitude,           \
                                     type_unit);                        \
          PKL_AST_TYPE (new) = ASTREF (type);                           \
          PKL_AST_LOC (new) = PKL_AST_LOC (PKL_PASS_NODE);              \
                                                                        \
          pkl_ast_node_free (PKL_PASS_NODE);                            \
          PKL_PASS_NODE = new;                                          \
          PKL_PASS_DONE;                                                \
        }                                                               \
    }                                                                   \
  while (0)

#define OP_BINARY_OOI(OP)                                               \
  do                                                                    \
    {                                                                   \
      pkl_ast_node op1 = PKL_AST_EXP_OPERAND (PKL_PASS_NODE, 0);        \
      pkl_ast_node op2 = PKL_AST_EXP_OPERAND (PKL_PASS_NODE, 1);        \
      pkl_ast_node type = PKL_AST_TYPE (PKL_PASS_NODE);                 \
      pkl_ast_node op1_type = PKL_AST_TYPE (op1);                       \
      pkl_ast_node op2_type = PKL_AST_TYPE (op2);                       \
                                                                        \
      if (PKL_AST_TYPE_CODE (type) == PKL_TYPE_INTEGRAL                 \
          && PKL_AST_TYPE_CODE (op1_type) == PKL_TYPE_OFFSET            \
          && PKL_AST_TYPE_CODE (op2_type) == PKL_TYPE_OFFSET)           \
        {                                                               \
          pkl_ast_node new;                                             \
          pkl_ast_node op1_magnitude, op1_unit;                         \
          pkl_ast_node op2_magnitude, op2_unit;                         \
          uint64_t result;                                              \
          uint64_t op1_magnitude_bits;                                  \
          uint64_t op2_magnitude_bits;                                  \
                                                                        \
          if (PKL_AST_CODE (op1) != PKL_AST_OFFSET                      \
              || PKL_AST_CODE (op2) != PKL_AST_OFFSET)                  \
            /* We cannot fold this expression.  */                      \
            PKL_PASS_DONE;                                              \
                                                                        \
          op1_magnitude = PKL_AST_OFFSET_MAGNITUDE (op1);               \
          op1_unit = PKL_AST_OFFSET_UNIT (op1);                         \
          op2_magnitude = PKL_AST_OFFSET_MAGNITUDE (op2);               \
          op2_unit = PKL_AST_OFFSET_UNIT (op2);                         \
                                                                        \
          if (PKL_AST_CODE (op1_magnitude) != PKL_AST_INTEGER           \
              || PKL_AST_CODE (op1_unit) != PKL_AST_INTEGER             \
              || PKL_AST_CODE (op2_magnitude) != PKL_AST_INTEGER        \
              || PKL_AST_CODE (op2_unit) != PKL_AST_INTEGER)            \
            /* We cannot fold this expression.  */                      \
            PKL_PASS_DONE;                                              \
                                                                        \
          op1_magnitude_bits = (PKL_AST_INTEGER_VALUE (op1_magnitude)   \
                                * PKL_AST_INTEGER_VALUE (op1_unit));    \
          op2_magnitude_bits = (PKL_AST_INTEGER_VALUE (op2_magnitude)   \
                                * PKL_AST_INTEGER_VALUE (op2_unit));    \
                                                                        \
          if (PKL_AST_TYPE_I_SIGNED (type))                             \
            result = emul_s_##OP (op1_magnitude_bits,                   \
                                  op2_magnitude_bits);                  \
          else                                                          \
            result = emul_u_##OP (op1_magnitude_bits,                   \
                                  op2_magnitude_bits);                  \
                                                                        \
          new = pkl_ast_make_integer (PKL_PASS_AST, result);            \
          PKL_AST_TYPE (new) = ASTREF (type);                           \
          PKL_AST_LOC (new) = PKL_AST_LOC (PKL_PASS_NODE);              \
                                                                        \
          pkl_ast_node_free (PKL_PASS_NODE);                            \
          PKL_PASS_NODE = new;                                          \
          PKL_PASS_DONE;                                                \
        }                                                               \
    }                                                                   \
  while (0)

#define OP_BINARY_OOO(OP)                                               \
  do                                                                    \
    {                                                                   \
      pkl_ast_node op1 = PKL_AST_EXP_OPERAND (PKL_PASS_NODE, 0);        \
      pkl_ast_node op2 = PKL_AST_EXP_OPERAND (PKL_PASS_NODE, 1);        \
      pkl_ast_node type = PKL_AST_TYPE (PKL_PASS_NODE);                 \
      pkl_ast_node op1_type = PKL_AST_TYPE (op1);                       \
      pkl_ast_node op2_type = PKL_AST_TYPE (op2);                       \
                                                                        \
      if (PKL_AST_TYPE_CODE (type) == PKL_TYPE_OFFSET                   \
          && PKL_AST_TYPE_CODE (op1_type) == PKL_TYPE_OFFSET            \
          && PKL_AST_TYPE_CODE (op2_type) == PKL_TYPE_OFFSET)           \
        {                                                               \
          pkl_ast_node new;                                             \
          pkl_ast_node type_base_type = PKL_AST_TYPE_O_BASE_TYPE (type);\
          pkl_ast_node type_unit = PKL_AST_TYPE_O_UNIT (type);          \
          pkl_ast_node op1_magnitude, op1_unit;                         \
          pkl_ast_node op2_magnitude, op2_unit;                         \
          pkl_ast_node magnitude;                                       \
          uint64_t result;                                              \
          uint64_t op1_magnitude_bits;                                  \
          uint64_t op2_magnitude_bits;                                  \
                                                                        \
          if (PKL_AST_CODE (op1) != PKL_AST_OFFSET                      \
              || PKL_AST_CODE (op2) != PKL_AST_OFFSET)                  \
            /* We cannot fold this expression.  */                      \
            PKL_PASS_DONE;                                              \
                                                                        \
          op1_magnitude = PKL_AST_OFFSET_MAGNITUDE (op1);               \
          op1_unit = PKL_AST_OFFSET_UNIT (op1);                         \
          op2_magnitude = PKL_AST_OFFSET_MAGNITUDE (op2);               \
          op2_unit = PKL_AST_OFFSET_UNIT (op2);                         \
                                                                        \
          if (PKL_AST_CODE (op1_magnitude) != PKL_AST_INTEGER           \
              || PKL_AST_CODE (op1_unit) != PKL_AST_INTEGER             \
              || PKL_AST_CODE (op2_magnitude) != PKL_AST_INTEGER        \
              || PKL_AST_CODE (op2_unit) != PKL_AST_INTEGER)            \
            /* We cannot fold this expression.  */                      \
            PKL_PASS_DONE;                                              \
                                                                        \
                                                                        \
          op1_magnitude_bits = (PKL_AST_INTEGER_VALUE (op1_magnitude)   \
                                * PKL_AST_INTEGER_VALUE (op1_unit));    \
          op2_magnitude_bits = (PKL_AST_INTEGER_VALUE (op2_magnitude)   \
                                * PKL_AST_INTEGER_VALUE (op2_unit));    \
                                                                        \
          if (PKL_AST_TYPE_I_SIGNED (type_base_type))                   \
            result = emul_s_##OP (op1_magnitude_bits,                   \
                                  op2_magnitude_bits);                  \
          else                                                          \
            result = emul_u_##OP (op1_magnitude_bits,                   \
                                  op2_magnitude_bits);                  \
                                                                        \
          /* Convert bits to the result unit.  */                       \
          assert (PKL_AST_INTEGER_VALUE (type_unit) != 0);              \
          result = result / PKL_AST_INTEGER_VALUE (type_unit);          \
                                                                        \
          magnitude = pkl_ast_make_integer (PKL_PASS_AST, result);      \
          PKL_AST_TYPE (magnitude) = ASTREF (type_base_type);           \
          PKL_AST_LOC (magnitude) = PKL_AST_LOC (PKL_PASS_NODE);        \
                                                                        \
          new = pkl_ast_make_offset (PKL_PASS_AST, magnitude,           \
                                     type_unit);                        \
          PKL_AST_TYPE (new) = ASTREF (type);                           \
          PKL_AST_LOC (new) = PKL_AST_LOC (PKL_PASS_NODE);              \
                                                                        \
          pkl_ast_node_free (PKL_PASS_NODE);                            \
          PKL_PASS_NODE = new;                                          \
          PKL_PASS_DONE;                                                \
        }                                                               \
    }                                                                   \
  while (0)

#define OP_BINARY_SIS(OP)                                               \
  do                                                                    \
    {                                                                   \
      pkl_ast_node new;                                                 \
      pkl_ast_node op1 = PKL_AST_EXP_OPERAND (PKL_PASS_NODE, 0);        \
      pkl_ast_node op2 = PKL_AST_EXP_OPERAND (PKL_PASS_NODE, 1);        \
      pkl_ast_node type = PKL_AST_TYPE (PKL_PASS_NODE);                 \
      pkl_ast_node op1_type = PKL_AST_TYPE (op1);                       \
      pkl_ast_node op2_type = PKL_AST_TYPE (op2);                       \
                                                                        \
      if (PKL_AST_TYPE_CODE (type) == PKL_TYPE_STRING                   \
          && ((PKL_AST_TYPE_CODE (op1_type) == PKL_TYPE_STRING          \
               && PKL_AST_TYPE_CODE (op2_type) == PKL_TYPE_INTEGRAL)    \
              || (PKL_AST_TYPE_CODE (op1_type) == PKL_TYPE_INTEGRAL     \
                  && PKL_AST_TYPE_CODE (op2_type) == PKL_TYPE_STRING))) \
        {                                                               \
          char *result;                                                 \
          pkl_ast_node string_op                                        \
            = (PKL_AST_TYPE_CODE (op1_type) == PKL_TYPE_STRING ?        \
               op1 : op2);                                              \
          pkl_ast_node int_op                                           \
            = (PKL_AST_TYPE_CODE (op1_type) == PKL_TYPE_INTEGRAL ?      \
               op1 : op2);                                              \
                                                                        \
          if (PKL_AST_CODE (string_op) != PKL_AST_STRING                \
              || PKL_AST_CODE (int_op) != PKL_AST_INTEGER)              \
            /* We cannot fold this expression.  */                      \
            PKL_PASS_DONE;                                              \
                                                                        \
          result = emul_##OP (PKL_AST_STRING_POINTER (string_op),       \
                              PKL_AST_INTEGER_VALUE (int_op));          \
                                                                        \
          new = pkl_ast_make_string (PKL_PASS_AST, result);             \
          PKL_AST_TYPE (new) = ASTREF (type);                           \
          PKL_AST_LOC (new) = PKL_AST_LOC (PKL_PASS_NODE);              \
                                                                        \
          pkl_ast_node_free (PKL_PASS_NODE);                            \
          PKL_PASS_NODE = new;                                          \
          PKL_PASS_DONE;                                                \
        }                                                               \
    }                                                                   \
  while (0)

#define OP_BINARY_OIO(OP)                                               \
  do                                                                    \
    {                                                                   \
      pkl_ast_node op1 = PKL_AST_EXP_OPERAND (PKL_PASS_NODE, 0);        \
      pkl_ast_node op2 = PKL_AST_EXP_OPERAND (PKL_PASS_NODE, 1);        \
      pkl_ast_node type = PKL_AST_TYPE (PKL_PASS_NODE);                 \
      pkl_ast_node op1_type = PKL_AST_TYPE (op1);                       \
      pkl_ast_node op2_type = PKL_AST_TYPE (op2);                       \
                                                                        \
      if (PKL_AST_TYPE_CODE (type) == PKL_TYPE_OFFSET                   \
          && ((PKL_AST_TYPE_CODE (op1_type) == PKL_TYPE_OFFSET          \
               && PKL_AST_TYPE_CODE (op2_type) == PKL_TYPE_INTEGRAL)     \
              || (PKL_AST_TYPE_CODE (op1_type) == PKL_TYPE_INTEGRAL     \
                  && PKL_AST_TYPE_CODE (op2_type) == PKL_TYPE_OFFSET))) \
        {                                                               \
          pkl_ast_node off_op                                           \
            = (PKL_AST_TYPE_CODE (op1_type) == PKL_TYPE_OFFSET ?        \
               op1 : op2);                                              \
          pkl_ast_node int_op                                           \
            = (PKL_AST_TYPE_CODE (op1_type) == PKL_TYPE_INTEGRAL ?      \
               op1 : op2);                                              \
                                                                        \
          pkl_ast_node new;                                             \
          pkl_ast_node type_base_type = PKL_AST_TYPE_O_BASE_TYPE (type);\
          pkl_ast_node type_unit = PKL_AST_TYPE_O_UNIT (type);          \
          pkl_ast_node op_type = PKL_AST_TYPE (off_op);                 \
          pkl_ast_node op_magnitude = PKL_AST_OFFSET_MAGNITUDE (off_op); \
          pkl_ast_node op_unit = PKL_AST_OFFSET_UNIT (off_op);          \
          pkl_ast_node magnitude;                                       \
          uint64_t result;                                              \
          uint64_t op_magnitude_bits;                                   \
                                                                        \
          if (PKL_AST_CODE (off_op) != PKL_AST_OFFSET                   \
              || PKL_AST_CODE (int_op) != PKL_AST_INTEGER               \
              || PKL_AST_CODE (op_magnitude) != PKL_AST_INTEGER         \
              || PKL_AST_CODE (op_unit) != PKL_AST_INTEGER)             \
            /* We cannot fold this expression.  */                      \
            PKL_PASS_DONE;                                              \
                                                                        \
          op_magnitude_bits = (PKL_AST_INTEGER_VALUE (op_magnitude)     \
                               * PKL_AST_INTEGER_VALUE (op_unit));      \
                                                                        \
          if (PKL_AST_TYPE_I_SIGNED (op_type))                          \
            result = emul_s_##OP (op_magnitude_bits,                    \
                                  PKL_AST_INTEGER_VALUE (int_op));      \
          else                                                          \
            result = emul_u_##OP (op_magnitude_bits,                    \
                                  PKL_AST_INTEGER_VALUE (int_op));      \
                                                                        \
          /* Convert bits to the result unit.  */                       \
          assert (PKL_AST_INTEGER_VALUE (type_unit) != 0);              \
          result = result / PKL_AST_INTEGER_VALUE (type_unit);          \
                                                                        \
          magnitude = pkl_ast_make_integer (PKL_PASS_AST, result);      \
          PKL_AST_TYPE (magnitude) = ASTREF (type_base_type);           \
          PKL_AST_LOC (magnitude) = PKL_AST_LOC (PKL_PASS_NODE);        \
                                                                        \
          new = pkl_ast_make_offset (PKL_PASS_AST, magnitude,           \
                                     type_unit);                        \
          PKL_AST_TYPE (new) = ASTREF (type);                           \
          PKL_AST_LOC (new) = PKL_AST_LOC (PKL_PASS_NODE);              \
                                                                        \
          pkl_ast_node_free (PKL_PASS_NODE);                            \
          PKL_PASS_NODE = new;                                          \
          PKL_PASS_DONE;                                                \
        }                                                               \
    }                                                                   \
  while (0)

#define OP_BINARY_III(OP)                                               \
  do                                                                    \
    {                                                                   \
      pkl_ast_node op1 = PKL_AST_EXP_OPERAND (PKL_PASS_NODE, 0);        \
      pkl_ast_node op2 = PKL_AST_EXP_OPERAND (PKL_PASS_NODE, 1);        \
      pkl_ast_node type = PKL_AST_TYPE (PKL_PASS_NODE);                 \
      pkl_ast_node op1_type = PKL_AST_TYPE (op1);                       \
      pkl_ast_node op2_type = PKL_AST_TYPE (op2);                       \
                                                                        \
      if (PKL_AST_TYPE_CODE (type) == PKL_TYPE_INTEGRAL                 \
          && PKL_AST_TYPE_CODE (op1_type) == PKL_TYPE_INTEGRAL          \
          && PKL_AST_TYPE_CODE (op2_type) == PKL_TYPE_INTEGRAL)         \
        {                                                               \
          pkl_ast_node new;                                             \
          uint64_t result;                                              \
                                                                        \
          if (PKL_AST_CODE (op1) != PKL_AST_INTEGER                     \
              || PKL_AST_CODE (op2) != PKL_AST_INTEGER)                 \
            /* We cannot fold this expression.  */                      \
            PKL_PASS_DONE;                                              \
                                                                        \
          if (PKL_AST_TYPE_I_SIGNED (type))                             \
            result = emul_s_##OP (PKL_AST_INTEGER_VALUE (op1),          \
                                  PKL_AST_INTEGER_VALUE (op2));         \
          else                                                          \
            result = emul_u_##OP (PKL_AST_INTEGER_VALUE (op1),          \
                                  PKL_AST_INTEGER_VALUE (op2));         \
                                                                        \
          new = pkl_ast_make_integer (PKL_PASS_AST, result);            \
          PKL_AST_TYPE (new) = ASTREF (type);                           \
          PKL_AST_LOC (new) = PKL_AST_LOC (PKL_PASS_NODE);              \
                                                                        \
          pkl_ast_node_free (PKL_PASS_NODE);                            \
          PKL_PASS_NODE = new;                                          \
          PKL_PASS_DONE;                                                \
        }                                                               \
    }                                                                   \
  while (0)

#define OP_BINARY_SSS(OP)                                               \
  do                                                                    \
    {                                                                   \
      pkl_ast_node op1 = PKL_AST_EXP_OPERAND (PKL_PASS_NODE, 0);        \
      pkl_ast_node op2 = PKL_AST_EXP_OPERAND (PKL_PASS_NODE, 1);        \
      pkl_ast_node op1_type = PKL_AST_TYPE (op1);                       \
      pkl_ast_node op2_type = PKL_AST_TYPE (op2);                       \
                                                                        \
      if (PKL_AST_TYPE_CODE (op1_type) == PKL_TYPE_STRING               \
          && PKL_AST_TYPE_CODE (op2_type) == PKL_TYPE_STRING)           \
        {                                                               \
          pkl_ast_node new;                                             \
          char *res;                                                    \
                                                                        \
          if (PKL_AST_CODE (op1) != PKL_AST_STRING                      \
              || PKL_AST_CODE (op2) != PKL_AST_STRING)                  \
            /* We cannot fold this expression.  */                      \
            PKL_PASS_DONE;                                              \
                                                                        \
          res = pk_str_concat (PKL_AST_STRING_POINTER (op1),            \
                               PKL_AST_STRING_POINTER (op2), NULL);     \
          if (!res)                                                     \
            PKL_ICE (PKL_AST_LOC (op1), _("out of memory"));            \
                                                                        \
          new = pkl_ast_make_string (PKL_PASS_AST, res);                \
          free (res);                                                   \
          PKL_AST_TYPE (new) = ASTREF (op1_type);                       \
          PKL_AST_LOC (new) = PKL_AST_LOC (PKL_PASS_NODE);              \
                                                                        \
          pkl_ast_node_free (PKL_PASS_NODE);                            \
          PKL_PASS_NODE = new;                                          \
          PKL_PASS_DONE;                                                \
        }                                                               \
    }                                                                   \
  while (0)

#define OP_BINARY_SSI(OP)                                               \
  do                                                                    \
    {                                                                   \
      pkl_ast_node op1 = PKL_AST_EXP_OPERAND (PKL_PASS_NODE, 0);        \
      pkl_ast_node op2 = PKL_AST_EXP_OPERAND (PKL_PASS_NODE, 1);        \
      pkl_ast_node type = PKL_AST_TYPE (PKL_PASS_NODE);                 \
      pkl_ast_node op1_type = PKL_AST_TYPE (op1);                       \
      pkl_ast_node op2_type = PKL_AST_TYPE (op2);                       \
                                                                        \
      if (PKL_AST_TYPE_CODE (type) == PKL_TYPE_INTEGRAL                 \
          && PKL_AST_TYPE_CODE (op1_type) == PKL_TYPE_STRING            \
          && PKL_AST_TYPE_CODE (op2_type) == PKL_TYPE_STRING)           \
        {                                                               \
          pkl_ast_node new;                                             \
                                                                        \
          if (PKL_AST_CODE (op1) != PKL_AST_STRING                      \
              || PKL_AST_CODE (op2) != PKL_AST_STRING)                  \
            /* We cannot fold this expression.  */                      \
            PKL_PASS_DONE;                                              \
                                                                        \
          new = pkl_ast_make_integer (PKL_PASS_AST,                     \
                                      emul_s_##OP (PKL_AST_STRING_POINTER (op1), \
                                                   PKL_AST_STRING_POINTER (op2))); \
          PKL_AST_TYPE (new) = ASTREF (type);                           \
          PKL_AST_LOC (new) = PKL_AST_LOC (PKL_PASS_NODE);              \
                                                                        \
          pkl_ast_node_free (PKL_PASS_NODE);                            \
          PKL_PASS_NODE = new;                                          \
          PKL_PASS_DONE;                                                \
        }                                                               \
    }                                                                   \
  while (0)

/* Handlers for the several expression codes.  */

#define PKL_PHASE_HANDLER_UNA_INT(OP)           \
  PKL_PHASE_BEGIN_HANDLER (pkl_fold_##OP)       \
  {                                             \
    OP_UNARY_II (OP);                           \
  }                                             \
  PKL_PHASE_END_HANDLER

PKL_PHASE_HANDLER_UNA_INT (not);

#define PKL_PHASE_HANDLER_UNA(OP)               \
  PKL_PHASE_BEGIN_HANDLER (pkl_fold_##OP)       \
  {                                             \
    OP_UNARY_II (OP);                           \
    OP_UNARY_OO (OP##o);                        \
  }                                             \
  PKL_PHASE_END_HANDLER

PKL_PHASE_HANDLER_UNA (pos);
PKL_PHASE_HANDLER_UNA (neg);
PKL_PHASE_HANDLER_UNA (bnot);

#define PKL_PHASE_HANDLER_BIN_LOGIC(OP)              \
  PKL_PHASE_BEGIN_HANDLER (pkl_fold_##OP)            \
  {                                                  \
    OP_BINARY_III (OP);                              \
  }                                                  \
  PKL_PHASE_END_HANDLER

PKL_PHASE_HANDLER_BIN_LOGIC (or);
PKL_PHASE_HANDLER_BIN_LOGIC (and);

#define PKL_PHASE_HANDLER_BIN_INTOFF(OP)             \
  PKL_PHASE_BEGIN_HANDLER (pkl_fold_##OP)            \
  {                                                  \
    OP_BINARY_III (OP);                              \
    OP_BINARY_OOO (OP);                              \
  }                                                  \
  PKL_PHASE_END_HANDLER

PKL_PHASE_HANDLER_BIN_INTOFF (ior);
PKL_PHASE_HANDLER_BIN_INTOFF (xor);
PKL_PHASE_HANDLER_BIN_INTOFF (band);

#define PKL_PHASE_HANDLER_BIN_BSHIFT(OP)             \
  PKL_PHASE_BEGIN_HANDLER (pkl_fold_##OP)            \
  {                                                  \
    OP_BINARY_III (OP);                              \
    OP_BINARY_OIO (OP##o);                           \
  }                                                  \
  PKL_PHASE_END_HANDLER

PKL_PHASE_HANDLER_BIN_BSHIFT (sr);
PKL_PHASE_HANDLER_BIN_BSHIFT (sl);

PKL_PHASE_BEGIN_HANDLER (pkl_fold_pow)
{
  OP_BINARY_III (pow);

  /* Handle OFFSET ** UINT -> OFFSET.  This is similar to
     OP_BINARY_OIO, but the offset magnitude is not converted to bits.
     This is to avoid overflow.  */
  {
    pkl_ast_node op1 = PKL_AST_EXP_OPERAND (PKL_PASS_NODE, 0);
    pkl_ast_node op2 = PKL_AST_EXP_OPERAND (PKL_PASS_NODE, 1);
    pkl_ast_node type = PKL_AST_TYPE (PKL_PASS_NODE);
    pkl_ast_node op1_type = PKL_AST_TYPE (op1);
    pkl_ast_node op2_type = PKL_AST_TYPE (op2);

    if (PKL_AST_TYPE_CODE (type) == PKL_TYPE_OFFSET
        && ((PKL_AST_TYPE_CODE (op1_type) == PKL_TYPE_OFFSET
             && PKL_AST_TYPE_CODE (op2_type) == PKL_TYPE_INTEGRAL)
            || (PKL_AST_TYPE_CODE (op1_type) == PKL_TYPE_INTEGRAL
                && PKL_AST_TYPE_CODE (op2_type) == PKL_TYPE_OFFSET)))
      {
        pkl_ast_node off_op
          = (PKL_AST_TYPE_CODE (op1_type) == PKL_TYPE_OFFSET ?
             op1 : op2);
        pkl_ast_node int_op
          = (PKL_AST_TYPE_CODE (op1_type) == PKL_TYPE_INTEGRAL ?
             op1 : op2);

        pkl_ast_node new;
        pkl_ast_node type_base_type = PKL_AST_TYPE_O_BASE_TYPE (type);
        pkl_ast_node type_unit = PKL_AST_TYPE_O_UNIT (type);
        pkl_ast_node op_type = PKL_AST_TYPE (off_op);
        pkl_ast_node op_magnitude = PKL_AST_OFFSET_MAGNITUDE (off_op);
        pkl_ast_node op_unit = PKL_AST_OFFSET_UNIT (off_op);
        pkl_ast_node magnitude;
        uint64_t result;

        if (PKL_AST_CODE (off_op) != PKL_AST_OFFSET
            || PKL_AST_CODE (int_op) != PKL_AST_INTEGER
            || PKL_AST_CODE (op_magnitude) != PKL_AST_INTEGER
            || PKL_AST_CODE (op_unit) != PKL_AST_INTEGER)
          /* We cannot fold this expression.  */
          PKL_PASS_DONE;

        if (PKL_AST_TYPE_I_SIGNED (op_type))
          result = emul_s_powo (PKL_AST_INTEGER_VALUE (op_magnitude),
                                PKL_AST_INTEGER_VALUE (int_op));
        else
          result = emul_u_powo (PKL_AST_INTEGER_VALUE (op_magnitude),
                                PKL_AST_INTEGER_VALUE (int_op));

        magnitude = pkl_ast_make_integer (PKL_PASS_AST, result);
        PKL_AST_TYPE (magnitude) = ASTREF (type_base_type);
        PKL_AST_LOC (magnitude) = PKL_AST_LOC (PKL_PASS_NODE);

        new = pkl_ast_make_offset (PKL_PASS_AST, magnitude,
                                   type_unit);
        PKL_AST_TYPE (new) = ASTREF (type);
        PKL_AST_LOC (new) = PKL_AST_LOC (PKL_PASS_NODE);

        pkl_ast_node_free (PKL_PASS_NODE);
        PKL_PASS_NODE = new;
        PKL_PASS_DONE;
      }
  }
}
PKL_PHASE_END_HANDLER

#define PKL_PHASE_HANDLER_BIN_RELA(OP)               \
  PKL_PHASE_BEGIN_HANDLER (pkl_fold_##OP)            \
  {                                                  \
    OP_BINARY_III (OP);                              \
    OP_BINARY_OOI (OP##o);                           \
    OP_BINARY_SSI (OP##s);                           \
  }                                                  \
  PKL_PHASE_END_HANDLER

PKL_PHASE_HANDLER_BIN_RELA (eq);
PKL_PHASE_HANDLER_BIN_RELA (ne);
PKL_PHASE_HANDLER_BIN_RELA (lt);
PKL_PHASE_HANDLER_BIN_RELA (gt);
PKL_PHASE_HANDLER_BIN_RELA (le);
PKL_PHASE_HANDLER_BIN_RELA (ge);

#define PKL_PHASE_HANDLER_BIN_ARITH(OP)              \
  PKL_PHASE_BEGIN_HANDLER (pkl_fold_##OP)            \
  {                                                  \
    OP_BINARY_III (OP);                              \
    OP_BINARY_OOO (OP##o);                           \
    OP_BINARY_SSS (OP);                              \
  }                                                  \
  PKL_PHASE_END_HANDLER

PKL_PHASE_HANDLER_BIN_ARITH (add);
PKL_PHASE_HANDLER_BIN_ARITH (sub);

PKL_PHASE_BEGIN_HANDLER (pkl_fold_gcd)
{
  OP_BINARY_III (gcd);
}
PKL_PHASE_END_HANDLER

PKL_PHASE_BEGIN_HANDLER (pkl_fold_mul)
{
  OP_BINARY_III (mul);
  OP_BINARY_OIO (mulo);
  OP_BINARY_SIS (muls);
}
PKL_PHASE_END_HANDLER

PKL_PHASE_BEGIN_HANDLER (pkl_fold_div)
{
  pkl_ast_node op2 = PKL_AST_EXP_OPERAND (PKL_PASS_NODE, 1);
  pkl_ast_node op2_type = PKL_AST_TYPE (op2);

  if (PKL_AST_TYPE_CODE (op2_type) == PKL_TYPE_INTEGRAL
      && PKL_AST_CODE (op2) == PKL_AST_INTEGER
      && PKL_AST_INTEGER_VALUE (op2) == 0)
    goto divbyzero;

  if (PKL_AST_TYPE_CODE (op2_type) == PKL_TYPE_OFFSET
      && PKL_AST_CODE (op2) == PKL_AST_OFFSET)
    {
      pkl_ast_node magnitude = PKL_AST_OFFSET_MAGNITUDE (op2);

      if (PKL_AST_CODE (magnitude) == PKL_AST_INTEGER
          && PKL_AST_INTEGER_VALUE (magnitude) == 0)
        goto divbyzero;
    }

  OP_BINARY_III (div);
  OP_BINARY_OOI (divo);

  PKL_PASS_DONE;

 divbyzero:
  PKL_ERROR (PKL_AST_LOC (op2), "division by zero");
  PKL_FOLD_PAYLOAD->errors++;
  PKL_PASS_ERROR;
}
PKL_PHASE_END_HANDLER

PKL_PHASE_BEGIN_HANDLER (pkl_fold_cdiv)
{
  pkl_ast_node op2 = PKL_AST_EXP_OPERAND (PKL_PASS_NODE, 1);
  pkl_ast_node op2_type = PKL_AST_TYPE (op2);

  if (PKL_AST_TYPE_CODE (op2_type) == PKL_TYPE_INTEGRAL
      && PKL_AST_CODE (op2) == PKL_AST_INTEGER
      && PKL_AST_INTEGER_VALUE (op2) == 0)
    goto divbyzero;

  if (PKL_AST_TYPE_CODE (op2_type) == PKL_TYPE_OFFSET
      && PKL_AST_CODE (op2) == PKL_AST_OFFSET)
    {
      pkl_ast_node magnitude = PKL_AST_OFFSET_MAGNITUDE (op2);

      if (PKL_AST_CODE (magnitude) == PKL_AST_INTEGER
          && PKL_AST_INTEGER_VALUE (magnitude) == 0)
        goto divbyzero;
    }

  OP_BINARY_III (cdiv);
  OP_BINARY_OOI (cdivo);

  PKL_PASS_DONE;

 divbyzero:
  PKL_ERROR (PKL_AST_LOC (op2), "division by zero");
  PKL_FOLD_PAYLOAD->errors++;
  PKL_PASS_ERROR;
}
PKL_PHASE_END_HANDLER


PKL_PHASE_BEGIN_HANDLER (pkl_fold_mod)
{
  pkl_ast_node op2 = PKL_AST_EXP_OPERAND (PKL_PASS_NODE, 1);
  pkl_ast_node op2_type = PKL_AST_TYPE (op2);

  if (PKL_AST_TYPE_CODE (op2_type) == PKL_TYPE_INTEGRAL
      && PKL_AST_CODE (op2) == PKL_AST_INTEGER
      && PKL_AST_INTEGER_VALUE (op2) == 0)
    goto divbyzero;

  if (PKL_AST_TYPE_CODE (op2_type) == PKL_TYPE_OFFSET
      && PKL_AST_CODE (op2) == PKL_AST_OFFSET)
    {
      pkl_ast_node magnitude = PKL_AST_OFFSET_MAGNITUDE (op2);

      if (PKL_AST_CODE (magnitude) == PKL_AST_INTEGER
          && PKL_AST_INTEGER_VALUE (magnitude) == 0)
        goto divbyzero;
    }

  OP_BINARY_III (mod);
  OP_BINARY_OOO (modo);

  PKL_PASS_DONE;

 divbyzero:
  PKL_ERROR (PKL_AST_LOC (op2), "division by zero");
  PKL_FOLD_PAYLOAD->errors++;
  PKL_PASS_ERROR;
}
PKL_PHASE_END_HANDLER

PKL_PHASE_BEGIN_HANDLER (pkl_fold_bconc)
{
  pkl_ast_node op1 = PKL_AST_EXP_OPERAND (PKL_PASS_NODE, 0);
  pkl_ast_node op2 = PKL_AST_EXP_OPERAND (PKL_PASS_NODE, 1);
  pkl_ast_node op1_type = PKL_AST_TYPE (op1);
  pkl_ast_node op2_type = PKL_AST_TYPE (op2);
  pkl_ast_node type = PKL_AST_TYPE (PKL_PASS_NODE);
  pkl_ast_node new;

  uint64_t result;

  assert (PKL_AST_TYPE_CODE (type) == PKL_TYPE_INTEGRAL
          && PKL_AST_TYPE_CODE (op1_type) == PKL_TYPE_INTEGRAL
          && PKL_AST_TYPE_CODE (op2_type) == PKL_TYPE_INTEGRAL);

  if (PKL_AST_CODE (op1) != PKL_AST_INTEGER
      || PKL_AST_CODE (op2) != PKL_AST_INTEGER)
    /* We cannot fold this expression.  */
    PKL_PASS_DONE;

  result = ((PKL_AST_INTEGER_VALUE (op1) << PKL_AST_TYPE_I_SIZE (op2_type))
            | PKL_AST_INTEGER_VALUE (op2));

  new = pkl_ast_make_integer (PKL_PASS_AST, result);
  PKL_AST_TYPE (new) = ASTREF (type);
  PKL_AST_LOC (new) = PKL_AST_LOC (PKL_PASS_NODE);

  pkl_ast_node_free (PKL_PASS_NODE);
  PKL_PASS_NODE = new;
}
PKL_PHASE_END_HANDLER

#define PKL_PHASE_HANDLER_UNIMPL(op)            \
  PKL_PHASE_BEGIN_HANDLER (pkl_fold_##op)       \
  {                                             \
    /* WRITEME */                               \
  }                                             \
  PKL_PHASE_END_HANDLER

PKL_PHASE_BEGIN_HANDLER (pkl_fold_ps_cast)
{
  pkl_ast_node cast = PKL_PASS_NODE;
  pkl_ast_node exp = PKL_AST_CAST_EXP (cast);
  pkl_ast_node from_type = PKL_AST_TYPE (exp);
  pkl_ast_node to_type = PKL_AST_CAST_TYPE (cast);

  pkl_ast_node new = NULL;

  if (PKL_AST_TYPE_CODE (from_type) == PKL_TYPE_INTEGRAL
      && PKL_AST_TYPE_CODE (to_type) == PKL_TYPE_INTEGRAL
      && PKL_AST_CODE (exp) == PKL_AST_INTEGER)
    {
      int size = PKL_AST_TYPE_I_SIZE (to_type);
      uint64_t mask = size < 64 ? (1LLU << size) - 1 : 0LLU - 1;

      new = pkl_ast_make_integer (PKL_PASS_AST,
                                  PKL_AST_INTEGER_VALUE (exp) & mask);
    }
  else if (PKL_AST_TYPE_CODE (from_type) == PKL_TYPE_OFFSET
           && PKL_AST_TYPE_CODE (to_type) == PKL_TYPE_OFFSET
           && PKL_AST_CODE (exp) == PKL_AST_OFFSET)
    {
      pkl_ast_node magnitude = PKL_AST_OFFSET_MAGNITUDE (exp);
      pkl_ast_node unit = PKL_AST_OFFSET_UNIT (exp);
      pkl_ast_node to_unit = PKL_AST_TYPE_O_UNIT (to_type);
      pkl_ast_node from_base_type = PKL_AST_TYPE_O_BASE_TYPE (from_type);
      pkl_ast_node to_base_type = PKL_AST_TYPE_O_BASE_TYPE (to_type);

      if (PKL_AST_CODE (magnitude) != PKL_AST_INTEGER
          || PKL_AST_CODE (unit) != PKL_AST_INTEGER
          || PKL_AST_CODE (to_unit) != PKL_AST_INTEGER)
        /* We can't fold this cast.  */
        PKL_PASS_DONE;

      /* Transform magnitude to bits.  */
      PKL_AST_INTEGER_VALUE (magnitude)
        = (PKL_AST_INTEGER_VALUE (magnitude) *
           PKL_AST_INTEGER_VALUE (unit));

      /* Calculate the new unit.  It should always be a new node,
         since otherwise we may be chaning an integer node that is
         also part of an unit declaration, or who knows what.  */
      {
        pkl_ast_node unit_type = PKL_AST_TYPE (unit);
        pkl_ast_node new_unit
          =  pkl_ast_make_integer (PKL_PASS_AST,
                                   PKL_AST_INTEGER_VALUE (to_unit));

        PKL_AST_TYPE (new_unit) = ASTREF (unit_type);
        PKL_AST_LOC (new_unit) = PKL_AST_LOC (unit);
        unit = new_unit;
      }

      /* We may need to create a new magnitude node, if the base type
         is different.  */
      if (!pkl_ast_type_equal (from_base_type, to_base_type))
        {
          int size = PKL_AST_TYPE_I_SIZE (to_base_type);
          uint64_t mask = size < 64 ? (1LLU << size) -1 : 0LLU - 1;

          magnitude = pkl_ast_make_integer (PKL_PASS_AST,
                                            PKL_AST_INTEGER_VALUE (magnitude) & mask);
          PKL_AST_TYPE (magnitude) = ASTREF (to_base_type);
          PKL_AST_LOC (magnitude) = PKL_AST_LOC (cast);
        }

      /* Transform magnitude to new unit.  */
      PKL_AST_INTEGER_VALUE (magnitude)
        = (PKL_AST_INTEGER_VALUE (magnitude)
           /  PKL_AST_INTEGER_VALUE (unit));

      new = pkl_ast_make_offset (PKL_PASS_AST,
                                 magnitude, unit);
    }
  else
    PKL_PASS_DONE;

  /* XXX handle array casts.  */

  /* `new' is the node to replace the cast.  */
  PKL_AST_TYPE (new) = ASTREF (to_type);
  PKL_AST_LOC (new) = PKL_AST_LOC (exp);
  pkl_ast_node_free (cast);
  PKL_PASS_NODE = new;
}
PKL_PHASE_END_HANDLER

/* If the condition expression of a conditional expression is
   constant, we can fold it into either the then-expression or the
   else-expression, depending on its value.  */

PKL_PHASE_BEGIN_HANDLER (pkl_fold_ps_cond_exp)
{
  pkl_ast_node cond_exp = PKL_PASS_NODE;
  pkl_ast_node cond = PKL_AST_COND_EXP_COND (cond_exp);

  if (PKL_AST_CODE (cond) == PKL_AST_INTEGER)
    {
      pkl_ast_node t;
      pkl_ast_node replacement_node
        = (PKL_AST_INTEGER_VALUE (cond)
           ? PKL_AST_COND_EXP_THENEXP (cond_exp)
           : PKL_AST_COND_EXP_ELSEEXP (cond_exp));

      t = PKL_PASS_NODE;
      PKL_PASS_NODE = ASTREF (replacement_node);
      pkl_ast_node_free (t);
    }
}
PKL_PHASE_END_HANDLER

/* If the container indexed (either an array or a string) is constant,
   and the indexing expression is also constant, then we can fold it
   into the referred element.  */

PKL_PHASE_BEGIN_HANDLER (pkl_fold_ps_indexer)
{
  pkl_ast_node indexer = PKL_PASS_NODE;
  pkl_ast_node container = PKL_AST_INDEXER_ENTITY (indexer);
  pkl_ast_node index = PKL_AST_INDEXER_INDEX (indexer);

  if (PKL_AST_CODE (index) == PKL_AST_INTEGER)
    {
      int64_t index_value = PKL_AST_INTEGER_VALUE (index);

      switch (PKL_AST_CODE (container))
        {
        case PKL_AST_STRING:
          {
            pkl_ast_node t, new, new_type;
            char *str = PKL_AST_STRING_POINTER (container);

            /* Check that the index is on bounds.  */
            if (index_value < 0
                || index_value >= strlen (str))
              {
                PKL_ERROR (PKL_AST_LOC (index),
                           "index is out of bounds of string");
                PKL_PASS_ERROR;
              }

            /* fold the indexer into the referred element, which is an
               uint<8> with the value of the corresponding character
               in the string.  */
            new_type = pkl_ast_make_integral_type (PKL_PASS_AST, 8, 0);
            new = pkl_ast_make_integer (PKL_PASS_AST, str[index_value]);

            PKL_AST_LOC (new_type) = PKL_AST_LOC (index);
            PKL_AST_LOC (new) = PKL_AST_LOC (index);
            PKL_AST_TYPE (new) = ASTREF (new_type);

            t = PKL_PASS_NODE;
            PKL_PASS_NODE = ASTREF (new);
            pkl_ast_node_free (t);
            break;
          }
        case PKL_AST_ARRAY:
          {
            pkl_ast_node t, elem = NULL;

            /* Look for the referred element in the array
               initializers.  */
            for (t = PKL_AST_ARRAY_INITIALIZERS (container);
                 t;
                 t = PKL_AST_CHAIN (t))
              {
                pkl_ast_node initializer_index
                  = PKL_AST_ARRAY_INITIALIZER_INDEX (t);
                uint64_t initializer_index_value;

                assert (PKL_AST_CODE (initializer_index) == PKL_AST_INTEGER);
                initializer_index_value
                  = PKL_AST_INTEGER_VALUE (initializer_index);

                if (index_value <= initializer_index_value)
                  {
                    elem = PKL_AST_ARRAY_INITIALIZER_EXP (t);
                    break;
                  }
              }

            /* Check that the index is on bounds.  */
            if (elem == NULL)
              {
                PKL_ERROR (PKL_AST_LOC (index),
                           "index is out of bounds of array");
                PKL_PASS_ERROR;
              }

            t = PKL_PASS_NODE;
            PKL_PASS_NODE = ASTREF (elem);
            pkl_ast_node_free (t);
            break;
          }
        default:
          break;
        }
    }
}
PKL_PHASE_END_HANDLER

struct pkl_phase pkl_phase_fold
  __attribute__ ((visibility ("hidden"))) =
  {
   PKL_PHASE_PR_HANDLER (PKL_AST_TYPE, pkl_fold_pr_type),
   PKL_PHASE_PS_HANDLER (PKL_AST_CAST, pkl_fold_ps_cast),
   PKL_PHASE_PS_HANDLER (PKL_AST_INDEXER, pkl_fold_ps_indexer),
   PKL_PHASE_PS_HANDLER (PKL_AST_COND_EXP, pkl_fold_ps_cond_exp),
#define ENTRY(ops, fs)\
   PKL_PHASE_PS_OP_HANDLER (PKL_AST_OP_##ops, pkl_fold_##fs)

   ENTRY (OR, or), ENTRY (IOR, ior), ENTRY (ADD, add),
   ENTRY (XOR, xor), ENTRY (AND, and), ENTRY (BAND, band),
   ENTRY (EQ, eq), ENTRY (NE, ne), ENTRY (SL, sl),
   ENTRY (SR, sr), ENTRY (ADD, add), ENTRY (SUB, sub),
   ENTRY (MUL, mul), ENTRY (DIV, div), ENTRY (CEILDIV, cdiv),
   ENTRY (MOD, mod), ENTRY (GCD, gcd),
   ENTRY (LT, lt), ENTRY (GT, gt), ENTRY (LE, le),
   ENTRY (GE, ge),
   ENTRY (BCONC, bconc),
   ENTRY (POS, pos), ENTRY (NEG, neg), ENTRY (BNOT, bnot),
   ENTRY (NOT, not), ENTRY (POW, pow),
#undef ENTRY
  };
