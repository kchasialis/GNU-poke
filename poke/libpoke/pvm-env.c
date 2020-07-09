/* pvm-env.c - Run-time environment for Poke.  */

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

#include <string.h>
#include <assert.h>

#include "pvm.h"
#include "pvm-alloc.h"

/* The variables in each frame are organized in an array that can be
   efficiently accessed using OVER.

   Entries are allocated in steps of STEP variables.

   UP is a link to the immediately enclosing frame.  This is NULL for
   the top-level frame.  */

struct pvm_env
{
  int num_vars;
  int step;
  pvm_val *vars;

  struct pvm_env *up;
};


/* The following functions are documentd in pvm-env.h */

pvm_env
pvm_env_new (int hint)
{
  pvm_env env = pvm_alloc (sizeof (struct pvm_env));

  env->step = hint == 0 ? 128 : hint;
  env->num_vars = 0;
  env->vars = NULL;
  return env;
}

pvm_env
pvm_env_push_frame (pvm_env env, int hint)
{
  pvm_env frame = pvm_env_new (hint);

  frame->up = env;
  return frame;
}

pvm_env
pvm_env_pop_frame (pvm_env env)
{
  assert (env->up != NULL);
  return env->up;
}

void
pvm_env_register (pvm_env env, pvm_val val)
{
  assert (env->step != 0);
  if (env->num_vars % env->step == 0)
    {
      size_t size = ((env->num_vars + env->step)
                     * sizeof (void*));
      env->vars = pvm_realloc (env->vars, size);
      memset (env->vars + env->num_vars, 0,
              env->step * sizeof (void*));
    }

  env->vars[env->num_vars++] = val;
}

/* Given an environment return the frame back frames up from the bottom
   one.  back is allowed to be zero, but not negative. */
static pvm_env
pvm_env_back (pvm_env env, int back)
{
  pvm_env frame = env;
  int i;

  for (i = 0; i < back; i ++)
    frame = frame->up;
  return frame;
}

pvm_val
pvm_env_lookup (pvm_env env, int back, int over)
{
  return pvm_env_back (env, back)->vars[over];
}

void
pvm_env_set_var (pvm_env env, int back, int over, pvm_val val)
{
  pvm_env_back (env, back)->vars[over] = val;
}

int
pvm_env_toplevel_p (pvm_env env)
{
  return (env->up == NULL);
}
