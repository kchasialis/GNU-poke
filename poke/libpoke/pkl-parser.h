/* pkl-parser.h - Parser for Poke.  */

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

#ifndef PKL_PARSER_H
#define PKL_PARSER_H

#include <config.h>
#include <stdio.h>

#include "pkl.h"
#include "pkl-env.h"
#include "pkl-ast.h"

/* The `pkl_parser' struct holds the parser state.

   SCANNER is a flex scanner.
   AST is the abstract syntax tree created by the bison parser.

   BOOTSTRAPPED is 1 if the compiler has been bootstrapped.  0
   otherwise.

   IN_METHOD_P is 1 if we are parsing the declaration of a struct
   method.  0 otherwise.  */

struct pkl_parser
{
  void *scanner;
  pkl_env env;
  pkl_ast ast;
  pkl_compiler compiler;
  int interactive;
  char *filename;
  int start_token;
  size_t nchars;
  int bootstrapped;
  int in_method_decl_p;
  char *alien_errmsg;
};

/* Public interface.  */

#define PKL_PARSE_PROGRAM 0
#define PKL_PARSE_EXPRESSION 1
#define PKL_PARSE_DECLARATION 2
#define PKL_PARSE_STATEMENT 3

int pkl_parse_file (pkl_compiler compiler, pkl_env *env, pkl_ast *ast,
                    FILE *fp, const char *fname)
  __attribute__ ((visibility ("hidden")));

int pkl_parse_buffer (pkl_compiler compiler, pkl_env *env, pkl_ast *ast,
                      int what, const char *buffer, const char **end)
  __attribute__ ((visibility ("hidden")));



#endif /* !PKL_PARSER_H */
