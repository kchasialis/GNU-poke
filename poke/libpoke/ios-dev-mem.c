/* ios-dev-mem.c - Memory IO devices.  */

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

#include <string.h>
#include <stdlib.h>

#include "ios.h"
#include "ios-dev.h"

/* State asociated with a memory device.  */

struct ios_dev_mem
{
  char *pointer;
  size_t size;
  uint64_t flags;
};

#define MEM_STEP (512 * 8)

static char *
ios_dev_mem_handler_normalize (const char *handler)
{
  if (handler[0] == '*' && handler[strlen (handler) - 1] == '*')
    return strdup (handler);
  return NULL;
}

static void *
ios_dev_mem_open (const char *handler, uint64_t flags, int *error)
{
  struct ios_dev_mem *mio = malloc (sizeof (struct ios_dev_mem));

  if (!mio)
    return NULL;

  mio->pointer = calloc (MEM_STEP, 1);
  if (!mio->pointer)
    {
      free (mio);
      return NULL;
    }

  mio->size = MEM_STEP;
  mio->flags = flags;

  return mio;
}

static int
ios_dev_mem_close (void *iod)
{
  struct ios_dev_mem *mio = iod;

  free (mio->pointer);
  free (mio);

  return 1;
}

static uint64_t
ios_dev_mem_get_flags (void *iod)
{
  struct ios_dev_mem *mio = iod;

  return mio->flags;
}

static int
ios_dev_mem_pread (void *iod, void *buf, size_t count, ios_dev_off offset)
{
  struct ios_dev_mem *mio = iod;

  if (offset + count > mio->size)
    return IOD_EOF;

  memcpy (buf, &mio->pointer[offset], count);
  return 0;
}

static int
ios_dev_mem_pwrite (void *iod, const void *buf, size_t count,
                    ios_dev_off offset)

{
  struct ios_dev_mem *mio = iod;

  if (offset + count > mio->size + MEM_STEP)
    return IOD_EOF;

  if (offset + count > mio->size) {
    void *pointer_bak = mio->pointer;

    mio->pointer = realloc (mio->pointer, mio->size + MEM_STEP);
    if (!mio->pointer)
      {
        /* Restore pointer after failed realloc and return error. */
        mio->pointer = pointer_bak;
        return IOD_ERROR;
      }

    memset (&mio->pointer[mio->size], 0, MEM_STEP);
    mio->size += MEM_STEP;
  }

  memcpy (&mio->pointer[offset], buf, count);
  return 0;
}

static ios_dev_off
ios_dev_mem_size (void *iod)
{
  struct ios_dev_mem *mio = iod;
  return mio->size;
}

struct ios_dev_if ios_dev_mem
  __attribute__ ((visibility ("hidden"))) =
  {
   .handler_normalize = ios_dev_mem_handler_normalize,
   .open = ios_dev_mem_open,
   .close = ios_dev_mem_close,
   .pread = ios_dev_mem_pread,
   .pwrite = ios_dev_mem_pwrite,
   .get_flags = ios_dev_mem_get_flags,
   .size = ios_dev_mem_size,
  };
