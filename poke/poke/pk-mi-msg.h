/* pk-mi-msg.h - Machine Interface messages.  */

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

#ifndef PK_MI_PROT
#define PK_MI_PROT

#include <config.h>
#include <stdint.h>

/* Each MI message contains a "sequence number".  The protocol uses
   this number to univocally identify certain messages.  */

typedef uint32_t pk_mi_seqnum;

/* Types of messages, requests, responses and events.  */

enum pk_mi_msg_type
{
  PK_MI_MSG_REQUEST,
  PK_MI_MSG_RESPONSE,
  PK_MI_MSG_EVENT,
};

enum pk_mi_req_type
{
  PK_MI_REQ_EXIT,
};

enum pk_mi_resp_type
{
  PK_MI_RESP_EXIT,
};

enum pk_mi_event_type
{
  PK_MI_EVENT_INITIALIZED,
};

/* The opaque pk_mi_msg type is fully defined in pk-mi-msg.c */

typedef struct pk_mi_msg *pk_mi_msg;

/*** API for building messages.  ***/

/* Requests.

   The argument accepted by specific request constructors are
   described before the corresponding prototype.

   The request constructors below return NULL in case there is any
   error, such as out of memory.  */

pk_mi_msg pk_mi_make_req_exit (void);

/* Responses.

   The response constructors below get some arguments which are common
   to all of them:

   REQ_SEQNUM is the sequence number of the request that is answered
   by this response.

   SUCCESS_P should be 0 if the requested operation couldn't be
   performed for whatever reason.

   ERRMSG is a pointer to a NULL-terminated string describing the
   reason why the operation couldn't be performed.  This argument
   should be NULL in responses in which SUCCESS_P is not 0.

   Many constructors accept other arguments.  These are described
   below before the individual constructor prototypes.

   If there is an error running a constructor, such as out of memory,
   then NULL is returned.  */

/* Build and return an EXIT response.  */

pk_mi_msg pk_mi_make_resp_exit (pk_mi_seqnum req_seqnum,
                                int success_p, const char *errmsg);

/* Events.

   The arguments accepted by specific event constructors are described
   before the corresponding prototype.

   The event constructors below return NULL in case there is any
   error, such as out of memory.  */

/* Build and return an INITIALIZED event.

   VERSION is a NULL-terminated string containing the version of the
   server program (i.e. poke) issuing the event.  */

pk_mi_msg pk_mi_make_event_initialized (const char *version);

/*** API for getting properties of messages.   */

enum pk_mi_msg_type pk_mi_msg_type (pk_mi_msg msg);
pk_mi_seqnum pk_mi_msg_number (pk_mi_msg msg);

enum pk_mi_req_type pk_mi_msg_req_type (pk_mi_msg msg);

enum pk_mi_resp_type pk_mi_msg_resp_type (pk_mi_msg msg);
pk_mi_seqnum pk_mi_msg_resp_req_number (pk_mi_msg msg);
int pk_mi_msg_resp_success_p (pk_mi_msg msg);
const char *pk_mi_msg_resp_errmsg (pk_mi_msg msg);

enum pk_mi_event_type pk_mi_msg_event_type (pk_mi_msg msg);
const char *pk_mi_msg_event_initialized_version (pk_mi_msg msg);
int pk_mi_msg_event_initialized_mi_version (pk_mi_msg msg);

/*** Other operations on messages.  ***/

/* Set the sequence number of a given MSG.  */

void pk_mi_set_msg_number (pk_mi_msg msg, pk_mi_seqnum number);

/* Free the resources used by the given message MSG.  */

void pk_mi_msg_free (pk_mi_msg msg);

#endif /* ! PK_MI_PROT */
