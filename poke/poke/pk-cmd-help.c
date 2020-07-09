/* pk-cmd-help.c - `help' command.  */

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
#include "pk-cmd.h"

extern struct pk_cmd null_cmd; /* pk-cmd.c  */

const struct pk_cmd *help_cmds[] =
  {
    &null_cmd
  };

struct pk_trie *help_trie;

const struct pk_cmd help_cmd =
  {"help", "", "", 0, &help_trie, NULL, "help COMMAND", NULL};
