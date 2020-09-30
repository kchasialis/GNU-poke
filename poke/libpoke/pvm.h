/* pvm.h - Poke Virtual Machine.  */

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

/* This is the public interface of the Poke Virtual Machine (PVM)
   services as provided by libpoke.  */

#ifndef PVM_H
#define PVM_H

#include <config.h>

#include "ios.h"

/* **************** Status Codes **************** */

/* The following status codes are used in the several APIs defined
   below in the file.  */

#define PVM_OK      0 /* The operation was performed to completion, in
                         the expected way.  */
#define PVM_ERROR  -1 /* An unspecified error condition happened.  */
#define PVM_EINVAL -3 /* Invalid argument.  */
#define PVM_EINSN  -4 /* Malformed instruction.  */

/* **************** PVM Values **************** */

/* The pvm_val type implements values that are native to the poke
   virtual machine.

   It is fundamental for pvm_val values to fit in 64 bits, in order to
   avoid expensive allocations and to also improve the performance of
   the virtual machine.  */

typedef uint64_t pvm_val;

/* PVM_NULL is an invalid pvm_val.  */

#define PVM_NULL 0x7ULL

/* **************** PVM Programs ****************  */

/* PVM programs are sequences of instructions, labels and directives,
   that can be run in the virtual machine.  */

typedef struct pvm_program *pvm_program;

/* Each PVM program can contain zero or more labels.  Labels are used
   as targets of branch instructions.  */

typedef int pvm_program_label;

/* The PVM features a set of registers.
   XXX  */

typedef unsigned int pvm_register;

/* Create a new PVM program.

   The created program is returned.  If there is a problem creating
   the program then this function returns NULL.  */

pvm_program pvm_program_new (void)
  __attribute__ ((visibility ("hidden")));

/* Destroy the given PVM program.  */

void pvm_destroy_program (pvm_program program)
  __attribute__ ((visibility ("hidden")));

/* Make the given PVM program executable so it can be run in the PVM.

   This function returns a status code indicating whether the
   operation was successful or not.  */

int pvm_program_make_executable (pvm_program program)
  __attribute__ ((visibility ("hidden")));

/* Print a native disassembly of the given program in the standard
   output.  */

void pvm_disassemble_program_nat (pvm_program program)
  __attribute__ ((visibility ("hidden")));

/* Print a disassembly of the given program in the standard
   output.  */

void pvm_disassemble_program (pvm_program program)
  __attribute__ ((visibility ("hidden")));

/* **************** Assembling PVM Programs ****************  */

/* Assembling a PVM program involves starting with an empty program
   and then appending its components, like labels and instructions.

   For each instruction, you need to append its parameters before
   appending the instruction itself.  For example, in order to build
   an instruction `foo 1, 2', you would need to:

     append parameter 1
     append parameter 2
     append instruction foo

   All the append functions below return a status code.  */

/* Create a fresh label for the given program and return it.  This
   label should be eventually appended to the program.  */

pvm_program_label pvm_program_fresh_label (pvm_program program)
  __attribute__ ((visibility ("hidden")));

/* Append a PVM value instruction parameter to a PVM program.

   PROGRAM is the program in which to append the parameter.
   VAL is the PVM value to use as the instruction parameter.  */

int pvm_program_append_val_parameter (pvm_program program,
                                      pvm_val val)
  __attribute__ ((visibility ("hidden")));

/* Append an unsigned integer literal instruction parameter to a PVM
   program.

   PROGRAM is the program in which to append the parameter.
   N is the literal to use as the instruction parameter.  */

int pvm_program_append_unsigned_parameter (pvm_program program,
                                           unsigned int n)
  __attribute__ ((visibility ("hidden")));

/* Append a PVM register instruction parameter to a PVM program.

   PROGRAM is the program in which to append the parameter.
   REG is the PVM register to use as the instruction parameter.

   If REG is not a valid register this function returns
   PVM_EINVAL.  */

int pvm_program_append_register_parameter (pvm_program program,
                                           pvm_register reg)
  __attribute__ ((visibility ("hidden")));

/* Appenda PVM label instruction parameter to a PVM program.

   PROGRAM is the program in which to append the parameter.
   LABEL is the PVM label to use as the instruction parameter.

   If LABEL is not a label in PROGRAM, this function returns
   PVM_EINVAL.  */

int pvm_program_append_label_parameter (pvm_program program,
                                        pvm_program_label label)
  __attribute__ ((visibility ("hidden")));

/* Append an instruction to a PVM program.

   PROGRAM is the program in which to append the instruction.
   INSN_NAME is the name of the instruction to append.

   If there is no instruction named INSN_NAME, this function returns
   PVM_EINVAL.

   If not all the parameters required by the instruction have been
   already appended, this function returns PVM_EINSN.  */

int pvm_program_append_instruction (pvm_program program,
                                    const char *insn_name)
  __attribute__ ((visibility ("hidden")));

/* Append a `push' instruction to a PVM program.

   Due to a limitation of Jitter, we can't build push instructions the
   usual way.  This function should be used instead.

   PROGRAM is the program in which to append the instruction.
   VAL is the PVM value that will be pushed by the instruction.  */

int pvm_program_append_push_instruction (pvm_program program,
                                         pvm_val val)
  __attribute__ ((visibility ("hidden")));

/* Append a PVM label to a PVM program.

   PROGRAM is the program in which to append the label.
   LABEL is the PVM label to append.

   If LABEL doesn't exist in PROGRAM this function return
   PVM_EINVAL.  */

int pvm_program_append_label (pvm_program program,
                              pvm_program_label label)
  __attribute__ ((visibility ("hidden")));

/* **************** Building PVM Values **************** */

/* Make signed and unsigned integer PVM values.
   SIZE is measured in bits and should be in the range 1 to 32.  */

pvm_val pvm_make_int (int32_t value, int size)
  __attribute__ ((visibility ("hidden")));

pvm_val pvm_make_uint (uint32_t value, int size)
  __attribute__ ((visibility ("hidden")));

/* Make signed and unsigned long PVM values.
   SIZE is measured in bits and should be in the range 1 to 64.  */

pvm_val pvm_make_long (int64_t value, int size)
  __attribute__ ((visibility ("hidden")));

pvm_val pvm_make_ulong (uint64_t value, int size)
  __attribute__ ((visibility ("hidden")));

/* Make a string PVM value.  */

pvm_val pvm_make_string (const char *value)
  __attribute__ ((visibility ("hidden")));

/* Make an offset PVM value.

   MAGNITUDE is a PVM integral value.

   UNIT is an ulong<64> PVM value specifying the unit of the offset,
   in terms of the basic unit which is the bit.  */

pvm_val pvm_make_offset (pvm_val magnitude, pvm_val unit)
  __attribute__ ((visibility ("hidden")));

/* Make an array PVM value.

   NELEM is an ulong<64> PVM value specifying the number of elements
   in the array.

   TYPE is a type PVM value specifying the type of the array.

   The elements in the created array are initialized to PVM_NULL.  */

pvm_val pvm_make_array (pvm_val nelem, pvm_val type)
  __attribute__ ((visibility ("hidden")));

/* Make a struct PVM value.

   NFIELDS is an ulong<64> PVM value specifying the number of fields
   in the struct.  This can be ulong<64>0 for an empty struct.

   NMETHODS is an ulong<64> PVM vlaue specifying the number of methods
   in the struct.

   TYPE is a type PVM value specifying the type of the struct.

   The fields and methods in the created struct are initialized to
   PVM_NULL.  */

pvm_val pvm_make_struct (pvm_val nfields, pvm_val nmethods, pvm_val type)
  __attribute__ ((visibility ("hidden")));

/* Make a closure PVM value.
   PROGRAM is a PVM program that conforms the body of the closure.  */

pvm_val pvm_make_cls (pvm_program program)
  __attribute__ ((visibility ("hidden")));

/* Compare two PVM values.

   Returns 1 if they match, 0 otherwise.  */

int pvm_val_equal_p (pvm_val val1, pvm_val val2)
  __attribute__ ((visibility ("hidden")));

/*** PVM values.  ***/

void pvm_print_string (pvm_val string)
  __attribute__ ((visibility ("hidden")));

pvm_val pvm_ref_struct (pvm_val sct, pvm_val name)
  __attribute__ ((visibility ("hidden")));

int pvm_set_struct (pvm_val sct, pvm_val name, pvm_val val)
  __attribute__ ((visibility ("hidden")));

pvm_val pvm_get_struct_method (pvm_val sct, const char *name)
  __attribute__ ((visibility ("hidden")));

pvm_val pvm_make_integral_type (pvm_val size, pvm_val signed_p)
  __attribute__ ((visibility ("hidden")));

pvm_val pvm_make_string_type (void)
  __attribute__ ((visibility ("hidden")));

pvm_val pvm_make_any_type (void)
  __attribute__ ((visibility ("hidden")));

pvm_val pvm_make_array_type (pvm_val type, pvm_val bound)
  __attribute__ ((visibility ("hidden")));

pvm_val pvm_make_struct_type (pvm_val nfields, pvm_val name,
                              pvm_val *fnames, pvm_val *ftypes)
  __attribute__ ((visibility ("hidden")));

pvm_val pvm_make_offset_type (pvm_val base_type, pvm_val unit)
  __attribute__ ((visibility ("hidden")));
pvm_val pvm_make_closure_type (pvm_val rtype, pvm_val nargs,
                               pvm_val *atypes)
  __attribute__ ((visibility ("hidden")));

pvm_val pvm_dup_type (pvm_val type)
  __attribute__ ((visibility ("hidden")));

pvm_val pvm_typeof (pvm_val val)
  __attribute__ ((visibility ("hidden")));

int pvm_type_equal (pvm_val type1, pvm_val type2)
  __attribute__ ((visibility ("hidden")));

pvm_program pvm_val_cls_program (pvm_val cls)
  __attribute__ ((visibility ("hidden")));

/* Return the size of VAL, in bits.  */

uint64_t pvm_sizeof (pvm_val val)
  __attribute__ ((visibility ("hidden")));

/* For strings, arrays and structs, return the number of
   elements/fields stored, as an unsigned 64-bits long.  Return 1
   otherwise.  */

pvm_val pvm_elemsof (pvm_val val)
  __attribute__ ((visibility ("hidden")));

/* Return the mapper function for the given value, and the writer
   function.  If the value is not mapped, return PVM_NULL.  */

pvm_val pvm_val_mapper (pvm_val val)
  __attribute__ ((visibility ("hidden")));

pvm_val pvm_val_writer (pvm_val val)
  __attribute__ ((visibility ("hidden")));

/* Return a PVM value for an exception with the given CODE, MESSAGE
   and EXIT_STATUS.  */

pvm_val pvm_make_exception (int code, char *message, int exit_status)
  __attribute__ ((visibility ("hidden")));


/* **************** The Run-Time Environment ****************  */

/* The poke virtual machine (PVM) maintains a data structure called
   the run-time environment.  This structure contains run-time frames,
   which in turn store the variables of PVM routines.

   A set of PVM instructions are provided to allow programs to
   manipulate the run-time environment.  These are implemented in
   pvm.jitter in the "Environment instructions" section, and
   summarized here:

   `pushf' pushes a new frame to the run-time environment.  This is
   used when entering a new environment, such as a function.

   `popf' pops a frame from the run-time environment.  After this
   happens, if no references are left to the popped frame, both the
   frame and the variables stored in the frame are eventually
   garbage-collected.

   `popvar' pops the value at the top of the main stack and creates a
   new variable in the run-time environment to hold that value.

   `pushvar BACK, OVER' retrieves the value of a variable from the
   run-time environment and pushes it in the main stack.  BACK is the
   number of frames to traverse and OVER is the order of the variable
   in its containing frame.  The BACK,OVER pairs (also known as
   lexical addresses) are produced by the compiler; see `pkl-env.h'
   for a description of the compile-time environment.

   This header file provides the prototypes for the functions used to
   implement the above-mentioned PVM instructions.  */

typedef struct pvm_env *pvm_env;  /* Struct defined in pvm-env.c */

/* Create a new run-time environment, containing an empty top-level
   frame, and return it.

   HINT specifies the expected number of variables that will be
   registered in this environment.  If HINT is 0 it indicates that we
   can't provide an estimation.  */

pvm_env pvm_env_new (int hint)
  __attribute__ ((visibility ("hidden")));

/* Push a new empty frame to ENV and return the modified run-time
   environment.

   HINT provides a hint on the number of entries that will be stored
   in the frame.  If HINT is 0, it indicates the number can't be
   estimated at all.  */

pvm_env pvm_env_push_frame (pvm_env env, int hint)
  __attribute__ ((visibility ("hidden")));

/* Pop a frame from ENV and return the modified run-time environment.
   The popped frame will eventually be garbage-collected if there are
   no more references to it.  Trying to pop the top-level frame is an
   error.  */

pvm_env pvm_env_pop_frame (pvm_env env)
  __attribute__ ((visibility ("hidden")));

/* Create a new variable in the current frame of ENV, whose value is
   VAL.  */

void pvm_env_register (pvm_env env, pvm_val val)
  __attribute__ ((visibility ("hidden")));

/* Return the value for the variable occupying the position BACK, OVER
   in the run-time environment ENV.  Return PVM_NULL if the variable
   is not found.  */

pvm_val pvm_env_lookup (pvm_env env, int back, int over)
  __attribute__ ((visibility ("hidden")));

/* Set the value of the variable occupying the position BACK, OVER in
   the run-time environment ENV to VAL.  */

void pvm_env_set_var (pvm_env env, int back, int over, pvm_val val)
  __attribute__ ((visibility ("hidden")));

/* Return 1 if the given run-time environment ENV contains only one
   frame.  Return 0 otherwise.  */

int pvm_env_toplevel_p (pvm_env env)
  __attribute__ ((visibility ("hidden")));

/*** Other Definitions.  ***/

enum pvm_omode
  {
    PVM_PRINT_FLAT,
    PVM_PRINT_TREE,
  };

/* The following enumeration contains every possible exit code
   resulting from the execution of a routine in the PVM.

   PVM_EXIT_OK is returned if the routine was executed successfully,
   and every raised exception was properly handled.

   PVM_EXIT_ERROR is returned in case of an unhandled exception.  */

enum pvm_exit_code
  {
    PVM_EXIT_OK = 0,
    PVM_EXIT_ERROR = 1
  };

/* Exceptions.  These should be in sync with the exception code
   variables, and the exception messages, declared in pkl-rt.pkl */

#define PVM_E_GENERIC       0
#define PVM_E_GENERIC_MSG "generic"
#define PVM_E_GENERIC_ESTATUS 1

#define PVM_E_DIV_BY_ZERO   1
#define PVM_E_DIV_BY_ZERO_MSG "division by zero"
#define PVM_E_DIV_BY_ZERO_ESTATUS 1

#define PVM_E_NO_IOS        2
#define PVM_E_NO_IOS_MSG "no IOS"
#define PVM_E_NO_IOS_ESTATUS 1

#define PVM_E_NO_RETURN     3
#define PVM_E_NO_RETURN_MSG "no return"
#define PVM_E_NO_RETURN_ESTATUS 1

#define PVM_E_OUT_OF_BOUNDS 4
#define PVM_E_OUT_OF_BOUNDS_MSG "out of bounds"
#define PVM_E_OUT_OF_BOUNDS_ESTATUS 1

#define PVM_E_MAP_BOUNDS    5
#define PVM_E_MAP_BOUNDS_MSG "out of map bounds"
#define PVM_E_MAP_BOUNDS_ESTATUS 1

#define PVM_E_EOF           6
#define PVM_E_EOF_MSG "EOF"
#define PVM_E_EOF_ESTATUS 1

#define PVM_E_MAP           7
#define PVM_E_MAP_MSG "no map"
#define PVM_E_MAP_ESTATUS 1

#define PVM_E_CONV          8
#define PVM_E_CONV_MSG "conversion error"
#define PVM_E_CONV_ESTATUS 1

#define PVM_E_ELEM          9
#define PVM_E_ELEM_MSG "invalid element"
#define PVM_E_ELEM_ESTATUS 1

#define PVM_E_CONSTRAINT   10
#define PVM_E_CONSTRAINT_MSG "constraint violation"
#define PVM_E_CONSTRAINT_ESTATUS 1

#define PVM_E_IO           11
#define PVM_E_IO_MSG "generic IO"
#define PVM_E_IO_ESTATUS 1

#define PVM_E_SIGNAL       12
#define PVM_E_SIGNAL_MSG ""
#define PVM_E_SIGNAL_ESTATUS 1

#define PVM_E_IOFLAGS      13
#define PVM_E_IOFLAGS_MSG "invalid IO flags"
#define PVM_E_IOFLAGS_ESTATUS 1

#define PVM_E_INVAL        14
#define PVM_E_INVAL_MSG "invalid argument"
#define PVM_E_INVAL_ESTATUS 1

#define PVM_E_EXIT         15
#define PVM_E_EXIT_MSG ""
#define PVM_E_EXIT_ESTATUS 0

typedef struct pvm *pvm;

/* Initialize a new Poke Virtual Machine and return it.  */

pvm pvm_init (void)
  __attribute__ ((visibility ("hidden")));

/* Finalize a Poke Virtual Machine, freeing all used resources.  */

void pvm_shutdown (pvm pvm)
  __attribute__ ((visibility ("hidden")));

/* Get the current run-time environment of PVM.  */

pvm_env pvm_get_env (pvm pvm)
  __attribute__ ((visibility ("hidden")));

/* Run a PVM program in a virtual machine.

   If the execution of PROGRAM generates a result value, it is put in
   RES.

   This function returns an exit code, indicating whether the
   execution was successful or not.  */

enum pvm_exit_code pvm_run (pvm vm,
                            pvm_program program,
                            pvm_val *res)
  __attribute__ ((visibility ("hidden")));

/* Get/set the current byte endianness of a virtual machine.

   The current endianness is used by certain VM instructions that
   perform IO.

   ENDIAN should be one of IOS_ENDIAN_LSB (for little-endian) or
   IOS_ENDIAN_MSB (for big-endian).  */

enum ios_endian pvm_endian (pvm pvm)
  __attribute__ ((visibility ("hidden")));

void pvm_set_endian (pvm pvm, enum ios_endian endian)
  __attribute__ ((visibility ("hidden")));

/* Get/set the current negative encoding of a virtual machine.

   The current negative encoding is used by certain VM instructions
   that perform IO.

   NENC should be one of the IOS_NENC_* values defined in ios.h */

enum ios_nenc pvm_nenc (pvm pvm)
  __attribute__ ((visibility ("hidden")));

void pvm_set_nenc (pvm pvm, enum ios_nenc nenc)
  __attribute__ ((visibility ("hidden")));

/* Get/set the pretty-print flag in a virtual machine.

   PRETTY_PRINT_P is a boolean indicating whether to use pretty print
   methods when printing struct PVM values.  This requires the
   presence of a compiler associated with the VM.  */

int pvm_pretty_print (pvm pvm)
  __attribute__ ((visibility ("hidden")));

void pvm_set_pretty_print (pvm pvm, int pretty_print_p)
  __attribute__ ((visibility ("hidden")));

/* Get/set the output parameters configured in a virtual machine.

   OBASE is the numeration based to be used when printing PVM values.
   Valid values are 2, 8, 10 and 16.

   OMAPS is a boolean indicating whether to show mapping information
   when printing PVM values.

   OINDENT is a number indicating the indentation step used when
   printing composite PVM values, measured in number of whitespace
   characters.

   ODEPTH is the maximum depth to use when printing composite PVM
   values.  A value of 0 indicates to not use a maximum depth, i.e. to
   print the whole structure.

   OACUTOFF is the maximum number of elements to show when printing
   array PVM values.  A value of 0 indicates to print all the elements
   of arrays.  */

int pvm_obase (pvm vm)
  __attribute__ ((visibility ("hidden")));

void pvm_set_obase (pvm vm, int obase)
  __attribute__ ((visibility ("hidden")));

enum pvm_omode pvm_omode (pvm vm)
  __attribute__ ((visibility ("hidden")));

void pvm_set_omode (pvm vm, enum pvm_omode omode)
  __attribute__ ((visibility ("hidden")));

int pvm_omaps (pvm vm)
  __attribute__ ((visibility ("hidden")));

void pvm_set_omaps (pvm vm, int omaps)
  __attribute__ ((visibility ("hidden")));

unsigned int pvm_oindent (pvm vm)
  __attribute__ ((visibility ("hidden")));

void pvm_set_oindent (pvm vm, unsigned int oindent)
  __attribute__ ((visibility ("hidden")));

unsigned int pvm_odepth (pvm vm)
  __attribute__ ((visibility ("hidden")));

void pvm_set_odepth (pvm vm, unsigned int odepth)
  __attribute__ ((visibility ("hidden")));

unsigned int pvm_oacutoff (pvm vm)
  __attribute__ ((visibility ("hidden")));
void pvm_set_oacutoff (pvm vm, unsigned int cutoff)
  __attribute__ ((visibility ("hidden")));

/* Get/set the compiler associated to a virtual machine.

   This compiler is used when the VM needs to build programs and
   execute them.

   It there is no compiler associated with VM, pvm_compiler returns
   NULL.  */

typedef struct pkl_compiler *pkl_compiler;

pkl_compiler pvm_compiler (pvm vm)
  __attribute__ ((visibility ("hidden")));

void pvm_set_compiler (pvm vm, pkl_compiler compiler)
  __attribute__ ((visibility ("hidden")));

/* The following function is to be used in pvm.jitter, because the
   system `assert' may expand to a macro and is therefore
   non-wrappeable.  */

void pvm_assert (int expression)
  __attribute__ ((visibility ("hidden")));

/* This is defined in the late-c block in pvm.jitter.  */

void pvm_handle_signal (int signal_number)
  __attribute__ ((visibility ("hidden")));

/* Call the pretty printer of the given value VAL.  */

int pvm_call_pretty_printer (pvm vm, pvm_val val)
  __attribute__ ((visibility ("hidden")));

/* Print a PVM value.

   DEPTH is a number that specifies the maximum depth used when
   printing composite values, i.e. arrays and structs.  If it is 0
   then it means there is no maximum depth.

   MODE is one of the PVM_PRINT_* values defined in pvm_omode, and
   specifies the output mode to use when printing the value.

   BASE is the numeration base to use when printing numbers.  It may
   be one of 2, 8, 10 or 16.

   INDENT is the step value to use when indenting nested structured
   when printin in tree mode.

   ACUTOFF is the maximum number of elements of arrays to print.
   Elements beyond are not printed.

   FLAGS is a 32-bit unsigned integer, that encodes certain properties
   to be used while printing:

   If PVM_PRINT_F_MAPS is specified then the attributes of mapped
   values (notably their offsets) are also printed out.  When
   PVM_PRINT_F_MAPS is not specified, mapped values are printed
   exactly the same way than non-mapped values.

   If PVM_PRINT_F_PPRINT is specified then pretty-printers are used to
   print struct values, if they are defined.  */

#define PVM_PRINT_F_MAPS   1
#define PVM_PRINT_F_PPRINT 2

void pvm_print_val (pvm vm, pvm_val val)
  __attribute__ ((visibility ("hidden")));

void pvm_print_val_with_params (pvm vm, pvm_val val,
                                int depth,int mode, int base,
                                int indent, int acutoff,
                                uint32_t flags)
  __attribute__ ((visibility ("hidden")));

#endif /* ! PVM_H */
