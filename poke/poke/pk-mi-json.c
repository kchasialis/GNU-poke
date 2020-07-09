/* pk-mi-json.c - Machine Interface JSON support */

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

#include <assert.h>
#include <config.h>
#include <string.h>
#include <json.h>

#include "pk-term.h"
#include "pk-mi.h"
#include "pk-mi-json.h"
#include "pk-mi-msg.h"

/* Message::
   {
     "seq"  : integer
     "type" : MessageType
     "data" : Request | Response | Event
   }

   MessageType:: ( 0 => request | 1 => response | 2 => event )

   Request::
   {
     "type" : RequestType
     "args"? : null
   }

   RequestType:: ( 0 => REQ_EXIT )

   Response::
   {
     "type" : ResponseType
     "req_number" : uint32
     "success_p: : boolean
     "errmsg" : string
     "result"? : null
   }

   ResponseType:: ( 0 => RESP_EXIT )

   Event::
   {
     "type" : EventType
     "args" : ( EventInitializedArgs | null )
   }

   EventType:: ( 0 => EVENT_INITIALIZED )

   EventInitializedArgs::
   {
     "version" : string
   }

*/

static json_object *
pk_mi_msg_to_json_object (pk_mi_msg msg)
{
  json_object *json = json_object_new_object ();
  enum pk_mi_msg_type msg_type = pk_mi_msg_type (msg);

  if (!json)
    goto out_of_memory;

  /* Add the number.  */
  {
    json_object *number
      = json_object_new_int (pk_mi_msg_number (msg));

    if (!number)
      goto out_of_memory;
    json_object_object_add (json, "seq", number);
  }

  /* Add the type.  */
  {
    json_object *integer = json_object_new_int (msg_type);

    if (!integer)
      goto out_of_memory;
    json_object_object_add (json, "type", integer);
  }

  /* Add the data.  */
  switch (msg_type)
  {
  case PK_MI_MSG_REQUEST:
    {
      enum pk_mi_req_type msg_req_type = pk_mi_msg_req_type (msg);
      json_object *req, *req_type;

      req = json_object_new_object ();
      if (!req)
        goto out_of_memory;

      req_type = json_object_new_int (msg_req_type);
      if (!req_type)
        goto out_of_memory;
      json_object_object_add (req, "type", req_type);

      switch (msg_req_type)
        {
        case PK_MI_REQ_EXIT:
          /* Request has no args.  */
          break;
        default:
          assert (0);
        }

      json_object_object_add (json, "data", req);
      break;
    }
  case PK_MI_MSG_RESPONSE:
    {
      enum pk_mi_resp_type msg_resp_type = pk_mi_msg_resp_type (msg);
      json_object *resp, *resp_type, *success_p, *req_number;

      resp = json_object_new_object ();
      if (!resp)
        goto out_of_memory;

      resp_type = json_object_new_int (msg_resp_type);
      if (!resp_type)
        goto out_of_memory;
      json_object_object_add (resp, "type", resp_type);

      success_p
        = json_object_new_boolean (pk_mi_msg_resp_success_p (msg));
      if (!success_p)
        goto out_of_memory;
      json_object_object_add (resp, "success_p", success_p);

      req_number
        = json_object_new_int (pk_mi_msg_resp_req_number (msg));
      json_object_object_add (resp, "req_number", req_number);

      if (pk_mi_msg_resp_errmsg (msg))
        {
          json_object *errmsg
            = json_object_new_string (pk_mi_msg_resp_errmsg (msg));

          if (!errmsg)
            goto out_of_memory;
          json_object_object_add (resp, "errmsg", errmsg);
        }

      switch (msg_resp_type)
        {
        case PK_MI_RESP_EXIT:
          /* Response has no result.  */
          break;
        default:
          assert (0);
        }

      json_object_object_add (json, "data", resp);
      break;
    }
  case PK_MI_MSG_EVENT:
    {
      enum pk_mi_event_type msg_event_type = pk_mi_msg_event_type (msg);
      json_object *event, *event_type;

      event = json_object_new_object ();
      if (!event)
        goto out_of_memory;

      event_type = json_object_new_int (msg_event_type);
      if (!event_type)
        goto out_of_memory;
      json_object_object_add (event, "type", event_type);

      switch (msg_event_type)
        {
        case PK_MI_EVENT_INITIALIZED:
          {
            json_object *args, *version, *mi_version;

            args = json_object_new_object ();
            if (!args)
              goto out_of_memory;

            mi_version
              = json_object_new_int (pk_mi_msg_event_initialized_mi_version (msg));
            if (!mi_version)
              goto out_of_memory;
            json_object_object_add (args, "mi_version", mi_version);

            version
              = json_object_new_string (pk_mi_msg_event_initialized_version (msg));
            if (!version)
              goto out_of_memory;
            json_object_object_add (args, "version", version);

            json_object_object_add (event, "args", args);
            break;
          }
        default:
          assert (0);
        }

      json_object_object_add (json, "data", event);
      break;
    }
  default:
    assert (0);
  }

  return json;

 out_of_memory:
  /* XXX: destroy obj, how?  */
  return NULL;
}

static pk_mi_msg
pk_mi_json_object_to_msg (json_object *json)
{
  enum pk_mi_msg_type msg_type;
  int msg_number;
  json_object *obj;
  pk_mi_msg msg = NULL;

  if (!json_object_is_type (json, json_type_object))
    return NULL;

  /* Get the message number.  */
  {
    json_object *number;

    if (!json_object_object_get_ex (json, "seq", &number))
      return NULL;
    if (!json_object_is_type (number, json_type_int))
      return NULL;

    msg_number = json_object_get_int (number);
  }

  /* Get the message type.  */
  if (!json_object_object_get_ex (json, "type", &obj))
    return NULL;
  if (!json_object_is_type (obj, json_type_int))
    return NULL;
  msg_type = json_object_get_int (obj);

  switch (msg_type)
    {
    case PK_MI_MSG_REQUEST:
      {
        json_object *req_json, *req_type;
        enum pk_mi_req_type msg_req_type;

        /* Get the request data.  */
        if (!json_object_object_get_ex (json, "data", &req_json))
          return NULL;
        if (!json_object_is_type (req_json, json_type_object))
          return NULL;

        /* Get the request type.  */
        if (!json_object_object_get_ex (json, "type", &req_type))
          return NULL;
        if (!json_object_is_type (req_type, json_type_int))
          return NULL;
        msg_req_type = json_object_get_int (req_type);

        switch (msg_req_type)
          {
          case PK_MI_REQ_EXIT:
            msg = pk_mi_make_req_exit ();
            break;
          default:
            return NULL;
          }
        break;
      }
    case PK_MI_MSG_RESPONSE:
      {
        json_object *resp_json, *obj;
        enum pk_mi_resp_type resp_type;
        pk_mi_seqnum req_number;
        int success_p;
        const char *errmsg;

        /* Get the response data.  */
        if (!json_object_object_get_ex (json, "data", &resp_json))
          return NULL;
        if (!json_object_is_type (resp_json, json_type_object))
          return NULL;

        /* Get the response type.  */
        if (!json_object_object_get_ex (resp_json, "type", &obj))
          return NULL;
        if (!json_object_is_type (obj, json_type_int))
          return NULL;
        resp_type = json_object_get_int (obj);

        /* Get the request number.  */
        if (!json_object_object_get_ex (resp_json, "req_number", &obj))
          return NULL;
        if (!json_object_is_type (obj, json_type_int))
          return NULL;
        req_number = json_object_get_int (obj);

        /* Get success_p.  */
        if (!json_object_object_get_ex (resp_json, "succcess_p", &obj))
          return NULL;
        if (!json_object_is_type (obj, json_type_boolean))
          return NULL;
        success_p = json_object_get_boolean (obj);

        /* Get errmsg.  */
        if (success_p)
          errmsg = NULL;
        else
          {
            if (!json_object_object_get_ex (resp_json, "errmsg", &obj))
              return NULL;
            if (!json_object_is_type (obj, json_type_string))
              return NULL;
            errmsg = json_object_get_string (obj);
          }

        switch (resp_type)
          {
          case PK_MI_RESP_EXIT:
            msg = pk_mi_make_resp_exit (req_number,
                                        success_p,
                                        errmsg);
            break;
          default:
            return NULL;
          }

        break;
      }
    case PK_MI_MSG_EVENT:
      {
        json_object *event_json, *obj;
        enum pk_mi_event_type event_type;

        /* Get the event data.  */
        if (!json_object_object_get_ex (json, "data", &event_json))
          return NULL;
        if (!json_object_is_type (event_json, json_type_object))
          return NULL;

        /* The event type.  */
        if (!json_object_object_get_ex (event_json, "type", &obj))
          return NULL;
        if (!json_object_is_type (obj, json_type_int))
          return NULL;
        event_type = json_object_get_int (obj);

        switch (event_type)
          {
          case PK_MI_EVENT_INITIALIZED:
            {
              json_object *args_json, *obj;
              const char *version;

              if (!json_object_object_get_ex (event_json, "args", &args_json))
                return NULL;
              if (!json_object_is_type (args_json, json_type_object))
                return NULL;

              /* Get the version artument of EVENT_INITIALIZER  */
              if (!json_object_object_get_ex (args_json, "version", &obj))
                return NULL;
              if (!json_object_is_type (obj, json_type_string))
                return NULL;
              version = json_object_get_string (obj);

              msg = pk_mi_make_event_initialized (version);
              break;
            }
          default:
            return NULL;
          }

        break;
      }
    default:
      return NULL;
    }

  pk_mi_set_msg_number (msg, msg_number);
  return msg;
}

const char *
pk_mi_msg_to_json (pk_mi_msg msg)
{
  json_object *json;

  json = pk_mi_msg_to_json_object (msg);
  if (!json)
    return NULL;

  return json_object_to_json_string_ext (json,
                                         JSON_C_TO_STRING_PLAIN);
}

pk_mi_msg
pk_mi_json_to_msg (const char *str)
{
  pk_mi_msg msg = NULL;
  struct json_tokener *tokener;
  json_object *json;

  tokener = json_tokener_new ();
  if (tokener == NULL)
    return NULL;

  json = json_tokener_parse_ex (tokener, str, strlen (str));
  if (!json)
    pk_printf ("internal error: %s\n",
               json_tokener_error_desc (json_tokener_get_error (tokener)));
  json_tokener_free (tokener);

  if (json)
    msg = pk_mi_json_object_to_msg (json);

  /* XXX: free the json object  */
  return msg;
}

/*Functions to convert pk_val to JSON Poke Value*/
static json_object* _pk_mi_make_json_int (pk_val integer, char **errmsg);
static json_object* _pk_mi_make_json_string (pk_val str, char **errmsg);
static json_object* _pk_mi_make_json_offset (pk_val offset, char **errmsg);
static json_object* _pk_mi_make_json_struct (pk_val sct, char **errmsg);
static json_object* _pk_mi_make_json_array (pk_val array, char **errmsg);
static json_object* _pk_mi_make_json_null ();

static json_object* _pk_mi_val_to_json (pk_val val, char **errmsg)
{
  struct json_object *pk_val_object = NULL;
  if  (val == PK_NULL) {
    pk_val_object = _pk_mi_make_json_null (errmsg);
  }
  else {
    switch (pk_type_code (pk_typeof (val))) {
      case PK_INT:
        pk_val_object = _pk_mi_make_json_int (val, errmsg);
        break;
      case PK_STRING:
        pk_val_object = _pk_mi_make_json_string (val, errmsg);
        break;
      case PK_OFFSET:
        pk_val_object = _pk_mi_make_json_offset (val, errmsg);
        break;
      case PK_STRUCT:
        pk_val_object = _pk_mi_make_json_struct (val, errmsg);
        break;
      case PK_ARRAY:
        pk_val_object = _pk_mi_make_json_array (val, errmsg);
        break;
      case PK_CLOSURE:
      case PK_ANY:
        assert (0);
    }
    if (!pk_val_object) {
      return NULL;
    }
  }

  return pk_val_object;
}

static json_object* 
_pk_mi_make_json_int (pk_val integer, char **errmsg) 
{ 
  struct json_object *int_object, *type_object, *value_object, *size_object;
  const char *type;
  int size;
  
  assert (pk_type_code (pk_typeof (integer)) == PK_INT);

  PK_MI_CHECK (errmsg, (int_object = json_object_new_object ()) != NULL, "json_object_new_object() failed");

  if  (! (pk_uint_value (pk_integral_type_signed_p (pk_typeof (integer))))) {
    PK_MI_CHECK (errmsg, (value_object = json_object_new_uint64 (pk_uint_value (integer))) != NULL, "json_object_new_object() failed");
    size = pk_uint_size (integer);
    type = "UnsignedInteger";
  }
  else {
    PK_MI_CHECK (errmsg,  (value_object = json_object_new_int64 (pk_int_value (integer))) != NULL, "json_object_new_object() failed");
    size = pk_int_size (integer);
    type = "Integer";
  }

  assert (size <= 64);

  PK_MI_CHECK (errmsg, (size_object = json_object_new_int (size)) != NULL, "json_object_new_object() failed");
  PK_MI_CHECK (errmsg, (type_object = json_object_new_string (type)) != NULL, "json_object_new_object() failed");
  
  /*OK, fill the properties of our object*/
  PK_MI_CHECK (errmsg, json_object_object_add (int_object, "type", type_object) != -1, "json_object_object_add() failed");
  PK_MI_CHECK (errmsg, json_object_object_add (int_object, "value", value_object) != -1, "json_object_object_add() failed");
  PK_MI_CHECK (errmsg, json_object_object_add (int_object, "size", size_object) != -1, "json_object_object_add() failed");

  return int_object;

  error:
    return NULL;
}

static json_object* 
_pk_mi_make_json_string (pk_val str, char **errmsg) 
{
  struct json_object *string_object, *string_type_object, *string_value_object;
  
  assert (pk_type_code (pk_typeof (str)) == PK_STRING);
  
  PK_MI_CHECK (errmsg, (string_object = json_object_new_object ()) != NULL, "json_object_new_object() failed");

  PK_MI_CHECK (errmsg, (string_type_object = json_object_new_string ("String")) != NULL, "json_object_new_object() failed");  
  PK_MI_CHECK (errmsg, (string_value_object = json_object_new_string (pk_string_str (str))) != NULL, "json_object_new_object() failed");

  /*OK, fill the properties of our object*/
  PK_MI_CHECK (errmsg, json_object_object_add (string_object, "type", string_type_object) != -1, "json_object_object_add() failed");
  PK_MI_CHECK (errmsg, json_object_object_add (string_object, "value", string_value_object) != -1, "json_object_object_add() failed");
 
  return string_object;

  error:
    return NULL;
}

static json_object* 
_pk_mi_make_json_offset (pk_val offset, char **errmsg) 
{
  struct json_object *offset_object, *offset_type_object;
  struct json_object *magnitude_object;
  struct json_object *unit_object, *unit_type_object, *unit_size_object, *unit_value_object;
  
  assert (pk_type_code (pk_typeof (offset)) == PK_OFFSET);

  PK_MI_CHECK (errmsg,  (offset_type_object = json_object_new_string ("Offset")) != NULL, "json_object_new_object() failed");

  magnitude_object = _pk_mi_make_json_int (pk_offset_magnitude (offset), errmsg);
  
  PK_MI_CHECK (errmsg,  (unit_type_object = json_object_new_string ("UnsignedInteger")) != NULL, "json_object_new_object() failed");
  PK_MI_CHECK (errmsg,  (unit_size_object = json_object_new_int (64)) != NULL, "json_object_new_object() failed");
  PK_MI_CHECK (errmsg,  (unit_value_object = json_object_new_uint64 (pk_uint_value (pk_offset_unit (offset)))) != NULL, "json_object_new_object() failed");
  
  PK_MI_CHECK (errmsg,  (unit_object = json_object_new_object ()) != NULL, "json_object_new_object() failed");

  PK_MI_CHECK (errmsg, json_object_object_add (unit_object, "type", unit_type_object) != -1, "json_object_object_add() failed");
  PK_MI_CHECK (errmsg, json_object_object_add (unit_object, "value", unit_value_object) != -1, "json_object_object_add() failed");
  PK_MI_CHECK (errmsg, json_object_object_add (unit_object, "size", unit_size_object) != -1, "json_object_object_add() failed");

  PK_MI_CHECK (errmsg,  (offset_object = json_object_new_object ()) != NULL, "json_object_new_object() failed");
  
  /*Built sub-objects, add them to our offset_object*/
  PK_MI_CHECK (errmsg, json_object_object_add (offset_object, "type", offset_type_object) != -1, "json_object_object_add() failed");
  PK_MI_CHECK (errmsg, json_object_object_add (offset_object, "magnitude", magnitude_object) != -1, "json_object_object_add() failed");
  PK_MI_CHECK (errmsg, json_object_object_add (offset_object, "unit", unit_object) != -1, "json_object_object_add() failed");

  return offset_object;

  error:
    return NULL;
}

static json_object* 
_pk_mi_make_json_mapping (pk_val val, char **errmsg) 
{
  struct json_object *mapping_object, *ios_object, *offset_object;

  offset_object = _pk_mi_make_json_offset (pk_val_offset (val), errmsg);
  
  PK_MI_CHECK (errmsg, (ios_object = json_object_new_int64 (pk_int_value (pk_val_ios (val)))) != NULL, "json_object_new_object() failed");

  PK_MI_CHECK (errmsg, (mapping_object = json_object_new_object ()) != NULL, "json_object_new_object() failed"); 

  PK_MI_CHECK (errmsg, json_object_object_add (mapping_object, "IOS", ios_object) != -1, "json_object_object_add() failed");
  PK_MI_CHECK (errmsg, json_object_object_add (mapping_object, "offset", offset_object) != -1, "json_object_object_add() failed");

  return mapping_object;

  error:
    return NULL;  
}

static json_object* 
_pk_mi_make_json_struct (pk_val sct, char **errmsg) 
{
  struct json_object *pk_struct_object, *pk_struct_type_object, *pk_struct_fields_object, *pk_struct_name_object, *pk_struct_mapping_object;
  struct json_object *pk_struct_field_object, *pk_struct_field_value_object, *pk_struct_field_offset_object, *pk_struct_field_name_object;
  
  assert (pk_type_code (pk_typeof (sct)) == PK_STRUCT);
  
  PK_MI_CHECK (errmsg,  (pk_struct_type_object = json_object_new_string ("Struct")) != NULL, "json_object_new_object() failed");
  PK_MI_CHECK (errmsg,  (pk_struct_fields_object = json_object_new_array ()) != NULL, "json_object_new_object() failed");
  PK_MI_CHECK (errmsg,  (pk_struct_name_object = _pk_mi_make_json_string (pk_struct_type_name (pk_struct_type (sct)), errmsg)) != NULL, "json_object_new_object() failed");

  /*Fill the array of struct fields*/
  for  (ssize_t i = 0 ; i < pk_uint_value (pk_struct_nfields (sct)) ; i++) {
    pk_struct_field_value_object = _pk_mi_val_to_json (pk_struct_field_value  (sct, i), errmsg);
    pk_struct_field_offset_object = _pk_mi_make_json_offset (pk_struct_field_boffset  (sct, i), errmsg);
    pk_struct_field_name_object = _pk_mi_make_json_string (pk_struct_field_name  (sct, i), errmsg);

    if (pk_struct_field_value_object == NULL || pk_struct_field_offset_object == NULL || pk_struct_field_name_object == NULL) {
      goto error;
    }
    
    pk_struct_field_object = json_object_new_object();
    json_object_object_add (pk_struct_field_object, "name", pk_struct_field_name_object);
    json_object_object_add (pk_struct_field_object, "value", pk_struct_field_value_object);
    json_object_object_add (pk_struct_field_object, "offset", pk_struct_field_offset_object);

    PK_MI_CHECK (errmsg, json_object_array_add (pk_struct_fields_object, pk_struct_field_object) != -1, "failed to add name object to struct field");
  }  

  /*Optionally, add a mapping*/
  pk_struct_mapping_object = pk_val_mapped_p  (sct) ? _pk_mi_make_json_mapping (sct, errmsg) : _pk_mi_make_json_null (errmsg);

  PK_MI_CHECK (errmsg,  (pk_struct_object = json_object_new_object ()) != NULL, "json_object_new_object() failed");

  PK_MI_CHECK (errmsg, json_object_object_add (pk_struct_object, "type", pk_struct_type_object) != -1, "failed to add type object to struct");
  PK_MI_CHECK (errmsg, json_object_object_add (pk_struct_object, "name", pk_struct_name_object) != -1, "failed to add type object to struct");
  PK_MI_CHECK (errmsg, json_object_object_add (pk_struct_object, "fields", pk_struct_fields_object) != -1, "failed to add fields object to struct");
  PK_MI_CHECK (errmsg, json_object_object_add (pk_struct_object, "mapping", pk_struct_mapping_object) != -1, "failed to add mapping object to struct");

  return pk_struct_object;

  error:
    return NULL;
}

static json_object* 
_pk_mi_make_json_array (pk_val array, char **errmsg) 
{
  struct json_object *pk_array_object, *pk_array_type_object, *pk_array_elements_object, *pk_array_mapping_object;
  struct json_object *pk_array_element_object, *pk_array_element_value_object, *pk_array_element_offset_object;

  assert (pk_type_code (pk_typeof (array)) == PK_ARRAY);
  
  PK_MI_CHECK (errmsg,  (pk_array_object = json_object_new_object ()) != NULL, "json_object_new_object() failed");

  const char *type = "Array";
  PK_MI_CHECK (errmsg,  (pk_array_type_object = json_object_new_string (type)) != NULL, "json_object_new_object() failed");
  
  /*Initialize our array*/
  PK_MI_CHECK (errmsg,  (pk_array_elements_object = json_object_new_array ()) != NULL, "json_object_new_object() failed");
   
  /*Fill elements object*/
  for  (size_t i = 0 ; i < pk_uint_value (pk_array_nelem (array)) ; i++) {
    pk_array_element_value_object = _pk_mi_val_to_json (pk_array_elem_val (array, i), errmsg);
    pk_array_element_offset_object = _pk_mi_make_json_offset (pk_array_elem_boffset (array, i), errmsg);
    
    pk_array_element_object = json_object_new_object ();
    json_object_object_add (pk_array_element_object, "value", pk_array_element_value_object);
    json_object_object_add (pk_array_element_object, "offset", pk_array_element_offset_object);

    PK_MI_CHECK (errmsg, json_object_array_add (pk_array_elements_object, pk_array_element_object) != -1, "failed to add element to array");
  }

  pk_array_mapping_object = pk_val_mapped_p (array) ? _pk_mi_make_json_mapping (array, errmsg) : _pk_mi_make_json_null (errmsg);
  if (pk_array_mapping_object == NULL) {
    goto error;
  }

  /*OK, fill the properties of the array object*/
  PK_MI_CHECK (errmsg, json_object_object_add (pk_array_object, "type", pk_array_type_object) != -1, "json_object_object_add() failed");
  PK_MI_CHECK (errmsg, json_object_object_add (pk_array_object, "elements", pk_array_elements_object) != -1, "json_object_object_add() failed");
  PK_MI_CHECK (errmsg, json_object_object_add (pk_array_object, "mapping", pk_array_mapping_object) != -1, "json_object_object_add() failed");

  return pk_array_object;

  error:
    return NULL;
}

static json_object* _pk_mi_make_json_null (char **errmsg) 
{
  struct json_object *pk_null_object;
  
  pk_null_object = json_object_new_object();
  PK_MI_CHECK (errmsg, pk_null_object != NULL, "json_object_new_object() failed");

  PK_MI_CHECK (errmsg, json_object_object_add (pk_null_object, "type", json_object_new_string ("Null")) != -1, "json_object_object_add() failed");
  PK_MI_CHECK (errmsg, json_object_object_add (pk_null_object, "value", json_object_new_null ()) != -1, "json_object_object_add() failed");

  return pk_null_object;

  error:
    return NULL;
}

const char * 
pk_mi_val_to_json (pk_val val, char **errmsg) 
{
  struct json_object *pk_val_object, *pk_object; 

  pk_val_object = _pk_mi_val_to_json (val, errmsg);
  
  pk_object = json_object_new_object ();
  PK_MI_CHECK (errmsg, pk_object != NULL, "failed to create new json_object");
  PK_MI_CHECK (errmsg, json_object_object_add (pk_object, "PokeValue", pk_val_object) != -1, "json_object_object_add() failed");

  return pk_object != NULL ? json_object_to_json_string_ext (pk_object, JSON_C_TO_STRING_PRETTY) : NULL;

  error:
    return NULL;  
}

/*Functions to convert JSON Poke Value to pk_val*/
static int _pk_mi_make_pk_int (pk_val *pk_int, struct json_object *int_obj, char **errmsg);
static int _pk_mi_make_pk_uint (pk_val *pk_uint, struct json_object *uint_obj, char **errmsg);
static int _pk_mi_make_pk_string (pk_val *pk_string, struct json_object *str_obj, char **errmsg);
static int _pk_mi_make_pk_offset (pk_val *pk_offset, struct json_object *offset_obj, char **errmsg);
static int _pk_mi_make_pk_struct (pk_val *pk_struct, struct json_object *sct_obj, char **errmsg);
static int _pk_mi_make_pk_array (pk_val *pk_array, struct json_object *array_obj, char **errmsg);
pk_val _pk_mi_json_to_val (struct json_object *obj, char **errmsg);

static const char*
_pk_mi_get_json_poke_value_type (struct json_object *obj)
{ 
  struct json_object *search_object;
  
  if (json_object_object_get_ex (obj, "type", &search_object) == 0) {
    return NULL;
  }
  
  return json_object_to_json_string (search_object);
}

static int 
_pk_mi_make_pk_int (pk_val *pk_int, struct json_object *int_obj, char **errmsg) 
{ 
  struct json_object *value_object, *size_object;  
  int64_t value; 
  int size;
  
  PK_MI_CHECK (errmsg, json_object_object_get_ex (int_obj, "value", &value_object) != 0, "json type %s does not contain key \"value\"", _pk_mi_get_json_poke_value_type (int_obj));
  value = json_object_get_int64 (value_object);

  PK_MI_CHECK (errmsg, json_object_object_get_ex (int_obj, "size", &size_object) != 0, "json type %s does not contain key \"size\"", _pk_mi_get_json_poke_value_type (int_obj));
  size = json_object_get_int (size_object);

  *pk_int = pk_make_int (value, size);
  PK_MI_CHECK (errmsg, *pk_int != PK_NULL, "pk_make_int failed");

  return 0;

  error:
    return -1;
}

static int 
_pk_mi_make_pk_uint (pk_val *pk_uint, struct json_object *uint_obj, char **errmsg) 
{ 
  struct json_object *value_object, *size_object;  
  uint64_t value; 
  int size;
  
  PK_MI_CHECK (errmsg, json_object_object_get_ex (uint_obj, "value", &value_object) != 0, "json type %s does not contain key \"value\"", _pk_mi_get_json_poke_value_type (uint_obj));
  value = json_object_get_uint64 (value_object);

  PK_MI_CHECK (errmsg, json_object_object_get_ex (uint_obj, "size", &size_object) != 0, "json type %s does not contain key \"size\"", _pk_mi_get_json_poke_value_type (uint_obj));
  size = json_object_get_int (size_object);

  *pk_uint = pk_make_uint (value, size);
  PK_MI_CHECK (errmsg, *pk_uint != PK_NULL, "pk_make_uint failed");

  return 0;

  error:
    return -1; 
}

static int 
_pk_mi_make_pk_string (pk_val *pk_string, struct json_object *str_obj, char **errmsg)
{ 
  struct json_object *value_object;
  const char *value_str;
  
  PK_MI_CHECK (errmsg, json_object_object_get_ex (str_obj, "value", &value_object) != 0, "json type %s does not contain key \"value\"", _pk_mi_get_json_poke_value_type (str_obj));
  value_str = json_object_get_string (value_object);

  *pk_string = pk_make_string (value_str);
  PK_MI_CHECK (errmsg, *pk_string != PK_NULL, "pk_make_string failed");

  return 0;

  error:
    return -1;
}

static int 
_pk_mi_make_pk_offset (pk_val *pk_offset, struct json_object *offset_obj, char **errmsg)
{
  /*To build a pk_offset, we need its magnitude and its unit*/
  pk_val magnitude, unit;
  struct json_object *magnitude_object, *unit_object;
  
  PK_MI_CHECK (errmsg, json_object_object_get_ex (offset_obj, "magnitude", &magnitude_object) != 0, "json type %s does not contain key \"magnitude\"", _pk_mi_get_json_poke_value_type (offset_obj));
  PK_MI_CHECK (errmsg, _pk_mi_make_pk_uint (&magnitude, magnitude_object, errmsg) != -1, "_pk_mi_make_pk_uint failed");

  PK_MI_CHECK (errmsg, json_object_object_get_ex (offset_obj, "unit", &unit_object) != 0, "json type %s does not contain key \"unit\"", _pk_mi_get_json_poke_value_type (offset_obj));

  assert (json_object_get_type (unit_object) == json_type_object);

  PK_MI_CHECK (errmsg, _pk_mi_make_pk_uint (&unit, unit_object, errmsg) != -1, "unable to conver offset unit to pk_uint");
  assert (pk_uint_size (unit) == 64);
  
  *pk_offset = pk_make_offset (magnitude, unit);
  PK_MI_CHECK(errmsg, *pk_offset != PK_NULL, "pk_make_offset failed");
  
  return 0;
  
  error:
    return -1;
}

static pk_val
pk_mi_make_pk_mapping ()
{
  /*TODO: build me*/
  return 0;
}

static int 
_pk_mi_make_pk_struct (pk_val *pk_struct, struct json_object *sct_obj, char **errmsg) 
{ 
  /*To build a pk_struct, we need its fields, its name and its mapping*/
  struct json_object *fields_object, *search_object;
  pk_val mapping, sct_type, sct_field_name, sct_field_value, sct_field_offset, sct, *fnames, *ftypes, nfields, name;
  size_t fields_len;
  
  PK_MI_CHECK (errmsg, json_object_object_get_ex (sct_obj, "fields", &search_object) != 0, "json type %s does does not contain key \"fields\"", _pk_mi_get_json_poke_value_type (sct_obj));
  
  fields_object = search_object;
  assert (json_object_get_type (fields_object) == json_type_array);

  fields_len = json_object_array_length (fields_object);
  
  if (fields_len > 0) {  
    PK_MI_CHECK (errmsg, json_object_object_get_ex (sct_obj, "name", &search_object) != 0, "json type %s does does not contain key \"name\"", _pk_mi_get_json_poke_value_type (sct_obj));
    nfields = pk_make_uint (fields_len, 64);
    name = _pk_mi_json_to_val (search_object, errmsg);
    pk_allocate_struct_attrs (nfields, &fnames, &ftypes);
    
    sct_type = pk_make_struct_type (nfields, name, fnames, ftypes);
    sct = pk_make_struct (nfields, sct_type);
    for (size_t i = 0 ; i < fields_len ; i++) {
      PK_MI_CHECK (errmsg, json_object_object_get_ex (json_object_array_get_idx (fields_object, i), "name", &search_object) != 0, "json type %s does not contain key \"name\"", _pk_mi_get_json_poke_value_type (json_object_array_get_idx (fields_object, i)));
      sct_field_name = _pk_mi_json_to_val (search_object, errmsg);

      PK_MI_CHECK (errmsg, json_object_object_get_ex (json_object_array_get_idx (fields_object, i), "value", &search_object) != 0, "json type %s does not contain key \"value\"", _pk_mi_get_json_poke_value_type (json_object_array_get_idx (fields_object, i)));
      sct_field_value = _pk_mi_json_to_val (search_object, errmsg);
      
      PK_MI_CHECK (errmsg, json_object_object_get_ex (json_object_array_get_idx (fields_object, i), "offset", &search_object) != 0, "json type %s does not contain key \"offset\"", _pk_mi_get_json_poke_value_type (json_object_array_get_idx (fields_object, i)));
      sct_field_offset = _pk_mi_json_to_val (search_object, errmsg);
      
      assert (pk_type_code (pk_typeof (sct_field_name)) == PK_STRING);
      assert (pk_type_code (pk_typeof (sct_field_offset)) == PK_OFFSET);
      
      pk_struct_type_set_fname (sct_type, i, sct_field_name);
      pk_struct_type_set_ftype (sct_type, i, pk_typeof(sct_field_value));
      pk_struct_set_field_boffset (sct, i, sct_field_offset);
      pk_struct_set_field_name (sct, i, sct_field_name);
      pk_struct_set_field_value (sct, i, sct_field_value);
    }

    PK_MI_CHECK (errmsg, json_object_object_get_ex (sct_obj, "mapping", &search_object) != 0, "json type %s does not contain key \"mapping\"", _pk_mi_get_json_poke_value_type (sct_obj));    
    mapping = pk_mi_make_pk_mapping (search_object, errmsg);
  }
  else {
    sct = PK_NULL;
  }

  *pk_struct = sct; 

  return 0;
  
  error:
    return -1;
}

static pk_val* 
_pk_mi_json_array_element_pair (struct json_object *elements_object, size_t idx, char **errmsg) 
{
  struct json_object *element_object, *search_object;
  /*value-offset pair*/
  pk_val *pair = (pk_val *) malloc (sizeof (pk_val) * 2);

  element_object = json_object_array_get_idx (elements_object, idx);

  PK_MI_CHECK (errmsg, json_object_object_get_ex (element_object, "value", &search_object) != 0, "json type %s does not contain key \"value\"", _pk_mi_get_json_poke_value_type (element_object));
  pair[0] =  _pk_mi_json_to_val (search_object, errmsg);
  
  PK_MI_CHECK (errmsg, json_object_object_get_ex (element_object, "offset", &search_object) != 0, "json type %s does not contain key \"offset\"", _pk_mi_get_json_poke_value_type (element_object));  
  pair[1] = _pk_mi_json_to_val (search_object, errmsg);

  return pair;

  error:
    return NULL;
} 

static int 
_pk_mi_make_pk_array (pk_val *pk_array, struct json_object *array_obj, char **errmsg)
{
  /*To build a pk_array, we need its elements and its mapping*/
  struct json_object *elements_object, *search_object;
  pk_val mapping, array_etype, array, *element_pair;
  size_t elements_len;
  
  PK_MI_CHECK (errmsg, json_object_object_get_ex (array_obj, "elements", &search_object) != 0, "json type %s does not contain key \"elements\"", _pk_mi_get_json_poke_value_type (array_obj));
  
  elements_object = search_object; 
  assert (json_object_get_type (elements_object) == json_type_array);
  
  elements_len = json_object_array_length (elements_object);
  
  if (elements_len > 0) {
    /*We need to get the type of array elements first*/
    element_pair = _pk_mi_json_array_element_pair (elements_object, 0, errmsg);
    if (element_pair == NULL) {
      goto error;
    }
    
    array_etype = pk_type_code (pk_typeof (element_pair[0]));
    array = pk_make_array (pk_make_uint (elements_len, 64), pk_make_array_type (array_etype, PK_NULL));
    
    pk_array_set_elem_val (array, 0, element_pair[0]);
    pk_array_set_elem_boffset (array, 0, element_pair[1]);
    
    for (size_t i = 1 ; i < elements_len ; i++) {
      element_pair = _pk_mi_json_array_element_pair (elements_object, i, errmsg);
      
      assert (pk_type_code (pk_typeof (element_pair[0])) == array_etype);
      assert (pk_type_code (pk_typeof (element_pair[1])) == PK_OFFSET);

      pk_array_set_elem_val (array, i, element_pair[0]);
      pk_array_set_elem_boffset (array, i, element_pair[1]);
    }

    PK_MI_CHECK (errmsg, json_object_object_get_ex (array_obj, "mapping", &search_object) != 0, "json type %s does not contain key \"mapping\"", _pk_mi_get_json_poke_value_type (array_obj));
    
    mapping = pk_mi_make_pk_mapping (search_object);

    free(element_pair);
  }
  else {
    array = PK_NULL;
  }

  *pk_array = array;
  
  return 0;

  error:
    return -1;
}

pk_val 
_pk_mi_json_to_val (struct json_object *obj, char **errmsg)
{ 
  pk_val poke_value; 
  const char *poke_object_type;
  
  poke_object_type = _pk_mi_get_json_poke_value_type (obj);
  if (poke_object_type == NULL) {
    goto error;
  }
 
  if (!strncmp (poke_object_type, "\"Integer\"", strlen ("\"Integer\""))) {
    if (_pk_mi_make_pk_int (&poke_value, obj, errmsg) == -1) {
      goto error;
    }
  }
  else if (!strncmp (poke_object_type, "\"UnsignedInteger\"", strlen ("\"UnsignedInteger\""))) {
    if (_pk_mi_make_pk_uint (&poke_value, obj, errmsg) == -1) {
      goto error;
    }
  }
  else if (!strncmp (poke_object_type, "\"String\"", strlen ("\"String\""))) {
    if (_pk_mi_make_pk_string (&poke_value, obj, errmsg) == -1) {
      goto error;
    }
  }
  else if (!strncmp (poke_object_type, "\"Offset\"", strlen ("\"Offset\""))) {
    if (_pk_mi_make_pk_offset (&poke_value, obj, errmsg) == -1) {
      goto error;
    }
  }
  else if (!strncmp (poke_object_type, "\"Array\"", strlen ("Array"))) {
    if (_pk_mi_make_pk_array (&poke_value, obj, errmsg) == -1) {
      goto error;
    }
  }
  else if (!strncmp (poke_object_type, "\"Struct\"", strlen ("\"Struct\""))) {\
    if (_pk_mi_make_pk_struct (&poke_value, obj, errmsg) == -1) {
      goto error;
    }
  }
  else if (!strncmp (poke_object_type, "\"Null\"", strlen("\"Null\""))) {
    poke_value = PK_NULL;
  }
  else {
    PK_MI_SET_ERRMSG (errmsg, "An error happened, this is a bug.");
    return PK_NULL;
  }

  return poke_value;

  error:
    return PK_NULL;
}

pk_val 
pk_mi_json_to_val (const char *poke_object_str, char **errmsg)
{
  pk_val poke_value; 
  struct json_object *search_object, *poke_object = NULL;
  struct json_tokener *tok = json_tokener_new ();
  enum json_tokener_error jerr;
  
  /*Parse the current object and get its PK_TYPE*/
  do {
    poke_object = json_tokener_parse_ex (tok, poke_object_str, strlen (poke_object_str));
  } while ((jerr = json_tokener_get_error (tok)) == json_tokener_continue);
  
  PK_MI_CHECK(errmsg, jerr == json_tokener_success, "%s", json_tokener_error_desc (jerr));
  
  search_object = json_object_object_get (poke_object, "PokeValue");
  PK_MI_CHECK(errmsg, search_object != NULL, "Not a valid PokeValue object");
  
  poke_value = _pk_mi_json_to_val (search_object, errmsg);

  json_tokener_free (tok);

  return poke_value; 

  error:
    return PK_NULL;
}