/* ios-dev-nbd.c - NBD IO devices.  */

/* Copyright (C) 2020 Eric Blake */

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

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <libnbd.h>

#include "ios.h"
#include "ios-dev.h"

/* State associated with an NBD device.  */

struct ios_dev_nbd
{
  struct nbd_handle *nbd;
  char *uri;
  ios_dev_off size;
  uint64_t flags;
};

static bool
startswith (const char *str, const char *prefix)
{
  return strncmp (str, prefix, strlen (prefix)) == 0;
}

static char *
ios_dev_nbd_handler_normalize (const char *handler)
{
  if (startswith (handler, "nbd://")
      || startswith (handler, "nbd+unix://"))
    return strdup (handler);
  return NULL;
}

static void *
ios_dev_nbd_open (const char *handler, uint64_t flags, int *error)
{
  struct ios_dev_nbd *nio = NULL;
  struct nbd_handle *nbd = NULL;
  uint8_t flags_mode = flags & IOS_FLAGS_MODE;
  int err = IOD_ERROR;
  int64_t size;

  /* We don't permit truncation.  */
  if (flags_mode & IOS_F_TRUNCATE)
    {
      err = IOD_EINVAL;
      goto err;
    }

  /* We have to connect before we know if server permits writes.  */
  nbd = nbd_create ();
  if (nbd == NULL)
    goto err;

  if (nbd_connect_uri (nbd, handler) == -1)
    goto err;

  if (flags_mode & IOS_F_WRITE && nbd_is_read_only (nbd))
    {
      err = IOD_EINVAL;
      goto err;
    }
  else if (flags_mode == 0)
    {
      flags |= IOS_F_READ;
      if (nbd_is_read_only (nbd) == 0)
        flags |= IOS_F_WRITE;
    }

  size = nbd_get_size (nbd);
  if (size < 0)
    goto err;

  nio = malloc (sizeof *nio);
  if (!nio)
    goto err;

  nio->uri = strdup (handler);
  if (!nio->uri)
    goto err;

  nio->nbd = nbd;
  nio->size = size;
  nio->flags = flags;

  return nio;

 err:
  /* Worth logging nbd_get_error ()?  */
  if (error)
    *error = err;

  if (nio)
    {
      free (nio->uri);
      free (nio);
    }

  nbd_close (nbd);

  return NULL;
}

static int
ios_dev_nbd_close (void *iod)
{
  struct ios_dev_nbd *nio = iod;

  /* Should this flush when possible?  */
  nbd_close (nio->nbd);
  free (nio->uri);
  free (nio);

  return 1;
}

static uint64_t
ios_dev_nbd_get_flags (void *iod)
{
  struct ios_dev_nbd *nio = iod;

  return nio->flags;
}

static int
ios_dev_nbd_pread (void *iod, void *buf, size_t count, ios_dev_off offset)
{
  struct ios_dev_nbd *nio = iod;

  return nbd_pread (nio->nbd, buf, count, offset, 0) == -1 ? IOD_EOF : 0;
}

static int
ios_dev_nbd_pwrite (void *iod, const void *buf, size_t count,
                    ios_dev_off offset)
{
  struct ios_dev_nbd *nio = iod;

  return nbd_pwrite (nio->nbd, buf, count, offset, 0) == -1 ? IOD_EOF : 0;
}

static ios_dev_off
ios_dev_nbd_size (void *iod)
{
  struct ios_dev_nbd *nio = iod;

  return nio->size;
}

struct ios_dev_if ios_dev_nbd
  __attribute__ ((visibility ("hidden"))) =
  {
   .handler_normalize = ios_dev_nbd_handler_normalize,
   .open = ios_dev_nbd_open,
   .close = ios_dev_nbd_close,
   .pread = ios_dev_nbd_pread,
   .pwrite = ios_dev_nbd_pwrite,
   .get_flags = ios_dev_nbd_get_flags,
   .size = ios_dev_nbd_size,
  };
