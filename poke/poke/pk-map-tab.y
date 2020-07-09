/* pk-map-tab.y - LALR(1) parser for Poke map files.  */

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

%define api.pure full
%define parse.error verbose
%locations
 /* %name-prefix "pk_map_tab_"*/
%define api.prefix {pk_map_tab_}

%lex-param {void *scanner}
%parse-param {struct pk_map_parser *map_parser}

%initial-action
{
    @$.first_line = @$.last_line = 1;
    @$.first_column = @$.last_column = 1;
};

%{
#include <config.h>

#include <assert.h>
#include <stdlib.h>
#include "xalloc.h"

#include "pk-utils.h"
#include "pk-term.h"
#include "pk-map.h"
#include "pk-map-parser.h"

#define PK_MAP_TAB_LTYPE struct pk_map_parser_loc
#include "pk-map-tab.h"
#include "pk-map-lex.h"

#define scanner (map_parser->lexer)

/* Routines to build parsed maps and parsed map entries.  */

static pk_map_parsed_entry
make_map_entry (const char *name,
                const char *condition,
                const char *type,
                const char *offset)
{
  pk_map_parsed_entry entry
    = xmalloc (sizeof (struct pk_map_parsed_entry));
  char *ename = xstrdup (name);

  pk_str_trim (&ename);
  PK_MAP_PARSED_ENTRY_NAME (entry) = ename;
  PK_MAP_PARSED_ENTRY_VARNAME (entry) = NULL;
  PK_MAP_PARSED_ENTRY_TYPE (entry) = xstrdup (type);
  PK_MAP_PARSED_ENTRY_OFFSET (entry) = xstrdup (offset);
  PK_MAP_PARSED_ENTRY_CHAIN (entry) = NULL;

  if (condition)
    PK_MAP_PARSED_ENTRY_CONDITION (entry) = xstrdup (condition);
  else
    PK_MAP_PARSED_ENTRY_CONDITION (entry) = NULL;

  return entry;
}

static pk_map_parsed_entry
map_entry_chainon (pk_map_parsed_entry entry1,
                   pk_map_parsed_entry entry2)
{
  if (entry1)
    {
      pk_map_parsed_entry tmp;

      for (tmp = entry1; tmp->chain; tmp = tmp->chain)
        assert (tmp != entry2);

      tmp->chain = entry2;
      return entry1;
    }

  return entry2;
}

static pk_map_parsed_map
make_map (const char *name, const char *prologue,
          pk_map_parsed_entry entries)
{
  pk_map_parsed_map new
    = xmalloc (sizeof (struct pk_map_parsed_map));

  PK_MAP_PARSED_MAP_NAME (new) = xstrdup (name);
  PK_MAP_PARSED_MAP_PROLOGUE (new) = xstrdup (prologue);
  PK_MAP_PARSED_MAP_ENTRIES (new) = entries;
  return new;
}

struct tagged_value
{
  int tag;
  char *data;
  struct tagged_value *chain;
  struct pk_map_parser_loc loc;
};

typedef struct tagged_value *tagged_value;

static tagged_value
make_tagged_value (int tag, const char *data)
{
  tagged_value tv = xmalloc (sizeof (struct tagged_value));

  tv->tag = tag;
  tv->data = xstrdup (data);
  tv->chain = NULL;
  return tv;
}

static void
free_tagged_value (tagged_value value)
{
  free (value->data);
  free (value);
}

static void
free_tagged_value_chain (tagged_value value)
{
  tagged_value val, tmp;

  for (val = value;
       val;
       val = tmp)
    {
      tmp = val->chain;
      free_tagged_value (val);
    }
}

static tagged_value
tagged_value_chainon (tagged_value tv1, tagged_value tv2)
{
  if (tv1)
    {
      tagged_value tmp;

      for (tmp = tv1; tmp->chain; tmp = tmp->chain)
        assert (tmp != tv2);

      tmp->chain = tv2;
      return tv1;
    }

  return tv2;
}

/* Get the tagged value with tag TAG from the chain of tagged values
   VALUES.

   If there is no tagged value with tag TAG in the chain, then return
   NULL.  */

static tagged_value
get_tagged_value (tagged_value values, int tag)
{
  tagged_value value;

  for (value = values;
       value;
       value = value->chain)
    {
      if (value->tag == tag)
        return value;
    }

  return NULL;
}

/* Error printer.  */

void
pk_map_printf_error (struct pk_map_parser *map_parser,
                     YYLTYPE loc, const char *format, ...)
{

  va_list ap;

  if (map_parser->filename)
    pk_printf ("%s:", map_parser->filename);

  if (loc.first_line != 0
      || loc.first_column != 0
      || loc.last_line != 0
      || loc.last_column != 0)
    {
      pk_term_class ("error-location");
      pk_printf ("%d:%d: ", loc.first_line, loc.first_column);
      pk_term_end_class ("error-location");
    }

  va_start (ap, format);
  pk_vprintf (format, ap);
  va_end (ap);
}

/* Check the list of tags for an entry are not duplicated.

   Return 1 if the tags are ok.  Return 0 otherwise.  */

static int
check_entry_duplicated_tags (struct pk_map_parser *map_parser,
                             tagged_value tagged_values)
{
  tagged_value value;

  for (value = tagged_values;
       value;
       value = value->chain)
    {
      tagged_value value2;

      for (value2 = value->chain;
           value2;
           value2 = value2->chain)
        {
          if (value->tag == value2->tag)
            {
              pk_map_printf_error (map_parser,
                                   value2->loc,
                                   "duplicated tag");
              return 0;
            }
        }
    }

  return 1;
}

/* Handler for syntax errors.  */

static void
pk_map_tab_error (YYLTYPE *llocp, struct pk_map_parser *map_parser, char const *err)
{
  pk_map_printf_error (map_parser, *llocp, "%s\n", err);
}

%}

/* Type of the semantic values for the rules.  */

%union {
  char *string;
  int integer;
  struct pk_map_parsed_map *map;
  struct pk_map_parsed_entry *entry;
  struct tagged_value *tagged_value;
}

/* Tokens.  */

%token ERROR SEP
%token ENTRY
%token <integer> TAG
%token <string> DATA

/* Rule semantic types.  */

%type <map> map
%type <entry> map_entries
%type <entry> map_entry
%type <tagged_value> tagged_value_list tagged_value

%start map

%%

map:
          DATA SEP map_entries
                {
                  /* XXX sort the entries?  Maybe not.  */
                  $$ = make_map ("XXX", $1, $3);
                  $$->loc = @$;
                  map_parser->map = ($$);
                }
        ;

map_entries:
          %empty
                { $$ = NULL; }
        | map_entries map_entry
                { $$ = map_entry_chainon ($1, $2); }

map_entry:
          ENTRY tagged_value_list
                  {
                  tagged_value name = get_tagged_value ($2, TAG_NAME);
                  tagged_value type = get_tagged_value ($2, TAG_TYPE);
                  tagged_value offset = get_tagged_value ($2, TAG_OFFSET);
                  tagged_value condition = get_tagged_value ($2, TAG_CONDITION);

                  /* XXX Handle duplicated attributes.  */
                  if (!check_entry_duplicated_tags (map_parser, $2))
                    YYERROR;

                  /* Check for mandatory attributes in the entry.  */
                  if (!name || !type || !offset)
                    {
                      if (!name)
                        pk_map_printf_error (map_parser, @$, "entry lacks a %name");
                      else if (!type)
                        pk_map_printf_error (map_parser, @$, "entry lacks a %type");
                      else if (!offset)
                        pk_map_printf_error (map_parser, @$, "entry lacks an %offset");
                      else
                        assert (0);

                      YYERROR;
                    }

                  /* For condition and offset, compile.  */
                  /* For name and type, keep strings.  */

                  $$ = make_map_entry (name->data,
                                       condition ? condition->data : NULL,
                                       type->data,
                                       offset->data);
                  $$->loc = @$;
                  free_tagged_value_chain ($2);
                }
        ;

tagged_value_list:
          %empty
                { $$ = NULL; }
        | tagged_value_list tagged_value
                { $$ = tagged_value_chainon ($1, $2); }
        ;

tagged_value:
          TAG DATA
          {
            if ($1 == TAG_UNKNOWN)
              {
                pk_map_printf_error (map_parser, @$, "unknown tag\n");
                YYERROR;
              }

            $$ = make_tagged_value ($1, $2);
            $$->loc = @$;
          }
        ;

%%

/* Parser public functions.  */

pk_map_parsed_map
pk_map_parse_file (const char *filename, FILE *fp)
{
  int ret;
  char *stderr;
  struct pk_map_parser map_parser;

  map_parser.map = NULL;
  map_parser.lexer = NULL;
  map_parser.once = 0;
  map_parser.nchars = 0;
  map_parser.filename = filename;

  pk_map_tab_lex_init (&map_parser.lexer);
  pk_map_tab_set_extra (&map_parser, map_parser.lexer);
  pk_map_tab_set_in (fp, map_parser.lexer);
  ret = pk_map_tab_parse (&map_parser);
  pk_map_tab_lex_destroy (map_parser.lexer);

  if (ret != 0)
    /* Parse error.  */
    return NULL;

  return map_parser.map;
}

/* For debugging.  */

static void
pk_map_print_loc (struct pk_map_parser_loc loc)
{
  printf ("%d,%d-%d,%d",
          loc.first_line, loc.first_column,
          loc.last_line, loc.last_column);
}

void
pk_map_print_parsed_map (pk_map_parsed_map parsed_map)
{
  pk_map_parsed_entry entry;

  printf ("MAP\n");
  printf ("ENTRY\n");
  printf (" loc: ");
  pk_map_print_loc (PK_MAP_PARSED_MAP_LOC (parsed_map));
  printf ("\n");
  printf (" name: %s\n", PK_MAP_PARSED_MAP_NAME (parsed_map));
  printf (" prologue:\n");
  printf ("%s", PK_MAP_PARSED_MAP_PROLOGUE (parsed_map));
  printf ("ENTRIES:\n");

  for (entry = PK_MAP_PARSED_MAP_ENTRIES (parsed_map);
       entry;
       entry = PK_MAP_PARSED_ENTRY_CHAIN (entry))
    {
      printf ("ENTRY\n");
      printf ("  loc: ");
      pk_map_print_loc (PK_MAP_PARSED_ENTRY_LOC (entry));
      printf ("\n");
      printf ("  name: %s", PK_MAP_PARSED_ENTRY_NAME (entry));
      printf ("  type: %s", PK_MAP_PARSED_ENTRY_TYPE (entry));
      printf ("  offset: %s", PK_MAP_PARSED_ENTRY_OFFSET (entry));
      if (PK_MAP_PARSED_ENTRY_CONDITION (entry))
        printf ("  condition: %s", PK_MAP_PARSED_ENTRY_CONDITION (entry));
    }
  printf ("\n");
}
