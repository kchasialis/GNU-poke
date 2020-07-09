/* pk-cmd-map.c - Commands related with maps.  */

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

#include <assert.h>
#include <readline.h> /* For rl_filename_completion_function */

#include "poke.h"
#include "pk-cmd.h"
#include "pk-utils.h"
#include "pk-map.h"

static int
pk_cmd_map_create (int argc, struct pk_cmd_arg argv[], uint64_t uflags)
{
  /* map create MAPNAME [,#IOS] */

  int ios_id;
  const char *mapname;

  assert (argc == 2);

  assert (PK_CMD_ARG_TYPE (argv[0]) == PK_CMD_ARG_STR);
  mapname = pk_map_normalize_name (PK_CMD_ARG_STR (argv[0]));

  if (strlen (mapname) == 0)
    {
      pk_printf (_("Invalid name for map\n"));
      return 0;
    }

  if (PK_CMD_ARG_TYPE (argv[1]) == PK_CMD_ARG_NULL)
    {
      pk_ios cur_ios = pk_ios_cur (poke_compiler);

      if (!cur_ios)
        {
          pk_printf (_("No current IOS\n"));
          return 0;
        }

      ios_id = pk_ios_get_id (cur_ios);
    }
  else
    {
      ios_id = PK_CMD_ARG_TAG (argv[1]);
      if (pk_ios_search_by_id (poke_compiler, ios_id) == NULL)
        {
          pk_printf (_("No such IOS #%d\n"), ios_id);
          return 0;
        }
    }

  if (!pk_map_create (ios_id, mapname, NULL /* source */))
    {
      pk_printf (_("The map `%s' already exists in IOS #%d\n"),
                 mapname, ios_id);
      return 0;
    }

  return 1;
}

static int
pk_cmd_map_remove (int argc, struct pk_cmd_arg argv[], uint64_t uflags)
{
  /* map remove MAPNAME [,#IOS] */

  int ios_id;
  const char *mapname;

  assert (argc == 2);

  assert (PK_CMD_ARG_TYPE (argv[0]) == PK_CMD_ARG_STR);
  mapname = PK_CMD_ARG_STR (argv[0]);

  if (strlen (mapname) == 0)
    {
      pk_printf (_("Invalid name for map\n"));
      return 0;
    }

  if (PK_CMD_ARG_TYPE (argv[1]) == PK_CMD_ARG_NULL)
    ios_id = pk_ios_get_id (pk_ios_cur (poke_compiler));
  else
    {
      ios_id = PK_CMD_ARG_TAG (argv[1]);
      if (pk_ios_search_by_id (poke_compiler, ios_id) == NULL)
        {
          pk_printf (_("No such IOS #%d\n"), ios_id);
          return 0;
        }
    }

  if (!pk_map_remove (ios_id, mapname))
    {
      pk_printf (_("No such map `%s' in IOS #%d\n"),
                 mapname, ios_id);
      return 0;
    }

  return 1;
}

static int
pk_cmd_map_show (int argc, struct pk_cmd_arg argv[], uint64_t uflags)
{
  /* map show MAPNAME [,#IOS] */

  int ios_id;
  const char *mapname;
  pk_map map;

  /* Get arguments.  */
  assert (argc == 2);
  assert (PK_CMD_ARG_TYPE (argv[0]) == PK_CMD_ARG_STR);

  if (PK_CMD_ARG_TYPE (argv[1]) == PK_CMD_ARG_NULL)
    ios_id = pk_ios_get_id (pk_ios_cur (poke_compiler));
  else
    {
      ios_id = PK_CMD_ARG_TAG (argv[1]);
      if (pk_ios_search_by_id (poke_compiler, ios_id) == NULL)
        {
          pk_printf (_("No such IOS #%d\n"), ios_id);
          return 0;
        }
    }

  mapname = PK_CMD_ARG_STR (argv[0]);

  /* Make sure the map exists.  */
  map = pk_map_search (ios_id, mapname);
  if (!map)
    {
      pk_printf (_("No such map `%s' in IOS #%d\n"),
                 mapname, ios_id);
      return 0;
    }

  /* Print out this map entries.  */
  pk_term_class ("table_header");
  pk_puts ("Offset\t\tEntry\n");
  pk_term_end_class ("table_header");

  {
    pk_map_entry entry;

    for (entry = PK_MAP_ENTRIES (map);
         entry;
         entry = PK_MAP_ENTRY_CHAIN (entry))
      {
        pk_print_val (poke_compiler,
                      PK_MAP_ENTRY_OFFSET (entry));
        pk_printf ("\t\t%s::%s\n",
                   mapname,
                   PK_MAP_ENTRY_NAME (entry));
      }
  }

  return 1;
}

static int
pk_cmd_map_entry_add (int argc, struct pk_cmd_arg argv[], uint64_t uflags)
{
  /* map entry add MAPNAME, VARNAME [,#IOS]  */

  int ios_id;
  const char *mapname;
  const char *varname;
  pk_val val;

  /* Get arguments.  */
  assert (argc == 3);
  assert (PK_CMD_ARG_TYPE (argv[0]) == PK_CMD_ARG_STR);
  assert (PK_CMD_ARG_TYPE (argv[1]) == PK_CMD_ARG_STR);

  mapname = PK_CMD_ARG_STR (argv[0]);
  varname = PK_CMD_ARG_STR (argv[1]);

  if (PK_CMD_ARG_TYPE (argv[2]) == PK_CMD_ARG_NULL)
    ios_id = pk_ios_get_id (pk_ios_cur (poke_compiler));
  else
    {
      ios_id = PK_CMD_ARG_TAG (argv[2]);
      if (pk_ios_search_by_id (poke_compiler, ios_id) == NULL)
        {
          pk_printf (_("No such IOS #%d\n"), ios_id);
          return 0;
        }
    }

  /* Make sure the specified map exists in the given IO space.  */
  if (!pk_map_search (ios_id, mapname))
    {
      pk_printf (_("No such map `%s' in IOS #%d\n"),
                 mapname, ios_id);
      return 0;
    }

  /* Make sure the variable exists in the top-level environment.  */
  if (!pk_decl_p (poke_compiler, varname, PK_DECL_KIND_VAR))
    {
      pk_printf ("Variable `%s' doesn't exist\n", varname);
      return 0;
    }

  /* Make sure that the variable is mapped, and that it is mapped in
     this very precise IO space.  */
  val = pk_decl_val (poke_compiler, varname);
  assert (val != PK_NULL);

  if (!pk_val_mapped_p (val)
      || pk_int_value (pk_val_ios (val)) != ios_id)
    {
      pk_printf ("Variable `%s' is not mapped in the IOS #%d\n",
                 varname, ios_id);
      return 0;
    }

  /* Ok, add the entry.  */
  if (!pk_map_add_entry (ios_id, mapname,
                         varname, varname, pk_val_offset (val)))
    {
      pk_printf ("The entry `%s' already exists in map `%s'\n",
                 varname, mapname);
      return 0;
    }

  return 1;
}

static int
pk_cmd_map_entry_remove (int argc, struct pk_cmd_arg argv[], uint64_t uflags)
{
  /* map entry remove MAPNAME, VARNAME [,#IOS]  */

  int ios_id;
  const char *mapname;
  const char *entryname;

  /* Get arguments.  */
  assert (argc == 3);
  assert (PK_CMD_ARG_TYPE (argv[0]) == PK_CMD_ARG_STR);
  assert (PK_CMD_ARG_TYPE (argv[1]) == PK_CMD_ARG_STR);

  mapname = PK_CMD_ARG_STR (argv[0]);
  entryname = PK_CMD_ARG_STR (argv[1]);

  if (PK_CMD_ARG_TYPE (argv[2]) == PK_CMD_ARG_NULL)
    ios_id = pk_ios_get_id (pk_ios_cur (poke_compiler));
  else
    {
      ios_id = PK_CMD_ARG_TAG (argv[2]);
      if (pk_ios_search_by_id (poke_compiler, ios_id) == NULL)
        {
          pk_printf (_("No such IOS #%d\n"), ios_id);
          return 0;
        }
    }

  /* Make sure the specified map exists in the given IO space.  */
  if (!pk_map_search (ios_id, mapname))
    {
      pk_printf (_("No such map `%s' in IOS #%d\n"),
                 mapname, ios_id);
      return 0;
    }

  if (!pk_map_remove_entry (ios_id, mapname, entryname))
    {
      pk_printf (_("no entry `%s' in map `%s'\n"),
                 entryname, mapname);
      return 0;
    }

  return 1;
}

static int
pk_cmd_map_load (int argc, struct pk_cmd_arg argv[], uint64_t uflags)
{
  /* map load MAPNAME [,#IOS] */

  int ios_id, filename_p;
  const char *mapname, *filename;
  char *emsg;

  assert (argc == 2);
  assert (PK_CMD_ARG_TYPE (argv[0]) == PK_CMD_ARG_STR);
  mapname = PK_CMD_ARG_STR (argv[0]);

  if (PK_CMD_ARG_TYPE (argv[1]) == PK_CMD_ARG_NULL)
    ios_id = pk_ios_get_id (pk_ios_cur (poke_compiler));
  else
    {
      ios_id = PK_CMD_ARG_TAG (argv[1]);
      if (pk_ios_search_by_id (poke_compiler, ios_id) == NULL)
        {
          pk_printf (_("No such IOS #%d\n"), ios_id);
          return 0;
        }
    }

  filename_p = (mapname[0] == '.' || mapname[0] == '/');
  filename = pk_map_resolve_map (mapname, filename_p);
  if (!filename)
    {
      pk_printf (_("No such map `%s'\n"), mapname);
      return 0;
    }

  if (!pk_map_load_file (ios_id, filename, &emsg))
    {
      if (emsg)
        {
          pk_printf ("%s", emsg);
          if (emsg[strlen (emsg) - 1] != '\n')
            pk_puts ("\n");
        }
      return 0;
    }

  return 1;
}

static int
pk_cmd_map_save (int argc, struct pk_cmd_arg argv[], uint64_t uflags)
{
  /* XXX writeme */
  return 1;
}

static int
pk_cmd_info_maps (int argc, struct pk_cmd_arg argv[], uint64_t uflags)
{
  /* info maps [,#IOS] */
  int ios_id;
  pk_map maps, map;

  assert (argc == 1);

  if (PK_CMD_ARG_TYPE (argv[0]) == PK_CMD_ARG_NULL)
    ios_id = pk_ios_get_id (pk_ios_cur (poke_compiler));
  else
    {
      ios_id = PK_CMD_ARG_TAG (argv[0]);
      if (pk_ios_search_by_id (poke_compiler, ios_id) == NULL)
        {
          pk_printf (_("No such IOS #%d\n"), ios_id);
          return 0;
        }
    }

  maps = pk_map_get_maps (ios_id);
  if (maps)
    {
      pk_term_class ("table_header");
      pk_puts ("IOS\tName\t\tSource\n");
      pk_term_end_class ("table_header");
    }

  for (map = maps; map; map = PK_MAP_CHAIN (map))
    pk_printf ("#%d\t%s\t\t%s\n", ios_id, PK_MAP_NAME (map),
               PK_MAP_SOURCE (map) ? PK_MAP_SOURCE (map) : "<stdin>");

  return 1;
}

static char *
map_completion_function (const char *x, int state)
{
  return pk_ios_completion_function (poke_compiler, x, state);
}

const struct pk_cmd map_entry_add_cmd =
  {"add", "s,s,?t", 0, 0, NULL, pk_cmd_map_entry_add, "add MAPNAME, VARNAME [,#IOS]",
   NULL};

const struct pk_cmd map_entry_remove_cmd =
  {"remove", "s,s,?t", 0, 0, NULL, pk_cmd_map_entry_remove, "remove MAPNAME, VARNAME [,#IOS]",
   NULL};

extern struct pk_cmd null_cmd; /* pk-cmd.c  */

const struct pk_cmd *map_entry_cmds[] =
{
  &map_entry_add_cmd,
  &map_entry_remove_cmd,
  &null_cmd,
};

struct pk_trie *map_trie;
struct pk_trie *map_entry_trie;

const struct pk_cmd map_entry_cmd =
  {"entry", "", 0, 0, &map_entry_trie, NULL, "map entry (add|remove)", NULL};

const struct pk_cmd map_create_cmd =
  {"create", "s,?t", 0, 0, NULL, pk_cmd_map_create, "create MAPNAME [,#IOS]",
   NULL};

const struct pk_cmd map_remove_cmd =
  {"remove", "s,?t", 0, 0, NULL, pk_cmd_map_remove, "remove MAPNAME [,#IOS]",
   NULL};

const struct pk_cmd map_show_cmd =
  {"show", "s,?t", 0, 0, NULL, pk_cmd_map_show, "show MAPNAME [,#IOS]",
   NULL};

const struct pk_cmd map_load_cmd =
  {"load", "s,?t", 0, PK_CMD_F_REQ_IO, NULL, pk_cmd_map_load, "load MAPNAME [,#IOS]",
   /* XXX use a completion function for maps.  */
   rl_filename_completion_function};

const struct pk_cmd map_save_cmd =
  {"save", "?f", 0, 0, NULL, pk_cmd_map_save, "save [FILENAME]",
   rl_filename_completion_function};

const struct pk_cmd *map_cmds[] =
{
  &map_create_cmd,
  &map_remove_cmd,
  &map_show_cmd,
  &map_load_cmd,
  &map_save_cmd,
  &map_entry_cmd,
  &null_cmd,
};

const struct pk_cmd map_cmd =
  {"map", "", "", 0, &map_trie, NULL, "map (create|show|entry|load|save)",
   NULL};

const struct pk_cmd info_maps_cmd =
  {"maps", "?t", "", PK_CMD_F_REQ_IO, NULL, pk_cmd_info_maps, "info maps",
   map_completion_function};
