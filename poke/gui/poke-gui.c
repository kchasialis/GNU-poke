/* poke-gui.c - A GUI for GNU poke  */

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

#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <alloca.h>
#include <locale.h>
#include <gettext.h>
#define _(str) dgettext (PACKAGE, str)

#include <tcl.h>
#include <tk.h>

/* The Tcl interpreter.  */
Tcl_Interp *tcl_interpreter;

/* The directory from where to load scripts.  */
char *poke_guidir;

/* Boolean indicating whether the GUI should log MI traffic on the
   standard error.  */
int poke_debug_mi_p;

enum
{
  HELP_ARG,
  VERSION_ARG,
  DEBUG_MI_ARG,
};

static const struct option long_options[] =
{
  {"help", no_argument, NULL, HELP_ARG},
  {"version", no_argument, NULL, VERSION_ARG},
  {"debug-mi", no_argument, NULL, DEBUG_MI_ARG},
  {NULL, 0, NULL, 0},
};

static void
print_help (void)
{
  /* TRANSLATORS: --help output, poke-gui synopsis.
     no-wrap */
  puts (_("\
Usage: poke-gui [OPTION]..."));

  /* TRANSLATORS: --help output, poke-gui summary.
     no-wrap */
  puts (_("\
Interactive editor for binary files."));

  puts (_("\
Debugging options:\n\
      --debug-mi                      emit logs on MI transactions to stderr."));

  puts ("\n");
  /* TRANSLATORS: --help output, less used poke-gui arguments.
     no-wrap */
  puts (_("\
      --help                          print a help message and exit.\n\
      --version                       show version and exit."));
  puts ("\n");

#ifdef PACKAGE_PACKAGER_BUG_REPORTS
  printf (_("Report %s bugs to: %s\n"), PACKAGE_PACKAGER,
          PACKAGE_PACKAGER_BUG_REPORTS);
#endif
  printf (_("%s home page: <%s>\n"), PACKAGE_NAME, PACKAGE_URL);
  printf (_("General help using GNU software: %s\n"), "<http://www.gnu.org/gethelp/>");
}

void
print_version (void)
{
  printf ("GNU poke %s\n", VERSION);
  printf (_("\
%s (C) %s Jose E. Marchesi.\n\
License GPLv3+: GNU GPL version 3 or later\n"), "Copyright", "2020");
  puts ("\
This is free software: you are free to change and redistribute it.\n\
There is NO WARRANTY, to the extent permitted by law.");
}

static void
parse_args (int argc, char *argv[])
{
    char c;
  int ret;

  while ((ret = getopt_long (argc,
                             argv,
                             "",
                             long_options,
                             NULL)) != -1)
    {
      c = ret;
      switch (c)
        {
        case HELP_ARG:
          print_help ();
          goto exit_success;
          break;
        case VERSION_ARG:
          print_version ();
          goto exit_success;
          break;
        case DEBUG_MI_ARG:
          poke_debug_mi_p = 1;
          break;
        }
    }

  return;
 exit_success:
  exit (EXIT_SUCCESS);
}

int
load_script (const char *script)
{
  char *path
    = alloca (strlen (poke_guidir) + strlen (script) + 2);

  strcpy (path, poke_guidir);
  strcat (path, "/");
  strcat (path, script);

  return Tcl_EvalFile (tcl_interpreter, path);
}

int
main (int argc, char *argv[])
{
  parse_args (argc, argv);
  tcl_interpreter = Tcl_CreateInterp ();

  /* i18n */
  setlocale (LC_ALL, "");
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);

  /* Initialize the Tcl/Tk environment.  */
  if (Tcl_Init (tcl_interpreter) == TCL_ERROR)
    goto tcl_error;

  if (Tk_Init (tcl_interpreter) == TCL_ERROR)
    goto tcl_error;

  /* Determine the location of the GUI scripts.  */
  poke_guidir = getenv ("POKEGUIDIR");

  if (poke_guidir == NULL)
    poke_guidir = POKEGUIDIR;

  /* Set some global Tcl variables.  */
  Tcl_SetVar (tcl_interpreter, "poke_guidir", poke_guidir,
              TCL_GLOBAL_ONLY);
  Tcl_SetVar (tcl_interpreter, "poke_debug_mi_p",
              poke_debug_mi_p ? "1" : "0",
              TCL_GLOBAL_ONLY);

  /* Load the scripts.  */
  if (load_script ("pk-main.tcl") == TCL_ERROR)
    goto tcl_error;

  /* Enter the event loop.  */
  Tk_MainLoop ();

  /* Cleanup.  */
  Tcl_DeleteInterp (tcl_interpreter);
  return 0;

 tcl_error:
  fprintf (stderr, "poke-gui: error: %s\n",
           Tcl_GetStringResult (tcl_interpreter));
  Tcl_DeleteInterp (tcl_interpreter);

  return 1;
}
