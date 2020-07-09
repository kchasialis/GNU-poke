/* pk-cmd-ios.c - Commands for operating on IO spaces.  */

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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <readline.h>
#include "xalloc.h"

#include "poke.h"
#include "pk-cmd.h"
#include "pk-map.h"
#include "pk-utils.h"
#include "pk-ios.h"
#if HAVE_HSERVER
#  include "pk-hserver.h"
#endif

static int
pk_cmd_ios (int argc, struct pk_cmd_arg argv[], uint64_t uflags)
{
  /* ios #ID */

  int io_id;
  pk_ios io;

  assert (argc == 1);
  assert (PK_CMD_ARG_TYPE (argv[0]) == PK_CMD_ARG_TAG);

  io_id = PK_CMD_ARG_TAG (argv[0]);
  io = pk_ios_search_by_id (poke_compiler, io_id);
  if (io == NULL)
    {
      pk_printf (_("No IOS with tag #%d\n"), io_id);
      return 0;
    }

  pk_ios_set_cur (poke_compiler, io);

  if (poke_interactive_p && !poke_quiet_p)
    pk_printf (_("The current IOS is now `%s'.\n"),
               pk_ios_handler (pk_ios_cur (poke_compiler)));
  return 1;
}

static int
pk_cmd_file (int argc, struct pk_cmd_arg argv[], uint64_t uflags)
{
  /* file FILENAME */

  assert (argc == 1);
  assert (PK_CMD_ARG_TYPE (argv[0]) == PK_CMD_ARG_STR);

  /* Create a new IO space.  */
  const char *arg_str = PK_CMD_ARG_STR (argv[0]);
  const char *filename = arg_str;

  if (access (filename, R_OK) != 0)
    {
      char *why = strerror (errno);
      pk_printf (_("%s: file cannot be read: %s\n"), arg_str, why);
      return 0;
    }

  if (pk_ios_search (poke_compiler, filename) != NULL)
    {
      printf (_("File %s already opened.  Use `.ios #N' to switch.\n"),
              filename);
      return 0;
    }

  if (PK_IOS_ERROR == pk_open_ios (filename, 1 /* set_cur_p */))
    {
      pk_term_class ("error");
      pk_puts ("error: ");
      pk_term_end_class ("error");

      pk_printf (_("opening %s\n"), filename);
      return 0;
    }

  return 1;
}

static int
pk_cmd_close (int argc, struct pk_cmd_arg argv[], uint64_t uflags)
{
  /* close [#ID]  */
  pk_ios io;
  int changed;

  assert (argc == 1);

  if (PK_CMD_ARG_TYPE (argv[0]) == PK_CMD_ARG_NULL)
    io = pk_ios_cur (poke_compiler);
  else
    {
      int io_id = PK_CMD_ARG_TAG (argv[0]);

      io = pk_ios_search_by_id (poke_compiler, io_id);
      if (io == NULL)
        {
          pk_printf (_("No such file #%d\n"), io_id);
          return 0;
        }
    }

  changed = (io == pk_ios_cur (poke_compiler));
  pk_ios_close (poke_compiler, io);

  if (changed)
    {
      if (pk_ios_cur (poke_compiler) == NULL)
        puts (_("No more IO spaces."));
      else
        {
          if (poke_interactive_p && !poke_quiet_p)
            pk_printf (_("The current file is now `%s'.\n"),
                       pk_ios_handler (pk_ios_cur (poke_compiler)));
        }
    }

  return 1;
}

static void
print_info_ios (pk_ios io, void *data)
{
  uint64_t flags = pk_ios_flags (io);
  char mode[3];
  mode[0] = flags & PK_IOS_F_READ ? 'r' : ' ';
  mode[1] = flags & PK_IOS_F_WRITE ? 'w' : ' ';
  mode[2] = '\0';

  pk_printf ("%s#%d\t%s\t",
             io == pk_ios_cur (poke_compiler) ? "* " : "  ",
             pk_ios_get_id (io),
             mode);

#if HAVE_HSERVER
  {
    char *cmd;
    char *hyperlink;

    asprintf (&cmd, "0x%08jx#B", pk_ios_size (io) / 8);
    hyperlink = pk_hserver_make_hyperlink ('i', cmd);
    free (cmd);

    pk_term_hyperlink (hyperlink, NULL);
    pk_printf ("0x%08jx#B", pk_ios_size (io) / 8);
    pk_term_end_hyperlink ();

    free (hyperlink);
  }
#else
  pk_printf ("0x%08jx#B", pk_ios_size (io) / 8);
#endif
  pk_puts ("\t");

#if HAVE_HSERVER
  {
    char *cmd;
    char *hyperlink;

    asprintf (&cmd, ".ios #%d", pk_ios_get_id (io));
    hyperlink = pk_hserver_make_hyperlink ('e', cmd);
    free (cmd);

    pk_term_hyperlink (hyperlink, NULL);
    pk_puts (pk_ios_handler (io));
    pk_term_end_hyperlink ();

    free (hyperlink);
  }
#else
  pk_puts (pk_ios_handler (io));
#endif

  pk_puts ("\n");
}

static int
pk_cmd_info_ios (int argc, struct pk_cmd_arg argv[], uint64_t uflags)
{
  assert (argc == 0);

  pk_printf (_("  Id\tMode\tSize\t\tName\n"));
  pk_ios_map (poke_compiler, print_info_ios, NULL);

  return 1;
}

static int
pk_cmd_load_file (int argc, struct pk_cmd_arg argv[], uint64_t uflags)
{
  /* load FILENAME */

  char *arg;
  char *filename = NULL;
  char *emsg;

  assert (argc == 1);
  arg = PK_CMD_ARG_STR (argv[0]);

  if ((emsg = pk_file_readable (arg)) == NULL)
    filename = arg;
  else if (arg[0] != '/')
    {
      /* Try to open the specified file relative to POKEDATADIR.  */
      if (asprintf (&filename, "%s/%s", poke_datadir, arg) == -1)
        {
          /* filename is undefined now, don't free */
          pk_puts ("Out of memory");
          return 0;
        }

      if ((emsg = pk_file_readable (filename)) == NULL)
        goto no_file;
    }
  else
    goto no_file;

  if (!pk_compile_file (poke_compiler, filename, NULL /* exit_status */))
    /* Note that the compiler emits its own error messages.  */
    goto error;

  if (filename != arg)
    free (filename);
  return 1;

 no_file:
  pk_puts (emsg);
 error:
  if (filename != arg)
    free (filename);
  return 0;
}

static int
pk_cmd_mem (int argc, struct pk_cmd_arg argv[], uint64_t uflags)
{
  /* mem NAME */

  assert (argc == 1);
  assert (PK_CMD_ARG_TYPE (argv[0]) == PK_CMD_ARG_STR);

  /* Create a new memory IO space.  */
  const char *arg_str = PK_CMD_ARG_STR (argv[0]);
  char *mem_name;

  if (asprintf (&mem_name, "*%s*", arg_str) == -1)
    {
      pk_puts (_("Out of memory"));
      return 0;
    }

  if (pk_ios_search (poke_compiler, mem_name) != NULL)
    {
      printf (_("Buffer %s already opened.  Use `.ios #N' to switch.\n"),
              mem_name);
      free (mem_name);
      return 0;
    }

  if (PK_IOS_ERROR == pk_ios_open (poke_compiler, mem_name, 0, 1))
    {
      pk_printf (_("Error creating memory IOS %s\n"), mem_name);
      free (mem_name);
      return 0;
    }

  free (mem_name);

  if (poke_interactive_p && !poke_quiet_p)
    pk_printf (_("The current IOS is now `%s'.\n"),
               pk_ios_handler (pk_ios_cur (poke_compiler)));

  return 1;
}

#ifdef HAVE_LIBNBD
static int
pk_cmd_nbd (int argc, struct pk_cmd_arg argv[], uint64_t uflags)
{
  /* nbd URI */

  assert (argc == 1);
  assert (PK_CMD_ARG_TYPE (argv[0]) == PK_CMD_ARG_STR);

  /* Create a new NBD IO space.  */
  const char *arg_str = PK_CMD_ARG_STR (argv[0]);
  char *nbd_name = xstrdup (arg_str);

  if (pk_ios_search (poke_compiler, nbd_name) != NULL)
    {
      printf (_("Buffer %s already opened.  Use `.ios #N' to switch.\n"),
              nbd_name);
      free (nbd_name);
      return 0;
    }

  if (PK_IOS_ERROR == pk_ios_open (poke_compiler, nbd_name, 0, 1))
    {
      pk_printf (_("Error creating NBD IOS %s\n"), nbd_name);
      free (nbd_name);
      return 0;
    }

  if (poke_interactive_p && !poke_quiet_p)
    pk_printf (_("The current IOS is now `%s'.\n"),
               pk_ios_handler (pk_ios_cur (poke_compiler)));

  return 1;
}
#endif /* HAVE_LIBNBD */

static char *
ios_completion_function (const char *x, int state)
{
  return pk_ios_completion_function (poke_compiler, x, state);
}

const struct pk_cmd ios_cmd =
  {"ios", "t", "", 0, NULL, pk_cmd_ios, "ios #ID", ios_completion_function};

const struct pk_cmd file_cmd =
  {"file", "f", "", 0, NULL, pk_cmd_file, "file FILE-NAME", rl_filename_completion_function};

const struct pk_cmd mem_cmd =
  {"mem", "s", "", 0, NULL, pk_cmd_mem, "mem NAME", NULL};

#ifdef HAVE_LIBNBD
const struct pk_cmd nbd_cmd =
  {"nbd", "s", "", 0, NULL, pk_cmd_nbd, "nbd URI", NULL};
#endif

const struct pk_cmd close_cmd =
  {"close", "?t", "", PK_CMD_F_REQ_IO, NULL, pk_cmd_close, "close [#ID]", ios_completion_function};

const struct pk_cmd info_ios_cmd =
  {"ios", "", "", 0, NULL, pk_cmd_info_ios, "info ios", NULL};

const struct pk_cmd load_cmd =
  {"load", "f", "", 0, NULL, pk_cmd_load_file, "load FILE-NAME", rl_filename_completion_function};
