/* libpoke.h - Public header for libpoke.  */

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

#ifndef LIBPOKE_H
#define LIBPOKE_H

#include <config.h>

#include <stdint.h>
#include <stdarg.h>

typedef struct pk_compiler *pk_compiler;
typedef struct pk_ios *pk_ios;
typedef uint64_t pk_val;

/* The following status codes are returned by many of the functions in
   this interface.  */

#define PK_OK 0
#define PK_ERROR 1

/* Terminal output callbacks.  */

struct pk_term_if
{
  /* Flush the output in the terminal.  */
  void (*flush_fn) (void);

  /* Output a NULL-terminated C string.  */
  void (*puts_fn) (const char *str);

  /* Output a formatted string.  */
  void (*printf_fn) (const char *format, ...);

  /* Output LVL levels of indentation, using STEP white chars in each
     indentation level.  */
  void (*indent_fn) (unsigned int lvl, unsigned int step);

  /* Mark the beginning of a styling class with name CLASS.  */
  void (*class_fn) (const char *class);

  /* Mark the end of a styling class with name CLASS.  */
  void (*end_class_fn) (const char *class);

  /* Mark the beginning of an hyperlink with url URL and identifier
     ID.  The identifier can be NULL.  */
  void (*hyperlink_fn) (const char *url, const char *id);

  /* Mark the end of the current hyperlink.  */
  void (*end_hyperlink_fn) (void);
};

/* Create and return a new Poke incremental compiler.

   RTPATH should contain the name of a directory where the compiler
   can find its run-time support files.

   TERM_IF is a pointer to a struct pk_term_if containing pointers to
   functions providing the output routines to be used by the
   incremental compiler.

   If there is an error creating the compiler this function returns
   NULL.  */

pk_compiler pk_compiler_new (const char *rtpath,
                             struct pk_term_if *term_if);

/* Destroy an instance of a Poke incremental compiler.

   PKC is a previously created incremental compiler.  */

void pk_compiler_free (pk_compiler pkc);

/* Compile a Poke program from the given file FILENAME.

   If not NULL, *EXIT_STATUS is set to the status resulting from the
   execution of the program.

   Return 0 in case of a compilation error, a non-zero value
   otherwise.  */

int pk_compile_file (pk_compiler pkc, const char *filename,
                     int *exit_status);

/* Compile a Poke program from a memory buffer.

   BUFFER is a NULL-terminated string.

   If not NULL, *END is set to the first character in BUFFER that is
   not part of the compiled entity.

   Return 0 in case of a compilation error, a non-zero value
   otherwise.  */

int pk_compile_buffer (pk_compiler pkc, const char *buffer,
                       const char **end);

/* Like pk_compile_buffer but compile and execute a single Poke
   statement, which may evaluate to a value if it is an "expression
   statement".

   VAL, if given, is a pointer to a pk_val variable that is set to the
   result value of an expression-statement, or to PK_NULL.  */

int pk_compile_statement (pk_compiler pkc, const char *buffer,
                          const char **end, pk_val *val);

/* Like pk_compile_buffer but compile and execute a single Poke
   expression, which evaluates to a value.

   VAL, if given, is a pointer to a pk_val variable that is set to the
   result value of executing the expression.  */

int pk_compile_expression (pk_compiler pkc, const char *buffer,
                           const char **end, pk_val *val);

/* Load a module using the given compiler.

   If the module cannot be loaded, return 1.
   Otherwise, return 0.  */

int pk_load (pk_compiler pkc, const char *module);

/* Print a disassembly of a function.

   FNAME is the name of the function to disassemble.  It should be
   defined in the global environment of the incremental compiler.

   NATIVE_P is a flag determining whether to emit a native disassembly
   or a PVM disassembly.

   If there is no function named FNAME, return PK_ERROR.  Otherwise,
   return PK_OK.  */

int pk_disassemble_function (pk_compiler pkc, const char *fname,
                             int native_p);

/* Print a disassembly of an expression.

   STR is a NULL-terminated string containing a Poke expression.

   NATIVE_P is a flag determining whether to emit a native disassembly
   or a PVM disassembly.

   If STR is not a valid Poke expression, return PK_ERROR.  Otherwise
   return PK_OK.  */

int pk_disassemble_expression (pk_compiler pkc, const char *str,
                               int native_p);

/* Set the QUIET_P flag in the compiler.  If this flag is set, the
   incremental compiler emits as few output as possible.  */

void pk_set_quiet_p (pk_compiler pkc, int quiet_p);

/* Install a handler for alien tokens in the incremental compiler.
   The handler gets a string with the token identifier (for $foo it
   would get `foo') and should return a string containing the
   resolving identifier.  If the handler returns NULL, then the alien
   token is not recognized as such in the compiler and ERRMSG points
   to an explicative error message.  */

typedef char *(*pk_alien_token_handler_fn) (const char *id,
                                            char **errmsg);
void pk_set_alien_token_fn (pk_compiler pkc, pk_alien_token_handler_fn cb);

/* Set the LEXICAL_CUCKOLDING_P flag in the compiler.  If this flag is
   set, alien tokens are recognized and processed by calling the
   handler installed by pk_set_alien_token_fn.

   Note that the flag will be honored only if the user has installed a
   handler for alien tokens.  */

void pk_set_lexical_cuckolding_p (pk_compiler pkc,
                                  int lexical_cuckolding_p);

/* Complete the name of a variable, function or type declared in the
   global environment of the given icnremental compiler.

   This function is to be called repeatedly when generating potential
   command line completions.  It returns command line completion based
   upon the current state of PKC.

   TEXT is the partial word to be completed.  STATE is zero the first
   time the function is called and non-zero for each subsequent call.

   On each call, the function returns a potential completion.  It
   returns NULL to indicate that there are no more possibilities
   left. */

char *pk_completion_function (pk_compiler pkc,
                              const char *text, int state);

/* Complete the tag of an IOS.

   TEXT is the partial word to be completed.  STATE is zero the first
   time the function is called and non-zero for each subsequent call.

   On each call, the function returns the tag of an IOS for which
   that tag and TEXT share a common substring. It returns NULL to
   indicate that there are no more such tags.  */

char *pk_ios_completion_function (pk_compiler pkc,
                                  const char *x, int state);

/* Return the handler operated by the given IO space.  */

const char *pk_ios_handler (pk_ios ios);

/* Return the current IO space, or NULL if there are open spaces in
   the given Poke compiler.  */

pk_ios pk_ios_cur (pk_compiler pkc);

/* Set the current IO space in the given incremental compiler.  */

void pk_ios_set_cur (pk_compiler pkc, pk_ios ios);

/* Return the IO space operating the given HANDLER.  Return NULL if no
   such space exist in the given Poke incremental compiler.  */

pk_ios pk_ios_search (pk_compiler pkc, const char *handler);

/* Return the IO space having the given ID.  Return NULL if no such
   space exist in the given Poke incremental compiler.  */

pk_ios pk_ios_search_by_id (pk_compiler pkc, int id);

/* Return the ID of the given IO space.  */

int pk_ios_get_id (pk_ios ios);

/* Return the size of the given IO space, in bits.  */

uint64_t pk_ios_size (pk_ios ios);

/* Return the flags which are active in a given IOS.  */

#define PK_IOS_F_READ     1
#define PK_IOS_F_WRITE    2
#define PK_IOS_F_TRUNCATE 8
#define PK_IOS_F_CREATE  16

uint64_t pk_ios_flags (pk_ios ios);

/* Open an IO space using a handler and if set_cur is set to 1, make
   the newly opened IO space the current space.  Return PK_IOS_ERROR
   if there is an error opening the space (such as an unrecognized
   handler), the ID of the new IOS otherwise.

   FLAGS is a bitmask.  The least significant 32 bits are reserved for
   common flags (the PK_IOS_F_* above).  The most significant 32 bits
   are reserved for IOD specific flags.

   If no PK_IOS_F_READ or PK_IOS_F_WRITE flags are specified, then the
   IOS will be opened in whatever mode makes more sense.  */

#define PK_IOS_OK     0
#define PK_IOS_ERROR -1

int pk_ios_open (pk_compiler pkc,
                 const char *handler, uint64_t flags, int set_cur_p);

/* Close the given IO space, freing all used resources and flushing
   the space cache associated with the space.  */

void pk_ios_close (pk_compiler pkc, pk_ios ios);

/* Map over all the IO spaces in a given incremental compiler,
   executing a handler.  */

typedef void (*pk_ios_map_fn) (pk_ios ios, void *data);
void pk_ios_map (pk_compiler pkc, pk_ios_map_fn cb, void *data);

/* Map over the declarations defined in the given incremental
   compiler, executing a handler.

   PKC is a Poke incremental compiler.

   KIND is one of the PK_DECL_KIND_* values defined below.  It
   determines what subset of declarations will be processed by the
   handler.

   HANDLER is a function of type pk_map_decl_fn.  */

#define PK_DECL_KIND_VAR 0
#define PK_DECL_KIND_FUNC 1
#define PK_DECL_KIND_TYPE 2

typedef void (*pk_map_decl_fn) (int kind,
                                const char *source,
                                const char *name,
                                const char *type,
                                int first_line, int last_line,
                                int first_column, int last_column,
                                void *data);
void pk_decl_map (pk_compiler pkc, int kind,
                  pk_map_decl_fn handler, void *data);

/* Determine whether there is a declaration with the given name, of
   the given type.  */

int pk_decl_p (pk_compiler pkc, const char *name, int kind);

/* Given the name of a variable declared in the compiler, return its
   value.

   If there is no variable defined with name NAME, return PK_NULL.  */

pk_val pk_decl_val (pk_compiler pkc, const char *name);

/* Declare a variable in the global environment of the given
   incremental compiler.

   If the operation is successful, return 1.  If a variable with name
   VARNAME already exists in the environment, return 0.  */

int pk_defvar (pk_compiler pkc, const char *varname, pk_val val);

/* Call a Poke function.

   CLS is the closure for the function to call.

   RET is set to the value returned by the function, or to PK_NULL if
   it is a void function.

   A variable number of function arguments follow, terminated by
   PK_NULL.

   Return 0 if there is a problem performing the operation, or if the
   execution of the function results in an unhandled exception.
   Return 1 otherwise.  */

int pk_call (pk_compiler pkc, pk_val cls, pk_val *ret, ...)
  __attribute__ ((sentinel));

/* Get and set properties of the incremental compiler.  */

int pk_obase (pk_compiler pkc);
void pk_set_obase (pk_compiler pkc, int obase);

unsigned int pk_oacutoff (pk_compiler pkc);
void pk_set_oacutoff (pk_compiler pkc, unsigned int oacutoff);

unsigned int pk_odepth (pk_compiler pkc);
void pk_set_odepth (pk_compiler pkc, unsigned int odepth);

unsigned int pk_oindent (pk_compiler pkc);
void pk_set_oindent (pk_compiler pkc, unsigned int oindent);

int pk_omaps (pk_compiler pkc);
void pk_set_omaps (pk_compiler pkc, int omaps_p);

enum pk_omode
  {
    PK_PRINT_FLAT,
    PK_PRINT_TREE
  };

enum pk_omode pk_omode (pk_compiler pkc);
void pk_set_omode (pk_compiler pkc, enum pk_omode omode);

int pk_error_on_warning (pk_compiler pkc);
void pk_set_error_on_warning (pk_compiler pkc, int error_on_warning_p);

enum pk_endian
  {
    PK_ENDIAN_LSB,
    PK_ENDIAN_MSB
  };

enum pk_endian pk_endian (pk_compiler pkc);
void pk_set_endian (pk_compiler pkc, enum pk_endian endian);

enum pk_nenc
  {
    PK_NENC_1,
    PK_NENC_2
  };

enum pk_nenc pk_nenc (pk_compiler pkc);
void pk_set_nenc (pk_compiler pkc, enum pk_nenc nenc);

int pk_pretty_print (pk_compiler pkc);
void pk_set_pretty_print (pk_compiler pkc, int pretty_print_p);

/*** API for manipulating Poke values.  ***/

/* PK_NULL is an invalid pk_val.
   This value should be the same than PVM_NULL in pvm.h.  */

#define PK_NULL 0x7ULL

/* Signed integers.  */

/* Build and return a Poke integer.

   VALUE is the numerical value for the new integer.
   SIZE is the number of bits composing the integer.

   Return PK_NULL if SIZE exceeds the maximum number of bits supported
   in Poke integers.  */

pk_val pk_make_int (int64_t value, int size);

/* Return the numerical value stored in the given Poke integer.
   VAL should be a Poke value of the right type.  */

int64_t pk_int_value (pk_val val);

/* Return the size (in bits) of the given Poke integer.
   VAL should be a Poke value of the right type.  */

int pk_int_size (pk_val val);

/* Unsigned integers.  */

/* Build and return a Poke unsigned integer.

   VALUE is the numerical value for the new integer.
   SIZE is the number of bits composing the integer.

   Return NULL if SIZE exceeds the maximum number of bits supported in
   Poke integers.  */

pk_val pk_make_uint (uint64_t value, int size);

/* Return the numerical value stored in the given Poke unsigned
   integer.

   VAL should be a Poke value of the right type.  */

uint64_t pk_uint_value (pk_val val);

/* Return the size (in bits) of the given Poke unsigned integer.
   VAL should be a Poke value of the right type.  */

int pk_uint_size (pk_val val);

/* Strings.  */

/* Build and return a Poke string.

   STR is a NULL-terminated string, a copy of which will become the
   value of the Poke string.  */

pk_val pk_make_string (const char *str);

/* Return a NULL-terminated string with the value of the given Poke
   string.  */

const char *pk_string_str (pk_val val);

/* Offsets.  */

/* Build and return a Poke offset.

   MAGNITUDE is either a Poke integer or a Poke unsigned integer, with
   the magnitude of the offset.

   UNIT is an unsigned 64-bit Poke integer specifying the unit of the
   offset, i.e. the multiple of the base unit, which is the bit.

   If any of the arguments is not of the right type return PK_NULL.
   Otherwise return the created offset.  */

pk_val pk_make_offset (pk_val magnitude, pk_val unit);

/* Return the magnitude of the given Poke offset.  */

pk_val pk_offset_magnitude (pk_val val);

/* Return the unit of the given Poke offset.  */

pk_val pk_offset_unit (pk_val val);

/* Structs. */

/* Build and return a Poke struct.

   NFIELDS is an uint<64> Poke value specifying the number of fields
   in the struct.  This can be uint<64>0 for an empty struct.

   TYPE is a type Poke value specifying the type of the struct.

   The fields and methods in the created struct are initialized to
   PK_NULL.  */

pk_val pk_make_struct (pk_val nfields, pk_val type);

/* Get the number of fields of a struct. */

pk_val pk_struct_nfields (pk_val sct);

/* Get the bit-offset of the field of a struct, relative to the
   beginning of the struct.

   SCT is the struct value.
   IDX is the index of the field in the struct.

   The returned bit-offset is an uint<64>.

   If IDX is invalid, PK_NULL is returned. */

pk_val pk_struct_field_boffset (pk_val sct, uint64_t idx);

/* Set the bit-offset of the field of an struct, relative to the
   beginning of the struct.

   ARRAY is the struct value.

   IDX is the index of the field in the struct.

   BOFFSET is an uint<64> value with the bit-offset of the field.

   If IDX is invalid, struct remains unchanged. */

void pk_struct_set_field_boffset (pk_val sct, uint64_t idx, pk_val boffset);

/* Get the NAME of the struct field.

   SCT is the struct value.

   IDX is the index of the field in the struct.

   If IDX is invalid, PK_NULL is returned. */

pk_val pk_struct_field_name (pk_val sct, uint64_t idx);

/* Set the NAME of the struct field.

   SCT is the struct value.

   IDX is the index of the field in the struct.

   NAME is the string name for this field.

   If IDX is invalid, struct remains unchanged. */

void pk_struct_set_field_name (pk_val sct, uint64_t idx, pk_val name);

/* Get the VALUE of the struct field.

   SCT is the struct value.

   IDX is the index of the field in the struct.

   If IDX is invalid, PK_NULL is returned. */

pk_val pk_struct_field_value (pk_val sct, uint64_t idx);

/* Set the VALUE of the struct field.

   SCT is the struct value.

   IDX is the index of the field in the struct.

   VALUE is the new value for this field.

   If IDX is invalid, struct remains unchanged. */

void pk_struct_set_field_value (pk_val sct, uint64_t idx, pk_val value);

/* Arrays.  */

/* Build and return a Poke array.

   NELEM is the number of the elements in the array.
   ARRAY_TYPE is the type of the array.

   The new array is created containing PK_NULL values.  */

pk_val pk_make_array (pk_val nelem, pk_val array_type);

/* Get the number of elements in the given array value, as an
   uint<64>.  */

pk_val pk_array_nelem (pk_val array);

/* Get the value of the element of an array.

   ARRAY is the array value.
   IDX is the index of the element in the array.

   If IDX is invalid, PK_NULL is returned. */

pk_val pk_array_elem_val (pk_val array, uint64_t idx);

/* Set the value of the element of an array.

   ARRAY is the array value.
   IDX is the index of the element whose value is set.
   VAL is the new value for the array element.

   Note that the type of the element is not checked by this function,
   so be careful.

   If IDX is invalid, array remains unchanged. */

void pk_array_set_elem_val (pk_val array, uint64_t idx, pk_val val);

/* Get the bit-offset of the element of an array, relative to the
   beginning of the array.

   ARRAY is the array value.
   IDX is the index of the element in the array.

   The returned bit-offset is an uint<64>.

   If IDX is invalid, PK_NULL is returned. */

pk_val pk_array_elem_boffset (pk_val array, uint64_t idx);

/* Set the bit-offset of the element of an array, relative to the
   beginning of the array.

   ARRAY is the array value.
   IDX is the index of the element in the array.
   BOFFSET is an uint<64> value with the bit-offset of the element.

   If IDX is invalid, array remains unchanged. */

void pk_array_set_elem_boffset (pk_val array, uint64_t idx, pk_val boffset);

/* Integral types.  */

/* Build and return an integral type.

   SIZE is an uint<64> with the size, in bits, of the integral type.
   SIGNED_P is an int<32> with a boolean specifying whether the type
   is signed or not.  */

pk_val pk_make_integral_type (pk_val size, pk_val signed_p);

/* Return an uint<64> containing the size, in bits, of the given
   integral type.  */

pk_val pk_integral_type_size (pk_val type);

/* Return an int<32> with a boolean specifying whether the given
   integral type is signed or not.  */

pk_val pk_integral_type_signed_p (pk_val type);

/* The string type.  */

/* Build and return the string type.  */

pk_val pk_make_string_type (void);

/* The `any' type.  */

/* Build and return the `any' type.  */

pk_val pk_make_any_type (void);

/* Offset types.  */

/* Build and return an offset type.

   BASE_TYPE is an integral type denoting the type of the base value
   of the offset.

   UNIT is an uint<64> with the unit of the offset type.  The unit is
   a multiple of the base unit, which is the bit.  */

pk_val pk_make_offset_type (pk_val base_type, pk_val unit);

/* Get the base type of a given offset type.  */

pk_val pk_offset_type_base_type (pk_val type);

/* Get the unit of a given offset type.  */

pk_val pk_offset_type_unit (pk_val type);

/* Struct types. */

/* Build and return a struct type.

   NFIELDS is the number of struct fields on this struct.

   NAME is a string containing the name of a struct type.

   FNAMES is a C array containing the name of each struct field.

   FTYPES is a C array containing the types of each struct field. */

pk_val pk_make_struct_type (pk_val nfields,
pk_val name, pk_val *fnames,
            pk_val *ftypes);

/* Get the type of a struct.  */

pk_val pk_struct_type (pk_val sct);

/* Allocate space for struct fields names and field types. */

void pk_allocate_struct_attrs (pk_val nfields,
pk_val **fnames, pk_val **ftypes);

/* Get the name of a struct type.

   If the struct type is anonymous, PK_NULL is returned.  */

pk_val pk_struct_type_name (pk_val type);

/* Get the number of fields of a struct type.

   The returned value is an uint<64> */

pk_val pk_struct_type_nfields (pk_val type);

/* Get the name of a field in a struct type.

   TYPE is a struct type.

   IDX is the index of the field in a struct type.

   If IDX is invalid, PK_NULL is returned.

   If the struct field is anonymous, PK_NULL is returned.  */

pk_val pk_struct_type_fname (pk_val type, uint64_t idx);

/* Set the name of a field of a struct type.

   TYPE is a struct type.

   IDX is the index of the field in a struct type.

   NAME is a string containing the name of the field in a struct type.

   If IDX is invalid, type remains unchanged.  */

void pk_struct_type_set_fname (pk_val type, uint64_t idx, pk_val field_name);

/* Get type of a field in the struct.

   TYPE is a struct type.

   IDX is the index of the struct field.

   If IDX is invalid, PK_NULL is returned.  */

pk_val pk_struct_type_ftype (pk_val type, uint64_t idx);

/* Set the type of a field of a struct type.

   TYPE is a struct type.

   IDX is the index of the field in a struct type.

   TYPE is the type of the field in a struct type.

   If IDX is invalid, type remains unchanged.  */

void pk_struct_type_set_ftype
(pk_val type, uint64_t idx, pk_val field_type);

/* Array types.  */

/* Build and return an array type.

   ETYPE is the type of the elements of the array.

   BOUND is an uint<64> with the number of elements of the
   array... XXX or a closure?? cant remember. At the moment the PKL
   compiler generates PVM_NULL for this... */

pk_val pk_make_array_type (pk_val etype, pk_val bound);

/* Get the type of the elements of the given array type.  */

pk_val pk_array_type_etype (pk_val type);

/* Get the bound of the given array type.  */

pk_val pk_array_type_bound (pk_val type);

/* Mapped values.  */

/* Return a boolean indicating whether the given value is mapped or
   not.  */

int pk_val_mapped_p (pk_val val);

/* Return the IOS identifier, an int<32>, in which the given value is
   mapped.  If the value is not mapped, return PK_NULL.  */

pk_val pk_val_ios (pk_val val);

/* Return the offset in which the given value is mapped.
   If the value is not mapped, return PK_NULL.  */

pk_val pk_val_offset (pk_val val);

/* Other operations on values.  */

/* Return the type of the given value.  */

pk_val pk_typeof (pk_val val);

/* Given a type value, return its code.  */

#define PK_UNKNOWN 0
#define PK_INT     1
#define PK_UINT    2
#define PK_STRING  3
#define PK_OFFSET  4
#define PK_ARRAY   5
#define PK_STRUCT  6
#define PK_CLOSURE 7
#define PK_ANY     8

int pk_type_code (pk_val val);

/* Compare two Poke values.

   Returns 1 if they match, 0 otherwise.  */

int pk_val_equal_p (pk_val val1, pk_val val2);

/* Print the given value.   */

void pk_print_val (pk_compiler pkc, pk_val val);

#endif /* ! LIBPOKE_H */
