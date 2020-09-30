/* pkl-tab.y - LALR(1) parser for Poke.  */

/* Copyright (C) 2019, 2020 Jose E. Marchesi.  */

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

%define api.pure full
%define parse.lac full
%define parse.error custom
 /* %deinfe parse.error custom */
%locations
%define api.prefix {pkl_tab_}

%lex-param {void *scanner}
%parse-param {struct pkl_parser *pkl_parser}

%initial-action
{
    @$.first_line = @$.last_line = 1;
    @$.first_column = @$.last_column = 1;
};

%{
#include <config.h>
#include <stdlib.h>
#include <stdio.h>
#include <xalloc.h>
#include <assert.h>
#include <string.h>
#include <gettext.h>
#define _(str) gettext (str)

#include "pk-utils.h"

#include "pkl.h"
#include "pkl-diag.h"
#include "pkl-ast.h"
#include "pkl-parser.h" /* For struct pkl_parser.  */

#include "pvm.h"

#define PKL_TAB_LTYPE pkl_ast_loc
#define YYDEBUG 1
#include "pkl-tab.h"
#include "pkl-lex.h"

#ifdef PKL_DEBUG
# include "pkl-gen.h"
#endif

#define scanner (pkl_parser->scanner)

/* YYLLOC_DEFAULT -> default code for computing locations.  */

#define PKL_AST_CHILDREN_STEP 12


/* Emit an error.  */

static void
pkl_tab_error (YYLTYPE *llocp,
               struct pkl_parser *pkl_parser,
               char const *err)
{
    pkl_error (pkl_parser->compiler, pkl_parser->ast, *llocp, "%s", err);
}

/* These are used in the defun_or_method rule.  */

#define IS_DEFUN 0
#define IS_METHOD 1

/* Register an argument in the compile-time environment.  This is used
   by function specifiers and try-catch statements.

   Return 0 if there was an error registering, 1 otherwise.  */

static int
pkl_register_arg (struct pkl_parser *parser, pkl_ast_node arg)
{
  pkl_ast_node arg_decl;
  pkl_ast_node arg_identifier = PKL_AST_FUNC_ARG_IDENTIFIER (arg);

  pkl_ast_node dummy
    = pkl_ast_make_integer (parser->ast, 0);
  PKL_AST_TYPE (dummy) = ASTREF (PKL_AST_FUNC_ARG_TYPE (arg));

  arg_decl = pkl_ast_make_decl (parser->ast,
                                PKL_AST_DECL_KIND_VAR,
                                arg_identifier,
                                dummy,
                                NULL /* source */);
  PKL_AST_LOC (arg_decl) = PKL_AST_LOC (arg);

  if (!pkl_env_register (parser->env,
                         PKL_ENV_NS_MAIN,
                         PKL_AST_IDENTIFIER_POINTER (arg_identifier),
                         arg_decl))
    {
      pkl_error (parser->compiler, parser->ast,PKL_AST_LOC (arg_identifier),
                 "duplicated argument name `%s' in function declaration",
                 PKL_AST_IDENTIFIER_POINTER (arg_identifier));
      /* Make sure to pop the function frame.  */
      parser->env = pkl_env_pop_frame (parser->env);
      return 0;
    }

  return 1;
}

#if 0
/* Register a list of arguments in the compile-time environment.  This
   is used by function specifiers and try-catch statements.

   Return 0 if there was an error registering, 1 otherwise.  */

static int
pkl_register_args (struct pkl_parser *parser, pkl_ast_node arg_list)
{
  pkl_ast_node arg;

  for (arg = arg_list; arg; arg = PKL_AST_CHAIN (arg))
    {
      pkl_ast_node arg_decl;
      pkl_ast_node arg_identifier = PKL_AST_FUNC_ARG_IDENTIFIER (arg);

      pkl_ast_node dummy
        = pkl_ast_make_integer (parser->ast, 0);
      PKL_AST_TYPE (dummy) = ASTREF (PKL_AST_FUNC_ARG_TYPE (arg));

      arg_decl = pkl_ast_make_decl (parser->ast,
                                    PKL_AST_DECL_KIND_VAR,
                                    arg_identifier,
                                    dummy,
                                    NULL /* source */);
      PKL_AST_LOC (arg_decl) = PKL_AST_LOC (arg);

      if (!pkl_env_register (parser->env,
                             PKL_ENV_NS_MAIN,
                             PKL_AST_IDENTIFIER_POINTER (arg_identifier),
                             arg_decl))
        {
          pkl_error (parser->compiler, parser->ast, PKL_AST_LOC (arg_identifier),
                     "duplicated argument name `%s' in function declaration",
                     PKL_AST_IDENTIFIER_POINTER (arg_identifier));
          /* Make sure to pop the function frame.  */
          parser->env = pkl_env_pop_frame (parser->env);
          return 0;
        }
    }

  return 1;
}
#endif

/* Register N dummy entries in the compilation environment.  */

static void
pkl_register_dummies (struct pkl_parser *parser, int n)
{
  int i;
  for (i = 0; i < n; ++i)
    {
      char *name;
      pkl_ast_node id;
      pkl_ast_node decl;
      int r;

      asprintf (&name, "@*UNUSABLE_OFF_%d*@", i);
      id = pkl_ast_make_identifier (parser->ast, name);
      decl = pkl_ast_make_decl (parser->ast,
                                PKL_AST_DECL_KIND_VAR,
                                id, NULL /* initial */,
                                NULL /* source */);

      r = pkl_env_register (parser->env, PKL_ENV_NS_MAIN, name, decl);
      assert (r);
    }
}

/* Load a module, given its name.
   If the module file cannot be read, return 1.
   If there is a parse error loading the module, return 2.
   Otherwise, return 0.  */

static int
load_module (struct pkl_parser *parser,
             const char *module, pkl_ast_node *node,
             int filename_p)
{
  char *module_filename = NULL;
  pkl_ast ast;
  FILE *fp;

  module_filename = pkl_resolve_module (parser->compiler,
                                        module,
                                        filename_p);
  if (module_filename == NULL)
    /* No file found.  */
    return 1;

  if (pkl_module_loaded_p (parser->compiler, module_filename))
    {
      /* Module already loaded.  */
      *node = NULL;
      return 0;
    }

  fp = fopen (module_filename, "rb");
  if (!fp)
    {
      free (module_filename);
      return 1;
    }

  /* Parse the file, using the given environment.  The declarations
     found in the parsed file are appended to that environment, so
     nothing extra should be done about that.  */
  if (pkl_parse_file (parser->compiler, &parser->env, &ast, fp,
                      module_filename)
      != 0)
    {
      fclose (fp);
      free (module_filename);
      return 2;
    }

  /* Add the module to the compiler's list of loaded modules.  */
  pkl_add_module (parser->compiler, module_filename);

  /* However, the AST nodes shall be appended explicitly, which is
     achieved by returning them to the caller in the NODE
     argument.  */
  *node = PKL_AST_PROGRAM_ELEMS (ast->ast);

  /* Dirty hack is dirty, but it works.  */
  PKL_AST_PROGRAM_ELEMS (ast->ast) = NULL;
  pkl_ast_free (ast);

  fclose (fp);
  free (module_filename);
  return 0;
}

%}

%union {
  pkl_ast_node ast;
  enum pkl_ast_op opcode;
  int integer;
}

%destructor {
  if ($$)
    {
      switch (PKL_AST_CODE ($$))
        {
        case PKL_AST_COMP_STMT:
            /*          pkl_parser->env = pkl_env_pop_frame (pkl_parser->env);*/
          break;
        case PKL_AST_TYPE:
          /*          if (PKL_AST_TYPE_CODE ($$) == PKL_TYPE_STRUCT)
                      pkl_parser->env = pkl_env_pop_frame (pkl_parser->env); */
          break;
        case PKL_AST_FUNC:
            /*          if (PKL_AST_FUNC_ARGS ($$))
                        pkl_parser->env = pkl_env_pop_frame (pkl_parser->env);*/
          break;
        default:
          break;
        }
    }

  $$ = ASTREF ($$); pkl_ast_node_free ($$);
 } <ast>

/* Primaries.  */

%token <ast> INTEGER     _("integer literal")
%token INTEGER_OVERFLOW
%token <ast> CHAR        _("character literal")
%token <ast> STR         _("string")
%token <ast> IDENTIFIER  _("identifier")
%token <ast> TYPENAME    _("type name")
%token <ast> UNIT        _("offset unit")

/* Reserved words.  */

%token ENUM              _("keyword `enum'")
%token <integer> PINNED  _("keyword `pinned'")
%token STRUCT            _("keyword `struct'")
token <integer> UNION    _("keyword `union'")
%token CONST             _("keyword `const'")
%token CONTINUE          _("keyword `continue'")
%token ELSE              _("keyword `else'")
%token IF                _("keyword `if'")
%token WHILE             _("keyword `while")
%token UNTIL             _("keyword `until'")
%token FOR               _("keyword `for'")
%token IN                _("keyword `in'")
%token WHERE             _("keyword `where'")
%token SIZEOF            _("keyword `sizeof'")
%token ASSERT            _("keyword `assert'")
%token ERR               _("token")
%token ALIEN
%token INTCONSTR         _("int type constructor")
%token UINTCONSTR        _("uint type constructor")
%token OFFSETCONSTR      _("offset type constructor")
%token DEFUN             _("keyword `defun'")
%token DEFSET            _("keyword `defset'")
%token DEFTYPE           _("keyword `deftype'")
%token DEFVAR            _("keyword `defvar'")
%token DEFUNIT           _("keyword `defunit'")
%token METHOD            _("keyword `method'")
%token RETURN            _("keyword `return'")
%token BREAK             _("keyword `break'")
%token STRING            _("string type specifier")
%token TRY               _("keyword `try'")
%token CATCH             _("keyword `catch'")
%token RAISE             _("keyword `raise'")
%token VOID              _("void type specifier")
%token ANY               _("any type specifier")
%token PRINT             _("keyword `print'")
%token PRINTF            _("keyword `printf'")
%token LOAD              _("keyword `load'")
%token BUILTIN_RAND BUILTIN_GET_ENDIAN BUILTIN_SET_ENDIAN
%token BUILTIN_GET_IOS BUILTIN_SET_IOS BUILTIN_OPEN BUILTIN_CLOSE
%token BUILTIN_IOSIZE BUILTIN_GETENV BUILTIN_FORGET

/* Compiler builtins.  */

/* Opcodes.  */

%token <opcode> MULA    _("multiply-and-assign operator")
%token <opcode> DIVA    _("divide-and-assing operator")
%token <opcode> MODA    _("modulus-and-assign operator")
%token <opcode> ADDA    _("add-and-assing operator")
%token <opcode> SUBA    _("subtract-and-assign operator")
%token <opcode> SLA     _("shift-left-and-assign operator")
%token <opcode> SRA     _("shift-right-and-assign operator")
%token <opcode> BANDA   _("bit-and-and-assign operator")
%token <opcode> XORA    _("bit-xor-and-assign operator")
%token <opcode> IORA    _("bit-or-and-assign operator")

%token OR               _("logical or operator")
%token AND              _("logical and operator")
%token '|'              _("bit-wise or operator")
%token '^'              _("bit-wise xor operator")
%token '&'              _("bit-wise and operator")
%token EQ               _("equality operator")
%token NE               _("inequality operator")
%token LE               _("less-or-equal operator")
%token GE               _("bigger-or-equal-than operator")
%token '<'              _("less-than operator")
%token '>'              _("bigger-than operator")
%token SL               _("left shift operator")
%token SR               _("right shift operator")
%token '+'              _("addition operator")
%token '-'              _("subtraction operator")
%token '*'              _("multiplication operator")
%token '/'              _("division operator")
%token CEILDIV          _("ceiling division operator")
%token '%'              _("modulus operator")
%token POW              _("power operator")
%token BCONC            _("bit-concatenation operator")
%token '@'              _("map operator")
%token INC              _("increment operator")
%token DEC              _("decrement operator")
%token AS               _("cast operator")
%token ISA              _("type identification operator")
%token '.'              _("dot operator")
%token <ast> ATTR       _("attribute")
%token UNMAP            _("unmap operator")

%token BIG              _("keyword `big'")
%token LITTLE           _("keyword `little'")
%token SIGNED           _("keyword `signed'")
%token UNSIGNED         _("keyword `unsigned'")
%token THREEDOTS        _("varargs indicator")

/* This is for the dangling ELSE.  */

%precedence THEN
%precedence ELSE

/* Operator tokens and their precedences, in ascending order.  */

%right '?' ':'
%left OR
%left AND
%left IN
%left '|'
%left '^'
%left '&'
%left EQ NE
%left LE GE '<' '>'
%left SL SR
%left '+' '-'
%left '*' '/' CEILDIV '%'
%left POW
%left BCONC
%right '@'
%nonassoc UNIT
%right UNARY INC DEC AS ISA
%left HYPERUNARY
%left '.'
%left ATTR

%type <opcode> unary_operator

%type <ast> start program program_elem_list program_elem load
%type <ast> expression primary identifier bconc map
%type <ast> funcall funcall_arg_list funcall_arg
%type <ast> array array_initializer_list array_initializer
%type <ast> struct_field_list struct_field
%type <ast> type_specifier simple_type_specifier
%type <ast> integral_type_specifier offset_type_specifier array_type_specifier
%type <ast> function_type_specifier function_type_arg_list function_type_arg
%type <ast> struct_type_specifier
%type <ast> struct_type_elem_list struct_type_field struct_type_field_identifier
%type <ast> struct_type_field_constraint_or_init struct_type_field_label
%type <ast> struct_type_field_optcond
%type <ast> declaration
%type <ast> function_specifier function_arg_list function_arg function_arg_initial
%type <ast> comp_stmt stmt_decl_list stmt print_stmt_arg_list
%type <ast> funcall_stmt funcall_stmt_arg_list funcall_stmt_arg
%type <ast> integral_struct
%type <integer> struct_type_pinned integral_type_sign struct_or_union
%type <integer> builtin endianness defun_or_method

/* The following two tokens are used in order to support several start
   rules: one is for parsing an expression, declaration or sentence,
   and the other for parsing a full poke programs.  This trick is
   explained in the Bison Manual in the "Multiple start-symbols"
   section.  */

%token START_EXP START_DECL START_STMT START_PROGRAM;

%start start

%% /* The grammar follows.  */

pushlevel:
          %empty
                {
                  pkl_parser->env = pkl_env_push_frame (pkl_parser->env);
                }
        ;


/*poplevel:
           %empty
                {
                  pkl_parser->env = pkl_env_pop_frame (pkl_parser->env);
                }
        ;
*/
start:
          START_EXP expression
                  {
                  $$ = pkl_ast_make_program (pkl_parser->ast, $2);
                  PKL_AST_LOC ($$) = @$;
                  pkl_parser->ast->ast = ASTREF ($$);
                }
        | START_EXP expression ','
                  {
                  $$ = pkl_ast_make_program (pkl_parser->ast, $2);
                  PKL_AST_LOC ($$) = @$;
                  pkl_parser->ast->ast = ASTREF ($$);
                  YYACCEPT;
                }
        | START_DECL declaration
                {
                  $$ = pkl_ast_make_program (pkl_parser->ast, $2);
                  PKL_AST_LOC ($$) = @$;
                  pkl_parser->ast->ast = ASTREF ($$);
                }
        | START_DECL declaration ','
                {
                  $$ = pkl_ast_make_program (pkl_parser->ast, $2);
                  PKL_AST_LOC ($$) = @$;
                  pkl_parser->ast->ast = ASTREF ($$);
                }
        | START_STMT stmt
                {
                  $$ = pkl_ast_make_program (pkl_parser->ast, $2);
                  PKL_AST_LOC ($$) = @$;
                  pkl_parser->ast->ast = ASTREF ($$);
                }
        | START_STMT stmt ';'
                {
                  /* This rule is to allow the presence of an extra
                     ';' after the sentence.  This to allow the poke
                     command manager to ease the handling of
                     semicolons in the command line.  */
                  $$ = pkl_ast_make_program (pkl_parser->ast, $2);
                  PKL_AST_LOC ($$) = @$;
                  pkl_parser->ast->ast = ASTREF ($$);
                }
        | START_STMT load
                {
                  $$ = pkl_ast_make_program (pkl_parser->ast, $2);
                  PKL_AST_LOC ($$) = @$;
                  pkl_parser->ast->ast = ASTREF ($$);
                }
        | START_STMT load ';'
                {
                  $$ = pkl_ast_make_program (pkl_parser->ast, $2);
                  PKL_AST_LOC ($$) = @$;
                  pkl_parser->ast->ast = ASTREF ($$);
                }
        | START_PROGRAM program
                {
                  $$ = pkl_ast_make_program (pkl_parser->ast, $2);
                  PKL_AST_LOC ($$) = @$;
                  pkl_parser->ast->ast = ASTREF ($$);
                }
        ;

program:
          %empty
                {
                  $$ = NULL;
                }
        | program_elem_list
        ;

program_elem_list:
          program_elem
        | program_elem_list program_elem
                {
                  if ($2 != NULL)
                    $$ = pkl_ast_chainon ($1, $2);
                  else
                    $$ = $1;
                }
        ;

program_elem:
          declaration
        | stmt
        | load
        ;

load:
        LOAD IDENTIFIER ';'
                {
                  int ret = load_module (pkl_parser,
                                         PKL_AST_IDENTIFIER_POINTER ($2),
                                         &$$, 0 /* filename_p */);
                  if (ret == 2)
                    /* The sub-parser should have emitted proper error
                       messages.  No need to be verbose here.  */
                    YYERROR;
                  else if (ret == 1)
                    {
                      pkl_error (pkl_parser->compiler, pkl_parser->ast, @2,
                                 "cannot load `%s'",
                                 PKL_AST_IDENTIFIER_POINTER ($2));
                      YYERROR;
                    }

                  $2 = ASTREF ($2);
                  pkl_ast_node_free ($2);
                }
        | LOAD STR ';'
                {
                  int ret = load_module (pkl_parser,
                                         PKL_AST_STRING_POINTER ($2),
                                         &$$, 1 /* filename_p */);
                  if (ret == 2)
                    /* The sub-parser should have emitted proper error
                       messages.  No need to be verbose here.  */
                    YYERROR;
                  else if (ret == 1)
                    {
                      pkl_error (pkl_parser->compiler, pkl_parser->ast, @2,
                                 "cannot load module from file `%s'",
                                 PKL_AST_STRING_POINTER ($2));
                      YYERROR;
                    }

                  $2 = ASTREF ($2);
                  pkl_ast_node_free ($2);
                }
        ;

/*
 * Identifiers.
 */

identifier:
          TYPENAME
        | IDENTIFIER
        ;

/*
 * Expressions.
 */

expression:
          primary
        | unary_operator expression %prec UNARY
                  {
                  $$ = pkl_ast_make_unary_exp (pkl_parser->ast,
                                               $1, $2);
                  PKL_AST_LOC ($$) = @1;
                }
        | SIZEOF '(' simple_type_specifier ')' %prec HYPERUNARY
                {
                  $$ = pkl_ast_make_unary_exp (pkl_parser->ast, PKL_AST_OP_SIZEOF, $3);
                  PKL_AST_LOC ($$) = @1;
                }
        | expression ATTR
                {
                  $$ = pkl_ast_make_binary_exp (pkl_parser->ast, PKL_AST_OP_ATTR,
                                                $1, $2);
                  PKL_AST_LOC ($2) = @2;
                  PKL_AST_LOC ($$) = @$;
                }
        | expression '+' expression
                {
                  $$ = pkl_ast_make_binary_exp (pkl_parser->ast, PKL_AST_OP_ADD,
                                                $1, $3);
                  PKL_AST_LOC ($$) = @$;
                }
        | expression '-' expression
                {
                  $$ = pkl_ast_make_binary_exp (pkl_parser->ast, PKL_AST_OP_SUB,
                                                $1, $3);
                  PKL_AST_LOC ($$) = @$;
                }
        | expression '*' expression
                {
                  $$ = pkl_ast_make_binary_exp (pkl_parser->ast, PKL_AST_OP_MUL,
                                                $1, $3);
                  PKL_AST_LOC ($$) = @$;
                }
        | expression '/' expression
                {
                  $$ = pkl_ast_make_binary_exp (pkl_parser->ast, PKL_AST_OP_DIV,
                                                $1, $3);
                  PKL_AST_LOC ($$) = @$;
                }
        | expression CEILDIV expression
                {
                  $$ = pkl_ast_make_binary_exp (pkl_parser->ast, PKL_AST_OP_CEILDIV, $1, $3);
                  PKL_AST_LOC ($$) = @$;
                }
        | expression POW expression
                {
                  $$ = pkl_ast_make_binary_exp (pkl_parser->ast, PKL_AST_OP_POW, $1, $3);
                  PKL_AST_LOC ($$) = @$;
                }
        | expression '%' expression
                {
                  $$ = pkl_ast_make_binary_exp (pkl_parser->ast, PKL_AST_OP_MOD,
                                                $1, $3);
                  PKL_AST_LOC ($$) = @$;
                }
        | expression SL expression
                {
                  $$ = pkl_ast_make_binary_exp (pkl_parser->ast, PKL_AST_OP_SL,
                                                $1, $3);
                  PKL_AST_LOC ($$) = @$;
                }
        | expression SR expression
                {
                  $$ = pkl_ast_make_binary_exp (pkl_parser->ast, PKL_AST_OP_SR,
                                                $1, $3);
                  PKL_AST_LOC ($$) = @$;
                }
        | expression EQ expression
                {
                  $$ = pkl_ast_make_binary_exp (pkl_parser->ast, PKL_AST_OP_EQ,
                                                $1, $3);
                  PKL_AST_LOC ($$) = @$;
                }
        | expression NE expression
                {
                  $$ = pkl_ast_make_binary_exp (pkl_parser->ast, PKL_AST_OP_NE,
                                                $1, $3);
                  PKL_AST_LOC ($$) = @$;
                }
        | expression '<' expression
                {
                  $$ = pkl_ast_make_binary_exp (pkl_parser->ast, PKL_AST_OP_LT,
                                                $1, $3);
                  PKL_AST_LOC ($$) = @$;
                }
        | expression '>' expression
                {
                  $$ = pkl_ast_make_binary_exp (pkl_parser->ast, PKL_AST_OP_GT,
                                                $1, $3);
                  PKL_AST_LOC ($$) = @$;
                }
        | expression LE expression
                {
                  $$ = pkl_ast_make_binary_exp (pkl_parser->ast, PKL_AST_OP_LE,
                                                $1, $3);
                  PKL_AST_LOC ($$) = @$;
                }
        | expression GE expression
                {
                  $$ = pkl_ast_make_binary_exp (pkl_parser->ast, PKL_AST_OP_GE,
                                                $1, $3);
                  PKL_AST_LOC ($$) = @$;
                }
        | expression '|' expression
                {
                  $$ = pkl_ast_make_binary_exp (pkl_parser->ast, PKL_AST_OP_IOR,
                                                $1, $3);
                  PKL_AST_LOC ($$) = @$;
                }
        | expression '^' expression
                {
                  $$ = pkl_ast_make_binary_exp (pkl_parser->ast, PKL_AST_OP_XOR,
                                                $1, $3);
                  PKL_AST_LOC ($$) = @$;
                }
        | expression '&' expression
                {
                  $$ = pkl_ast_make_binary_exp (pkl_parser->ast, PKL_AST_OP_BAND,
                                                $1, $3);
                  PKL_AST_LOC ($$) = @$;
                }
        | expression AND expression
                {
                  $$ = pkl_ast_make_binary_exp (pkl_parser->ast, PKL_AST_OP_AND,
                                                $1, $3);
                  PKL_AST_LOC ($$) = @$;
                }
        | expression OR expression
                {
                  $$ = pkl_ast_make_binary_exp (pkl_parser->ast, PKL_AST_OP_OR,
                                                $1, $3);
                  PKL_AST_LOC ($$) = @$;
                }
        | expression IN expression
                {
                  $$ = pkl_ast_make_binary_exp (pkl_parser->ast, PKL_AST_OP_IN,
                                                $1, $3);
                  PKL_AST_LOC ($$) = @$;
                }
        | expression AS simple_type_specifier
                {
                  $$ = pkl_ast_make_cast (pkl_parser->ast, $3, $1);
                  PKL_AST_LOC ($$) = @$;
                }
        | expression ISA simple_type_specifier
                {
                  $$ = pkl_ast_make_isa (pkl_parser->ast, $3, $1);
                  PKL_AST_LOC ($$) = @$;
                }
        | expression '?' expression ':' expression
                {
                  $$ = pkl_ast_make_cond_exp (pkl_parser->ast,
                                              $1, $3, $5);
                  PKL_AST_LOC ($$) = @$;
                }
        | TYPENAME '{' struct_field_list opt_comma '}'
                  {
                  pkl_ast_node type;
                  pkl_ast_node astruct;

                  pkl_ast_node decl = pkl_env_lookup (pkl_parser->env,
                                                      PKL_ENV_NS_MAIN,
                                                      PKL_AST_IDENTIFIER_POINTER ($1),
                                                      NULL, NULL);
                  assert (decl != NULL
                          && PKL_AST_DECL_KIND (decl) == PKL_AST_DECL_KIND_TYPE);

                  type = PKL_AST_DECL_INITIAL (decl);
                  if (PKL_AST_TYPE_CODE (type) != PKL_TYPE_STRUCT)
                    {
                      pkl_error (pkl_parser->compiler, pkl_parser->ast, @1,
                                 "expected struct type in constructor");
                      YYERROR;
                    }

                  if (PKL_AST_TYPE_NAME (type) == NULL)
                    PKL_AST_TYPE_NAME (type) = ASTREF ($1);
                  else
                    {
                      $1 = ASTREF ($1);
                      pkl_ast_node_free ($1);
                    }

                  astruct = pkl_ast_make_struct (pkl_parser->ast,
                                                 0 /* nelem */, $3);
                  PKL_AST_LOC (astruct) = @$;

                  $$ = pkl_ast_make_scons (pkl_parser->ast,
                                           type,
                                           astruct);
                  PKL_AST_LOC ($$) = @$;
                }
        | UNIT
                {
                  if ($1 == NULL)
                    {
                      pkl_error (pkl_parser->compiler, pkl_parser->ast, @1,
                                 "invalid unit in offset");
                      YYERROR;
                    }

                    $$ = pkl_ast_make_offset (pkl_parser->ast, NULL, $1);
                    PKL_AST_LOC ($1) = @1;
                    if (PKL_AST_TYPE ($1))
                        PKL_AST_LOC (PKL_AST_TYPE ($1)) = @1;
                    PKL_AST_LOC ($$) = @$;
                }
        | expression UNIT
                {
                  if ($2 == NULL)
                    {
                      pkl_error (pkl_parser->compiler, pkl_parser->ast, @2,
                                 "invalid unit in offset");
                      YYERROR;
                    }

                    $$ = pkl_ast_make_offset (pkl_parser->ast, $1, $2);
                    PKL_AST_LOC ($2) = @2;
                    if (PKL_AST_TYPE ($2))
                        PKL_AST_LOC (PKL_AST_TYPE ($2)) = @2;
                    PKL_AST_LOC ($$) = @$;
                }
        | bconc
        | map
        ;

bconc:
          expression BCONC expression
                {
                  $$ = pkl_ast_make_binary_exp (pkl_parser->ast, PKL_AST_OP_BCONC,
                                                $1, $3);
                  PKL_AST_LOC ($$) = @$;
                }
;

map:
          simple_type_specifier '@' expression %prec THEN
                {
                    $$ = pkl_ast_make_map (pkl_parser->ast, $1, NULL, $3);
                    PKL_AST_LOC ($$) = @$;
                }
        | simple_type_specifier '@' expression ':' expression %prec ELSE
                 {
                    $$ = pkl_ast_make_map (pkl_parser->ast, $1, $3, $5);
                    PKL_AST_LOC ($$) = @$;
                }
;

unary_operator:
          '-'                { $$ = PKL_AST_OP_NEG; }
        | '+'                { $$ = PKL_AST_OP_POS; }
        | '~'                { $$ = PKL_AST_OP_BNOT; }
        | '!'                { $$ = PKL_AST_OP_NOT; }
        | UNMAP                { $$ = PKL_AST_OP_UNMAP; }
        ;

primary:
          IDENTIFIER
                  {
                  /* Search for a variable definition in the
                     compile-time environment, and create a
                     PKL_AST_VAR node with it's lexical environment,
                     annotated with its initialization.  */

                  int back, over;
                  const char *name = PKL_AST_IDENTIFIER_POINTER ($1);

                  pkl_ast_node decl
                    = pkl_env_lookup (pkl_parser->env,
                                      PKL_ENV_NS_MAIN,
                                      name, &back, &over);
                  if (!decl
                      || (PKL_AST_DECL_KIND (decl) != PKL_AST_DECL_KIND_VAR
                          && PKL_AST_DECL_KIND (decl) != PKL_AST_DECL_KIND_FUNC))
                    {
                      pkl_error (pkl_parser->compiler, pkl_parser->ast, @1,
                                 "undefined variable '%s'", name);
                      YYERROR;
                    }

                  $$ = pkl_ast_make_var (pkl_parser->ast,
                                         $1, /* name.  */
                                         decl,
                                         back, over);
                  PKL_AST_LOC ($$) = @1;
                }
        | INTEGER
                {
                  $$ = $1;
                  PKL_AST_LOC ($$) = @$;
                  PKL_AST_LOC (PKL_AST_TYPE ($$)) = @$;
                }
        | INTEGER_OVERFLOW
                {
                  $$ = NULL; /* To avoid bison warning.  */
                  pkl_error (pkl_parser->compiler, pkl_parser->ast, @1,
                             "integer literal is too big");
                  YYERROR;
                }
        | CHAR
                {
                  $$ = $1;
                  PKL_AST_LOC ($$) = @$;
                  PKL_AST_LOC (PKL_AST_TYPE ($$)) = @$;
                }
        | STR
                {
                  $$ = $1;
                  PKL_AST_LOC ($$) = @$;
                  PKL_AST_LOC (PKL_AST_TYPE ($$)) = @$;
                }
        | '(' expression ')'
                {
                  $$ = $2;
                }
        | array
        | primary '.' identifier
                {
                    $$ = pkl_ast_make_struct_ref (pkl_parser->ast, $1, $3);
                    PKL_AST_LOC ($3) = @3;
                    PKL_AST_LOC ($$) = @$;
                }
        | primary '[' expression ']' %prec '.'
                {
                  $$ = pkl_ast_make_indexer (pkl_parser->ast, $1, $3);
                  PKL_AST_LOC ($$) = @$;
                }
        | primary '[' expression ':' expression ']' %prec '.'
                {
                  $$ = pkl_ast_make_trimmer (pkl_parser->ast,
                                             $1, $3, $5);
                  PKL_AST_LOC ($$) = @$;
                }
        | primary '[' ':' ']' %prec '.'
                {
                  $$ = pkl_ast_make_trimmer (pkl_parser->ast,
                                             $1, NULL, NULL);
                  PKL_AST_LOC ($$) = @$;
                }
        | primary '[' ':' expression ']' %prec '.'
                {
                  $$ = pkl_ast_make_trimmer (pkl_parser->ast,
                                             $1, NULL, $4);
                  PKL_AST_LOC ($$) = @$;
                }
        | primary '[' expression ':' ']' %prec '.'
                {
                  $$ = pkl_ast_make_trimmer (pkl_parser->ast,
                                             $1, $3, NULL);
                  PKL_AST_LOC ($$) = @$;
                }
        | funcall
        | '(' funcall_stmt ')'
                {
                  $$ = $2;
                }
        ;

funcall:
          primary '(' funcall_arg_list ')' %prec '.'
                  {
                  $$ = pkl_ast_make_funcall (pkl_parser->ast,
                                             $1, $3);
                  PKL_AST_LOC ($$) = @$;
                }
        ;

funcall_arg_list:
          %empty
                { $$ = NULL; }
        | funcall_arg
        | funcall_arg_list ',' funcall_arg
                {
                  $$ = pkl_ast_chainon ($1, $3);
                }
        ;

funcall_arg:
           expression
                  {
                  $$ = pkl_ast_make_funcall_arg (pkl_parser->ast,
                                                 $1, NULL /* name */);
                  PKL_AST_LOC ($$) = @$;
                }
        ;

opt_comma:
          %empty
        | ','
        ;

struct_field_list:
          %empty
                { $$ = NULL; }
        | struct_field
        | struct_field_list ',' struct_field
                {
                  $$ = pkl_ast_chainon ($1, $3);
                }
        ;

struct_field:
          expression
                  {
                    $$ = pkl_ast_make_struct_field (pkl_parser->ast,
                                                    NULL /* name */,
                                                    $1);
                    PKL_AST_LOC ($$) = @$;
                }
        | identifier '=' expression
                {
                    $$ = pkl_ast_make_struct_field (pkl_parser->ast,
                                                    $1,
                                                    $3);
                    PKL_AST_LOC ($1) = @1;
                    PKL_AST_LOC ($$) = @$;
                }
        ;

array:
          '[' array_initializer_list opt_comma ']'
                {
                    $$ = pkl_ast_make_array (pkl_parser->ast,
                                             0 /* nelem */,
                                             0 /* ninitializer */,
                                             $2);
                    PKL_AST_LOC ($$) = @$;
                }
        ;

array_initializer_list:
          array_initializer
        | array_initializer_list ',' array_initializer
                  {
                  $$ = pkl_ast_chainon ($1, $3);
                }
        ;

array_initializer:
          expression
                  {
                    $$ = pkl_ast_make_array_initializer (pkl_parser->ast,
                                                         NULL, $1);
                    PKL_AST_LOC ($$) = @$;
                }
        | '.' '[' expression ']' '=' expression
                {
                    $$ = pkl_ast_make_array_initializer (pkl_parser->ast,
                                                         $3, $6);
                    PKL_AST_LOC ($$) = @$;
                }
        ;

/*
 * Functions.
 */

pushlevel_args:
          %empty
                {
                  /* Push the lexical frame for the function's
                     arguments.  */
                  pkl_parser->env = pkl_env_push_frame (pkl_parser->env);

                  /* If in a method, register a dummy for the initial
                     implicit argument.  */
                  if (pkl_parser->in_method_decl_p)
                    pkl_register_dummies (pkl_parser, 1);
                }
        ;

function_specifier:
          '(' pushlevel_args function_arg_list ')' simple_type_specifier ':' comp_stmt
                {
                  $$ = pkl_ast_make_func (pkl_parser->ast,
                                          $5, $3, $7);
                  PKL_AST_LOC ($$) = @$;

                  /* Pop the frame introduced by `pushlevel'
                     above.  */
                  pkl_parser->env = pkl_env_pop_frame (pkl_parser->env);
                }
        | simple_type_specifier ':' pushlevel_args comp_stmt
                {
                  $$ = pkl_ast_make_func (pkl_parser->ast,
                                          $1, NULL, $4);
                  PKL_AST_LOC ($$) = @$;

                  /* Pop the frame introduced by `pushlevel'
                     above.  */
                  pkl_parser->env = pkl_env_pop_frame (pkl_parser->env);
                }
        ;

function_arg_list:
          function_arg
        | function_arg ',' function_arg_list
                  {
                  $$ = pkl_ast_chainon ($1, $3);
                }
        ;

function_arg:
          simple_type_specifier identifier function_arg_initial
                  {
                  $$ = pkl_ast_make_func_arg (pkl_parser->ast,
                                              $1, $2, $3);
                  PKL_AST_LOC ($2) = @2;
                  PKL_AST_LOC ($$) = @$;

                  if (!pkl_register_arg (pkl_parser, $$))
                      YYERROR;
                }
        | identifier THREEDOTS
                {
                  pkl_ast_node type
                    = pkl_ast_make_any_type (pkl_parser->ast);
                  pkl_ast_node array_type
                    = pkl_ast_make_array_type (pkl_parser->ast,
                                               type,
                                               NULL /* bound */);

                  PKL_AST_LOC (type) = @1;
                  PKL_AST_LOC (array_type) = @1;

                  $$ = pkl_ast_make_func_arg (pkl_parser->ast,
                                              array_type,
                                              $1,
                                              NULL /* initial */);
                  PKL_AST_FUNC_ARG_VARARG ($$) = 1;
                  PKL_AST_LOC ($1) = @1;
                  PKL_AST_LOC ($$) = @$;

                  if (!pkl_register_arg (pkl_parser, $$))
                      YYERROR;
                }
        ;

function_arg_initial:
        %empty                        { $$ = NULL; }
        | '=' expression        { $$ = $2; }
      ;

/*
 * Types.
 */

type_specifier:
          simple_type_specifier
        | struct_type_specifier
        | function_type_specifier
        ;

simple_type_specifier:
          TYPENAME
                  {
                  pkl_ast_node decl = pkl_env_lookup (pkl_parser->env,
                                                      PKL_ENV_NS_MAIN,
                                                      PKL_AST_IDENTIFIER_POINTER ($1),
                                                      NULL, NULL);
                  assert (decl != NULL
                          && PKL_AST_DECL_KIND (decl) == PKL_AST_DECL_KIND_TYPE);
                  $$ = PKL_AST_DECL_INITIAL (decl);
                  PKL_AST_LOC ($$) = @$;
                  $1 = ASTREF ($1); pkl_ast_node_free ($1);
                }
        | ANY
                {
                  $$ = pkl_ast_make_any_type (pkl_parser->ast);
                  PKL_AST_LOC ($$) = @$;
                }
        | VOID
                {
                  $$ = pkl_ast_make_void_type (pkl_parser->ast);
                  PKL_AST_LOC ($$) = @$;
                }
        | STRING
                {
                  $$ = pkl_ast_make_string_type (pkl_parser->ast);
                  PKL_AST_LOC ($$) = @$;
                }
        | integral_type_specifier
        | offset_type_specifier
        | array_type_specifier
        ;

integral_type_specifier:
          integral_type_sign INTEGER '>'
                {
                    $$ = pkl_ast_make_integral_type (pkl_parser->ast,
                                                     PKL_AST_INTEGER_VALUE ($2),
                                                     $1);
                    $2 = ASTREF ($2); pkl_ast_node_free ($2);
                    PKL_AST_LOC ($$) = @$;
                }
        ;

integral_type_sign:
          INTCONSTR        { $$ = 1; }
        | UINTCONSTR        { $$ = 0; }
        ;

offset_type_specifier:
          OFFSETCONSTR simple_type_specifier ',' identifier '>'
                {
                  pkl_ast_node decl
                    = pkl_env_lookup (pkl_parser->env,
                                      PKL_ENV_NS_UNITS,
                                      PKL_AST_IDENTIFIER_POINTER ($4),
                                      NULL, NULL);

                  if (!decl)
                    {
                      /* This could be the name of a type.  Try it out.  */
                      decl = pkl_env_lookup (pkl_parser->env,
                                             PKL_ENV_NS_MAIN,
                                             PKL_AST_IDENTIFIER_POINTER ($4),
                                             NULL, NULL);

                      if (!decl)
                        {
                          pkl_error (pkl_parser->compiler, pkl_parser->ast, @4,
                                     "invalid unit in offset type");
                          YYERROR;
                        }
                    }

                  $$ = pkl_ast_make_offset_type (pkl_parser->ast,
                                                 $2,
                                                 PKL_AST_DECL_INITIAL (decl));

                  ASTREF ($4); pkl_ast_node_free ($4);
                  PKL_AST_LOC ($$) = @$;
                }
        | OFFSETCONSTR simple_type_specifier ',' INTEGER '>'
                {
                    $$ = pkl_ast_make_offset_type (pkl_parser->ast,
                                                   $2, $4);
                    PKL_AST_LOC (PKL_AST_TYPE ($4)) = @4;
                    PKL_AST_LOC ($4) = @4;
                    PKL_AST_LOC ($$) = @$;
                }
        ;

array_type_specifier:
          simple_type_specifier '[' ']'
                {
                  $$ = pkl_ast_make_array_type (pkl_parser->ast, $1,
                                                NULL /* bound */);
                  PKL_AST_LOC ($$) = @$;
                }
        | simple_type_specifier '[' expression ']'
                {
                  $$ = pkl_ast_make_array_type (pkl_parser->ast, $1, $3);
                  PKL_AST_LOC ($$) = @$;
                }
        ;

function_type_specifier:
           '(' function_type_arg_list ')' simple_type_specifier
                {
                  $$ = pkl_ast_make_function_type (pkl_parser->ast,
                                                   $4, 0 /* narg */,
                                                   $2);
                  PKL_AST_LOC ($$) = @$;
                }
        | '(' ')' simple_type_specifier
                {
                  $$ = pkl_ast_make_function_type (pkl_parser->ast,
                                                   $3, 0 /* narg */,
                                                   NULL);
                  PKL_AST_LOC ($$) = @$;
                }
        ;

function_type_arg_list:
           function_type_arg
        |  function_type_arg ',' function_type_arg_list
                {
                  $$ = pkl_ast_chainon ($1, $3);
                }
        ;

function_type_arg:
          simple_type_specifier
                  {
                  $$ = pkl_ast_make_func_type_arg (pkl_parser->ast,
                                                   $1, NULL /* name */);
                  PKL_AST_LOC ($$) = @$;
                }
        | simple_type_specifier '?'
                {
                  $$ = pkl_ast_make_func_type_arg (pkl_parser->ast,
                                                   $1, NULL /* name */);
                  PKL_AST_LOC ($$) = @$;
                  PKL_AST_FUNC_TYPE_ARG_OPTIONAL ($$) = 1;
                }
        | THREEDOTS
                {
                  pkl_ast_node type
                    = pkl_ast_make_any_type (pkl_parser->ast);
                  pkl_ast_node array_type
                    = pkl_ast_make_array_type (pkl_parser->ast,
                                               type, NULL /* bound */);

                  PKL_AST_LOC (type) = @1;
                  PKL_AST_LOC (array_type) = @1;

                  $$ = pkl_ast_make_func_type_arg (pkl_parser->ast,
                                                   array_type, NULL /* name */);
                  PKL_AST_LOC ($$) = @$;
                  PKL_AST_FUNC_TYPE_ARG_VARARG ($$) = 1;
                }
        ;

struct_type_specifier:
          pushlevel struct_type_pinned struct_or_union
          integral_struct '{' '}'
                  {
                    $$ = pkl_ast_make_struct_type (pkl_parser->ast,
                                                   0 /* nelem */,
                                                   0 /* nfield */,
                                                   0 /* ndecl */,
                                                   $4,
                                                   NULL /* elems */,
                                                   $2, $3);
                    PKL_AST_LOC ($$) = @$;

                    /* The pushlevel in this rule and the subsequent
                       pop_frame, while not strictly needed, is to
                       avoid shift/reduce conflicts with the next
                       rule.  */
                    pkl_parser->env = pkl_env_pop_frame (pkl_parser->env);
                }
        | pushlevel struct_type_pinned struct_or_union
          integral_struct '{'
                {
                  /* Register dummies for the locals used in
                     pkl-gen.pks:struct_mapper.  */
                  pkl_register_dummies (pkl_parser, 4);
                }
          struct_type_elem_list '}'
                {
                    $$ = pkl_ast_make_struct_type (pkl_parser->ast,
                                                   0 /* nelem */,
                                                   0 /* nfield */,
                                                   0 /* ndecl */,
                                                   $4,
                                                   $7,
                                                   $2, $3);
                    PKL_AST_LOC ($$) = @$;

                    /* Pop the frame pushed in the `pushlevel' above.  */
                    pkl_parser->env = pkl_env_pop_frame (pkl_parser->env);
                }
        ;

struct_or_union:
          STRUCT        { $$ = 0; }
        | UNION         { $$ = 1; }
        ;

struct_type_pinned:
        %empty          { $$ = 0; }
        | PINNED        { $$ = 1; }
        ;

integral_struct:
        %empty           { $$ = NULL; }
        | simple_type_specifier { $$ = $1; }
        ;

struct_type_elem_list:
          struct_type_field
        | declaration
        | struct_type_elem_list declaration
                  { $$ = pkl_ast_chainon ($1, $2); }
        | struct_type_elem_list struct_type_field
                { $$ = pkl_ast_chainon ($1, $2); }
        ;

endianness:
          %empty        { $$ = PKL_AST_ENDIAN_DFL; }
        | LITTLE        { $$ = PKL_AST_ENDIAN_LSB; }
        | BIG                { $$ = PKL_AST_ENDIAN_MSB; }
        ;

struct_type_field:
          endianness type_specifier struct_type_field_identifier
                  {
                  if ($3 != NULL)
                    {
                      /* Register a variable IDENTIFIER in the current
                         environment.  We do it in this mid-rule so
                         the element can be used in the
                         constraint.  */

                      pkl_ast_node dummy, decl;

                      dummy = pkl_ast_make_integer (pkl_parser->ast, 0);
                      PKL_AST_TYPE (dummy) = ASTREF ($2);
                      decl = pkl_ast_make_decl (pkl_parser->ast,
                                                PKL_AST_DECL_KIND_VAR,
                                                $3, dummy,
                                                NULL /* source */);
                      PKL_AST_DECL_STRUCT_FIELD_P (decl) = 1;
                      PKL_AST_LOC (decl) = @$;

                      if (!pkl_env_register (pkl_parser->env,
                                             PKL_ENV_NS_MAIN,
                                             PKL_AST_IDENTIFIER_POINTER ($3),
                                             decl))
                        {
                          pkl_error (pkl_parser->compiler, pkl_parser->ast, @3,
                                     "duplicated struct element '%s'",
                                     PKL_AST_IDENTIFIER_POINTER ($3));
                          YYERROR;
                        }
                    }
                }
          struct_type_field_constraint_or_init struct_type_field_label
          struct_type_field_optcond ';'
                  {
                  pkl_ast_node constraint = NULL;
                  pkl_ast_node initializer = NULL;

                  if ($5 && PKL_AST_EXP_FLAG ($5))
                    {
                      pkl_ast_node field_decl, field_var;
                      int back, over;

                      /* We need a field name.  */
                      if ($3 == NULL)
                        {
                          pkl_error (pkl_parser->compiler, pkl_parser->ast, @$,
                                     "no initializer allowed in anonymous field");
                          YYERROR;
                        }

                      initializer = $5;

                      /* Build a constraint derived from the
                         initializer.  */
                      field_decl = pkl_env_lookup (pkl_parser->env,
                                                   PKL_ENV_NS_MAIN,
                                                   PKL_AST_IDENTIFIER_POINTER ($3),
                                                   &back, &over);
                      assert (field_decl);

                      field_var = pkl_ast_make_var (pkl_parser->ast,
                                                    $3,
                                                    field_decl,
                                                    back, over);
                      PKL_AST_LOC (field_var) = PKL_AST_LOC (initializer);

                      constraint = pkl_ast_make_binary_exp (pkl_parser->ast,
                                                            PKL_AST_OP_EQ,
                                                            field_var,
                                                            initializer);
                      PKL_AST_LOC (constraint) = PKL_AST_LOC (initializer);
                    }
                  else
                    constraint = $5;

                  $$ = pkl_ast_make_struct_type_field (pkl_parser->ast, $3, $2,
                                                       constraint, initializer,
                                                       $6, $1, $7);
                  PKL_AST_LOC ($$) = @$;

                  /* If endianness is empty, bison includes the blank
                     characters before the type field as if they were
                     part of this rule.  Therefore the location should
                     be adjusted here.  */
                  if ($1 == PKL_AST_ENDIAN_DFL)
                    {
                      PKL_AST_LOC ($$).first_line = @2.first_line;
                      PKL_AST_LOC ($$).first_column = @2.first_column;
                    }

                  if ($3 != NULL)
                    {
                      PKL_AST_LOC ($3) = @3;
                      PKL_AST_TYPE ($3) = pkl_ast_make_string_type (pkl_parser->ast);
                      ASTREF (PKL_AST_TYPE ($3));
                      PKL_AST_LOC (PKL_AST_TYPE ($3)) = @3;
                    }
                }
        ;

struct_type_field_identifier:
          %empty        { $$ = NULL; }
        | identifier        { $$ = $1; }
        ;

struct_type_field_label:
          %empty
                {
                  $$ = NULL;
                }
                | '@' expression
                {
                  $$ = $2;
                  PKL_AST_LOC ($$) = @$;
                }
        ;

struct_type_field_constraint_or_init:
          %empty
                {
                  $$ = NULL;
                }
          | ':' expression
                  {
                  $$ = $2;
                  PKL_AST_LOC ($$) = @$;
                }
          | '=' expression
                  {
                  $$ = $2;
                  /* Use use the flag to distinguish between
                     constraint and initializer.  */
                  PKL_AST_EXP_FLAG ($$) = 1;
                  PKL_AST_LOC ($$) = @$;
                }
          ;

struct_type_field_optcond:
          %empty
                {
                  $$ = NULL;
                }
        | IF expression
                {
                  $$ = $2;
                  PKL_AST_LOC ($$) = @$;
                }
        ;

/*
 * Declarations.
 */

declaration:
        defun_or_method identifier
                {
                  /* In order to allow for the function to be called
                     from within itself (recursive calls) we should
                     register a partial declaration in the
                     compile-time environment before processing the
                     `function_specifier' below.  */

                  $<ast>$ = pkl_ast_make_decl (pkl_parser->ast,
                                               PKL_AST_DECL_KIND_FUNC, $2,
                                               NULL /* initial */,
                                               pkl_parser->filename);
                  PKL_AST_LOC ($2) = @2;
                  PKL_AST_LOC ($<ast>$) = @$;

                  if (!pkl_env_register (pkl_parser->env,
                                         PKL_ENV_NS_MAIN,
                                         PKL_AST_IDENTIFIER_POINTER ($2),
                                         $<ast>$))
                    {
                      pkl_error (pkl_parser->compiler, pkl_parser->ast, @2,
                                 "function or variable `%s' already defined",
                                 PKL_AST_IDENTIFIER_POINTER ($2));
                      YYERROR;
                    }

                  /* function_specifier needs to know whether we are
                     in a function declaration or a method
                     declaration.  */
                  pkl_parser->in_method_decl_p = ($1 == IS_METHOD);
                }
        '=' function_specifier
                {
                  /* Complete the declaration registered above with
                     it's initial value, which is the specifier of the
                     function being defined.  */
                  PKL_AST_DECL_INITIAL ($<ast>3)
                    = ASTREF ($5);
                  $$ = $<ast>3;

                  /* If the reference counting of the declaration is
                     bigger than 1, this means there are recursive
                     calls in the function body.  Reset the refcount
                     to 1, since these references are weak.  */
                  if (PKL_AST_REFCOUNT ($<ast>3) > 1)
                    PKL_AST_REFCOUNT ($<ast>3) = 1;

                  /* Annotate the contained RETURN statements with
                     their function and their lexical nest level
                     within the function.  */
                  pkl_ast_finish_returns ($5);

                  /* Annotate the function to be a method whenever
                     appropriate.  */
                  if ($1 == IS_METHOD)
                    PKL_AST_FUNC_METHOD_P ($5) = 1;

                  /* XXX: move to trans1.  */
                  PKL_AST_FUNC_NAME ($5)
                    = xstrdup (PKL_AST_IDENTIFIER_POINTER ($2));

                  pkl_parser->in_method_decl_p = 0;
                }
        | DEFVAR identifier '=' expression ';'
                {
                  $$ = pkl_ast_make_decl (pkl_parser->ast,
                                          PKL_AST_DECL_KIND_VAR, $2, $4,
                                          pkl_parser->filename);
                  PKL_AST_LOC ($2) = @2;
                  PKL_AST_LOC ($$) = @$;

                  if (!pkl_env_register (pkl_parser->env,
                                         PKL_ENV_NS_MAIN,
                                         PKL_AST_IDENTIFIER_POINTER ($2),
                                         $$))
                    {
                      pkl_error (pkl_parser->compiler, pkl_parser->ast, @2,
                                 "the variable `%s' is already defined",
                                 PKL_AST_IDENTIFIER_POINTER ($2));
                      YYERROR;
                    }
                }
        | DEFTYPE identifier '=' type_specifier ';'
                {
                  $$ = pkl_ast_make_decl (pkl_parser->ast,
                                          PKL_AST_DECL_KIND_TYPE, $2, $4,
                                          pkl_parser->filename);
                  PKL_AST_LOC ($2) = @2;
                  PKL_AST_LOC ($$) = @$;

                  PKL_AST_TYPE_NAME ($4) = ASTREF ($2);

                  if (!pkl_env_register (pkl_parser->env,
                                         PKL_ENV_NS_MAIN,
                                         PKL_AST_IDENTIFIER_POINTER ($2),
                                         $$))
                    {
                      pkl_error (pkl_parser->compiler, pkl_parser->ast, @2,
                                 "the type `%s' is already defined",
                                 PKL_AST_IDENTIFIER_POINTER ($2));
                      YYERROR;
                    }
                }
        | DEFUNIT identifier '=' expression ';'
                {
                  /* We need to cast the expression to uint<64> here,
                     instead of pkl-promo, because the installed
                     initializer is used as earlier as in the
                     lexer.  Not pretty.  */
                  pkl_ast_node type
                    = pkl_ast_make_integral_type (pkl_parser->ast,
                                                  64, 0);
                  pkl_ast_node cast
                    = pkl_ast_make_cast (pkl_parser->ast,
                                         type, $4);

                  $$ = pkl_ast_make_decl (pkl_parser->ast,
                                          PKL_AST_DECL_KIND_UNIT, $2, cast,
                                          pkl_parser->filename);

                  PKL_AST_LOC (type) = @4;
                  PKL_AST_LOC (cast) = @4;
                  PKL_AST_LOC ($2) = @2;
                  PKL_AST_LOC ($$) = @$;

                  if (!pkl_env_register (pkl_parser->env,
                                         PKL_ENV_NS_UNITS,
                                         PKL_AST_IDENTIFIER_POINTER ($2),
                                         $$))
                    {
                      pkl_error (pkl_parser->compiler, pkl_parser->ast, @2,
                                 "the unit `%s' is already defined",
                                 PKL_AST_IDENTIFIER_POINTER ($2));
                      YYERROR;
                    }
                }
        ;

defun_or_method:
          DEFUN                { $$ = IS_DEFUN; }
        | METHOD        { $$ = IS_METHOD; }
        ;

/*
 * Statements.
 */

comp_stmt:
          pushlevel '{' '}'
            {
              $$ = pkl_ast_make_comp_stmt (pkl_parser->ast, NULL);
              PKL_AST_LOC ($$) = @$;

              /* Pop the frame pushed by the `pushlevel' above.  */
              pkl_parser->env = pkl_env_pop_frame (pkl_parser->env);
            }
         |  pushlevel '{' stmt_decl_list '}'
            {
              $$ = pkl_ast_make_comp_stmt (pkl_parser->ast, $3);
              PKL_AST_LOC ($$) = @$;

              /* Pop the frame pushed by the `pushlevel' above.  */
              pkl_parser->env = pkl_env_pop_frame (pkl_parser->env);
            }
        | pushlevel builtin
        {
          $$ = pkl_ast_make_builtin (pkl_parser->ast, $2);
          PKL_AST_LOC ($$) = @$;

          /* Pop the frame pushed by the `pushlevel' above.  */
          pkl_parser->env = pkl_env_pop_frame (pkl_parser->env);
        }
        ;

builtin:
          BUILTIN_RAND                { $$ = PKL_AST_BUILTIN_RAND; }
        | BUILTIN_GET_ENDIAN        { $$ = PKL_AST_BUILTIN_GET_ENDIAN; }
        | BUILTIN_SET_ENDIAN        { $$ = PKL_AST_BUILTIN_SET_ENDIAN; }
        | BUILTIN_GET_IOS       { $$ = PKL_AST_BUILTIN_GET_IOS; }
        | BUILTIN_SET_IOS       { $$ = PKL_AST_BUILTIN_SET_IOS; }
        | BUILTIN_OPEN                { $$ = PKL_AST_BUILTIN_OPEN; }
        | BUILTIN_CLOSE                { $$ = PKL_AST_BUILTIN_CLOSE; }
        | BUILTIN_IOSIZE        { $$ = PKL_AST_BUILTIN_IOSIZE; }
        | BUILTIN_GETENV        { $$ = PKL_AST_BUILTIN_GETENV; }
        | BUILTIN_FORGET        { $$ = PKL_AST_BUILTIN_FORGET; }
        ;

stmt_decl_list:
          stmt
        | stmt_decl_list stmt
                  { $$ = pkl_ast_chainon ($1, $2); }
        | declaration
        | stmt_decl_list declaration
                  { $$ = pkl_ast_chainon ($1, $2); }
        ;

stmt:
          comp_stmt
        | ';'
                  {
                  $$ = pkl_ast_make_null_stmt (pkl_parser->ast);
                  PKL_AST_LOC ($$) = @$;
                }
        | primary '=' expression ';'
                  {
                  $$ = pkl_ast_make_ass_stmt (pkl_parser->ast,
                                              $1, $3);
                  PKL_AST_LOC ($$) = @$;
                }
        | bconc '=' expression ';'
                {
                  $$ = pkl_ast_make_ass_stmt (pkl_parser->ast,
                                              $1, $3);
                  PKL_AST_LOC ($$) = @$;
                }
        | map '=' expression ';'
                {
                  $$ = pkl_ast_make_ass_stmt (pkl_parser->ast,
                                              $1, $3);
                  PKL_AST_LOC ($$) = @$;
                }
        | IF '(' expression ')' stmt %prec THEN
                {
                  $$ = pkl_ast_make_if_stmt (pkl_parser->ast,
                                             $3, $5, NULL);
                  PKL_AST_LOC ($$) = @$;
                }
        | IF '(' expression ')' stmt ELSE stmt %prec ELSE
                {
                  $$ = pkl_ast_make_if_stmt (pkl_parser->ast,
                                             $3, $5, $7);
                  PKL_AST_LOC ($$) = @$;
                }
        | WHILE '(' expression ')' stmt
                {
                  $$ = pkl_ast_make_loop_stmt (pkl_parser->ast,
                                               NULL, /* iterator */
                                               $3,   /* condition */
                                               $5);  /* body */
                  PKL_AST_LOC ($$) = @$;

                  /* Annotate the contained BREAK statements with
                     their lexical level within this loop.  */
                  pkl_ast_finish_breaks ($$, $5);
                }
        | FOR '(' IDENTIFIER IN expression pushlevel
                {
                  /* Push a new lexical level and register a variable
                     with name IDENTIFIER.  Note that the variable is
                     created with a dummy INITIAL, as there is none.  */

                  pkl_ast_node dummy = pkl_ast_make_integer (pkl_parser->ast,
                                                             0);
                  PKL_AST_LOC (dummy) = @3;

                  $<ast>$ = pkl_ast_make_decl (pkl_parser->ast,
                                               PKL_AST_DECL_KIND_VAR,
                                               $3,
                                               dummy,
                                               pkl_parser->filename);
                  PKL_AST_LOC ($<ast>$) = @3;

                  if (!pkl_env_register (pkl_parser->env,
                                         PKL_ENV_NS_MAIN,
                                         PKL_AST_IDENTIFIER_POINTER ($3),
                                         $<ast>$))
                    /* This should never happen.  */
                    assert (0);
                }
          ')' stmt
                {
                  pkl_ast_node iterator
                    = pkl_ast_make_loop_stmt_iterator (pkl_parser->ast,
                                                       $<ast>7, /* decl */
                                                       $5); /* container */
                  PKL_AST_LOC (iterator) = @$;

                  $$ = pkl_ast_make_loop_stmt (pkl_parser->ast,
                                               iterator,
                                               NULL, /* condition */
                                               $9);  /* body */
                  PKL_AST_LOC ($$) = @$;

                  /* Free the identifier.  */
                  ASTREF ($3); pkl_ast_node_free ($3);

                  /* Annotate the contained BREAK statements with
                     their lexical level within this loop.  */
                  pkl_ast_finish_breaks ($$, $9);

                  /* Pop the frame introduced by `pushlevel'
                     above.  */
                  pkl_parser->env = pkl_env_pop_frame (pkl_parser->env);
                }
        | FOR '(' IDENTIFIER IN expression pushlevel
                {
                  /* XXX: avoid code replication here.  */

                  /* Push a new lexical level and register a variable
                     with name IDENTIFIER.  Note that the variable is
                     created with a dummy INITIAL, as there is none.  */

                  pkl_ast_node dummy = pkl_ast_make_integer (pkl_parser->ast,
                                                             0);
                  PKL_AST_LOC (dummy) = @3;

                  $<ast>$ = pkl_ast_make_decl (pkl_parser->ast,
                                               PKL_AST_DECL_KIND_VAR,
                                               $3,
                                               dummy,
                                               pkl_parser->filename);
                  PKL_AST_LOC ($<ast>$) = @3;

                  if (!pkl_env_register (pkl_parser->env,
                                         PKL_ENV_NS_MAIN,
                                         PKL_AST_IDENTIFIER_POINTER ($3),
                                         $<ast>$))
                    /* This should never happen.  */
                    assert (0);
                }
          WHERE expression ')' stmt
                {
                  pkl_ast_node iterator
                    = pkl_ast_make_loop_stmt_iterator (pkl_parser->ast,
                                                       $<ast>7, /* decl */
                                                       $5); /* container */
                  PKL_AST_LOC (iterator) = @$;

                  $$ = pkl_ast_make_loop_stmt (pkl_parser->ast,
                                               iterator,
                                               $9, /* condition */
                                               $11); /* body */
                  PKL_AST_LOC ($3) = @3;
                  PKL_AST_LOC ($$) = @$;

                  /* Annotate the contained BREAK statements with
                     their lexical level within this loop.  */
                  pkl_ast_finish_breaks ($$, $11);

                  /* Pop the frame introduced by `pushlevel'
                     above.  */
                  pkl_parser->env = pkl_env_pop_frame (pkl_parser->env);
                }
        | BREAK ';'
                {
                  $$ = pkl_ast_make_break_stmt (pkl_parser->ast);
                  PKL_AST_LOC ($$) = @$;
                }
        | RETURN ';'
                {
                  $$ = pkl_ast_make_return_stmt (pkl_parser->ast,
                                                 NULL);
                  PKL_AST_LOC ($$) = @$;
                }
        | RETURN expression ';'
                {
                  $$ = pkl_ast_make_return_stmt (pkl_parser->ast,
                                                 $2);
                  PKL_AST_LOC ($$) = @$;
                }
        | TRY stmt CATCH comp_stmt
                {
                  $$ = pkl_ast_make_try_catch_stmt (pkl_parser->ast,
                                                    $2, $4, NULL, NULL);
                  PKL_AST_LOC ($$) = @$;
                }
        | TRY stmt CATCH IF expression comp_stmt
                {
                  $$ = pkl_ast_make_try_catch_stmt (pkl_parser->ast,
                                                    $2, $6, NULL, $5);
                  PKL_AST_LOC ($$) = @$;
                }
        | TRY stmt CATCH  '(' pushlevel function_arg ')' comp_stmt
                {
                  $$ = pkl_ast_make_try_catch_stmt (pkl_parser->ast,
                                                    $2, $8, $6, NULL);
                  PKL_AST_LOC ($$) = @$;

                  /* Pop the frame introduced by `pushlevel'
                     above.  */
                  pkl_parser->env = pkl_env_pop_frame (pkl_parser->env);
                }
        | TRY stmt UNTIL expression ';'
                {
                  $$ = pkl_ast_make_try_until_stmt (pkl_parser->ast,
                                                    $2, $4);
                  PKL_AST_LOC ($$) = @$;

                  /* Annotate the contained BREAK statements with
                     their lexical level within this loop.  */
                  pkl_ast_finish_breaks ($$, $2);
                }
        | RAISE ';'
                {
                  $$ = pkl_ast_make_raise_stmt (pkl_parser->ast,
                                                NULL);
                  PKL_AST_LOC ($$) = @$;
                }
        | RAISE expression ';'
                {
                  $$ = pkl_ast_make_raise_stmt (pkl_parser->ast,
                                                $2);
                  PKL_AST_LOC ($$) = @$;
                }
        | expression ';'
                {
                  $$ = pkl_ast_make_exp_stmt (pkl_parser->ast,
                                              $1);
                  PKL_AST_LOC ($$) = @$;
                }
        | PRINT expression ';'
                {
                  $$ = pkl_ast_make_print_stmt (pkl_parser->ast,
                                                NULL /* fmt */, $2);
                  PKL_AST_LOC ($$) = @$;
                }
        | PRINTF STR print_stmt_arg_list ';'
                {
                  $$ = pkl_ast_make_print_stmt (pkl_parser->ast,
                                                $2, $3);
                  PKL_AST_LOC ($2) = @2;
                  if (PKL_AST_TYPE ($2))
                    PKL_AST_LOC (PKL_AST_TYPE ($2)) = @2;
                  PKL_AST_LOC ($$) = @$;
                }
        | PRINTF '(' STR print_stmt_arg_list ')' ';'
                {
                  $$ = pkl_ast_make_print_stmt (pkl_parser->ast,
                                                $3, $4);
                  PKL_AST_LOC ($3) = @3;
                  if (PKL_AST_TYPE ($3))
                    PKL_AST_LOC (PKL_AST_TYPE ($3)) = @3;
                  PKL_AST_LOC ($$) = @$;
                }
        | funcall_stmt ';'
                {
                  $$ = pkl_ast_make_exp_stmt (pkl_parser->ast,
                                              $1);
                  PKL_AST_LOC ($$) = @$;
                }
        ;

print_stmt_arg_list:
          %empty
                {
                  $$ = NULL;
                }
        | print_stmt_arg_list ',' expression
                {
                  pkl_ast_node arg
                    = pkl_ast_make_print_stmt_arg (pkl_parser->ast, $3);
                  PKL_AST_LOC (arg) = @3;

                  $$ = pkl_ast_chainon ($1, arg);
                }
        ;

funcall_stmt:
        primary funcall_stmt_arg_list
                {
                  $$ = pkl_ast_make_funcall (pkl_parser->ast,
                                             $1, $2);
                  PKL_AST_LOC ($$) = @$;
                }
        ;

funcall_stmt_arg_list:
          funcall_stmt_arg
        | funcall_stmt_arg_list funcall_stmt_arg
                {
                  $$ = pkl_ast_chainon ($1, $2);
                }
        ;

funcall_stmt_arg:
          ':' IDENTIFIER expression
                  {
                  $$ = pkl_ast_make_funcall_arg (pkl_parser->ast,
                                                 $3, $2);
                  PKL_AST_LOC ($2) = @2;
                  PKL_AST_LOC ($$) = @$;
                }
        ;

/*
 * Enumerations.
 */

/*
enum_specifier:
          ENUM IDENTIFIER '{' enumerator_list '}'
                  {
                  if (! enum_specifier_action (pkl_parser,
                                               &$$,
                                               $IDENTIFIER, &@IDENTIFIER,
                                               $enumerator_list, &@enumerator_list,
                                               NULL, NULL))
                    YYERROR;
                }
        ;

enumerator_list:
          enumerator
        | enumerator_list ',' enumerator
                  { $$ = pkl_ast_chainon ($1, $3); }
        ;

enumerator:
          IDENTIFIER
                { $$ = pkl_ast_make_enumerator ($1, NULL, NULL); }
        | IDENTIFIER '=' constant_expression
                { $$ = pkl_ast_make_enumerator ($1, $3, NULL); }
        ;
*/

%%

/* Handle syntax errors.  */

int
yyreport_syntax_error (const yypcontext_t *ctx,
                       struct pkl_parser *pkl_parser)
{
  int res = 0;
  char *errmsg;
  yysymbol_kind_t lookahead = yypcontext_token (ctx);

  errmsg = strdup ("syntax error");
  if (!errmsg)
    return YYENOMEM;

  /* if the unexpected token is alien, then report
     pkl_parser->alien_err_msg.  */
  if (lookahead == YYSYMBOL_ALIEN)
    {
      pkl_tab_error (yypcontext_location (ctx),
                     pkl_parser,
                     pkl_parser->alien_errmsg);
      free (pkl_parser->alien_errmsg);
      pkl_parser->alien_errmsg = NULL;
    }
  else
    {
      /* report tokens expected at this point.  */
      yysymbol_kind_t expected[YYNTOKENS];
      int nexpected = yypcontext_expected_tokens (ctx, expected, YYNTOKENS);

      if (nexpected < 0)
        /* forward errors to yyparse.  */
        res = nexpected;
      else
        {
          /* XXX use expected?  */
#if 0
          int i;

          for (i = 0; i < nexpected; ++i)
            {
              char *tmp = pk_str_concat (errmsg,
                                         i == 0 ? ": expected " : " or ",
                                         yysymbol_name (expected[i]),
                                         NULL);
              free (errmsg);
              errmsg = tmp;
            }
#endif
          /* XXX use a table with better names for tokens.  */
          if (lookahead != YYSYMBOL_YYEMPTY)
            {
              char *tmp = pk_str_concat (errmsg,
                                         ": unexpected ",
                                         yysymbol_name (lookahead),
                                         NULL);
              free (errmsg);
              if (!tmp)
                return YYENOMEM;
              errmsg = tmp;
            }

          pkl_tab_error (yypcontext_location (ctx), pkl_parser, errmsg);
          free (errmsg);
        }
    }

  return res;
}
