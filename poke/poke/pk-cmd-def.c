/* pk-cmd-def.c - commands related to definitions.  */

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

#include "poke.h"
#include "pk-cmd.h"

static void
print_var_decl (int kind,
                const char *source,
                const char *name,
                const char *type,
                int first_line, int last_line,
                int first_column, int last_column,
                void *data)
{
  pk_puts (name);
  pk_puts ("\t\t");

  if (source)
    pk_printf ("%s:%d\n", basename (source), first_line);
  else
    pk_puts ("<stdin>\n");
}

static void
print_fun_decl (int kind,
                const char *source,
                const char *name,
                const char *type,
                int first_line, int last_line,
                int first_column, int last_column,
                void *data)
{
  pk_puts (name);
  pk_puts ("  ");
  pk_puts (type);
  pk_puts ("  ");

  if (source)
    pk_printf ("%s:%d\n", basename (source), first_line);
  else
    pk_puts ("<stdin>\n");
}

static int
pk_cmd_info_var (int argc, struct pk_cmd_arg argv[], uint64_t uflags)
{
  pk_puts (_("Name\t\tDeclared at\n"));
  pk_decl_map (poke_compiler, PK_DECL_KIND_VAR, print_var_decl, NULL);
  return 1;
}

static int
pk_cmd_info_fun (int argc, struct pk_cmd_arg argv[], uint64_t uflags)
{
  pk_decl_map (poke_compiler, PK_DECL_KIND_FUNC, print_fun_decl, NULL);
  return 1;
}

const struct pk_cmd info_var_cmd =
  {"variable", "", "", 0, NULL, pk_cmd_info_var,
   "info variable", NULL};

const struct pk_cmd info_fun_cmd =
  {"function", "", "", 0, NULL, pk_cmd_info_fun,
   "info funtion", NULL};
