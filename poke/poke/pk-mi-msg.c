/* pk-mi-msg.h - Machine Interface messages */

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

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "pk-mi.h" /* For MI_VERSION */
#include "pk-mi-msg.h"

/*** Data structures.  ***/

/* Requests are initiated by the client.

   Once a request is sent, it will trigger a response.  The response
   is paired with the triggering request by the request's sequence
   number.

   TYPE identifies the kind of request.  It is one of the PK_MI_REQ_*
   enumerated values defined below.

   ARGS are the arguments of the request.  They depend on the specific
   request type, and are described below.

   The following request types are supported:

   PK_MI_REQ_EXIT requests poke to finalize and exit.  */

#define PK_MI_REQ_TYPE(REQ) ((REQ)->type)

struct pk_mi_req
{
  enum pk_mi_req_type type;

  union
  {
  } args;
};

typedef struct pk_mi_req *pk_mi_req;

/* Responses are initiated by poke, in response to a request received
   from the client.

   REQ_NUMBER is the sequence number of the request for which this is
   a response.

   SUCCESS_P is the outcome of the request.  A value of 0 means the
   request wasn't successful.  A value other than 0 means the request
   was successful.

   ERRMSG points to a NULL-terminated string if SUCCESS_P is 0.  This
   string describes the error condition that prevented the request to
   be performed successfully.

   RESULT is the result of the associated request.  Its contents
   depend on the specific request type, and are described below.

   The following responses are supported:

   PK_MI_RESP_EXIT is the response to a PK_MI_REQ_EXIT request.  */

#define PK_MI_RESP_TYPE(RESP) ((RESP)->type)
#define PK_MI_RESP_REQ_NUMBER(RESP) ((RESP)->req_number)
#define PK_MI_RESP_SUCCESS_P(RESP) ((RESP)->success_p)
#define PK_MI_RESP_ERRMSG(RESP) ((RESP)->errmsg)

struct pk_mi_msg;

struct pk_mi_resp
{
  enum pk_mi_resp_type type;
  pk_mi_seqnum req_number;
  int success_p;
  char *errmsg;

  union
  {
  } result;
};

typedef struct pk_mi_resp *pk_mi_resp;

/* Events are initiated by poke.

   TYPE identifies the kind of event.  It is one of the PK_MI_EVENT_*
   enumerated values described below.

   ARGS contains the arguments of the event.  Their contents depend on
   the specific request type, and are described below.

   The following events are supported:

   PK_MI_EVENT_INITIALIZED indicates the client that poke is
   initialized and ready to process requests.  No request shall be
   sent to poke until this event is received.  This event is sent just
   once.  This event has the following arguments:

      INITIALIZED_MI_VERSION is an integer specifying the version of
      the MI protocol that this poke speaks.

      INITIALIZED_VERSION is a NULL-terminated string with the
      version of the poke program sending the event.
*/

#define PK_MI_EVENT_TYPE(EVENT) ((EVENT)->type)
#define PK_MI_EVENT_INITIALIZED_MI_VERSION(EVENT) ((EVENT)->args.initialized.mi_version)
#define PK_MI_EVENT_INITIALIZED_VERSION(EVENT) ((EVENT)->args.initialized.version)

struct pk_mi_event
{
  enum pk_mi_event_type type;
  union
  {
    struct
    {
      /* MI version this poke speaks.  */
      int mi_version;
      /* String with the version of poke.  */
      char *version;
    } initialized;

  } args;
};

typedef struct pk_mi_event *pk_mi_event;

/* Messages.  */

#define PK_MI_MSG_NUMBER(MSG) ((MSG)->number)
#define PK_MI_MSG_TYPE(MSG) ((MSG)->type)
#define PK_MI_MSG_REQUEST(MSG) ((MSG)->data.request)
#define PK_MI_MSG_RESPONSE(MSG) ((MSG)->data.response)
#define PK_MI_MSG_EVENT(MSG) ((MSG)->data.event)

struct pk_mi_msg
{
  pk_mi_seqnum number;
  enum pk_mi_msg_type type;
  union
  {
    struct pk_mi_req *request;
    struct pk_mi_resp *response;
    struct pk_mi_event *event;
  } data;
};

typedef struct pk_mi_msg *pk_mi_msg;

/*** Variables and code  ***/

/* Global with the next available message sequence number.  */
static pk_mi_seqnum next_seqnum;

static pk_mi_req
pk_mi_make_req (enum pk_mi_req_type type)
{
  pk_mi_req req = malloc (sizeof (struct pk_mi_req));

  if (req)
    PK_MI_REQ_TYPE (req) = type;

  return req;
}

static pk_mi_resp
pk_mi_make_resp (enum pk_mi_resp_type type)
{
  pk_mi_resp resp = malloc (sizeof (struct pk_mi_resp));

  if (resp)
    PK_MI_RESP_TYPE (resp) = type;

  return resp;
}

static pk_mi_event
pk_mi_make_event (enum pk_mi_event_type type)
{
  pk_mi_event event = malloc (sizeof (struct pk_mi_event));

  if (event)
    {
      PK_MI_EVENT_TYPE (event) = type;

      switch (type)
        {
        case PK_MI_EVENT_INITIALIZED:
          PK_MI_EVENT_INITIALIZED_VERSION (event) = NULL;
          break;
        default:
          assert (0);
        }
    }

  return event;
}

static pk_mi_msg
pk_mi_make_msg (enum pk_mi_msg_type type)
{
  pk_mi_msg msg = malloc (sizeof (struct pk_mi_msg));

  if (msg)
    {
      msg->number = next_seqnum++;
      msg->type = type;
    }

  return msg;
}

void
pk_mi_req_free (pk_mi_req req)
{
  if (req)
    {
      switch (PK_MI_REQ_TYPE (req))
        {
        case PK_MI_REQ_EXIT:
          /* Nothing to do.  */
          break;
        default:
          assert (0);
        }

      free (req);
    }
}

void
pk_mi_resp_free (pk_mi_resp resp)
{
  if (resp)
    {
      switch (PK_MI_RESP_TYPE (resp))
        {
        case PK_MI_RESP_EXIT:
          /* Nothing to do here.  */
          break;
        default:
          assert (0);
        }

      free (PK_MI_RESP_ERRMSG (resp));
      free (resp);
    }
}

void
pk_mi_event_free (pk_mi_event event)
{
  if (event)
    {
      switch (PK_MI_EVENT_TYPE (event))
        {
        case PK_MI_EVENT_INITIALIZED:
          free (PK_MI_EVENT_INITIALIZED_VERSION (event));
          break;
        default:
          assert (0);
        }

      free (event);
    }
}

pk_mi_req
pk_mi_req_dup (pk_mi_req req)
{
  pk_mi_req new = pk_mi_make_req (PK_MI_REQ_TYPE (req));

  if (new)
    {
      switch (PK_MI_REQ_TYPE (req))
        {
        case PK_MI_REQ_EXIT:
          /* Nothing to do.  */
          break;
        default:
          assert (0);
        }
    }

  return new;
}

pk_mi_resp
pk_mi_resp_dup (pk_mi_resp resp)
{
  pk_mi_resp new = pk_mi_make_resp (PK_MI_RESP_TYPE (resp));

  if (new)
    {
      switch (PK_MI_RESP_TYPE (resp))
        {
        case PK_MI_RESP_EXIT:
          /* Nothing to do here.  */
          break;
        default:
          assert (0);
        }

      PK_MI_RESP_REQ_NUMBER (new) = PK_MI_RESP_REQ_NUMBER (resp);
      PK_MI_RESP_SUCCESS_P (new) = PK_MI_RESP_SUCCESS_P (resp);

      PK_MI_RESP_ERRMSG (new) = strdup (PK_MI_RESP_ERRMSG (resp));
      if (!PK_MI_RESP_ERRMSG (new))
        {
          free (new);
          return NULL;
        }
    }

  return new;
}

pk_mi_event
pk_mi_event_dup (pk_mi_event event)
{
  pk_mi_event new = pk_mi_make_event (PK_MI_EVENT_TYPE (event));

  if (new)
    {
      switch (PK_MI_EVENT_TYPE (event))
        {
        case PK_MI_EVENT_INITIALIZED:
          PK_MI_EVENT_INITIALIZED_VERSION (new)
            = strdup PK_MI_EVENT_INITIALIZED_VERSION (event);

          if (!PK_MI_EVENT_INITIALIZED_VERSION (new))
            {
              free (new);
              return NULL;
            }
          break;
        default:
          assert (0);
        }
    }

  return new;
}

pk_mi_msg
pk_mi_msg_dup (pk_mi_msg msg)
{
  pk_mi_msg new = pk_mi_make_msg (PK_MI_MSG_TYPE (msg));

  if (new)
    {
      switch (PK_MI_MSG_TYPE (msg))
        {
        case PK_MI_MSG_REQUEST:
          PK_MI_MSG_REQUEST (new)
            = pk_mi_req_dup (PK_MI_MSG_REQUEST (msg));

          if (!PK_MI_MSG_REQUEST (new))
            {
              free (new);
              return NULL;
            }
          break;
        case PK_MI_MSG_RESPONSE:
          PK_MI_MSG_RESPONSE (new)
            = pk_mi_resp_dup (PK_MI_MSG_RESPONSE (msg));

          if (!PK_MI_MSG_RESPONSE (new))
            {
              free (new);
              return NULL;
            }
          break;
        case PK_MI_MSG_EVENT:
          PK_MI_MSG_EVENT (new)
            = pk_mi_event_dup (PK_MI_MSG_EVENT (msg));

          if (!PK_MI_MSG_EVENT (new))
            {
              free (new);
              return NULL;
            }
        default:
          assert (0);
        }
    }

  return new;
}

pk_mi_msg
pk_mi_make_req_exit (void)
{
  pk_mi_req req;
  pk_mi_msg msg;

  req = pk_mi_make_req (PK_MI_REQ_EXIT);
  if (!req)
    return NULL;

  msg = pk_mi_make_msg (PK_MI_MSG_REQUEST);
  if (!msg)
    {
      free (req);
      return NULL;
    }

  PK_MI_MSG_REQUEST (msg) = req;
  return msg;
}

pk_mi_msg pk_mi_make_resp_exit (pk_mi_seqnum req_seqnum,
                                int success_p, const char *errmsg)
{
  pk_mi_resp resp;
  pk_mi_msg msg;

  resp = pk_mi_make_resp (PK_MI_RESP_EXIT);
  if (!resp)
    return NULL;

  PK_MI_RESP_REQ_NUMBER (resp) = req_seqnum;
  PK_MI_RESP_SUCCESS_P (resp) = success_p;
  if (errmsg)
    {
      PK_MI_RESP_ERRMSG (resp) = strdup (errmsg);
      if (!PK_MI_RESP_ERRMSG (resp))
        {
          free (resp);
          return NULL;
        }
    }
  else
    PK_MI_RESP_ERRMSG (resp) = NULL;

  msg = pk_mi_make_msg (PK_MI_MSG_RESPONSE);
  if (!msg)
    {
      free (resp);
      return NULL;
    }

  PK_MI_MSG_RESPONSE (msg) = resp;
  return msg;
}

pk_mi_msg
pk_mi_make_event_initialized (const char *version)
{
  pk_mi_event event;
  pk_mi_msg msg;

  event = pk_mi_make_event (PK_MI_EVENT_INITIALIZED);
  if (!event)
    return NULL;

  PK_MI_EVENT_INITIALIZED_MI_VERSION (event) = MI_VERSION;
  PK_MI_EVENT_INITIALIZED_VERSION (event) = strdup (version);
  if (!PK_MI_EVENT_INITIALIZED_VERSION (event))
    {
      free (event);
      return NULL;
    }

  msg = pk_mi_make_msg (PK_MI_MSG_EVENT);
  if (!msg)
    {
      free (event);
      return NULL;
    }

  PK_MI_MSG_EVENT (msg) = event;
  return msg;
}

void
pk_mi_msg_free (pk_mi_msg msg)
{
  if (msg)
    {
      switch (PK_MI_MSG_TYPE (msg))
        {
        case PK_MI_MSG_REQUEST:
          pk_mi_req_free (PK_MI_MSG_REQUEST (msg));
          break;
        case PK_MI_MSG_RESPONSE:
          pk_mi_resp_free (PK_MI_MSG_RESPONSE (msg));
          break;
        case PK_MI_MSG_EVENT:
          pk_mi_event_free (PK_MI_MSG_EVENT (msg));
          break;
        default:
          assert (0);
        }

      free (msg);
    }
}

enum pk_mi_msg_type
pk_mi_msg_type (pk_mi_msg msg)
{
  return PK_MI_MSG_TYPE (msg);
}

pk_mi_seqnum
pk_mi_msg_number (pk_mi_msg msg)
{
  return PK_MI_MSG_NUMBER (msg);
}

void
pk_mi_set_msg_number (pk_mi_msg msg, pk_mi_seqnum number)
{
  PK_MI_MSG_NUMBER (msg) = number;
}

enum pk_mi_req_type
pk_mi_msg_req_type (pk_mi_msg msg)
{
  return PK_MI_REQ_TYPE (PK_MI_MSG_REQUEST (msg));
}

enum pk_mi_resp_type
pk_mi_msg_resp_type (pk_mi_msg msg)
{
  return PK_MI_RESP_TYPE (PK_MI_MSG_RESPONSE (msg));
}

pk_mi_seqnum
pk_mi_msg_resp_req_number (pk_mi_msg msg)
{
  return PK_MI_RESP_REQ_NUMBER (PK_MI_MSG_RESPONSE (msg));
}

int
pk_mi_msg_resp_success_p (pk_mi_msg msg)
{
  return PK_MI_RESP_SUCCESS_P (PK_MI_MSG_RESPONSE (msg));
}

const char *
pk_mi_msg_resp_errmsg (pk_mi_msg msg)
{
  return PK_MI_RESP_ERRMSG (PK_MI_MSG_RESPONSE (msg));
}

enum pk_mi_event_type
pk_mi_msg_event_type (pk_mi_msg msg)
{
  return PK_MI_EVENT_TYPE (PK_MI_MSG_EVENT (msg));
}

const char *
pk_mi_msg_event_initialized_version (pk_mi_msg msg)
{
  return PK_MI_EVENT_INITIALIZED_VERSION (PK_MI_MSG_EVENT (msg));
}

int
pk_mi_msg_event_initialized_mi_version (pk_mi_msg msg)
{
  return PK_MI_EVENT_INITIALIZED_MI_VERSION (PK_MI_MSG_EVENT (msg));
}
