/* pk-cmd-editor.c - Command to invoke an external editor.  */

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

#include <unistd.h>
#include <stdlib.h>
#include <tmpdir.h>
#include <assert.h>
#include "xalloc.h"
#include "findprog.h"
#include "pathmax.h"

#include "poke.h"
#include "pk-cmd.h"
#include "pk-utils.h"

static int
pk_cmd_editor (int argc, struct pk_cmd_arg argv[], uint64_t uflags)
{
  const char *editor;
  char *cmdline;
  char tmpfile[1024];
  int des;
  FILE *fp;

  /* Do nothing (succesfully) if not in interactive mode.  */
  if (!poke_interactive_p)
    return 1;

  /* editor */
  assert (argc == 0);

  /* Get the value of EDITOR.  */
  editor = getenv ("EDITOR");
  /* Debian based systems should always have "sensible-editor"
     in the path.  */
  if (!editor)
    {
      editor = find_in_path ("sensible-editor");
      if (STREQ (editor, "sensible-editor"))
        editor = NULL;
    }
  if (!editor)
    {
      pk_term_class ("error");
      pk_puts ("error: ");
      pk_term_end_class ("error");
      pk_puts ("the EDITOR environment variable is not set.\n");
      return 0;
    }

  /* Get a temporary file.  */
  if (((des = path_search (tmpfile, PATH_MAX, NULL, "poke", true)) == -1)
      || ((des = mkstemp (tmpfile)) == -1))
    {
      pk_term_class ("error");
      pk_puts ("error: ");
      pk_term_end_class ("error");
      pk_puts ("determining a temporary file name.\n");
      return 0;
    }

  /* Mount the shell command.  */
  asprintf (&cmdline, "%s %s", editor, tmpfile);

  /* Start command.  */
  if (system (cmdline) != 0)
    {
      pk_term_class ("error");
      pk_puts ("error: ");
      pk_term_end_class ("error");
      pk_puts ("executing editor.\n");
      free (cmdline);
      return 0;
    }
  free (cmdline);

  /* If the editor returned success and a file got created, read the
     contents of the file, turn newlines into spaces and execute
     it.  */
  if ((fp = fopen (tmpfile, "r")) != NULL)
    {
      const int STEP = 128;
      char *newline = xmalloc (STEP);
      size_t size, i = 0;
      int c;

      for (size = STEP; (c = fgetc (fp)) != EOF; i++)
        {
          if (i % STEP == 0)
            {
              newline = xrealloc (newline, size);
              size = size + 128;
            }
          if (c == '\n')
            c = ' ';

          newline[i] = c;
        }
      newline[i] = '\0';
      fclose (fp);

      if (*newline != '\0')
        {
          pk_puts ("(poke) ");
          pk_puts (newline);
          pk_puts ("\n");
          pk_cmd_exec (newline);
        }
      free (newline);
    }

  /* Remove the temporary file.  */
  if (unlink (tmpfile) != 0)
    {
      pk_term_class ("error");
      pk_puts ("error: ");
      pk_term_end_class ("error");
      pk_printf ("removing temporary file %s\n", tmpfile);
      return 0;
    }

  return 1;
}

const struct pk_cmd editor_cmd =
  {"editor", "", "", 0, NULL, pk_cmd_editor, ".editor", NULL};
