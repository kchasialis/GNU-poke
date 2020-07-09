/* pk-mi.c - A Machine Interface for poke.  */

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

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>

#include "poke.h"

#include "pk-mi-msg.h"
#include "pk-mi-json.h"

/* Transport Layer.

   At the transport level the communication is performed in terms of
   "frame messages".

   The layout of each message is:

   deftype PMI_FrameMessage =
    struct
    {
       big uint<4> size : size <= 2048;
       byte[size] payload;
    }

   Where SIZE is the length of the payload, measured in bytes.  The
   maximum lenght of a frame message payload is two kilobytes.  */

#define MAXMSG 2048

static void pk_mi_dispatch_msg (pk_mi_msg msg);

static void
pk_mi_process_frame_msg (int size, char *frame_msg)
{
  pk_mi_msg msg = pk_mi_json_to_msg (frame_msg);

  if (!msg)
    /* Bad message.  Ignore it.  */
    /* XXX: raise an event to communicate this to the client?  */
    return;

  pk_mi_dispatch_msg (msg);
}

static int
pk_mi_read_from_client (int filedes)
{
  static char in_msg_size[4];
  static char in_msg[MAXMSG];
  static unsigned int in_msg_size_bytes_read = 0;
  static unsigned int in_msg_bytes_read = 0;
  static unsigned int msg_size = 0;

  char buffer[MAXMSG];
  ssize_t i, nbytes;

  if (in_msg_size_bytes_read < 4)
    {
      nbytes = read (filedes, buffer,
                     4 - in_msg_size_bytes_read);
      if (nbytes < 0)
        {
          perror ("read");
          goto fatal;
        }
      else if (nbytes == 0)
        goto end_of_file;

      for (i = 0; i < nbytes; ++i)
        {
          in_msg_size[in_msg_size_bytes_read] = buffer[i];
          in_msg_size_bytes_read++;
        }
    }
  else
    {
      msg_size = ((unsigned int) in_msg_size[0] << 24
                  | (unsigned int) in_msg_size[1] << 16
                  | (unsigned int) in_msg_size[2] << 8
                  | (unsigned int) in_msg_size[3]);

      if (msg_size > MAXMSG)
        goto protocol_error;

      nbytes = read (filedes, buffer,
                     msg_size - in_msg_bytes_read);
      if (nbytes < 0)
        {
          perror ("read");
          goto fatal;
        }
      else if (nbytes == 0)
        goto end_of_file;

      for (i = 0; i < nbytes; ++i)
        {
          in_msg[in_msg_bytes_read] = buffer[i];
          in_msg_bytes_read++;
        }
    }

  if (in_msg_bytes_read != 0
      && in_msg_bytes_read == msg_size)
    {
      /* A framed message is ready.  Process it.  */
      pk_mi_process_frame_msg (in_msg_bytes_read, in_msg);

      /* Prepare to receive another message.  */
      in_msg_size_bytes_read = 0;
      in_msg_bytes_read = 0;
      msg_size = 0;
    }

  return 0;

 end_of_file:
  return -1;

 protocol_error:
  return -2;

 fatal:
  pk_fatal (NULL);
  return -1;
}

static void
pk_mi_send_frame_msg (const char *payload)
{
  uint32_t size = strlen (payload) + 1 /* the newline */;

  fputc ((size >> 24 & 0xff), stdout);
  fputc ((size >> 16 & 0xff), stdout);
  fputc ((size >> 8 & 0xff), stdout);
  fputc ((size >> 0 & 0xff), stdout);
  fputs (payload, stdout);
  fputc ('\n', stdout); /* To ease debugging, logging, etc.  */

  fflush (stdout);
}

/* Set fd to non-blocking and return old flags or -1 on error. */
static int pk_mi_fd_set_nonblocking (int fd)
{
  int flags = fcntl (fd, F_GETFL, 0);

  if (flags >= 0 && (flags & O_NONBLOCK) == 0)
    {
      if (fcntl (fd, F_SETFL, flags | O_NONBLOCK))
        flags = -1;
    }

  if (flags < 0)
    perror ("fcntl");

  return flags;
}

/* Restore flags to fd if it was set to blocking previously. */
static void pk_mi_fd_restore_blocking (int fd, int flags)
{
  if (flags >= 0 && (flags & O_NONBLOCK) == 0)
    {
      if (fcntl (fd, F_SETFL, flags) < 0)
        perror ("fcntl");
    }
}

/* This global is used to finalize the MI loop.  */
static int pk_mi_exit_p;

static int
pk_mi_loop (int fd)
{
  fd_set active_fd_set, read_fd_set;
  int old_flags;
  int ret;

  FD_ZERO (&active_fd_set);
  FD_SET (fd, &active_fd_set);

  /* Make sure that fd is non-blocking.
   * From 'man 2 select':
   *  On Linux, select may report a socket file descriptor as
   *  "ready for reading", while nevertheless a subsequent read blocks." */
  old_flags = pk_mi_fd_set_nonblocking (fd);

  pk_mi_exit_p = 0;

  while (1)
    {
      read_fd_set = active_fd_set;
      if (select (FD_SETSIZE, &read_fd_set, NULL, NULL,
                  NULL /* timeout */) < 0)
        {
          perror ("select");
          pk_fatal (NULL);
        }

      if (FD_ISSET (fd, &read_fd_set))
        {
          ret = pk_mi_read_from_client (fd);

          if (pk_mi_exit_p)
            {
              /* Client requested us to close the connection.  */
              ret = 1;
              break;
            }
          else if (ret == -1)
            {
              /* Client closed the connection.  */
              ret = 1;
              break;
            }
          else if (ret == -2)
            {
              /* Protocol error.  */
              ret = 0;
              break;
            }
        }
    }

  pk_mi_fd_restore_blocking (fd, old_flags);

  return ret;
}

/* Message sender.  */

static void
pk_mi_send (pk_mi_msg msg)
{
  const char *payload = pk_mi_msg_to_json (msg);

  if (!payload)
    pk_fatal ("converting MI msg to json");

  pk_mi_send_frame_msg (payload);
}

/* Message dispatcher.  */

static void
pk_mi_dispatch_msg (pk_mi_msg msg)
{
  if (pk_mi_msg_type (msg) == PK_MI_MSG_REQUEST)
    {
      switch (pk_mi_msg_req_type (msg))
        {
        case PK_MI_REQ_EXIT:
          {
            pk_mi_msg resp
              = pk_mi_make_resp_exit (pk_mi_msg_number (msg),
                                      1 /* success_p */,
                                      NULL /* errmsg */);
            pk_mi_send (resp);
            pk_mi_msg_free (resp);
            pk_mi_exit_p = 1;
            break;
          }
        default:
          assert (0);
        }
    }
  else
    {
      /* XXX Send a message dropped event  */
    }

  pk_mi_msg_free (msg);
}


/* Main entry.  */

int
pk_mi (void)
{
  pk_mi_msg initialized_msg
    = pk_mi_make_event_initialized (VERSION);

  if (!initialized_msg)
    return 0;

  pk_mi_send (initialized_msg);
  return pk_mi_loop (STDIN_FILENO);
}
