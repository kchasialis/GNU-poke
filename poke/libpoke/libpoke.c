/* libpoke.c - Implementation of the public services in libpoke.  */

/* Copyright (C) 2020 Jose E. Marchesi */

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
#include <stdlib.h>
#include <assert.h>

#include "pkt.h"
#include "pkl.h"
#include "pkl-ast.h" /* XXX */
#include "pkl-env.h" /* XXX */
#include "pvm.h"
#include "libpoke.h"

struct pk_compiler
{
  pkl_compiler compiler;
  pvm vm;

  pkl_ast_node complete_type;
};

struct pk_term_if libpoke_term_if
__attribute__ ((visibility ("hidden")));

pk_compiler
pk_compiler_new (const char *rtpath,
                 struct pk_term_if *term_if)
{
  pk_compiler pkc
    = malloc (sizeof (struct pk_compiler));

  if (pkc)
    {
      libpoke_term_if = *term_if;

      pkc->vm = pvm_init ();
      if (pkc->vm == NULL)
        goto error;
      pkc->compiler = pkl_new (pkc->vm, rtpath);
      if (pkc->compiler == NULL)
        goto error;
      pkc->complete_type = NULL;

      pvm_set_compiler (pkc->vm, pkc->compiler);
    }

  return pkc;

 error:
  free (pkc);
  return NULL;
}

void
pk_compiler_free (pk_compiler pkc)
{
  if (pkc)
    {
      pkl_free (pkc->compiler);
      pvm_shutdown (pkc->vm);
    }

  free (pkc);
}

int
pk_compile_file (pk_compiler pkc, const char *filename,
                 int *exit_status)
{
  return pkl_execute_file (pkc->compiler, filename,
                           exit_status);
}

int
pk_compile_buffer (pk_compiler pkc, const char *buffer,
                   const char **end)
{
  return pkl_execute_buffer (pkc->compiler, buffer, end);
}

int
pk_compile_statement (pk_compiler pkc, const char *buffer,
                      const char **end, pk_val *valp)
{
  pvm_val val;

  if (!pkl_execute_statement (pkc->compiler, buffer, end, &val))
    return 0;

  if (valp)
    *valp = val;

  return 1;
}

int
pk_compile_expression (pk_compiler pkc, const char *buffer,
                       const char **end, pk_val *valp)
{
  pvm_val val;

  if (!pkl_execute_expression (pkc->compiler, buffer, end, &val))
    return 0;

  if (valp)
    *valp = val;

  return 1;
}

int
pk_load (pk_compiler pkc, const char *module)
{
  return pkl_load (pkc->compiler, module);
}

void
pk_set_quiet_p (pk_compiler pkc, int quiet_p)
{
  pkl_set_quiet_p (pkc->compiler, quiet_p);
}

void
pk_set_lexical_cuckolding_p (pk_compiler pkc, int lexical_cuckolding_p)
{
  pkl_set_lexical_cuckolding_p (pkc->compiler, lexical_cuckolding_p);
}

void
pk_set_alien_token_fn (pk_compiler pkc, pk_alien_token_handler_fn cb)
{
  pkl_set_alien_token_fn (pkc->compiler, cb);
}

static char *
complete_struct (pk_compiler pkc,
                 int *idx, const char *x, size_t len, int state)
{
  char *ret = NULL;
  pkl_ast_node type = pkc->complete_type;
  pkl_ast_node t;
  size_t trunk_len;
  int j;

  if (state == 0)
    {
      pkl_env compiler_env;
      int back, over;
      char *base;

      compiler_env = pkl_get_env (pkc->compiler);
      base = strndup (x, len - strlen (strchr (x, '.')));

      type = pkl_env_lookup (compiler_env, PKL_ENV_NS_MAIN,
                             base, &back, &over);
      free (base);

      if (type == NULL || PKL_AST_DECL_KIND (type) != PKL_AST_DECL_KIND_VAR)
        return NULL;

      type = PKL_AST_TYPE (PKL_AST_DECL_INITIAL (type));
      type = pkl_struct_type_traverse (type, x);
      if (type == NULL)
        {
          ret = NULL;
          goto exit;
        }
    }

  trunk_len = len - strlen (strrchr (x, '.')) + 1;

  t = PKL_AST_TYPE_S_ELEMS (type);

  for (j = 0; j < (*idx); j++)
    t = PKL_AST_CHAIN (t);

  for (; t; t = PKL_AST_CHAIN (t), (*idx)++)
    {
      pkl_ast_node ename;
      char *elem;

      if (PKL_AST_CODE (t) == PKL_AST_STRUCT_TYPE_FIELD
          || (PKL_AST_CODE (t) == PKL_AST_DECL
              && PKL_AST_DECL_KIND (t) == PKL_AST_DECL_KIND_FUNC
              && PKL_AST_FUNC_METHOD_P (PKL_AST_DECL_INITIAL (t))))
        {
          if (PKL_AST_CODE (t) == PKL_AST_STRUCT_TYPE_FIELD)
            ename = PKL_AST_STRUCT_TYPE_FIELD_NAME (t);
          else
            ename = PKL_AST_DECL_NAME (t);

          if (ename)
            elem = PKL_AST_IDENTIFIER_POINTER (ename);
          else
            elem = "<unnamed field>";

          if (strncmp (x + trunk_len, elem, len - trunk_len) == 0)
            {
              char *name;

              if (asprintf (&name, "%.*s%s", (int) trunk_len, x, elem) == -1)
                {
                  ret = NULL;
                  goto exit;
                }

              (*idx)++;
              ret = name;
              goto exit;
            }
        }
    }

 exit:
  pkc->complete_type = type;
  return ret;
}

/* This function is called repeatedly by the readline library, when
   generating potential command line completions.  It returns
   command line completion based upon the current state of PKC.

   TEXT is the partial word to be completed.  STATE is zero the first
   time the function is called and non-zero for each subsequent call.

   On each call, the function returns a potential completion.  It
   returns NULL to indicate that there are no more possibilities left. */
char *
pk_completion_function (pk_compiler pkc,
                        const char *text, int state)
{
  char *function_name;
  static int idx = 0;
  static struct pkl_ast_node_iter iter;
  pkl_env env = pkl_get_env (pkc->compiler);
  if (state == 0)
    {
      pkl_env_iter_begin (env, &iter);
      idx = 0;
    }
  else
    {
      if (pkl_env_iter_end (env, &iter))
        idx++;
      else
        pkl_env_iter_next (env, &iter);
    }

  size_t len = strlen (text);

  if ((text[0] != '.') && (strchr (text, '.') != NULL))
    return complete_struct (pkc, &idx, text, len, state);

  function_name = pkl_env_get_next_matching_decl (env, &iter, text, len);
  return function_name;
}

/* This function provides command line completion when the tag of an
   IOS is an appropriate completion.

   TEXT is the partial word to be completed.  STATE is zero the first
   time the function is called and non-zero for each subsequent call.

   On each call, the function returns the tag of an IOS for which
   that tag and TEXT share a common substring. It returns NULL to
   indicate that there are no more such tags.
 */
char *
pk_ios_completion_function (pk_compiler pkc __attribute__ ((unused)),
                            const char *text, int state)
{
  static ios io;
  if (state == 0)
    {
      io = ios_begin ();
    }
  else
    {
      io = ios_next (io);
    }

  int len  = strlen (text);
  while (1)
    {
      if (ios_end (io))
        break;

      char buf[16];
      snprintf (buf, 16, "#%d", ios_get_id (io));

      if (strncmp (buf, text, len) == 0)
        return strdup (buf);

      io = ios_next (io);
    }

  return NULL;
}

int
pk_disassemble_function (pk_compiler pkc,
                         const char *fname, int native_p)
{
  pvm_program program;
  int back, over;
  pvm_val val;

  pkl_env compiler_env = pkl_get_env (pkc->compiler);
  pvm_env runtime_env = pvm_get_env (pkc->vm);

  pkl_ast_node decl = pkl_env_lookup (compiler_env,
                                      PKL_ENV_NS_MAIN,
                                      fname,
                                      &back, &over);

  if (decl == NULL
      || PKL_AST_DECL_KIND (decl) != PKL_AST_DECL_KIND_FUNC)
    /* Function not found.  */
    return PK_ERROR;

  val = pvm_env_lookup (runtime_env, back, over);
  program = pvm_val_cls_program (val);

  if (native_p)
    pvm_disassemble_program_nat (program);
  else
    pvm_disassemble_program (program);

  return PK_OK;
}

int
pk_disassemble_expression (pk_compiler pkc, const char *str,
                           int native_p)
{
  const char *end;
  const char *program_string;

  pvm_program program;

  program_string = str;
  program = pkl_compile_expression (pkc->compiler,
                                    program_string, &end);

  if (program == NULL)
    /* Invalid expression.  */
    return PK_ERROR;

  if (*end != '\0')
    {
      pvm_destroy_program (program);
      return PK_ERROR;
    }

  if (native_p)
    pvm_disassemble_program_nat (program);
  else
    pvm_disassemble_program (program);

  return PK_OK;
}

pk_ios
pk_ios_cur (pk_compiler pkc)
{
  return (pk_ios) ios_cur ();
}

void
pk_ios_set_cur (pk_compiler pkc, pk_ios io)
{
  /* XXX use pkc */
  ios_set_cur ((ios) io);
}

const char *
pk_ios_handler (pk_ios io)
{
  return ios_handler ((ios) io);
}

uint64_t
pk_ios_flags (pk_ios io)
{
  return ios_flags ((ios) io);
}

pk_ios
pk_ios_search (pk_compiler pkc, const char *handler)
{
  /* XXX use pkc */
  return (pk_ios) ios_search (handler);
}

pk_ios
pk_ios_search_by_id (pk_compiler pkc, int id)
{
  /* XXX use pkc */
  return (pk_ios) ios_search_by_id (id);
}

int
pk_ios_open (pk_compiler pkc,
             const char *handler, uint64_t flags, int set_cur_p)
{
  /* XXX use pkc */
  return ios_open (handler, flags, set_cur_p);
}

void
pk_ios_close (pk_compiler pkc, pk_ios io)
{
  /* XXX use pkc */
  ios_close ((ios) io);
}

int
pk_ios_get_id (pk_ios io)
{
  return ios_get_id ((ios) io);
}

uint64_t
pk_ios_size (pk_ios io)
{
  return ios_size ((ios) io);
}

struct ios_map_fn_payload
{
  pk_ios_map_fn cb;
  void *data;
};

static void
my_ios_map_fn (ios io, void *data)
{
  struct ios_map_fn_payload *payload = data;
  payload->cb ((pk_ios) io, payload->data);
}

void
pk_ios_map (pk_compiler pkc,
            pk_ios_map_fn cb, void *data)
{
  struct ios_map_fn_payload payload = { cb, data };
  /* XXX use pkc */
  ios_map (my_ios_map_fn, (void *) &payload);
}

struct decl_map_fn_payload
{
  pk_map_decl_fn cb;
  void *data;
};

static void
my_decl_map_fn (pkl_ast_node decl, void *data)
{
  struct decl_map_fn_payload *payload = data;

  pkl_ast_node decl_name = PKL_AST_DECL_NAME (decl);
  pkl_ast_node initial = PKL_AST_DECL_INITIAL (decl);
  pkl_ast_loc loc = PKL_AST_LOC (decl);
  char *source =  PKL_AST_DECL_SOURCE (decl);
  char *type = NULL;
  int kind;

  /* Skip mappers, i.e. function declarations whose initials are
     actually struct types, and not function literals.  */

  if (PKL_AST_DECL_KIND (decl) == PKL_AST_DECL_KIND_FUNC
      && PKL_AST_CODE (initial) != PKL_AST_FUNC)
    return;

  switch (PKL_AST_DECL_KIND (decl))
    {
    case PKL_AST_DECL_KIND_VAR: kind = PK_DECL_KIND_VAR; break;
    case PKL_AST_DECL_KIND_FUNC: kind = PK_DECL_KIND_FUNC; break;
    case PKL_AST_DECL_KIND_TYPE: kind = PK_DECL_KIND_TYPE; break;
    default:
      /* Ignore this declaration.  */
      return;
    }

  type = pkl_type_str (PKL_AST_TYPE (initial), 1);
  payload->cb (kind,
               source,
               PKL_AST_IDENTIFIER_POINTER (decl_name),
               type,
               loc.first_line, loc.last_line,
               loc.first_column, loc.last_column,
               payload->data);
  free (type);
}

void
pk_decl_map (pk_compiler pkc, int kind,
             pk_map_decl_fn handler, void *data)
{
  struct decl_map_fn_payload payload = { handler, data };
  pkl_env compiler_env = pkl_get_env (pkc->compiler);
  int pkl_kind;

  switch (kind)
    {
    case PK_DECL_KIND_VAR: pkl_kind = PKL_AST_DECL_KIND_VAR; break;
    case PK_DECL_KIND_FUNC: pkl_kind = PKL_AST_DECL_KIND_FUNC; break;
    case PK_DECL_KIND_TYPE: pkl_kind = PKL_AST_DECL_KIND_TYPE; break;
    default:
      return;
    }

  pkl_env_map_decls (compiler_env, pkl_kind,
                     my_decl_map_fn, (void *) &payload);
}

int
pk_decl_p (pk_compiler pkc, const char *name, int kind)
{
  pkl_env compiler_env = pkl_get_env (pkc->compiler);

  pkl_ast_node decl = pkl_env_lookup (compiler_env,
                                      PKL_ENV_NS_MAIN,
                                      name,
                                      NULL, NULL);

  int pkl_kind;
  switch (kind)
    {
    case PK_DECL_KIND_VAR: pkl_kind = PKL_AST_DECL_KIND_VAR; break;
    case PK_DECL_KIND_FUNC: pkl_kind = PKL_AST_DECL_KIND_FUNC; break;
    case PK_DECL_KIND_TYPE: pkl_kind = PKL_AST_DECL_KIND_TYPE; break;
    default:
      return 0;
    }

  if (decl && PKL_AST_DECL_KIND (decl) == pkl_kind)
    return 1;
  else
    return 0;
}

pk_val
pk_decl_val (pk_compiler pkc, const char *name)
{
  pkl_env compiler_env = pkl_get_env (pkc->compiler);
  pvm_env runtime_env = pvm_get_env (pkc->vm);
  int back, over;

  pkl_ast_node decl = pkl_env_lookup (compiler_env,
                                      PKL_ENV_NS_MAIN,
                                      name,
                                      &back, &over);

  if (decl == NULL
      || PKL_AST_DECL_KIND (decl) != PKL_AST_DECL_KIND_VAR)
    return PK_NULL;

  return pvm_env_lookup (runtime_env, back, over);
}

int
pk_defvar (pk_compiler pkc, const char *varname, pk_val val)
{
  pvm_env runtime_env = pvm_get_env (pkc->vm);

  if (!pkl_defvar (pkc->compiler, varname, val))
    return 0;
  pvm_env_register (runtime_env, val);

  return 1;
}

int
pk_call (pk_compiler pkc, pk_val cls, pk_val *ret, ...)
{
  pvm_program program;
  va_list ap;
  enum pvm_exit_code rret;

  /* Compile a program that calls the function.  */
  va_start (ap, ret);
  program = pkl_compile_call (pkc->compiler, cls, ret, ap);
  va_end (ap);
  if (!program)
    return 0;

  /* Run the program in the poke VM.  */
  pvm_program_make_executable (program);
  rret = pvm_run (pkc->vm, program, ret);

  pvm_destroy_program (program);
  return (rret == PVM_EXIT_OK);
}

int
pk_obase (pk_compiler pkc)
{
  return pvm_obase (pkc->vm);
}

void
pk_set_obase (pk_compiler pkc, int obase)
{
  pvm_set_obase (pkc->vm, obase);
}

unsigned int
pk_oacutoff (pk_compiler pkc)
{
  return pvm_oacutoff (pkc->vm);
}

void pk_set_oacutoff (pk_compiler pkc, unsigned int oacutoff)
{
  pvm_set_oacutoff (pkc->vm, oacutoff);
}

unsigned int
pk_odepth (pk_compiler pkc)
{
  return pvm_odepth (pkc->vm);
}

void
pk_set_odepth (pk_compiler pkc, unsigned int odepth)
{
  pvm_set_odepth (pkc->vm, odepth);
}

unsigned int
pk_oindent (pk_compiler pkc)
{
  return pvm_oindent (pkc->vm);
}

void
pk_set_oindent (pk_compiler pkc, unsigned int oindent)
{
  pvm_set_oindent (pkc->vm, oindent);
}

int
pk_omaps (pk_compiler pkc)
{
  return pvm_omaps (pkc->vm);
}

void
pk_set_omaps (pk_compiler pkc, int omaps_p)
{
  pvm_set_omaps (pkc->vm, omaps_p);
}

enum pk_omode
pk_omode (pk_compiler pkc)
{
  enum pk_omode omode;

  switch (pvm_omode (pkc->vm))
    {
    case PVM_PRINT_FLAT: omode = PK_PRINT_FLAT; break;
    case PVM_PRINT_TREE: omode = PK_PRINT_TREE; break;
    default:
      assert (0);
    }

  return omode;
}

void
pk_set_omode (pk_compiler pkc, enum pk_omode omode)
{
  enum pvm_omode mode;

  switch (omode)
    {
    case PK_PRINT_FLAT: mode = PVM_PRINT_FLAT; break;
    case PK_PRINT_TREE: mode = PVM_PRINT_TREE; break;
    default:
      assert (0);
    }

  pvm_set_omode (pkc->vm, mode);
}

int
pk_error_on_warning (pk_compiler pkc)
{
  return pkl_error_on_warning (pkc->compiler);
}

void
pk_set_error_on_warning (pk_compiler pkc, int error_on_warning_p)
{
  pkl_set_error_on_warning (pkc->compiler, error_on_warning_p);
}

enum pk_endian
pk_endian (pk_compiler pkc)
{
  enum pk_endian endian;

  switch (pvm_endian (pkc->vm))
    {
    case IOS_ENDIAN_LSB: endian = PK_ENDIAN_LSB; break;
    case IOS_ENDIAN_MSB: endian = PK_ENDIAN_MSB; break;
    default:
      assert (0);
    }

  return endian;
}

void
pk_set_endian (pk_compiler pkc, enum pk_endian endian)
{
  enum ios_endian ios_endian;

  switch (endian)
    {
    case PK_ENDIAN_LSB: ios_endian = IOS_ENDIAN_LSB; break;
    case PK_ENDIAN_MSB: ios_endian = IOS_ENDIAN_MSB; break;
    default:
      assert (0);
    }

  pvm_set_endian (pkc->vm, ios_endian);
}

enum pk_nenc
pk_nenc (pk_compiler pkc)
{
  enum pk_nenc nenc;

  switch (pvm_nenc (pkc->vm))
    {
    case IOS_NENC_1: nenc = PK_NENC_1; break;
    case IOS_NENC_2: nenc = PK_NENC_2; break;
    default:
      assert (0);
    }

  return nenc;
}

void
pk_set_nenc (pk_compiler pkc, enum pk_nenc nenc)
{
  enum ios_nenc ios_nenc;

  switch (nenc)
    {
    case PK_NENC_1: ios_nenc = IOS_NENC_1; break;
    case PK_NENC_2: ios_nenc = IOS_NENC_2; break;
    default:
      assert (0);
    }

  pvm_set_nenc (pkc->vm, ios_nenc);
}

int
pk_pretty_print (pk_compiler pkc)
{
  return pvm_pretty_print (pkc->vm);
}

void
pk_set_pretty_print (pk_compiler pkc, int pretty_print_p)
{
  pvm_set_pretty_print (pkc->vm, pretty_print_p);
}

void
pk_print_val (pk_compiler pkc, pk_val val)
{
  pvm_print_val (pkc->vm, val);
}
