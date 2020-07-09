/* pk-repl.c - A REPL ui for poke.  */

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

#include <signal.h>
#include <pthread.h>
#include <unistd.h>
#include <setjmp.h>
#include <stdlib.h>
#include "readline.h"
#if defined HAVE_READLINE_HISTORY_H
# include <readline/history.h>
#endif
#include "xalloc.h"
#include "xstrndup.h"

#include "poke.h"
#include "pk-cmd.h"
#if HAVE_HSERVER
#  include "pk-hserver.h"
#endif
#include "pk-utils.h"
#include "pk-map.h"

/* The thread that contains the non-local entry point for reentering
   the REPL.  */
static pthread_t volatile ctrlc_thread;
/* The non-local entry point for reentering the REPL.  */
static sigjmp_buf /*volatile*/ ctrlc_buf;
/* When nonzero, ctrlc_thread and ctrlc_buf contain valid values.  */
static int volatile ctrlc_buf_valid;

/* This function is called repeatedly by the readline library, when
   generating potential command line completions.

   TEXT is the partial word to be completed.  STATE is zero the first
   time the function is called and a positive non-zero integer for
   each subsequent call.

   On each call, the function returns a potential completion.  It
   returns NULL to indicate that there are no more possibilities left. */
static char *
poke_completion_function (const char *text, int state)
{
  /* First try to complete with "normal" commands.  */
  char *function_name = pk_completion_function (poke_compiler,
                                                text, state);

  /* Then try with dot-commands. */
  if (function_name == NULL)
    function_name = pk_cmd_get_next_match (text, strlen (text));

  return function_name;
}

/*  A trivial completion function.  No completions are possible.  */
static char *
null_completion_function (const char *x, int state)
{
  return NULL;
}

char * doc_completion_function (const char *x, int state);

#define SPACE_SUBSTITUTE  '/'

/* Display the list of matches, replacing SPACE_SUBSTITUTE with
   a space.  */
static void
space_substitute_display_matches (char **matches, int num_matches,
                                int max_length)
{
  for (int i = 0; i < num_matches + 1; ++i)
    {
      for (char *m = matches[i]; *m; ++m)
        {
          if (*m == SPACE_SUBSTITUTE)
            *m = ' ';
        }
    }

  rl_display_match_list (matches, num_matches, max_length);
}

/* Display the rl_line_buffer substituting
SPACE_SUBSTITUTE with a space.  */
static void
space_substitute_redisplay (void)
{
  /* Take a copy of the line_buffer.  */
  char *olb = xstrdup (rl_line_buffer);

  for (char *x = rl_line_buffer; *x ; x++)
    {
      if (*x == SPACE_SUBSTITUTE)
        *x = ' ';
    }

  rl_redisplay ();

  /* restore the line_buffer to its original state.  */
  strcpy (rl_line_buffer, olb);
  free (olb);
}

/* Readline's getc callback.
   Use this function to update the completer which
   should be used.
*/
static int
poke_getc (FILE *stream)
{
  char *line_to_point = xstrndup (rl_line_buffer, rl_point ? rl_point - 1 : 0);
  char *tok = strtok (line_to_point, "\t ");
  const struct pk_cmd *cmd = pk_cmd_find (tok);

  free (line_to_point);

  if (cmd == NULL)
    rl_completion_entry_function = poke_completion_function;

   if (rl_completion_entry_function == poke_completion_function)
     {
       if (cmd)
         {
           if (cmd->completer)
             rl_completion_entry_function = cmd->completer;
           else
             rl_completion_entry_function = null_completion_function;
         }
     }

  int c =  rl_getc (stream);

  /* Due to readline's apparent inability to change the word break
     character in the middle of a line, we have to do some underhand
     skullduggery here.  Spaces are substituted with SPACE_SUBSTITUTE,
     and then substituted back again in various callback functions.  */
  if (rl_completion_entry_function == doc_completion_function)
    {
      rl_completion_display_matches_hook = space_substitute_display_matches;
      rl_redisplay_function = space_substitute_redisplay;

      if (c == ' ')
        c = SPACE_SUBSTITUTE;
    }
  else
    {
      rl_completion_display_matches_hook = NULL;
      rl_redisplay_function = rl_redisplay;
    }

  return c;
}


static void
banner (void)
{
  if (!poke_quiet_p)
    {
      pk_print_version ();
      pk_puts ("\n");

#if HAVE_HSERVER
      if (poke_hserver_p)
        {
          pk_printf ("hserver listening in port %d.\n",
                     pk_hserver_port ());
          pk_puts ("\n");
        }
#endif

#if HAVE_HSERVER
      if (poke_hserver_p)
        {
          char *help_hyperlink
            = pk_hserver_make_hyperlink ('e', ".help");

          pk_puts (_("For help, type \""));
          pk_term_hyperlink (help_hyperlink, NULL);
          pk_puts (".help");
          pk_term_end_hyperlink ();
          pk_puts ("\".\n");
          free (help_hyperlink);
        }
      else
#endif
      pk_puts (_("For help, type \".help\".\n"));
      pk_puts (_("Type \".exit\" to leave the program.\n"));
    }

}

static _GL_ASYNC_SAFE void
poke_sigint_handler (int sig)
{
  if (ctrlc_buf_valid)
    {
      /* Abort the current command, and return to the REPL.  */
      if (!pthread_equal (pthread_self (), ctrlc_thread))
        {
          /* We are not in the thread that established the jmp_buf for
             returning to the REPL.  Redirect the signal to that thread,
             and decline responsibility for future deliveries of this
             signal.  */
          sigset_t mask;
          if (sigemptyset (&mask) == 0
              && sigaddset (&mask, SIGINT) == 0)
            pthread_sigmask (SIG_BLOCK, &mask, NULL);

          pthread_kill (ctrlc_thread, SIGINT);
        }
      else
        {
          /* We are in the thread that established the jmp_buf for returning
             to the REPL.  Put the readline library into a sane state, then
             unwind the stack.  */
          rl_free_line_state ();
          rl_cleanup_after_signal ();
          rl_line_buffer[rl_point = rl_end = rl_mark = 0] = 0;
          printf ("\n");
          siglongjmp (ctrlc_buf, 1);
        }
    }
  else
    {
      /* The default behaviour for SIGINT is to terminate the process.
         Let's do that here.  */
      struct sigaction sa;
      sa.sa_handler = SIG_DFL;
      sa.sa_flags = 0;
      sigemptyset (&sa.sa_mask);
      sigaction (SIGINT, &sa, NULL);
      raise (SIGINT);
    }
}

/* Return a copy of TEXT, with every instance of the space character
   prepended with the backslash character.   The caller is responsible
   for freeing the result.  */
static char *
escape_metacharacters (char *text, int match_type, char *qp)
{
  char *p = text;
  char *r = xmalloc (strlen (text) * 2 + 1);
  char *s = r;

  while (*p)
    {
      char c = *p++;
      if (c == ' ')
        *r++ = '\\';
      *r++ = c;
    }
  *r = '\0';

  return s;
}

static char *
pk_prompt (void)
{
  char *prompt = "";

  if (poke_prompt_maps_p)
    {
      pk_ios cur_ios;

      cur_ios = pk_ios_cur (poke_compiler);
      if (cur_ios)
        {
          pk_map maps
            = pk_map_get_maps (pk_ios_get_id (cur_ios));

          if (maps)
            {
              pk_map map;

              prompt = pk_str_concat (prompt, "[", NULL);
              for (map = maps; map; map = PK_MAP_CHAIN (map))
                prompt = pk_str_concat (prompt, PK_MAP_NAME (map),
                                        PK_MAP_CHAIN (map) != NULL ? "," : NULL,
                                        NULL);
              prompt = pk_str_concat (prompt, "]", NULL);
            }
        }
    }

  prompt = pk_str_concat (prompt, "(poke) ", NULL);
  return prompt;
}

void
pk_repl (void)
{
  banner ();

  /* Arrange for the current line to be cancelled on SIGINT.
     Since some library code is also interested in SIGINT
     (GNU libtextstyle, via gnulib module fatal-signal), it is better
     to install it once, rather than to install and uninstall it once
     in each round of the REP loop below.  */
  ctrlc_buf_valid = 0;
  struct sigaction sa;
  sa.sa_handler = poke_sigint_handler;
  sa.sa_flags = 0;
  sigemptyset (&sa.sa_mask);
  sigaction (SIGINT, &sa, NULL);

#if defined HAVE_READLINE_HISTORY_H
  char *poke_history = NULL;
  /* Load the user's history file ~/.poke_history, if it exists
     in the HOME directory.  */
  char *homedir = getenv ("HOME");

  if (homedir != NULL)
    {
      if (asprintf (&poke_history, "%s/.poke_history", homedir) != -1)
        {
          if (access (poke_history, R_OK) == 0)
            read_history (poke_history);
        }
      else
        poke_history = NULL;
    }
#endif
  rl_getc_function = poke_getc;
  rl_completer_quote_characters = "\"";
  rl_filename_quote_characters = " ";
  rl_filename_quoting_function = escape_metacharacters;

  ctrlc_thread = pthread_self ();
  sigsetjmp (ctrlc_buf, 1);
  ctrlc_buf_valid = 1;

  while (!poke_exit_p)
    {
      char *prompt;
      char *line;

      pk_term_flush ();
      rl_completion_entry_function = poke_completion_function;
      prompt = pk_prompt ();
      line = readline (prompt);
      free (prompt);
      if (line == NULL)
        {
          /* EOF in stdin (probably Ctrl-D).  */
          pk_puts ("\n");
          break;
        }

      if (rl_completion_entry_function == doc_completion_function)
        {
          for (char *s = line; *s; ++s)
            {
              if (*s == SPACE_SUBSTITUTE)
              *s = ' ';
            }
        }

      /* Ignore empty lines.  */
      if (*line != '\0')
        {
#if defined HAVE_READLINE_HISTORY_H
          add_history (line);
#endif

          pk_cmd_exec (line);
        }
      free (line);
    }
#if defined HAVE_READLINE_HISTORY_H
  if (poke_history) {
    write_history (poke_history);
    free (poke_history);
  }
#endif

  ctrlc_buf_valid = 0;
}

static int saved_point;
static int saved_end;

void
pk_repl_display_begin (void)
{
  saved_point = rl_point;
  saved_end = rl_end;
  rl_point = rl_end = 0;

  rl_save_prompt ();
  rl_clear_message ();

  pk_puts (rl_prompt);
}

void
pk_repl_display_end (void)
{
  pk_term_flush ();
  rl_restore_prompt ();
  rl_point = saved_point;
  rl_end = saved_end;
  rl_forced_update_display ();
}

void
pk_repl_insert (const char *str)
{
  rl_insert_text (str);
  rl_redisplay ();
}
