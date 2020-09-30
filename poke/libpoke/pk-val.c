/* pk-val.c - poke values.  */

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

#include "pvm.h"
#include "pvm-val.h"
#include "libpoke.h"

pk_val
pk_make_int (int64_t value, int size)
{
  pk_val new;

  /* At the moment poke integers are limited to a maximum number of
     bits.  */
  if (size > 64)
    return PK_NULL;

  if (size <= 32)
    new = pvm_make_int (value, size);
  else
    new = pvm_make_long (value, size);

  return new;
}

int64_t
pk_int_value (pk_val val)
{
  if (PVM_IS_INT (val))
    return PVM_VAL_INT (val);
  else
    return PVM_VAL_LONG (val);
}

int
pk_int_size (pk_val val)
{
  if (PVM_IS_INT (val))
    return PVM_VAL_INT_SIZE (val);
  else
    return PVM_VAL_LONG_SIZE (val);
}

pk_val
pk_make_uint (uint64_t value, int size)
{
  pk_val new;

  /* At the moment poke integers are limited to a maximum number of
     bits.  */
  if (size > 64)
    return PK_NULL;

  if (size <= 32)
    new = pvm_make_uint (value, size);
  else
    new = pvm_make_ulong (value, size);

  return new;
}

uint64_t
pk_uint_value (pk_val val)
{
  if (PVM_IS_UINT (val))
    return PVM_VAL_UINT (val);
  else
    return PVM_VAL_ULONG (val);
}

int
pk_uint_size (pk_val val)
{
  if (PVM_IS_UINT (val))
    return PVM_VAL_UINT_SIZE (val);
  else
    return PVM_VAL_ULONG_SIZE (val);
}

pk_val
pk_make_string (const char *str)
{
  return pvm_make_string (str);
}

const char *
pk_string_str (pk_val val)
{
  return PVM_VAL_STR (val);
}

pk_val
pk_make_offset (pk_val magnitude, pk_val unit)
{
  if (!PVM_IS_INTEGRAL (magnitude)
      || !PVM_IS_ULONG (unit)
      || PVM_VAL_ULONG_SIZE (unit) != 64)
    return PK_NULL;
  else
    return pvm_make_offset (magnitude, unit);
}

pk_val
pk_offset_magnitude (pk_val val)
{
  return PVM_VAL_OFF_MAGNITUDE (val);
}

pk_val
pk_offset_unit (pk_val val)
{
  return PVM_VAL_OFF_UNIT (val);
}

int
pk_val_mapped_p (pk_val val)
{
  return PVM_VAL_MAPPER (val) != PVM_NULL;
}

pk_val
pk_val_ios (pk_val val)
{
  return PVM_VAL_IOS (val);
}

pk_val
pk_val_offset (pk_val val)
{
  /* The offset in the PVM value is a bit-offset.  Convert to a proper
     offset.  */
  uint64_t bit_offset = PVM_VAL_ULONG (PVM_VAL_OFFSET (val));

  /* XXX "upunit" properly so we get a nice unit, not just bytes or
     bits.  */
  if (bit_offset % 8 == 0)
    return pvm_make_offset (pvm_make_ulong (bit_offset / 8, 64),
                            pvm_make_ulong (8, 32));
  else
    return pvm_make_offset (PVM_VAL_OFFSET (val),
                            pvm_make_ulong (1, 32));
}

int
pk_type_code (pk_val val)
{
  switch (PVM_VAL_TYP_CODE (val))
    {
    case PVM_TYPE_INTEGRAL:
      if ((pk_int_value (pk_integral_type_signed_p (val))))
        return PK_INT;
      else
        return PK_UINT;
    case PVM_TYPE_STRING:
      return PK_STRING;
    case PVM_TYPE_ARRAY:
      return PK_ARRAY;
    case PVM_TYPE_STRUCT:
      return PK_STRUCT;
    case PVM_TYPE_OFFSET:
      return PK_OFFSET;
    case PVM_TYPE_CLOSURE:
      return PK_CLOSURE;
    case PVM_TYPE_ANY:
      return PK_ANY;
    default:
      return PK_UNKNOWN;
    }
}

int
pk_val_equal_p (pk_val val1, pk_val val2)
{
  return pvm_val_equal_p (val1, val2);
}

pk_val
pk_make_struct (pk_val nfields, pk_val type)
{
  return pvm_make_struct (nfields, pvm_make_ulong (0, 64), type);
}

pk_val
pk_struct_nfields (pk_val sct)
{
  return PVM_VAL_SCT_NFIELDS (sct);
}

pk_val pk_struct_field_boffset (pk_val sct, uint64_t idx)
{
  if (idx < pk_uint_value (pk_struct_nfields (sct)))
    return PVM_VAL_SCT_FIELD_OFFSET (sct, idx);
  else
    return PK_NULL;
}

void pk_struct_set_field_boffset (pk_val sct, uint64_t idx, pk_val boffset)
{
  if (idx < pk_uint_value (pk_struct_nfields (sct)))
    PVM_VAL_SCT_FIELD_OFFSET (sct, idx) = boffset;
}

pk_val pk_struct_field_name (pk_val sct, uint64_t idx)
{
  if (idx < pk_uint_value (pk_struct_nfields (sct)))
    return PVM_VAL_SCT_FIELD_NAME (sct, idx);
  else
    return PK_NULL;
}

void pk_struct_set_field_name (pk_val sct, uint64_t idx, pk_val name)
{
  if (idx < pk_uint_value (pk_struct_nfields (sct)))
    PVM_VAL_SCT_FIELD_NAME (sct, idx) = name;
}

pk_val pk_struct_field_value (pk_val sct, uint64_t idx)
{
  if (idx < pk_uint_value (pk_struct_nfields (sct)))
    return PVM_VAL_SCT_FIELD_VALUE (sct, idx);
  else
    return PK_NULL;
}

void pk_struct_set_field_value (pk_val sct, uint64_t idx, pk_val value)
{
  if (idx < pk_uint_value (pk_struct_nfields (sct)))
    PVM_VAL_SCT_FIELD_VALUE (sct, idx) = value;
}

pk_val
pk_make_array (pk_val nelem, pk_val array_type)
{
  return pvm_make_array (nelem, array_type);
}

pk_val
pk_make_integral_type (pk_val size, pk_val signed_p)
{
  return pvm_make_integral_type (size, signed_p);
}

pk_val
pk_integral_type_size (pk_val type)
{
  return PVM_VAL_TYP_I_SIZE (type);
}

pk_val
pk_integral_type_signed_p (pk_val type)
{
  return PVM_VAL_TYP_I_SIGNED_P (type);
}

pk_val
pk_make_string_type (void)
{
  return pvm_make_string_type ();
}

pk_val
pk_make_offset_type (pk_val base_type, pk_val unit)
{
  return pvm_make_offset_type (base_type, unit);
}

pk_val
pk_offset_type_base_type (pk_val type)
{
  return PVM_VAL_TYP_O_BASE_TYPE (type);
}

pk_val
pk_offset_type_unit (pk_val type)
{
  return PVM_VAL_TYP_O_UNIT (type);
}

pk_val
pk_make_any_type (void)
{
  return pvm_make_any_type ();
}

pk_val
pk_make_struct_type (pk_val nfields, pk_val name, pk_val *fnames, pk_val *ftypes)
{
  return pvm_make_struct_type (nfields, name, fnames, ftypes);
}

pk_val
pk_struct_type (pk_val sct)
{
  return PVM_VAL_SCT_TYPE (sct);
}

void
pk_allocate_struct_attrs (pk_val nfields, pk_val **fnames, pk_val **ftypes)
{
  pvm_allocate_struct_attrs (nfields, fnames, ftypes);
}

pk_val
pk_struct_type_name (pk_val type)
{
  return PVM_VAL_TYP_S_NAME (type);
}

pk_val
pk_struct_type_nfields (pk_val type)
{
  return PVM_VAL_TYP_S_NFIELDS (type);
}

pk_val
pk_struct_type_fname (pk_val type, uint64_t idx)
{
  if (idx < pk_uint_value (pk_struct_type_nfields (type)))
    return PVM_VAL_TYP_S_FNAME (type, idx);
  else
    return PK_NULL;
}

void
pk_struct_type_set_fname (pk_val type, uint64_t idx, pk_val field_name)
{
  if (idx < pk_uint_value (pk_struct_type_nfields (type)))
    PVM_VAL_TYP_S_FNAME (type, idx) = field_name;
}

pk_val
pk_struct_type_ftype (pk_val type, uint64_t idx)
{
  if (idx < pk_uint_value (pk_struct_type_nfields (type)))
    return PVM_VAL_TYP_S_FTYPE (type, idx);
  else
    return PK_NULL;
}

void
pk_struct_type_set_ftype (pk_val type, uint64_t idx, pk_val field_type)
{
  if (idx < pk_uint_value (pk_struct_type_nfields (type)))
    PVM_VAL_TYP_S_FTYPE (type, idx) = field_type;
}

pk_val
pk_make_array_type (pk_val etype, pk_val bound)
{
  return pvm_make_array_type (etype, bound);
}

pk_val
pk_array_type_etype (pk_val type)
{
  return PVM_VAL_TYP_A_ETYPE (type);
}

pk_val
pk_array_type_bound (pk_val type)
{
  return PVM_VAL_TYP_A_BOUND (type);
}

pk_val
pk_typeof (pk_val val)
{
  return pvm_typeof (val);
}

pk_val
pk_array_nelem (pk_val array)
{
  return PVM_VAL_ARR_NELEM (array);
}

pk_val
pk_array_elem_val (pk_val array, uint64_t idx)
{
  if (idx < pk_uint_value (pk_array_nelem (array)))
    return PVM_VAL_ARR_ELEM_VALUE (array, idx);
  else
    return PK_NULL;
}

void
pk_array_set_elem_val (pk_val array, uint64_t idx, pk_val val)
{
  if (idx < pk_uint_value (pk_array_nelem (array)))
    PVM_VAL_ARR_ELEM_VALUE (array, idx) = val;
}

pk_val
pk_array_elem_boffset (pk_val array, uint64_t idx)
{
  if (idx < pk_uint_value (pk_array_nelem (array)))
    return PVM_VAL_ARR_ELEM_OFFSET (array, idx);
  else
    return PK_NULL;
}

void
pk_array_set_elem_boffset (pk_val array, uint64_t idx, pk_val boffset)
{
  if (idx < pk_uint_value (pk_array_nelem (array)))
    PVM_VAL_ARR_ELEM_OFFSET (array, idx) = boffset;
}
