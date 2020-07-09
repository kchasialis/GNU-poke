/* pk-map-parser.h - Definitions for the map files parser.  */

/* Copyright (C) 2020 Jose E. Marchesi.  */

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

#ifndef PK_MAP_PARSER_H
#define PK_MAP_PARSER_H

/* Parser locations.  */

struct pk_map_parser_loc
{
  int first_line;
  int first_column;
  int last_line;
  int last_column;
};

/* Maps and map entries.  */

#define PK_MAP_PARSED_ENTRY_NAME(ENTRY) ((ENTRY)->name)
#define PK_MAP_PARSED_ENTRY_VARNAME(ENTRY) ((ENTRY)->varname)
#define PK_MAP_PARSED_ENTRY_TYPE(ENTRY) ((ENTRY)->type)
#define PK_MAP_PARSED_ENTRY_OFFSET(ENTRY) ((ENTRY)->offset)
#define PK_MAP_PARSED_ENTRY_CONDITION(ENTRY) ((ENTRY)->condition)
#define PK_MAP_PARSED_ENTRY_CHAIN(ENTRY) ((ENTRY)->chain)
#define PK_MAP_PARSED_ENTRY_LOC(ENTRY) ((ENTRY)->loc)
#define PK_MAP_PARSED_ENTRY_SKIPPED_P(ENTRY) ((ENTRY)->skipped_p)

struct pk_map_parsed_entry
{
  struct pk_map_parser_loc loc;
  char *name;
  char *varname;
  char *type;
  char *offset;
  char *condition;
  int skipped_p;
  struct pk_map_parsed_entry *chain;
};

typedef struct pk_map_parsed_entry *pk_map_parsed_entry;

#define PK_MAP_PARSED_MAP_NAME(MAP) ((MAP)->name)
#define PK_MAP_PARSED_MAP_PROLOGUE(MAP) ((MAP)->prologue)
#define PK_MAP_PARSED_MAP_ENTRIES(MAP) ((MAP)->entries)
#define PK_MAP_PARSED_MAP_LOC(MAP) ((MAP)->loc)

struct pk_map_parsed_map
{
  struct pk_map_parser_loc loc;
  char *name;
  char *prologue;
  pk_map_parsed_entry entries;
};

typedef struct pk_map_parsed_map *pk_map_parsed_map;

/* Parse a map definition from the given file.

   FILENAME is the name of the file to parse.
   FP is a FILE * for the file whose contents are to be parsed.

   Return the parsed map, or NULL if there is an error.  */

pk_map_parsed_map pk_map_parse_file (const char *filename, FILE *fp);

/* Free the resources used by the given parsed map struct.  */

void pk_map_free_parsed_map (pk_map_parsed_map parsed_map);

/* The following type is used internall in pk-map-tab.y.  Needs to be
   here because the generate dpk-map-tab.h needs access to it, and it
   is included in both the .y and .l files.  */

struct pk_map_parser
{
  struct pk_map_parsed_map *map;
  void *lexer;
  const char *filename;
  int once; /* See pk-map-lex.l for the purpose of this. */
  size_t nchars;
};

/* Tagged values have the form %FOO DATA.
   They always start at the beginning of a line.  */

#define TAG_UNKNOWN     0
#define TAG_NAME        1
#define TAG_TYPE        2
#define TAG_OFFSET      3
#define TAG_CONDITION   4

/* For debugging.  */

void pk_map_print_parsed_map (pk_map_parsed_map parsed_map);

#endif /* ! PK_MAP_PARSER_H */
