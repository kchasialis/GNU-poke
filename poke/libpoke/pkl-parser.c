/* pkl-parser.c - Parser for Poke.  */

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

#include "pkl-ast.h"
#include "pkl-parser.h"
#include "pkl-tab.h"
#define YYSTYPE PKL_TAB_STYPE /* XXX */
#define YYLTYPE PKL_TAB_LTYPE /* XXX */
#include "pkl-lex.h"

/* Allocate and initialize a parser.  */

static struct pkl_parser *
pkl_parser_init (void)
{
  struct pkl_parser *parser;

  parser = calloc (1, sizeof (struct pkl_parser));
  if (!parser)
    return NULL;

  pkl_tab_lex_init (&(parser->scanner));
  pkl_tab_set_extra (parser, parser->scanner);

  parser->env = NULL;
  parser->ast = pkl_ast_init ();
  parser->interactive = 0;
  parser->filename = NULL;
  parser->nchars = 0;
  parser->bootstrapped = 0;
  parser->in_method_decl_p = 0;

  return parser;
}

/* Free resources used by a parser, exceptuating the AST.  */

static void
pkl_parser_free (struct pkl_parser *parser)
{
  pkl_tab_lex_destroy (parser->scanner);
  free (parser->filename);

  free (parser);

  return;
}

/* Read from FD until end of file, parsing its contents as a PKL
   program.  Return 0 if the parsing was successful, 1 if there was a
   syntax error and 2 if there was a memory exhaustion.  */

int
pkl_parse_file (pkl_compiler compiler, pkl_env *env,
                pkl_ast *ast, FILE *fp, const char *fname)
{
  int ret;
  struct pkl_parser *parser;
  char *filename = strdup (fname);
  char *ast_filename = strdup (fname);

  if (!filename || !ast_filename)
    goto out_of_memory;

  parser = pkl_parser_init ();
  if (!parser)
    goto out_of_memory;

  parser->filename = filename;
  parser->start_token = START_PROGRAM;
  parser->compiler = compiler;
  parser->bootstrapped = pkl_bootstrapped_p (compiler);

  parser->env = *env;
  parser->ast->file = fp;
  parser->ast->filename = ast_filename;
  pkl_tab_set_in (fp, parser->scanner);
  ret = pkl_tab_parse (parser);
  *ast = parser->ast;
  *env = parser->env;

  /* In the absence of an error, only the top-level compile-time
     environment should remain after parsing.  In the case of an
     error, this doesn't matter since the environment is gonna be
     discarded anyway.  XXX but it would be nice to fix this in the
     parser's destructor.  */
  assert (ret != 0 || pkl_env_toplevel_p (parser->env));
  pkl_parser_free (parser);

  return ret;

out_of_memory:
  free (ast_filename);
  free (filename);
  return 2;
}

/* Parse the contents of BUFFER as a PKL program, or an expression
   depending on the value of WHAT.  If END is not NULL, set it to the
   first character after the parsed string.  Return 0 if the parsing
   was successful, 1 if there was a syntax error and 2 if there was a
   memory exhaustion.  */

int
pkl_parse_buffer (pkl_compiler compiler, pkl_env *env,
                  pkl_ast *ast, int what, const char *buffer, const char **end)
{
  YY_BUFFER_STATE yybuffer;
  struct pkl_parser *parser;
  int ret;
  char *buffer_dup = strdup (buffer);

  if (!buffer_dup)
    goto out_of_memory;

  parser = pkl_parser_init ();
  if (!parser)
    goto out_of_memory;

  parser->interactive = 1;
  parser->compiler = compiler;
  parser->bootstrapped = pkl_bootstrapped_p (compiler);

  if (what == PKL_PARSE_PROGRAM)
    parser->start_token = START_PROGRAM;
  else if (what == PKL_PARSE_EXPRESSION)
    parser->start_token = START_EXP;
  else if (what == PKL_PARSE_DECLARATION)
    parser->start_token = START_DECL;
  else if (what == PKL_PARSE_STATEMENT)
    parser->start_token = START_STMT;
  else
    assert (0);

  yybuffer = pkl_tab__scan_string (buffer, parser->scanner);

  /* XXX */
  /* pkl_tab_debug = 1; */
  parser->env = *env;
  parser->ast->buffer = buffer_dup;
  ret = pkl_tab_parse (parser);
  *ast = parser->ast;
  *env = parser->env;
  if (end != NULL)
    *end = buffer + parser->nchars;
  pkl_tab__delete_buffer (yybuffer, parser->scanner);

  /* In the absence of an error, only the top-level compile-time
     environment should remain after parsing.  In the case of an
     error, this doesn't matter since the environment is gonna be
     discarded anyway.  XXX but it would be nice to fix this in the
     parser's destructor.  */
  assert (ret != 0 || pkl_env_toplevel_p (parser->env));
  pkl_parser_free (parser);

  return ret;

out_of_memory:
  free (buffer_dup);
  return 2;
}
