/* pk-cmd-vm.c - PVM related commands.  */

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
#include <assert.h>

#include "poke.h"
#include "pk-cmd.h"

#define PK_VM_DIS_UFLAGS "n"
#define PK_VM_DIS_F_NAT 0x1

static int
pk_cmd_vm_disas_exp (int argc, struct pk_cmd_arg argv[], uint64_t uflags)
{
  /* disassemble expression EXP.  */

  int ret;
  const char *expr;

  assert (argc == 1);
  assert (PK_CMD_ARG_TYPE (argv[0]) == PK_CMD_ARG_STR);

  expr = PK_CMD_ARG_STR (argv[0]);
  ret = pk_disassemble_expression (poke_compiler, expr,
                                   uflags & PK_VM_DIS_F_NAT);

  if (ret == PK_ERROR)
    {
      pk_term_class ("error");
      pk_puts ("error: ");
      pk_term_end_class ("error");
      pk_puts ("invalid expression\n");
      return 0;
    }
  return 1;
}

static int
pk_cmd_vm_disas_fun (int argc, struct pk_cmd_arg argv[], uint64_t uflags)
{
  /* disassemble function FNAME.  */

  int ret;
  const char *fname;

  assert (argc == 1);
  assert (PK_CMD_ARG_TYPE (argv[0]) == PK_CMD_ARG_STR);

  fname = PK_CMD_ARG_STR (argv[0]);
  ret = pk_disassemble_function (poke_compiler, fname,
                                 uflags & PK_VM_DIS_F_NAT);

  if (ret == PK_ERROR)
    {
      pk_term_class ("error");
      pk_puts ("error: ");
      pk_term_end_class ("error");
      pk_printf ("no such function `%s'\n", fname);
      return 0;
    }

  return 1;
}

extern struct pk_cmd null_cmd; /* pk-cmd.c  */

const struct pk_cmd vm_disas_exp_cmd =
  {"expression", "s", PK_VM_DIS_UFLAGS, 0, NULL, pk_cmd_vm_disas_exp,
   "vm disassemble expression[/n] EXP\n\
Flags:\n\
  n (do a native disassemble)", NULL};

const struct pk_cmd vm_disas_fun_cmd =
  {"function", "s", PK_VM_DIS_UFLAGS, 0, NULL, pk_cmd_vm_disas_fun,
   "vm disassemble function[/n] FUNCTION_NAME\n\
Flags:\n\
  n (do a native disassemble)", NULL};

const struct pk_cmd *vm_disas_cmds[] =
  {
   &vm_disas_exp_cmd,
   &vm_disas_fun_cmd,
   &null_cmd
  };

struct pk_trie *vm_disas_trie;

const struct pk_cmd vm_disas_cmd =
  {"disassemble", "e", PK_VM_DIS_UFLAGS, 0, &vm_disas_trie, NULL,
   "vm disassemble (expression|function)", NULL};

struct pk_trie *vm_trie;

const struct pk_cmd *vm_cmds[] =
  {
    &vm_disas_cmd,
    &null_cmd
  };

const struct pk_cmd vm_cmd =
  {"vm", "", "", 0, &vm_trie, NULL, "vm (disassemble)", NULL};
