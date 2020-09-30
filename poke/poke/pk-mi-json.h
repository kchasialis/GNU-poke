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

#include <config.h>

#include <stdlib.h>

#include "pk-mi-msg.h"
#include "libpoke.h"

/* Given a string containing a JSON message, parse it and return a MI
   message.

   In case of error return NULL.  */

pk_mi_msg pk_mi_json_to_msg (const char *str);

/* Given a MI message, return a string with the JSON representation of
   the message.

   In case of error return NULL.  */

const char *pk_mi_msg_to_json (pk_mi_msg msg);

/* Given a json object, create the Poke value associated with it.

   VALUE is the Poke value to be created.

   STR is the string value of a pk_val to be converted.

   ERRMSG is a buffer to be allocated that contains any error messages.

   Its caller's responsibility to deallocate the buffer (ERRMSG).

   If ERRMSG is NULL, nothing happens on the buffer.

   In case of error returns -1 and sets errmsg appropriately.  */

int pk_mi_json_to_val (pk_val *value, const char *str, char **errmsg);

/* Given a pk val, return the json object associated with this val

   VAL is the pk_val to be converted.

   ERRMSG is a buffer to be allocated that contains any error messages.

   Its caller's responsibility to deallocate the buffer (ERRMSG).

   If ERRMSG is NULL, nothing happens on the buffer.

   In case of error return NULL.  */

const char *pk_mi_val_to_json (pk_val val, char **errmsg);

#endif /* ! PK_MI_JSON */
