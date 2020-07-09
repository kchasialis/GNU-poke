/* pkl-ast.h - Abstract Syntax Tree for Poke.  */

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

#ifndef PKL_AST_H
#define PKL_AST_H

#include <config.h>

#include <stdio.h>
#include <stdint.h>

#include "pvm.h" /* For pvm_val */

/* The following enumeration defines the codes characterizing the
   several types of nodes supported in the PKL abstract syntax
   trees.  */

enum pkl_ast_code
{
  PKL_AST_PROGRAM,
  /* Expressions.  */
  PKL_AST_FIRST_EXP,
  PKL_AST_EXP = PKL_AST_FIRST_EXP,
  PKL_AST_COND_EXP,
  PKL_AST_INTEGER,
  PKL_AST_STRING,
  PKL_AST_IDENTIFIER,
  PKL_AST_ARRAY,
  PKL_AST_ARRAY_INITIALIZER,
  PKL_AST_INDEXER,
  PKL_AST_TRIMMER,
  PKL_AST_STRUCT,
  PKL_AST_STRUCT_FIELD,
  PKL_AST_STRUCT_REF,
  PKL_AST_OFFSET,
  PKL_AST_CAST,
  PKL_AST_ISA,
  PKL_AST_MAP,
  PKL_AST_SCONS,
  PKL_AST_FUNCALL,
  PKL_AST_FUNCALL_ARG,
  PKL_AST_VAR,
  PKL_AST_GCD,
  PKL_AST_LAST_EXP = PKL_AST_GCD,
  /* Types.  */
  PKL_AST_TYPE,
  PKL_AST_STRUCT_TYPE_FIELD,
  PKL_AST_FUNC_TYPE_ARG,
  PKL_AST_ENUM,
  PKL_AST_ENUMERATOR,
  /* Functions.  */
  PKL_AST_FUNC,
  PKL_AST_FUNC_ARG,
  /* Declarations.  */
  PKL_AST_DECL,
  /* Statements.  */
  PKL_AST_FIRST_STMT,
  PKL_AST_COMP_STMT = PKL_AST_FIRST_STMT,
  PKL_AST_NULL_STMT,
  PKL_AST_ASS_STMT,
  PKL_AST_IF_STMT,
  PKL_AST_LOOP_STMT,
  PKL_AST_LOOP_STMT_ITERATOR,
  PKL_AST_RETURN_STMT,
  PKL_AST_EXP_STMT,
  PKL_AST_TRY_CATCH_STMT,
  PKL_AST_TRY_UNTIL_STMT,
  PKL_AST_PRINT_STMT,
  PKL_AST_BREAK_STMT,
  PKL_AST_RAISE_STMT,
  PKL_AST_LAST_STMT = PKL_AST_RAISE_STMT,
  PKL_AST_PRINT_STMT_ARG,
  PKL_AST_LAST
};

/* The following macros implement some node code categories.  */

#define PKL_AST_IS_EXP(AST)                              \
  (PKL_AST_CODE ((AST)) >= PKL_AST_FIRST_EXP             \
   && PKL_AST_CODE ((AST)) <= PKL_AST_LAST_EXP)

#define PKL_AST_IS_STMT(AST)                            \
  (PKL_AST_CODE ((AST)) >= PKL_AST_FIRST_STMT            \
   && PKL_AST_CODE ((AST)) <= PKL_AST_LAST_STMT)

/* The AST nodes representing expressions are characterized by
   operators (see below in this file for more details on this.)  The
   following enumeration defines the operator codes.

   The definitions of the operators are in pkl-ops.def.  */

#define PKL_DEF_OP(SYM, STRING) SYM,
enum pkl_ast_op
{
#include "pkl-ops.def"
 PKL_AST_OP_LAST
};
#undef PKL_DEF_OP

/* Similarly, attribute operators are characterized by an attribute.
   The following enumeration defines the attribute codes.

   The definitions of the attributes are in pkl-attrs.def.  */

#define PKL_DEF_ATTR(SYM, STRING) SYM,
enum pkl_ast_attr
{
#include "pkl-attrs.def"
 PKL_AST_ATTR_NONE
};
#undef PKL_DEF_ATTR

/* Certain AST nodes can be characterized of featuring a byte
   endianness.  The following values are supported:

   DFL means the default endianness, which is the endianness used by
   the system running poke.

   MSB means that the most significative bytes come first.  This is
   what is popularly known as big-endian.

   LSB means that the least significative bytes come first.  This is
   what is known as little-endian.

   In both endiannesses the bits inside the bytes are ordered from
   most significative to least significative.  */

enum pkl_ast_endian
{
  PKL_AST_ENDIAN_DFL, /* Default endian.  */
  PKL_AST_ENDIAN_MSB, /* Big-endian.  */
  PKL_AST_ENDIAN_LSB  /* Little-endian.  */
};

enum pkl_ast_type_code
{
  PKL_TYPE_INTEGRAL,
  PKL_TYPE_STRING,
  PKL_TYPE_VOID,
  PKL_TYPE_ARRAY,
  PKL_TYPE_STRUCT,
  PKL_TYPE_FUNCTION,
  PKL_TYPE_OFFSET,
  PKL_TYPE_ANY,
  PKL_TYPE_NOTYPE,
};

/* Next we define the several supported types of nodes in the abstract
   syntax tree, which are discriminated using the codes defined in the
   `pkl_ast_code' enumeration above.

   Accessor macros are defined to access the attributes of the
   different nodes, and should be used as both l-values and r-values
   to inspect and modify nodes, respectively.

   Declarations for constructor functions are also provided, that can
   be used to create new instances of nodes.  */

typedef union pkl_ast_node *pkl_ast_node;

/* The `pkl_ast_common' struct defines fields which are common to
   every node in the AST, regardless of their type.

   AST is a pointer to the `pkl_ast' structure containing this node.

   UID is an unique identifier that characterizes the node.  It is
   allocated when the node is created, and then never reused again.

   CHAIN and CHAIN2 are used to chain AST nodes together.  This serves
   several purposes in the compiler:

   CHAIN is used to form sibling relationships in the tree.

   CHAIN2 is used to link nodes together in containers, such as hash
   table buckets, and frames.

   The `pkl_ast_chainon' utility function is provided in order to
   confortably add elements to a list of nodes.  It operates on CHAIN,
   not CHAIN2.

   CODE identifies the type of node.

   LOC is the location in the source program of the entity represented
   by the node.  It is the task of the parser to fill in this
   information, which is used in error reporting.

   The LITERAL_P flag is used in expression nodes, and tells whether
   the expression is constant, i.e. whether the value of the
   expression can be calculated at compile time.  This is used to
   implement some optimizations.

   It is possible for a node to be referred from more than one place.
   To manage memory, we use a REFCOUNT that is initially 0.  The
   ASTREF macro defined below tells the node a new reference is being
   made.

   There is no constructor defined for common nodes.  */

#define PKL_AST_AST(AST) ((AST)->common.ast)
#define PKL_AST_UID(AST) ((AST)->common.uid)
#define PKL_AST_CHAIN(AST) ((AST)->common.chain)
#define PKL_AST_TYPE(AST) ((AST)->common.type)
#define PKL_AST_CHAIN2(AST) ((AST)->common.chain2)
#define PKL_AST_CODE(AST) ((AST)->common.code)
#define PKL_AST_LOC(AST) ((AST)->common.loc)
#define PKL_AST_LITERAL_P(AST) ((AST)->common.literal_p)
#define PKL_AST_REGISTERED_P(AST) ((AST)->common.registered_p)
#define PKL_AST_REFCOUNT(AST) ((AST)->common.refcount)

/* NOTE: both ASTREF and ASTDEREF need an l-value!  */
#define ASTREF(AST) ((AST) ? (++((AST)->common.refcount), (AST)) \
                     : NULL)
#define ASTDEREF(AST) ((AST) ? (--((AST)->common.refcount), (AST)) \
                       : NULL)

struct pkl_ast_loc
{
  int first_line;
  int first_column;
  int last_line;
  int last_column;
};

typedef struct pkl_ast_loc pkl_ast_loc;

static struct pkl_ast_loc PKL_AST_NOLOC __attribute__((unused))
   = { 0, 0, 0, 0 };

#define PKL_AST_LOC_VALID(L)                    \
  (!((L).first_line == 0                        \
     && (L).first_column == 0                   \
     && (L).last_line == 0                      \
     && (L).last_column == 0))

struct pkl_ast_common
{
  struct pkl_ast *ast;
  uint64_t uid;
  union pkl_ast_node *chain;
  union pkl_ast_node *type;
  union pkl_ast_node *chain2;
  enum pkl_ast_code code : 8;
  struct pkl_ast_loc loc;
  int refcount;

  unsigned literal_p : 1;
};

pkl_ast_node pkl_ast_chainon (pkl_ast_node ast1,
                              pkl_ast_node ast2)
  __attribute__ ((visibility ("hidden")));

typedef struct pkl_ast *pkl_ast; /* Forward declaration. */

/* PKL_AST_PROGRAM nodes represent PKL programs.

   ELEMS points to a possibly empty list of struct definitions,
   enumerations, and expressions i.e. to nodes of types
   PKL_AST_STRUCT, PKL_AST_ENUM and PKL_AST_EXP respectively.  */

#define PKL_AST_PROGRAM_ELEMS(AST) ((AST)->program.elems)

struct pkl_ast_program
{
  struct pkl_ast_common common;
  union pkl_ast_node *elems;
};

pkl_ast_node pkl_ast_make_program (pkl_ast ast,
                                   pkl_ast_node declarations)
  __attribute__ ((visibility ("hidden")));

/* PKL_AST_IDENTIFIER nodes represent identifiers in PKL programs.

   POINTER must point to a NULL-terminated string.

   LENGTH contains the size in bytes of the identifier

   BACK and OVER conform the lexical address to find the storage of
   the entity represented by the identifier.  See pkl-env.h for a
   description of these.  */

#define PKL_AST_IDENTIFIER_LENGTH(AST) ((AST)->identifier.length)
#define PKL_AST_IDENTIFIER_POINTER(AST) ((AST)->identifier.pointer)
#define PKL_AST_IDENTIFIER_BACK(AST) ((AST)->identifier.back)
#define PKL_AST_IDENTIFIER_OVER(AST) ((AST)->identifier.over)

struct pkl_ast_identifier
{
  struct pkl_ast_common common;
  size_t length;
  char *pointer;
  int back;
  int over;
};

pkl_ast_node pkl_ast_make_identifier (pkl_ast ast,
                                      const char *str)
  __attribute__ ((visibility ("hidden")));

/* PKL_AST_INTEGER nodes represent integer constants in poke programs.

   VALUE contains a 64-bit unsigned integer.  This contains the
   encoding of a Poke integer, which may be signed or unsigned.  The
   lexer generates only unsigned integers.  */

#define PKL_AST_INTEGER_VALUE(AST) ((AST)->integer.value)

struct pkl_ast_integer
{
  struct pkl_ast_common common;
  uint64_t value;
};

pkl_ast_node pkl_ast_make_integer (pkl_ast ast,
                                   uint64_t value)
  __attribute__ ((visibility ("hidden")));

/* PKL_AST_STRING nodes represent string literals in PKL programs.

   POINTER must point to a NULL-terminated string.
   LENGTH contains the size in bytes of the string.  */

#define PKL_AST_STRING_LENGTH(AST) ((AST)->string.length)
#define PKL_AST_STRING_POINTER(AST) ((AST)->string.pointer)

struct pkl_ast_string
{
  struct pkl_ast_common common;
  size_t length;
  char *pointer;
};

pkl_ast_node pkl_ast_make_string (pkl_ast ast,
                                  const char *str)
  __attribute__ ((visibility ("hidden")));

/* PKL_AST_ARRAY nodes represent array literals.  Each array holds a
   sequence of elements, all of them having the same type.  There must
   be at least one element in the array, i.e. emtpy arrays are not
   allowed.  */

#define PKL_AST_ARRAY_NELEM(AST) ((AST)->array.nelem)
#define PKL_AST_ARRAY_NINITIALIZER(AST) ((AST)->array.ninitializer)
#define PKL_AST_ARRAY_INITIALIZERS(AST) ((AST)->array.initializers)

struct pkl_ast_array
{
  struct pkl_ast_common common;

  size_t nelem;
  size_t ninitializer;
  union pkl_ast_node *initializers;
};

pkl_ast_node pkl_ast_make_array (pkl_ast ast,
                                 size_t nelem,
                                 size_t ninitializer,
                                 pkl_ast_node initializers)
  __attribute__ ((visibility ("hidden")));


/* PKL_AST_ARRAY_INITIALIZER nodes represent initializers in array
   literals.  They are characterized by an index into the array and a
   contained expression.  */

#define PKL_AST_ARRAY_INITIALIZER_INDEX(AST) ((AST)->array_initializer.index)
#define PKL_AST_ARRAY_INITIALIZER_EXP(AST) ((AST)->array_initializer.exp)

struct pkl_ast_array_initializer
{
  struct pkl_ast_common common;

  union pkl_ast_node *index;
  union pkl_ast_node *exp;
};

pkl_ast_node pkl_ast_make_array_initializer (pkl_ast ast,
                                             pkl_ast_node index,
                                             pkl_ast_node exp)
  __attribute__ ((visibility ("hidden")));


/* PKL_AST_STRUCT nodes represent struct literals.  */

#define PKL_AST_STRUCT_NELEM(AST) ((AST)->sct.nelem)
#define PKL_AST_STRUCT_FIELDS(AST) ((AST)->sct.elems)

struct pkl_ast_struct
{
  struct pkl_ast_common common;

  size_t nelem;
  union pkl_ast_node *elems;
};

pkl_ast_node pkl_ast_make_struct (pkl_ast ast,
                                  size_t nelem,
                                  pkl_ast_node elems)
  __attribute__ ((visibility ("hidden")));

/* PKL_AST_STRUCT_FIELD nodes represent elements in struct
   literals.

   NAME is a PKL_AST_IDENTIFIER node with the name of the struct
   element.  If no name is specified, this is NULL.

   EXP is the value of the struct field.  */

#define PKL_AST_STRUCT_FIELD_NAME(AST) ((AST)->sct_field.name)
#define PKL_AST_STRUCT_FIELD_EXP(AST) ((AST)->sct_field.exp)

struct pkl_ast_struct_field
{
  struct pkl_ast_common common;

  union pkl_ast_node *name;
  union pkl_ast_node *exp;
};

pkl_ast_node pkl_ast_make_struct_field (pkl_ast ast,
                                        pkl_ast_node name,
                                        pkl_ast_node exp)
  __attribute__ ((visibility ("hidden")));

/* PKL_AST_EXP nodes represent unary and binary expressions,
   consisting on an operator and one or two operators, respectively.

   The supported operators are specified in pkl-ops.def.
   The supported attributes are defined in pkl-attrs.def.

   In PKL_AST_OP_ATTR expressions, ATTR contains the code for the
   invoked attribute.

   FLAG is used for several purposes.

   There are two constructors for this node type: one for unary
   expressions, another for binary expressions.  */

#define PKL_AST_EXP_CODE(AST) ((AST)->exp.code)
#define PKL_AST_EXP_ATTR(AST) ((AST)->exp.attr)
#define PKL_AST_EXP_NUMOPS(AST) ((AST)->exp.numops)
#define PKL_AST_EXP_OPERAND(AST,I) ((AST)->exp.operands[(I)])
#define PKL_AST_EXP_FLAG(AST) ((AST)->exp.flag)

struct pkl_ast_exp
{
  struct pkl_ast_common common;

  enum pkl_ast_op code;
  enum pkl_ast_attr attr;
  uint8_t numops : 8;
  union pkl_ast_node *operands[2];
  int flag;
};

pkl_ast_node pkl_ast_make_unary_exp (pkl_ast ast,
                                     enum pkl_ast_op code,
                                     pkl_ast_node op)
  __attribute__ ((visibility ("hidden")));

pkl_ast_node pkl_ast_make_binary_exp (pkl_ast ast,
                                      enum pkl_ast_op code,
                                      pkl_ast_node op1,
                                      pkl_ast_node op2)
  __attribute__ ((visibility ("hidden")));

const char *pkl_attr_name (enum pkl_ast_attr attr);

/* PKL_AST_COND_EXP nodes represent conditional expressions, having
   exactly the same semantics than the C tertiary operator:

                   exp1 ? exp2 : exp3

   where exp1 is evaluated and then, depending on its value, either
   exp2 (if exp1 is not 0) or exp3 (if exp1 is 0) are executed.

   COND, THENEXP and ELSEEXP must point to expressions, i.e. to nodes
   node of type  PKL_AST_EXP or type PKL_AST_COND_EXP.  */

#define PKL_AST_COND_EXP_COND(AST) ((AST)->cond_exp.cond)
#define PKL_AST_COND_EXP_THENEXP(AST) ((AST)->cond_exp.thenexp)
#define PKL_AST_COND_EXP_ELSEEXP(AST) ((AST)->cond_exp.elseexp)

struct pkl_ast_cond_exp
{
  struct pkl_ast_common common;
  union pkl_ast_node *cond;
  union pkl_ast_node *thenexp;
  union pkl_ast_node *elseexp;
};

pkl_ast_node pkl_ast_make_cond_exp (pkl_ast ast,
                                    pkl_ast_node cond,
                                    pkl_ast_node thenexp,
                                    pkl_ast_node elseexp)
  __attribute__ ((visibility ("hidden")));

/* PKL_AST_ENUMERATOR nodes represent the definition of a constant
   into an enumeration.

   Each constant is characterized with an IDENTIFIER that identifies
   it globally (meaning enumerator identifiers must be unique), an
   optional VALUE, which must be a constant expression, and an
   optional doc-string.

   If the value is not explicitly provided, it must be calculated
   considering the rest of the enumerators in the enumeration, exactly
   like in C enums.  */

#define PKL_AST_ENUMERATOR_IDENTIFIER(AST) ((AST)->enumerator.identifier)
#define PKL_AST_ENUMERATOR_VALUE(AST) ((AST)->enumerator.value)

struct pkl_ast_enumerator
{
  struct pkl_ast_common common;
  union pkl_ast_node *identifier;
  union pkl_ast_node *value;
};

pkl_ast_node pkl_ast_make_enumerator (pkl_ast ast,
                                      pkl_ast_node identifier,
                                      pkl_ast_node value)
  __attribute__ ((visibility ("hidden")));

/* PKL_AST_ENUM nodes represent enumerations, having semantics much
   like the C enums.

   TAG is mandatory and must point to a PKL_AST_IDENTIFIER.  This
   identifier characterizes the enumeration globally in the enums
   namespace.

   VALUES must point to a chain of PKL_AST_ENUMERATOR nodes containing
   at least one node.  This means empty enumerations are not allowed.  */

#define PKL_AST_ENUM_TAG(AST) ((AST)->enumeration.tag)
#define PKL_AST_ENUM_VALUES(AST) ((AST)->enumeration.values)

struct pkl_ast_enum
{
  struct pkl_ast_common common;

  union pkl_ast_node *tag;
  union pkl_ast_node *values;
};

pkl_ast_node pkl_ast_make_enum (pkl_ast ast,
                                pkl_ast_node tag,
                                pkl_ast_node values)
  __attribute__ ((visibility ("hidden")));

/* PKL_AST_FUNC nodes represent a function definition.

   RET_TYPE is the type of the value returned by the function, or NULL
   if the type doesn't return any value.

   ARGS is a chain of PKL_AST_FUNC_ARG nodes describing the formal
   arguments to the function.  It can be NULL if the function takes no
   arguments.

   FIRST_OPT_ARG is the first argument (in ARGS) that has an
   associated initial.  If no argument in ARGS has an associated
   initial, this is NULL.

   BODY is a PKL_AST_COMP_STMT node containing the statements that
   conform the function body.

   NFRAMES is a counter used by the parser as an aid in determining
   the number of lexical frames a RETURN_STMT should pop before
   returning from the function.  While parsing, this contains the
   number of frames pushed to the environment at any moment.  After
   parsing, this field is not used anymore.

   NARGS is the number of formal arguments.

   NAME is a C string containing the name used to declare the
   function.

   If the function is a method defined in a struct type, then METHOD_P
   is not 0.  */

#define PKL_AST_FUNC_RET_TYPE(AST) ((AST)->func.ret_type)
#define PKL_AST_FUNC_ARGS(AST) ((AST)->func.args)
#define PKL_AST_FUNC_FIRST_OPT_ARG(AST) ((AST)->func.first_opt_arg)
#define PKL_AST_FUNC_BODY(AST) ((AST)->func.body)
#define PKL_AST_FUNC_NFRAMES(AST) ((AST)->func.nframes)
#define PKL_AST_FUNC_NAME(AST) ((AST)->func.name)
#define PKL_AST_FUNC_NARGS(AST) ((AST)->func.nargs)
#define PKL_AST_FUNC_METHOD_P(AST) ((AST)->func.method_p)

struct pkl_ast_func
{
  struct pkl_ast_common common;

  union pkl_ast_node *ret_type;
  union pkl_ast_node *args;
  union pkl_ast_node *first_opt_arg;
  union pkl_ast_node *body;

  int nargs;
  int nframes;
  char *name;
  int method_p;
};

pkl_ast_node pkl_ast_make_func (pkl_ast ast,
                                pkl_ast_node ret_type,
                                pkl_ast_node args,
                                pkl_ast_node body)
  __attribute__ ((visibility ("hidden")));

int pkl_ast_func_all_optargs (pkl_ast_node type)
  __attribute__ ((visibility ("hidden")));

/* PKL_AST_FUNC_ARG nodes represent a formal argument in a function
   definition.

   TYPE is the type of the argument.

   IDENTIFIER is the name of the argument.  It is a PKL_AST_IDENTIFIER
   node.

   VARARG is 1 if this argument is a vararg.  0  otherwise.

   INITIAL, if not NULL, is an expression providing the default value
   for the function argument.  This expression can refer to previous
   arguments.  */

#define PKL_AST_FUNC_ARG_TYPE(AST) ((AST)->func_arg.type)
#define PKL_AST_FUNC_ARG_IDENTIFIER(AST) ((AST)->func_arg.identifier)
#define PKL_AST_FUNC_ARG_INITIAL(AST) ((AST)->func_arg.initial)
#define PKL_AST_FUNC_ARG_VARARG(AST) ((AST)->func_arg.vararg)

struct pkl_ast_func_arg
{
  struct pkl_ast_common common;

  union pkl_ast_node *type;
  union pkl_ast_node *identifier;
  union pkl_ast_node *initial;
  int vararg;
};

pkl_ast_node pkl_ast_make_func_arg (pkl_ast ast,
                                    pkl_ast_node type,
                                    pkl_ast_node identifier,
                                    pkl_ast_node init)
  __attribute__ ((visibility ("hidden")));

/* PKL_AST_TRIMMER nodes represent a trim of an array, or a string.

   ENTITY is either an array or a string, which is the subject of
   the trim.

   FROM is an expression that should evaluate to an uint<64>, which is
   the index of the first element of the trim.  If FROM is NULL, then
   the index of the first element of the trim is 0.

   TO is an expression that should evaluate to an uint<64>, which is
   the index of the last element of the trim.  If TO is NULL, then the
   indes of the last element of the trim is L-1, where L is the length
   of ENTITY.  */

#define PKL_AST_TRIMMER_ENTITY(AST) ((AST)->trimmer.entity)
#define PKL_AST_TRIMMER_FROM(AST) ((AST)->trimmer.from)
#define PKL_AST_TRIMMER_TO(AST) ((AST)->trimmer.to)

struct pkl_ast_trimmer
{
  struct pkl_ast_common common;

  union pkl_ast_node *entity;
  union pkl_ast_node *from;
  union pkl_ast_node *to;
};

pkl_ast_node pkl_ast_make_trimmer (pkl_ast ast,
                                   pkl_ast_node entity,
                                   pkl_ast_node from,
                                   pkl_ast_node to)
  __attribute__ ((visibility ("hidden")));

/* PKL_AST_INDEXER nodes represent references to an array element.

   BASE must point to a PKL_AST_ARRAY node.

   INDEX must point to an expression whose evaluation is the offset of
   the element into the field, in units of the field's SIZE.  */

#define PKL_AST_INDEXER_ENTITY(AST) ((AST)->indexer.entity)
#define PKL_AST_INDEXER_INDEX(AST) ((AST)->indexer.index)

struct pkl_ast_indexer
{
  struct pkl_ast_common common;
  union pkl_ast_node *entity;
  union pkl_ast_node *index;
};

pkl_ast_node pkl_ast_make_indexer (pkl_ast ast,
                                     pkl_ast_node array,
                                     pkl_ast_node index)
  __attribute__ ((visibility ("hidden")));

/* PKL_AST_STRUCT_REF nodes represent references to a struct
   element.  */

#define PKL_AST_STRUCT_REF_STRUCT(AST) ((AST)->sref.sct)
#define PKL_AST_STRUCT_REF_IDENTIFIER(AST) ((AST)->sref.identifier)

struct pkl_ast_struct_ref
{
  struct pkl_ast_common common;

  union pkl_ast_node *sct;
  union pkl_ast_node *identifier;
};

pkl_ast_node pkl_ast_make_struct_ref (pkl_ast ast,
                                      pkl_ast_node sct,
                                      pkl_ast_node identifier)
  __attribute__ ((visibility ("hidden")));

/* PKL_AST_STRUCT_TYPE_FIELD nodes represent the field part of a
   struct type.

   NAME is a PKL_AST_IDENTIFIER node, or NULL if the struct type
   element has no name.

   TYPE is a PKL_AST_TYPE node.

   CONSTRAINT is a constraint associated with the struct field.  It is
   an expression that should evaluate to a boolean.

   LABEL is an expression that, if present, should evaluate to an
   offset value.  If the struct type element doesn't have a label,
   this is NULL.

   ENDIAN is the endianness to use when reading and writing data
   to/from the field.

   OPTCOND is a boolean expression that, if present, specifies whether
   the field exists in the struct or not.

   INITIALIZER is an expression, that will be used to derive an
   implicit constraint, and also as the initialization value for this
   field when constructing structs.  If no initializer is provided in
   the struct field definition this is NULL.  */

#define PKL_AST_STRUCT_TYPE_FIELD_NAME(AST) ((AST)->sct_type_elem.name)
#define PKL_AST_STRUCT_TYPE_FIELD_TYPE(AST) ((AST)->sct_type_elem.type)
#define PKL_AST_STRUCT_TYPE_FIELD_CONSTRAINT(AST) ((AST)->sct_type_elem.constraint)
#define PKL_AST_STRUCT_TYPE_FIELD_LABEL(AST) ((AST)->sct_type_elem.label)
#define PKL_AST_STRUCT_TYPE_FIELD_ENDIAN(AST) ((AST)->sct_type_elem.endian)
#define PKL_AST_STRUCT_TYPE_FIELD_OPTCOND(AST) ((AST)->sct_type_elem.optcond)
#define PKL_AST_STRUCT_TYPE_FIELD_INITIALIZER(AST) ((AST)->sct_type_elem.initializer)

struct pkl_ast_struct_type_field
{
  struct pkl_ast_common common;

  union pkl_ast_node *name;
  union pkl_ast_node *type;
  union pkl_ast_node *constraint;
  union pkl_ast_node *initializer;
  union pkl_ast_node *label;
  union pkl_ast_node *optcond;
  int endian;
};

pkl_ast_node pkl_ast_make_struct_type_field (pkl_ast ast,
                                             pkl_ast_node name,
                                             pkl_ast_node type,
                                             pkl_ast_node constraint,
                                             pkl_ast_node initializer,
                                             pkl_ast_node label,
                                             int endian,
                                             pkl_ast_node optcond)
   __attribute__ ((visibility ("hidden")));

/* PKL_AST_FUNC_TYPE_ARG nodes represent the arguments part of a
   function type.

   TYPE is a PKL_AST_TYPE node describing the type of the
   argument.

   NAME, if not NULL, is an IDENTIFIER node describing the name
   of the argument.

   OPTIONAL is 1 if the argument is optional.  0 otherwise.
   VARARG is 1 if the argument is a vararg.  0 otherwise.  */

#define PKL_AST_FUNC_TYPE_ARG_TYPE(AST) ((AST)->fun_type_arg.type)
#define PKL_AST_FUNC_TYPE_ARG_NAME(AST) ((AST)->fun_type_arg.name)
#define PKL_AST_FUNC_TYPE_ARG_OPTIONAL(AST) ((AST)->fun_type_arg.optional)
#define PKL_AST_FUNC_TYPE_ARG_VARARG(AST) ((AST)->fun_type_arg.vararg)

struct pkl_ast_func_type_arg
{
  struct pkl_ast_common common;
  union pkl_ast_node *type;
  union pkl_ast_node *name;
  int optional;
  int vararg;
};

pkl_ast_node pkl_ast_make_func_type_arg (pkl_ast ast,
                                         pkl_ast_node type, pkl_ast_node name)
  __attribute__ ((visibility ("hidden")));

/* PKL_AST_TYPE nodes represent type expressions.

   If NAME is not NULL, then this specific type instance has a given
   name, which is encoded in a PKL_AST_IDENTIFIER node.

   CODE contains the kind of type, as defined in the pkl_ast_type_code
   enumeration above.

   COMPILED is 0 if the type has not been compiled yet.  1 otherwise.
   This is used to avoid unneccessary work in the compiler.

   In integral types, SIGNED is 1 if the type denotes a signed numeric
   type.  In non-integral types SIGNED is 0.  SIZE is the size in bits
   of type.

   In array types, ETYPE is a PKL_AST_TYPE node reflecting the type of
   the elements stored in the array.  If the array type is bounded by
   number of elements, then BOUND is an expression that must evaluate
   to an integer.  If the array type is bounded by size, then BOUND is
   an expression that must evaluate to an offset.  If the array type
   is unbounded, then BOUND is NULL.  MAPPER, WRITER, CONSTRUCTOR and
   BOUNDCLS are used to hold closures, or PVM_NULL. The field
   LEX_CORRECTED is used by the transl phase, to keep record of array
   types that have been lexically corrected; this is to avoid
   processing the same type more than once.

   In struct types, NELEM is the number of elements in the struct
   type, NFIELD is the number of fields, and NDECL is the number of
   declarations.  ELEMS is a chain of elements, which can be
   PKL_AST_STRUCT_TYPE_FIELD or PKL_AST_DECL nodes, potentially mixed.
   PINNED is 1 if the struct is pinned, 0 otherwise.  MAPPER, WRITER
   CONSTRUCTOR and COMPARATOR are used to hold closures, or PVM_NULL.

   In offset types, BASE_TYPE is a PKL_AST_TYPE with the base type for
   the offset's magnitude, and UNIT is either a PKL_AST_IDENTIFIER
   containing one of few recognized keywords (b, B, Kb, etc) or a
   PKL_AST_TYPE.

   In function types, NARG is the number of formal arguments in the
   function type.  ARGS is a chain of PKL_AST_FUNC_TYPE_ARG nodes.
   RTYPE is the type of the returned value, or NULL if the function
   type denotes a void function.  FIRST_OPT_ARG is the first argument
   (in ARGS) that has an associated initial.  If no argument in ARGS
   has an associated initial, this is NULL.  VARARG is 1 if the
   function takes a variable number of arguments.  0 otherwise.

   When the size of a value of a given type can be determined at
   compile time, we say that such type is "complete".  Otherwise, we
   say that the type is "incomplete" and should be completed at
   run-time.  */

#define PKL_AST_TYPE_CODE(AST) ((AST)->type.code)
#define PKL_AST_TYPE_NAME(AST) ((AST)->type.name)
#define PKL_AST_TYPE_COMPLETE(AST) ((AST)->type.complete)
#define PKL_AST_TYPE_COMPILED(AST) ((AST)->type.compiled)
#define PKL_AST_TYPE_I_SIZE(AST) ((AST)->type.val.integral.size)
#define PKL_AST_TYPE_I_SIGNED(AST) ((AST)->type.val.integral.signed_p)
#define PKL_AST_TYPE_A_BOUND(AST) ((AST)->type.val.array.bound)
#define PKL_AST_TYPE_A_ETYPE(AST) ((AST)->type.val.array.etype)
#define PKL_AST_TYPE_A_MAPPER(AST) ((AST)->type.val.array.mapper)
#define PKL_AST_TYPE_A_WRITER(AST) ((AST)->type.val.array.writer)
#define PKL_AST_TYPE_A_BOUNDER(AST) ((AST)->type.val.array.bounder)
#define PKL_AST_TYPE_A_CONSTRUCTOR(AST) ((AST)->type.val.array.constructor)
#define PKL_AST_TYPE_S_NFIELD(AST) ((AST)->type.val.sct.nfield)
#define PKL_AST_TYPE_S_NDECL(AST) ((AST)->type.val.sct.ndecl)
#define PKL_AST_TYPE_S_NELEM(AST) ((AST)->type.val.sct.nelem)
#define PKL_AST_TYPE_S_ELEMS(AST) ((AST)->type.val.sct.elems)
#define PKL_AST_TYPE_S_PINNED(AST) ((AST)->type.val.sct.pinned)
#define PKL_AST_TYPE_S_UNION(AST) ((AST)->type.val.sct.union_p)
#define PKL_AST_TYPE_S_MAPPER(AST) ((AST)->type.val.sct.mapper)
#define PKL_AST_TYPE_S_WRITER(AST) ((AST)->type.val.sct.writer)
#define PKL_AST_TYPE_S_CONSTRUCTOR(AST) ((AST)->type.val.sct.constructor)
#define PKL_AST_TYPE_S_COMPARATOR(AST) ((AST)->type.val.sct.comparator)
#define PKL_AST_TYPE_O_UNIT(AST) ((AST)->type.val.off.unit)
#define PKL_AST_TYPE_O_BASE_TYPE(AST) ((AST)->type.val.off.base_type)
#define PKL_AST_TYPE_F_RTYPE(AST) ((AST)->type.val.fun.rtype)
#define PKL_AST_TYPE_F_NARG(AST) ((AST)->type.val.fun.narg)
#define PKL_AST_TYPE_F_ARGS(AST) ((AST)->type.val.fun.args)
#define PKL_AST_TYPE_F_VARARG(AST) ((AST)->type.val.fun.vararg)
#define PKL_AST_TYPE_F_FIRST_OPT_ARG(AST) ((AST)->type.val.fun.first_opt_arg)

#define PKL_AST_TYPE_COMPLETE_UNKNOWN 0
#define PKL_AST_TYPE_COMPLETE_YES 1
#define PKL_AST_TYPE_COMPLETE_NO 2

struct pkl_ast_type
{
  struct pkl_ast_common common;

  union pkl_ast_node *name;
  enum pkl_ast_type_code code;
  int complete;
  int compiled;

  union
  {
    struct
    {
      size_t size;
      int signed_p;
    } integral;

    struct
    {
      union pkl_ast_node *bound;
      union pkl_ast_node *etype;
      pvm_val mapper;
      pvm_val writer;
      pvm_val bounder;
      pvm_val constructor;
    } array;

    struct
    {
      size_t nelem;
      size_t nfield;
      size_t ndecl;
      union pkl_ast_node *elems;
      int pinned;
      int union_p;
      pvm_val mapper;
      pvm_val writer;
      pvm_val constructor;
      pvm_val comparator;
    } sct;

    struct
    {
      union pkl_ast_node *unit;
      union pkl_ast_node *base_type;
    } off;

    struct
    {
      union pkl_ast_node *rtype;
      int narg;
      int vararg;
      union pkl_ast_node *args;
      union pkl_ast_node *first_opt_arg;
    } fun;

  } val;
};

pkl_ast_node pkl_ast_make_named_type (pkl_ast ast, pkl_ast_node name)
  __attribute__ ((visibility ("hidden")));

pkl_ast_node pkl_ast_make_integral_type (pkl_ast ast, size_t size, int signed_p)
  __attribute__ ((visibility ("hidden")));

pkl_ast_node pkl_ast_make_void_type (pkl_ast ast)
  __attribute__ ((visibility ("hidden")));

pkl_ast_node pkl_ast_make_string_type (pkl_ast ast)
  __attribute__ ((visibility ("hidden")));

pkl_ast_node pkl_ast_make_array_type (pkl_ast ast, pkl_ast_node etype,
                                      pkl_ast_node bound)
  __attribute__ ((visibility ("hidden")));

pkl_ast_node pkl_ast_make_struct_type (pkl_ast ast, size_t nelem, size_t nfield,
                                       size_t ndecl, pkl_ast_node elems,
                                       int pinned, int union_p)
  __attribute__ ((visibility ("hidden")));

pkl_ast_node pkl_ast_make_offset_type (pkl_ast ast, pkl_ast_node base_type,
                                       pkl_ast_node unit)
  __attribute__ ((visibility ("hidden")));

pkl_ast_node pkl_ast_make_function_type (pkl_ast ast, pkl_ast_node rtype,
                                         size_t narg, pkl_ast_node args)
  __attribute__ ((visibility ("hidden")));

pkl_ast_node pkl_ast_make_any_type (pkl_ast)
  __attribute__ ((visibility ("hidden")));

pkl_ast_node pkl_ast_dup_type (pkl_ast_node type)
  __attribute__ ((visibility ("hidden")));

int pkl_ast_type_equal (pkl_ast_node t1, pkl_ast_node t2)
  __attribute__ ((visibility ("hidden")));

int pkl_ast_type_mappable_p (pkl_ast_node type)
  __attribute__ ((visibility ("hidden")));

int pkl_ast_type_is_exception (pkl_ast_node type)
  __attribute__ ((visibility ("hidden")));

int pkl_ast_type_promoteable (pkl_ast_node ft, pkl_ast_node tt,
                              int promote_array_of_any)
  __attribute__ ((visibility ("hidden")));

pkl_ast_node pkl_ast_sizeof_type (pkl_ast ast, pkl_ast_node type)
  __attribute__ ((visibility ("hidden")));

int pkl_ast_type_is_complete (pkl_ast_node type)
  __attribute__ ((visibility ("hidden")));

void pkl_ast_array_type_remove_bounders (pkl_ast_node type)
  __attribute__ ((visibility ("hidden")));

void pkl_print_type (FILE *out, pkl_ast_node type, int use_given_name)
  __attribute__ ((visibility ("hidden")));

char *pkl_type_str (pkl_ast_node type, int use_given_name)
  __attribute__ ((visibility ("hidden")));

pkl_ast_node pkl_struct_type_traverse (pkl_ast_node type, const char *path)
  __attribute__ ((visibility ("hidden")));

/* PKL_AST_DECL nodes represent the declaration of a named entity:
   function, type, variable....

   KIND allows to quickly identify the nature of the entity being
   declared: PKL_AST_DECL_KIND_VAR, PKL_AST_DECL_KIND_TYPE,
   PKL_AST_DECL_KIND_FUNC or PKL_AST_DECL_KIND_UNIT.

   NAME is PKL_AST_IDENTIFIER node containing the name in the
   association.

   INITIAL is the initial value of the entity.  The kind of node
   depends on what is being declared:
   - An expression node for a variable.
   - A PKL_AST_TYPE for a type.
   - A PKL_AST_FUNC for a function.
   - A constant expression node for an unit.

   ORDER is the order of the declaration in its containing
   compile-time environment.  It is filled up when the declaration is
   registered in an environment.

   SOURCE is a string describing where the declaration comes from.
   Usually it will be the name of a source file, or "<stdin>" or
   whatever.

   STRUCT_FIELD_P indicates whether this declaration is for a variable
   corresponding to a struct field.  */

#define PKL_AST_DECL_KIND(AST) ((AST)->decl.kind)
#define PKL_AST_DECL_NAME(AST) ((AST)->decl.name)
#define PKL_AST_DECL_TYPE(AST) ((AST)->decl.type)
#define PKL_AST_DECL_INITIAL(AST) ((AST)->decl.initial)
#define PKL_AST_DECL_ORDER(AST) ((AST)->decl.order)
#define PKL_AST_DECL_SOURCE(AST) ((AST)->decl.source)
#define PKL_AST_DECL_STRUCT_FIELD_P(AST) ((AST)->decl.struct_field_p)
#define PKL_AST_DECL_IN_STRUCT_P(AST) ((AST)->decl.in_struct_p)

#define PKL_AST_DECL_KIND_ANY 0
#define PKL_AST_DECL_KIND_VAR 1
#define PKL_AST_DECL_KIND_TYPE 2
#define PKL_AST_DECL_KIND_FUNC 3
#define PKL_AST_DECL_KIND_UNIT 4

struct pkl_ast_decl
{
  struct pkl_ast_common common;

  int kind;
  int struct_field_p;
  int in_struct_p;
  char *source;
  union pkl_ast_node *name;
  union pkl_ast_node *initial;
  int order;
};

pkl_ast_node pkl_ast_make_decl (pkl_ast ast, int kind,
                                pkl_ast_node name, pkl_ast_node initial,
                                const char *source)
  __attribute__ ((visibility ("hidden")));

/* PKL_AST_OFFSET nodes represent poke object constructions.

   MAGNITUDE is an integer expression.
   UNIT can be an IDENTIFIER, an INTEGER or a TYPE.  */

#define PKL_AST_OFFSET_MAGNITUDE(AST) ((AST)->offset.magnitude)
#define PKL_AST_OFFSET_UNIT(AST) ((AST)->offset.unit)

#define PKL_AST_OFFSET_UNIT_BITS 1
#define PKL_AST_OFFSET_UNIT_NIBBLES 4
#define PKL_AST_OFFSET_UNIT_BYTES (2 * PKL_AST_OFFSET_UNIT_NIBBLES)
#define PKL_AST_OFFSET_UNIT_KILOBITS (1024 * PKL_AST_OFFSET_UNIT_BITS)
#define PKL_AST_OFFSET_UNIT_KILOBYTES (1024 * PKL_AST_OFFSET_UNIT_BYTES)
#define PKL_AST_OFFSET_UNIT_MEGABITS (1024 * PKL_AST_OFFSET_UNIT_KILOBITS)
#define PKL_AST_OFFSET_UNIT_MEGABYTES (1024 * PKL_AST_OFFSET_UNIT_KILOBYTES)
#define PKL_AST_OFFSET_UNIT_GIGABITS (1024 * PKL_AST_OFFSET_UNIT_MEGABITS)

struct pkl_ast_offset
{
  struct pkl_ast_common common;

  union pkl_ast_node *magnitude;
  union pkl_ast_node *unit;
};

pkl_ast_node pkl_ast_make_offset (pkl_ast ast,
                                  pkl_ast_node magnitude,
                                  pkl_ast_node unit)
  __attribute__ ((visibility ("hidden")));

/* PKL_AST_CAST nodes represent casts at the language level.

   TYPE is the target type in the case.
   EXP is the expression whole value should be casted to the targe
   type.  */

#define PKL_AST_CAST_TYPE(AST) ((AST)->cast.type)
#define PKL_AST_CAST_EXP(AST) ((AST)->cast.exp)

struct pkl_ast_cast
{
  struct pkl_ast_common common;

  union pkl_ast_node *type;
  union pkl_ast_node *exp;
};

pkl_ast_node pkl_ast_make_cast (pkl_ast ast,
                                pkl_ast_node type,
                                pkl_ast_node exp)
  __attribute__ ((visibility ("hidden")));

/* PKL_AST_ISA nodes represent an application of the ISA operator.

   EXP is the expression whose's type is checked.
   TYPE is the type to check for.  */

#define PKL_AST_ISA_TYPE(AST) ((AST)->isa.type)
#define PKL_AST_ISA_EXP(AST) ((AST)->isa.exp)

struct pkl_ast_isa
{
  struct pkl_ast_common common;

  union pkl_ast_node *type;
  union pkl_ast_node *exp;
};

pkl_ast_node pkl_ast_make_isa (pkl_ast ast,
                               pkl_ast_node type,
                               pkl_ast_node exp)
  __attribute__ ((visibility ("hidden")));

/* PKL_AST_MAP nodes represent maps, i.e. the mapping of some type at
   some offset in IO space.

   TYPE is the mapped type.

   IOS is an expression that should evaluate to an integer, and
   identifies the IOS where the map operation is performed.  If it is
   NULL then the map is performed in the current IOS.

   OFFSET is the offset in IO space where the TYPE is mapped.  */

#define PKL_AST_MAP_TYPE(AST) ((AST)->map.type)
#define PKL_AST_MAP_IOS(AST) ((AST)->map.ios)
#define PKL_AST_MAP_OFFSET(AST) ((AST)->map.offset)

struct pkl_ast_map
{
  struct pkl_ast_common common;

  union pkl_ast_node *type;
  union pkl_ast_node *offset;
  union pkl_ast_node *ios;
};

pkl_ast_node pkl_ast_make_map (pkl_ast ast,
                               pkl_ast_node type,
                               pkl_ast_node ios,
                               pkl_ast_node offset)
  __attribute__ ((visibility ("hidden")));

/* PKL_AST_SCONS nodes represent struct constructors.

   TYPE is a struct type.

   VALUE is a struct value.  */

#define PKL_AST_SCONS_TYPE(AST) ((AST)->scons.type)
#define PKL_AST_SCONS_VALUE(AST) ((AST)->scons.value)

struct pkl_ast_scons
{
  struct pkl_ast_common common;

  union pkl_ast_node *type;
  union pkl_ast_node *value;
};

pkl_ast_node pkl_ast_make_scons (pkl_ast ast,
                                 pkl_ast_node type,
                                 pkl_ast_node value)
  __attribute__ ((visibility ("hidden")));

/* PKL_AST_FUNCALL nodes represent the invocation of a function.

   FUNCTION is a variable with the function being invoked.
   ARGS is a chain of PKL_AST_FUNCALL_ARG nodes.

   NARG is the number of arguments in the funcall.  */

#define PKL_AST_FUNCALL_ARGS(AST) ((AST)->funcall.args)
#define PKL_AST_FUNCALL_FUNCTION(AST) ((AST)->funcall.function)
#define PKL_AST_FUNCALL_NARG(AST) ((AST)->funcall.narg)

struct pkl_ast_funcall
{
  struct pkl_ast_common common;

  int narg;
  union pkl_ast_node *function;
  union pkl_ast_node *args;
};

pkl_ast_node pkl_ast_make_funcall (pkl_ast ast,
                                   pkl_ast_node function,
                                   pkl_ast_node args)
  __attribute__ ((visibility ("hidden")));

/* PKL_AST_FUNCALL_ARG nodes represent actual arguments in function
   calls.

   EXP is the value passed for the argument.  Note that this can be
   NULL for a placeholder for a missing actual to an optional formal
   argument.

   NAME, if not NULL, is an IDENTIFIER node with the name of the
   argument.

   FIRST_VARARG is 1 if this actual corresponds to the first
   vararg.  0 otherwise.  */

#define PKL_AST_FUNCALL_ARG_EXP(AST) ((AST)->funcall_arg.exp)
#define PKL_AST_FUNCALL_ARG_NAME(AST) ((AST)->funcall_arg.name)
#define PKL_AST_FUNCALL_ARG_FIRST_VARARG(AST) ((AST)->funcall_arg.first_vararg)

struct pkl_ast_funcall_arg
{
  struct pkl_ast_common common;

  union pkl_ast_node *exp;
  union pkl_ast_node *name;
  int first_vararg;
};

pkl_ast_node pkl_ast_make_funcall_arg (pkl_ast ast, pkl_ast_node exp,
                                       pkl_ast_node name)
  __attribute__ ((visibility ("hidden")));

/* PKL_AST_VAR nodes represent variable references.

   NAME is a PKL_AST_IDENTIFIER containing the name used to refer to
   the variable.

   DECL is the declaration that declared the variable.

   BACK is the number of compile-time environment frames to traverse
   in order to find the frame where the referred variable is declared.

   OVER is the position in the frame, i.e. the variable declaration is
   the OVERth variable declaration in the frame.

   IS_RECURSIVE is a boolean indicating whether the variable
   references the declaration of the containing function.

   FUNCTION is the function immediately enclosing the variable
   reference, or NULL.

   FUNCTION_BACK is the lexical depth relative to the immediately
   enclosing function.  This field is meaningful only if FUNCTION is
   not NULL.  */

#define PKL_AST_VAR_NAME(AST) ((AST)->var.name)
#define PKL_AST_VAR_DECL(AST) ((AST)->var.decl)
#define PKL_AST_VAR_BACK(AST) ((AST)->var.back)
#define PKL_AST_VAR_OVER(AST) ((AST)->var.over)
#define PKL_AST_VAR_IS_RECURSIVE(AST) ((AST)->var.is_recursive)
#define PKL_AST_VAR_FUNCTION(AST) ((AST)->var.function)
#define PKL_AST_VAR_FUNCTION_BACK(AST) ((AST)->var.function_back)

struct pkl_ast_var
{
  struct pkl_ast_common common;

  union pkl_ast_node *name;
  union pkl_ast_node *decl;
  int back;
  int over;
  int is_recursive;

  union pkl_ast_node *function;
  int function_back;
};

pkl_ast_node pkl_ast_make_var (pkl_ast ast,
                               pkl_ast_node name,
                               pkl_ast_node initial,
                               int back, int over)
  __attribute__ ((visibility ("hidden")));

/* PKL_AST_COMPOUND_STMT nodes represent compound statements in the
   language.

   STMTS is a possibly empty chain of statements.

   If BUILTIN is not PKL_AST_BUILTIN_NONE, then this compound
   statement is a compiler builtin, i.e. specific code will be
   generated for this node.  In this case, STMTS should be NULL.  */

#define PKL_AST_COMP_STMT_STMTS(AST) ((AST)->comp_stmt.stmts)
#define PKL_AST_COMP_STMT_BUILTIN(AST) ((AST)->comp_stmt.builtin)
#define PKL_AST_COMP_STMT_NUMVARS(AST) ((AST)->comp_stmt.numvars)

#define PKL_AST_BUILTIN_NONE 0
#define PKL_AST_BUILTIN_PRINT 1
#define PKL_AST_BUILTIN_RAND 2
#define PKL_AST_BUILTIN_GET_ENDIAN 3
#define PKL_AST_BUILTIN_SET_ENDIAN 4
#define PKL_AST_BUILTIN_GET_IOS 5
#define PKL_AST_BUILTIN_SET_IOS 6
#define PKL_AST_BUILTIN_OPEN 7
#define PKL_AST_BUILTIN_CLOSE 8
#define PKL_AST_BUILTIN_IOSIZE 9
#define PKL_AST_BUILTIN_GETENV 10

struct pkl_ast_comp_stmt
{
  struct pkl_ast_common common;

  union pkl_ast_node *stmts;
  int builtin;
  int numvars;
};

pkl_ast_node pkl_ast_make_comp_stmt (pkl_ast ast, pkl_ast_node stmts)
  __attribute__ ((visibility ("hidden")));

pkl_ast_node pkl_ast_make_builtin (pkl_ast ast, int builtin)
  __attribute__ ((visibility ("hidden")));

/* PKL_AST_NULL_STMT nodes represent the "null statement".  It can
   appear anywhere an statement is expected, but it has no effect.  */

struct pkl_ast_null_stmt
{
  struct pkl_ast_common common;
};

pkl_ast_node pkl_ast_make_null_stmt (pkl_ast ast)
  __attribute__ ((visibility ("hidden")));

/* PKL_AST_ASS_STMT nodes represent assignment statements in the
   language.

   LVALUE is the l-value of the assignment.
   EXP is the r-value of the assignment.

   If the l-value is of a map-able type, VALMAPPER_BACK and
   VALMAPPER_OVER constitute the lexical address of a value-mapper
   function, or -1.  */

#define PKL_AST_ASS_STMT_LVALUE(AST) ((AST)->ass_stmt.lvalue)
#define PKL_AST_ASS_STMT_EXP(AST) ((AST)->ass_stmt.exp)
#define PKL_AST_ASS_STMT_VALMAPPER_BACK(AST) ((AST)->ass_stmt.valmapper_back)
#define PKL_AST_ASS_STMT_VALMAPPER_OVER(AST) ((AST)->ass_stmt.valmapper_over)
#define PKL_AST_ASS_STMT_VALMAPPER_P(AST)                               \
  ((AST)->ass_stmt.valmapper_back != -1 && (AST)->ass_stmt.valmapper_over != -1)

struct pkl_ast_ass_stmt
{
  struct pkl_ast_common common;

  union pkl_ast_node *lvalue;
  union pkl_ast_node *exp;
  int valmapper_back;
  int valmapper_over;
};

pkl_ast_node pkl_ast_make_ass_stmt (pkl_ast ast,
                                    pkl_ast_node lvalue, pkl_ast_node exp)
  __attribute__ ((visibility ("hidden")));

/* PKL_AST_IF_STMT nodes represent conditional statements, with an
   optional `else' part.

   EXP is the expression that is to be evaluated to decide what branch
   to take.

   THEN_STMT is the statement to be executed when EXP holds true.
   ELSE_STMT is the statement to be executed when EXP holds false.  */

#define PKL_AST_IF_STMT_EXP(AST) ((AST)->if_stmt.exp)
#define PKL_AST_IF_STMT_THEN_STMT(AST) ((AST)->if_stmt.then_stmt)
#define PKL_AST_IF_STMT_ELSE_STMT(AST) ((AST)->if_stmt.else_stmt)

struct pkl_ast_if_stmt
{
  struct pkl_ast_common common;

  union pkl_ast_node *exp;
  union pkl_ast_node *then_stmt;
  union pkl_ast_node *else_stmt;
};

pkl_ast_node pkl_ast_make_if_stmt (pkl_ast ast,
                                   pkl_ast_node exp,
                                   pkl_ast_node then_stmt,
                                   pkl_ast_node else_stmt)
  __attribute__ ((visibility ("hidden")));

int pkl_ast_lvalue_p (pkl_ast_node node)
  __attribute__ ((visibility ("hidden")));

/* PKL_AST_LOOP_STMT nodes represent iterative statements.

   ITERATOR is a PKL_AST_LOOP_STMT_ITERATOR node, or NULL.

   CONDITION is an expression that should evaluate to a boolean, that
   is evaluated at the beginning of the loop.  If it evals to false,
   the loop is exited.  This is used in WHILE, FOR-IN and FOR-IN-WHERE
   loops.  In loops with an iterator, the iterator variable is
   available in the scope where CONDITION is evaluated, and CONDITION
   determines whether BODY is executed in the current iteration.

   BODY is a statement, which is the body of the loop.  */

#define PKL_AST_LOOP_STMT_ITERATOR(AST) ((AST)->loop_stmt.iterator)
#define PKL_AST_LOOP_STMT_CONDITION(AST) ((AST)->loop_stmt.condition)
#define PKL_AST_LOOP_STMT_BODY(AST) ((AST)->loop_stmt.body)

struct pkl_ast_loop_stmt
{
  struct pkl_ast_common COMMON;

  union pkl_ast_node *iterator;
  union pkl_ast_node *condition;
  union pkl_ast_node *body;
};

pkl_ast_node pkl_ast_make_loop_stmt (pkl_ast ast,
                                     pkl_ast_node iterator,
                                     pkl_ast_node condition,
                                     pkl_ast_node body)
  __attribute__ ((visibility ("hidden")));

/* PKL_AST_LOOP_STMT_ITERATOR nodes represent an iterator in a loop
   statement.

   DECL is a declaration for a variable created in a new lexical
   scope.

   CONTAINER is an expression that evaluates to an array.  This is
   used in FOR-IN and FOR-IN-HWERE loops.  */

#define PKL_AST_LOOP_STMT_ITERATOR_DECL(AST) ((AST)->loop_stmt_iterator.decl)
#define PKL_AST_LOOP_STMT_ITERATOR_CONTAINER(AST) ((AST)->loop_stmt_iterator.container)

struct pkl_ast_loop_stmt_iterator
{
  struct pkl_ast_common COMMON;

  union pkl_ast_node *decl;
  union pkl_ast_node *container;
};

pkl_ast_node pkl_ast_make_loop_stmt_iterator (pkl_ast ast,
                                              pkl_ast_node decl,
                                              pkl_ast_node container)
  __attribute__ ((visibility ("hidden")));

/* PKL_AST_RETURN_STMT nodes represent return statements.

   EXP is the expression to return to the caller.

   NFRAMES is the number of lexical frames to pop before returning
   from the function.  This is used by the code generator.

   NDROPS is the number of stack elements to drop before returning
   from the function.

   FUNCTION is the PKL_AST_FUNCTION containing this return
   statement. */

#define PKL_AST_RETURN_STMT_EXP(AST) ((AST)->return_stmt.exp)
#define PKL_AST_RETURN_STMT_NFRAMES(AST) ((AST)->return_stmt.nframes)
#define PKL_AST_RETURN_STMT_NDROPS(AST) ((AST)->return_stmt.ndrops)
#define PKL_AST_RETURN_STMT_FUNCTION(AST) ((AST)->return_stmt.function)

struct pkl_ast_return_stmt
{
  struct pkl_ast_common common;

  union pkl_ast_node *exp;
  union pkl_ast_node *function;
  int nframes;
  int ndrops;
};

pkl_ast_node pkl_ast_make_return_stmt (pkl_ast ast, pkl_ast_node exp)
  __attribute__ ((visibility ("hidden")));

void pkl_ast_finish_returns (pkl_ast_node function)
  __attribute__ ((visibility ("hidden")));

/* PKL_AST_EXP_STMT nodes represent "expression statements".

   EXP is the expression conforming the statement.  */

#define PKL_AST_EXP_STMT_EXP(AST) ((AST)->exp_stmt.exp)

struct pkl_ast_exp_stmt
{
  struct pkl_ast_common common;
  union pkl_ast_node *exp;
};

pkl_ast_node pkl_ast_make_exp_stmt (pkl_ast ast, pkl_ast_node exp)
  __attribute__ ((visibility ("hidden")));

/* PKL_AST_TRY_UNTIL_STMT nodes represent try-until statements.

   CODE is a statement.

   EXP is an expression that should evaluate to a 32-bit signed
   integer.  CODE will be executed repeteadly until the given
   exception type is catched, or some other exception is raised.  */

#define PKL_AST_TRY_UNTIL_STMT_CODE(AST) ((AST)->try_until_stmt.code)
#define PKL_AST_TRY_UNTIL_STMT_EXP(AST) ((AST)->try_until_stmt.exp)

struct pkl_ast_try_until_stmt
{
  struct pkl_ast_common common;

  union pkl_ast_node *code;
  union pkl_ast_node *exp;
};

pkl_ast_node pkl_ast_make_try_until_stmt (pkl_ast ast,
                                          pkl_ast_node code, pkl_ast_node exp)
  __attribute__ ((visibility ("hidden")));

/* PKL_AST_TRY_CATCH_STMT nodes represent try-catch statements, which
   are used in order to support exception handlers.

   CODE is a statement that is executed.

   HANDLER is a statement that will be executed in case an exception
   is raised while executing CODE.

   TYPE, if specified, is the argument to the catch clause.  The type
   of the argument must be an Exception.

   EXP, if specified, is an expression evaluating to an Exception.
   Exceptions having any other type won't be catched by the `catch'
   clause of the statement.

   Note that TYPE and EXP are mutually exclusive.  */

#define PKL_AST_TRY_CATCH_STMT_CODE(AST) ((AST)->try_catch_stmt.code)
#define PKL_AST_TRY_CATCH_STMT_HANDLER(AST) ((AST)->try_catch_stmt.handler)
#define PKL_AST_TRY_CATCH_STMT_ARG(AST) ((AST)->try_catch_stmt.arg)
#define PKL_AST_TRY_CATCH_STMT_EXP(AST) ((AST)->try_catch_stmt.exp)

struct pkl_ast_try_catch_stmt
{
  struct pkl_ast_common common;

  union pkl_ast_node *code;
  union pkl_ast_node *handler;
  union pkl_ast_node *arg;
  union pkl_ast_node *exp;
};

pkl_ast_node pkl_ast_make_try_catch_stmt (pkl_ast ast,
                                          pkl_ast_node code, pkl_ast_node handler,
                                          pkl_ast_node arg, pkl_ast_node exp)
  __attribute__ ((visibility ("hidden")));

/* PKL_AST_PRINT_STMT nodes represent `print' statements.

   ARGS is a chained list of PKL_AST_PRINT_STMT_ARG nodes.  In `print'
   statements, this only contains one argument, whose expression
   should be of type string.  In `printf' statements, this may contain
   zero or more nodes, and the types of their expressions should match
   teh format expressed in FMT.

   FMT, if not NULL, is a format string node.

   TYPES is a linked list of type nodes, corresponding to the %-
   directives in FMT.  Note that %<class> and %</class> directives
   have type PKL_TYPE_VOID.

   NARGS is the number of arguments in ARGS.

   PREFIX, if not NULL, is a C string that should be printed before
   the arguments.

   FMT_PROCESSED_P is a boolean indicating whether the format string
   has been already processed in this node.  */

#define PKL_AST_PRINT_STMT_FMT(AST) ((AST)->print_stmt.fmt)
#define PKL_AST_PRINT_STMT_TYPES(AST) ((AST)->print_stmt.types)
#define PKL_AST_PRINT_STMT_ARGS(AST) ((AST)->print_stmt.args)
#define PKL_AST_PRINT_STMT_NARGS(AST) ((AST)->print_stmt.nargs)
#define PKL_AST_PRINT_STMT_PREFIX(AST) ((AST)->print_stmt.prefix)
#define PKL_AST_PRINT_STMT_FMT_PROCESSED_P(AST) ((AST)->print_stmt.fmt_processed_p)

struct pkl_ast_print_stmt
{
  struct pkl_ast_common common;

  int nargs;
  char *prefix;
  int fmt_processed_p;
  union pkl_ast_node *fmt;
  union pkl_ast_node *types;
  union pkl_ast_node *args;
};

pkl_ast_node pkl_ast_make_print_stmt (pkl_ast ast,
                                      pkl_ast_node fmt, pkl_ast_node args)
  __attribute__ ((visibility ("hidden")));

/* PKL_AST_PRINT_STMT_ARG nodes represent expression arguments to
   `printf' statements.

   EXP is an expression node evaluating to the value to print.

   BASE is the numeration base to use when printing this argument.

   BEGIN_SC, if not NULL, marks that this argument is a %<class>
   directive, and is a NULL-terminated string with the name of the
   styling class to begin.

   END_SC, if not NULL, marks that this argument is a %</class>
   directive, and is a NULL-terminated string with the name of the
   styling class to end.

   SUFFIX, if not NULL, is a C string that should be printed after the
   value of EXP, respectively.

   VALUE_P indicates whether the argument shall be printed as a PVM
   value or not.

   PRINT_MODE and PRINT_DEPTH specify how the argument shall be
   printed if VALUE_P is true.  PRINT_MODE can be one of the
   PKL_AST_PRINT_MODE_* constants defined below, while PRINT_DEPTH can
   be zero or a positive integer.  */

#define PKL_AST_PRINT_STMT_ARG_EXP(AST) ((AST)->print_stmt_arg.exp)
#define PKL_AST_PRINT_STMT_ARG_BASE(AST) ((AST)->print_stmt_arg.base)
#define PKL_AST_PRINT_STMT_ARG_SUFFIX(AST) ((AST)->print_stmt_arg.suffix)
#define PKL_AST_PRINT_STMT_ARG_BEGIN_SC(AST) ((AST)->print_stmt_arg.begin_sc)
#define PKL_AST_PRINT_STMT_ARG_END_SC(AST) ((AST)->print_stmt_arg.end_sc)
#define PKL_AST_PRINT_STMT_ARG_VALUE_P(AST) ((AST)->print_stmt_arg.value_p)
#define PKL_AST_PRINT_STMT_ARG_PRINT_MODE(AST) ((AST)->print_stmt_arg.print_mode)
#define PKL_AST_PRINT_STMT_ARG_PRINT_DEPTH(AST) ((AST)->print_stmt_arg.print_depth)

#define PKL_AST_PRINT_MODE_FLAT 0
#define PKL_AST_PRINT_MODE_TREE 1

struct pkl_ast_print_stmt_arg
{
  struct pkl_ast_common common;

  char *begin_sc;
  char *end_sc;
  int base;
  int value_p;
  int print_mode;
  int print_depth;
  char *suffix;
  union pkl_ast_node *exp;
};

pkl_ast_node pkl_ast_make_print_stmt_arg (pkl_ast ast, pkl_ast_node exp)
  __attribute__ ((visibility ("hidden")));

/* PKL_AST_BREAK_STMT nodes represent `break' statements.  Each break
   statement is associated to a loop or switch node.

   ENTITY is the loop or switch node associated with this break
   statement.

   NFRAMES is the lexical depth of the break statement, relative to
   the enclosing entity.  */

#define PKL_AST_BREAK_STMT_ENTITY(AST) ((AST)->break_stmt.entity)
#define PKL_AST_BREAK_STMT_NFRAMES(AST) ((AST)->break_stmt.nframes)

struct pkl_ast_break_stmt
{
  struct pkl_ast_common common;
  union pkl_ast_node *entity;
  int nframes;
};

pkl_ast_node pkl_ast_make_break_stmt (pkl_ast ast)
  __attribute__ ((visibility ("hidden")));

void pkl_ast_finish_breaks (pkl_ast_node entity, pkl_ast_node stmt)
  __attribute__ ((visibility ("hidden")));

/* PKL_AST_RAISE_STMT nodes represent `raise' statements, which are
   used in order to raise exceptions at the program level.

   EXP is an expression that should evaluate to an exception object.
   At the present time, this is simply a 32-bit integer.  */

#define PKL_AST_RAISE_STMT_EXP(AST) ((AST)->raise_stmt.exp)

struct pkl_ast_raise_stmt
{
  struct pkl_ast_common common;
  union pkl_ast_node *exp;
};

pkl_ast_node pkl_ast_make_raise_stmt (pkl_ast ast, pkl_ast_node exp)
  __attribute__ ((visibility ("hidden")));

/* Finally, the `pkl_ast_node' type, which represents an AST node of
   any type.  */

union pkl_ast_node
{
  struct pkl_ast_common common; /* This field _must_ appear first.  */
  struct pkl_ast_program program;
  /* Expressions.  */
  struct pkl_ast_exp exp;
  struct pkl_ast_cond_exp cond_exp;
  struct pkl_ast_integer integer;
  struct pkl_ast_string string;
  struct pkl_ast_identifier identifier;
  struct pkl_ast_array array;
  struct pkl_ast_array_initializer array_initializer;
  struct pkl_ast_indexer indexer;
  struct pkl_ast_trimmer trimmer;
  struct pkl_ast_struct sct;
  struct pkl_ast_struct_field sct_field;
  struct pkl_ast_struct_ref sref;
  struct pkl_ast_offset offset;
  struct pkl_ast_cast cast;
  struct pkl_ast_isa isa;
  struct pkl_ast_map map;
  struct pkl_ast_scons scons;
  struct pkl_ast_funcall funcall;
  struct pkl_ast_funcall_arg funcall_arg;
  struct pkl_ast_var var;
  /* Types.  */
  struct pkl_ast_type type;
  struct pkl_ast_struct_type_field sct_type_elem;
  struct pkl_ast_func_type_arg fun_type_arg;
  struct pkl_ast_enum enumeration;
  struct pkl_ast_enumerator enumerator;
  /* Functions.  */
  struct pkl_ast_func func;
  struct pkl_ast_func_arg func_arg;
  /* Declarations.  */
  struct pkl_ast_decl decl;
  /* Statements.  */
  struct pkl_ast_comp_stmt comp_stmt;
  struct pkl_ast_ass_stmt ass_stmt;
  struct pkl_ast_if_stmt if_stmt;
  struct pkl_ast_loop_stmt loop_stmt;
  struct pkl_ast_loop_stmt_iterator loop_stmt_iterator;
  struct pkl_ast_return_stmt return_stmt;
  struct pkl_ast_exp_stmt exp_stmt;
  struct pkl_ast_try_catch_stmt try_catch_stmt;
  struct pkl_ast_try_until_stmt try_until_stmt;
  struct pkl_ast_break_stmt break_stmt;
  struct pkl_ast_raise_stmt raise_stmt;
  struct pkl_ast_print_stmt print_stmt;
  struct pkl_ast_print_stmt_arg print_stmt_arg;
};

/* The `pkl_ast' struct defined below contains a PKL abstract syntax tree.

   AST contains the tree of linked nodes, starting with a
   PKL_AST_PROGRAM node.

   `pkl_ast_init' allocates and initializes a new AST and returns a
   pointer to it.

   `pkl_ast_free' frees all the memory allocated to store the AST
   nodes.

   `pkl_ast_node_free' frees the memory allocated to store a single
   node in the AST and its descendants.  This function is used by the
   bison parser.  */

#define HASH_TABLE_SIZE 1008
typedef pkl_ast_node pkl_hash[HASH_TABLE_SIZE];

struct pkl_ast
{
  size_t uid;
  pkl_ast_node ast;

  char *buffer;
  FILE *file;
  char *filename;
};

pkl_ast pkl_ast_init (void)
  __attribute__ ((visibility ("hidden")));

void pkl_ast_free (pkl_ast ast)
  __attribute__ ((visibility ("hidden")));

void pkl_ast_node_free (pkl_ast_node ast)
  __attribute__ ((visibility ("hidden")));

void pkl_ast_node_free_chain (pkl_ast_node ast)
  __attribute__ ((visibility ("hidden")));

/* Reverse the order of elements chained by CHAIN, and return the new
   head of the chain (old last element).  */

pkl_ast_node pkl_ast_reverse (pkl_ast_node ast)
  __attribute__ ((visibility ("hidden")));

void pkl_ast_print (FILE *fp, pkl_ast_node ast)
  __attribute__ ((visibility ("hidden")));

#endif /* ! PKL_AST_H */
