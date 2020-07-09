/* pk-utils.c - Common utility functions for poke.  */

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
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <gettext.h>
#define _(str) dgettext (PACKAGE, str)
#include <stdarg.h> /* va_... */
#include <stddef.h> /* size_t */
#include <string.h> /* strcpy */
#include <ctype.h> /* isspace */

#include "pk-utils.h"

char *
pk_file_readable (const char *filename)
{
  static char errmsg[4096];
  struct stat statbuf;
  if (0 != stat (filename, &statbuf))
    {
      char *why = strerror (errno);
      snprintf (errmsg, 4096, _("Cannot stat %s: %s\n"), filename, why);
      return errmsg;
    }

  if (S_ISDIR (statbuf.st_mode))
    {
      snprintf (errmsg, 4096, _("%s is a directory\n"), filename);
      return errmsg;
    }

  if (access (filename, R_OK) != 0)
    {
      char *why = strerror (errno);
      snprintf (errmsg, 4096, _("%s: file cannot be read: %s\n"),
                filename, why);
      return errmsg;
    }

  return 0;
}

#define PK_POW(NAME,TYPE)                       \
  TYPE                                          \
  NAME (TYPE base, uint32_t exp)                \
  {                                             \
    TYPE result = 1;                            \
    while (1)                                   \
      {                                         \
        if (exp & 1)                            \
          result *= base;                       \
        exp >>= 1;                              \
        if (!exp)                               \
          break;                                \
        base *= base;                           \
      }                                         \
    return result;                              \
  }

PK_POW (pk_ipow, int64_t)
PK_POW (pk_upow, uint64_t)

#undef PK_POW

void
pk_print_binary (void (*puts_fn) (const char *str),
                 uint64_t val, int size, int sign)
{
  char b[65];

  if (size != 64 && size != 32 && size != 16 && size != 8
      && size != 4)
    {
      snprintf (b, sizeof (b), "(%sint<%d>) ", sign ? "" : "u", size);
      puts_fn (b);
    }

  for (int z = 0; z < size; z++) {
    b[size-1-z] = ((val >> z) & 0x1) + '0';
  }
  b[size] = '\0';

  puts_fn ("0b");
  puts_fn (b);

  if (size == 64)
    puts_fn (sign ? "L" : "UL");
  else if (size == 16)
    puts_fn (sign ? "H" : "UH");
  else if (size == 8)
    puts_fn (sign ? "B" : "UB");
  else if (size == 4)
    puts_fn (sign ? "N" : "UN");
}

/* Concatenate 2+ strings.
 * Last argument must be NULL.
 * Returns the malloc'ed concatenated string or NULL when out of memory.
 */
char *
pk_str_concat (const char *s0, ...)
{
  va_list args;
  size_t len = 0;
  const char *s;
  char *d, *res;

  va_start (args, s0);
  for (s = s0; s; s = va_arg (args, const char *))
    len += strlen (s);
  va_end (args);

  res = malloc (len + 1);
  if (!res)
    return NULL;

  va_start (args, s0);
  for (d = res, s = s0; s; s = va_arg (args, const char *))
    {
      strcpy (d, s);
      d += strlen (s);
    }
  va_end (args);

  return res;
}

/* Replace all occurrences of SEARCH within IN by REPLACE.
 * Return IN when SEARCH was not found, else
 * return a new allocated string with the replaced sequences.
 * Return NULL on allocation failure.
 */
char *
pk_str_replace (const char *in, const char *search, const char *replace)
{
  const char *s, *e;
  char *out, *d;
  int num = 0;

  /* count number of occurrences of 'search' within IN */
  for (s = in; (s = strstr (s, search)); s++, num++)
    ;

  if (!num)
    return (char *) in;

  size_t search_len = strlen (search);
  size_t replace_len = strlen (replace);
  size_t in_len = strlen (in);

  d = out = malloc (in_len + (replace_len - search_len) * num + 1);
  if (!out)
    return NULL;

  for (s = in; (e = strstr (s, search)); s = e + search_len)
    {
      memcpy (d, s, e - s);
      d += e - s;

      memcpy (d, replace, replace_len);
      d += replace_len;
    }

  /* copy rest of IN + trailing zero */
  strcpy (d, s);

  return out;
}

void
pk_str_trim (char **str)
{
  char *end;

  while (isspace (**str))
    (*str)++;
  end = *str + strlen (*str);
  while (isspace (*--end));
  *(end + 1) = '\0';
}
