/* pk-cmd-set.c - Commands to show and set properties.  */

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
#include <string.h>
#include <arpa/inet.h> /* For htonl */
#include <stdlib.h>
#include "xalloc.h"

#include "poke.h"
#include "pk-cmd.h"
#include "pk-utils.h"

static int
pk_cmd_set_obase (int argc, struct pk_cmd_arg argv[], uint64_t uflags)
{
  /* set obase [{2,8,10,16}] */

  assert (argc == 1);

  if (PK_CMD_ARG_TYPE (argv[0]) == PK_CMD_ARG_NULL)
    pk_printf ("%d\n", pk_obase (poke_compiler));
  else
    {
      int base = PK_CMD_ARG_INT (argv[0]);

      if (base != 10 && base != 16 && base != 2 && base != 8)
        {
          pk_term_class ("error");
          pk_puts ("error: ");
          pk_term_end_class ("error");
          pk_puts ("obase should be one of 2, 8, 10 or 16.\n");
          return 0;
        }

      pk_set_obase (poke_compiler, base);
    }

  return 1;
}

static int
pk_cmd_set_endian (int argc, struct pk_cmd_arg argv[], uint64_t uflags)
{
  /* set endian {little,big,host,network}  */

  assert (argc == 1);

  if (PK_CMD_ARG_TYPE (argv[0]) == PK_CMD_ARG_NULL)
    {
      enum pk_endian endian = pk_endian (poke_compiler);

      switch (endian)
        {
        case PK_ENDIAN_LSB: pk_puts ("little\n"); break;
        case PK_ENDIAN_MSB: pk_puts ("big\n"); break;
        default:
          assert (0);
        }
    }
  else
    {
      enum pk_endian endian;
      const char *arg = PK_CMD_ARG_STR (argv[0]);

      if (STREQ (arg, "little"))
        endian = PK_ENDIAN_LSB;
      else if (STREQ (arg, "big"))
        endian = PK_ENDIAN_MSB;
      else if (STREQ (arg, "host"))
        {
#ifdef WORDS_BIGENDIAN
          endian = PK_ENDIAN_MSB;
#else
          endian = PK_ENDIAN_LSB;
#endif
        }
      else if (STREQ (arg, "network"))
        {
          uint32_t canary = 0x11223344;

          if (canary == htonl (canary))
            {
#ifdef WORDS_BIGENDIAN
              endian = PK_ENDIAN_MSB;
#else
              endian = PK_ENDIAN_LSB;
#endif
            }
          else
            {
#ifdef WORDS_BIGENDIAN
              endian = PK_ENDIAN_LSB;
#else
              endian = PK_ENDIAN_MSB;
#endif
            }
        }
      else
        {
          pk_term_class ("error");
          pk_puts ("error: ");
          pk_term_end_class ("error");
          pk_puts ("endian should be one of `little', `big', `host' or `network'.\n");
          return 0;
        }

      pk_set_endian (poke_compiler, endian);
    }

  return 1;
}

static int
pk_cmd_set_nenc (int argc, struct pk_cmd_arg argv[], uint64_t uflags)
{
  /* set nenc {1c,2c}  */

  assert (argc == 1);

  if (PK_CMD_ARG_TYPE (argv[0]) == PK_CMD_ARG_NULL)
    {
      enum pk_nenc nenc = pk_nenc (poke_compiler);

      switch (nenc)
        {
        case PK_NENC_1: pk_puts ("1c\n"); break;
        case PK_NENC_2: pk_puts ("2c\n"); break;
        default:
          assert (0);
        }
    }
  else
    {
      enum pk_nenc nenc;
      const char *arg = PK_CMD_ARG_STR (argv[0]);

      if (STREQ (arg, "1c"))
        nenc = PK_NENC_1;
      else if (STREQ (arg, "2c"))
        nenc = PK_NENC_2;
      else
        {
          pk_term_class ("error");
          pk_puts ("error: ");
          pk_term_end_class ("error");
          pk_puts (" nenc should be one of `1c' or `2c'.\n");
          return 0;
        }

      pk_set_nenc (poke_compiler, nenc);
    }

  return 1;
}

static int
pk_cmd_set_auto_map (int argc, struct pk_cmd_arg argv[], uint64_t uflags)
{
  /* set auto-map {yes,no} */

  const char *arg;

  /* Note that it is not possible to distinguish between no argument
     and an empty unique string argument.  Therefore, argc should be
     always 1 here, and we determine when no value was specified by
     checking whether the passed string is empty or not.  */

  if (argc != 1)
    assert (0);

  arg = PK_CMD_ARG_STR (argv[0]);

  if (*arg == '\0')
    {
      if (poke_auto_map_p)
        pk_puts ("yes\n");
      else
        pk_puts ("no\n");
    }
  else
    {
      if (STREQ (arg, "yes"))
        poke_auto_map_p = 1;
      else if (STREQ (arg, "no"))
        poke_auto_map_p = 0;
      else
        {
          pk_term_class ("error");
          pk_puts ("error: ");
          pk_term_end_class ("error");
          pk_puts (" auto-map should be one of `yes' or `no'\n");
          return 0;
        }
    }

  return 1;
}

static int
pk_cmd_set_prompt_maps (int argc, struct pk_cmd_arg argv[], uint64_t uflags)
{
  /* set prompt-maps {yes,no} */

  const char *arg;

  /* Note that it is not possible to distinguish between no argument
     and an empty unique string argument.  Therefore, argc should be
     always 1 here, and we determine when no value was specified by
     checking whether the passed string is empty or not.  */

  if (argc != 1)
    assert (0);

  arg = PK_CMD_ARG_STR (argv[0]);

  if (*arg == '\0')
    {
      if (poke_prompt_maps_p)
        pk_puts ("yes\n");
      else
        pk_puts ("no\n");
    }
  else
    {
      if (STREQ (arg, "yes"))
        poke_prompt_maps_p = 1;
      else if (STREQ (arg, "no"))
        poke_prompt_maps_p = 0;
      else
        {
          pk_term_class ("error");
          pk_puts ("error: ");
          pk_term_end_class ("error");
          pk_puts (" prompt-maps should be one of `yes' or `no'\n");
          return 0;
        }
    }

  return 1;
}

static int
pk_cmd_set_pretty_print (int argc, struct pk_cmd_arg argv[], uint64_t uflags)
{
  /* set pretty-print {yes,no}  */

  const char *arg;

  /* Note that it is not possible to distinguish between no argument
     and an empty unique string argument.  Therefore, argc should be
     always 1 here, and we determine when no value was specified by
     checking whether the passed string is empty or not.  */

  if (argc != 1)
    assert (0);

  arg = PK_CMD_ARG_STR (argv[0]);

  if (*arg == '\0')
    {
      if (pk_pretty_print (poke_compiler))
        pk_puts ("yes\n");
      else
        pk_puts ("no\n");
    }
  else
    {
      int do_pretty_print;

      if (STREQ (arg, "yes"))
        do_pretty_print = 1;
      else if (STREQ (arg, "no"))
        do_pretty_print = 0;
      else
        {
          pk_term_class ("error");
          pk_puts ("error: ");
          pk_term_end_class ("error");
          pk_puts (" pretty-print should be one of `yes' or `no'.\n");
          return 0;
        }

      pk_set_pretty_print (poke_compiler, do_pretty_print);
    }

  return 1;
}

static int
pk_cmd_set_oacutoff (int argc, struct pk_cmd_arg argv[], uint64_t uflags)
{
  /* set oacutoff [CUTOFF]  */

  assert (argc == 1);

  if (PK_CMD_ARG_TYPE (argv[0]) == PK_CMD_ARG_NULL)
    pk_printf ("%d\n", pk_oacutoff (poke_compiler));
  else
    {
      int cutoff = PK_CMD_ARG_INT (argv[0]);

      if (cutoff < 0 || cutoff > 15)
        {
          pk_term_class ("error");
          pk_puts ("error: ");
          pk_term_end_class ("error");
          pk_puts (_(" cutoff should be between 0 and 15.\n"));
          return 0;
        }

      pk_set_oacutoff (poke_compiler, cutoff);
    }

  return 1;
}

static int
pk_cmd_set_odepth (int argc, struct pk_cmd_arg argv[], uint64_t uflags)
{
  /* set odepth [DEPTH]  */

  assert (argc == 1);

  if (PK_CMD_ARG_TYPE (argv[0]) == PK_CMD_ARG_NULL)
    pk_printf ("%d\n", pk_odepth (poke_compiler));
  else
    {
      int odepth = PK_CMD_ARG_INT (argv[0]);

      if (odepth < 0 || odepth > 15)
        {
          pk_term_class ("error");
          pk_puts ("error: ");
          pk_term_end_class ("error");
          pk_puts (_(" odepth should be between 0 and 15.\n"));
          return 0;
        }

      pk_set_odepth (poke_compiler, odepth);
    }

  return 1;
}

static int
pk_cmd_set_oindent (int argc, struct pk_cmd_arg argv[], uint64_t uflags)
{
  /* set oindent [INDENT]  */

  assert (argc == 1);

  if (PK_CMD_ARG_TYPE (argv[0]) == PK_CMD_ARG_NULL)
    pk_printf ("%d\n", pk_oindent (poke_compiler));
  else
    {
      int oindent = PK_CMD_ARG_INT (argv[0]);

      if (oindent < 1 || oindent > 10)
        {
          pk_term_class ("error");
          pk_puts ("error: ");
          pk_term_end_class ("error");
          pk_puts (_(" oindent should be >=1 and <= 10.\n"));
          return 0;
        }

      pk_set_oindent (poke_compiler, oindent);
    }

  return 1;
}

static int
pk_cmd_set_omaps (int argc, struct pk_cmd_arg argv[], uint64_t uflags)
{
  /* set omaps {yes|no} */

  assert (argc == 1);

  if (PK_CMD_ARG_TYPE (argv[0]) == PK_CMD_ARG_NULL)
    {
      if (pk_omaps (poke_compiler))
        pk_puts ("yes\n");
      else
        pk_puts ("no\n");
    }
  else
    {
      int omaps_p = 0;
      const char *arg = PK_CMD_ARG_STR (argv[0]);

      if (STREQ (arg, "yes"))
        omaps_p = 1;
      else if (STREQ (arg, "no"))
        omaps_p = 0;
      else
        {
          pk_term_class ("error");
          pk_puts (_("error: "));
          pk_term_end_class ("error");
          pk_puts (_(" omap should be one of `yes' or `no'.\n"));
          return 0;
        }

      pk_set_omaps (poke_compiler, omaps_p);
    }

  return 1;
}

static int
pk_cmd_set_omode (int argc, struct pk_cmd_arg argv[], uint64_t uflags)
{
  /* set omode {normal|tree}  */

  enum pk_omode omode;

  assert (argc == 1);

  if (PK_CMD_ARG_TYPE (argv[0]) == PK_CMD_ARG_NULL)
    {
      omode = pk_omode (poke_compiler);

      switch (omode)
        {
        case PK_PRINT_FLAT: pk_puts ("flat\n"); break;
        case PK_PRINT_TREE: pk_puts ("tree\n"); break;
        default:
          assert (0);
        }
    }
  else
    {
      const char *arg = PK_CMD_ARG_STR (argv[0]);

      if (STREQ (arg, "flat"))
        omode = PK_PRINT_FLAT;
      else if (STREQ (arg, "tree"))
        omode = PK_PRINT_TREE;
      else
        {
          pk_term_class ("error");
          pk_puts (_("error: "));
          pk_term_end_class ("error");
          pk_puts (_(" omode should be one of `flat' or `tree'.\n"));
          return 0;
        }

      pk_set_omode (poke_compiler, omode);
    }

  return 1;
}

static int
pk_cmd_set_error_on_warning (int argc, struct pk_cmd_arg argv[],
                             uint64_t uflags)
{
  /* set error-on-warning {yes,no} */

  const char *arg;

  /* Note that it is not possible to distinguish between no argument
     and an empty unique string argument.  Therefore, argc should be
     always 1 here, and we determine when no value was specified by
     checking whether the passed string is empty or not.  */

  if (argc != 1)
    assert (0);

  arg = PK_CMD_ARG_STR (argv[0]);

  if (*arg == '\0')
    {
      if (pk_error_on_warning (poke_compiler))
        pk_puts ("yes\n");
      else
        pk_puts ("no\n");
    }
  else
    {
      int error_on_warning;

      if (STREQ (arg, "yes"))
        error_on_warning = 1;
      else if (STREQ (arg, "no"))
        error_on_warning = 0;
      else
        {
          pk_term_class ("error");
          pk_puts ("error: ");
          pk_term_end_class ("error");
          pk_puts ("error-on-warning should be one of `yes' or `no'.\n");
          return 0;
        }

      pk_set_error_on_warning (poke_compiler, error_on_warning);
    }

  return 1;
}

static int
pk_cmd_set_doc_viewer (int argc, struct pk_cmd_arg argv[],
                       uint64_t uflags)
{
  /* set doc viewer {info,less} */

  const char *arg;

  /* Note that it is not possible to distinguish between no argument
     and an empty unique string argument.  Therefore, argc should be
     always 1 here, and we determine when no value was specified by
     checking whether the passed string is empty or not.  */

  if (argc != 1)
    assert (0);

  arg = PK_CMD_ARG_STR (argv[0]);

  if (*arg == '\0')
    {
      pk_printf ("%s\n", poke_doc_viewer);
    }
  else
    {
      if (STRNEQ (arg, "info") && STRNEQ (arg, "less"))
        {
          pk_term_class ("error");
          pk_puts ("error: ");
          pk_term_end_class ("error");
          pk_puts ("doc-viewer should be one of `info' or `less'.\n");
          return 0;
        }

      free (poke_doc_viewer);
      poke_doc_viewer = xstrdup (arg);
    }

  return 1;
}

extern struct pk_cmd null_cmd; /* pk-cmd.c  */

const struct pk_cmd set_oacutoff_cmd =
  {"oacutoff", "?i", "", 0, NULL, pk_cmd_set_oacutoff, "set oacutoff [CUTOFF]", NULL};

const struct pk_cmd set_oindent_cmd =
  {"oindent", "?i", "", 0, NULL, pk_cmd_set_oindent, "set oindent [INDENT]", NULL};

const struct pk_cmd set_odepth_cmd =
  {"odepth", "?i", "", 0, NULL, pk_cmd_set_odepth, "set odepth [DEPTH]", NULL};

const struct pk_cmd set_omode_cmd =
  {"omode", "?s", "", 0, NULL, pk_cmd_set_omode, "set omode (normal|tree)", NULL};

const struct pk_cmd set_omaps_cmd =
  {"omaps", "?s", "", 0, NULL, pk_cmd_set_omaps, "set omaps (yes|no)", NULL};

const struct pk_cmd set_obase_cmd =
  {"obase", "?i", "", 0, NULL, pk_cmd_set_obase, "set obase (2|8|10|16)", NULL};

const struct pk_cmd set_endian_cmd =
  {"endian", "?s", "", 0, NULL, pk_cmd_set_endian, "set endian (little|big|host)", NULL};

const struct pk_cmd set_nenc_cmd =
  {"nenc", "?s", "", 0, NULL, pk_cmd_set_nenc, "set nenc (1c|2c)", NULL};

const struct pk_cmd set_pretty_print_cmd =
  {"pretty-print", "s?", "", 0, NULL, pk_cmd_set_pretty_print,
   "set pretty-print (yes|no)", NULL};

const struct pk_cmd set_error_on_warning_cmd =
  {"error-on-warning", "s?", "", 0, NULL, pk_cmd_set_error_on_warning,
   "set error-on-warning (yes|no)", NULL};

const struct pk_cmd set_doc_viewer =
  {"doc-viewer", "s?", "", 0, NULL, pk_cmd_set_doc_viewer,
   "set doc-viewer (info|less)", NULL};

const struct pk_cmd set_auto_map =
  {"auto-map", "s?", "", 0, NULL, pk_cmd_set_auto_map,
   "set auto-map (yes|no)", NULL};

const struct pk_cmd set_prompt_maps =
  {"prompt-maps", "s?", "", 0, NULL, pk_cmd_set_prompt_maps,
   "set prompt-maps (yes|no)", NULL};

const struct pk_cmd *set_cmds[] =
  {
   &set_oacutoff_cmd,
   &set_obase_cmd,
   &set_omode_cmd,
   &set_omaps_cmd,
   &set_odepth_cmd,
   &set_oindent_cmd,
   &set_endian_cmd,
   &set_nenc_cmd,
   &set_pretty_print_cmd,
   &set_error_on_warning_cmd,
   &set_doc_viewer,
   &set_auto_map,
   &set_prompt_maps,
   &null_cmd
  };

static char *
set_completion_function (const char *x, int state)
{
  static int idx = 0;
  if (state == 0)
    idx = 0;
  else
    ++idx;

  int len = strlen (x);
  for (;;)
    {
      const struct pk_cmd *c = set_cmds[idx];
      if (c == &null_cmd)
        break;

      if (strncmp (c->name, x, len) == 0)
        return xstrdup (c->name);

      idx++;
    }

  return NULL;
}

struct pk_trie *set_trie;

const struct pk_cmd set_cmd =
  {"set", "", "", 0, &set_trie, NULL, "set PROPERTY", set_completion_function};
