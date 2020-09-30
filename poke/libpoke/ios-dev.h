/* ios-dev.h - IO devices interface.  */

/* Copyright (C) 2019, 2020 Jose E. Marchesi */

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

/* An IO space operates on one or more "IO devices", which are
   abstractions providing pread and pwrite byte-oriented operations.
   Typical abstracted entities are files stored in some file system,
   the memory of a process, etc.

   Since the IO devices are byte-oriented, aspects like endianness,
   alignment and negative encoding are not of consideration.

   IOD offsets shall always be interpreted as numbers of bytes.  */

typedef uint64_t ios_dev_off;

/* The following macros are part of the device interface.  */



/* Error codes to be used in the interface below.  */

#define IOD_ERROR  -1 /* Generic error.     */
#define IOD_EOF    -2
#define IOD_EINVAL -3 /* Invalid argument.  */

/* Each IO backend should implement a device interface, by filling an
   instance of the struct defined below.  */

struct ios_dev_if
{
  /* Determine whether the provided HANDLER is recognized as a valid
     device spec by this backend, and if so, return its normalized
     form (caller will free).  If not, return NULL.  */

  char *(*handler_normalize) (const char *handler, uint64_t flags);

  /* Open a device using the provided HANDLER.  Return the opened
     device, or NULL if there was an error.  In case of invalid flags,
     store IOD_EINVAL in ERROR.  Note that this function assumes that
     HANDLER is recognized as a handler by the backend, i.e. HANDLER_P
     returns 1 if HANDLER is passed to it.  */

  void *(*open) (const char *handler, uint64_t flags, int *error);

  /* Close the given device.  Return 0 if there was an error during

     the operation, 1 otherwise.  */

  int (*close) (void *dev);

  /* Read a small byte buffer from the given device at the given byte offset.
     Return 0 on success, or IOD_EOF on error, including on short reads.  */

  int (*pread) (void *dev, void *buf, size_t count, ios_dev_off offset);

  /* Write a small byte buffer to the given device at the given byte offset.
     Return 0 on success, or IOD_EOF on error, including short writes.  */

  int (*pwrite) (void *dev, const void *buf, size_t count, ios_dev_off offset);

  /* Return the flags of the device, as it was opened.  */

  uint64_t (*get_flags) (void *dev);

  /* Return the size of the device, in bytes.  */

  ios_dev_off (*size) (void *dev);

  /* If called on a in-stream, free the buffer before OFFSET.  If called on
     an out-stream, flush the data till OFFSET and free the buffer before
     OFFSET.  Otherwise, do not do anything.  Return IOS_OK ın success and
     an error code on failure.  */
  int (*flush) (void *dev, ios_dev_off offset);
};

#define IOS_FILE_HANDLER_NORMALIZE(handler, newhandler)                 \
  do                                                                    \
    {                                                                   \
      /* File devices are special, in the sense that they accept any */ \
      /* handler. However, we want to ensure that the ios name is */    \
      /* unambiguous from other ios devices, by prepending ./ to */     \
      /* relative names that might otherwise be confusing.  */          \
      static const char safe[] =                                        \
        "abcdefghijklmnopqrstuvwxyz"                                    \
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"                                    \
        "0123456789/+_-";                                               \
                                                                        \
      if (handler[0] == '/'                                             \
          || strspn (handler, safe) == strlen (handler))                \
        (newhandler) = strdup ((handler));                              \
      else if (asprintf (&(newhandler), "./%s", (handler)) == -1)       \
        (newhandler) = NULL;                                            \
    }                                                                   \
  while (0)
