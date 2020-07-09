/* pk-cmd.c - terminal related stuff.  */

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

#include <stdlib.h> /* For exit.  */
#include <assert.h> /* For assert. */
#include <string.h>
#include <unistd.h> /* For isatty */
#include <textstyle.h>

#include "poke.h"

/* The following global is the libtextstyle output stream to use to
   emit contents to the terminal.  */
static styled_ostream_t pk_ostream;

void
pk_term_init (int argc, char *argv[])
{
  int i;

  /* Process terminal-related command-line options.  */
  for (i = 1; i < argc; i++)
    {
      const char *arg = argv[i];

      if (strncmp (arg, "--color=", 8) == 0)
        {
          if (handle_color_option (arg + 8))
            pk_fatal ("handle_color_option failed");
        }
      else if (strncmp (arg, "--style=", 8) == 0)
        handle_style_option (arg + 8);
    }

  /* Handle the --color=test special argument.  */
  if (color_test_mode)
    {
      print_color_test ();
      exit (EXIT_SUCCESS);
    }

  /* Note that the following code needs to be compiled conditionally
     because the textstyle.h file provided by the gnulib module
     libtextstyle-optional defines style_file_name as an r-value.  */

#ifdef HAVE_LIBTEXTSTYLE
  /* Open the specified style.  */
  if (color_mode == color_yes
      || (color_mode == color_tty
          && isatty (STDOUT_FILENO)
          && getenv ("NO_COLOR") == NULL)
      || color_mode == color_html)
    {
      /* Find the style file.  */
      style_file_prepare ("POKE_STYLE", "POKESTYLESDIR", PKGDATADIR,
                          "poke-default.css");
    }
  else
    /* No styling.  */
    style_file_name = NULL;
#endif

  /* Create the output styled stream.  */
  pk_ostream =
    (color_mode == color_html
     ? html_styled_ostream_create (file_ostream_create (stdout),
                                   style_file_name)
     : styled_ostream_create (STDOUT_FILENO, "(stdout)",
                              TTYCTL_AUTO, style_file_name));
}

void
pk_term_shutdown ()
{
  styled_ostream_free (pk_ostream);
}

void
pk_term_flush ()
{
  ostream_flush (pk_ostream, FLUSH_THIS_STREAM);
}

void
pk_puts (const char *str)
{
  ostream_write_str (pk_ostream, str);
}

__attribute__ ((__format__ (__printf__, 1, 2)))
void
pk_printf (const char *format, ...)
{
  va_list ap;
  char *str;
  int r;

  va_start (ap, format);
  r = vasprintf (&str, format, ap);
  assert (r != -1);
  va_end (ap);

  ostream_write_str (pk_ostream, str);
  free (str);
}

void
pk_vprintf (const char *format, va_list ap)
{
  char *str;
  int r;

  r = vasprintf (&str, format, ap);
  assert (r != -1);

  ostream_write_str (pk_ostream, str);
  free (str);
}


void
pk_term_indent (unsigned int lvl,
                unsigned int step)
{
  pk_printf ("\n%*s", (step * lvl), "");
}

void
pk_term_class (const char *class)
{
  styled_ostream_begin_use_class (pk_ostream, class);
}

void
pk_term_end_class (const char *class)
{
  styled_ostream_end_use_class (pk_ostream, class);
}

void
pk_term_hyperlink (const char *url, const char *id)
{
#ifdef HAVE_TEXTSTYLE_HYPERLINK_SUPPORT
  styled_ostream_set_hyperlink (pk_ostream, url, id);
#endif
}

void
pk_term_end_hyperlink (void)
{
#ifdef HAVE_TEXTSTYLE_HYPERLINK_SUPPORT
  styled_ostream_set_hyperlink (pk_ostream, NULL, NULL);
#endif
}

int
pk_term_color_p (void)
{
  return (color_mode == color_yes
          || (color_mode == color_tty
              && isatty (STDOUT_FILENO)
              && getenv ("NO_COLOR") == NULL));
}
