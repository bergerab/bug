#include "marshal.h"

/*===============================*
 *===============================*
 * String cache                  *
 *===============================*
 *===============================*/
struct object *string_marshal_cache_get_default() {
  struct object *cache;
  cache = dynamic_array(20);
  /* call out a bunch of names that could be frequently used in the bytecode.
     if a symbol (used as a value or looked up directly) uses one of the names below
     for its symbol-name or the home package's name. */
  dynamic_array_push(cache, string("user"));
  dynamic_array_push(cache, string("lisp"));
  dynamic_array_push(cache, string("keyword"));
  dynamic_array_push(cache, string("impl"));
  dynamic_array_push(cache, string("t"));
  dynamic_array_push(cache, string("var"));
  dynamic_array_push(cache, string("list"));
  dynamic_array_push(cache, string("cons"));
  return cache;
}

struct object *do_string_marshal_cache_intern_cstr(struct object *cache, char *str, ufixnum_t length, ufixnum_t *index, struct object *existing_str) {
  ufixnum_t i, j;
  struct object *ostr;
  for (i = 0; ; ++i) {
    skip_item:
    if (i >= DYNAMIC_ARRAY_LENGTH(cache)) break;
    ostr = DYNAMIC_ARRAY_VALUES(cache)[i];
    /* if the lengths don't match, the string will never match */
    if (length == STRING_LENGTH(ostr)) {
      /* check each byte of the strings */
      for (j = 0; j < length; ++j) {
        if (str[j] != STRING_CONTENTS(ostr)[j]) {
          ++i;
          goto skip_item;
        }
      }
      *index = i;
      return ostr;
    }
  }
  /* add to cache */
  if (existing_str == NULL) { 
    existing_str = dynamic_byte_array(length);
    memcpy(DYNAMIC_BYTE_ARRAY_BYTES(existing_str), str, length);
    STRING_LENGTH(existing_str) = length;
    OBJECT_TYPE(existing_str) = type_string;
  }
  dynamic_array_push(cache, existing_str);
  *index = DYNAMIC_ARRAY_LENGTH(cache) - 1;
  return existing_str;
}

struct object *string_marshal_cache_intern_cstr(struct object *cache, char *str, ufixnum_t *index) {
  return do_string_marshal_cache_intern_cstr(cache, str, strlen(str), index, NULL);
}

struct object *string_marshal_cache_intern(struct object *cache, struct object *str, ufixnum_t *index) {
  dynamic_byte_array_force_cstr(str);
  return do_string_marshal_cache_intern_cstr(cache, STRING_CONTENTS(str), STRING_LENGTH(str), index, str);
}
/*===============================*
 *===============================*
 * Marshaling                    *
 *===============================*
 *===============================*/
struct object *marshal_fixnum_t(fixnum_t n, struct object *ba) {
  unsigned char byte;

  if (ba == NULL)
    ba = dynamic_byte_array(4);
  /* the header must be included, because it includes sign information */
  dynamic_byte_array_push_char(ba, n < 0 ? marshaled_type_negative_integer : marshaled_type_integer);
  n = n < 0 ? -n : n; /* abs */
  do {
    byte = n & 0x7F;
    if (n > 0x7F) /* flip the continuation bit if there are more bytes */
      byte |= 0x80;
    dynamic_byte_array_push_char(ba, byte);
    n >>= 7;
  } while (n > 0);
  return ba;
}

struct object *marshal_ufixnum_t(ufixnum_t n, struct object *ba, char include_header) {
  unsigned char byte;
  if (ba == NULL)
    ba = dynamic_byte_array(4);
  /* the header is optional, because the code that unmarshals might already know that the number must be positive (e.g. string lengths). */
  if (include_header) dynamic_byte_array_push_char(ba, marshaled_type_integer);
  do {
    byte = n & 0x7F;
    if (n > 0x7F) /* flip the continuation bit if there are more bytes */
      byte |= 0x80;
    dynamic_byte_array_push_char(ba, byte);
    n >>= 7;
  } while (n > 0);
  return ba;
}

struct object *marshal_string(struct object *str, struct object *ba, char include_header, struct object *cache) {
  ufixnum_t i;
  OT("marshal_string", 0, str, type_string);
  if (ba == NULL)
    ba = dynamic_byte_array(10);
  if (include_header)
    dynamic_byte_array_push_char(ba, marshaled_type_string);
  if (cache == NULL) { /* if not using a cache, use the string directly */
    marshal_ufixnum_t(STRING_LENGTH(str), ba, 0);
    dynamic_byte_array_push_all(ba, str);
  } else { /* otherwise, just the cache index value */
    string_marshal_cache_intern(cache, str, &i);
    marshal_ufixnum_t(i, ba, 0);
  }
  return ba;
}

/** there is no option to disable to header, because the header contains important information
    (if the symbol has a home package or not) */
struct object *marshal_symbol(struct object *sym, struct object *ba, struct object *cache) {
  OT("marshal_symbol", 0, sym, type_symbol);
  if (ba == NULL)
    ba = dynamic_byte_array(10);
  if (SYMBOL_PACKAGE(sym) == NIL) {
    dynamic_byte_array_push_char(ba, marshaled_type_uninterned_symbol);
  } else {
    dynamic_byte_array_push_char(ba, marshaled_type_symbol);
    marshal_string(PACKAGE_NAME(SYMBOL_PACKAGE(sym)), ba, 0, cache);
  }
  marshal_string(SYMBOL_NAME(sym), ba, 0, cache);
  return ba;
}

struct object *marshal_dynamic_byte_array(struct object *ba0, struct object *ba, char include_header) {
  OT("marshal_dynamic_byte_array", 0, ba0, type_dynamic_byte_array);
  if (ba == NULL)
    ba = dynamic_byte_array(10);
  if (include_header)
    dynamic_byte_array_push_char(ba, marshaled_type_dynamic_byte_array);
  marshal_ufixnum_t(DYNAMIC_BYTE_ARRAY_LENGTH(ba0), ba, 0);
  dynamic_byte_array_push_all(ba, ba0);
  return ba;
}

struct object *marshal_nil(struct object *ba) {
  if (ba == NULL)
    ba = dynamic_byte_array(1);
  dynamic_byte_array_push_char(ba, marshaled_type_nil);
  return ba;
}

struct object *marshal_cons(struct object *o, struct object *ba, char include_header, struct object *cache) {
  OT("marshal_cons", 0, o, type_cons);
  if (ba == NULL)
    ba = dynamic_byte_array(10);
  if (include_header)
    dynamic_byte_array_push_char(ba, marshaled_type_cons);
  marshal(CONS_CAR(o), ba, cache);
  marshal(CONS_CDR(o), ba, cache);
  return ba;
}

struct object *marshal_dynamic_array(struct object *arr, struct object *ba, char include_header, struct object *cache) {
  ufixnum_t arr_length, i;
  struct object **c_arr;
  OT("marshal_dynamic_array", 0, arr, type_dynamic_array);
  if (ba == NULL)
    ba = dynamic_byte_array(10);
  if (include_header)
    dynamic_byte_array_push_char(ba, marshaled_type_dynamic_array);
  arr_length = DYNAMIC_ARRAY_LENGTH(arr);
  c_arr = DYNAMIC_ARRAY_VALUES(arr);
  marshal_ufixnum_t(arr_length, ba, 0);
  for (i = 0; i < arr_length; ++i) marshal(c_arr[i], ba, cache);
  return ba;
}

struct object *marshal_dynamic_string_array(struct object *arr, struct object *ba, char include_header, struct object *cache, ufixnum_t start_index) {
  OT("marshal_dynamic_string_array", 0, arr, type_dynamic_array);
  if (ba == NULL)
    ba = dynamic_byte_array(10);
  if (include_header)
    dynamic_byte_array_push_char(ba, marshaled_type_dynamic_string_array);
  marshal_ufixnum_t(DYNAMIC_ARRAY_LENGTH(arr) - start_index, ba, 0);
  for (; start_index < DYNAMIC_ARRAY_LENGTH(arr); ++start_index) marshal_string(DYNAMIC_ARRAY_VALUES(arr)[start_index], ba, 0, cache);
  return ba;
}

/**
 * The format used for marshaling fixnums is platform independent
 * it has the following form (each is one byte):
 *     0x04 <sign> <continuation_bit=1|7bits> <continuation_bit=1|7bits> etc...
 * <continutation_bit=0|7bits> a sign of 1 means it is a negative number, a sign
 * of 0 means positive number the continuation bit of 1 means the next byte is
 * still part of the number once the continuation bit of 0 is reached, the
 * number is complete
 *
 * I'm not sure if there is any advantage between using sign magnitude vs 1 or
 * 2s compliment for the marshaling format besides the memory cost of the sign
 * byte (1 byte per number).
 */
struct object *marshal_fixnum(struct object *n, struct object *ba) {
  OT("marshal_fixnum", 0, n, type_fixnum);
  return marshal_fixnum_t(FIXNUM_VALUE(n), ba);
}

struct object *marshal_ufixnum(struct object *n, struct object *ba, char include_header) {
  OT2("marshal_ufixnum", 0, n, type_ufixnum, type_fixnum);
  return marshal_ufixnum_t(UFIXNUM_VALUE(n), ba, include_header);
}

struct object *marshal_16_bit_fix(fixnum_t n, struct object *ba) {
  if (ba == NULL)
    ba = dynamic_byte_array(4);
  dynamic_byte_array_push_char(ba, n >> 8);
  dynamic_byte_array_push_char(ba, n & 0xFF);
  return ba;
}

struct object *marshal_flonum(flonum_t n, struct object *ba) {
  ufixnum_t mantissa_fix;
  flonum_t mantissa;
  int exponent;

  if (ba == NULL)
    ba = dynamic_byte_array(10);
  dynamic_byte_array_push_char(ba, n < 0 ? marshaled_type_negative_float : marshaled_type_float);
  /* Inefficient in both time and space but work short term.
   *
   * The idea was for the marshaled format to be as exact as possible
   * (arbitrary precision), while have the types used in the program be inexact
   * (e.g. float, double). I chose to use the base 2 floating point format
   * (mantissa * 2^exponent = value) using arbitrary long mantissas and
   * exponents. This is probably naive and might not work for NaN or Infinity.
   */
  mantissa = frexp(n, &exponent);
  /* frexp keeps sign information on the mantissa, we convert it to a unsigned
   * fixnum (hence the abs(...)). */
  /* we already have the sign information in the marshaled flonum */
  mantissa = mantissa < 0 ? -mantissa : mantissa;
  mantissa_fix = mantissa * pow(2, DBL_MANT_DIG);
  /* TODO: make it choose between DBL/FLT properly */
  marshal_ufixnum_t(mantissa_fix, ba, 0);
  marshal_16_bit_fix(exponent, ba);
  return ba;
}

/**
 * turns function into a byte-array that can be stored to a file
 */
struct object *marshal_function(struct object *bc, struct object *ba, char include_header, struct object *cache) {
  OT("marshal_function", 0, bc, type_function);
  if (ba == NULL)
    ba = dynamic_byte_array(100);
  if (include_header)
    dynamic_byte_array_push_char(ba, marshaled_type_function);
  marshal_dynamic_array(FUNCTION_CONSTANTS(bc), ba, 0, cache);
  marshal_ufixnum_t(FUNCTION_STACK_SIZE(bc), ba, 0);
  marshal_dynamic_byte_array(FUNCTION_CODE(bc), ba, 0);
  marshal_ufixnum_t(FUNCTION_NAME(bc) != NIL, ba, 0);
  if (FUNCTION_NAME(bc) != NIL)
    marshal_symbol(FUNCTION_NAME(bc), ba, cache);
  marshal_ufixnum_t(FUNCTION_NARGS(bc), ba, 0);
  marshal_ufixnum_t(FUNCTION_ACCEPTS_ALL(bc), ba, 0);
  return ba;
}

struct object *marshal_vec2(struct object *vec2, struct object *ba, char include_header) {
  OT("marshal_vec2", 0, vec2, type_vec2);
  if (ba == NULL)
    ba = dynamic_byte_array(10);
  if (include_header)
    dynamic_byte_array_push_char(ba, marshaled_type_vec2);
  marshal_flonum(VEC2_X(vec2), ba);
  marshal_flonum(VEC2_Y(vec2), ba);
  return ba;
}

/**
 * Takes any object and returns a byte-array containing the binary
 * representation of the object.
 */
struct object *marshal(struct object *o, struct object *ba, struct object *cache) {
  struct object *t;
  if (ba == NULL) ba = dynamic_byte_array(10);
  if (o == NIL) return marshal_nil(ba);
  t = type_of(o);
  if (t == gis->dynamic_byte_array_type) {
      return marshal_dynamic_byte_array(o, ba, 1);
  } else if (t == gis->dynamic_array_type) {
      return marshal_dynamic_array(o, ba, 1, cache);
  } else if (t == gis->cons_type) {
      return marshal_cons(o, ba, 1, cache);
  } else if (t == gis->vec2_type) {
      return marshal_vec2(o, ba, 1);
  } else if (t == gis->fixnum_type) {
      return marshal_fixnum(o, ba);
  } else if (t == gis->ufixnum_type) {
      return marshal_ufixnum(o, ba, 1);
  } else if (t == gis->flonum_type) {
      return marshal_flonum(FLONUM_VALUE(o), ba);
  } else if (t == gis->string_type) {
      return marshal_string(o, ba, 1, cache);
  } else if (t == gis->symbol_type) {
      return marshal_symbol(o, ba, cache);
  } else if (t == gis->function_type) {
      return marshal_function(o, ba, 1, cache);
  } else {
      printf("BC: cannot marshal type %s.\n", type_name_of_cstr(o));
      PRINT_STACK_TRACE_AND_QUIT();
  }
}

/*===============================*
 *===============================*
 * Unmarshaling                  *
 *===============================*
 *===============================*/
/* these are used for lengths -- this will never be used for numbers used in the code
   those use unmarshal_integer, because it is uncertain if they will fit into a fix/ufix/flo.
   But these are required to fit into a ufixnum on this machine. It would be an error if the number
   doesn't fit into the ufix. 
   
   this always assumes the data was written with include_header=0. */
ufixnum_t unmarshal_ufixnum_t(struct object *s) {
  ufixnum_t n;
  ufixnum_t byte;
  char byte_count;
  s = byte_stream_lift(s);
  OT2("unmarshal_ufixnum_t", 0, s, type_enumerator, type_file);
  byte = byte_stream_read_byte(s);
  n = byte & 0x7F;
  byte_count = 1;
  while (byte & 0x80) {
    byte = byte_stream_read_byte(s);
    n = ((byte & 0x7F) << (7 * byte_count)) | n;
    ++byte_count;
    /* TODO: error if trying to read more bytes than what fits into a ufix */
  }
  return n;
}

fixnum_t unmarshal_16_bit_fix(struct object *s) {
  fixnum_t n;
  s = byte_stream_lift(s);
  OT2("unmarshal_16_bit_fix", 0, s, type_enumerator, type_file);
  n = byte_stream_read_byte(s) << 8;
  n = byte_stream_read_byte(s) | n;
  return n;
}

struct object *unmarshal_integer(struct object *s) {
  unsigned char t, sign, is_flo, is_init_flo;
  ufixnum_t ufix, next_ufix, byte_count,
      byte; /* the byte must be a ufixnum because it is used in operations that
               result in ufixnum */
  fixnum_t fix, next_fix;
  flonum_t flo;

  s = byte_stream_lift(s);
  OT2("unmarshal_integer", 0, s, type_enumerator, type_file);
  t = byte_stream_read_byte(s);
  if (t != marshaled_type_integer && t != marshaled_type_negative_integer) {
    printf(
        "BC: unmarshal expected marshaled_integer or "
        "marsahled_negative_interger type, but "
        "was %d.",
        t);
    PRINT_STACK_TRACE_AND_QUIT();
  }

  sign = t == marshaled_type_negative_integer;

  is_flo = 0;
  is_init_flo = 0;
  ufix = 0;
  fix = 0;
  flo = 0;
  byte_count = 0; /* number of bytes we have read (each byte contains 7 bits of
                     the number) */
  do {
    byte = byte_stream_read_byte(s);

    /* if the number fit into a ufixnum or a fixnum, use a flonum */
    if (is_flo) {
      /* if the fixnum was just exceeded */
      if (!is_init_flo) {
        flo = sign == 1 ? fix : ufix;
        is_init_flo = 1;
      }
      flo += pow(2, 7 * byte_count) * (byte & 0x7F);
    } else {
      if (sign == 1) { /* if the number is negative, it must be a signed fixnum */
        next_fix = fix | (byte & 0x7F) << (7 * byte_count);
        if (next_fix < fix ||
            -next_fix >
                -fix) { /* if overflow occurred, or underflow occurred */
          is_flo = 1;
          --byte_count; /* force the next iteration to process the number as a
                           flonum re-use the same byte, because this one
                           couldn't fit. */
        } else {
          fix = next_fix;
        }
      } else { /* otherwise try to fit it into a unsigned fixnum */
        next_ufix = ufix | ((byte & 0x7F) << (7 * byte_count));
        if (next_ufix < ufix) { /* if overflow occurred */
          is_flo = 1;
          --byte_count; /* force the next iteration to process the number as a
                           flonum re-use the same byte, because this one
                           couldn't fit. */
        } else {
          ufix = next_ufix;
        }
      }
    }
    ++byte_count;
  } while (byte & 0x80);

  if (is_flo) return flonum(sign ? -flo : flo);
  if (sign == 1) {
    return fixnum(sign ? -fix : fix);
  }
  /* if the ufix fits into a fix, use it as a fix */
  if (ufix < INT64_MAX) { /* TODO: define FIXNUM_MAX */
    return fixnum(ufix);
  }
  return ufixnum(ufix);
}

flonum_t unmarshal_float_t(struct object *s) {
  unsigned char t;
  fixnum_t exponent;
  flonum_t flo, mantissa;
  ufixnum_t mantissa_fix;
  s = byte_stream_lift(s);
  OT2("unmarshal_float", 0, s, type_file, type_enumerator);
  t = byte_stream_read_byte(s);
  if (t != marshaled_type_float && t != marshaled_type_negative_float) {
    printf("BC: unmarshal expected float type, but was %d.", t);
    PRINT_STACK_TRACE_AND_QUIT();
  }
  mantissa_fix = unmarshal_ufixnum_t(s);
  mantissa = (flonum_t)mantissa_fix / pow(2, DBL_MANT_DIG);
  /* TODO: make it choose between DBL/FLT properly */
  exponent = unmarshal_16_bit_fix(s);
  flo = ldexp(mantissa, exponent);
  /* the sign could also be embedded in the "exponent" part -- use one bit for
     it causing the exponent to use 15 bits. But this won't actually save space
     because all floats current need the header anyway, so may as well use the
     header for signing information (it is easier to code that). */
  return t == marshaled_type_negative_float ? -flo : flo;
}

struct object *unmarshal_float(struct object *s) {
  return flonum(unmarshal_float_t(s));
}

/* clone_from_cache -- if cache is not NULL, this parameter is used. If clone_from_cache=1, when
   the string is taken from the cache, it will immediately clone it (intended for strings that can be
   mutated). */
struct object *unmarshal_string(struct object *s, char includes_header, struct object *cache, char clone_from_cache) {
  unsigned char t;
  struct object *str;
  ufixnum_t length;
  s = byte_stream_lift(s);
  OT2("unmarshal_string", 0, s, type_file, type_enumerator);
  if (includes_header) {
    t = byte_stream_read_byte(s);
    if (t != marshaled_type_string) {
      printf("BC: unmarshal expected string type, but was %d.", t);
      PRINT_STACK_TRACE_AND_QUIT();
    }
  }
  if (cache == NULL) {
    length = unmarshal_ufixnum_t(s);
    str = byte_stream_read(s, length);
    OBJECT_TYPE(str) = type_string;
  } else {
    length = unmarshal_ufixnum_t(s); /* this is the index in the cache where the string is found */
    return clone_from_cache ? string_clone(DYNAMIC_ARRAY_VALUES(cache)[length]) : DYNAMIC_ARRAY_VALUES(cache)[length];
  }
  return str;
}

struct object *unmarshal_symbol(struct object *s, struct object *cache) {
  unsigned char t;
  struct object *symbol_name, *package_name, *package;
  s = byte_stream_lift(s);
  OT2("unmarshal_symbol", 0, s, type_file, type_enumerator);
  t = byte_stream_read_byte(s);
  if (t != marshaled_type_symbol && t != marshaled_type_uninterned_symbol) {
    printf("BC: unmarshal expected symbol or uninterned symbol type, but was %d.", t);
    PRINT_STACK_TRACE_AND_QUIT();
  }
  if (t == marshaled_type_symbol) {
    package_name = unmarshal_string(s, 0, cache, 0); /* clone_from_cache is 0 because modifying symbol names is not allowed */
    symbol_name = unmarshal_string(s, 0, cache, 0);
    package = find_package(package_name);
    if (package == NIL) {
      printf("Unmarshaled symbol had a package, that didn't exist.\n");
      print(symbol_name);
      print(package_name);
      exit(1);
    }
    return intern(symbol_name, package == NIL ? GIS_PACKAGE : package);
  } else {
    symbol_name = unmarshal_string(s, 0, cache, 0);
    return symbol(symbol_name);
  }
}

struct object *unmarshal_cons(struct object *s, char includes_header, struct object *cache) {
  unsigned char t;
  struct object *car, *cdr;
  s = byte_stream_lift(s);
  if (includes_header) {
    t = byte_stream_read_byte(s);
    if (t != marshaled_type_cons) {
      printf("BC: unmarshaling cons expected cons type, but was %d.", t);
      PRINT_STACK_TRACE_AND_QUIT();
    }
  }
  car = unmarshal(s, cache);
  cdr = unmarshal(s, cache);
  return cons(car, cdr);
}

struct object *unmarshal_dynamic_byte_array(struct object *s, char includes_header) {
  unsigned char t;
  ufixnum_t length;
  struct object *ret;
  s = byte_stream_lift(s);
  if (includes_header) {
    t = byte_stream_read_byte(s);
    if (t != marshaled_type_dynamic_byte_array) {
      printf(
          "BC: unmarshaling dynamic-byte-array expected dynamic-byte-array "
          "type, "
          "but was %d.",
          t);
      PRINT_STACK_TRACE_AND_QUIT();
    }
  }
  length = unmarshal_ufixnum_t(s);
  ret = byte_stream_read(s, length);
  if (DYNAMIC_BYTE_ARRAY_LENGTH(ret) != length) {
    printf("Failed to unmarshal dynamic byte array. There were not enough bytes in the input stream (maybe not all bytes of the DBA were written?).\n");
    printf("Asked for %d bytes, but only found %d bytes.\n", (int) length, (int) DYNAMIC_BYTE_ARRAY_LENGTH(ret));
    exit(1);
  }
  return ret;
}

struct object *unmarshal_dynamic_array(struct object *s, char includes_header, struct object *cache) {
  unsigned char t;
  struct object *darr;
  ufixnum_t length;
  s = byte_stream_lift(s);
  if (includes_header) {
    t = byte_stream_read_byte(s);
    if (t != marshaled_type_dynamic_array) {
      printf(
          "BC: unmarshal expected dynamic-array type, but was "
          "%d.",
          t);
      PRINT_STACK_TRACE_AND_QUIT();
    }
  }
  length = unmarshal_ufixnum_t(s);
  darr = dynamic_array(length);
  /* unmarshal all items: */
  while (length-- > 0) dynamic_array_push(darr, unmarshal(s, cache));
  return darr;
}

/* if given an existing dynamic array, it will push all items to that. otherwise makes a new one. */
struct object *unmarshal_dynamic_string_array(struct object *s, char includes_header, struct object *cache) {
  unsigned char t;
  ufixnum_t length;
  s = byte_stream_lift(s);
  if (includes_header) {
    t = byte_stream_read_byte(s);
    if (t != marshaled_type_dynamic_string_array) {
      printf(
          "BC: unmarshal expected dynamic-string-array type, but was "
          "%d.",
          t);
      PRINT_STACK_TRACE_AND_QUIT();
    }
  }
  length = unmarshal_ufixnum_t(s);
  while (length-- > 0) dynamic_array_push(cache, unmarshal_string(s, 0, NULL, 0));
  return cache;
}

struct object *unmarshal_vec2(struct object *s, char includes_header) {
  unsigned char t;
  flonum_t x, y;
  s = byte_stream_lift(s);
  if (includes_header) {
    t = byte_stream_read_byte(s);
    if (t != marshaled_type_vec2) {
      printf("BC: unmarshal expected vec2 type, but was %d.",
             t);
      PRINT_STACK_TRACE_AND_QUIT();
    }
  }
  x = unmarshal_float_t(s);
  y = unmarshal_float_t(s);
  return vec2(x, y);
}

struct object *unmarshal_function(struct object *s, char includes_header, struct object *cache) {
  unsigned char t;
  struct object *constants, *code, *f;
  ufixnum_t stack_size;
  s = byte_stream_lift(s);

  if (includes_header) {
    t = byte_stream_read_byte(s);
    if (t != marshaled_type_function) {
      printf("BC: unmarshaling function expected function type, but was %d.",
             t);
      PRINT_STACK_TRACE_AND_QUIT();
    }
  }

  constants = unmarshal_dynamic_array(s, 0, cache);
  stack_size = unmarshal_ufixnum_t(s);
  code = unmarshal_dynamic_byte_array(s, 0);
  f = function(constants, code, stack_size);
  if (unmarshal_ufixnum_t(s) > 0) {
    FUNCTION_NAME(f) = unmarshal_symbol(s, cache);
  }
  FUNCTION_NARGS(f) = unmarshal_ufixnum_t(s);
  FUNCTION_ACCEPTS_ALL(f) = unmarshal_ufixnum_t(s);
  return f;
}

struct object *unmarshal_nil(struct object *s) {
  unsigned char t;
  s = byte_stream_lift(s);
  t = byte_stream_read_byte(s);
  if (t != marshaled_type_nil) {
    printf("BC: unmarshaling nil expected nil type, but was %d.", t);
    PRINT_STACK_TRACE_AND_QUIT();
  }
  return NIL;
}

struct object *unmarshal(struct object *s, struct object *cache) {
  enum marshaled_type t;

  s = byte_stream_lift(s);
  t = byte_stream_peek_byte(s);

  switch (t) {
    case marshaled_type_dynamic_byte_array:
      return unmarshal_dynamic_byte_array(s, 1);
    case marshaled_type_dynamic_array:
      return unmarshal_dynamic_array(s, 1, cache);
    case marshaled_type_cons:
      return unmarshal_cons(s, 1, cache);
    case marshaled_type_nil:
      return unmarshal_nil(s);
    case marshaled_type_integer:
    case marshaled_type_negative_integer:
      return unmarshal_integer(s);
    case marshaled_type_float:
    case marshaled_type_negative_float:
      return unmarshal_float(s);
    case marshaled_type_string:
      return unmarshal_string(s, 1, cache, 1);
    case marshaled_type_vec2:
      return unmarshal_vec2(s, 1);
    case marshaled_type_symbol:
      return unmarshal_symbol(s, cache);
    case marshaled_type_function:
      return unmarshal_function(s, 1, cache);
    default:
      printf("BC: cannot unmarshal marshaled type %d.", t);
      exit(1);
      return NIL;
  }
}

/*===============================*
 *===============================*
 * Bytecode File Formatting      *
 *===============================*
 *===============================*/
struct object *make_bytecode_file_header() {
  struct object *ba = dynamic_byte_array(10);
  dynamic_byte_array_push_char(ba, 'b');
  dynamic_byte_array_push_char(ba, 'u');
  dynamic_byte_array_push_char(ba, 'g');
  marshal_ufixnum_t(BC_VERSION, ba, 0);
  return ba;
}

/**
 * Writes bytecode to a file
 * @param bc the bytecode to write
 */
 void write_bytecode_file(struct object *file, struct object *bc) {
  struct object *cache, *ba;
  ufixnum_t user_cache_start_index;
  OT("write_bytecode_file", 0, file, type_file);
  OT("write_bytecode_file", 1, bc, type_function);
  write_file(file, make_bytecode_file_header());
  cache = string_marshal_cache_get_default();
  user_cache_start_index = DYNAMIC_ARRAY_LENGTH(cache);
  ba = marshal_function(bc, NULL, 0, cache);
  write_file(file, marshal_dynamic_string_array(cache, NULL, 0, NULL, user_cache_start_index));
  write_file(file, ba);
}

struct object *make_image_header() {
  struct object *ba = dynamic_byte_array(10);
  dynamic_byte_array_push_char(ba, 'b');
  dynamic_byte_array_push_char(ba, 'u');
  dynamic_byte_array_push_char(ba, 'g');
  dynamic_byte_array_push_char(ba, 'i');
  marshal_ufixnum_t(IMAGE_VERSION, ba, 0);
  return ba;
}

/**
 * Writes image to a file
 * 
 * String cache list, then:
 * There should be a package list:
 * [package, package, package]
 *  name - string
 *  symbols - list of ufixnum
 * [type, type, type] - gis->types
 *  name - string
 *  is_builtin - string
 *  struct_field_names - only if not builtin - string list
 *  struct_field_types - only if not builtin - ufixnum_t list reference to types index
 * There should be a symbol list:
 * [symbol, symbol]
 *  name - string
 *  has home package - byte
 *  home package - ufixnum
 *  is value set - byte 
 *  value slot - object (present if value is set)
 *  is function set - byte
 *  function slot - object (present if value is set)
 *  is type set- byte
 *  type - object
 * 
 * Each package should be marshaled with a list of ufixnums that says which symbols.
 * 
 * Build symbol list in memory, write package list to the file.
 * Then write the symbol list to the file.
 * 
 */
 void write_image(struct object *file) {
  struct object *cache, *symbol_cache, *package_cursor, *ba;
  ufixnum_t user_cache_start_index, i;

  OT("write_image", 0, file, type_file);

  ba = dynamic_byte_array(1024);

  write_file(file, make_image_header());
  cache = string_marshal_cache_get_default();
  user_cache_start_index = DYNAMIC_ARRAY_LENGTH(cache);

  symbol_cache = NIL;

  /* write string cache */
  write_file(file, marshal_dynamic_string_array(cache, NULL, 0, NULL, user_cache_start_index));

  /* write list of packages */
  package_cursor = symbol_get_value(gis->impl_packages_sym);
  marshal_ufixnum_t(count(package_cursor), ba, 0);
  while (package_cursor != NIL) {
    marshal_image_package(CONS_CAR(package_cursor), ba, cache, symbol_cache);
    package_cursor = CONS_CDR(package_cursor);
  }

  /* write list of symbols */
  marshal_ufixnum_t(DYNAMIC_ARRAY_LENGTH(symbol_cache), ba, 0);
  for (i = 0; i < DYNAMIC_ARRAY_LENGTH(symbol_cache); ++i) {
    marshal_image_symbol(dynamic_array_get_ufixnum_t(symbol_cache, i), ba, cache);
  }

  write_file(file, ba);
}

/**
 * Read 
 * 
 * 1. Load the types, and add them to the gis->types list
 * 2. 
 * 
 */
void load_image(struct object *file) {
  return;
}

struct object *marshal_image_symbol(struct object *sym, struct object *ba, struct object *string_cache) {
  OT("marshal_image_symbol", 0, sym, type_symbol);
  if (ba == NULL)
    ba = dynamic_byte_array(10);

  marshal_string(SYMBOL_NAME(sym), ba, 0, string_cache);
  marshal_ufixnum_t(SYMBOL_PACKAGE(sym) != NIL, ba, 0);
  if (SYMBOL_PACKAGE(sym) != NIL) {
    /* TODO look up package's index */
    marshal_ufixnum_t(0, ba, 0);
  }

  marshal_ufixnum_t(SYMBOL_VALUE_IS_SET(sym), ba, 0);
  if (SYMBOL_VALUE_IS_SET(sym)) {
    marshal(SYMBOL_VALUE(sym), ba, string_cache);
  }

  marshal_ufixnum_t(SYMBOL_FUNCTION_IS_SET(sym), ba, 0);
  if (SYMBOL_FUNCTION_IS_SET(sym)) {
    marshal(SYMBOL_FUNCTION(sym), ba, string_cache);
    /* TODO: what happens with FFI? What if the function is a foreign function? Should those be skipped? */
  }

  /* Saving types is complicated -- they can be ffi_type* -- might need a separate cache for those
  marshal_ufixnum_t(SYMBOL_TYPE_IS_SET(sym), ba, 0);
  if (SYMBOL_TYPE_IS_SET(sym)) {
    marshal(SYMBOL_TYPE(sym), ba, string_cache);
  }
  */

  return ba;
}

struct object *marshal_image_package(struct object *pack, struct object *ba, struct object *string_cache, struct object *symbol_cache) {
  struct object *symbol_indices, *cursor;
  ufixnum_t i;

  OT("marshal_image_package", 0, pack, type_package);
  if (ba == NULL)
    ba = dynamic_byte_array(10);
  marshal_string(PACKAGE_NAME(pack), ba, 0, string_cache);

  symbol_indices = dynamic_array(100);

  /* look up index in symbol_cache */
  cursor = PACKAGE_SYMBOLS(pack);
  while (cursor != NIL) {
    for (i = 0; i < DYNAMIC_ARRAY_LENGTH(symbol_cache); ++i) {
      if (CONS_CAR(cursor) == DYNAMIC_ARRAY_VALUES(symbol_cache)[i]) {
        break;
      }
    }
    if (DYNAMIC_ARRAY_LENGTH(symbol_cache) == i) {
      printf("Attempted to look up symbol in symbol cache, but couldn't find it.\n");
      PRINT_STACK_TRACE_AND_QUIT();
    }
    dynamic_array_push(symbol_indices, ufixnum(i));
    cursor = CONS_CDR(cursor);
  }

  /* how many symbols are in the package */
  marshal_ufixnum_t(DYNAMIC_ARRAY_LENGTH(symbol_indices), ba, 0);
  /* the index in the symbol cache of each symbol */
  for (i = 0; i < DYNAMIC_ARRAY_LENGTH(symbol_indices); ++i) {
    marshal_ufixnum_t(i, ba, 0);
  }

  /* TODO: free symbol_indices */

  return ba;
}

struct object *read_bytecode_file(struct object *s) {
  struct object *bc, *cache;
  ufixnum_t version;

  s = byte_stream_lift(s);

  if (byte_stream_read_byte(s) != 'b' || byte_stream_read_byte(s) != 'u' ||
      byte_stream_read_byte(s) != 'g') {
    printf("BC: Invalid magic string\n");
    PRINT_STACK_TRACE_AND_QUIT();
  }

  version = unmarshal_ufixnum_t(s);
  if (version != BC_VERSION) {
    printf(
        "BC: Version mismatch (this interpreter has version %d, the file has "
        "version %u).\n",
        BC_VERSION, (unsigned int)version);
    PRINT_STACK_TRACE_AND_QUIT();
  }

  /* load cache with defaults, then fill with additional from file */
  cache = string_marshal_cache_get_default();
  unmarshal_dynamic_string_array(s, 0, cache);

  bc = unmarshal_function(s, 0, cache);

  if (type_of(bc) != gis->function_type) {
    printf(
        "BC: function file requires a marshalled function object immediately "
        "after the header, but found a %s.\n",
        type_name_of_cstr(bc));
    PRINT_STACK_TRACE_AND_QUIT();
  }
  return bc;
}

/*===============================*
 *===============================*
 * Byte Streams (for marshaling) *
 *===============================*
 *===============================*/

/**
 * Idempotently lift the value into a stream.
 */
struct object *byte_stream_lift(struct object *e) {
  if (type_of(e) == gis->dynamic_byte_array_type ||
      type_of(e) == gis->string_type) {
    return enumerator(e);
  } else if (type_of(e) == gis->file_type ||
             type_of(e) == gis->enumerator_type) {
    return e;
  } else {
    printf("BC: attempted to lift unsupported type %s into a byte-stream",
           type_name_of_cstr(e));
    PRINT_STACK_TRACE_AND_QUIT();
  }
}

/* TODO: limit n so that it cannot read more than the file */
struct object *byte_stream_do_read(struct object *e, fixnum_t n, char peek) {
  struct object *ret, *t;
  fixnum_t i;
  int c;

  OT2("byte_stream_do_read", 0, e, type_enumerator, type_file);

  ret = dynamic_byte_array(n);

  if (type_of(e) == gis->file_type) {
    /* TODO there is a bug here somewhere... i can read bytecode files fine when I read them all at once, then make a string stream from them
       but if i use a file directly, bad things happen (runs out of characters from the stream) */

    if (peek) {
      for (i = 0; i < n; ++i) {
        c = fgetc(FILE_FP(e));
        if (c == EOF) {
          n = i;
          break;
        }
        dynamic_byte_array_push_char(ret, c);
      }

      for (i = DYNAMIC_BYTE_ARRAY_LENGTH(ret) - 1; i >= 0; --i) {
        c = dynamic_byte_array_get(ret, i);
        ungetc(c, FILE_FP(e));
      }
    } else {
      /* fread and fseek do not play well with all file handles (e.g. stdin stdout) when doing peek (fseek).
         so reads can be done with fread, peeks with fgetc. */
      n = fread(DYNAMIC_BYTE_ARRAY_BYTES(ret), sizeof(char), n, FILE_FP(e));
    }
  } else {
    t = type_of(ENUMERATOR_SOURCE(e));
    if (t == gis->dynamic_byte_array_type || t == gis->string_type) {
        memcpy(DYNAMIC_BYTE_ARRAY_BYTES(ret),
               &DYNAMIC_BYTE_ARRAY_BYTES(
                   ENUMERATOR_SOURCE(e))[ENUMERATOR_INDEX(e)],
               sizeof(char) * n);
        if (!peek) {
          ENUMERATOR_INDEX(e) += n;
        }
    } else {
        printf("BC: byte stream get is not implemented for type %s.",
               type_name_of_cstr(e));
        PRINT_STACK_TRACE_AND_QUIT();
    }
  }
  DYNAMIC_ARRAY_LENGTH(ret) = n;
  return ret;
}

char byte_stream_has(struct object *e) {
  char c;
  struct object *t;
  OT2("byte_steam_has", 0, e, type_enumerator, type_file);
  if (type_of(e) == gis->file_type) {
    c = fgetc(FILE_FP(e));
    ungetc(c, FILE_FP(e));
    return c != EOF;
  } else {
    t = type_of(ENUMERATOR_SOURCE(e));
    if (t == gis->dynamic_byte_array_type || t == gis->string_type) {
        return ENUMERATOR_INDEX(e) < DYNAMIC_BYTE_ARRAY_LENGTH(ENUMERATOR_SOURCE(e));
    } else {
        print(ENUMERATOR_SOURCE(e));
        printf("BC: byte_stream_has is not implemented for type %s.\n",
               type_name_of_cstr(ENUMERATOR_SOURCE(e)));
        PRINT_STACK_TRACE_AND_QUIT();
    }
  }
  return c;
}

char byte_stream_do_read_byte(struct object *e, char peek) {
  struct object *t;
  char c;
  OT2("byte_steam_get_char", 0, e, type_enumerator, type_file);
  if (type_of(e) == gis->file_type) {
    c = fgetc(FILE_FP(e));
    if (peek) ungetc(c, FILE_FP(e));
  } else {
    t = type_of(ENUMERATOR_SOURCE(e));
    if (t == gis->dynamic_byte_array_type || t == gis->string_type) {
        c = DYNAMIC_BYTE_ARRAY_BYTES(ENUMERATOR_SOURCE(e))[ENUMERATOR_INDEX(e)];
        if (!peek) ++ENUMERATOR_INDEX(e);
    } else {
        printf("BC: byte stream get char is not implemented for type %s.",
               type_name_of_cstr(ENUMERATOR_SOURCE(e)));
        PRINT_STACK_TRACE_AND_QUIT();
    }
  }
  return c;
}

struct object *byte_stream_read(struct object *e, fixnum_t n) {
  return byte_stream_do_read(e, n, 0);
}

struct object *byte_stream_peek(struct object *e, fixnum_t n) {
  return byte_stream_do_read(e, n, 1);
}

char byte_stream_read_byte(struct object *e) {
  return byte_stream_do_read_byte(e, 0);
}

char byte_stream_peek_byte(struct object *e) {
  return byte_stream_do_read_byte(e, 1);
}