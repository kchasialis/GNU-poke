/* pk-ios.c - IOS-related functions for poke.  */

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

#include <regex.h>

#include "poke.h"
#include "pk-ios.h"
#include "pk-map.h"

int
pk_open_ios (const char *handler, int set_cur_p)
{
  int ios_id;

  ios_id = pk_ios_open (poke_compiler, handler, 0, 1);
  if (ios_id == PK_IOS_ERROR)
    return ios_id;

  if (poke_auto_map_p)
  {
    int i;
    pk_val auto_map;
    pk_val nelem;

    auto_map = pk_decl_val (poke_compiler, "auto_map");
    if (auto_map == PK_NULL)
      pk_fatal ("auto_map is PK_NULL");

    nelem = pk_array_nelem (auto_map);
    for (i = 0; i < pk_uint_value (nelem); ++i)
      {
        pk_val auto_map_entry;
        pk_val regex, mapname;
        regex_t regexp;
        regmatch_t matches;

        auto_map_entry = pk_array_elem_val (auto_map, i);
        if (pk_type_code (pk_typeof (auto_map_entry)) != PK_ARRAY
            || pk_uint_value (pk_array_nelem (auto_map_entry)) != 2)
          pk_fatal ("invalid entry in auto_val");

        regex = pk_array_elem_val (auto_map_entry, 0);
        if (pk_type_code (pk_typeof (regex)) != PK_STRING)
          pk_fatal ("regexp should be a string in an auto_val entry");

        mapname = pk_array_elem_val (auto_map_entry, 1);
        if (pk_type_code (pk_typeof (mapname)) != PK_STRING)
          pk_fatal ("mapname should be a string in an auto_val entry");

        if (regcomp (&regexp, pk_string_str (regex),
                     REG_EXTENDED | REG_NOSUB) != 0)
          {
            pk_term_class ("error");
            pk_puts ("error: ");
            pk_term_end_class ("error");

            pk_printf ("invalid regexp `%s' in auto_map.  Skipping entry.\n",
                       pk_string_str (regex));
          }
        else
          {
            if (regexec (&regexp, handler, 1, &matches, 0) == 0)
              {
                /* Load the map.  */

                const char *map_handler
                  = pk_map_resolve_map (pk_string_str (mapname),
                                        0 /* handler_p */);

                if (!map_handler)
                  {
                    pk_term_class ("error");
                    pk_puts ("warning: ");
                    pk_term_end_class ("error");

                    pk_printf ("auto-map: unknown map `%s'",
                               pk_string_str (mapname));
                    regfree (&regexp);
                    break;
                  }

                if (!pk_map_load_file (ios_id, map_handler, NULL))
                  {
                    pk_term_class ("error");
                    pk_puts ("error: ");
                    pk_term_end_class ("error");

                    pk_printf ("auto-map: loading `%s'\n",
                               pk_string_str (mapname));
                    regfree (&regexp);
                    break;
                  }

                if (poke_interactive_p && !poke_quiet_p && !poke_prompt_maps_p)
                  pk_printf ("auto-map: map `%s' loaded\n",
                             pk_string_str (mapname));
              }

            regfree (&regexp);
          }
      }
  }

  return ios_id;
}
