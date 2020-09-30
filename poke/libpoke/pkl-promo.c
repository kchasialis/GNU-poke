/* pkl-promo.c - Operand promotion phase for the poke compiler.  */

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

/* This file implements a compiler phase that promotes the operands of
   expressions following language rules.  This phase expects that
   every expression operand is anotated with its proper type.  */

#include <config.h>

#include "pk-utils.h"

#include "pkl.h"
#include "pkl-ast.h"
#include "pkl-pass.h"

/* Note the following macro evaluates the arguments twice!  */
#define MAX(A,B) ((A) > (B) ? (A) : (B))

/* Promote a given node A to an integral type of width SIZE and sign
   SIGN, if possible.  Put the resulting node in A.  Return 1 if the
   promotion was successful, 0 otherwise.  */

static int
promote_integral (pkl_ast ast,
                  size_t size, int sign,
                  pkl_ast_node *a,
                  int *restart)
{
  pkl_ast_node type = PKL_AST_TYPE (*a);

  /* Support promoting integral structs.  */
  if (PKL_AST_TYPE_CODE (type) == PKL_TYPE_STRUCT
      && PKL_AST_TYPE_S_ITYPE (type))
    type = PKL_AST_TYPE_S_ITYPE (type);

  *restart = 0;
  if (PKL_AST_TYPE_CODE (type) == PKL_TYPE_INTEGRAL)
    {
      if (PKL_AST_TYPE_I_SIZE (type) != size
          || PKL_AST_TYPE_I_SIGNED_P (type) != sign
          || PKL_AST_TYPE_CODE (PKL_AST_TYPE (*a)) == PKL_TYPE_STRUCT)
        {
          pkl_ast_node desired_type
            = pkl_ast_make_integral_type (ast, size, sign);
          pkl_ast_loc loc = PKL_AST_LOC (*a);

          *a = pkl_ast_make_cast (ast, desired_type, ASTDEREF (*a));
          PKL_AST_TYPE (*a) = ASTREF (desired_type);
          PKL_AST_LOC (*a) = loc;
          PKL_AST_LOC (desired_type) = loc;
          ASTREF (*a);
          *restart = 1;
        }

      return 1;
    }

  return 0;
}

/* Promote a given node A, which should be an offset, to an offset
   type featuring SIZE, SIGN and UNIT.  Put the resulting node in A.
   Return 1 if the promotion was successful, 0 otherwise.  */

static int
promote_offset (pkl_ast ast,
                size_t size, int sign,
                pkl_ast_node unit,
                pkl_ast_node *a,
                int *restart)
{
  pkl_ast_node a_type = PKL_AST_TYPE (*a);

  *restart = 0;
  if (PKL_AST_TYPE_CODE (a_type) == PKL_TYPE_OFFSET)
    {
      pkl_ast_node a_type_base_type = PKL_AST_TYPE_O_BASE_TYPE (a_type);
      pkl_ast_node a_type_unit = PKL_AST_TYPE_O_UNIT (a_type);

      size_t a_type_base_type_size = PKL_AST_TYPE_I_SIZE (a_type_base_type);
      int a_type_base_type_sign = PKL_AST_TYPE_I_SIGNED_P (a_type_base_type);

      int different_units = 1;

      /* If the offset units happen to be integer nodes, we can
         determine whether they are equal right away.  */
      if (PKL_AST_CODE (a_type_unit) == PKL_AST_INTEGER
          && PKL_AST_CODE (unit) == PKL_AST_INTEGER
          && (PKL_AST_INTEGER_VALUE (a_type_unit)
              == PKL_AST_INTEGER_VALUE (unit)))
        different_units = 0;

      if (a_type_base_type_size != size
          || a_type_base_type_sign != sign
          || different_units)
        {
          pkl_ast_loc loc = PKL_AST_LOC (*a);
          pkl_ast_node base_type
            = pkl_ast_make_integral_type (ast, size, sign);
          pkl_ast_node unit_type
            = pkl_ast_make_integral_type (ast, 64, 0);
          pkl_ast_node type
            = pkl_ast_make_offset_type (ast, base_type, unit);

          PKL_AST_TYPE (unit) = ASTREF (unit_type);
          PKL_AST_LOC (base_type) = loc;
          PKL_AST_LOC (unit_type) = loc;
          PKL_AST_LOC (type) = loc;

          *a = pkl_ast_make_cast (ast, type, ASTDEREF (*a));
          PKL_AST_TYPE (*a) = ASTREF (type);
          PKL_AST_LOC (*a) = loc;
          ASTREF (*a);
          *restart = 1;
        }

      return 1;
    }

  return 0;
}

/* Promote a given node A, which should be an array expression, to the
   given array type.  Put the resulting node in A.  Return 1 if the
   promotion was successful, 0 otherwise.  */

static int
promote_array (pkl_ast ast,
               pkl_ast_node type,
               pkl_ast_node *a,
               int *restart)
{
  /* XXX this should be, somehow, recursive, to handle arrays of
     arrays... */

  pkl_ast_node from_type = PKL_AST_TYPE (*a);
  pkl_ast_node etype = PKL_AST_TYPE_A_ETYPE (type);
  pkl_ast_node bound = PKL_AST_TYPE_A_BOUND (type);
  pkl_ast_node from_bound = PKL_AST_TYPE_A_BOUND (from_type);

  *restart = 0;

  /* Note that at this point it is assured (by typify1) that the array
     element types of both arrays are equivalent.  XXX but not really,
     because the bounds in contained arrays may be different!!  We
     need a strict version of type_equal for arrays.  */

  /* Promotions to any[] do not require any explicit action.  */
  if (PKL_AST_TYPE_CODE (etype) == PKL_TYPE_ANY)
    return 1;

  /* The case of static array types (bounded by constant number of
     elements) is handled in type equivalence.  No cast is needed.  */
  if (bound && PKL_AST_CODE (bound) == PKL_AST_INTEGER)
    return 1;

  /* Unbounded array to unbounded array doesn't require of any
     cast.  */
  if (!bound && !from_bound)
    return 1;

  /* Any other promotion requires a cast.  Note that this includes
     unbounded arrays, for which run-time type change should be
     performed even if there is not an actual check.  */
  {
    pkl_ast_loc loc = PKL_AST_LOC (*a);

    *a = pkl_ast_make_cast (ast, type, ASTDEREF (*a));
    PKL_AST_TYPE (*a) = ASTREF (type);
    PKL_AST_LOC (*a) = loc;
    ASTREF (*a);
    *restart = 1;
    return 1;
  }

  return 0;
}

/* Division is defined on the following configurations of operands and
   result types:

      INTEGRAL / INTEGRAL -> INTEGRAL
      OFFSET   / OFFSET   -> INTEGRAL

   In the I / I -> I configuration, the types of the operands are
   promoted to match the type of the result, if needed.

   In the O / O -> I configuration, the magnitude types of the offset
   operands are promoted to match the type of the integral result, if
   needed.  The units are normalized to bits.  */

PKL_PHASE_BEGIN_HANDLER (pkl_promo_ps_op_div)
{
  pkl_ast_node exp = PKL_PASS_NODE;
  pkl_ast_node exp_type = PKL_AST_TYPE (exp);
  size_t size = PKL_AST_TYPE_I_SIZE (exp_type);
  int signed_p = PKL_AST_TYPE_I_SIGNED_P (exp_type);
  pkl_ast_node op1 = PKL_AST_EXP_OPERAND (exp, 0);
  pkl_ast_node op1_type = PKL_AST_TYPE (op1);
  pkl_ast_node op2 = PKL_AST_EXP_OPERAND (exp, 1);
  pkl_ast_node op2_type = PKL_AST_TYPE (op2);

  /* Support promoting integral structs.  */
  if (PKL_AST_TYPE_CODE (op1_type) == PKL_TYPE_STRUCT
      && PKL_AST_TYPE_S_ITYPE (op1_type))
    op1_type = PKL_AST_TYPE_S_ITYPE (op1_type);

  if (PKL_AST_TYPE_CODE (op2_type) == PKL_TYPE_STRUCT
      && PKL_AST_TYPE_S_ITYPE (op2_type))
    op2_type = PKL_AST_TYPE_S_ITYPE (op2_type);

  /* Note we discriminate on the first operand type in order to
     distinguish between configurations.  */
  switch (PKL_AST_TYPE_CODE (op1_type))
    {
    case PKL_TYPE_INTEGRAL:
      {
        int restart1, restart2;

        if (!promote_integral (PKL_PASS_AST, size, signed_p,
                               &PKL_AST_EXP_OPERAND (exp, 0), &restart1))
          goto error;

        if (!promote_integral (PKL_PASS_AST, size, signed_p,
                               &PKL_AST_EXP_OPERAND (exp, 1), &restart2))
          goto error;

        PKL_PASS_RESTART = restart1 || restart2;
        break;
      }
    case PKL_TYPE_OFFSET:
      {
        int restart1, restart2;

        pkl_ast_node op1_base_type = PKL_AST_TYPE_O_BASE_TYPE (op1_type);
        pkl_ast_node op2_base_type = PKL_AST_TYPE_O_BASE_TYPE (op2_type);

        size_t size = MAX (PKL_AST_TYPE_I_SIZE (op1_base_type),
                           PKL_AST_TYPE_I_SIZE (op2_base_type));
        int signed_p = (PKL_AST_TYPE_I_SIGNED_P (op1_base_type)
                        && PKL_AST_TYPE_I_SIGNED_P (op2_base_type));

        pkl_ast_node unit_bit = pkl_ast_make_integer (PKL_PASS_AST, 1);

        ASTREF (unit_bit);
        PKL_AST_LOC (unit_bit) = PKL_AST_LOC (exp);

        if (!promote_offset (PKL_PASS_AST,
                             size, signed_p, unit_bit,
                             &PKL_AST_EXP_OPERAND (exp, 0), &restart1))
          goto error;

        if (!promote_offset (PKL_PASS_AST,
                             size, signed_p, unit_bit,
                             &PKL_AST_EXP_OPERAND (exp, 1), &restart2))
          goto error;

        pkl_ast_node_free (unit_bit);

        PKL_PASS_RESTART = restart1 || restart2;
        break;
      }
    default:
      goto error;
    }

  PKL_PASS_DONE;

 error:
  PKL_ICE (PKL_AST_LOC (exp),
           "couldn't promote operands of expression #%" PRIu64,
           PKL_AST_UID (exp));
  PKL_PASS_ERROR;
}
PKL_PHASE_END_HANDLER

/* Addition, subtraction, modulus, and the bitwise binary operators
   are defined on the following configurations of operand and result
   types:

      INTEGRAL x INTEGRAL -> INTEGRAL
      OFFSET   x OFFSET   -> OFFSET

   In the I x I -> I configuration, the types of the operands are
   promoted to match the type of the result, if needed.

   In the O x O -> O configuration, the magnitude types of the offset
   operands are promoted to match the type of the magnitude type of
   the result offset, if needed.

   Also, addition is used to concatenate strings and arrays:

      STRING x STRING -> STRING
      ARRAY  x ARRAY  -> ARRAY

   In these configurations no promotions are done.  */

PKL_PHASE_BEGIN_HANDLER (pkl_promo_ps_op_binary_intoffstrarr)
{
  pkl_ast_node exp = PKL_PASS_NODE;
  pkl_ast_node type = PKL_AST_TYPE (exp);

  switch (PKL_AST_TYPE_CODE (type))
    {
    case PKL_TYPE_INTEGRAL:
      {
        int restart1, restart2;

        size_t size = PKL_AST_TYPE_I_SIZE (type);
        int signed_p = PKL_AST_TYPE_I_SIGNED_P (type);

        if (!promote_integral (PKL_PASS_AST, size, signed_p,
                               &PKL_AST_EXP_OPERAND (exp, 0), &restart1))
          goto error;

        if (!promote_integral (PKL_PASS_AST, size, signed_p,
                               &PKL_AST_EXP_OPERAND (exp, 1), &restart2))
          goto error;

        PKL_PASS_RESTART = restart1 || restart2;
        break;
      }
    case PKL_TYPE_OFFSET:
      {
        int restart1, restart2;

        pkl_ast_node base_type = PKL_AST_TYPE_O_BASE_TYPE (type);
        pkl_ast_node unit = PKL_AST_TYPE_O_UNIT (type);

        size_t size = PKL_AST_TYPE_I_SIZE (base_type);
        int signed_p = PKL_AST_TYPE_I_SIGNED_P (base_type);

        if (!promote_offset (PKL_PASS_AST,
                             size, signed_p, unit,
                             &PKL_AST_EXP_OPERAND (exp, 0), &restart1))
          goto error;

        if (!promote_offset (PKL_PASS_AST,
                             size, signed_p, unit,
                             &PKL_AST_EXP_OPERAND (exp, 1), &restart2))
          goto error;

        PKL_PASS_RESTART = restart1 || restart2;
        break;
      }
    case PKL_TYPE_STRING:
      /* Fallthrough.  */
    case PKL_TYPE_ARRAY:
      if (PKL_AST_EXP_CODE (exp) != PKL_AST_OP_ADD)
        goto error;
      break;
    default:
      goto error;
    }

  PKL_PASS_DONE;

 error:
  PKL_ICE (PKL_AST_LOC (exp),
           "couldn't promote operands of expression #%" PRIu64,
           PKL_AST_UID (exp));
  PKL_PASS_ERROR;
}
PKL_PHASE_END_HANDLER

/* Bit-concatenation is defined on the following configurations of
   operand and result types:

   INTEGRAL x INTEGRAL -> INTEGRAL

   Since the operands can be of any integral type, this handler
   focuses on promoting integral struct operands to their itype.  */

PKL_PHASE_BEGIN_HANDLER (pkl_promo_ps_op_bconc)
{
  pkl_ast_node exp = PKL_PASS_NODE;
  int restart = 0;
  int i;

  for (i = 0; i < 2; ++i)
    {
      pkl_ast_node op = PKL_AST_EXP_OPERAND (exp, i);
      pkl_ast_node op_type = PKL_AST_TYPE (op);

      if (PKL_AST_TYPE_CODE (op_type) == PKL_TYPE_STRUCT)
        {
          pkl_ast_node itype = PKL_AST_TYPE_S_ITYPE (op_type);

          /* Guaranteed as per typify.  */
          assert (itype);

          size_t size = PKL_AST_TYPE_I_SIZE (itype);
          int signed_p = PKL_AST_TYPE_I_SIGNED_P (itype);

          if (!promote_integral (PKL_PASS_AST,
                                 size, signed_p,
                                 &PKL_AST_EXP_OPERAND (exp, i),
                                 &restart))
            {
              PKL_ICE (PKL_AST_LOC (exp),
                       "couldn't promote operands of expression #%" PRIu64,
                       PKL_AST_UID (exp));
              PKL_PASS_ERROR;
            }

          PKL_PASS_RESTART = PKL_PASS_RESTART || restart;
        }
    }
}
PKL_PHASE_END_HANDLER

/* Multiplication is defined on the following configurations of
   operand and result types:

      INTEGRAL x INTEGRAL -> INTEGRAL
      OFFSET   x INTEGRAL -> OFFSET
      INTEGRAL x OFFSET   -> OFFSET
      STRING   x INTEGRAL -> STRING
      INTEGRAL x STRING   -> STRING

   In the I x I -> I configuration, the types of the operands are
   promoted to match the type of the result, if needed.

   In the O x I -> O and I x O -> O configurations, both the type of
   the integral operand and the base type of the offset operand are
   promoted to match the base type of the offset result.

   In the S x I -> S and I x S -> S configurations, the type of the
   integral operand shall be promoted to an uint64.  */

PKL_PHASE_BEGIN_HANDLER (pkl_promo_ps_op_mul)
{
  pkl_ast_node exp = PKL_PASS_NODE;
  pkl_ast_node exp_type = PKL_AST_TYPE (exp);
  int exp_type_code = PKL_AST_TYPE_CODE (exp_type);
  int i;

  for (i = 0; i < 2; ++i)
    {
      int restart;

      pkl_ast_node op = PKL_AST_EXP_OPERAND (exp, i);
      pkl_ast_node op_type = PKL_AST_TYPE (op);

      /* Handle integral struct operand.  */
      if (PKL_AST_TYPE_CODE (op_type) == PKL_TYPE_STRUCT
          && PKL_AST_TYPE_S_ITYPE (op_type))
        op_type = PKL_AST_TYPE_S_ITYPE (op_type);

      if (PKL_AST_TYPE_CODE (op_type) == PKL_TYPE_INTEGRAL)
        {
          size_t size;
          int signed_p;

          if (exp_type_code == PKL_TYPE_INTEGRAL)
            {
              size = PKL_AST_TYPE_I_SIZE (exp_type);
              signed_p = PKL_AST_TYPE_I_SIGNED_P  (exp_type);
            }
          else if (exp_type_code == PKL_TYPE_STRING)
            {
              size = 64;
              signed_p = 0;
            }
          else
            {
              pkl_ast_node exp_base_type
                = PKL_AST_TYPE_O_BASE_TYPE (exp_type);

              size = PKL_AST_TYPE_I_SIZE (exp_base_type);
              signed_p = PKL_AST_TYPE_I_SIGNED_P (exp_base_type);
            }

          if (!promote_integral (PKL_PASS_AST, size, signed_p,
                                 &PKL_AST_EXP_OPERAND (exp, i), &restart))
            goto error;

          PKL_PASS_RESTART = restart;
        }
      else if (PKL_AST_TYPE_CODE (op_type) == PKL_TYPE_OFFSET)
        {
          pkl_ast_node base_type = PKL_AST_TYPE_O_BASE_TYPE (exp_type);
          pkl_ast_node unit = PKL_AST_TYPE_O_UNIT (exp_type);

          size_t size = PKL_AST_TYPE_I_SIZE (base_type);
          int signed_p = PKL_AST_TYPE_I_SIGNED_P (base_type);

          if (!promote_offset (PKL_PASS_AST,
                               size, signed_p, unit,
                               &PKL_AST_EXP_OPERAND (exp, i), &restart))
            goto error;

          PKL_PASS_RESTART = restart;
        }
      else if (PKL_AST_TYPE_CODE (op_type) == PKL_TYPE_STRING)
        ;
      else
        assert (0);
    }

  PKL_PASS_DONE;

 error:
  PKL_ICE (PKL_AST_LOC (exp),
           "couldn't promote operands of expression #%" PRIu64,
           PKL_AST_UID (exp));
  PKL_PASS_ERROR;
}
PKL_PHASE_END_HANDLER

/* The relational operations are defined on the following
   configurations of operand and result types:

           INTEGRAL x INTEGRAL -> BOOL
           STRING   x STRING   -> BOOL
           OFFSET   x OFFSET   -> BOOL
           ARRAY    x ARRAY    -> BOOL
           STRUCT   x STRUCT   -> BOOL

   In the I x I -> I configuration, the types of the operands are
   promoted in a way both operands end having the same type, following
   the language's promotion rules.

   The same logic is applied to the magnitudes of the offset operands
   in the O x O -> O configuration.

   No operand promotion is performed in the S x S -> S
   configuration.  Ditto for A x A -> A.  */

PKL_PHASE_BEGIN_HANDLER (pkl_promo_ps_op_rela)
{
  pkl_ast_node exp = PKL_PASS_NODE;
  pkl_ast_node op1 = PKL_AST_EXP_OPERAND (exp, 0);
  pkl_ast_node op2 = PKL_AST_EXP_OPERAND (exp, 1);
  pkl_ast_node op1_type = PKL_AST_TYPE (op1);
  pkl_ast_node op2_type = PKL_AST_TYPE (op2);

  if (PKL_AST_TYPE_CODE (op1_type) != PKL_AST_TYPE_CODE (op2_type))
    goto error;

  /* Handle integral struct operands.  */
  if (PKL_AST_TYPE_CODE (op1_type) == PKL_TYPE_STRUCT
      && PKL_AST_TYPE_S_ITYPE (op1_type))
    op1_type = PKL_AST_TYPE_S_ITYPE (op1_type);
  if (PKL_AST_TYPE_CODE (op2_type) == PKL_TYPE_STRUCT
      && PKL_AST_TYPE_S_ITYPE (op1_type))
    op2_type = PKL_AST_TYPE_S_ITYPE (op2_type);

  switch (PKL_AST_TYPE_CODE (op1_type))
    {
    case PKL_TYPE_INTEGRAL:
      {
        int restart1, restart2;

        size_t size = MAX (PKL_AST_TYPE_I_SIZE (op1_type),
                           PKL_AST_TYPE_I_SIZE (op2_type));
        int signed_p = (PKL_AST_TYPE_I_SIGNED_P (op1_type)
                        && PKL_AST_TYPE_I_SIGNED_P (op2_type));


        if (!promote_integral (PKL_PASS_AST, size, signed_p,
                               &PKL_AST_EXP_OPERAND (exp, 0), &restart1))
          goto error;

        if (!promote_integral (PKL_PASS_AST, size, signed_p,
                               &PKL_AST_EXP_OPERAND (exp, 1), &restart2))
          goto error;

        PKL_PASS_RESTART = restart1 || restart2;
        break;
      }
    case PKL_TYPE_OFFSET:
      {
        int restart1, restart2;

        pkl_ast_node op1_base_type = PKL_AST_TYPE_O_BASE_TYPE (op1_type);
        pkl_ast_node op2_base_type = PKL_AST_TYPE_O_BASE_TYPE (op2_type);

        size_t size = MAX (PKL_AST_TYPE_I_SIZE (op1_base_type),
                           PKL_AST_TYPE_I_SIZE (op2_base_type));
        int signed_p = (PKL_AST_TYPE_I_SIGNED_P (op1_base_type)
                        && PKL_AST_TYPE_I_SIGNED_P (op2_base_type));

        pkl_ast_node unit_bit = pkl_ast_make_integer (PKL_PASS_AST, 1);
        ASTREF (unit_bit);
        PKL_AST_LOC (unit_bit) = PKL_AST_LOC (exp);

        if (!promote_offset (PKL_PASS_AST,
                             size, signed_p, unit_bit,
                             &PKL_AST_EXP_OPERAND (exp, 0), &restart1))
          goto error;

        if (!promote_offset (PKL_PASS_AST,
                             size, signed_p, unit_bit,
                             &PKL_AST_EXP_OPERAND (exp, 1), &restart2))
          goto error;

        pkl_ast_node_free (unit_bit);

        PKL_PASS_RESTART = restart1 || restart2;
        break;
      }
    case PKL_TYPE_STRING:
      /* Fallthrough.  */
    case PKL_TYPE_ARRAY:
      /* Fallthrough.  */
    case PKL_TYPE_STRUCT:
      /* Nothing to do.  */
      break;
    default:
      goto error;
    }

  PKL_PASS_DONE;

 error:
  PKL_ICE (PKL_AST_LOC (exp),
           "couldn't promote operands of expression #%" PRIu64,
           PKL_AST_UID (exp));
  PKL_PASS_ERROR;
}
PKL_PHASE_END_HANDLER

/* The bit shift operations, and also exponentiation, are defined on
   the following configurations of operand and result types:

           INTEGRAL x INTEGRAL(32,0) -> INTEGRAL
           OFFSET   x INTEGRAL(32,0) -> OFFSET

   In this configuration, the type of the first operand is promoted to
   match the type of the result.  The type of the second operand is
   promoted to an unsigned 32-bit integral type.  */

PKL_PHASE_BEGIN_HANDLER (pkl_promo_ps_op_bshiftpow)
{
  int restart1, restart2;

  pkl_ast_node exp = PKL_PASS_NODE;
  pkl_ast_node type = PKL_AST_TYPE (exp);

  switch (PKL_AST_TYPE_CODE (type))
    {
    case PKL_TYPE_INTEGRAL:
      {
        size_t size = PKL_AST_TYPE_I_SIZE (type);
        int signed_p = PKL_AST_TYPE_I_SIGNED_P (type);

        if (!promote_integral (PKL_PASS_AST, size, signed_p,
                               &PKL_AST_EXP_OPERAND (exp, 0), &restart1)
            || !promote_integral (PKL_PASS_AST, 32, 0,
                                  &PKL_AST_EXP_OPERAND (exp, 1), &restart2))
          goto error;

        PKL_PASS_RESTART = restart1 || restart2;
        break;
      }
    case PKL_TYPE_OFFSET:
      {
        pkl_ast_node base_type = PKL_AST_TYPE_O_BASE_TYPE (type);
        pkl_ast_node unit = PKL_AST_TYPE_O_UNIT (type);

        size_t size = PKL_AST_TYPE_I_SIZE (base_type);
        int signed_p = PKL_AST_TYPE_I_SIGNED_P (base_type);

        if (!promote_offset (PKL_PASS_AST,
                             size, signed_p, unit,
                             &PKL_AST_EXP_OPERAND (exp, 0), &restart1)
            || !promote_integral (PKL_PASS_AST, 32, 0,
                                  &PKL_AST_EXP_OPERAND (exp, 1), &restart2))
          goto error;

        PKL_PASS_RESTART = restart1 || restart2;
        break;
      }
    default:
      assert (0);
    }

  PKL_PASS_DONE;

 error:
  PKL_ICE (PKL_AST_LOC (exp),
           "couldn't promote operands of expression #%" PRIu64,
           PKL_AST_UID (exp));
  PKL_PASS_ERROR;
}
PKL_PHASE_END_HANDLER

/* The rest of the binary operations are defined on the following
   configurations of operand and result types:

       INTEGRAL OP INTEGRAL -> INTEGRAL.

   In the I OP I -> I configuration, the types of the operands are
   promoted to match the type of the result, if needed.  */

PKL_PHASE_BEGIN_HANDLER (pkl_promo_ps_op_binary)
{
  int restart1, restart2;

  pkl_ast_node exp = PKL_PASS_NODE;
  pkl_ast_node type = PKL_AST_TYPE (exp);

  if (PKL_AST_TYPE_CODE (type) == PKL_TYPE_INTEGRAL)
    {
      size_t size = PKL_AST_TYPE_I_SIZE (type);
      int signed_p = PKL_AST_TYPE_I_SIGNED_P (type);

      if (!promote_integral (PKL_PASS_AST, size, signed_p,
                             &PKL_AST_EXP_OPERAND (exp, 0), &restart1)
          || !promote_integral (PKL_PASS_AST, size, signed_p,
                                &PKL_AST_EXP_OPERAND (exp, 1), &restart2))
        {
          PKL_ICE (PKL_AST_LOC (exp),
                   "couldn't promote operands of expression #%" PRIu64,
                   PKL_AST_UID (exp));
          PKL_PASS_ERROR;
        }

      PKL_PASS_RESTART = restart1 || restart2;
    }
}
PKL_PHASE_END_HANDLER

/* All the unary operations are defined on the following
   configurations of operand and result types:

                    INTEGRAL -> INTEGRAL

   In the I -> I configuration, the type of the operand is promoted to
   match the type of the result, if needed.  */

PKL_PHASE_BEGIN_HANDLER (pkl_promo_ps_op_unary)
{
  int restart = 0;

  pkl_ast_node node = PKL_PASS_NODE;
  pkl_ast_node type = PKL_AST_TYPE (node);

  if (PKL_AST_TYPE_CODE (type) == PKL_TYPE_INTEGRAL)
    {
      size_t size = PKL_AST_TYPE_I_SIZE (type);
      int signed_p = PKL_AST_TYPE_I_SIGNED_P (type);

      if (!promote_integral (PKL_PASS_AST, size, signed_p,
                             &PKL_AST_EXP_OPERAND (node, 0), &restart))
        {
          PKL_ICE (PKL_AST_LOC (node),
                   "couldn't promote operands of expression #%" PRIu64,
                   PKL_AST_UID (node));
          PKL_PASS_ERROR;
        }
    }

  PKL_PASS_RESTART = restart;
}
PKL_PHASE_END_HANDLER

/* Handler for promoting indexes in indexers to unsigned 64 bit
   values.  */

PKL_PHASE_BEGIN_HANDLER (pkl_promo_ps_indexer)
{
  int restart;
  pkl_ast_node node = PKL_PASS_NODE;

  if (!promote_integral (PKL_PASS_AST, 64, 0,
                         &PKL_AST_INDEXER_INDEX (node), &restart))
    {
      PKL_ICE (PKL_AST_LOC (node),
               "couldn't promote indexer subscript");
      PKL_PASS_ERROR;
    }

  PKL_PASS_RESTART = restart;
}
PKL_PHASE_END_HANDLER

/* Handler for promoting indexes in trimmers to unsigned 64 bit
   values.  */

PKL_PHASE_BEGIN_HANDLER (pkl_promo_ps_trimmer)
{
  int restart;
  pkl_ast_node trimmer = PKL_PASS_NODE;
  pkl_ast_node from = PKL_AST_TRIMMER_FROM (trimmer);
  pkl_ast_node to = PKL_AST_TRIMMER_TO (trimmer);

  if (!promote_integral (PKL_PASS_AST, 64, 0,
                         &PKL_AST_TRIMMER_FROM (trimmer), &restart))
    {
      PKL_ICE (PKL_AST_LOC (from),
               "couldn't promote trimmer index");
      PKL_PASS_ERROR;
    }

  if (!promote_integral (PKL_PASS_AST, 64, 0,
                         &PKL_AST_TRIMMER_TO (trimmer), &restart))
    {
      PKL_ICE (PKL_AST_LOC (to),
               "couldn't promote trimmer index");
      PKL_PASS_ERROR;
    }

  PKL_PASS_RESTART = restart;
}
PKL_PHASE_END_HANDLER

/* Handler for promoting the unit in offset type specifiers to 64
   unsigned bit values.  */

PKL_PHASE_BEGIN_HANDLER (pkl_promo_ps_type_offset)
{
  int restart;
  pkl_ast_node offset_type = PKL_PASS_NODE;
  pkl_ast_node offset_type_unit = PKL_AST_TYPE_O_UNIT (offset_type);

  if (PKL_AST_CODE (offset_type_unit) != PKL_AST_INTEGER)
    PKL_PASS_DONE;

  if (!promote_integral (PKL_PASS_AST, 64, 0,
                         &PKL_AST_TYPE_O_UNIT (offset_type),
                         &restart))
    {
      PKL_ICE (PKL_AST_LOC (offset_type_unit),
               "couldn't promote offset type unit to uint<64>");
      PKL_PASS_ERROR;
    }

  PKL_PASS_RESTART = restart;
}
PKL_PHASE_END_HANDLER

/* Handler for promoting the array size in array type literals to 64
   unsigned bit values, or to offset<uint<64>,b> if they are
   offsets.  */

PKL_PHASE_BEGIN_HANDLER (pkl_promo_ps_type_array)
{
  int restart;
  pkl_ast_node array_type = PKL_PASS_NODE;
  pkl_ast_node bound = PKL_AST_TYPE_A_BOUND (array_type);
  pkl_ast_node bound_type;

  if (bound == NULL)
    /* This array type hasn't a number of elements.  Be done.  */
    PKL_PASS_DONE;

  bound_type = PKL_AST_TYPE (bound);

  switch (PKL_AST_TYPE_CODE (bound_type))
    {
    case PKL_TYPE_INTEGRAL:
      if (!promote_integral (PKL_PASS_AST, 64, 0,
                             &PKL_AST_TYPE_A_BOUND (array_type), &restart))
        {
          PKL_ICE (PKL_AST_LOC (bound),
                   "couldn't promote array type size expression");
          PKL_PASS_ERROR;
        }
      break;
    case PKL_TYPE_OFFSET:
      {
        pkl_ast_node unit_bit = pkl_ast_make_integer (PKL_PASS_AST, 1);

        ASTREF (unit_bit);
        PKL_AST_LOC (unit_bit) = PKL_AST_LOC (PKL_PASS_NODE);

        if (!promote_offset (PKL_PASS_AST,
                             64, 0, unit_bit,
                             &PKL_AST_TYPE_A_BOUND (array_type), &restart))
          {
            PKL_ICE (PKL_AST_LOC (bound),
                     "couldn't promote array type size expression");
            PKL_PASS_ERROR;
          }

        pkl_ast_node_free (unit_bit);

        break;
      }
    default:
      assert (0); /* This can't happen.  */
    }

  PKL_PASS_RESTART = restart;
}
PKL_PHASE_END_HANDLER

/* Indexes in array initializers should be unsigned long.  */

PKL_PHASE_BEGIN_HANDLER (pkl_promo_ps_array_initializer)
{
  pkl_ast_node node = PKL_PASS_NODE;
  pkl_ast_node index = PKL_AST_ARRAY_INITIALIZER_INDEX (node);

  /* Note that the index is optional.  */
  if (index != NULL)
    {
      /* We can't use casts here, as array index initializers should
         be INTEGER nodes, not expressions.  */

      pkl_ast_node index_type = PKL_AST_TYPE (index);

      if (PKL_AST_TYPE_CODE (index_type) != PKL_TYPE_INTEGRAL
          || PKL_AST_TYPE_I_SIZE (index_type) != 64
          || PKL_AST_TYPE_I_SIGNED_P (index_type) != 0)
        {
          pkl_ast_node_free (index_type);

          index_type = pkl_ast_make_integral_type (PKL_PASS_AST,
                                                   64, 0);
          PKL_AST_TYPE (index) = ASTREF (index_type);
          PKL_AST_LOC (index_type) = PKL_AST_LOC (node);
          PKL_PASS_RESTART = 1;
        }
    }
}
PKL_PHASE_END_HANDLER

/* In assignments, the r-value should be promoted to the type of the
   l-value, if that is suitable.  */

PKL_PHASE_BEGIN_HANDLER (pkl_promo_ps_ass_stmt)
{
  pkl_ast_node ass_stmt = PKL_PASS_NODE;
  pkl_ast_node lvalue = PKL_AST_ASS_STMT_LVALUE (ass_stmt);
  pkl_ast_node exp = PKL_AST_ASS_STMT_EXP (ass_stmt);
  pkl_ast_node lvalue_type = PKL_AST_TYPE (lvalue);
  pkl_ast_node exp_type = PKL_AST_TYPE (exp);
  int restart = 0;

  /* At this point it is assured that exp_type is promoteable to
     lvalue_type, or typify1 wouldn't have allowed this node to
     pass.  */

  /* If the lvalue type is not an array, and both types are
     equivalent, then we are done.  Arrays are excluded because of how
     the boundary is ignored in type equivalence.  */
  if (PKL_AST_TYPE_CODE (lvalue_type) != PKL_TYPE_ARRAY
      && pkl_ast_type_equal (lvalue_type, exp_type))
    PKL_PASS_DONE;

  /* A promotion is needed.  */
  switch (PKL_AST_TYPE_CODE (lvalue_type))
    {
    case PKL_TYPE_ANY:
      break;
    case PKL_TYPE_ARRAY:
      if (!promote_array (PKL_PASS_AST,
                          lvalue_type,
                          &PKL_AST_ASS_STMT_EXP (ass_stmt),
                          &restart))
        goto error;
      break;
    case PKL_TYPE_INTEGRAL:
      if (!promote_integral (PKL_PASS_AST,
                             PKL_AST_TYPE_I_SIZE (lvalue_type),
                             PKL_AST_TYPE_I_SIGNED_P (lvalue_type),
                             &PKL_AST_ASS_STMT_EXP (ass_stmt),
                             &restart))
        goto error;
      break;
    case PKL_TYPE_OFFSET:
      {
        pkl_ast_node base_type = PKL_AST_TYPE_O_BASE_TYPE (lvalue_type);
        pkl_ast_node unit = PKL_AST_TYPE_O_UNIT (lvalue_type);

        size_t size = PKL_AST_TYPE_I_SIZE (base_type);
        int signed_p = PKL_AST_TYPE_I_SIGNED_P (base_type);

        if (!promote_offset (PKL_PASS_AST,
                             size, signed_p, unit,
                             &PKL_AST_ASS_STMT_EXP (ass_stmt),
                             &restart))
          goto error;
      }
      break;
    default:
      PKL_ICE (PKL_AST_LOC (ass_stmt),
               "non-promoteable r-value in assignment statement at promo time");
      PKL_PASS_ERROR;
      break;
    }

  PKL_PASS_RESTART = restart;
  PKL_PASS_DONE;

 error:
  PKL_ICE (PKL_AST_LOC (exp),
           "couldn't promote r-value in assignment");
  PKL_PASS_ERROR;
}
PKL_PHASE_END_HANDLER

/* In function calls, the actual arguments should be promoted to the
   type of the formal arguments, if that is suitable.  */

PKL_PHASE_BEGIN_HANDLER (pkl_promo_ps_funcall)
{
  pkl_ast_node funcall = PKL_PASS_NODE;
  pkl_ast_node function = PKL_AST_FUNCALL_FUNCTION (funcall);
  pkl_ast_node function_type = PKL_AST_TYPE (function);

  pkl_ast_node fa, aa;

  for (fa = PKL_AST_TYPE_F_ARGS (function_type),
       aa = PKL_AST_FUNCALL_ARGS (funcall);
       fa && aa;
       fa = PKL_AST_CHAIN (fa), aa = PKL_AST_CHAIN (aa))
    {
      pkl_ast_node fa_type = PKL_AST_FUNC_ARG_TYPE (fa);
      pkl_ast_node aa_exp = PKL_AST_FUNCALL_ARG_EXP (aa);
      int restart = 0;

      /* Ignore non-specified actuals for optional formals.  */
      if (aa_exp)
        {
          pkl_ast_node aa_type = PKL_AST_TYPE (aa_exp);

          /* Do not promote varargs.  */
          if (! PKL_AST_FUNC_TYPE_ARG_VARARG (fa))
            {
              /* At this point it is assured that the types of the actual
                 argument and the formal argument are promoteable, or typify
                 wouldn't have allowed it to pass.  */

              /* If both types are equivalent, then we are done.  */
              if (! pkl_ast_type_equal (fa_type, aa_type))
                /* A promotion is needed.  */
                switch (PKL_AST_TYPE_CODE (fa_type))
                  {
                  case PKL_TYPE_ANY:
                    break;
                  case PKL_TYPE_ARRAY:
                    /* Array promotion of arguments is performed in the function
                       prologue, not here.  This is because the target type is
                       defined in the function's environment.  */
                    break;
                  case PKL_TYPE_INTEGRAL:
                    if (!promote_integral (PKL_PASS_AST,
                                           PKL_AST_TYPE_I_SIZE (fa_type),
                                           PKL_AST_TYPE_I_SIGNED_P (fa_type),
                                           &PKL_AST_FUNCALL_ARG_EXP (aa),
                                           &restart))
                      goto error;
                    break;
                  case PKL_TYPE_OFFSET:
                    {
                      pkl_ast_node base_type = PKL_AST_TYPE_O_BASE_TYPE (fa_type);
                      pkl_ast_node unit = PKL_AST_TYPE_O_UNIT (fa_type);

                      size_t size = PKL_AST_TYPE_I_SIZE (base_type);
                      int signed_p = PKL_AST_TYPE_I_SIGNED_P (base_type);

                      if (!promote_offset (PKL_PASS_AST,
                                           size, signed_p, unit,
                                           &PKL_AST_FUNCALL_ARG_EXP (aa),
                                           &restart))
                        goto error;
                    }
                    break;
                  default:
                    PKL_ICE (PKL_AST_LOC (funcall),
                             "funcall contains non-promoteable arguments at promo time");
                    PKL_PASS_ERROR;
                    break;
                  }

              PKL_PASS_RESTART = PKL_PASS_RESTART || restart;
            }
        }
    }

  PKL_PASS_DONE;

 error:
  PKL_ICE (PKL_AST_LOC (aa),
           "couldn't promote funcall argument");
  PKL_PASS_ERROR;
}
PKL_PHASE_END_HANDLER

/* Promoting the condition of an IF statement to int<32> is not
   strictly necessary (nor desirable) since the code generator will
   generate the right compare-and-branch instruction.  However, we
   still need to promote integral structs to their itype.  */

PKL_PHASE_BEGIN_HANDLER (pkl_promo_ps_if_stmt)
{
  pkl_ast_node if_stmt = PKL_PASS_NODE;
  pkl_ast_node exp = PKL_AST_IF_STMT_EXP (if_stmt);
  pkl_ast_node exp_type = PKL_AST_TYPE (exp);

  if (PKL_AST_TYPE_CODE (exp_type) == PKL_TYPE_STRUCT
      && PKL_AST_TYPE_S_ITYPE (exp_type))
    {
      pkl_ast_node itype = PKL_AST_TYPE_S_ITYPE (exp_type);
      size_t size = PKL_AST_TYPE_I_SIZE (itype);
      int signed_p = PKL_AST_TYPE_I_SIGNED_P (itype);
      int restart;

      if (!promote_integral (PKL_PASS_AST,
                             size, signed_p,
                             &PKL_AST_IF_STMT_EXP (if_stmt),
                             &restart))
        {
          PKL_ICE (PKL_AST_LOC (if_stmt),
                   "couldn't promote condition of if-stmt #%" PRIu64,
                   PKL_AST_UID (if_stmt));
          PKL_PASS_ERROR;
        }

      PKL_PASS_RESTART = restart;
    }
}
PKL_PHASE_END_HANDLER

/* The condition in loop statements, if present, shall be promoted to
   int<32>.  */

PKL_PHASE_BEGIN_HANDLER (pkl_promo_ps_loop_stmt)
{
  pkl_ast_node loop_stmt = PKL_PASS_NODE;
  pkl_ast_node condition = PKL_AST_LOOP_STMT_CONDITION (loop_stmt);

  if (condition)
    {
      int restart;

      /* Note that the condition node is promoteable as per
         typify.  */

      if (!promote_integral (PKL_PASS_AST,
                             32, 1,
                             &PKL_AST_LOOP_STMT_CONDITION (loop_stmt),
                             &restart))
        {
          PKL_ICE (PKL_AST_LOC (loop_stmt),
                   "couldn't promote condition of lop-stmt #%" PRIu64,
                   PKL_AST_UID (loop_stmt));
          PKL_PASS_ERROR;
        }

      PKL_PASS_RESTART = restart;
    }
}
PKL_PHASE_END_HANDLER

/* The value returned from a function can be promoted in certain
   circumstances.  Do it!  */

PKL_PHASE_BEGIN_HANDLER (pkl_promo_ps_return_stmt)
{
  pkl_ast_node return_stmt = PKL_PASS_NODE;
  pkl_ast_node exp = PKL_AST_RETURN_STMT_EXP (return_stmt);
  pkl_ast_node function = PKL_AST_RETURN_STMT_FUNCTION (return_stmt);

  pkl_ast_node returned_type;
  pkl_ast_node expected_type;

  if (exp == NULL)
    PKL_PASS_DONE;

  returned_type = PKL_AST_TYPE (exp);
  expected_type = PKL_AST_FUNC_RET_TYPE (function);

  /* At this point it is assured that the expected type and returned
     type are promoteable, or typify wouldn't have allowed it to
     pass.  */

  /* If the expected type is not an array, and both types are
     equivalent, then we are done.  Arrays are excluded because of how
     the boundary is ignored in type equivalence.  */
  if (PKL_AST_TYPE_CODE (expected_type) != PKL_TYPE_ARRAY
      && pkl_ast_type_equal (expected_type, returned_type))
    PKL_PASS_DONE;

  if (PKL_AST_TYPE_CODE (expected_type) != PKL_TYPE_VOID)
    {
      int restart = 0;

      switch (PKL_AST_TYPE_CODE (expected_type))
        {
        case PKL_TYPE_ANY:
          break;
        case PKL_TYPE_ARRAY:
          if (!promote_array (PKL_PASS_AST,
                              expected_type,
                              &PKL_AST_RETURN_STMT_EXP (return_stmt),
                              &restart))
            goto error;
          break;
        case PKL_TYPE_INTEGRAL:
          if (!promote_integral (PKL_PASS_AST,
                                 PKL_AST_TYPE_I_SIZE (expected_type),
                                 PKL_AST_TYPE_I_SIGNED_P (expected_type),
                                 &PKL_AST_RETURN_STMT_EXP (return_stmt),
                                 &restart))
            goto error;
          break;
        case PKL_TYPE_OFFSET:
          {
            pkl_ast_node base_type = PKL_AST_TYPE_O_BASE_TYPE (expected_type);
            pkl_ast_node unit = PKL_AST_TYPE_O_UNIT (expected_type);

            size_t size = PKL_AST_TYPE_I_SIZE (base_type);
            int signed_p = PKL_AST_TYPE_I_SIGNED_P (base_type);

            if (!promote_offset (PKL_PASS_AST,
                                 size, signed_p, unit,
                                 &PKL_AST_RETURN_STMT_EXP (return_stmt),
                                 &restart))
              goto error;
          }
          break;
        default:
          PKL_ICE (PKL_AST_LOC (return_stmt),
                   "return statement non-promoteable arguments at promo time");
          PKL_PASS_ERROR;
          break;
        }

      PKL_PASS_RESTART = restart;
    }

  PKL_PASS_DONE;

 error:
  PKL_ICE (PKL_AST_LOC (exp),
           "couldn't promote return expression");
  PKL_PASS_ERROR;
}
PKL_PHASE_END_HANDLER

/* Promote arguments to printf.  */

PKL_PHASE_BEGIN_HANDLER (pkl_promo_ps_print_stmt)
{
  pkl_ast_node print_stmt = PKL_PASS_NODE;
  pkl_ast_node print_stmt_types = PKL_AST_PRINT_STMT_TYPES (print_stmt);
  pkl_ast_node print_stmt_args = PKL_AST_PRINT_STMT_ARGS (print_stmt);

  pkl_ast_node type, arg;

  for (arg = print_stmt_args, type = print_stmt_types;
       arg && type;
       arg = PKL_AST_CHAIN (arg), type = PKL_AST_CHAIN (type))
    {
      pkl_ast_node arg_exp = PKL_AST_PRINT_STMT_ARG_EXP (arg);

      /* Skip arguments without associated values.  Also skip
         arguments with declared type ANY (%v) */
      if (arg_exp
          && PKL_AST_TYPE_CODE (type) != PKL_TYPE_ANY)
        {
          pkl_ast_node exp_type = PKL_AST_TYPE (arg_exp);

          if (PKL_AST_TYPE_CODE (exp_type) == PKL_TYPE_INTEGRAL)
            {
              int restart = 0;

              if (!promote_integral (PKL_PASS_AST,
                                     PKL_AST_TYPE_I_SIZE (type),
                                     PKL_AST_TYPE_I_SIGNED_P (type),
                                     &PKL_AST_PRINT_STMT_ARG_EXP (arg),
                                     &restart))
                {
                  PKL_ICE (PKL_AST_LOC (arg),
                           "couldn't promote printf argument initializer");
                      PKL_PASS_ERROR;
                }

              PKL_PASS_RESTART = PKL_PASS_RESTART || restart;
            }
        }
    }
}
PKL_PHASE_END_HANDLER

/* Promote function argument initializers, if needed.  */

PKL_PHASE_BEGIN_HANDLER (pkl_promo_ps_func_arg)
{
  pkl_ast_node func_arg = PKL_PASS_NODE;
  pkl_ast_node initial = PKL_AST_FUNC_ARG_INITIAL (func_arg);

  if (initial)
    {
      pkl_ast_node arg_type = PKL_AST_FUNC_ARG_TYPE (func_arg);
      pkl_ast_node initial_type = PKL_AST_TYPE (initial);
      int restart = 0;

      /* At this point it is assured that the arg type and initial
         types are promoteable, or typify wouldn't have allowed it to
         pass.  */

      /* If the arg tyep is not an array, and both types are
         equivalent, then we are done.  Arrays are excluded because of
         how the boundary is ignored in type equivalence.  */
      if (PKL_AST_TYPE_CODE (arg_type) != PKL_TYPE_ARRAY
          && pkl_ast_type_equal (arg_type, initial_type))
        PKL_PASS_DONE;

      switch (PKL_AST_TYPE_CODE (arg_type))
            {
            case PKL_TYPE_ANY:
              break;
            case PKL_TYPE_ARRAY:
              if (!promote_array (PKL_PASS_AST,
                                  arg_type,
                                  &PKL_AST_FUNC_ARG_INITIAL (func_arg),
                                  &restart))
                goto error;
              break;
            case PKL_TYPE_INTEGRAL:
              if (!promote_integral (PKL_PASS_AST,
                                     PKL_AST_TYPE_I_SIZE (arg_type),
                                     PKL_AST_TYPE_I_SIGNED_P (arg_type),
                                     &PKL_AST_FUNC_ARG_INITIAL (func_arg),
                                     &restart))
                goto error;
              break;
            case PKL_TYPE_OFFSET:
              {
                pkl_ast_node base_type = PKL_AST_TYPE_O_BASE_TYPE (arg_type);
                pkl_ast_node unit = PKL_AST_TYPE_O_UNIT (arg_type);

                size_t size = PKL_AST_TYPE_I_SIZE (base_type);
                int signed_p = PKL_AST_TYPE_I_SIGNED_P (base_type);

                if (!promote_offset (PKL_PASS_AST,
                                     size, signed_p, unit,
                                     &PKL_AST_FUNC_ARG_INITIAL (func_arg),
                                     &restart))
                  goto error;
              }
              break;
            default:
              PKL_ICE (PKL_AST_LOC (initial),
                       "non-promoteable function argument initializer at promo time");
              PKL_PASS_ERROR;
            }

      PKL_PASS_RESTART = restart;
    }

  PKL_PASS_DONE;

 error:
  PKL_ICE (PKL_AST_LOC (initial),
           "couldn't promote argument initializer");
  PKL_PASS_ERROR;
}
PKL_PHASE_END_HANDLER

/* The offsets in maps should be promoted to offset<uint<64>,b>.  This
   is expected by the code generator and the run-time.

   Also the IOS identifier in a map operator should be promoted to an
   32-bit signed integer.  */


PKL_PHASE_BEGIN_HANDLER (pkl_promo_ps_map)
{
  pkl_ast_node map = PKL_PASS_NODE;
  pkl_ast_node map_offset = PKL_AST_MAP_OFFSET (map);
  pkl_ast_node map_ios = PKL_AST_MAP_IOS (map);
  int restart;

  pkl_ast_node unit_bit = pkl_ast_make_integer (PKL_PASS_AST, 1);

  ASTREF (unit_bit);
  PKL_AST_LOC (unit_bit) = PKL_AST_LOC (PKL_PASS_NODE);

  if (!promote_offset (PKL_PASS_AST,
                       64, 0, unit_bit,
                       &PKL_AST_MAP_OFFSET (map),
                       &restart))
    {
      PKL_ICE (PKL_AST_LOC (map_offset),
               "couldn't promote offset of map #%" PRIu64,
               PKL_AST_UID (map));
      PKL_PASS_ERROR;
    }

  pkl_ast_node_free (unit_bit);

  if (map_ios)
    {
      int lrestart;

      if (!promote_integral (PKL_PASS_AST,
                             32, 1,
                             &PKL_AST_MAP_IOS (map),
                             &lrestart))
        {
          PKL_ICE (PKL_AST_LOC (map_ios),
                   "couldn't promote ios of map #%" PRIu64,
                   PKL_AST_UID (map));
          PKL_PASS_ERROR;
        }

      restart |= lrestart;
    }

  PKL_PASS_RESTART = restart;
}
PKL_PHASE_END_HANDLER

/* The condition expression in a conditional ternary expression is
   promoteable to a boolean.  */

PKL_PHASE_BEGIN_HANDLER (pkl_promo_ps_cond_exp)
{
    pkl_ast_node cond_exp = PKL_PASS_NODE;
    pkl_ast_node cond = PKL_AST_COND_EXP_COND (cond_exp);
    pkl_ast_node cond_type = PKL_AST_TYPE (cond);
    int restart;

    assert (PKL_AST_TYPE_CODE (cond_type) == PKL_TYPE_INTEGRAL
            || (PKL_AST_TYPE_CODE (cond_type) == PKL_TYPE_STRUCT
                && PKL_AST_TYPE_S_ITYPE (cond_type)));

    if (!promote_integral (PKL_PASS_AST, 32, 1,
                           &PKL_AST_COND_EXP_COND (cond_exp),
                           &restart))
      {
        PKL_ICE (PKL_AST_LOC (cond),
                 "couldn't promote condition expression in ternary conditional\
 operator");
        PKL_PASS_ERROR;
      }

    PKL_PASS_RESTART = restart;
}
PKL_PHASE_END_HANDLER

/* Element constraints in struct types are promoteable to
   booleans.  Ditto for optconds.

   Also, the initializer in a struct type field should be promoted to
   the field type.

   Struct field labels are promoted to offset<uint<64>,b>, in order to
   make the mapper code more efficient.  */

PKL_PHASE_BEGIN_HANDLER (pkl_promo_ps_struct_type_field)
{
  pkl_ast_node elem = PKL_PASS_NODE;
  pkl_ast_node elem_constraint = PKL_AST_STRUCT_TYPE_FIELD_CONSTRAINT (elem);
  pkl_ast_node elem_initializer = PKL_AST_STRUCT_TYPE_FIELD_INITIALIZER (elem);
  pkl_ast_node elem_optcond = PKL_AST_STRUCT_TYPE_FIELD_OPTCOND (elem);
  pkl_ast_node elem_label = PKL_AST_STRUCT_TYPE_FIELD_LABEL (elem);

  if (elem_constraint)
    {
      pkl_ast_node constraint_type = PKL_AST_TYPE (elem_constraint);
      int restart = 0;

      if (PKL_AST_TYPE_CODE (constraint_type) == PKL_TYPE_STRUCT
          && PKL_AST_TYPE_S_ITYPE (constraint_type))
        constraint_type = PKL_AST_TYPE_S_ITYPE (constraint_type);

      switch (PKL_AST_TYPE_CODE (constraint_type))
        {
        case PKL_TYPE_INTEGRAL:
          if (!promote_integral (PKL_PASS_AST, 32, 1,
                                 &PKL_AST_STRUCT_TYPE_FIELD_CONSTRAINT (elem),
                                 &restart))
            {
              PKL_ICE (PKL_AST_LOC (elem_constraint),
                       "couldn't promote struct field constraint");
              PKL_PASS_ERROR;
            }
          break;
        default:
          PKL_ICE (PKL_AST_LOC (elem_constraint),
                   "non-promoteable struct field constraint at promo time");
          PKL_PASS_ERROR;
          break;
        }

      PKL_PASS_RESTART = restart;
    }

  if (elem_initializer)
    {
      int restart = 0;
      pkl_ast_node elem_type = PKL_AST_STRUCT_TYPE_FIELD_TYPE (elem);

      switch (PKL_AST_TYPE_CODE (elem_type))
        {
        case PKL_TYPE_INTEGRAL:
          {
            if (!promote_integral (PKL_PASS_AST,
                                   PKL_AST_TYPE_I_SIZE (elem_type),
                                   PKL_AST_TYPE_I_SIGNED_P (elem_type),
                                   &PKL_AST_STRUCT_TYPE_FIELD_INITIALIZER (elem),
                                   &restart))
              {
                PKL_ICE (PKL_AST_LOC (elem_initializer),
                         "couldn't promote struct type field initializer");
                PKL_PASS_ERROR;
              }
            break;
          }
        case PKL_TYPE_OFFSET:
          {
            pkl_ast_node base_type
              = PKL_AST_TYPE_O_BASE_TYPE (elem_type);

            if (!promote_offset (PKL_PASS_AST,
                                 PKL_AST_TYPE_I_SIZE (base_type),
                                 PKL_AST_TYPE_I_SIGNED_P (base_type),
                                 PKL_AST_TYPE_O_UNIT (elem_type),
                                 &PKL_AST_STRUCT_TYPE_FIELD_INITIALIZER (elem),
                                 &restart))
              {
                PKL_ICE (PKL_AST_LOC (elem_initializer),
                         "couldn't promote struct type field initializer");
                PKL_PASS_ERROR;
              }
            break;
          }
        case PKL_TYPE_ARRAY:
          {
            if (!promote_array (PKL_PASS_AST,
                                elem_type,
                                &PKL_AST_STRUCT_TYPE_FIELD_INITIALIZER (elem),
                                &restart))
              {
                PKL_ICE (PKL_AST_LOC (elem_initializer),
                         "couldn't promote struct type field initializer");
                PKL_PASS_ERROR;
              }
            break;
          }
        case PKL_TYPE_STRUCT:
          /* Nothing to do.  */
          break;
        default:
          assert (0);
        }

      PKL_PASS_RESTART = PKL_PASS_RESTART || restart;
    }

  if (elem_optcond)
    {
      pkl_ast_node optcond_type = PKL_AST_TYPE (elem_optcond);
      int restart = 0;

      /* Handle integral structure.  */
      if (PKL_AST_TYPE_CODE (optcond_type) == PKL_TYPE_STRUCT
          && PKL_AST_TYPE_S_ITYPE (optcond_type))
        optcond_type = PKL_AST_TYPE_S_ITYPE (optcond_type);

      switch (PKL_AST_TYPE_CODE (optcond_type))
        {
        case PKL_TYPE_INTEGRAL:
          if (!promote_integral (PKL_PASS_AST, 32, 1,
                                 &PKL_AST_STRUCT_TYPE_FIELD_OPTCOND (elem),
                                 &restart))
            {
              PKL_ICE (PKL_AST_LOC (elem_optcond),
                       "couldn't promote struct field optcond");
              PKL_PASS_ERROR;
            }
          break;
        default:
          PKL_ICE (PKL_AST_LOC (elem_optcond),
                   "non-promoteable struct field optcond at promo time");
          PKL_PASS_ERROR;
          break;
        }

      PKL_PASS_RESTART = PKL_PASS_RESTART || restart;
    }

  if (elem_label)
    {
      pkl_ast_node label_type = PKL_AST_TYPE (elem_label);
      int restart = 0;

      switch (PKL_AST_TYPE_CODE (label_type))
        {
        case PKL_TYPE_OFFSET:
          {
            pkl_ast_node unit_bit = pkl_ast_make_integer (PKL_PASS_AST, 1);

            if (!promote_offset (PKL_PASS_AST,
                                 64, 0, unit_bit,
                                 &PKL_AST_STRUCT_TYPE_FIELD_LABEL (elem),
                                 &restart))
              {
                PKL_ICE (PKL_AST_LOC (elem_label),
                         "couldn't promote struct field label");
                PKL_PASS_ERROR;
              }

            ASTREF (unit_bit); pkl_ast_node_free (unit_bit);
            break;
          }
        default:
          PKL_ICE (PKL_AST_LOC (elem_label),
                   "non-promoteable struct field label at promo time");
          PKL_PASS_ERROR;
          break;
        }

      PKL_PASS_RESTART = PKL_PASS_RESTART || restart;
    }
}
PKL_PHASE_END_HANDLER

/* The left operand of an `in' operator shall be promoted to the type
 * of the elements stored in the array at the right operand.  */

PKL_PHASE_BEGIN_HANDLER (pkl_promo_ps_op_in)
{
  pkl_ast_node exp = PKL_PASS_NODE;
  pkl_ast_node op1 = PKL_AST_EXP_OPERAND (exp, 0);
  pkl_ast_node op2 = PKL_AST_EXP_OPERAND (exp, 1);
  pkl_ast_node t1 = PKL_AST_TYPE (op1);
  pkl_ast_node t2 = PKL_AST_TYPE_A_ETYPE (PKL_AST_TYPE (op2));

  int restart = 0;

  if (pkl_ast_type_equal (t1, t2))
    PKL_PASS_DONE;

  switch (PKL_AST_TYPE_CODE (t2))
    {
    case PKL_TYPE_INTEGRAL:
      if (!promote_integral (PKL_PASS_AST,
                             PKL_AST_TYPE_I_SIZE (t2),
                             PKL_AST_TYPE_I_SIGNED_P (t2),
                             &PKL_AST_EXP_OPERAND (exp, 0),
                             &restart))
        goto error;

      PKL_PASS_RESTART = 1;
      break;
    case PKL_TYPE_OFFSET:
      {
        pkl_ast_node base_type = PKL_AST_TYPE_O_BASE_TYPE (t2);
        pkl_ast_node unit = PKL_AST_TYPE_O_UNIT (t2);

        size_t size = PKL_AST_TYPE_I_SIZE (base_type);
        int signed_p = PKL_AST_TYPE_I_SIGNED_P (base_type);

        if (!promote_offset (PKL_PASS_AST,
                             size, signed_p, unit,
                             &PKL_AST_EXP_OPERAND (exp, 0),
                             &restart))
          goto error;

        PKL_PASS_RESTART = 1;
      }
      break;
    case PKL_TYPE_STRING:
      break;
    default:
      assert (0);
      break;
    }

  PKL_PASS_DONE;

 error:
  PKL_ICE (PKL_AST_LOC (op1),
           "couldn't promote operand argument");
  PKL_PASS_ERROR;
}
PKL_PHASE_END_HANDLER

/* The fields in struct constructors may have to be promoted to their
   corresponding types.  */

PKL_PHASE_BEGIN_HANDLER (pkl_promo_ps_scons)
{
  pkl_ast_node scons = PKL_PASS_NODE;
  pkl_ast_node scons_type = PKL_AST_SCONS_TYPE (scons);
  pkl_ast_node astruct = PKL_AST_SCONS_VALUE (scons);
  pkl_ast_node struct_fields = PKL_AST_STRUCT_FIELDS (astruct);
  pkl_ast_node elem = NULL;

  for (elem = struct_fields; elem; elem = PKL_AST_CHAIN (elem))
    {
      pkl_ast_node type_elem;

      pkl_ast_node elem_name = PKL_AST_STRUCT_FIELD_NAME (elem);
      pkl_ast_node elem_exp = PKL_AST_STRUCT_FIELD_EXP (elem);
      pkl_ast_node elem_type = PKL_AST_TYPE (elem_exp);

      /* Look for the target type of this struct element.  As per
         typify, the later can always be promoted to the first.  */

      for (type_elem = PKL_AST_TYPE_S_ELEMS (scons_type);
           type_elem;
           type_elem = PKL_AST_CHAIN (type_elem))
        {
          pkl_ast_node type_elem_name;

          /* Process only struct type fields.  */
          if (PKL_AST_CODE (type_elem) == PKL_AST_STRUCT_TYPE_FIELD)
            {
              type_elem_name = PKL_AST_STRUCT_TYPE_FIELD_NAME (type_elem);
              if (type_elem_name
                  && STREQ (PKL_AST_IDENTIFIER_POINTER (type_elem_name),
                            PKL_AST_IDENTIFIER_POINTER (elem_name)))
                {
                  pkl_ast_node type_elem_type
                    = PKL_AST_STRUCT_TYPE_FIELD_TYPE (type_elem);

                  if (!pkl_ast_type_equal (elem_type, type_elem_type)
                      || (PKL_AST_TYPE_CODE (type_elem_type) == PKL_TYPE_ARRAY))
                    {
                      int restart = 0;

                      switch (PKL_AST_TYPE_CODE (type_elem_type))
                        {
                        case PKL_TYPE_INTEGRAL:
                          if (!promote_integral (PKL_PASS_AST,
                                                 PKL_AST_TYPE_I_SIZE (type_elem_type),
                                                 PKL_AST_TYPE_I_SIGNED_P (type_elem_type),
                                                 &PKL_AST_STRUCT_FIELD_EXP (elem),
                                                 &restart))
                            goto error;

                          PKL_PASS_RESTART |= restart;
                          break;
                        case PKL_TYPE_OFFSET:
                          {
                            pkl_ast_node base_type
                              = PKL_AST_TYPE_O_BASE_TYPE (type_elem_type);
                            pkl_ast_node unit
                              = PKL_AST_TYPE_O_UNIT (type_elem_type);

                            size_t size = PKL_AST_TYPE_I_SIZE (base_type);
                            int signed_p = PKL_AST_TYPE_I_SIGNED_P (base_type);

                            if (!promote_offset (PKL_PASS_AST,
                                                 size, signed_p, unit,
                                                 &PKL_AST_STRUCT_FIELD_EXP (elem),
                                                 &restart))
                              goto error;

                            PKL_PASS_RESTART |= restart;
                            break;
                          }
                        case PKL_TYPE_ARRAY:
                          /* Array promotion of scons initializers is
                             performed in the constructor, not here.  This
                             is because the target type's bounder shall be
                             created in the constructor's environment.  */
                          break;
                        default:
                          assert (0);
                          break;
                        }
                    }
                }
            }
        }

      continue;
    error:
      PKL_ICE (PKL_AST_LOC (elem),
               "couldn't promote field in struct constructor");
      PKL_PASS_ERROR;
    }

  PKL_PASS_DONE;
}
PKL_PHASE_END_HANDLER

struct pkl_phase pkl_phase_promo
  __attribute__ ((visibility ("hidden"))) =
  {
   PKL_PHASE_PS_OP_HANDLER (PKL_AST_OP_EQ, pkl_promo_ps_op_rela),
   PKL_PHASE_PS_OP_HANDLER (PKL_AST_OP_NE, pkl_promo_ps_op_rela),
   PKL_PHASE_PS_OP_HANDLER (PKL_AST_OP_LT, pkl_promo_ps_op_rela),
   PKL_PHASE_PS_OP_HANDLER (PKL_AST_OP_GT, pkl_promo_ps_op_rela),
   PKL_PHASE_PS_OP_HANDLER (PKL_AST_OP_LE, pkl_promo_ps_op_rela),
   PKL_PHASE_PS_OP_HANDLER (PKL_AST_OP_GE, pkl_promo_ps_op_rela),
   PKL_PHASE_PS_OP_HANDLER (PKL_AST_OP_SL, pkl_promo_ps_op_bshiftpow),
   PKL_PHASE_PS_OP_HANDLER (PKL_AST_OP_SR, pkl_promo_ps_op_bshiftpow),
   PKL_PHASE_PS_OP_HANDLER (PKL_AST_OP_IOR, pkl_promo_ps_op_binary_intoffstrarr),
   PKL_PHASE_PS_OP_HANDLER (PKL_AST_OP_XOR, pkl_promo_ps_op_binary_intoffstrarr),
   PKL_PHASE_PS_OP_HANDLER (PKL_AST_OP_BAND, pkl_promo_ps_op_binary_intoffstrarr),
   PKL_PHASE_PS_OP_HANDLER (PKL_AST_OP_AND, pkl_promo_ps_op_binary),
   PKL_PHASE_PS_OP_HANDLER (PKL_AST_OP_OR, pkl_promo_ps_op_binary),
   PKL_PHASE_PS_OP_HANDLER (PKL_AST_OP_NOT, pkl_promo_ps_op_unary),
   PKL_PHASE_PS_OP_HANDLER (PKL_AST_OP_NEG, pkl_promo_ps_op_unary),
   PKL_PHASE_PS_OP_HANDLER (PKL_AST_OP_POS, pkl_promo_ps_op_unary),
   PKL_PHASE_PS_OP_HANDLER (PKL_AST_OP_BNOT, pkl_promo_ps_op_unary),
   PKL_PHASE_PS_OP_HANDLER (PKL_AST_OP_ADD, pkl_promo_ps_op_binary_intoffstrarr),
   PKL_PHASE_PS_OP_HANDLER (PKL_AST_OP_SUB, pkl_promo_ps_op_binary_intoffstrarr),
   PKL_PHASE_PS_OP_HANDLER (PKL_AST_OP_MOD, pkl_promo_ps_op_binary_intoffstrarr),
   PKL_PHASE_PS_OP_HANDLER (PKL_AST_OP_MUL, pkl_promo_ps_op_mul),
   PKL_PHASE_PS_OP_HANDLER (PKL_AST_OP_BCONC, pkl_promo_ps_op_bconc),
   PKL_PHASE_PS_OP_HANDLER (PKL_AST_OP_POW, pkl_promo_ps_op_bshiftpow),
   PKL_PHASE_PS_OP_HANDLER (PKL_AST_OP_DIV, pkl_promo_ps_op_div),
   PKL_PHASE_PS_OP_HANDLER (PKL_AST_OP_CEILDIV, pkl_promo_ps_op_div),
   PKL_PHASE_PS_OP_HANDLER (PKL_AST_OP_IN, pkl_promo_ps_op_in),
   PKL_PHASE_PS_HANDLER (PKL_AST_FUNC_ARG, pkl_promo_ps_func_arg),
   PKL_PHASE_PS_HANDLER (PKL_AST_MAP, pkl_promo_ps_map),
   PKL_PHASE_PS_HANDLER (PKL_AST_INDEXER, pkl_promo_ps_indexer),
   PKL_PHASE_PS_HANDLER (PKL_AST_TRIMMER, pkl_promo_ps_trimmer),
   PKL_PHASE_PS_HANDLER (PKL_AST_ARRAY_INITIALIZER, pkl_promo_ps_array_initializer),
   PKL_PHASE_PS_HANDLER (PKL_AST_FUNCALL, pkl_promo_ps_funcall),
   PKL_PHASE_PS_HANDLER (PKL_AST_ASS_STMT, pkl_promo_ps_ass_stmt),
   PKL_PHASE_PS_HANDLER (PKL_AST_RETURN_STMT, pkl_promo_ps_return_stmt),
   PKL_PHASE_PS_HANDLER (PKL_AST_PRINT_STMT, pkl_promo_ps_print_stmt),
   PKL_PHASE_PS_HANDLER (PKL_AST_IF_STMT, pkl_promo_ps_if_stmt),
   PKL_PHASE_PS_HANDLER (PKL_AST_LOOP_STMT, pkl_promo_ps_loop_stmt),
   PKL_PHASE_PS_HANDLER (PKL_AST_STRUCT_TYPE_FIELD, pkl_promo_ps_struct_type_field),
   PKL_PHASE_PS_HANDLER (PKL_AST_COND_EXP, pkl_promo_ps_cond_exp),
   PKL_PHASE_PS_HANDLER (PKL_AST_SCONS, pkl_promo_ps_scons),
   PKL_PHASE_PS_TYPE_HANDLER (PKL_TYPE_ARRAY, pkl_promo_ps_type_array),
   PKL_PHASE_PS_TYPE_HANDLER (PKL_TYPE_OFFSET, pkl_promo_ps_type_offset),
  };
