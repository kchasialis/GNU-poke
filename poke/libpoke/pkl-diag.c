/* pkl-diag.c - Functions to emit compiler diagnostics.  */

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

#include <stdlib.h>
#include <assert.h>
#include "tmpdir.h"
#include "tempname.h"

#include "pkt.h"
#include "pkl.h"
#include "pkl-diag.h"

static void
pkl_detailed_location (pkl_ast ast, pkl_ast_loc loc,
                       const char *style_class)
{
  size_t cur_line = 1;
  size_t cur_column = 1;
  int i;

  if (!PKL_AST_LOC_VALID (loc))
    return;

  if (ast->buffer)
    {
      char *p;
      for (p = ast->buffer; *p != '\0'; ++p)
        {
          if (*p == '\n')
            {
              cur_line++;
              cur_column = 1;
            }
          else
            cur_column++;

          if (cur_line >= loc.first_line
              && cur_line <= loc.last_line)
            {
              /* Print until newline or end of string.  */
              for (;*p != '\0' && *p != '\n'; ++p)
                pk_printf ("%c", *p);
              break;
            }
        }
    }
  else
    {
      FILE *fp = ast->file;
      int c;

      off_t cur_pos = ftello (fp);
      off_t tmp;

      /* Seek to the beginning of the file.  */
      tmp = fseeko (fp, 0, SEEK_SET);
      assert (tmp == 0);

      while ((c = fgetc (fp)) != EOF)
        {
          if (c == '\n')
            {
              cur_line++;
              cur_column = 1;
            }
          else
            cur_column++;

          if (cur_line >= loc.first_line
              && cur_line <= loc.last_line)
            {
              /* Print until newline or end of string.  */
              do
                {
                  if (c != '\n')
                    pk_printf ("%c", c);
                  c = fgetc (fp);
                }
              while (c != EOF && c != '\0' && c != '\n');
              break;
            }
        }

      /* Restore the file position so parsing can continue.  */
      tmp = fseeko (fp, cur_pos, SEEK_SET);
      assert (tmp == 0);
    }

  pk_puts ("\n");

  for (i = 1; i < loc.first_column; ++i)
    pk_puts (" ");

  pk_term_class (style_class);
  for (; i < loc.last_column; ++i)
    if (i == loc.first_column)
      pk_puts ("^");
    else
      pk_puts ("~");
  pk_term_end_class (style_class);
  pk_puts ("\n");
}

static void
pkl_error_internal (pkl_compiler compiler,
                    pkl_ast ast,
                    pkl_ast_loc loc,
                    const char *fmt,
                    va_list valist)
{
  char *errmsg, *p;

  /* Write out the error message, line by line.  */
  vasprintf (&errmsg, fmt, valist);

  p = errmsg;
  while (*p != '\0')
    {
      pk_term_class ("error-filename");
      if (ast->filename)
        pk_printf ("%s:", ast->filename);
      else
        pk_puts ("<stdin>:");
      pk_term_end_class ("error-filename");

      if (PKL_AST_LOC_VALID (loc))
        {
          pk_term_class ("error-location");
          if (pkl_quiet_p (compiler))
            pk_printf ("%d: ", loc.first_line);
          else
            pk_printf ("%d:%d: ",
                       loc.first_line, loc.first_column);
          pk_term_end_class ("error-location");
        }

      pk_term_class ("error");
      pk_puts ("error: ");
      pk_term_end_class ("error");

      while (*p != '\n' && *p != '\0')
        {
          pk_printf ("%c", *p);
          p++;
        }
      if (*p == '\n')
        p++;
      pk_puts ("\n");
    }
  free (errmsg);

  if (!pkl_quiet_p (compiler))
    pkl_detailed_location (ast, loc, "error");
}

void
pkl_error (pkl_compiler compiler,
           pkl_ast ast,
           pkl_ast_loc loc,
           const char *fmt,
           ...)
{
  va_list valist;

  va_start (valist, fmt);
  pkl_error_internal (compiler, ast, loc, fmt, valist);
  va_end (valist);
}

void
pkl_warning (pkl_compiler compiler,
             pkl_ast ast,
             pkl_ast_loc loc,
             const char *fmt,
             ...)
{
  va_list valist;
  char *msg;

  if (pkl_error_on_warning (compiler))
    {
      va_start (valist, fmt);
      pkl_error_internal (compiler, ast, loc, fmt, valist);
      va_end (valist);
      return;
    }

  va_start (valist, fmt);
  vasprintf (&msg, fmt, valist);
  va_end (valist);

  pk_term_class ("error-filename");
  if (ast->filename)
    pk_printf ("%s:", ast->filename);
  else
    pk_puts ("<stdin>:");
  pk_term_end_class ("error-filename");

  if (PKL_AST_LOC_VALID (loc))
    {
      pk_term_class ("error-location");
      pk_printf ("%d:%d: ", loc.first_line, loc.first_column);
      pk_term_end_class ("error-location");
    }
  pk_term_class ("warning");
  pk_puts ("warning: ");
  pk_term_end_class ("warning");
  pk_puts (msg);
  pk_puts ("\n");

  free (msg);

  if (!pkl_quiet_p (compiler))
    pkl_detailed_location (ast, loc, "warning");
}

void
pkl_ice (pkl_compiler compiler,
         pkl_ast ast,
         pkl_ast_loc loc,
         const char *fmt,
         ...)
{
  va_list valist;
  char tmpfile[1024];

  if (!pkl_quiet_p (compiler))
  {
    int des;
    FILE *out;

    if (((des = path_search (tmpfile, 1024, NULL, "poke", true)) == -1)
        || ((des = mkstemp (tmpfile)) == -1))
      {
        pk_term_class ("error");
        pk_puts ("internal error: ");
        pk_term_end_class ("error");
        pk_puts ("determining a temporary file name\n");

        return;
      }

    out = fdopen (des, "w");
    if (out == NULL)
      {
        pk_term_class ("error");
        pk_puts ("internal error: ");
        pk_term_end_class ("error");
        pk_printf ("opening temporary file `%s'\n", tmpfile);
        return;
      }

    fputs ("internal compiler error: ", out);
    va_start (valist, fmt);
    vfprintf (out, fmt, valist);
    va_end (valist);
    fputc ('\n', out);
    pkl_ast_print (out, ast->ast);
    fclose (out);
  }

  if (PKL_AST_LOC_VALID (loc))
    {
      pk_term_class ("error-location");
      pk_printf ("%d:%d: ", loc.first_line, loc.first_column);
      pk_term_end_class ("error-location");
    }
  pk_puts ("internal compiler error: ");
  {
    char *msg;

    va_start (valist, fmt);
    vasprintf (&msg, fmt, valist);
    va_end (valist);

    pk_puts (msg);
    free (msg);
  }
  pk_puts ("\n");
  if (!pkl_quiet_p (compiler))
    {
      pk_printf ("Important information has been dumped in %s.\n",
                 tmpfile);
      pk_puts ("Please attach it to a bug report and send it to");
      pk_term_hyperlink ("mailto:" PACKAGE_BUGREPORT, NULL);
      pk_puts (" " PACKAGE_BUGREPORT);
    }
  pk_term_end_hyperlink ();
  pk_puts (".\n");
}
