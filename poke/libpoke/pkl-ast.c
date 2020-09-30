/* pkl-ast.c - Abstract Syntax Tree for Poke.  */

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
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include "xalloc.h"

#include "pvm.h"
#include "pvm-alloc.h" /* For pvm_alloc_{add/remove}_gc_roots */
#include "pk-utils.h"
#include "pkl-ast.h"

/* Allocate and return a new AST node, with the given CODE.  The rest
   of the node is initialized to zero.  */

static pkl_ast_node
pkl_ast_make_node (pkl_ast ast,
                   enum pkl_ast_code code)
{
  pkl_ast_node node;

  node = xzalloc (sizeof (union pkl_ast_node));
  PKL_AST_AST (node) = ast;
  PKL_AST_CODE (node) = code;
  PKL_AST_UID (node) = ast->uid++;

  return node;
}

/* Chain AST2 at the end of the tree node chain in AST1.  If AST1 is
   null then it returns AST2.  */

pkl_ast_node
pkl_ast_chainon (pkl_ast_node ast1, pkl_ast_node ast2)
{
  if (ast1)
    {
      pkl_ast_node tmp;

      for (tmp = ast1; PKL_AST_CHAIN (tmp); tmp = PKL_AST_CHAIN (tmp))
        if (tmp == ast2)
          abort ();

      PKL_AST_CHAIN (tmp) = ASTREF (ast2);
      return ast1;
    }

  return ast2;
}

/* Build and return an AST node for an integer constant.  */

pkl_ast_node
pkl_ast_make_integer (pkl_ast ast,
                      uint64_t value)
{
  pkl_ast_node new = pkl_ast_make_node (ast, PKL_AST_INTEGER);

  PKL_AST_INTEGER_VALUE (new) = value;
  PKL_AST_LITERAL_P (new) = 1;

  return new;
}

/* Build and return an AST node for a string constant.  */

pkl_ast_node
pkl_ast_make_string (pkl_ast ast,
                     const char *str)
{
  pkl_ast_node new = pkl_ast_make_node (ast, PKL_AST_STRING);

  assert (str);

  PKL_AST_STRING_POINTER (new) = xstrdup (str);
  PKL_AST_STRING_LENGTH (new) = strlen (str);
  PKL_AST_LITERAL_P (new) = 1;

  return new;
}

/* Build and return an AST node for an identifier.  */

pkl_ast_node
pkl_ast_make_identifier (pkl_ast ast,
                         const char *str)
{
  pkl_ast_node id = pkl_ast_make_node (ast, PKL_AST_IDENTIFIER);

  PKL_AST_IDENTIFIER_POINTER (id) = xstrdup (str);
  PKL_AST_IDENTIFIER_LENGTH (id) = strlen (str);

  return id;
}

/* Build and return an AST node for an enumerator.  */

pkl_ast_node
pkl_ast_make_enumerator (pkl_ast ast,
                         pkl_ast_node identifier,
                         pkl_ast_node value)
{
  pkl_ast_node enumerator
    = pkl_ast_make_node (ast, PKL_AST_ENUMERATOR);

  assert (identifier != NULL);

  PKL_AST_ENUMERATOR_IDENTIFIER (enumerator) = ASTREF (identifier);
  PKL_AST_ENUMERATOR_VALUE (enumerator) = ASTREF (value);

  return enumerator;
}

/* Build and return an AST node for a conditional expression.  */

pkl_ast_node
pkl_ast_make_cond_exp (pkl_ast ast,
                       pkl_ast_node cond,
                       pkl_ast_node thenexp,
                       pkl_ast_node elseexp)
{
  pkl_ast_node cond_exp
    = pkl_ast_make_node (ast, PKL_AST_COND_EXP);

  assert (cond && thenexp && elseexp);

  PKL_AST_COND_EXP_COND (cond_exp) = ASTREF (cond);
  PKL_AST_COND_EXP_THENEXP (cond_exp) = ASTREF (thenexp);
  PKL_AST_COND_EXP_ELSEEXP (cond_exp) = ASTREF (elseexp);

  PKL_AST_LITERAL_P (cond_exp)
    = PKL_AST_LITERAL_P (thenexp) && PKL_AST_LITERAL_P (elseexp);

  return cond_exp;
}

/* Build and return an AST node for a binary expression.  */

pkl_ast_node
pkl_ast_make_binary_exp (pkl_ast ast,
                         enum pkl_ast_op code,
                         pkl_ast_node op1,
                         pkl_ast_node op2)
{
  pkl_ast_node exp = pkl_ast_make_node (ast, PKL_AST_EXP);

  assert (op1 && op2);

  PKL_AST_EXP_CODE (exp) = code;
  PKL_AST_EXP_ATTR (exp) = PKL_AST_ATTR_NONE;
  PKL_AST_EXP_NUMOPS (exp) = 2;
  PKL_AST_EXP_OPERAND (exp, 0) = ASTREF (op1);
  PKL_AST_EXP_OPERAND (exp, 1) = ASTREF (op2);

  PKL_AST_LITERAL_P (exp)
    = PKL_AST_LITERAL_P (op1) && PKL_AST_LITERAL_P (op2);

  return exp;
}

/* Return the written form of the given attribute code.  This returns
   NULL for PKL_AST_ATTR_NONE */

const char *
pkl_attr_name (enum pkl_ast_attr attr)
{
#define PKL_DEF_ATTR(SYM, STRING) STRING,
  static const char* attr_names[] =
    {
#include "pkl-attrs.def"
     NULL
    };
#undef PKL_DEF_ATTR

  return attr_names[attr];
}

/* Build and return an AST node for an unary expression.  */

pkl_ast_node
pkl_ast_make_unary_exp (pkl_ast ast,
                        enum pkl_ast_op code,
                        pkl_ast_node op)
{
  pkl_ast_node exp = pkl_ast_make_node (ast, PKL_AST_EXP);

  PKL_AST_EXP_CODE (exp) = code;
  PKL_AST_EXP_ATTR (exp) = PKL_AST_ATTR_NONE;
  PKL_AST_EXP_NUMOPS (exp) = 1;
  PKL_AST_EXP_OPERAND (exp, 0) = ASTREF (op);
  PKL_AST_LITERAL_P (exp) = PKL_AST_LITERAL_P (op);

  return exp;
}

/* Build and return an AST node for a function definition.  */

pkl_ast_node
pkl_ast_make_func (pkl_ast ast, pkl_ast_node ret_type,
                   pkl_ast_node args, pkl_ast_node body)
{
  pkl_ast_node func = pkl_ast_make_node (ast, PKL_AST_FUNC);

  assert (body);

  if (ret_type)
    PKL_AST_FUNC_RET_TYPE (func) = ASTREF (ret_type);
  if (args)
    PKL_AST_FUNC_ARGS (func) = ASTREF (args);
  PKL_AST_FUNC_BODY (func) = ASTREF (body);

  PKL_AST_FUNC_FIRST_OPT_ARG (func) = NULL;

  return func;
}

/* Build and return an AST node for a function definition formal
   argument.  */

pkl_ast_node
pkl_ast_make_func_arg (pkl_ast ast, pkl_ast_node type,
                       pkl_ast_node identifier,
                       pkl_ast_node initial)
{
  pkl_ast_node func_arg = pkl_ast_make_node (ast, PKL_AST_FUNC_ARG);

  assert (identifier);

  PKL_AST_FUNC_ARG_TYPE (func_arg) = ASTREF (type);
  PKL_AST_FUNC_ARG_IDENTIFIER (func_arg) = ASTREF (identifier);
  if (initial)
    PKL_AST_FUNC_ARG_INITIAL (func_arg) = ASTREF (initial);
  PKL_AST_FUNC_ARG_VARARG (func_arg) = 0;

  return func_arg;
}

/* Build and return an AST node for a trimmer.  */

pkl_ast_node
pkl_ast_make_trimmer (pkl_ast ast, pkl_ast_node entity,
                      pkl_ast_node from, pkl_ast_node to)
{
  pkl_ast_node trimmer = pkl_ast_make_node (ast, PKL_AST_TRIMMER);

  PKL_AST_TRIMMER_ENTITY (trimmer) = ASTREF (entity);
  if (from)
    PKL_AST_TRIMMER_FROM (trimmer) = ASTREF (from);
  if (to)
    PKL_AST_TRIMMER_TO (trimmer) = ASTREF (to);

  return trimmer;
}

/* Build and return an AST node for an indexer.  */

pkl_ast_node
pkl_ast_make_indexer (pkl_ast ast,
                        pkl_ast_node entity, pkl_ast_node index)
{
  pkl_ast_node indexer = pkl_ast_make_node (ast, PKL_AST_INDEXER);

  assert (entity && index);

  PKL_AST_INDEXER_ENTITY (indexer) = ASTREF (entity);
  PKL_AST_INDEXER_INDEX (indexer) = ASTREF (index);
  PKL_AST_LITERAL_P (indexer) = 0;

  return indexer;
}

/* Build and return an AST node for a struct reference.  */

pkl_ast_node
pkl_ast_make_struct_ref (pkl_ast ast,
                         pkl_ast_node sct, pkl_ast_node identifier)
{
  pkl_ast_node sref = pkl_ast_make_node (ast, PKL_AST_STRUCT_REF);

  assert (sct && identifier);

  PKL_AST_STRUCT_REF_STRUCT (sref) = ASTREF (sct);
  PKL_AST_STRUCT_REF_IDENTIFIER (sref) = ASTREF (identifier);

  return sref;
}

/* Build and return type AST nodes.  */

static pkl_ast_node
pkl_ast_make_type (pkl_ast ast)
{
  pkl_ast_node type = pkl_ast_make_node (ast, PKL_AST_TYPE);

  PKL_AST_TYPE_NAME (type) = NULL;
  PKL_AST_TYPE_COMPLETE (type)
    = PKL_AST_TYPE_COMPLETE_UNKNOWN;
  return type;
}

pkl_ast_node
pkl_ast_make_named_type (pkl_ast ast, pkl_ast_node name)
{
  pkl_ast_node type = pkl_ast_make_type (ast);

  assert (name);

  PKL_AST_TYPE_NAME (type) = ASTREF (name);
  return type;
}

pkl_ast_node
pkl_ast_make_integral_type (pkl_ast ast, size_t size, int signed_p)
{
  pkl_ast_node type = pkl_ast_make_type (ast);

  assert (signed_p == 0 || signed_p == 1);

  PKL_AST_TYPE_CODE (type) = PKL_TYPE_INTEGRAL;
  PKL_AST_TYPE_COMPLETE (type)
    = PKL_AST_TYPE_COMPLETE_YES;
  PKL_AST_TYPE_I_SIGNED_P (type) = signed_p;
  PKL_AST_TYPE_I_SIZE (type) = size;
  return type;
}

pkl_ast_node
pkl_ast_make_array_type (pkl_ast ast, pkl_ast_node etype, pkl_ast_node bound)
{
  pkl_ast_node type = pkl_ast_make_type (ast);

  assert (etype);

  PKL_AST_TYPE_CODE (type) = PKL_TYPE_ARRAY;
  PKL_AST_TYPE_A_ETYPE (type) = ASTREF (etype);
  if (bound)
    PKL_AST_TYPE_A_BOUND (type) = ASTREF (bound);

  PKL_AST_TYPE_A_MAPPER (type) = PVM_NULL;
  PKL_AST_TYPE_A_WRITER (type) = PVM_NULL;
  PKL_AST_TYPE_A_BOUNDER (type) = PVM_NULL;
  PKL_AST_TYPE_A_CONSTRUCTOR (type) = PVM_NULL;

  /* The closure slots are GC roots.  */
  pvm_alloc_add_gc_roots (&PKL_AST_TYPE_A_MAPPER (type), 1);
  pvm_alloc_add_gc_roots (&PKL_AST_TYPE_A_WRITER (type), 1);
  pvm_alloc_add_gc_roots (&PKL_AST_TYPE_A_BOUNDER (type), 1);
  pvm_alloc_add_gc_roots (&PKL_AST_TYPE_A_CONSTRUCTOR (type), 1);

  return type;
}

pkl_ast_node
pkl_ast_make_string_type (pkl_ast ast)
{
  pkl_ast_node type = pkl_ast_make_type (ast);

  PKL_AST_TYPE_CODE (type) = PKL_TYPE_STRING;
  PKL_AST_TYPE_COMPLETE (type)
    = PKL_AST_TYPE_COMPLETE_NO;
  return type;
}

pkl_ast_node
pkl_ast_make_void_type (pkl_ast ast)
{
  pkl_ast_node type = pkl_ast_make_type (ast);

  PKL_AST_TYPE_CODE (type) = PKL_TYPE_VOID;
  PKL_AST_TYPE_COMPLETE (type)
    = PKL_AST_TYPE_COMPLETE_NO;
  return type;
}

pkl_ast_node
pkl_ast_make_offset_type (pkl_ast ast,
                          pkl_ast_node base_type,
                          pkl_ast_node unit)
{
  pkl_ast_node type = pkl_ast_make_type (ast);

  assert (base_type && unit);

  PKL_AST_TYPE_CODE (type) = PKL_TYPE_OFFSET;
  PKL_AST_TYPE_COMPLETE (type)
    = PKL_AST_TYPE_COMPLETE_YES;
  PKL_AST_TYPE_O_UNIT (type) = ASTREF (unit);
  PKL_AST_TYPE_O_BASE_TYPE (type) = ASTREF (base_type);

  return type;
}

pkl_ast_node
pkl_ast_make_struct_type (pkl_ast ast,
                          size_t nelem,
                          size_t nfield,
                          size_t ndecl,
                          pkl_ast_node itype,
                          pkl_ast_node struct_type_elems,
                          int pinned_p, int union_p)
{
  pkl_ast_node type = pkl_ast_make_type (ast);

  PKL_AST_TYPE_CODE (type) = PKL_TYPE_STRUCT;
  PKL_AST_TYPE_S_NELEM (type) = nelem;
  PKL_AST_TYPE_S_NFIELD (type) = nfield;
  PKL_AST_TYPE_S_NDECL (type) = ndecl;
  if (struct_type_elems)
    PKL_AST_TYPE_S_ELEMS (type) = ASTREF (struct_type_elems);
  if (itype)
    PKL_AST_TYPE_S_ITYPE (type) = ASTREF (itype);
  PKL_AST_TYPE_S_PINNED_P (type) = pinned_p;
  PKL_AST_TYPE_S_UNION_P (type) = union_p;
  PKL_AST_TYPE_S_MAPPER (type) = PVM_NULL;
  PKL_AST_TYPE_S_WRITER (type) = PVM_NULL;
  PKL_AST_TYPE_S_CONSTRUCTOR (type) = PVM_NULL;
  PKL_AST_TYPE_S_COMPARATOR (type) = PVM_NULL;
  PKL_AST_TYPE_S_INTEGRATOR (type) = PVM_NULL;

  /* The closure slots are GC roots.  */
  pvm_alloc_add_gc_roots (&PKL_AST_TYPE_S_WRITER (type), 1);
  pvm_alloc_add_gc_roots (&PKL_AST_TYPE_S_MAPPER (type), 1);
  pvm_alloc_add_gc_roots (&PKL_AST_TYPE_S_CONSTRUCTOR (type), 1);
  pvm_alloc_add_gc_roots (&PKL_AST_TYPE_S_COMPARATOR (type), 1);
  pvm_alloc_add_gc_roots (&PKL_AST_TYPE_S_INTEGRATOR (type), 1);

  return type;
}

pkl_ast_node
pkl_ast_make_struct_type_field (pkl_ast ast,
                                pkl_ast_node name,
                                pkl_ast_node type,
                                pkl_ast_node constraint,
                                pkl_ast_node initializer,
                                pkl_ast_node label,
                                int endian,
                                pkl_ast_node optcond)
{
  pkl_ast_node struct_type_elem
    = pkl_ast_make_node (ast, PKL_AST_STRUCT_TYPE_FIELD);

  PKL_AST_STRUCT_TYPE_FIELD_NAME (struct_type_elem)
    = ASTREF (name);
  PKL_AST_STRUCT_TYPE_FIELD_TYPE (struct_type_elem)
    = ASTREF (type);
  if (constraint)
    PKL_AST_STRUCT_TYPE_FIELD_CONSTRAINT (struct_type_elem)
      = ASTREF (constraint);
  if (label)
    PKL_AST_STRUCT_TYPE_FIELD_LABEL (struct_type_elem)
      = ASTREF (label);
  if (optcond)
    PKL_AST_STRUCT_TYPE_FIELD_OPTCOND (struct_type_elem)
      = ASTREF (optcond);
  if (initializer)
    PKL_AST_STRUCT_TYPE_FIELD_INITIALIZER (struct_type_elem)
      = ASTREF (initializer);

  PKL_AST_STRUCT_TYPE_FIELD_ENDIAN (struct_type_elem)
    = endian;

  return struct_type_elem;
}

pkl_ast_node
pkl_ast_make_function_type (pkl_ast ast, pkl_ast_node rtype,
                            size_t narg, pkl_ast_node args)
{
  pkl_ast_node type = pkl_ast_make_type (ast);

  PKL_AST_TYPE_CODE (type) = PKL_TYPE_FUNCTION;
  PKL_AST_TYPE_F_RTYPE (type) = ASTREF (rtype);
  PKL_AST_TYPE_F_NARG (type) = narg;
  PKL_AST_TYPE_F_ARGS (type) = ASTREF (args);
  PKL_AST_TYPE_F_VARARG (type) = 0;
  PKL_AST_TYPE_F_FIRST_OPT_ARG (type) = NULL;

  return type;
}

pkl_ast_node
pkl_ast_make_any_type (pkl_ast ast)
{
  pkl_ast_node type = pkl_ast_make_type (ast);
  PKL_AST_TYPE_CODE (type) = PKL_TYPE_ANY;
  return type;
}

pkl_ast_node
pkl_ast_make_func_type_arg (pkl_ast ast, pkl_ast_node type,
                            pkl_ast_node name)
{
  pkl_ast_node function_type_arg
    = pkl_ast_make_node (ast, PKL_AST_FUNC_TYPE_ARG);

  PKL_AST_FUNC_TYPE_ARG_TYPE (function_type_arg)
    = ASTREF (type);
  if (name)
    PKL_AST_FUNC_TYPE_ARG_NAME (function_type_arg)
      = ASTREF (name);
  PKL_AST_FUNC_TYPE_ARG_OPTIONAL (function_type_arg) = 0;
  PKL_AST_FUNC_TYPE_ARG_VARARG (function_type_arg) = 0;

  return function_type_arg;
}

/* Allocate and return a duplicated type AST node.  */

pkl_ast_node
pkl_ast_dup_type (pkl_ast_node type)
{
  pkl_ast_node t, new = pkl_ast_make_type (PKL_AST_AST (type));

  PKL_AST_TYPE_CODE (new) = PKL_AST_TYPE_CODE (type);
  PKL_AST_TYPE_COMPLETE (new) = PKL_AST_TYPE_COMPLETE (type);

  switch (PKL_AST_TYPE_CODE (type))
    {
    case PKL_TYPE_ANY:
      break;
    case PKL_TYPE_INTEGRAL:
      PKL_AST_TYPE_I_SIZE (new) = PKL_AST_TYPE_I_SIZE (type);
      PKL_AST_TYPE_I_SIGNED_P (new) = PKL_AST_TYPE_I_SIGNED_P (type);
      break;
    case PKL_TYPE_ARRAY:
      {
        pkl_ast_node etype
          = pkl_ast_dup_type (PKL_AST_TYPE_A_ETYPE (type));
        PKL_AST_TYPE_A_BOUND (new) = ASTREF (PKL_AST_TYPE_A_BOUND (type));
        PKL_AST_TYPE_A_ETYPE (new) = ASTREF (etype);
      }
      break;
    case PKL_TYPE_STRUCT:
      PKL_AST_TYPE_S_NELEM (new) = PKL_AST_TYPE_S_NELEM (type);
      PKL_AST_TYPE_S_NFIELD (new) = PKL_AST_TYPE_S_NFIELD (type);
      PKL_AST_TYPE_S_NDECL (new) = PKL_AST_TYPE_S_NDECL (type);
      for (t = PKL_AST_TYPE_S_ELEMS (type); t; t = PKL_AST_CHAIN (t))
        {
          pkl_ast_node struct_type_elem_name;
          pkl_ast_node struct_type_elem_type;
          pkl_ast_node struct_type_elem_constraint;
          pkl_ast_node struct_type_elem_initializer;
          pkl_ast_node struct_type_elem_label;
          pkl_ast_node new_struct_type_elem_name;
          pkl_ast_node struct_type_elem;
          pkl_ast_node struct_type_elem_optcond;
          int struct_type_elem_endian;

          /* Process only struct type fields.  XXX But what about
             declarations?  These should also be duplicated.  */
          if (PKL_AST_CODE (t) != PKL_AST_STRUCT_TYPE_FIELD)
            break;

          struct_type_elem_name = PKL_AST_STRUCT_TYPE_FIELD_NAME (t);
          struct_type_elem_type = PKL_AST_STRUCT_TYPE_FIELD_TYPE (t);
          struct_type_elem_constraint = PKL_AST_STRUCT_TYPE_FIELD_CONSTRAINT (t);
          struct_type_elem_initializer = PKL_AST_STRUCT_TYPE_FIELD_INITIALIZER (t);
          struct_type_elem_label = PKL_AST_STRUCT_TYPE_FIELD_LABEL (t);
          struct_type_elem_endian = PKL_AST_STRUCT_TYPE_FIELD_ENDIAN (t);
          struct_type_elem_optcond = PKL_AST_STRUCT_TYPE_FIELD_OPTCOND (t);

          new_struct_type_elem_name
            = (struct_type_elem_name
               ? pkl_ast_make_identifier (PKL_AST_AST (new),
                                          PKL_AST_IDENTIFIER_POINTER (struct_type_elem_name))
               : NULL);
          struct_type_elem
            = pkl_ast_make_struct_type_field (PKL_AST_AST (new),
                                              new_struct_type_elem_name,
                                              pkl_ast_dup_type (struct_type_elem_type),
                                              struct_type_elem_constraint,
                                              struct_type_elem_initializer,
                                              struct_type_elem_label,
                                              struct_type_elem_endian,
                                              struct_type_elem_optcond);

          PKL_AST_TYPE_S_ELEMS (new)
            = pkl_ast_chainon (PKL_AST_TYPE_S_ELEMS (new),
                               struct_type_elem);
          PKL_AST_TYPE_S_ELEMS (new) = ASTREF (PKL_AST_TYPE_S_ELEMS (type));
          PKL_AST_TYPE_S_PINNED_P (new) = PKL_AST_TYPE_S_PINNED_P (type);
          PKL_AST_TYPE_S_UNION_P (new) = PKL_AST_TYPE_S_UNION_P (type);
        }
      break;
    case PKL_TYPE_FUNCTION:
      PKL_AST_TYPE_F_RTYPE (new)
        = pkl_ast_dup_type (PKL_AST_TYPE_F_RTYPE (type));
      PKL_AST_TYPE_F_NARG (new) = PKL_AST_TYPE_F_NARG (type);
      for (t = PKL_AST_TYPE_F_ARGS (type); t; t = PKL_AST_CHAIN (t))
        {
          pkl_ast_node fun_type_arg_type
            = PKL_AST_FUNC_TYPE_ARG_TYPE (t);
          pkl_ast_node fun_type_arg_name
            = PKL_AST_FUNC_TYPE_ARG_NAME (t);

          pkl_ast_node function_type_arg
            = pkl_ast_make_func_type_arg (PKL_AST_AST (new),
                                          fun_type_arg_type,
                                          fun_type_arg_name);
          PKL_AST_FUNC_TYPE_ARG_OPTIONAL (function_type_arg)
            = PKL_AST_FUNC_TYPE_ARG_OPTIONAL (t);
          PKL_AST_FUNC_TYPE_ARG_VARARG (function_type_arg)
            = PKL_AST_FUNC_TYPE_ARG_VARARG (t);
          PKL_AST_TYPE_F_ARGS (new)
            = pkl_ast_chainon (PKL_AST_TYPE_F_ARGS (new),
                               function_type_arg);
          PKL_AST_TYPE_F_ARGS (new) = ASTREF (PKL_AST_TYPE_F_ARGS (t));
        }
      PKL_AST_TYPE_F_FIRST_OPT_ARG (new)
        = ASTREF (PKL_AST_TYPE_F_FIRST_OPT_ARG (type));
      PKL_AST_TYPE_F_VARARG (new)
        = PKL_AST_TYPE_F_VARARG (type);
      break;
    case PKL_TYPE_OFFSET:
      /* Fallthrough.  */
    case PKL_TYPE_STRING:
      /* Fallthrough.  */
    default:
      break;
    }

  return new;
}

/* Given a struct type node AST and a string in the form BB.CC.CC.xx,
   check that the intermediate fields are valid struct references, and return
   the pkl_ast_node corresponding to the type of the latest field CC. */

pkl_ast_node
pkl_struct_type_traverse (pkl_ast_node type, const char *path)
{
  char *trunk, *sub, *base;

  if (PKL_AST_TYPE_CODE (type) != PKL_TYPE_STRUCT)
    return NULL;

  trunk = strndup (path, strlen (path) - strlen (strrchr (path, '.')));
  base = strtok (trunk, ".");

  /* Node in the form XX. Check to silence the compiler about base not used */
  if (base == NULL)
    {
      free (trunk);
      return type;
    }

  while ((sub = strtok (NULL, ".")) != NULL)
    {
      pkl_ast_node ename;
      pkl_ast_node etype, t;
      char *field;

      etype = NULL;

      if (PKL_AST_TYPE_CODE (type) != PKL_TYPE_STRUCT)
        goto out;

      for (t = PKL_AST_TYPE_S_ELEMS (type); t;
           t = PKL_AST_CHAIN (t))
        {
          if (PKL_AST_CODE (t) == PKL_AST_STRUCT_TYPE_FIELD)
            {
              ename = PKL_AST_STRUCT_TYPE_FIELD_NAME (t);
              etype = PKL_AST_STRUCT_TYPE_FIELD_TYPE (t);

              field = PKL_AST_IDENTIFIER_POINTER (ename);

              if (STREQ (field, sub))
                {
                  type = etype;
                  break;
                }
            }
        }

      if (type != etype)
        goto out;
    }

  free (trunk);
  return type;

out:
  free (trunk);
  return NULL;
}

/* Return whether the given type AST node corresponds to an exception
   type.  */

int
pkl_ast_type_is_exception (pkl_ast_node type)
{
  pkl_ast_node type_name = PKL_AST_TYPE_NAME (type);

  return (PKL_AST_TYPE_CODE (type) == PKL_TYPE_STRUCT
          && type_name
          && STREQ (PKL_AST_IDENTIFIER_POINTER (type_name), "Exception"));
}

/* Remove bounder closures from the given array type.  This works
   recursively for arrays of arrays.  */

void
pkl_ast_array_type_remove_bounders (pkl_ast_node type)
{
  pkl_ast_node etype;

  assert (PKL_AST_TYPE_CODE (type) == PKL_TYPE_ARRAY);

  PKL_AST_TYPE_A_BOUNDER (type) = PVM_NULL;

  for (etype = PKL_AST_TYPE_A_ETYPE (type);
       PKL_AST_TYPE_CODE (etype) == PKL_TYPE_ARRAY
         && PKL_AST_TYPE_NAME (etype) == NULL;
       etype = PKL_AST_TYPE_A_ETYPE (etype))
    {
      PKL_AST_TYPE_A_BOUNDER (etype) = PVM_NULL;
    }
}

/* Return whether two given type AST nodes are equal, i.e. they denote
   the same type.  */

int
pkl_ast_type_equal (pkl_ast_node a, pkl_ast_node b)
{
  if (PKL_AST_TYPE_CODE (a) != PKL_AST_TYPE_CODE (b))
    return 0;

  switch (PKL_AST_TYPE_CODE (a))
    {
    case PKL_TYPE_ANY:
      return 1;
      break;
    case PKL_TYPE_INTEGRAL:
      return (PKL_AST_TYPE_I_SIZE (a) == PKL_AST_TYPE_I_SIZE (b)
              && PKL_AST_TYPE_I_SIGNED_P (a) == PKL_AST_TYPE_I_SIGNED_P (b));
      break;
    case PKL_TYPE_ARRAY:
      {
        /* If the array types denote static arrays, i.e. the array
           types are bounded by a _constant_ number of elements then
           we can actually do some control here.  */
        pkl_ast_node ba = PKL_AST_TYPE_A_BOUND (a);
        pkl_ast_node bb = PKL_AST_TYPE_A_BOUND (b);

        if (ba && bb)
          {
            pkl_ast_node tba = PKL_AST_TYPE (ba);
            pkl_ast_node tbb = PKL_AST_TYPE (bb);

            if (PKL_AST_TYPE_CODE (tba) == PKL_TYPE_INTEGRAL
                && PKL_AST_CODE (ba) == PKL_AST_INTEGER
                && PKL_AST_TYPE_CODE (tbb) == PKL_TYPE_INTEGRAL
                && PKL_AST_CODE (bb) == PKL_AST_INTEGER)
              {
                if (PKL_AST_INTEGER_VALUE (ba)
                    != PKL_AST_INTEGER_VALUE (bb))
                  return 0;
              }
          }

        return pkl_ast_type_equal (PKL_AST_TYPE_A_ETYPE (a),
                                   PKL_AST_TYPE_A_ETYPE (b));
        break;
      }
    case PKL_TYPE_STRUCT:
      {
        /* Anonymous structs are always unequal.  */
        if (PKL_AST_TYPE_NAME (a) == NULL || PKL_AST_TYPE_NAME (b) == NULL)
          return 0;

        /* Struct types are compared by name.  */
        return (STREQ (PKL_AST_IDENTIFIER_POINTER (PKL_AST_TYPE_NAME (a)),
                       PKL_AST_IDENTIFIER_POINTER (PKL_AST_TYPE_NAME (b))));
        break;
      }
    case PKL_TYPE_FUNCTION:
      {
        pkl_ast_node fa, fb;

        if (PKL_AST_TYPE_F_NARG (a) != PKL_AST_TYPE_F_NARG (b))
          return 0;
        for (fa = PKL_AST_TYPE_F_ARGS (a), fb = PKL_AST_TYPE_F_ARGS (b);
             fa && fb;
             fa = PKL_AST_CHAIN (fa), fb = PKL_AST_CHAIN (fb))
          {
            if (PKL_AST_FUNC_TYPE_ARG_OPTIONAL (fa)
                != PKL_AST_FUNC_TYPE_ARG_OPTIONAL (fb))
              return 0;

            if (PKL_AST_FUNC_TYPE_ARG_VARARG (fa)
                != PKL_AST_FUNC_TYPE_ARG_VARARG (fb))
              return 0;

            if (!pkl_ast_type_equal (PKL_AST_FUNC_TYPE_ARG_TYPE (fa),
                                     PKL_AST_FUNC_TYPE_ARG_TYPE (fb)))
              return 0;
          }
        break;
      }
    case PKL_TYPE_OFFSET:
      {
        pkl_ast_node a_unit = PKL_AST_TYPE_O_UNIT (a);
        pkl_ast_node b_unit = PKL_AST_TYPE_O_UNIT (b);

        /* If the units of the types are not known yet (because they
           are identifiers, or whatever then we cannot guarantee the
           types are the same.  */
        if (PKL_AST_CODE (a_unit) != PKL_AST_INTEGER
            || PKL_AST_CODE (b_unit) != PKL_AST_INTEGER)
          return 0;

        return (PKL_AST_INTEGER_VALUE (a_unit) == PKL_AST_INTEGER_VALUE (b_unit)
                  && pkl_ast_type_equal (PKL_AST_TYPE_O_BASE_TYPE (a),
                                         PKL_AST_TYPE_O_BASE_TYPE (b)));
      }
      break;
    case PKL_TYPE_STRING:
      /* Fallthrough.  */
    default:
      break;
    }

  return 1;
}

/* Return whether the type FT is promoteable to type TT.  Note that,
   unlike pkl_ast_type_equal above, this operation is not
   generally commutative.  */

int
pkl_ast_type_promoteable (pkl_ast_node ft, pkl_ast_node tt,
                          int promote_array_of_any)
{
  if (pkl_ast_type_equal (ft, tt))
    return 1;

  /* Any type is promoteable to ANY.  */
  if (PKL_AST_TYPE_CODE (tt) == PKL_TYPE_ANY)
    return 1;

  /* An integral type is promoteable to other integral types.  */
  if (PKL_AST_TYPE_CODE (tt) == PKL_TYPE_INTEGRAL
      && PKL_AST_TYPE_CODE (ft) == PKL_TYPE_INTEGRAL)
    return 1;

  /* An offset type is promoteable to other offset types.  */
  if (PKL_AST_TYPE_CODE (tt) == PKL_TYPE_OFFSET
      && PKL_AST_TYPE_CODE (ft) == PKL_TYPE_OFFSET)
    return 1;

  /* Any array[] type is promoteable to ANY[].  */
  if (promote_array_of_any
      && PKL_AST_TYPE_CODE (ft) == PKL_TYPE_ARRAY
      && PKL_AST_TYPE_CODE (tt) == PKL_TYPE_ARRAY
      && PKL_AST_TYPE_CODE (PKL_AST_TYPE_A_ETYPE (tt)) == PKL_TYPE_ANY)
    return 1;

  /* An array type is promoteable to other array type if the types of
     the elements are equal.  Only length may differ.  */
  if (PKL_AST_TYPE_CODE (ft) == PKL_TYPE_ARRAY
      && PKL_AST_TYPE_CODE (tt) == PKL_TYPE_ARRAY)
    {
      pkl_ast_node ft_bound = PKL_AST_TYPE_A_BOUND (ft);
      pkl_ast_node tt_bound = PKL_AST_TYPE_A_BOUND (tt);
      pkl_ast_node ft_etype = PKL_AST_TYPE_A_ETYPE (ft);
      pkl_ast_node tt_etype = PKL_AST_TYPE_A_ETYPE (tt);

      if (!pkl_ast_type_equal (tt_etype, ft_etype))
        return 0;

      /* Static array types can be handled here.  */
      if (tt_bound && (PKL_AST_CODE (tt_bound) == PKL_AST_INTEGER))
        {
          if (!ft_bound || PKL_AST_CODE (ft_bound) != PKL_AST_INTEGER)
            return 0;

          if (PKL_AST_INTEGER_VALUE (ft_bound)
              != PKL_AST_INTEGER_VALUE (tt_bound))
            return 0;
        }

      return 1;
      //      return pkl_ast_type_promoteable (PKL_AST_TYPE_A_ETYPE (ft),
      //                                       PKL_AST_TYPE_A_ETYPE (tt),
      //                                       promote_array_of_any);
    }

  /* A struct type is promoteable to any integral type if the struct
     itself is integral.  */
  if (PKL_AST_TYPE_CODE (ft) == PKL_TYPE_STRUCT
      && PKL_AST_TYPE_S_ITYPE (ft)
      && PKL_AST_TYPE_CODE (tt) == PKL_TYPE_INTEGRAL)
    return 1;

  return 0;
}

/* Build and return an expression that computes the size of TYPE in
   bits, as an unsigned 64-bit value.  */

pkl_ast_node
pkl_ast_sizeof_type (pkl_ast ast, pkl_ast_node type)
{
  pkl_ast_node res;
  pkl_ast_node res_type
    = pkl_ast_make_integral_type (ast, 64, 0);
  PKL_AST_LOC (res_type) = PKL_AST_LOC (type);

  /* This function should only be called on complete types.  */
  assert (PKL_AST_TYPE_COMPLETE (type)
          == PKL_AST_TYPE_COMPLETE_YES);

  switch (PKL_AST_TYPE_CODE (type))
    {
    case PKL_TYPE_INTEGRAL:
      {
        res = pkl_ast_make_integer (ast, PKL_AST_TYPE_I_SIZE (type));
        PKL_AST_LOC (res) = PKL_AST_LOC (type);
        PKL_AST_TYPE (res) = ASTREF (res_type);
        break;
      }
    case PKL_TYPE_ARRAY:
      {
        pkl_ast_node sizeof_etype
          = pkl_ast_sizeof_type (ast,
                                 PKL_AST_TYPE_A_ETYPE (type));
        res= pkl_ast_make_binary_exp (ast, PKL_AST_OP_MUL,
                                      PKL_AST_TYPE_A_BOUND (type),
                                      sizeof_etype);
        PKL_AST_LOC (res) = PKL_AST_LOC (type);
        PKL_AST_TYPE (res) = ASTREF (res_type);
        break;
      }
    case PKL_TYPE_STRUCT:
      {
        pkl_ast_node t;

        res = pkl_ast_make_integer (ast, 0);
        PKL_AST_TYPE (res) = ASTREF (res_type);
        PKL_AST_LOC (res) = PKL_AST_LOC (type);

        for (t = PKL_AST_TYPE_S_ELEMS (type); t; t = PKL_AST_CHAIN (t))
          {
            if (PKL_AST_CODE (t) == PKL_AST_STRUCT_TYPE_FIELD)
              {
                pkl_ast_node elem_type;

                /* Struct fields with labels are not expected, as these
                   cannot appear in complete struct types.  Ditto for
                   optional fields.  */
                assert (PKL_AST_STRUCT_TYPE_FIELD_LABEL (t) == NULL);
                assert (PKL_AST_STRUCT_TYPE_FIELD_OPTCOND (t) == NULL);

                elem_type = PKL_AST_STRUCT_TYPE_FIELD_TYPE (t);
                res = pkl_ast_make_binary_exp (ast, PKL_AST_OP_ADD,
                                               res,
                                               pkl_ast_sizeof_type (ast,
                                                                    elem_type));
                PKL_AST_TYPE (res) = ASTREF (res_type);
                PKL_AST_LOC (res) = PKL_AST_LOC (type);
              }
          }

        break;
      }
    case PKL_TYPE_FUNCTION:
      {
        /* By convention functions have sizeof 0#b  */
        res = pkl_ast_make_integer (ast, 0);
        PKL_AST_TYPE (res) = ASTREF (res_type);
        PKL_AST_LOC (res) = PKL_AST_LOC (type);
        break;
      }
    case PKL_TYPE_OFFSET:
      return pkl_ast_sizeof_type (ast, PKL_AST_TYPE_O_BASE_TYPE (type));
      break;
    case PKL_TYPE_STRING:
    default:
      assert (0);
      break;
    }

  return res;
}

/* Return 1 if the given TYPE can be mapped in IO.  0 otherwise.  */

int
pkl_ast_type_mappable_p (pkl_ast_node type)
{
  switch (PKL_AST_TYPE_CODE (type))
    {
    case PKL_TYPE_INTEGRAL:
    case PKL_TYPE_STRING:
    case PKL_TYPE_OFFSET:
      return 1;
    case PKL_TYPE_ARRAY:
      return pkl_ast_type_mappable_p (PKL_AST_TYPE_A_ETYPE (type));
      break;
    case PKL_TYPE_STRUCT:
      {
        pkl_ast_node elem;

        for (elem = PKL_AST_TYPE_S_ELEMS (type);
             elem;
             elem = PKL_AST_CHAIN (elem))
          {
            if (PKL_AST_CODE (elem) == PKL_AST_STRUCT_TYPE_FIELD
                && !pkl_ast_type_mappable_p (PKL_AST_STRUCT_TYPE_FIELD_TYPE (elem)))
              return 0;
          }

        return 1;
        break;
      }
    default:
      break;
    }

  return 0;
}

/* Return PKL_AST_TYPE_COMPLETE_YES if the given TYPE is a complete
   type.  Return PKL_AST_TYPE_COMPLETE_NO otherwise.  This function
   assumes that the children of TYPE have correct completeness
   annotations.  */

int
pkl_ast_type_is_complete (pkl_ast_node type)
{
  int complete = PKL_AST_TYPE_COMPLETE_UNKNOWN;

  switch (PKL_AST_TYPE_CODE (type))
    {
      /* Integral, offset and struct types are always complete.  */
    case PKL_TYPE_INTEGRAL:
    case PKL_TYPE_OFFSET:
      complete = PKL_AST_TYPE_COMPLETE_YES;
      break;
      /* String and function types are never complete.  */
    case PKL_TYPE_FUNCTION:
    case PKL_TYPE_STRING:
      complete = PKL_AST_TYPE_COMPLETE_NO;
      break;
      /* Struct types are complete if their fields are also of
         complete types and there are no labels nor optconds.  */
    case PKL_TYPE_STRUCT:
      {
        pkl_ast_node elem;

        complete = PKL_AST_TYPE_COMPLETE_YES;
        for (elem = PKL_AST_TYPE_S_ELEMS (type);
             elem;
             elem = PKL_AST_CHAIN (elem))
          {
            if (PKL_AST_CODE (elem) == PKL_AST_STRUCT_TYPE_FIELD
                && (PKL_AST_STRUCT_TYPE_FIELD_LABEL (elem)
                    || PKL_AST_STRUCT_TYPE_FIELD_OPTCOND (elem)
                    || (pkl_ast_type_is_complete (PKL_AST_STRUCT_TYPE_FIELD_TYPE (elem))
                        == PKL_AST_TYPE_COMPLETE_NO)))
              {
                complete = PKL_AST_TYPE_COMPLETE_NO;
                break;
              }
          }
        break;
      }
      /* Array types are complete if the number of elements in the
         array are specified and it is a literal expression.  */
    case PKL_TYPE_ARRAY:
      {
        pkl_ast_node bound = PKL_AST_TYPE_A_BOUND (type);

        if (bound)
          {
            pkl_ast_node bound_type = PKL_AST_TYPE (bound);

            /* The type of the bounding expression should have been
               calculated at this point.  */
            assert (bound_type);

            if (PKL_AST_TYPE_CODE (bound_type) == PKL_TYPE_INTEGRAL
                && PKL_AST_LITERAL_P (bound))
              complete = PKL_AST_TYPE_COMPLETE_YES;
            else
              complete = PKL_AST_TYPE_COMPLETE_NO;
          }
        else
          complete = PKL_AST_TYPE_COMPLETE_NO;
      }
    default:
      break;
    }

  assert (complete != PKL_AST_TYPE_COMPLETE_UNKNOWN);
  return complete;
}

/* Print a textual description of TYPE to the file OUT.  If TYPE is a
   named type then its given name is preferred if USE_GIVEN_NAME is
   1.  */

void
pkl_print_type (FILE *out, pkl_ast_node type, int use_given_name)
{
  assert (PKL_AST_CODE (type) == PKL_AST_TYPE);

  /* Use the type's given name, if requested and this specific type
     instance is named.  */
  if (use_given_name
      && PKL_AST_TYPE_NAME (type))
    {
      fprintf (out, "%s",
               PKL_AST_IDENTIFIER_POINTER (PKL_AST_TYPE_NAME (type)));
      return;
    }

  /* Otherwise, print a description of the type, as terse as possible
     but complete.  The descriptions should follow the same
     style/syntax/conventions used in both the language specification
     and the PVM.  */
  switch (PKL_AST_TYPE_CODE (type))
    {
    case PKL_TYPE_ANY:
      fprintf (out, "any");
      break;
    case PKL_TYPE_INTEGRAL:
      if (!PKL_AST_TYPE_I_SIGNED_P (type))
        fputc ('u', out);
      fprintf (out, "int<%zd>", PKL_AST_TYPE_I_SIZE (type));
      break;
    case PKL_TYPE_VOID:
      fprintf (out, "void");
      break;
    case PKL_TYPE_STRING:
      fprintf (out, "string");
      break;
    case PKL_TYPE_ARRAY:
      {
        pkl_ast_node bound = PKL_AST_TYPE_A_BOUND (type);

        pkl_print_type (out, PKL_AST_TYPE_A_ETYPE (type),
                        use_given_name);
        fputc ('[', out);
        if (bound != NULL)
          {
            pkl_ast_node bound_type = PKL_AST_TYPE (bound);

            if (bound_type
                && PKL_AST_TYPE_CODE (bound_type) == PKL_TYPE_INTEGRAL
                && PKL_AST_CODE (bound) == PKL_AST_INTEGER)
              {
                fprintf (out, "%" PRIu64, PKL_AST_INTEGER_VALUE (bound));
              }
          }
        fputc (']', out);
        break;
      }
    case PKL_TYPE_STRUCT:
      {
        pkl_ast_node t;

        fputs ("struct {", out);

        for (t = PKL_AST_TYPE_S_ELEMS (type); t;
             t = PKL_AST_CHAIN (t))
          {
            if (PKL_AST_CODE (t) == PKL_AST_STRUCT_TYPE_FIELD)
              {
                pkl_ast_node ename = PKL_AST_STRUCT_TYPE_FIELD_NAME (t);
                pkl_ast_node etype = PKL_AST_STRUCT_TYPE_FIELD_TYPE (t);

                pkl_print_type (out, etype, use_given_name);
                if (ename)
                  fprintf (out, " %s",
                           PKL_AST_IDENTIFIER_POINTER (ename));
                fputc (';', out);
              }
          }
        fputs ("}", out);
        break;
      }
    case PKL_TYPE_FUNCTION:
      {
        pkl_ast_node t;

        if (PKL_AST_TYPE_F_NARG (type) > 0)
          {
            fputc ('(', out);

            for (t = PKL_AST_TYPE_F_ARGS (type); t;
                 t = PKL_AST_CHAIN (t))
              {
                pkl_ast_node atype
                  = PKL_AST_FUNC_TYPE_ARG_TYPE (t);

                if (PKL_AST_FUNC_TYPE_ARG_VARARG (t))
                  fputs ("...", out);
                else
                  {
                    if (t != PKL_AST_TYPE_F_ARGS (type))
                      fputc (',', out);
                    pkl_print_type (out, atype, use_given_name);
                    if (PKL_AST_FUNC_TYPE_ARG_OPTIONAL (t))
                      fputc ('?', out);
                  }
              }

            fputc (')', out);
          }

        pkl_print_type (out,
                        PKL_AST_TYPE_F_RTYPE (type),
                        use_given_name);
        fputc (':', out);
        break;
      }
    case PKL_TYPE_OFFSET:
      {
        pkl_ast_node unit = PKL_AST_TYPE_O_UNIT (type);

        fputs ("offset<", out);
        pkl_print_type (out, PKL_AST_TYPE_O_BASE_TYPE (type),
                        use_given_name);
        fputc (',', out);

        if (PKL_AST_CODE (unit) == PKL_AST_TYPE)
          pkl_print_type (out, unit, use_given_name);
        else if (PKL_AST_CODE (unit) == PKL_AST_IDENTIFIER)
          fputs (PKL_AST_IDENTIFIER_POINTER (unit), out);
        else if (PKL_AST_CODE (unit) == PKL_AST_INTEGER)
          fprintf (out, "%" PRIu64, PKL_AST_INTEGER_VALUE (unit));
        else
          assert (0);

        fputc ('>', out);
        break;
      }
    case PKL_TYPE_NOTYPE:
    default:
      assert (0);
      break;
    }
}

/* Like pkl_print_type, but return the string describing the type in a
   string.  It is up to the caller to free the string memory.  */

char *
pkl_type_str (pkl_ast_node type, int use_given_name)
{
  char *str;
  size_t str_size;
  FILE *buffer = open_memstream (&str, &str_size);

  pkl_print_type (buffer, type, use_given_name);
  fclose (buffer);
  return str;
}

/* Return a boolean telling whether the given type function only have
   optional arguments, i.e. all arguments have an initializer.  */

int
pkl_ast_func_all_optargs (pkl_ast_node type)
{
  pkl_ast_node arg;
  int all_optargs = 1;

  for (arg = PKL_AST_TYPE_F_ARGS (type); arg; arg = PKL_AST_CHAIN (arg))
    {
      if (!PKL_AST_FUNC_TYPE_ARG_OPTIONAL (arg))
        {
          all_optargs = 0;
          break;
        }
    }

  return all_optargs;
}

/* Build and return an AST node for an enum.  */

pkl_ast_node
pkl_ast_make_enum (pkl_ast ast,
                   pkl_ast_node tag, pkl_ast_node values)
{
  pkl_ast_node enumeration
    = pkl_ast_make_node (ast, PKL_AST_ENUM);

  assert (tag && values);

  PKL_AST_ENUM_TAG (enumeration) = ASTREF (tag);
  PKL_AST_ENUM_VALUES (enumeration) = ASTREF (values);

  return enumeration;
}

/* Build and return an AST node for an array.  */

pkl_ast_node
pkl_ast_make_array (pkl_ast ast,
                    size_t nelem, size_t ninitializer,
                    pkl_ast_node initializers)
{
  pkl_ast_node array
    = pkl_ast_make_node (ast, PKL_AST_ARRAY);

  PKL_AST_ARRAY_NELEM (array) = nelem;
  PKL_AST_ARRAY_NINITIALIZER (array) = ninitializer;
  PKL_AST_ARRAY_INITIALIZERS (array) = ASTREF (initializers);

  return array;
}

/* Build and return an AST node for an array element.  */

pkl_ast_node
pkl_ast_make_array_initializer (pkl_ast ast,
                                pkl_ast_node index, pkl_ast_node exp)
{
  pkl_ast_node initializer
    = pkl_ast_make_node (ast, PKL_AST_ARRAY_INITIALIZER);

  PKL_AST_ARRAY_INITIALIZER_INDEX (initializer) = ASTREF (index);
  PKL_AST_ARRAY_INITIALIZER_EXP (initializer) = ASTREF (exp);

  return initializer;
}

/* Build and return an AST node for a struct.  */

pkl_ast_node
pkl_ast_make_struct (pkl_ast ast,
                     size_t nelem,
                     pkl_ast_node elems)
{
  pkl_ast_node sct = pkl_ast_make_node (ast, PKL_AST_STRUCT);

  PKL_AST_STRUCT_NELEM (sct) = nelem;
  PKL_AST_STRUCT_FIELDS (sct) = ASTREF (elems);

  return sct;
}

/* Build and return an AST node for a struct field.  */

pkl_ast_node
pkl_ast_make_struct_field (pkl_ast ast,
                           pkl_ast_node name,
                           pkl_ast_node exp)
{
  pkl_ast_node elem = pkl_ast_make_node (ast, PKL_AST_STRUCT_FIELD);

  if (name != NULL)
    PKL_AST_STRUCT_FIELD_NAME (elem) = ASTREF (name);
  PKL_AST_STRUCT_FIELD_EXP (elem) = ASTREF (exp);

  return elem;
}

/* Build and return an AST node for a declaration.  */

pkl_ast_node
pkl_ast_make_decl (pkl_ast ast, int kind, pkl_ast_node name,
                   pkl_ast_node initial, const char *source)
{
  pkl_ast_node decl = pkl_ast_make_node (ast, PKL_AST_DECL);

  assert (name);

  PKL_AST_DECL_KIND (decl) = kind;
  PKL_AST_DECL_NAME (decl) = ASTREF (name);
  PKL_AST_DECL_INITIAL (decl) = ASTREF (initial);
  if (source)
    PKL_AST_DECL_SOURCE (decl) = xstrdup (source);

  return decl;
}

/* Build and return an AST node for an offset construct.  */

pkl_ast_node
pkl_ast_make_offset (pkl_ast ast,
                     pkl_ast_node magnitude, pkl_ast_node unit)
{
  pkl_ast_node offset = pkl_ast_make_node (ast, PKL_AST_OFFSET);

  assert (unit);

  if (magnitude != NULL)
    PKL_AST_OFFSET_MAGNITUDE (offset) = ASTREF (magnitude);
  PKL_AST_OFFSET_UNIT (offset) = ASTREF (unit);

  return offset;
}

/* Build and return an AST node for a cast.  */

pkl_ast_node
pkl_ast_make_cast (pkl_ast ast,
                   pkl_ast_node type, pkl_ast_node exp)
{
  pkl_ast_node cast = pkl_ast_make_node (ast, PKL_AST_CAST);

  assert (type && exp);

  PKL_AST_CAST_TYPE (cast) = ASTREF (type);
  PKL_AST_CAST_EXP (cast) = ASTREF (exp);

  return cast;
}

/* Build and return an AST node for an `isa' operation.  */

pkl_ast_node
pkl_ast_make_isa (pkl_ast ast,
                  pkl_ast_node type, pkl_ast_node exp)
{
  pkl_ast_node isa = pkl_ast_make_node (ast, PKL_AST_ISA);

  assert (type && exp);

  PKL_AST_ISA_TYPE (isa) = ASTREF (type);
  PKL_AST_ISA_EXP (isa) = ASTREF (exp);

  return isa;
}

/* Build and return an AST node for a map.  */

pkl_ast_node
pkl_ast_make_map (pkl_ast ast,
                  pkl_ast_node type,
                  pkl_ast_node ios,
                  pkl_ast_node offset)
{
  pkl_ast_node map = pkl_ast_make_node (ast, PKL_AST_MAP);

  assert (type && offset);

  PKL_AST_MAP_TYPE (map) = ASTREF (type);
  PKL_AST_MAP_IOS (map) = ASTREF (ios);
  PKL_AST_MAP_OFFSET (map) = ASTREF (offset);

  return map;
}

/* Build and return an AST node for a struct constructor.  */

pkl_ast_node
pkl_ast_make_scons (pkl_ast ast,
                    pkl_ast_node type, pkl_ast_node value)
{
  pkl_ast_node scons = pkl_ast_make_node (ast, PKL_AST_SCONS);

  assert (type && value);

  PKL_AST_SCONS_TYPE (scons) = ASTREF (type);
  PKL_AST_SCONS_VALUE (scons) = ASTREF (value);
  return scons;
}

/* Build and return an AST node for a function call.  */

pkl_ast_node
pkl_ast_make_funcall (pkl_ast ast,
                      pkl_ast_node function, pkl_ast_node args)
{
  pkl_ast_node funcall = pkl_ast_make_node (ast,
                                            PKL_AST_FUNCALL);

  assert (function);

  PKL_AST_FUNCALL_FUNCTION (funcall) = ASTREF (function);
  if (args)
    PKL_AST_FUNCALL_ARGS (funcall) = ASTREF (args);
  return funcall;
}

/* Build and return an AST node for an actual argument of a function
   call.  */

pkl_ast_node
pkl_ast_make_funcall_arg (pkl_ast ast, pkl_ast_node exp,
                          pkl_ast_node name)
{
  pkl_ast_node funcall_arg = pkl_ast_make_node (ast,
                                                PKL_AST_FUNCALL_ARG);

  if (exp)
    PKL_AST_FUNCALL_ARG_EXP (funcall_arg) = ASTREF (exp);
  if (name)
    PKL_AST_FUNCALL_ARG_NAME (funcall_arg) = ASTREF (name);
  PKL_AST_FUNCALL_ARG_FIRST_VARARG (funcall_arg) = 0;
  return funcall_arg;
}

/* Build and return an AST node for a variable reference.  */

pkl_ast_node
pkl_ast_make_var (pkl_ast ast, pkl_ast_node name,
                  pkl_ast_node decl, int back, int over)
{
  pkl_ast_node var = pkl_ast_make_node (ast, PKL_AST_VAR);

  assert (name && decl);

  PKL_AST_VAR_NAME (var) = ASTREF (name);
  PKL_AST_VAR_DECL (var) = ASTREF (decl);
  PKL_AST_VAR_BACK (var) = back;
  PKL_AST_VAR_OVER (var) = over;

  return var;
}

/* Build and return an AST node for a compound statement.  */

pkl_ast_node
pkl_ast_make_comp_stmt (pkl_ast ast, pkl_ast_node stmts)
{
  pkl_ast_node comp_stmt = pkl_ast_make_node (ast,
                                              PKL_AST_COMP_STMT);

  if (stmts)
    PKL_AST_COMP_STMT_STMTS (comp_stmt) = ASTREF (stmts);
  PKL_AST_COMP_STMT_BUILTIN (comp_stmt) = PKL_AST_BUILTIN_NONE;
  return comp_stmt;
}

/* Build and return an AST node for a compiler builtin.  */

pkl_ast_node
pkl_ast_make_builtin (pkl_ast ast, int builtin)
{
  pkl_ast_node comp_stmt = pkl_ast_make_node (ast,
                                              PKL_AST_COMP_STMT);

  PKL_AST_COMP_STMT_BUILTIN (comp_stmt) = builtin;
  return comp_stmt;
}

/* Build and return an AST node for an assignment statement.  */

pkl_ast_node
pkl_ast_make_ass_stmt (pkl_ast ast, pkl_ast_node lvalue,
                       pkl_ast_node exp)
{
  pkl_ast_node ass_stmt = pkl_ast_make_node (ast,
                                             PKL_AST_ASS_STMT);

  assert (lvalue && exp);

  PKL_AST_ASS_STMT_LVALUE (ass_stmt) = ASTREF (lvalue);
  PKL_AST_ASS_STMT_EXP (ass_stmt) = ASTREF (exp);

  return ass_stmt;
}

/* Build and return an AST node for a conditional statement.  */

pkl_ast_node
pkl_ast_make_if_stmt (pkl_ast ast, pkl_ast_node exp,
                      pkl_ast_node then_stmt, pkl_ast_node else_stmt)
{
  pkl_ast_node if_stmt = pkl_ast_make_node (ast, PKL_AST_IF_STMT);

  assert (exp && then_stmt);

  PKL_AST_IF_STMT_EXP (if_stmt) = ASTREF (exp);
  PKL_AST_IF_STMT_THEN_STMT (if_stmt) = ASTREF (then_stmt);
  if (else_stmt)
    PKL_AST_IF_STMT_ELSE_STMT (if_stmt) = ASTREF (else_stmt);

  return if_stmt;
}

/* Build and return an AST node for a loop statement.  */

pkl_ast_node
pkl_ast_make_loop_stmt (pkl_ast ast, pkl_ast_node iterator,
                        pkl_ast_node condition, pkl_ast_node body)
{
  pkl_ast_node loop_stmt
    = pkl_ast_make_node (ast, PKL_AST_LOOP_STMT);

  assert (body);

  if (iterator)
    PKL_AST_LOOP_STMT_ITERATOR (loop_stmt)
      = ASTREF (iterator);
  if (condition)
    PKL_AST_LOOP_STMT_CONDITION (loop_stmt)
      = ASTREF (condition);
  PKL_AST_LOOP_STMT_BODY (loop_stmt) = ASTREF (body);

  return loop_stmt;
}

/* Build and return an AST node for the iterator of a loop
   statement.  */

pkl_ast_node
pkl_ast_make_loop_stmt_iterator (pkl_ast ast, pkl_ast_node decl,
                                 pkl_ast_node container)
{
  pkl_ast_node loop_stmt_iterator
    = pkl_ast_make_node (ast, PKL_AST_LOOP_STMT_ITERATOR);

  assert (decl && container);

  PKL_AST_LOOP_STMT_ITERATOR_DECL (loop_stmt_iterator)
    = ASTREF (decl);
  PKL_AST_LOOP_STMT_ITERATOR_CONTAINER (loop_stmt_iterator)
    = ASTREF (container);

  return loop_stmt_iterator;
}

/* Build and return an AST node for a return statement.  */

pkl_ast_node
pkl_ast_make_return_stmt (pkl_ast ast, pkl_ast_node exp)
{
  pkl_ast_node return_stmt = pkl_ast_make_node (ast,
                                                PKL_AST_RETURN_STMT);

  PKL_AST_RETURN_STMT_EXP (return_stmt) = ASTREF (exp);
  return return_stmt;
}

/* Build and return an AST node for a "null statement".  */

pkl_ast_node
pkl_ast_make_null_stmt (pkl_ast ast)
{
  pkl_ast_node null_stmt = pkl_ast_make_node (ast,
                                              PKL_AST_NULL_STMT);
  return null_stmt;
}

/* Build and return an AST node for an "expression statement".  */

pkl_ast_node
pkl_ast_make_exp_stmt (pkl_ast ast, pkl_ast_node exp)
{
  pkl_ast_node exp_stmt = pkl_ast_make_node (ast,
                                             PKL_AST_EXP_STMT);

  assert (exp);

  PKL_AST_EXP_STMT_EXP (exp_stmt) = ASTREF (exp);
  return exp_stmt;
}

/* Build and return an AST node for a try-until statement.  */

pkl_ast_node
pkl_ast_make_try_until_stmt (pkl_ast ast, pkl_ast_node code,
                             pkl_ast_node exp)
{
  pkl_ast_node try_until_stmt = pkl_ast_make_node (ast,
                                                   PKL_AST_TRY_UNTIL_STMT);

  assert (code && exp);

  PKL_AST_TRY_UNTIL_STMT_CODE (try_until_stmt) = ASTREF (code);
  PKL_AST_TRY_UNTIL_STMT_EXP (try_until_stmt) = ASTREF (exp);

  return try_until_stmt;
}

/* Build and return an AST node for a try-catch statement.  */

pkl_ast_node
pkl_ast_make_try_catch_stmt (pkl_ast ast, pkl_ast_node code,
                             pkl_ast_node handler, pkl_ast_node arg,
                             pkl_ast_node exp)
{
  pkl_ast_node try_catch_stmt = pkl_ast_make_node (ast,
                                                   PKL_AST_TRY_CATCH_STMT);

  assert (code && handler);
  assert (!arg || !exp);

  PKL_AST_TRY_CATCH_STMT_CODE (try_catch_stmt) = ASTREF (code);
  PKL_AST_TRY_CATCH_STMT_HANDLER (try_catch_stmt) = ASTREF (handler);
  if (arg)
    PKL_AST_TRY_CATCH_STMT_ARG (try_catch_stmt) = ASTREF (arg);
  if (exp)
    PKL_AST_TRY_CATCH_STMT_EXP (try_catch_stmt) = ASTREF (exp);

  return try_catch_stmt;
}

/* Build and return an AST node for a `print' statement.  */

pkl_ast_node
pkl_ast_make_print_stmt (pkl_ast ast,
                         pkl_ast_node fmt, pkl_ast_node args)
{
  pkl_ast_node print_stmt = pkl_ast_make_node (ast,
                                               PKL_AST_PRINT_STMT);

  if (fmt)
    PKL_AST_PRINT_STMT_FMT (print_stmt) = ASTREF (fmt);
  if (args)
    PKL_AST_PRINT_STMT_ARGS (print_stmt) = ASTREF (args);

  return print_stmt;
}

/* Build and return an AST node for a `printf' argument.  */

pkl_ast_node
pkl_ast_make_print_stmt_arg (pkl_ast ast, pkl_ast_node exp)
{
  pkl_ast_node print_stmt_arg = pkl_ast_make_node (ast,
                                                   PKL_AST_PRINT_STMT_ARG);

  if (exp)
    PKL_AST_PRINT_STMT_ARG_EXP (print_stmt_arg) = ASTREF (exp);
  return print_stmt_arg;
}

/* Build and return an AST node for a `break' statement.  */

pkl_ast_node
pkl_ast_make_break_stmt (pkl_ast ast)
{
  pkl_ast_node break_stmt = pkl_ast_make_node (ast,
                                               PKL_AST_BREAK_STMT);
  return break_stmt;
}

/* Build and return an AST node for a `raise' statement.  */

pkl_ast_node
pkl_ast_make_raise_stmt (pkl_ast ast, pkl_ast_node exp)
{
  pkl_ast_node raise_stmt = pkl_ast_make_node (ast,
                                               PKL_AST_RAISE_STMT);

  if (exp)
    PKL_AST_RAISE_STMT_EXP (raise_stmt) = ASTREF (exp);
  return raise_stmt;
}

/* Build and return an AST node for a PKL program.  */

pkl_ast_node
pkl_ast_make_program (pkl_ast ast, pkl_ast_node elems)
{
  pkl_ast_node program
    = pkl_ast_make_node (ast, PKL_AST_PROGRAM);

  PKL_AST_PROGRAM_ELEMS (program) = ASTREF (elems);
  return program;
}

/* Free the AST nodes linked by PKL_AST_CHAIN.  */
void
pkl_ast_node_free_chain (pkl_ast_node ast)
{
  pkl_ast_node n, next;

  for (n = ast; n; n = next)
    {
      next = PKL_AST_CHAIN (n);
      pkl_ast_node_free (n);
    }
}

/* Free all allocated resources used by AST.  Note that nodes marked
   as "registered", as well as their children, are not disposed.  */

void
pkl_ast_node_free (pkl_ast_node ast)
{
  pkl_ast_node t, n;
  size_t i;

  if (ast == NULL)
    return;

  assert (PKL_AST_REFCOUNT (ast) > 0);

  if (PKL_AST_REFCOUNT (ast) > 1)
    {
      PKL_AST_REFCOUNT (ast) -= 1;
      return;
    }

  switch (PKL_AST_CODE (ast))
    {
    case PKL_AST_PROGRAM:

      for (t = PKL_AST_PROGRAM_ELEMS (ast); t; t = n)
        {
          n = PKL_AST_CHAIN (t);
          pkl_ast_node_free (t);
        }

      break;

    case PKL_AST_EXP:

      for (i = 0; i < PKL_AST_EXP_NUMOPS (ast); i++)
        pkl_ast_node_free (PKL_AST_EXP_OPERAND (ast, i));

      break;

    case PKL_AST_COND_EXP:

      pkl_ast_node_free (PKL_AST_COND_EXP_COND (ast));
      pkl_ast_node_free (PKL_AST_COND_EXP_THENEXP (ast));
      pkl_ast_node_free (PKL_AST_COND_EXP_ELSEEXP (ast));
      break;

    case PKL_AST_ENUM:

      pkl_ast_node_free (PKL_AST_ENUM_TAG (ast));

      for (t = PKL_AST_ENUM_VALUES (ast); t; t = n)
        {
          n = PKL_AST_CHAIN (t);
          pkl_ast_node_free (t);
        }

      break;

    case PKL_AST_ENUMERATOR:

      pkl_ast_node_free (PKL_AST_ENUMERATOR_IDENTIFIER (ast));
      pkl_ast_node_free (PKL_AST_ENUMERATOR_VALUE (ast));
      break;

    case PKL_AST_TYPE:

      free (PKL_AST_TYPE_NAME (ast));
      switch (PKL_AST_TYPE_CODE (ast))
        {
        case PKL_TYPE_ARRAY:
          /* Remove GC roots.  */
          pvm_alloc_remove_gc_roots (&PKL_AST_TYPE_A_MAPPER (ast), 1);
          pvm_alloc_remove_gc_roots (&PKL_AST_TYPE_A_WRITER (ast), 1);
          pvm_alloc_remove_gc_roots (&PKL_AST_TYPE_A_BOUNDER (ast), 1);
          pvm_alloc_remove_gc_roots (&PKL_AST_TYPE_A_CONSTRUCTOR (ast), 1);

          pkl_ast_node_free (PKL_AST_TYPE_A_BOUND (ast));
          pkl_ast_node_free (PKL_AST_TYPE_A_ETYPE (ast));
          break;
        case PKL_TYPE_STRUCT:
          /* Remove GC roots.  */
          pvm_alloc_remove_gc_roots (&PKL_AST_TYPE_S_WRITER (ast), 1);
          pvm_alloc_remove_gc_roots (&PKL_AST_TYPE_S_MAPPER (ast), 1);
          pvm_alloc_remove_gc_roots (&PKL_AST_TYPE_S_CONSTRUCTOR (ast), 1);
          pvm_alloc_remove_gc_roots (&PKL_AST_TYPE_S_COMPARATOR (ast), 1);
          pvm_alloc_remove_gc_roots (&PKL_AST_TYPE_S_INTEGRATOR (ast), 1);

          for (t = PKL_AST_TYPE_S_ELEMS (ast); t; t = n)
            {
              n = PKL_AST_CHAIN (t);
              pkl_ast_node_free (t);
            }
          break;
        case PKL_TYPE_FUNCTION:
          pkl_ast_node_free (PKL_AST_TYPE_F_RTYPE (ast));
          pkl_ast_node_free (PKL_AST_TYPE_F_FIRST_OPT_ARG (ast));
          for (t = PKL_AST_TYPE_F_ARGS (ast); t; t = n)
            {
              n = PKL_AST_CHAIN (t);
              pkl_ast_node_free (t);
            }
          break;
        case PKL_TYPE_OFFSET:
          pkl_ast_node_free (PKL_AST_TYPE_O_UNIT (ast));
          pkl_ast_node_free (PKL_AST_TYPE_O_BASE_TYPE (ast));
          break;
        case PKL_TYPE_INTEGRAL:
        case PKL_TYPE_STRING:
        default:
          break;
        }

      break;

    case PKL_AST_STRUCT_TYPE_FIELD:

      pkl_ast_node_free (PKL_AST_STRUCT_TYPE_FIELD_NAME (ast));
      pkl_ast_node_free (PKL_AST_STRUCT_TYPE_FIELD_TYPE (ast));
      pkl_ast_node_free (PKL_AST_STRUCT_TYPE_FIELD_CONSTRAINT (ast));
      pkl_ast_node_free (PKL_AST_STRUCT_TYPE_FIELD_INITIALIZER (ast));
      pkl_ast_node_free (PKL_AST_STRUCT_TYPE_FIELD_LABEL (ast));
      pkl_ast_node_free (PKL_AST_STRUCT_TYPE_FIELD_OPTCOND (ast));
      break;

    case PKL_AST_FUNC_TYPE_ARG:

      pkl_ast_node_free (PKL_AST_FUNC_TYPE_ARG_TYPE (ast));
      pkl_ast_node_free (PKL_AST_FUNC_TYPE_ARG_NAME (ast));
      break;

    case PKL_AST_INDEXER:

      pkl_ast_node_free (PKL_AST_INDEXER_ENTITY (ast));
      pkl_ast_node_free (PKL_AST_INDEXER_INDEX (ast));
      break;

    case PKL_AST_TRIMMER:

      pkl_ast_node_free (PKL_AST_TRIMMER_ENTITY (ast));
      pkl_ast_node_free (PKL_AST_TRIMMER_FROM (ast));
      pkl_ast_node_free (PKL_AST_TRIMMER_TO (ast));
      break;

    case PKL_AST_FUNC:

      free (PKL_AST_FUNC_NAME (ast));
      pkl_ast_node_free (PKL_AST_FUNC_RET_TYPE (ast));
      pkl_ast_node_free (PKL_AST_FUNC_BODY (ast));
      pkl_ast_node_free (PKL_AST_FUNC_FIRST_OPT_ARG (ast));
      for (t = PKL_AST_FUNC_ARGS (ast); t; t = n)
            {
              n = PKL_AST_CHAIN (t);
              pkl_ast_node_free (t);
            }
      break;

    case PKL_AST_FUNC_ARG:

      pkl_ast_node_free (PKL_AST_FUNC_ARG_TYPE (ast));
      pkl_ast_node_free (PKL_AST_FUNC_ARG_IDENTIFIER (ast));
      pkl_ast_node_free (PKL_AST_FUNC_ARG_INITIAL (ast));
      break;

    case PKL_AST_STRING:

      free (PKL_AST_STRING_POINTER (ast));
      break;

    case PKL_AST_IDENTIFIER:

      free (PKL_AST_IDENTIFIER_POINTER (ast));
      break;

    case PKL_AST_STRUCT_REF:

      pkl_ast_node_free (PKL_AST_STRUCT_REF_STRUCT (ast));
      pkl_ast_node_free (PKL_AST_STRUCT_REF_IDENTIFIER (ast));
      break;

    case PKL_AST_STRUCT_FIELD:

      pkl_ast_node_free (PKL_AST_STRUCT_FIELD_NAME (ast));
      pkl_ast_node_free (PKL_AST_STRUCT_FIELD_EXP (ast));
      break;

    case PKL_AST_STRUCT:

      for (t = PKL_AST_STRUCT_FIELDS (ast); t; t = n)
        {
          n = PKL_AST_CHAIN (t);
          pkl_ast_node_free (t);
        }
      break;

    case PKL_AST_ARRAY_INITIALIZER:

      pkl_ast_node_free (PKL_AST_ARRAY_INITIALIZER_INDEX (ast));
      pkl_ast_node_free (PKL_AST_ARRAY_INITIALIZER_EXP (ast));
      break;

    case PKL_AST_ARRAY:

      for (t = PKL_AST_ARRAY_INITIALIZERS (ast); t; t = n)
        {
          n = PKL_AST_CHAIN (t);
          pkl_ast_node_free (t);
        }

      break;

    case PKL_AST_DECL:

      free (PKL_AST_DECL_SOURCE (ast));
      pkl_ast_node_free (PKL_AST_DECL_NAME (ast));
      pkl_ast_node_free (PKL_AST_DECL_INITIAL (ast));
      break;

    case PKL_AST_OFFSET:

      pkl_ast_node_free (PKL_AST_OFFSET_MAGNITUDE (ast));
      pkl_ast_node_free (PKL_AST_OFFSET_UNIT (ast));
      break;

    case PKL_AST_CAST:

      pkl_ast_node_free (PKL_AST_CAST_TYPE (ast));
      pkl_ast_node_free (PKL_AST_CAST_EXP (ast));
      break;

    case PKL_AST_ISA:

      pkl_ast_node_free (PKL_AST_ISA_TYPE (ast));
      pkl_ast_node_free (PKL_AST_ISA_EXP (ast));
      break;

    case PKL_AST_MAP:

      pkl_ast_node_free (PKL_AST_MAP_TYPE (ast));
      pkl_ast_node_free (PKL_AST_MAP_IOS (ast));
      pkl_ast_node_free (PKL_AST_MAP_OFFSET (ast));
      break;

    case PKL_AST_SCONS:

      pkl_ast_node_free (PKL_AST_SCONS_TYPE (ast));
      pkl_ast_node_free (PKL_AST_SCONS_VALUE (ast));
      break;

    case PKL_AST_FUNCALL:

      pkl_ast_node_free (PKL_AST_FUNCALL_FUNCTION (ast));
      for (t = PKL_AST_FUNCALL_ARGS (ast); t; t = n)
        {
          n = PKL_AST_CHAIN (t);
          pkl_ast_node_free (t);
        }

      break;

    case PKL_AST_FUNCALL_ARG:

      pkl_ast_node_free (PKL_AST_FUNCALL_ARG_EXP (ast));
      pkl_ast_node_free (PKL_AST_FUNCALL_ARG_NAME (ast));
      break;

    case PKL_AST_VAR:

      pkl_ast_node_free (PKL_AST_VAR_NAME (ast));
      if (!PKL_AST_VAR_IS_RECURSIVE (ast))
        pkl_ast_node_free (PKL_AST_VAR_DECL (ast));
      break;

    case PKL_AST_COMP_STMT:

      for (t = PKL_AST_COMP_STMT_STMTS (ast); t; t = n)
        {
          n = PKL_AST_CHAIN (t);
          pkl_ast_node_free (t);
        }

      break;

    case PKL_AST_ASS_STMT:

      pkl_ast_node_free (PKL_AST_ASS_STMT_LVALUE (ast));
      pkl_ast_node_free (PKL_AST_ASS_STMT_EXP (ast));
      break;

    case PKL_AST_IF_STMT:

      pkl_ast_node_free (PKL_AST_IF_STMT_EXP (ast));
      pkl_ast_node_free (PKL_AST_IF_STMT_THEN_STMT (ast));
      pkl_ast_node_free (PKL_AST_IF_STMT_ELSE_STMT (ast));
      break;

    case PKL_AST_LOOP_STMT:

      pkl_ast_node_free (PKL_AST_LOOP_STMT_ITERATOR (ast));
      pkl_ast_node_free (PKL_AST_LOOP_STMT_CONDITION (ast));
      pkl_ast_node_free (PKL_AST_LOOP_STMT_BODY (ast));

      break;

    case PKL_AST_LOOP_STMT_ITERATOR:

      pkl_ast_node_free (PKL_AST_LOOP_STMT_ITERATOR_DECL (ast));
      pkl_ast_node_free (PKL_AST_LOOP_STMT_ITERATOR_CONTAINER (ast));

      break;

    case PKL_AST_RETURN_STMT:

      pkl_ast_node_free (PKL_AST_RETURN_STMT_EXP (ast));
      break;

    case PKL_AST_EXP_STMT:

      pkl_ast_node_free (PKL_AST_EXP_STMT_EXP (ast));
      break;

    case PKL_AST_TRY_CATCH_STMT:

      pkl_ast_node_free (PKL_AST_TRY_CATCH_STMT_CODE (ast));
      pkl_ast_node_free (PKL_AST_TRY_CATCH_STMT_HANDLER (ast));
      pkl_ast_node_free (PKL_AST_TRY_CATCH_STMT_ARG (ast));
      pkl_ast_node_free (PKL_AST_TRY_CATCH_STMT_EXP (ast));
      break;

    case PKL_AST_TRY_UNTIL_STMT:

      pkl_ast_node_free (PKL_AST_TRY_UNTIL_STMT_CODE (ast));
      pkl_ast_node_free (PKL_AST_TRY_UNTIL_STMT_EXP (ast));
      break;

    case PKL_AST_PRINT_STMT_ARG:
      free (PKL_AST_PRINT_STMT_ARG_SUFFIX (ast));
      free (PKL_AST_PRINT_STMT_ARG_BEGIN_SC (ast));
      free (PKL_AST_PRINT_STMT_ARG_END_SC (ast));
      pkl_ast_node_free (PKL_AST_PRINT_STMT_ARG_EXP (ast));
      break;

    case PKL_AST_PRINT_STMT:
      {
        free (PKL_AST_PRINT_STMT_PREFIX (ast));
        for (t = PKL_AST_PRINT_STMT_ARGS (ast); t; t = n)
          {
            n = PKL_AST_CHAIN (t);
            pkl_ast_node_free (t);
          }
        for (t = PKL_AST_PRINT_STMT_TYPES (ast); t; t = n)
          {
            n = PKL_AST_CHAIN (t);
            pkl_ast_node_free (t);
          }
        pkl_ast_node_free (PKL_AST_PRINT_STMT_FMT (ast));
        break;
      }

    case PKL_AST_BREAK_STMT:
      break;

    case PKL_AST_RAISE_STMT:

      pkl_ast_node_free (PKL_AST_RAISE_STMT_EXP (ast));
      break;

    case PKL_AST_NULL_STMT:
      break;

    case PKL_AST_INTEGER:
      break;

    default:
      assert (0);
    }

  pkl_ast_node_free (PKL_AST_TYPE (ast));
  free (ast);
}

/* Allocate and initialize a new AST and return it.  */

pkl_ast
pkl_ast_init (void)
{
  struct pkl_ast *ast;

  /* Allocate a new AST and initialize it to 0.  */
  ast = xzalloc (sizeof (struct pkl_ast));

  return ast;
}

/* Free all the memory allocated to store the nodes of an AST.  */

void
pkl_ast_free (pkl_ast ast)
{
  if (ast == NULL)
    return;

  pkl_ast_node_free (ast->ast);
  free (ast->buffer);
  free (ast->filename);
  free (ast);
}

pkl_ast_node
pkl_ast_reverse (pkl_ast_node ast)
{
  pkl_ast_node prev = NULL, decl, next;

  ASTDEREF (ast);
  for (decl = ast; decl != NULL; decl = next)
    {
      next = PKL_AST_CHAIN (decl);
      ASTDEREF (next);
      PKL_AST_CHAIN (decl) = ASTREF (prev);
      prev = decl;
    }

  return prev;
}

/* Annotate the break statements within a given entity (loop or switch
   or...) with a pointer to the entity and their lexical nest level
   within the entity.  */

static void
pkl_ast_finish_breaks_1 (pkl_ast_node entity, pkl_ast_node stmt,
                         int *nframes)
{
  /* STMT can be a statement or a declaration.  */

  switch (PKL_AST_CODE (stmt))
    {
    case PKL_AST_BREAK_STMT:
      PKL_AST_BREAK_STMT_ENTITY (stmt) = entity; /* Note no ASTREF */
      PKL_AST_BREAK_STMT_NFRAMES (stmt) = *nframes;
      break;
    case PKL_AST_COMP_STMT:
      {
        pkl_ast_node t;

        *nframes += 1;
        for (t = PKL_AST_COMP_STMT_STMTS (stmt); t;
             t = PKL_AST_CHAIN (t))
          pkl_ast_finish_breaks_1 (entity, t, nframes);

        /* Pop the frame of the compound itself.  */
        *nframes -= 1;
        break;
      }
    case PKL_AST_IF_STMT:
      pkl_ast_finish_breaks_1 (entity,
                               PKL_AST_IF_STMT_THEN_STMT (stmt),
                               nframes);
      if (PKL_AST_IF_STMT_ELSE_STMT (stmt))
        pkl_ast_finish_breaks_1 (entity,
                                 PKL_AST_IF_STMT_ELSE_STMT (stmt),
                                 nframes);
      break;
    case PKL_AST_TRY_CATCH_STMT:
      pkl_ast_finish_breaks_1 (entity,
                               PKL_AST_TRY_CATCH_STMT_CODE (stmt),
                               nframes);
      pkl_ast_finish_breaks_1 (entity,
                               PKL_AST_TRY_CATCH_STMT_HANDLER (stmt),
                               nframes);
      break;
    case PKL_AST_TRY_UNTIL_STMT:
      pkl_ast_finish_breaks_1 (entity,
                               PKL_AST_TRY_CATCH_STMT_CODE (stmt),
                               nframes);
      break;
    case PKL_AST_DECL:
    case PKL_AST_RETURN_STMT:
    case PKL_AST_LOOP_STMT:
    case PKL_AST_LOOP_STMT_ITERATOR:
    case PKL_AST_EXP_STMT:
    case PKL_AST_ASS_STMT:
    case PKL_AST_PRINT_STMT:
    case PKL_AST_RAISE_STMT:
    /* XXX: Add switch statements here.  */
    case PKL_AST_NULL_STMT:
      break;
    default:
      printf ("XXX %d\n", PKL_AST_CODE (stmt));
      assert (0);
      break;
    }
}

void
pkl_ast_finish_breaks (pkl_ast_node entity, pkl_ast_node stmt)
{
  int nframes = 0;
  pkl_ast_finish_breaks_1 (entity, stmt, &nframes);
}

/* Annotate FUNCTIONs return statements with the function and their
   nest level within the function.

   XXX: move to handlers in trans1.  */

static void
pkl_ast_finish_returns_1 (pkl_ast_node function, pkl_ast_node stmt,
                          int *nframes, int *ndrops)
{
  /* STMT can be a statement or a declaration.  */

  switch (PKL_AST_CODE (stmt))
    {
    case PKL_AST_RETURN_STMT:
      PKL_AST_RETURN_STMT_FUNCTION (stmt) = function; /* Note no ASTREF.  */
      PKL_AST_RETURN_STMT_NFRAMES (stmt) = *nframes;
      PKL_AST_RETURN_STMT_NDROPS (stmt) = *ndrops;
      break;
    case PKL_AST_COMP_STMT:
      {
        pkl_ast_node t;

        *nframes += 1;
        for (t = PKL_AST_COMP_STMT_STMTS (stmt); t;
             t = PKL_AST_CHAIN (t))
          pkl_ast_finish_returns_1 (function, t, nframes, ndrops);

        /* Pop the frame of the compound itself.  */
        *nframes -= 1;
        break;
      }
    case PKL_AST_IF_STMT:
      pkl_ast_finish_returns_1 (function,
                                PKL_AST_IF_STMT_THEN_STMT (stmt),
                                nframes, ndrops);
      if (PKL_AST_IF_STMT_ELSE_STMT (stmt))
        pkl_ast_finish_returns_1 (function,
                                  PKL_AST_IF_STMT_ELSE_STMT (stmt),
                                  nframes, ndrops);
      break;
    case PKL_AST_LOOP_STMT:
      {
        if (PKL_AST_LOOP_STMT_ITERATOR (stmt))
          *ndrops += 3;
        pkl_ast_finish_returns_1 (function,
                                  PKL_AST_LOOP_STMT_BODY (stmt),
                                  nframes, ndrops);
        if (PKL_AST_LOOP_STMT_ITERATOR (stmt))
          *ndrops -= 3;
        break;
      }
    case PKL_AST_TRY_CATCH_STMT:
      pkl_ast_finish_returns_1 (function,
                                PKL_AST_TRY_CATCH_STMT_CODE (stmt),
                                nframes, ndrops);
      pkl_ast_finish_returns_1 (function,
                                PKL_AST_TRY_CATCH_STMT_HANDLER (stmt),
                                nframes, ndrops);
      break;
    case PKL_AST_TRY_UNTIL_STMT:
      pkl_ast_finish_returns_1 (function,
                                PKL_AST_TRY_UNTIL_STMT_CODE (stmt),
                                nframes, ndrops);
      break;
    case PKL_AST_DECL:
    case PKL_AST_EXP_STMT:
    case PKL_AST_ASS_STMT:
    case PKL_AST_PRINT_STMT:
    case PKL_AST_BREAK_STMT:
    case PKL_AST_RAISE_STMT:
    case PKL_AST_NULL_STMT:
      break;
    default:
      assert (0);
      break;
    }
}

void
pkl_ast_finish_returns (pkl_ast_node function)
{
  int nframes = 0;
  int ndrops = 0;
  pkl_ast_finish_returns_1 (function, PKL_AST_FUNC_BODY (function),
                            &nframes, &ndrops);
}

int
pkl_ast_lvalue_p (pkl_ast_node node)
{
  switch (PKL_AST_CODE (node))
    {
    case PKL_AST_VAR:
    case PKL_AST_MAP:
      /* Variable references and maps can always be used as
         l-values.  */
      return 1;
      break;
    case PKL_AST_STRUCT_REF:
      /* A field reference can be used as a l-value if the referred
         struct is itself a l-value.  */
      return pkl_ast_lvalue_p (PKL_AST_STRUCT_REF_STRUCT (node));
      break;
    case PKL_AST_INDEXER:
      /* An indexer can be used as a l-value if the referred entity is
         an array, and it is itself a l-value.  */
      /* XXX: to change when we support s[1] = 'x'  */
      {
        pkl_ast_node entity = PKL_AST_INDEXER_ENTITY (node);
        pkl_ast_node entity_type = PKL_AST_TYPE (entity);

        if (PKL_AST_TYPE_CODE (entity_type) == PKL_TYPE_ARRAY)
          return pkl_ast_lvalue_p (entity);

        break;
      }
    case PKL_AST_EXP:
      if (PKL_AST_EXP_CODE (node) == PKL_AST_OP_BCONC)
        return (pkl_ast_lvalue_p (PKL_AST_EXP_OPERAND (node, 0))
                && pkl_ast_lvalue_p (PKL_AST_EXP_OPERAND (node, 1)));
      break;
    default:
      break;
    }

  return 0;
}

#ifdef PKL_DEBUG

/* The following macros are commodities to be used to keep the
   `pkl_ast_print' function as readable and easy to update as
   possible.  Do not use them anywhere else.  */

#define IPRINTF(...)                            \
  do                                            \
    {                                           \
      int i;                                    \
      for (i = 0; i < indent; i++)              \
        if (indent >= 2 && i % 2 == 0)          \
          fprintf (fp, "|");                    \
        else                                    \
          fprintf (fp, " ");                    \
      fprintf (fp, __VA_ARGS__);                \
    } while (0)

#define PRINT_AST_IMM(NAME,MACRO,FMT)                    \
  do                                                     \
    {                                                    \
      IPRINTF (#NAME ":\n");                             \
      IPRINTF ("  " FMT "\n", PKL_AST_##MACRO (ast));    \
    }                                                    \
  while (0)

#define PRINT_AST_SUBAST(NAME,MACRO)                            \
  do                                                            \
    {                                                           \
      IPRINTF (#NAME ":\n");                                    \
      pkl_ast_print_1 (fp, PKL_AST_##MACRO (ast), indent + 2);  \
    }                                                           \
  while (0)

#define PRINT_AST_OPT_IMM(NAME,MACRO,FMT)       \
  if (PKL_AST_##MACRO (ast))                    \
    {                                           \
      PRINT_AST_IMM (NAME, MACRO, FMT);         \
    }

#define PRINT_AST_OPT_SUBAST(NAME,MACRO)        \
  if (PKL_AST_##MACRO (ast))                    \
    {                                           \
      PRINT_AST_SUBAST (NAME, MACRO);           \
    }

#define PRINT_AST_SUBAST_CHAIN(MACRO)           \
  for (child = PKL_AST_##MACRO (ast);           \
       child;                                   \
       child = PKL_AST_CHAIN (child))           \
    {                                           \
      pkl_ast_print_1 (fp, child, indent + 2);  \
    }

/* Auxiliary function used by `pkl_ast_print', defined below.  */

static void
pkl_ast_print_1 (FILE *fp, pkl_ast_node ast, int indent)
{
  pkl_ast_node child;
  size_t i;

  if (ast == NULL)
    {
      IPRINTF ("NULL::\n");
      return;
    }

#define PRINT_COMMON_FIELDS                                     \
  do                                                            \
    {                                                           \
      IPRINTF ("uid: %" PRIu64 "\n", PKL_AST_UID (ast));        \
      IPRINTF ("refcount: %d\n", PKL_AST_REFCOUNT (ast));\
      IPRINTF ("location: %d,%d-%d,%d\n",                       \
               PKL_AST_LOC (ast).first_line,                    \
               PKL_AST_LOC (ast).first_column,                  \
               PKL_AST_LOC (ast).last_line,                     \
               PKL_AST_LOC (ast).last_column);                  \
    }                                                           \
  while (0)

  switch (PKL_AST_CODE (ast))
    {
    case PKL_AST_PROGRAM:
      IPRINTF ("PROGRAM::\n");

      PRINT_COMMON_FIELDS;
      PRINT_AST_SUBAST_CHAIN (PROGRAM_ELEMS);
      break;

    case PKL_AST_IDENTIFIER:
      IPRINTF ("IDENTIFIER::\n");

      PRINT_COMMON_FIELDS;
      PRINT_AST_IMM (length, IDENTIFIER_LENGTH, "%zu");
      PRINT_AST_IMM (pointer, IDENTIFIER_POINTER, "%p");
      PRINT_AST_OPT_IMM (*pointer, IDENTIFIER_POINTER, "'%s'");
      break;

    case PKL_AST_INTEGER:
      IPRINTF ("INTEGER::\n");

      PRINT_COMMON_FIELDS;
      PRINT_AST_SUBAST (type, TYPE);
      PRINT_AST_IMM (value, INTEGER_VALUE, "%" PRIu64);
      break;

    case PKL_AST_STRING:
      IPRINTF ("STRING::\n");

      PRINT_COMMON_FIELDS;
      PRINT_AST_SUBAST (type, TYPE);
      PRINT_AST_IMM (length, STRING_LENGTH, "%zu");
      PRINT_AST_IMM (pointer, STRING_POINTER, "%p");
      PRINT_AST_OPT_IMM (*pointer, STRING_POINTER, "'%s'");
      break;

    case PKL_AST_EXP:
      {

#define PKL_DEF_OP(SYM, STRING) STRING,
        static const char *pkl_ast_op_name[] =
          {
#include "pkl-ops.def"
          };
#undef PKL_DEF_OP

        IPRINTF ("EXPRESSION::\n");

        PRINT_COMMON_FIELDS;
        IPRINTF ("opcode: %s\n",
                 pkl_ast_op_name[PKL_AST_EXP_CODE (ast)]);
        if (PKL_AST_EXP_ATTR (ast) != PKL_AST_ATTR_NONE)
          IPRINTF ("attr: %s\n", pkl_attr_name (PKL_AST_EXP_ATTR (ast)));
        IPRINTF ("literal_p: %d\n", PKL_AST_LITERAL_P (ast));
        PRINT_AST_SUBAST (type, TYPE);
        PRINT_AST_IMM (numops, EXP_NUMOPS, "%d");
        IPRINTF ("operands:\n");
        for (i = 0; i < PKL_AST_EXP_NUMOPS (ast); i++)
          pkl_ast_print_1 (fp, PKL_AST_EXP_OPERAND (ast, i),
                         indent + 2);
        break;
      }

    case PKL_AST_COND_EXP:
      IPRINTF ("COND_EXPRESSION::\n");

      PRINT_COMMON_FIELDS;
      PRINT_AST_SUBAST (condition, COND_EXP_COND);
      PRINT_AST_OPT_SUBAST (thenexp, COND_EXP_THENEXP);
      PRINT_AST_OPT_SUBAST (elseexp, COND_EXP_ELSEEXP);
      break;

    case PKL_AST_STRUCT_FIELD:
      IPRINTF ("STRUCT_FIELD::\n");

      PRINT_COMMON_FIELDS;
      PRINT_AST_SUBAST (type, TYPE);
      PRINT_AST_SUBAST (name, STRUCT_FIELD_NAME);
      PRINT_AST_SUBAST (exp, STRUCT_FIELD_EXP);
      break;

    case PKL_AST_STRUCT:
      IPRINTF ("STRUCT::\n");

      PRINT_COMMON_FIELDS;
      PRINT_AST_SUBAST (type, TYPE);
      PRINT_AST_IMM (nelem, STRUCT_NELEM, "%zu");
      IPRINTF ("elems:\n");
      PRINT_AST_SUBAST_CHAIN (STRUCT_FIELDS);
      break;

    case PKL_AST_ARRAY_INITIALIZER:
      IPRINTF ("ARRAY_INITIALIZER::\n");

      PRINT_COMMON_FIELDS;
      PRINT_AST_SUBAST (index, ARRAY_INITIALIZER_INDEX);
      PRINT_AST_SUBAST (exp, ARRAY_INITIALIZER_EXP);
      break;

    case PKL_AST_ARRAY:
      IPRINTF ("ARRAY::\n");

      PRINT_COMMON_FIELDS;
      PRINT_AST_IMM (nelem, ARRAY_NELEM, "%zu");
      PRINT_AST_IMM (ninitializer, ARRAY_NINITIALIZER, "%zu");
      PRINT_AST_SUBAST (type, TYPE);
      IPRINTF ("initializers:\n");
      PRINT_AST_SUBAST_CHAIN (ARRAY_INITIALIZERS);
      break;

    case PKL_AST_ENUMERATOR:
      IPRINTF ("ENUMERATOR::\n");

      PRINT_COMMON_FIELDS;
      PRINT_AST_SUBAST (identifier, ENUMERATOR_IDENTIFIER);
      PRINT_AST_SUBAST (value, ENUMERATOR_VALUE);
      break;

    case PKL_AST_ENUM:
      IPRINTF ("ENUM::\n");

      PRINT_COMMON_FIELDS;
      PRINT_AST_SUBAST (tag, ENUM_TAG);
      IPRINTF ("values:\n");
      PRINT_AST_SUBAST_CHAIN (ENUM_VALUES);
      break;

    case PKL_AST_TYPE:
      IPRINTF ("TYPE::\n");

      PRINT_COMMON_FIELDS;
      if (PKL_AST_TYPE_NAME (ast))
        PRINT_AST_SUBAST (name, TYPE_NAME);
      else
        {
          IPRINTF ("code:\n");
          switch (PKL_AST_TYPE_CODE (ast))
            {
            case PKL_TYPE_ANY: IPRINTF ("  any\n"); break;
            case PKL_TYPE_INTEGRAL: IPRINTF ("  integral\n"); break;
            case PKL_TYPE_STRING: IPRINTF ("  string\n"); break;
            case PKL_TYPE_ARRAY: IPRINTF ("  array\n"); break;
            case PKL_TYPE_STRUCT: IPRINTF ("  struct\n"); break;
            case PKL_TYPE_FUNCTION: IPRINTF ("  function\n"); break;
            case PKL_TYPE_OFFSET: IPRINTF ("  offset\n"); break;
            default:
              IPRINTF (" unknown (%d)\n", PKL_AST_TYPE_CODE (ast));
              break;
            }
          PRINT_AST_IMM (complete, TYPE_COMPLETE, "%d");
          switch (PKL_AST_TYPE_CODE (ast))
            {
            case PKL_TYPE_INTEGRAL:
              PRINT_AST_IMM (signed_p, TYPE_I_SIGNED_P, "%d");
              PRINT_AST_IMM (size, TYPE_I_SIZE, "%zu");
              break;
            case PKL_TYPE_ARRAY:
              PRINT_AST_SUBAST (bound, TYPE_A_BOUND);
              PRINT_AST_SUBAST (etype, TYPE_A_ETYPE);
              break;
            case PKL_TYPE_STRUCT:
              PRINT_AST_IMM (pinned_p, TYPE_S_PINNED_P, "%d");
              PRINT_AST_IMM (union_p, TYPE_S_UNION_P, "%d");
              PRINT_AST_IMM (nelem, TYPE_S_NELEM, "%zu");
              PRINT_AST_IMM (nfield, TYPE_S_NFIELD, "%zu");
              PRINT_AST_IMM (ndecl, TYPE_S_NDECL, "%zu");
              PRINT_AST_SUBAST (itype, TYPE_S_ITYPE);
              IPRINTF ("elems:\n");
              PRINT_AST_SUBAST_CHAIN (TYPE_S_ELEMS);
              break;
            case PKL_TYPE_FUNCTION:
              PRINT_AST_IMM (narg, TYPE_F_NARG, "%d");
              IPRINTF ("args:\n");
              PRINT_AST_SUBAST_CHAIN (TYPE_F_ARGS);
              break;
            case PKL_TYPE_OFFSET:
              PRINT_AST_SUBAST (base_type, TYPE_O_BASE_TYPE);
              PRINT_AST_SUBAST (unit, TYPE_O_UNIT);
              break;
            case PKL_TYPE_STRING:
            case PKL_TYPE_ANY:
            default:
              break;
            }
        }
      break;

    case PKL_AST_STRUCT_TYPE_FIELD:
      IPRINTF ("STRUCT_TYPE_FIELD::\n");

      PRINT_COMMON_FIELDS;
      PRINT_AST_SUBAST (name, STRUCT_TYPE_FIELD_NAME);
      PRINT_AST_SUBAST (type, STRUCT_TYPE_FIELD_TYPE);
      PRINT_AST_SUBAST (exp, STRUCT_TYPE_FIELD_CONSTRAINT);
      PRINT_AST_SUBAST (exp, STRUCT_TYPE_FIELD_INITIALIZER);
      PRINT_AST_SUBAST (exp, STRUCT_TYPE_FIELD_LABEL);
      PRINT_AST_SUBAST (exp, STRUCT_TYPE_FIELD_OPTCOND);
      PRINT_AST_IMM (endian, STRUCT_TYPE_FIELD_ENDIAN, "%d");
      break;

    case PKL_AST_FUNC_TYPE_ARG:
      IPRINTF ("FUNC_TYPE_ARG::\n");

      PRINT_COMMON_FIELDS;
      PRINT_AST_SUBAST (type, FUNC_TYPE_ARG_TYPE);
      PRINT_AST_SUBAST (type, FUNC_TYPE_ARG_NAME);
      PRINT_AST_IMM (optional, FUNC_TYPE_ARG_OPTIONAL, "%d");
      break;

    case PKL_AST_TRIMMER:
      IPRINTF ("TRIMMER::\n");
      PRINT_AST_SUBAST (from, TRIMMER_FROM);
      PRINT_AST_SUBAST (to, TRIMMER_TO);
      PRINT_AST_SUBAST (entity, TRIMMER_ENTITY);
      break;

    case PKL_AST_INDEXER:
      IPRINTF ("INDEXER::\n");

      PRINT_COMMON_FIELDS;
      PRINT_AST_SUBAST (type, TYPE);
      PRINT_AST_SUBAST (entity, INDEXER_ENTITY);
      PRINT_AST_SUBAST (index, INDEXER_INDEX);
      break;

    case PKL_AST_FUNC:
      IPRINTF ("FUNC::\n");

      PRINT_COMMON_FIELDS;
      PRINT_AST_IMM (nargs, FUNC_NARGS, "%d");
      PRINT_AST_IMM (method_p, FUNC_METHOD_P, "%d");
      PRINT_AST_SUBAST (ret_type, FUNC_RET_TYPE);
      PRINT_AST_SUBAST_CHAIN (FUNC_ARGS);
      PRINT_AST_SUBAST (first_opt_arg, FUNC_FIRST_OPT_ARG);
      PRINT_AST_SUBAST (body, FUNC_BODY);
      break;

    case PKL_AST_FUNC_ARG:
      IPRINTF ("FUNC_ARG::\n");

      PRINT_COMMON_FIELDS;
      PRINT_AST_SUBAST (type, FUNC_ARG_TYPE);
      PRINT_AST_SUBAST (identifier, FUNC_ARG_IDENTIFIER);
      PRINT_AST_IMM (vararg, FUNC_ARG_VARARG, "%d");
      break;

    case PKL_AST_STRUCT_REF:
      IPRINTF ("STRUCT_REF::\n");

      PRINT_COMMON_FIELDS;
      PRINT_AST_SUBAST (type, TYPE);
      PRINT_AST_SUBAST (struct, STRUCT_REF_STRUCT);
      PRINT_AST_SUBAST (identifier, STRUCT_REF_IDENTIFIER);
      break;

    case PKL_AST_DECL:
      IPRINTF ("DECL::\n");

      PRINT_COMMON_FIELDS;
      PRINT_AST_IMM (kind, DECL_KIND, "%d");
      if (PKL_AST_DECL_SOURCE (ast))
        PRINT_AST_IMM (source, DECL_SOURCE, "'%s'");
      PRINT_AST_SUBAST (name, DECL_NAME);
      PRINT_AST_SUBAST (initial, DECL_INITIAL);
      break;

    case PKL_AST_OFFSET:
      IPRINTF ("OFFSET::\n");

      PRINT_COMMON_FIELDS;
      PRINT_AST_SUBAST (type, TYPE);
      PRINT_AST_SUBAST (magnitude, OFFSET_MAGNITUDE);
      PRINT_AST_SUBAST (unit, OFFSET_UNIT);
      break;

    case PKL_AST_CAST:
      IPRINTF ("CAST::\n");

      PRINT_COMMON_FIELDS;
      PRINT_AST_SUBAST (type, TYPE);
      PRINT_AST_SUBAST (cast_type, CAST_TYPE);
      PRINT_AST_SUBAST (exp, CAST_EXP);
      break;

    case PKL_AST_ISA:
      IPRINTF ("ISA::\n");

      PRINT_COMMON_FIELDS;
      PRINT_AST_SUBAST (type, TYPE);
      PRINT_AST_SUBAST (isa_type, ISA_TYPE);
      PRINT_AST_SUBAST (exp, ISA_EXP);
      break;

    case PKL_AST_MAP:
      IPRINTF ("MAP::\n");

      PRINT_COMMON_FIELDS;
      PRINT_AST_SUBAST (type, TYPE);
      PRINT_AST_SUBAST (map_type, MAP_TYPE);
      PRINT_AST_SUBAST (ios, MAP_IOS);
      PRINT_AST_SUBAST (offset, MAP_OFFSET);
      break;

    case PKL_AST_SCONS:
      IPRINTF ("SCONS::\n");

      PRINT_COMMON_FIELDS;
      PRINT_AST_SUBAST (type, TYPE);
      PRINT_AST_SUBAST (scons_type, SCONS_TYPE);
      PRINT_AST_SUBAST (scons_value, SCONS_VALUE);
      break;

    case PKL_AST_FUNCALL:
      IPRINTF ("FUNCALL::\n");

      PRINT_COMMON_FIELDS;
      PRINT_AST_SUBAST (function, FUNCALL_FUNCTION);
      IPRINTF ("args:\n");
      PRINT_AST_SUBAST_CHAIN (FUNCALL_ARGS);
      break;

    case PKL_AST_FUNCALL_ARG:
      IPRINTF ("FUNCALL_ARG::\n");

      PRINT_AST_SUBAST (exp, FUNCALL_ARG_EXP);
      PRINT_AST_SUBAST (name, FUNCALL_ARG_NAME);
      PRINT_AST_IMM (first_vararg, FUNCALL_ARG_FIRST_VARARG, "%d");
      break;

    case PKL_AST_VAR:
      IPRINTF ("VAR::\n");

      PRINT_COMMON_FIELDS;
      PRINT_AST_SUBAST (type, TYPE);
      PRINT_AST_IMM (back, VAR_BACK, "%d");
      PRINT_AST_IMM (over, VAR_OVER, "%d");
      break;

    case PKL_AST_COMP_STMT:
      IPRINTF ("COMP_STMT::\n");

      PRINT_COMMON_FIELDS;
      PRINT_AST_IMM (builtin, COMP_STMT_BUILTIN, "%d");
      PRINT_AST_IMM (numvars, COMP_STMT_NUMVARS, "%d");
      IPRINTF ("stmts:\n");
      PRINT_AST_SUBAST_CHAIN (COMP_STMT_STMTS);
      break;

    case PKL_AST_ASS_STMT:
      IPRINTF ("ASS_STMT::\n");

      PRINT_COMMON_FIELDS;
      PRINT_AST_SUBAST (lvalue, ASS_STMT_LVALUE);
      PRINT_AST_SUBAST (exp, ASS_STMT_EXP);
      break;

    case PKL_AST_IF_STMT:
      IPRINTF ("IF_STMT::\n");

      PRINT_COMMON_FIELDS;
      PRINT_AST_SUBAST (exp, IF_STMT_EXP);
      PRINT_AST_SUBAST (then_stmt, IF_STMT_THEN_STMT);
      PRINT_AST_SUBAST (else_stmt, IF_STMT_ELSE_STMT);
      break;

    case PKL_AST_LOOP_STMT:
      IPRINTF ("LOOP_STMT::\n");

      PRINT_COMMON_FIELDS;
      PRINT_AST_SUBAST (iterator, LOOP_STMT_ITERATOR);
      PRINT_AST_SUBAST (condition, LOOP_STMT_CONDITION);
      PRINT_AST_SUBAST (body, LOOP_STMT_BODY);
      break;

    case PKL_AST_LOOP_STMT_ITERATOR:
      IPRINTF ("LOOP_STMT_ITERATOR::\n");

      PRINT_COMMON_FIELDS;
      PRINT_AST_SUBAST (decl, LOOP_STMT_ITERATOR_DECL);
      PRINT_AST_SUBAST (container, LOOP_STMT_ITERATOR_CONTAINER);
      break;

    case PKL_AST_RETURN_STMT:
      IPRINTF ("RETURN_STMT::\n");

      PRINT_COMMON_FIELDS;
      PRINT_AST_SUBAST (exp, RETURN_STMT_EXP);
      break;

    case PKL_AST_EXP_STMT:
      IPRINTF ("EXP_STMT::\n");

      PRINT_COMMON_FIELDS;
      PRINT_AST_SUBAST (exp_stmt, EXP_STMT_EXP);
      break;

    case PKL_AST_TRY_CATCH_STMT:
      IPRINTF ("TRY_CATCH_STMT::\n");

      PRINT_COMMON_FIELDS;
      PRINT_AST_SUBAST (try_catch_stmt_code, TRY_CATCH_STMT_CODE);
      PRINT_AST_SUBAST (try_catch_stmt_handler, TRY_CATCH_STMT_HANDLER);
      PRINT_AST_SUBAST (try_catch_stmt_arg, TRY_CATCH_STMT_ARG);
      PRINT_AST_SUBAST (try_catch_stmt_exp, TRY_CATCH_STMT_EXP);
      break;

    case PKL_AST_TRY_UNTIL_STMT:
      IPRINTF ("TRY_UNTIL_STMT::\n");

      PRINT_COMMON_FIELDS;
      PRINT_AST_SUBAST (try_until_stmt_code, TRY_UNTIL_STMT_CODE);
      PRINT_AST_SUBAST (try_until_stmt_exp, TRY_UNTIL_STMT_EXP);
      break;

    case PKL_AST_PRINT_STMT_ARG:
      IPRINTF ("PRINT_STMT_ARG::\n");
      PRINT_COMMON_FIELDS;
      if (PKL_AST_PRINT_STMT_ARG_BEGIN_SC (ast))
        PRINT_AST_IMM (begin_sc, PRINT_STMT_ARG_BEGIN_SC, "'%s'");
      if (PKL_AST_PRINT_STMT_ARG_END_SC (ast))
        PRINT_AST_IMM (end_sc, PRINT_STMT_ARG_END_SC, "'%s'");
      if (PKL_AST_PRINT_STMT_ARG_SUFFIX (ast))
        PRINT_AST_IMM (suffix, PRINT_STMT_ARG_SUFFIX, "'%s'");
      PRINT_AST_SUBAST (exp, PRINT_STMT_ARG_EXP);
      break;

    case PKL_AST_PRINT_STMT:
      IPRINTF ("PRINT_STMT::\n");
      PRINT_COMMON_FIELDS;
      if (PKL_AST_PRINT_STMT_PREFIX (ast))
        PRINT_AST_IMM (prefix, PRINT_STMT_PREFIX, "'%s'");
      PRINT_AST_SUBAST (fmt, PRINT_STMT_FMT);
      PRINT_AST_SUBAST_CHAIN (PRINT_STMT_TYPES);
      PRINT_AST_SUBAST_CHAIN (PRINT_STMT_ARGS);
      break;

    case PKL_AST_BREAK_STMT:
      IPRINTF ("BREAK_STMT::\n");
      PRINT_COMMON_FIELDS;
      break;

    case PKL_AST_RAISE_STMT:
      IPRINTF ("RAISE_STMT::\n");

      PRINT_COMMON_FIELDS;
      PRINT_AST_SUBAST (raise_stmt_exp, RAISE_STMT_EXP);
      break;

    case PKL_AST_NULL_STMT:
      IPRINTF ("NULL_STMT::\n");

      PRINT_COMMON_FIELDS;
      break;

    default:
      IPRINTF ("UNKNOWN:: code=%d\n", PKL_AST_CODE (ast));
      break;
    }
}

/* Dump a printable representation of AST to the file descriptor FD.
   This function is intended to be useful to debug the PKL
   compiler.  */

void
pkl_ast_print (FILE *fp, pkl_ast_node ast)
{
  pkl_ast_print_1 (fp, ast, 0);
}

#undef IPRINTF
#undef PRINT_AST_IMM
#undef PRINT_AST_SUBAST
#undef PRINT_AST_OPT_FIELD
#undef PRINT_AST_OPT_SUBAST

#endif /* PKL_DEBUG */
