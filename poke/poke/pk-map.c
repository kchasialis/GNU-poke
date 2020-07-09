/* pk-map.c - Support for map files.  */

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

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <xalloc.h>
#include <assert.h>
#include "dirname.h"

#include "poke.h"
#include "pk-utils.h"
#include "pk-term.h"
#include "pk-map.h"
#include "pk-map-parser.h"

/* Unique map ID.
   This number is unique per map in a poke session.  */

static uint64_t next_map_id;

/* Maps for a given IOS.

   IOS_ID is the identifier of the ios.
   MAPS is a list of chained maps.
   CHAIN is a pointer to another next map_ios, or NULL.  */

#define PK_MAP_IOS_ID(MAP_IOS) ((MAP_IOS)->ios_id)
#define PK_MAP_IOS_MAPS(MAP_IOS) ((MAP_IOS)->maps)
#define PK_MAP_IOS_CHAIN(MAP_IOS) ((MAP_IOS)->chain)

struct pk_map_ios
{
  int ios_id;
  struct pk_map *maps;
  struct pk_map_ios *chain;
};

typedef struct pk_map_ios *pk_map_ios;

/* Global containing the IOS maps defined in the running poke.  */
static struct pk_map_ios *poke_maps;

static pk_map_ios
search_map_ios (int ios_id)
{
  pk_map_ios map_ios;

  for (map_ios = poke_maps;
       map_ios;
       map_ios = PK_MAP_IOS_CHAIN (map_ios))
    {
      if (PK_MAP_IOS_ID (map_ios) == ios_id)
        break;
    }

  return map_ios;
}

static void
free_entry (pk_map_entry entry)
{
  free (PK_MAP_ENTRY_VARNAME (entry));
  free (entry);
}

static void
free_map (pk_map map)
{
  pk_map_entry entry, tmp;

  for (entry = PK_MAP_ENTRIES (map); entry; entry = tmp)
    {
      tmp = PK_MAP_CHAIN (entry);
      free_entry (entry);
    }

  free (PK_MAP_NAME (map));
  free (PK_MAP_SOURCE (map));
  free (map);
}

static pk_map
search_map (pk_map_ios map_ios, const char *mapname)
{
  pk_map map;

  for (map = PK_MAP_IOS_MAPS (map_ios);
       map;
       map = PK_MAP_CHAIN (map))
    {
      if (STREQ (PK_MAP_NAME (map), mapname))
        break;
    }

  return map;
}

static pk_map_entry
search_map_entry (pk_map map, const char *name)
{
  pk_map_entry map_entry;

  for (map_entry = PK_MAP_ENTRIES (map);
       map_entry;
       map_entry = PK_MAP_ENTRY_CHAIN (map_entry))
    {
      if (STREQ (PK_MAP_ENTRY_NAME (map_entry), name))
        break;
    }

  return map_entry;
}

static char *
entry_name_to_varname (const char *name)
{
  char *varname;

  if (asprintf (&varname, "__map_entry_%lu_%s", next_map_id, name) == -1)
    pk_fatal ("out of memory");
  return varname;
}

static char *
pk_map_alien_token_handler (const char *id, char **errmsg)
{
  char *map_name = NULL;
  char *entry_name = NULL;
  pk_ios cur_ios;

  *errmsg = NULL;

  /* No point on going ahead if there is no current IOS.  */
  cur_ios = pk_ios_cur (poke_compiler);
  if (!cur_ios)
    goto error;

  /* The format of the identifier should be:

     $MAPNAME::ENTRYNAME

     i.e. the identifier should have two components.  Verify this
     holds for ID, and extract the fields.  */

  entry_name = strstr (id, "::");
  if (!entry_name || strstr (entry_name + 2, "::"))
    goto error;

  if (entry_name)
    {
      pk_map map;
      int ios_id;

      map_name = xmalloc (entry_name - id + 1);
      strncpy (map_name, id, entry_name - id);
      map_name[entry_name - id] = '\0';
      entry_name += 2;
      ios_id = pk_ios_get_id (cur_ios);

      map = pk_map_search (ios_id, map_name);
      if (map)
        {
          pk_map_entry entry;

          for (entry = PK_MAP_ENTRIES (map);
               entry;
               entry = PK_MAP_ENTRY_CHAIN (entry))
            {
              if (STREQ (PK_MAP_ENTRY_NAME (entry), entry_name))
                return xstrdup (PK_MAP_ENTRY_VARNAME (entry));
            }
        }

      free (map_name);

    }

 error:
  *errmsg = xstrdup ("invalid map entry");
  return NULL;
}

void
pk_map_init (void)
{
  poke_maps = NULL;

  /* Install the handler for alien variables that recognizes map
     entries.  */
  pk_set_alien_token_fn (poke_compiler, pk_map_alien_token_handler);
}

void
pk_map_shutdown (void)
{
  struct pk_map_ios *map_ios, *next_map_ios;

  for (map_ios = poke_maps; map_ios; map_ios = next_map_ios)
    {
      struct pk_map *map, *next_map;

      for (map = PK_MAP_IOS_MAPS (map_ios); map; map = next_map)
        {
          struct pk_map_entry *entry, *next_entry;

          for (entry = PK_MAP_ENTRIES (map);
               entry;
               entry = next_entry)
            {
              next_entry = PK_MAP_ENTRY_CHAIN (entry);
              free (entry->varname);
              free (entry);
            }

          next_map = PK_MAP_CHAIN (map);
          free (map->name);
          free (map);
        }

      next_map_ios = PK_MAP_IOS_CHAIN (map_ios);
      free (map_ios);
    }

  poke_maps = NULL;
}

int
pk_map_create (int ios_id, const char *mapname,
               const char *source)
{
  pk_map_ios map_ios;

  /* Search for the right map_ios in poke_maps.  */
  map_ios = search_map_ios (ios_id);

  /* If there is not a map_ios entry for this IO space, create
     one.  */
  if (!map_ios)
    {
      map_ios = xmalloc (sizeof (struct pk_map_ios));
      PK_MAP_IOS_ID (map_ios) = ios_id;
      PK_MAP_IOS_MAPS (map_ios) = NULL;

      PK_MAP_IOS_CHAIN (map_ios) = poke_maps;
      poke_maps = map_ios;
    }

  /* Create a new map and add it to the chain of maps in the
     map_ios.  */
  {
    pk_map maps, map;

    maps = pk_map_get_maps (ios_id);

    /* Make sure there is not already a map with the given name.  */
    for (map = maps; map; map = PK_MAP_CHAIN (map))
      {
        if (STREQ (PK_MAP_NAME (map), mapname))
          return 0;
      }

    /* Create an empty map and add it to the sequence.  */
    map = xmalloc (sizeof (struct pk_map));
    PK_MAP_ID (map) = next_map_id++;
    PK_MAP_NAME (map) = xstrdup (mapname);
    if (source)
      PK_MAP_SOURCE (map) = xstrdup (source);
    else
      PK_MAP_SOURCE (map) = NULL;
    PK_MAP_ENTRIES (map) = NULL;

    PK_MAP_CHAIN (map) = PK_MAP_IOS_MAPS (map_ios);
    PK_MAP_IOS_MAPS (map_ios) = map;
  }

  return 1;
}

int
pk_map_remove (int ios_id, const char *mapname)
{
  pk_map_ios map_ios;
  pk_map prev, map;

  /* Search for the right map_ios in poke_maps.  */
  map_ios = search_map_ios (ios_id);

  if (!map_ios)
    return 0;

  for (prev = NULL, map = PK_MAP_IOS_MAPS (map_ios);
       map;
       prev = map, map = PK_MAP_CHAIN (map))
    {
      if (STREQ (PK_MAP_NAME (map), mapname))
        break;
    }

  if (map == NULL)
    /* Map not found.  */
    return 0;

  if (prev)
    PK_MAP_CHAIN (prev) = PK_MAP_CHAIN (map);
  else
    PK_MAP_IOS_MAPS (map_ios) = PK_MAP_CHAIN (map);

  free_map (map);

  return 1;
}

pk_map
pk_map_search (int ios_id, const char *name)
{
  pk_map maps = pk_map_get_maps (ios_id);

  if (maps)
    {
      pk_map map;

      for (map = maps; map; map = PK_MAP_CHAIN (map))
        if (STREQ (PK_MAP_NAME (map), name))
          return map;
    }

  return NULL;
}

int
pk_map_add_entry (int ios_id, const char *mapname,
                  const char *name, const char *varname,
                  pk_val offset)
{
  pk_map_ios map_ios;
  pk_map map;
  pk_map_entry entry;

  map_ios = search_map_ios (ios_id);
  if (!map_ios)
    return 0;

  map = search_map (map_ios, mapname);
  if (!map)
    return 0;

  entry = search_map_entry (map, name);
  if (entry)
    return 0;

  /* Create a new entry and chain it in the map.  The entries are kept
     sorted by offset.  */
  entry = xmalloc (sizeof (struct pk_map_entry));
  PK_MAP_ENTRY_NAME (entry) = xstrdup (name);
  PK_MAP_ENTRY_VARNAME (entry) = xstrdup (varname);
  PK_MAP_ENTRY_OFFSET (entry) = offset;

  if (PK_MAP_ENTRIES (map) == NULL)
    {
      PK_MAP_ENTRY_CHAIN (entry) = NULL;
      PK_MAP_ENTRIES (map) = entry;
    }
  else
    {
      pk_map_entry e, p;
      uint64_t offset_bits
        = (pk_uint_value (pk_offset_magnitude (offset))
           * pk_uint_value (pk_offset_unit (offset)));

      for (p = NULL, e = PK_MAP_ENTRIES (map);
           e;
           p = e, e = PK_MAP_ENTRY_CHAIN (e))
        {
          pk_val e_offset = PK_MAP_ENTRY_OFFSET (e);

          if ((pk_uint_value (pk_offset_magnitude (e_offset))
               * pk_uint_value (pk_offset_unit (e_offset)))
              > offset_bits)
            break;
        }


      PK_MAP_ENTRY_CHAIN (entry) = e;
      if (p)
        PK_MAP_ENTRY_CHAIN (p) = entry;
      else
        PK_MAP_ENTRIES (map) = entry;
    }

  return 1;
}

int
pk_map_remove_entry (int ios_id, const char *mapname,
                     const char *entryname)
{
  pk_map_ios map_ios;
  pk_map map;
  pk_map_entry entry, prev;

  map_ios = search_map_ios (ios_id);
  if (!map_ios)
    return 0;

  map = search_map (map_ios, mapname);
  if (!map)
    return 0;

  for (prev = NULL, entry = PK_MAP_ENTRIES (map);
       entry;
       prev = entry, entry = PK_MAP_ENTRY_CHAIN (entry))
    {
      if (STREQ (PK_MAP_ENTRY_NAME (entry), entryname))
        {
          if (prev)
            PK_MAP_ENTRY_CHAIN (prev) = PK_MAP_ENTRY_CHAIN (entry);
          else
            PK_MAP_ENTRIES (map) = PK_MAP_ENTRY_CHAIN (entry);

          free_entry (entry);
          return 1;
        }
    }

  return 0;
}

pk_map
pk_map_get_maps (int ios_id)
{
  pk_map_ios map_ios;

  for (map_ios = poke_maps;
       map_ios;
       map_ios = PK_MAP_IOS_CHAIN (map_ios))
    {
      if (PK_MAP_IOS_ID (map_ios) == ios_id)
        return PK_MAP_IOS_MAPS (map_ios);
    }

  return NULL;
}

static int
pk_map_load_parsed_map (int ios_id, const char *mapname,
                        const char *filename,
                        pk_map_parsed_map map)
{
  pk_map_parsed_entry entry;

  /* First, compile the prologue.  */
  /* XXX set error location and disable verbose error messages in
     poke_compiler.  */
  if (!pk_compile_buffer (poke_compiler,
                          PK_MAP_PARSED_MAP_PROLOGUE (map),
                          NULL))
    return 0;

  /* Process the map entries and create the mapped global
     variables.  */

  for (entry = PK_MAP_PARSED_MAP_ENTRIES (map);
       entry;
       entry = PK_MAP_PARSED_ENTRY_CHAIN (entry))
    {
      pk_val val;
      int process_p = 1;
      const char *condition = PK_MAP_PARSED_ENTRY_CONDITION (entry);

      /* Evaluate the condition.  */
      if (condition)
        {
          /* XXX set error location... */
          if (!pk_compile_expression (poke_compiler,
                                      condition,
                                      NULL /* end */,
                                      &val))
            goto error;

          if (pk_type_code (pk_typeof (val)) != PK_INT
              && pk_type_code (pk_typeof (val)) != PK_UINT)
            {
              /* XXX error location.  */
              pk_printf ("error: invalid condition expression\n");
              goto error;
            }

          if (!(pk_type_code (pk_typeof (val)) == PK_INT
                ? pk_int_value (val) : pk_uint_value (val)))
            {
              process_p = 0;
              /* Process this entry.  */
              printf ("SKIPPING ENTRY\n");
            }
        }

      PK_MAP_PARSED_ENTRY_SKIPPED_P (entry) = !process_p;
      if (process_p)
        {
          const char *name = PK_MAP_PARSED_ENTRY_NAME (entry);
          const char *type = PK_MAP_PARSED_ENTRY_TYPE (entry);
          const char *offset = PK_MAP_PARSED_ENTRY_OFFSET (entry);

          PK_MAP_PARSED_ENTRY_VARNAME (entry)
            = entry_name_to_varname (name);

          /* XXX set error location with compiler pragmas... */
          char *defvar_str
            = pk_str_concat ("defvar ",
                             PK_MAP_PARSED_ENTRY_VARNAME (entry),
                             " = ", type, " @ ", offset, ";", NULL);

          /* XXX what about constraints?  */
          if (!pk_compile_buffer (poke_compiler,
                                  defvar_str,
                                  NULL /* end */))
            goto error;
        }
    }

  /* Create the map.  */
  if (!pk_map_create (ios_id, mapname, filename))
    return 0;

  /* Add the map entries.  */
  for (entry = PK_MAP_PARSED_MAP_ENTRIES (map);
       entry;
       entry = PK_MAP_PARSED_ENTRY_CHAIN (entry))
    {
      if (!PK_MAP_PARSED_ENTRY_SKIPPED_P (entry))
        {
          char *name = PK_MAP_PARSED_ENTRY_NAME (entry);
          char *varname = PK_MAP_PARSED_ENTRY_VARNAME (entry);
          pk_val offset;

          offset = pk_decl_val (poke_compiler, varname);
          assert (offset != PK_NULL);
          offset = pk_val_offset (offset);

          if (!pk_map_add_entry (ios_id, mapname, name,
                                 varname, offset))
            goto error;
        }
    }

  return 1;

 error:
  return 0;
}

char *
pk_map_normalize_name (const char *str)
{
  char *mapname, *p;

  mapname = xstrdup (str);

  /* Strip the ".map" extension.  */
  if (strlen (mapname) > 4
      && STREQ (mapname + strlen (mapname) - 4, ".map"))
    mapname[strlen (mapname) - 4] = '\0';

  /* Normalize the name, which basically consists on turning any
     character not in [0-9a-zA-Z_] into _ */
  for (p = mapname; *p != '\0'; ++p)
    {
      if (!((*p >= '0' && *p <= '9')
            || (*p >= 'a' && *p <= 'z')
            || (*p >= 'A' && *p <= 'Z')
            || (*p == '_')))
        *p = '_';
    }

  return mapname;
}

int
pk_map_load_file (int ios_id,
                  const char *path, char **errmsg)
{
  char *emsg, *mapname;
  FILE *fp;
  pk_map_parsed_map parsed_map;

  /* Do not attempt to load the mapfile if there is already a map with
     the same name defined in the IO space.  */
  mapname = pk_map_normalize_name (last_component (path));
  if (pk_map_search (ios_id, mapname) != NULL)
    {
      *errmsg = "map already loaded";
      return 0;
    }

  /* Open the file whose contents are to be parsed.  */
  if ((emsg = pk_file_readable (path)) != NULL)
    {
      *errmsg = emsg;
      return 0;
    }

  fp = fopen (path, "r");
  if (!fp)
    {
      *errmsg = strerror (errno);
      free (mapname);
      return 0;
    }

  /* Parse the file contents and close the file.  */
  parsed_map = pk_map_parse_file (path, fp);
  if (!parsed_map)
    {
      if (errmsg)
        *errmsg = "";
      return 0;
    }

  if (fclose (fp) == EOF)
    {
      *errmsg = strerror (errno);
      free (mapname);
      return 0;
    }

  /* XXX */
  //  pk_map_print_parsed_map (parsed_map);

  /* Process the result.  */
  if (!pk_map_load_parsed_map (ios_id,
                               mapname,
                               path,
                               parsed_map))
    {
      free (mapname);
      return 0;
    }

  free (mapname);
  return 1;
}

char *
pk_map_resolve_map (const char *mapname, int filename_p)
{
  pk_val val;
  const char *map_load_path;
  char *full_filename = NULL;

  val = pk_decl_val (poke_compiler, "map_load_path");
  if (val == PK_NULL)
    pk_fatal ("couldn't get `map_load_path'");

  if (pk_type_code (pk_typeof (val)) != PK_STRING)
    pk_fatal ("map_load_path should be a string");

  map_load_path = pk_string_str (val);

  /* Traverse the directories in the load path and try to load the
     requested map.  */
  {
    const char *ext = filename_p ? "" : ".map";
    const char *s, *e;

    char *fixed_load_path
      = pk_str_replace (map_load_path, "%DATADIR%", PKGDATADIR);

    for (s = fixed_load_path, e = s; *e; s = e + 1)
      {
        /* Ignore empty entries. */
        if ((e = strchrnul (s, ':')) == s)
          continue;

        asprintf (&full_filename, "%.*s/%s%s", (int) (e - s), s, mapname, ext);

        if (pk_file_readable (full_filename) == NULL)
          break;

        free (full_filename);
        full_filename = NULL;
      }

    if (fixed_load_path != map_load_path)
      free (fixed_load_path);
  }

  return full_filename;
}
