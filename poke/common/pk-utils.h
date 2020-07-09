/* pk-utils.h - Common utility functions for poke.  */

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

#ifndef PK_UTILS_H
#define PK_UTILS_H

#include <config.h>

#include <string.h>
#include <stdint.h>

/* Macros to avoid using strcmp directly.  */

#define STREQ(a, b) (strcmp (a, b) == 0)
#define STRNEQ(a, b) (strcmp (a, b) != 0)

/* Returns zero iff FILENAME is the name
   of an entry in the file system which :
   * is not a directory;
   * is readable; AND
   * exists.
   If it satisfies the above, the function returns NULL.
   Otherwise, returns a pointer to a statically allocated
   error message describing how the file doesn't satisfy
   the conditions.  */

char *pk_file_readable (const char *filename);

/* Integer exponentiation by squaring, for both signed and unsigned
   integers.  */

int64_t pk_ipow (int64_t base, uint32_t exp);
uint64_t pk_upow (uint64_t base, uint32_t exp);

/* Print the given unsigned 64-bit integer in binary. */
void pk_print_binary (void (*puts_fn) (const char *str), uint64_t val, int size, int sign);

/* Concatenate string arguments into an malloc'ed string. */
char *pk_str_concat (const char *s0, ...) __attribute__ ((sentinel));

/* Replace all occurrences of SEARCH within IN by REPLACE. */
char *pk_str_replace (const char *in, const char *search, const char *replace);

/* Left and rigth trim the given string from whitespaces.  */
void pk_str_trim (char **str);

#endif /* ! PK_UTILS_H */
