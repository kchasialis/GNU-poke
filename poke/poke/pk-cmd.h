/* pk-cmd.h - Poke commands.  */

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

#ifndef PK_H_CMD
#define PK_H_CMD

#include <config.h>

#include <stddef.h>
#include "poke.h"

enum pk_cmd_arg_type
{
  PK_CMD_ARG_NULL,
  PK_CMD_ARG_INT,
  PK_CMD_ARG_STR,
  PK_CMD_ARG_TAG
};

#define PK_CMD_ARG_TYPE(arg) ((arg).type)
#define PK_CMD_ARG_INT(arg) ((arg).val.integer)
#define PK_CMD_ARG_STR(arg) ((arg).val.str)
#define PK_CMD_ARG_TAG(arg) ((arg).val.tag)

struct pk_cmd_arg
{
  enum pk_cmd_arg_type type;
  union
  {
    int64_t integer;
    char *str;
    int64_t tag;
  } val;
};

typedef int (*pk_cmd_fn) (int argc, struct pk_cmd_arg argv[], uint64_t uflags);

#define PK_CMD_F_REQ_IO 0x1  /* Command requires an IO space.  */
#define PK_CMD_F_REQ_W  0x2  /* Command requires a writable IO space.  */

/* This is the same as rl_compentry_func_t from readline.h
   Unfortunately #including readline.h here causes bad things
   to happen, because it defines lots of macros conflicting
   with ones used elsewhere.  */
typedef char* (*completer_t) (const char *, int);

struct pk_cmd
{
  /* Name of the command.  It is a NULL-terminated string composed by
     alphanumeric characters and '_'.  */
  const char *name;
  /* String specifying the arguments accepted by the command.  */
  const char *arg_fmt;
  /* String specifying the user flags accepted by the command.  */
  const char *uflags;
  /* A value composed of or-ed PK_CMD_F_* flags.  See above.  */
  int flags;
  /* Subcommands.  */
  struct pk_trie **subtrie;
  /* Function implementing the command.  */
  pk_cmd_fn handler;
  /* Usage message.  */
  const char *usage;
  /* The completion generator which generates arguments/operands
     for this command.  */
  completer_t completer;
};

/* Parse STR and execute a command.  Return 1 if the command was
   executed successfully, 0 otherwise.  */

int pk_cmd_exec (const char *str);

/* Execute commands from the given FILENAME.  Return 1 if all the
   commands were executed successfully, 0 otherwise.  */

int pk_cmd_exec_script (const char *filename);

/* Initialize the cmd subsystem.  */

void pk_cmd_init (void);

/* Shutdown the cmd subsystem, freeing all used resources.  */

void pk_cmd_shutdown (void);

char *pk_cmd_get_next_match (const char *x, size_t len);

const struct pk_cmd *pk_cmd_find (const char *cmdname);

#endif /* ! PK_H_CMD */
