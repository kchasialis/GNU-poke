/* pk-ios.h - IOS-related functions for poke.  */

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

#ifndef PK_IOS_H
#define PK_IOS_H

#include <config.h>

/* Open a new IO space in the application and return its IOS id.

   HANDLER is the handler identifying the IO space.  This is typically
   the name of a file.

   SET_CUR_P is 1 if the IOS is to become the current IOS after being
   opened.  0 otherwise.

   Return the IOS id of the newly opened IOS, or PK_IOS_ERROR if the
   given handler coulnd't be opened.  */

int pk_open_ios (const char *handler, int set_cur_p);

#endif /* ! PK_IOS_H */
