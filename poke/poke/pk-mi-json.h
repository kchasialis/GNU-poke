/* pk-mi-json.h - Machine Interface JSON support */

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

#ifndef PK_MI_JSON
#define PK_MI_JSON

#include <stdlib.h>
#include <config.h>

#include "pk-mi-msg.h"
#include "libpoke.h"

#define PK_MI_SET_ERRMSG(errmsg, M, ...)  \
   sprintf(*errmsg, "[ERROR] " M "\n",\
        ##__VA_ARGS__)

#define PK_MI_CHECK(errmsg, A, M, ...) \
   if(!(A)) {                  \
     if (errmsg == NULL) goto error; \
     *errmsg = (char *) malloc (1024);    \
     PK_MI_SET_ERRMSG(errmsg, M, ##__VA_ARGS__); goto error;}

#define PK_MI_DEBUG(M, ...)                  \
      fprintf(stderr, "DEBUG %s:%d: " M "\n",\
        __FILE__, __LINE__, ##__VA_ARGS__)


/* Given a string containing a JSON message, parse it and return a MI
   message.

   In case of error return NULL.  */

pk_mi_msg pk_mi_json_to_msg (const char *str);

/* Given a MI message, return a string with the JSON representation of
   the message.

   In case of error return NULL.  */

const char *pk_mi_msg_to_json (pk_mi_msg msg);

/* XXX services to pk_val <-> json */
/* XXX services to pk_type <-> json */

/* Given a pvm val, return the json object associated with this val
	
   In case of error return NULL.  */

const char *pk_mi_val_to_json (pk_val val, char **errmsg);

/* Given a json object, return the pvm val associated with it
   In case of error return NULL.  */

pk_val pk_mi_json_to_val (const char *str, char **errmsg);

#endif /* ! PK_MI_JSON */
