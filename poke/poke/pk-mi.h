/* pk-mi.h - A Machine Interface for poke.  */

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

#ifndef PK_MI_H
#define PK_MI_H

/* Version of the protocol implemented by this MI.  */

#define MI_VERSION 0

/* Launch the MI interface and process MI requests.

   Return 1 if the MI terminated because the client disconnected.
   Return 0 in case there was a fatal error running the MI.  */

int pk_mi (void);

#endif /* ! PK_MI_H */
