/* ios.h - IO spaces for poke.  */

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

#ifndef IOS_H
#define IOS_H

#include <config.h>
#include <stdint.h>
#include <stdbool.h>

/* The following two functions intialize and shutdown the IO poke
   subsystem.  */

void ios_init (void)
  __attribute__ ((visibility ("hidden")));

void ios_shutdown (void)
    __attribute__ ((visibility ("hidden")));

/* "IO spaces" are the entities used in poke in order to abstract the
   heterogeneous devices that are suitable to be edited, such as
   files, file systems, memory images of processes, etc.

        "IO spaces"               "IO devices"

   Space of IO objects <=======> Space of bytes

                             +------+
                      +----->| File |
       +-------+      |      +------+
       |  IO   |      |
       | space |<-----+      +---------+
       |       |      +----->| Process |
       +-------+      |      +---------+

                      :           :

                      |      +-------------+
                      +----->| File system |
                             +-------------+

   IO spaces are bit-addressable spaces of "IO objects", which can be
   generally read (peeked) and written (poked).  The kind of objects
   supported are:

   - "ints", which are signed integers from 1 to 64 bits wide.  They
     can be stored using either msb or lsb endianness.  Negative
     quantities are encoded using one of the supported negative
     encodings.

   - "uints", which are unsigned integers from 1 to 64 bits wide.
     They can be stored using either msb or lsb endianness.

   - "strings", which are sequences of bytes terminated by a NULL
     byte, much like C strings.

   IO spaces also provide caching capabilities, transactions,
   serialization of concurrent accesses, and more goodies.  */

typedef struct ios *ios;

/* IO spaces are bit-addressable.  "Offsets" characterize positions
   into IO spaces.

   Offsets are encoded in 64-bit integers, which denote the number of
   bits since the beginning of the space.  They can be added,
   subtracted and multiplied.

   Since negative offsets are possible, the maximum size of any given
   IO space is 2^60 bytes.  */

typedef int64_t ios_off;

/* The following status codes are used in the several APIs defined
   below in the file.  */

#define IOS_OK 0       /* The operation was performed to completion, in
                          the expected way.  */

#define IOS_ERROR -1   /* An unspecified error condition happened.  */

#define IOS_ENOMEM -5  /* Memory allocation failure.  */

/* **************** IOS flags ******************************

   The 64-bit unsigned flags associated with IO spaces have the
   following structure:


    63                   32 31              8 7     0
   |  IOD-specific flags   |  generic flags  |  mode |

*/

#define IOS_FLAGS_MODE 0xff

/* Mode flags.  */

#define IOS_F_READ   1
#define IOS_F_WRITE  2
#define IOS_F_TRUNCATE 8
#define IOS_F_CREATE 16

#define IOS_M_RDONLY (IOS_F_READ)
#define IOS_M_WRONLY (IOS_F_WRITE)
#define IOS_M_RDWR (IOS_F_READ | IOS_F_WRITE)

/* **************** IO space collection API ****************

   The collection of open IO spaces are organized in a global list.
   At every moment some given space is the "current space", unless
   there are no spaces open:

          space1  ->  space2  ->  ...  ->  spaceN

                        ^
                        |

                      current

   The functions declared below are used to manage this
   collection.  */


/* Open an IO space using a handler and if set_cur is set to 1, make
   the newly opened IO space the current space.  Return IOS_ERROR if
   there is an error opening the space (such as an unrecognized
   handler), the ID of the new IOS otherwise.

   FLAGS is a bitmask.  The least significant 32 bits are
   reserved for common flags (the IOS_F_* above).  The most
   significant 32 bits are reserved for IOD specific flags.

   If no IOS_F_READ or IOS_F_WRITE flags are specified, then the IOS
   will be opened in whatever mode makes more sense.  */

int ios_open (const char *handler, uint64_t flags, int set_cur)
  __attribute__ ((visibility ("hidden")));

/* Close the given IO space, freing all used resources and flushing
   the space cache associated with the space.  */

void ios_close (ios io)
  __attribute__ ((visibility ("hidden")));

/* Return the flags which are active in a given IO.  Note that this
   doesn't necessarily correspond to the flags passed when opening the
   IO space: some IOD backends modify them.  */

uint64_t ios_flags (ios io)
  __attribute__ ((visibility ("hidden")));

/* The following function returns the handler operated by the given IO
   space.  */

const char *ios_handler (ios io)
  __attribute__ ((visibility ("hidden")));

/* Return the current IO space, or NULL if there are no open
   spaces.  */

ios ios_cur (void)
  __attribute__ ((visibility ("hidden")));

/* Set the current IO space to IO.  */

void ios_set_cur (ios io)
  __attribute__ ((visibility ("hidden")));

/* Return the IO space operating the given HANDLER.  Return NULL if no
   such space exists.  */

ios ios_search (const char *handler)
  __attribute__ ((visibility ("hidden")));

/* Return the IO space having the given ID.  Return NULL if no such
   space exists.  */

ios ios_search_by_id (int id)
  __attribute__ ((visibility ("hidden")));

/* Return the ID of the given IO space.  */

int ios_get_id (ios io)
  __attribute__ ((visibility ("hidden")));

/* Return the first IO space.  */

ios ios_begin (void)
  __attribute__ ((visibility ("hidden")));

/* Return the space following IO.  */

ios ios_next (const ios io)
  __attribute__ ((visibility ("hidden")));

/* Return true iff IO is past the last one.  */

bool ios_end (const ios io)
  __attribute__ ((visibility ("hidden")));

/* Map over all the open IO spaces executing a handler.  */

typedef void (*ios_map_fn) (ios io, void *data);

void ios_map (ios_map_fn cb, void *data)
  __attribute__ ((visibility ("hidden")));

/* **************** IOS properties************************  */

/* Return the size of the given IO, in bits.  */

uint64_t ios_size (ios io)
  __attribute__ ((visibility ("hidden")));

/* The IOS bias is added to every offset used in a read/write
   operation.  It is signed and measured in bits.  By default it is
   zero, i.e. no bias is applied.

   The following functions set and get the bias of a given IO
   space.  */

ios_off ios_get_bias (ios io)
  __attribute__ ((visibility ("hidden")));

void ios_set_bias (ios io, ios_off bias)
  __attribute__ ((visibility ("hidden")));

/* **************** Object read/write API ****************  */

/* An integer with flags is passed to the read/write operations,
   impacting the way the operation is performed.  */

#define IOS_F_BYPASS_CACHE  1  /* Bypass the IO space cache.  This
                                  makes this write operation to
                                  immediately write to the underlying
                                  IO device.  */

#define IOS_F_BYPASS_UPDATE 2  /* Do not call update hooks that would
                                  be triggered by this write
                                  operation.  Note that this can
                                  obviously lead to inconsistencies
                                  ;) */

/* The functions conforming the read/write API below return an integer
   that reflects the state of the requested operation.  The following
   values are supported, as well as the more generic IOS_OK and
   IOS_ERROR, */

#define IOS_EIOFF -2  /* The provided offset is invalid.  This happens
                         for example when the offset translates into a
                         byte offset that exceeds the capacity of the
                         underlying IO device, or when a negative
                         offset is provided in the wrong context.  */

#define IOS_EIOBJ -3  /* A valid object couldn't be found at the
                         requested offset.  This happens for example
                         when an end-of-file condition happens in the
                         underlying IO device.  */

/* The following error code is returned when the IO backend can't
   handle the specified flags in ios_open. */

#define IOS_EFLAGS -4 /* Invalid flags specified.  */

/* When reading and writing integers from/to IO spaces, it is needed
   to specify some details on how the integers values are encoded in
   the underlying storage.  The following enumerations provide the
   supported byte endianness and negative encodings.  The later are
   obviously used when reading and writing signed integers.  */

enum ios_nenc
  {
   IOS_NENC_1, /* One's complement.  */
   IOS_NENC_2  /* Two's complement.  */
  };

enum ios_endian
  {
   IOS_ENDIAN_LSB, /* Byte little endian.  */
   IOS_ENDIAN_MSB  /* Byte big endian.  */
  };

/* Read a signed integer of size BITS located at the given OFFSET, and
   put its value in VALUE.  It is assumed the integer is encoded using
   the ENDIAN byte endianness and NENC negative encoding.  */

int ios_read_int (ios io, ios_off offset, int flags,
                  int bits,
                  enum ios_endian endian,
                  enum ios_nenc nenc,
                  int64_t *value)
  __attribute__ ((visibility ("hidden")));

/* Read an unsigned integer of size BITS located at the given OFFSET,
   and put its value in VALUE.  It is assumed the integer is encoded
   using the ENDIAN byte endianness.  */

int ios_read_uint (ios io, ios_off offset, int flags,
                   int bits,
                   enum ios_endian endian,
                   uint64_t *value)
  __attribute__ ((visibility ("hidden")));

/* Read a NULL-terminated string of bytes located at the given OFFSET,
   and put its value in VALUE.  It is up to the caller to free the
   memory occupied by the returned string, when no longer needed.  */

int ios_read_string (ios io, ios_off offset, int flags, char **value)
  __attribute__ ((visibility ("hidden")));

/* Write the signed integer of size BITS in VALUE to the space IO, at
   the given OFFSET.  Use the byte endianness ENDIAN and encoding NENC
   when writing the value.  */

int ios_write_int (ios io, ios_off offset, int flags,
                   int bits,
                   enum ios_endian endian,
                   enum ios_nenc nenc,
                   int64_t value)
  __attribute__ ((visibility ("hidden")));

/* Write the unsigned integer of size BITS in VALUE to the space IO,
   at the given OFFSET.  Use the byte endianness ENDIAN when writing
   the value.  */

int ios_write_uint (ios io, ios_off offset, int flags,
                    int bits,
                    enum ios_endian endian,
                    uint64_t value)
  __attribute__ ((visibility ("hidden")));

/* Write the NULL-terminated string in VALUE to the space IO, at the
   given OFFSET.  */

int ios_write_string (ios io, ios_off offset, int flags,
                      const char *value)
  __attribute__ ((visibility ("hidden")));

/* **************** Update API **************** */

/* XXX: writeme.  */

/* XXX: we need functions to temporarily disable updates in a given IO
   range, to be used by the struct writer functions.  */

/* **************** Transaction API **************** */

/* XXX: writeme.  */

#endif /* ! IOS_H */
