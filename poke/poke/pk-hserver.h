/* pk-hserver.h - A terminal hyperlinks server for poke.  */

/* Copyright (C) 2019 Jose E. Marchesi */

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

#ifndef PK_HSERVER_H
#define PK_HSERVER_H

#include <config.h>

/* Initialize and finalize the server.  */
void pk_hserver_init (void);
void pk_hserver_shutdown (void);

/* Get a new token.  */
int pk_hserver_get_token (void);

/* Return the port number where the server is listening.  This
   function shall be called after pk_hserver_init.  */
int pk_hserver_port (void);

/* Build hyperlinks.  */
char *pk_hserver_make_hyperlink (char type, const char *cmd);

#endif /* PK_HSERVER_H */
