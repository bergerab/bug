#include "string.h"

void string_reverse(struct object *o) {
  ufixnum_t i;
  char temp;

  OT("string_reverse", 0, o, type_string);

  for (i = 0; i < STRING_LENGTH(o)/2; ++i) {
    temp = STRING_CONTENTS(o)[i];
    STRING_CONTENTS(o)[i] = STRING_CONTENTS(o)[STRING_LENGTH(o)-1-i];
    STRING_CONTENTS(o)[STRING_LENGTH(o)-1-i] = temp;
  }
}

struct object *string_concat(struct object *s0, struct object *s1) {
  struct object *s2; 

  OT2("string_concat", 0, s0, type_string, type_dynamic_byte_array);
  OT2("string_concat", 1, s1, type_string, type_dynamic_byte_array);

  s2 = dynamic_byte_array_concat(s0, s1);
  OBJECT_TYPE(s2) = type_string;
  return s2;
}

struct object *string_clone(struct object *str0) {
  struct object *str1;
  OT("string_clone", 0, str0, type_string);
  str1 = dynamic_byte_array(STRING_LENGTH(str0));
  memcpy(DYNAMIC_BYTE_ARRAY_BYTES(str1), DYNAMIC_BYTE_ARRAY_BYTES(str0), STRING_LENGTH(str0));
  OBJECT_TYPE(str1) = type_string;
  STRING_LENGTH(str1) = STRING_LENGTH(str0);
  return str1;
}

struct object *to_string_ufixnum_t(ufixnum_t n) {
  struct object *str;
  ufixnum_t cursor;

  if (n == 0) return string("0");
  str = string("");
  cursor = n;
  while (cursor != 0) {
    dynamic_byte_array_push_char(str, '0' + cursor % 10);
    cursor /= 10;
  }
  string_reverse(str);
  return str;
}

struct object *to_string_fixnum_t(fixnum_t n) {
  if (n < 0)
    return dynamic_byte_array_concat(string("-"), to_string_ufixnum_t(-n));
  return to_string_ufixnum_t(n);
}

/* a buggy float to string implementation 
   shows a maximum of 9 decimal digits even if the double contains more */
struct object *to_string_flonum_t(flonum_t n) {
  ufixnum_t integral_part, decimal_part;
  ufixnum_t ndecimal, nintegral; /* how many digits in the decimal and integral part to keep */
  flonum_t nexp, nexp2;
  fixnum_t i, leading_zero_threshold;
  struct object *str;

  if (n == 0) return string("0.0");
  if (n != n) return string("NaN");
  if (n > DBL_MAX) return string("Infinity"); /* TODO: update to be FLONUM_MAX/FLONUM_MIN*/
  if (n < DBL_MIN) return string("-Infinity");

  ndecimal = 9;
  nintegral = 16; /* the threshold where scientific notation is used */
  leading_zero_threshold = 7; /* the threshold when scientific notation is used */

  nexp = n == 0 ? 0 : log10(n);

  integral_part = n; /* casting to a ufixnum extracts the integral part */
  decimal_part = (n - integral_part) * pow(10, ndecimal);

  while (decimal_part > 0 && decimal_part % 10 == 0) decimal_part /= 10;

  if (nexp > nintegral) {
    /* trim all right most 0s (those will be the decimal part of the normalized output) */
    while (integral_part > 0 && integral_part % 10 == 0) integral_part /= 10;

    nexp2 = log10(integral_part);
    if (nexp2 > ndecimal + 1) {
      for (i = nexp2 - ndecimal; i > 0;
           --i) { /* keep ndecimal + 1 digits (plus one is to keep the first
                     part of the scientific notation 9.2342341e+4) */
        if (i == 1 && integral_part % 10 > 4) {
          integral_part += 10; /* round if this is the last digit and 5 or more */
        }
        integral_part /= 10;
      }
    }
    str = to_string_ufixnum_t(integral_part);
    if (integral_part >= 10)
      dynamic_byte_array_insert_char(str, 1, '.');
    dynamic_byte_array_push_char(str, 'e');
    dynamic_byte_array_push_char(str, '+');
    str = dynamic_byte_array_concat(str, to_string_fixnum_t(nexp));
    return str;
  }

  if (nexp <= -leading_zero_threshold) { /* check if this could be a 0.{...0...}n
                                           number */
    /* first is to normalize 0.00000000nnnn to 0.nnnn, second is to convert to unsigned fixnum (plus one because one extra digit will be in the integral part another plus one so we can have one digit for rounding) */
    decimal_part = n * pow(10, (fixnum_t)-nexp) * pow(10, ndecimal + 1 + 1);
    if (decimal_part % 10 > 4) {
      decimal_part += 10; /* round if this is the last digit and 5 or more */
    }
    decimal_part /= 10; /* drop the extra digit we grabbed for rounding */
    while (decimal_part > 0 && decimal_part % 10 == 0) decimal_part /= 10; /* trim all rhs zeros */
    str = to_string_ufixnum_t(decimal_part);
    if (decimal_part >= 10) dynamic_byte_array_insert_char(str, 1, '.');
    dynamic_byte_array_push_char(str, 'e');
    str = dynamic_byte_array_concat(str, to_string_fixnum_t(floor(nexp)));
    return str;
  }

  str = to_string_ufixnum_t(integral_part);
  dynamic_byte_array_push_char(str, '.');
  while (nexp < -1) {
    dynamic_byte_array_push_char(str, '0');
    ++nexp;
  }
  str = dynamic_byte_array_concat(str, to_string_fixnum_t(decimal_part));
  return str;
}

struct object *to_string_dynamic_byte_array(struct object *dba) {
  struct object *str;
  ufixnum_t i;
  unsigned char byte, nib0, nib1;

  OT("to_string_dynamic_byte_array", 0, dba, type_dynamic_byte_array);

  str = string("[");
  for (i = 0; i < DYNAMIC_BYTE_ARRAY_LENGTH(dba); ++i) {
    byte = DYNAMIC_BYTE_ARRAY_BYTES(dba)[i];
    nib0 = byte & 0x0F;
    nib1 = byte >> 4;

    dynamic_byte_array_push_char(str, '0');
    dynamic_byte_array_push_char(str, 'x');
    if (nib1 > 9) dynamic_byte_array_push_char(str, 'A' + (nib1 - 10));
    else dynamic_byte_array_push_char(str, '0' + nib1);
    if (nib0 > 9) dynamic_byte_array_push_char(str, 'A' + (nib0 - 10));
    else dynamic_byte_array_push_char(str, '0' + nib0);

    dynamic_byte_array_push_char(str, ' ');
  }
  if (DYNAMIC_BYTE_ARRAY_LENGTH(dba) > 0)
    dynamic_byte_array_pop(str); /* remove the extra space */
  dynamic_byte_array_push_char(str, ']');
  return str;
}

struct object *to_string_vec2(struct object *vec2) {
  struct object *str;

  OT("to_string_vec2", 0, vec2, type_vec2);

  str = string("<");
  str = dynamic_byte_array_concat(str, to_string_flonum_t(VEC2_X(vec2)));
  dynamic_byte_array_push_char(str, ' ');
  str = dynamic_byte_array_concat(str, to_string_flonum_t(VEC2_Y(vec2)));
  dynamic_byte_array_push_char(str, '>');
  OBJECT_TYPE(str) = type_string;
  return str;
}

struct object *to_string_dynamic_array(struct object *da) {
  struct object *str;
  ufixnum_t i;

  OT("to_string_dynamic_array", 0, da, type_dynamic_array);

  str = string("[");
  for (i = 0; i < DYNAMIC_ARRAY_LENGTH(da); ++i) {
    str = string_concat(str, do_to_string(dynamic_array_get_ufixnum_t(da, i), 1));
    dynamic_byte_array_push_char(str, ' ');
  }
  if (DYNAMIC_ARRAY_LENGTH(da) > 0)
    dynamic_byte_array_pop(str); /* remove the extra space */
  dynamic_byte_array_push_char(str, ']');
  return str;
}

struct object *do_to_string(struct object *o, char repr) {
  struct object *str;
  char buf[100];
  enum object_type t;

  t = object_type_of(o);

  switch (t) {
    case type_cons:
      str = string("(");
      str = string_concat(str, do_to_string(CONS_CAR(o), 1));
      while (type_of(CONS_CDR(o)) == gis->cons_type) {
        o = CONS_CDR(o);
        str = string_concat(str, string(" "));
        str = string_concat(str, do_to_string(CONS_CAR(o), 1));
      }
      if (CONS_CDR(o) == NIL) {
        dynamic_byte_array_push_char(str, ')');
      } else {
        str = string_concat(str, string(" "));
        str = string_concat(str, do_to_string(CONS_CDR(o), 1));
        dynamic_byte_array_push_char(str, ')');
      }
      return str;
    case type_string:
      if (repr) {
        o = string_concat(string("\""), o);
        dynamic_byte_array_push_char(o, '\"');
      }
      return o;
    case type_flonum:
      return to_string_flonum_t(FLONUM_VALUE(o));
    case type_ufixnum:
      return to_string_ufixnum_t(UFIXNUM_VALUE(o));
    case type_fixnum:
      return to_string_fixnum_t(FIXNUM_VALUE(o));
    case type_dynamic_byte_array:
      return to_string_dynamic_byte_array(o);
    case type_vec2:
      return to_string_vec2(o);
    case type_dynamic_array:
      return to_string_dynamic_array(o);
    case type_function:
      if (0 && FUNCTION_NAME(o) != NIL) {
        str = string("<function ");
        str = string_concat(str, SYMBOL_NAME(FUNCTION_NAME(o)));
        dynamic_byte_array_push_char(str, '>');
        return str;
      }
      str = string("<function ");
      str = string_concat(str, do_to_string(FUNCTION_CONSTANTS(o), 1));
      dynamic_byte_array_push_char(str, ' ');
      str = string_concat(str, do_to_string(FUNCTION_CODE(o), 1));
      dynamic_byte_array_push_char(str, '>');
      return str;
    case type_record:
      return string("<record>");
    case type_package:
      str = string("<package ");
      str = string_concat(str, do_to_string(PACKAGE_NAME(o), 1));
      dynamic_byte_array_push_char(str, '>');
      return str;
    case type_symbol:
      return SYMBOL_NAME(o);
    case type_dlib:
      str = string("<dynamic-library \"");
      str = string_concat(str, DLIB_PATH(o));
      dynamic_byte_array_push_char(str, '"');
      dynamic_byte_array_push_char(str, '>');
      return str;
    case type_ffun:
      str = string("<foreign-function \"");
      str = string_concat(str, FFUN_FFNAME(o));
      dynamic_byte_array_push_char(str, '"');
      dynamic_byte_array_push_char(str, ' ');
      str = string_concat(str, do_to_string(FFUN_DLIB(o), 1));
      dynamic_byte_array_push_char(str, '>');
      return str;
    case type_ptr:
      str = string("<pointer ");
      sprintf(buf, "%p", OBJECT_POINTER(o));
      dynamic_byte_array_push_char(str, '0');
      dynamic_byte_array_push_char(str, 'x');
      str = string_concat(str, string(buf));
      dynamic_byte_array_push_char(str, '>');
      return str;
    case type_struct:
      str = string("<struct ");
      str = string_concat(str, STRUCTURE_NAME(o));
      dynamic_byte_array_push_char(str, '>');
      return str;
    case type_type:
      str = string("<type ");
      str = string_concat(str, TYPE_NAME(o));
      dynamic_byte_array_push_char(str, '>');
      return str;
    case type_enumerator:
      return string("<enumerator>");
    case type_file:
      return string("<file>");
  }

  /* handle user defined type */
  str = string("<");
  str = string_concat(str, TYPE_NAME(type_of(o)));
  dynamic_byte_array_push_char(str, ' ');
  sprintf(buf, "%p", OBJECT_POINTER(o));
  str = string_concat(str, string(buf));
  dynamic_byte_array_push_char(str, '>');
  return str;
}

struct object *to_string(struct object *o) {
  return do_to_string(o, 0);
}

struct object *to_repr(struct object *o) {
  return do_to_string(o, 1);
}

struct object *string_designator(struct object *sd) {
  struct object *t = type_of(sd);
  if (t == gis->string_type)
    return sd;
  else if (t == gis->symbol_type)
    return SYMBOL_NAME(sd);
  else {
    printf("Invalid string designator %s.\n", type_name_cstr(t));
    PRINT_STACK_TRACE_AND_QUIT();
  }
}
