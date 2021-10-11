/** @file interpreter.c
 *
 * The Bug Bytecode interpreter
 */

#include "bytecode.h"

/**
 * The global interpreter state
 * 
 * Initialized using init()
 */
struct gis *gis;

/*===============================*
 *===============================*
 * Utility Procedures            *
 *===============================*
 *===============================*/
double log2(double n) { return log(n) / log(2); }

char *get_type_name(enum type t) {
  switch (t) {
    case type_cons:
      return "cons";
    case type_fixnum:
      return "fixnum";
    case type_ufixnum:
      return "ufixnum";
    case type_flonum:
      return "flonum";
    case type_symbol:
      return "symbol";
    case type_dynamic_array:
      return "dynamic-array";
    case type_package:
      return "package";
    case type_string:
      return "string";
    case type_bytecode:
      return "bytecode";
    case type_dynamic_byte_array:
      return "dynamic-byte-array";
    case type_file:
      return "file";
    case type_enumerator:
      return "enumerator";
    case type_nil:
      return "nil";
    case type_record:
      return "record";
    case type_function:
      return "function";
  }
  printf("BC: Given type number has no implemented name %d", t);
  exit(1);
}

enum type get_object_type(struct object *o) {
  if (o == NULL) return type_nil;
  if (OBJECT_TYPE(o) & 0x2) return OBJECT_TYPE(o);
  return type_cons;
}

char *get_object_type_name(struct object *o) {
  return get_type_name(get_object_type(o));
}

#ifdef RUN_TIME_CHECKS
void type_check(char *name, unsigned int argument, struct object *o,
                enum type type) {
  if (get_object_type(o) != type) {
    printf(
        "BC: Function \"%s\" was called with an invalid argument "
        "at index %d. Expected type %s, but was %s.\n",
        name, argument, get_type_name(type), get_object_type_name(o));
    exit(1);
  }
}

void type_check_or2(char *name, unsigned int argument, struct object *o,
                enum type type0, enum type type1) {
  if (get_object_type(o) != type0 && get_object_type(o) != type1) {
    printf(
        "BC: Function \"%s\" was called with an invalid argument "
        "at index %d. Expected either %s or %s, but was %s.\n",
        name, argument, get_type_name(type0), get_type_name(type1), get_object_type_name(o));
    exit(1);
  }
}

/**
 * Converts a bug string to a c string
 */
char *bstring_to_cstring(struct object *str) {
  fixnum_t length;
  char *buf;
  TC2("bstring_to_cstring", 0, str, type_string, type_dynamic_byte_array);
  length = STRING_LENGTH(str);
  buf = malloc(sizeof(char) * (length + 1));
  memcpy(buf, STRING_CONTENTS(str), length);
  buf[length] = '\0';
  return buf;
}

/**
 * Counts the number of items in a cons list
 */
fixnum_t count(struct object *list) {
  fixnum_t count = 0;
  while (list != 0) {
    ++count;
    list = list->w1.cdr;
  }
  return count;
}

/**
 * Checks the number of items on the stack (for bytecode interpreter)
 */
void stack_check(char *name, int n) {
  unsigned int stack_count = count(gis->stack);
  if (stack_count < n) {
    printf(
        "BC: Operation \"%s\" was called when the stack had too "
        "few items. Expected %d items on the stack, but were %d.\n",
        name, n, stack_count);
    exit(1);
  }
}
#endif

/*
 * Value constructors
 */
struct object *object(enum type type) {
  struct object *o = malloc(sizeof(struct object));
  if (o == NULL) return NULL;
  OBJECT_TYPE(o) = type;
  return o;
}

struct object *fixnum(fixnum_t fixnum) {
  struct object *o = object(type_fixnum);
  NC(o, "Failed to allocate fixnum object.");
  o->w1.value.fixnum = fixnum;
  return o;
}

struct object *ufixnum(ufixnum_t ufixnum) {
  struct object *o = object(type_ufixnum);
  NC(o, "Failed to allocate ufixnum object.");
  o->w1.value.ufixnum = ufixnum;
  return o;
}

struct object *flonum(flonum_t flo) {
  struct object *o = object(type_flonum);
  NC(o, "Failed to allocate flonum object.");
  o->w1.value.flonum = flo;
  return o;
}

struct object *function(struct object *name, struct object *bytecode) {
  struct object *o;
  TC("function", 0, name, type_symbol);
  TC("function", 1, bytecode, type_bytecode);
  o = object(type_function);
  NC(o, "Failed to allocate function object.");
  FUNCTION_NAME(o) = name;
  FUNCTION_BYTECODE(o) = bytecode;
  return o;
}

struct object *dynamic_array(fixnum_t initial_capacity) {
  struct object *o = object(type_dynamic_array);
  NC(o, "Failed to allocate dynamic-array object.");
  o->w1.value.dynamic_array = malloc(sizeof(struct dynamic_array));
  NC(o->w1.value.dynamic_array, "Failed to allocate dynamic-array.");
  DYNAMIC_ARRAY_CAPACITY(o) = initial_capacity < 0 ? DEFAULT_INITIAL_CAPACITY : initial_capacity;
  o->w1.value.dynamic_array->values = malloc(initial_capacity * sizeof(struct object));
  NC(DYNAMIC_ARRAY_VALUES(o), "Failed to allocate dynamic-array values array.");
  DYNAMIC_ARRAY_LENGTH(o) = 0;
  return o;
}

struct object *dynamic_byte_array(ufixnum_t initial_capacity) {
  struct object *o;
  o = object(type_dynamic_byte_array);
  NC(o, "Failed to allocate dynamic-byte-array object.");
  o->w1.value.dynamic_byte_array = malloc(sizeof(struct dynamic_byte_array));
  NC(o->w1.value.dynamic_byte_array, "Failed to allocate dynamic-byte-array.");
  DYNAMIC_BYTE_ARRAY_CAPACITY(o) = initial_capacity <= 0 ? DEFAULT_INITIAL_CAPACITY : initial_capacity;
  DYNAMIC_BYTE_ARRAY_BYTES(o) = malloc(DYNAMIC_BYTE_ARRAY_CAPACITY(o) * sizeof(char));
  NC(DYNAMIC_BYTE_ARRAY_BYTES(o), "Failed to allocate dynamic-byte-array bytes.");
  DYNAMIC_BYTE_ARRAY_LENGTH(o) = 0;
  return o;
}

struct object *bytecode(struct object *constants, struct object *code) {
  struct object *o;
  TC("bytecode", 0, constants, type_dynamic_array);
  TC("bytecode", 1, code, type_dynamic_byte_array);
  o = object(type_bytecode);
  NC(o, "Failed to allocate bytecode object.");
  o->w1.value.bytecode = malloc(sizeof(struct bytecode));
  NC(o->w1.value.bytecode, "Failed to allocate bytecode.");
  BYTECODE_CODE(o) = code;
  BYTECODE_CONSTANTS(o) = constants;
  return o;
}

struct object *string(char *contents) {
  struct object *o;
  ufixnum_t length = strlen(contents);
  o = dynamic_byte_array(length);
  OBJECT_TYPE(o) = type_string;
  memcpy(DYNAMIC_BYTE_ARRAY_BYTES(o), contents, length);
  DYNAMIC_BYTE_ARRAY_LENGTH(o) = length;
  return o;
}

struct object *enumerator(struct object *source) {
  struct object *o;
  o = object(type_enumerator);
  NC(o, "Failed to allocate enumerator object.");
  o->w1.value.enumerator = malloc(sizeof(struct enumerator));
  NC(o->w1.value.enumerator, "Failed to allocate enumerator value.");
  ENUMERATOR_SOURCE(o) = source;
  ENUMERATOR_VALUE(o) = NULL;
  ENUMERATOR_INDEX(o) = 0;
  return o;
}

struct object *package(struct object *name, struct object *packages) {
  struct object *o = object(type_package);
  NC(o, "Failed to allocate package object.");
  o->w1.value.package = malloc(sizeof(struct package));
  NC(o->w1.value.package, "Failed to allocate package object.");
  PACKAGE_NAME(o) = name;
  PACKAGE_SYMBOLS(o) = NULL;
  PACKAGE_PACKAGES(o) = packages;
  return o;
}

struct object *cons(struct object *car, struct object *cdr) {
  struct object *o = object(type_cons);
  o->w0.car = car;
  o->w1.cdr = cdr;
  return o;
}

/* Creates a new symbol. Does NOT intern or add to the current package. */
struct object *symbol(struct object *name) {
  struct object *o = object(type_symbol);
  NC(o, "Failed to allocate symbol object.");
  o->w1.value.symbol = malloc(sizeof(struct symbol));
  NC(o->w1.value.symbol, "Failed to allocate symbol.");
  SYMBOL_NAME(o) = name;
  SYMBOL_VALUES(o) = NULL;
  SYMBOL_PACKAGE(o) = NULL;
  return o;
}

struct object *file_stdin() {
  struct object *o;
  o = object(type_file);
  o->w1.value.file = malloc(sizeof(struct file));
  NC(o->w1.value.file, "Failed to allocate file value.");
  FILE_FP(o) = stdin;
  FILE_MODE(o) = NULL;
  FILE_PATH(o) = NULL;
  return o;
}

struct object *file_stdout() {
  struct object *o;
  o = object(type_file);
  o->w1.value.file = malloc(sizeof(struct file));
  NC(o->w1.value.file, "Failed to allocate file value.");
  FILE_FP(o) = stdout;
  FILE_MODE(o) = NULL;
  FILE_PATH(o) = NULL;
  return o;
}

struct object *open_file(struct object *path, struct object *mode) {
  FILE *fp;
  struct object *o;
  char *path_cs, *mode_cs;

  path_cs = bstring_to_cstring(path);
  mode_cs = bstring_to_cstring(mode);

  fp = fopen(path_cs, mode_cs);

  if (fp == NULL) {
    printf("BC: Failed to open file at path %s with mode %s.\n", path_cs,
           mode_cs);
    free(path_cs);
    free(mode_cs);
    exit(1);
  }

  o = object(type_file);

  o->w1.value.file = malloc(sizeof(struct file));
  NC(o->w1.value.file, "Failed to allocate file value.");

  FILE_FP(o) = fp;
  FILE_MODE(o) = string(mode_cs);
  FILE_PATH(o) = string(path_cs);

  free(path_cs);
  free(mode_cs);

  return o;
}

struct object *close_file(struct object *file) {
  TC("close_file", 0, file, type_file);
  fclose(FILE_FP(file)); /* TODO: handle failure */
  return NULL;
}

/*
 * Dynamic Array
 */
struct object *dynamic_array_get_fixnum_t(struct object *da, fixnum_t index) {
  TC("dynamic_array_get", 0, da, type_dynamic_array);
  return DYNAMIC_ARRAY_VALUES(da)[index];
}
struct object *dynamic_array_get(struct object *da, struct object *index) {
  TC("dynamic_array_get", 0, da, type_dynamic_array);
  TC("dynamic_array_get", 1, index, type_fixnum);
  return DYNAMIC_ARRAY_VALUES(da)[FIXNUM_VALUE(index)];
}
struct object *dynamic_array_set(struct object *da, struct object *index, struct object *value) {
  TC("dynamic_array_set", 0, da, type_dynamic_array);
  TC("dynamic_array_set", 1, index, type_fixnum);
  TC("dynamic_array_set", 2, value, type_fixnum);
  DYNAMIC_ARRAY_VALUES(da)[FIXNUM_VALUE(index)] = value;
  return NULL;
}
struct object *dynamic_array_length(struct object *da) {
  TC("dynamic_array_length", 0, da, type_dynamic_array);
  return fixnum(DYNAMIC_ARRAY_LENGTH(da));
}
struct object *dynamic_array_push(struct object *da, struct object *value) {
  TC("dynamic_array_push", 0, da, type_dynamic_array);
  if (DYNAMIC_ARRAY_LENGTH(da) >= DYNAMIC_ARRAY_CAPACITY(da)) {
    DYNAMIC_ARRAY_CAPACITY(da) = (DYNAMIC_ARRAY_LENGTH(da) + 1) * 3/2.0;
    DYNAMIC_ARRAY_VALUES(da) = realloc(DYNAMIC_ARRAY_VALUES(da), DYNAMIC_ARRAY_CAPACITY(da) * sizeof(char));
    if (DYNAMIC_ARRAY_VALUES(da) == NULL) {
      printf("BC: Failed to realloc dynamic-array.");
      exit(1);
    }
  }
  DYNAMIC_ARRAY_VALUES(da)[DYNAMIC_ARRAY_LENGTH(da)++] = value;
  return NULL;
}
struct object *dynamic_array_pop(struct object *da) {
  TC("dynamic_array_pop", 0, da, type_dynamic_array);
  if (DYNAMIC_ARRAY_LENGTH(da) < 1) {
    printf("BC: Attempted to pop an empty dynamic-array");
    exit(1);
  }
  DYNAMIC_ARRAY_LENGTH(da)--;
  return NULL;
}
struct object *dynamic_array_concat(struct object *da0, struct object *da1) {
  struct object *da2; 

  TC("dynamic_array_concat", 0, da0, type_dynamic_array);
  TC("dynamic_array_concat", 1, da1, type_dynamic_array);

  da2 = dynamic_array(DYNAMIC_ARRAY_CAPACITY(da0) + DYNAMIC_ARRAY_CAPACITY(da1));
  memcpy(DYNAMIC_ARRAY_VALUES(da2), DYNAMIC_ARRAY_VALUES(da0), DYNAMIC_ARRAY_LENGTH(da0) * sizeof(char));
  memcpy(&DYNAMIC_ARRAY_VALUES(da2)[DYNAMIC_ARRAY_LENGTH(da0)], DYNAMIC_ARRAY_VALUES(da1), DYNAMIC_ARRAY_LENGTH(da1) * sizeof(char));
  return da2;
}

/*
 * Dynamic Byte Array
 */
char dynamic_byte_array_get(struct object *dba, fixnum_t index) {
  TC2("dynamic_byte_array_get", 0, dba, type_dynamic_byte_array, type_string);
  return DYNAMIC_BYTE_ARRAY_BYTES(dba)[index];
}
struct object *dynamic_byte_array_set(struct object *dba, struct object *index, struct object *value) {
  TC2("dynamic_byte_array_set", 0, dba, type_dynamic_byte_array, type_string);
  TC("dynamic_byte_array_set", 1, index, type_fixnum);
  TC("dynamic_byte_array_set", 2, value, type_fixnum);
  DYNAMIC_BYTE_ARRAY_BYTES(dba)[FIXNUM_VALUE(index)] = FIXNUM_VALUE(value);
  return NULL;
}
struct object *dynamic_byte_array_length(struct object *dba) {
  TC2("dynamic_byte_array_length", 0, dba, type_dynamic_byte_array, type_string);
  return fixnum(DYNAMIC_BYTE_ARRAY_LENGTH(dba));
}
void dynamic_byte_array_ensure_capacity(struct object *dba) {
  TC2("dynamic_byte_array_ensure_capacity", 0, dba, type_dynamic_byte_array, type_string);
  if (DYNAMIC_BYTE_ARRAY_LENGTH(dba) >= DYNAMIC_BYTE_ARRAY_CAPACITY(dba)) {
    DYNAMIC_BYTE_ARRAY_CAPACITY(dba) = (DYNAMIC_BYTE_ARRAY_LENGTH(dba) + 1) * 3/2.0;
    DYNAMIC_BYTE_ARRAY_BYTES(dba) = realloc(DYNAMIC_BYTE_ARRAY_BYTES(dba), DYNAMIC_BYTE_ARRAY_CAPACITY(dba) * sizeof(char));
    if (DYNAMIC_BYTE_ARRAY_BYTES(dba) == NULL) {
      printf("BC: Failed to realloc dynamic-byte-array.");
      exit(1);
    }
  }
}
struct object *dynamic_byte_array_push(struct object *dba, struct object *value) {
  TC2("dynamic_byte_array_push", 0, dba, type_dynamic_byte_array, type_string);
  TC("dynamic_byte_array_push", 1, value, type_fixnum);
  dynamic_byte_array_ensure_capacity(dba);
  DYNAMIC_BYTE_ARRAY_BYTES(dba)[DYNAMIC_BYTE_ARRAY_LENGTH(dba)++] = FIXNUM_VALUE(value);
  return NULL;
}
/** pushes a single byte (to avoid wrapping in a fixnum object) */
struct object *dynamic_byte_array_push_char(struct object *dba, char x) {
  dynamic_byte_array_ensure_capacity(dba);
  DYNAMIC_BYTE_ARRAY_BYTES(dba)[DYNAMIC_BYTE_ARRAY_LENGTH(dba)++] = x;
  return NULL;
}
struct object *dynamic_byte_array_insert_char(struct object *dba, ufixnum_t i, char x) {
  ufixnum_t j;

  dynamic_byte_array_ensure_capacity(dba);
  for (j = DYNAMIC_BYTE_ARRAY_LENGTH(dba); j > i; --j) {
    DYNAMIC_BYTE_ARRAY_BYTES(dba)[j] = DYNAMIC_BYTE_ARRAY_BYTES(dba)[j-1];
  }
  DYNAMIC_BYTE_ARRAY_BYTES(dba)[i] = x;
  ++DYNAMIC_BYTE_ARRAY_LENGTH(dba);
  return NULL;
}
struct object *dynamic_byte_array_pop(struct object *dba) {
  TC2("dynamic_byte_array_pop", 0, dba, type_dynamic_byte_array, type_string);
  if (DYNAMIC_BYTE_ARRAY_LENGTH(dba) < 1) {
    printf("BC: Attempted to pop an empty dynamic-byte-array");
    exit(1);
  }
  DYNAMIC_BYTE_ARRAY_LENGTH(dba)--;
  return NULL;
}
struct object *dynamic_byte_array_concat(struct object *dba0, struct object *dba1) {
  struct object *dba2; 

  TC2("dynamic_byte_array_concat", 0, dba0, type_dynamic_byte_array, type_string);
  TC2("dynamic_byte_array_concat", 1, dba1, type_dynamic_byte_array, type_string);

  dba2 = dynamic_byte_array(DYNAMIC_BYTE_ARRAY_CAPACITY(dba0) + DYNAMIC_BYTE_ARRAY_CAPACITY(dba1));
  DYNAMIC_BYTE_ARRAY_LENGTH(dba2) = DYNAMIC_BYTE_ARRAY_LENGTH(dba0) + DYNAMIC_BYTE_ARRAY_LENGTH(dba1);
  memcpy(DYNAMIC_BYTE_ARRAY_BYTES(dba2), DYNAMIC_BYTE_ARRAY_BYTES(dba0), DYNAMIC_BYTE_ARRAY_LENGTH(dba0) * sizeof(char));
  memcpy(&DYNAMIC_BYTE_ARRAY_BYTES(dba2)[DYNAMIC_BYTE_ARRAY_LENGTH(dba0)], DYNAMIC_BYTE_ARRAY_BYTES(dba1), DYNAMIC_BYTE_ARRAY_LENGTH(dba1) * sizeof(char));
  return dba2;
}
struct object *dynamic_byte_array_from_array(ufixnum_t length, unsigned char *arr) {
  struct object *dba;
  dba = dynamic_byte_array(length);
  DYNAMIC_BYTE_ARRAY_LENGTH(dba) = length;
  memcpy(DYNAMIC_BYTE_ARRAY_BYTES(dba), arr, sizeof(unsigned char));
  return dba;
}

/*===============================*
 *===============================*
 * to-string                     *
 *===============================*
 *===============================*/
struct object *do_to_string(struct object *o, char repr);

void reverse_string(struct object *o) {
  ufixnum_t i;
  char temp;

  for (i = 0; i < STRING_LENGTH(o)/2; ++i) {
    temp = STRING_CONTENTS(o)[i];
    STRING_CONTENTS(o)[i] = STRING_CONTENTS(o)[STRING_LENGTH(o)-1-i];
    STRING_CONTENTS(o)[STRING_LENGTH(o)-1-i] = temp;
  }
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
  reverse_string(str);
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
  char byte, nib0, nib1;

  TC("to_string_dynamic_byte_array", 0, dba, type_dynamic_byte_array);

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

struct object *to_string_dynamic_array(struct object *da) {
  struct object *str;
  ufixnum_t i;

  TC("to_string_dynamic_array", 0, da, type_dynamic_array);

  str = string("[");
  for (i = 0; i < DYNAMIC_ARRAY_LENGTH(da); ++i) {
    str = dynamic_byte_array_concat(str, do_to_string(dynamic_array_get_fixnum_t(da, i), 1));
    dynamic_byte_array_push_char(str, ' ');
  }
  if (DYNAMIC_ARRAY_LENGTH(da) > 0)
    dynamic_byte_array_pop(str); /* remove the extra space */
  dynamic_byte_array_push_char(str, ']');
  return str;
}

struct object *do_to_string(struct object *o, char repr) {
  struct object *str;

  switch (get_object_type(o)) {
    case type_cons:
      str = string("(");
      str = dynamic_byte_array_concat(str, do_to_string(CONS_CAR(o), 1));
      str = dynamic_byte_array_concat(str, string(" "));
      str = dynamic_byte_array_concat(str, do_to_string(CONS_CDR(o), 1));
      dynamic_byte_array_push_char(str, ')');
      return str;
    case type_string:
      if (repr) {
        o = dynamic_byte_array_concat(string("\""), o);
        dynamic_byte_array_push_char(o, '\"');
      }
      return o;
    case type_nil:
      return string("nil");
    case type_flonum:
      return to_string_flonum_t(FLONUM_VALUE(o));
    case type_ufixnum:
      return to_string_ufixnum_t(UFIXNUM_VALUE(o));
    case type_fixnum:
      return to_string_fixnum_t(FIXNUM_VALUE(o));
    case type_dynamic_byte_array:
      return to_string_dynamic_byte_array(o);
    case type_dynamic_array:
      return to_string_dynamic_array(o);
    case type_bytecode: /* TODO */
      str = string("#[");
      str = dynamic_byte_array_concat(str, do_to_string(BYTECODE_CONSTANTS(o), 1));
      dynamic_byte_array_push_char(str, ' ');
      str = dynamic_byte_array_concat(str, do_to_string(BYTECODE_CODE(o), 1));
      dynamic_byte_array_push_char(str, ']');
      return str;
    case type_record: /* TODO */
      return string("<record>");
    case type_symbol:
      return SYMBOL_NAME(o);
    default:
      printf("Type doesn't support to-string\n");
      exit(1);
  }
}

struct object *to_string(struct object *o) {
  return do_to_string(o, 0);
}

struct object *to_repr(struct object *o) {
  return do_to_string(o, 1);
}

/*===============================*
 *===============================*
 * Print                         *
 *===============================*
 *===============================*/
void print(struct object *o, char newline) {
  ufixnum_t i;
  o = to_string(o);
  for (i = 0; i < STRING_LENGTH(o); ++i) {
    printf("%c", STRING_CONTENTS(o)[i]);
  }
  if (newline)
    printf("\n");
}

/*===============================*
 *===============================*
 * Equality                      *
 *     Structural equality
 *===============================*
 *===============================*/
char equals(struct object *o0, struct object *o1);

char equals(struct object *o0, struct object *o1) {
  ufixnum_t i;

  if (get_object_type(o0) != get_object_type(o1)) return 0;
  switch (get_object_type(o0)) {
    case type_cons:
      return equals(CONS_CAR(o0), CONS_CAR(o1)) && equals(CONS_CDR(o0), CONS_CDR(o1));
    case type_string:
    case type_dynamic_byte_array:
      if (DYNAMIC_BYTE_ARRAY_LENGTH(o0) != DYNAMIC_BYTE_ARRAY_LENGTH(o1))
        return 0;
      for (i = 0; i < DYNAMIC_BYTE_ARRAY_LENGTH(o0); ++i)
        if (DYNAMIC_BYTE_ARRAY_BYTES(o0)[i] != DYNAMIC_BYTE_ARRAY_BYTES(o1)[i])
          return 0;
      return 1;
    case type_nil:
      return 1;
    case type_flonum:
      return FLONUM_VALUE(o0) == FLONUM_VALUE(o1);
    case type_ufixnum:
      return UFIXNUM_VALUE(o0) == UFIXNUM_VALUE(o1);
    case type_fixnum:
      return FIXNUM_VALUE(o0) == FIXNUM_VALUE(o1);
    case type_dynamic_array:
      if (DYNAMIC_ARRAY_LENGTH(o0) != DYNAMIC_ARRAY_LENGTH(o1))
        return 0;
      for (i = 0; i < DYNAMIC_ARRAY_LENGTH(o0); ++i)
        if (!equals(DYNAMIC_ARRAY_VALUES(o0)[i], DYNAMIC_ARRAY_VALUES(o1)[i]))
          return 0;
      return 1;
    case type_file:
      return o0 == o1;
    case type_enumerator:
      return equals(ENUMERATOR_SOURCE(o0), ENUMERATOR_SOURCE(o1)) &&
             ENUMERATOR_INDEX(o0) == ENUMERATOR_INDEX(o1);
    case type_package:
      return o0 == o1;
    case type_symbol:
      return o0 == o1;
    case type_bytecode:
      return equals(BYTECODE_CONSTANTS(o0), BYTECODE_CONSTANTS(o1)) &&
             equals(BYTECODE_CODE(o0), BYTECODE_CODE(o1));
    case type_function:
      return equals(FUNCTION_NAME(o0), FUNCTION_NAME(o1)) &&
             equals(FUNCTION_BYTECODE(o0), FUNCTION_BYTECODE(o1));
    case type_record:
      /* TODO */
      return 0;
  }
  printf("Unhandled equals");
  exit(1);
}

/*===============================*
 *===============================*
 * alist                         *
 *===============================*
 *===============================*/
struct object *cons_reverse(struct object *cursor) {
  struct object *ret;
  ret = NULL;
  while (cursor != NULL) {
    ret = cons(CONS_CAR(cursor), ret);
    cursor = CONS_CDR(cursor);
  }
  return ret;
}

struct object *alist_get_slot(struct object *alist, struct object *key) {
  TC2("alist_get", 0, alist, type_cons, type_nil);
  while (alist != NULL) {
    if (equals(CONS_CAR(CONS_CAR(alist)), key))
      return CONS_CAR(alist);
    alist = CONS_CDR(alist);
  }
  return NULL;
}

struct object *alist_get_value(struct object *alist, struct object *key) {
  alist = alist_get_slot(alist, key);
  if (alist == NULL)
    return NULL;
  return CONS_CDR(alist);
}

struct object *alist_extend(struct object *alist, struct object *key, struct object *value) {
  TC2("alist_extend", 0, alist, type_cons, type_nil);
  return cons(cons(key, value), alist);
}


/*===============================*
 *===============================*
 * Symbol Interning and access   *
 *===============================*
 *===============================*/

struct object *symbol_get_slot(struct object *sym, struct object* slot_name) {
  return alist_get_slot(SYMBOL_VALUES(sym), slot_name);
}

void symbol_set(struct object *sym, struct object* slot_name, struct object *value) {
  struct object *slot = symbol_get_slot(sym, slot_name);
  if (slot == NULL) {
    SYMBOL_VALUES(sym) = alist_extend(SYMBOL_VALUES(sym), slot_name, value);
  } else {
    CONS_CDR(slot) = value;
  }
}

struct object *intern(struct object *string, struct gis *g, struct object *package) {
  struct object *cursor, *sym;

  TC("intern", 0, string, type_string);
  TC2("intern", 2, package, type_package, type_nil);

  /* use the global GIS if not given one */
  if (package == NULL)
    package = gis->package;

  cursor = PACKAGE_SYMBOLS(package);
  while (cursor != NULL) {
    sym = CONS_CAR(cursor);
    if (equals(SYMBOL_NAME(sym), string)) return sym;
    cursor = CONS_CDR(cursor);
  }

  /* If no existing symbol was found, create a new one and add it to the current package. */
  sym = symbol(string);
  SYMBOL_PACKAGE(sym) = package; /* set the home package */
  PACKAGE_SYMBOLS(package) = cons(sym, PACKAGE_SYMBOLS(package));
  if (package == g->keyword_package) { /* all symbols in keyword package have
                                          the value of themselves*/
    symbol_set(sym, g->value_keyword, sym);
  }

  return sym;
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
  if (get_object_type(e) == type_dynamic_byte_array ||
      get_object_type(e) == type_string) {
    return enumerator(e);
  } else if (get_object_type(e) == type_file ||
             get_object_type(e) == type_enumerator) {
    return e;
  } else {
    printf("BC: attempted to lift unsupported type %s into a byte-stream",
           get_object_type_name(e));
    exit(1);
  }
}

struct object *byte_stream_do_read(struct object *e, fixnum_t n, char peek) {
  struct object *ret;

  TC2("byte_stream_get", 0, e, type_enumerator, type_file);

  ret = dynamic_byte_array(n);
  DYNAMIC_ARRAY_LENGTH(ret) = n;

  if (get_object_type(e) == type_file) {
    fread(DYNAMIC_BYTE_ARRAY_BYTES(ret), sizeof(char), n, FILE_FP(e));
    if (peek) fseek(FILE_FP(e), -n, SEEK_CUR);
  } else {
    switch (get_object_type(ENUMERATOR_SOURCE(e))) {
      case type_dynamic_byte_array:
      case type_string:
        memcpy(DYNAMIC_BYTE_ARRAY_BYTES(ret),
               &DYNAMIC_BYTE_ARRAY_BYTES(
                   ENUMERATOR_SOURCE(e))[ENUMERATOR_INDEX(e)],
               sizeof(char) * n);
        if (!peek) ENUMERATOR_INDEX(e) += n;
        break;
      default:
        printf("BC: byte stream get is not implemented for type %s.",
               get_object_type_name(e));
        exit(1);
    }
  }
  return ret;
}

char byte_stream_has(struct object *e) {
  char c;
  TC2("byte_steam_has", 0, e, type_enumerator, type_file);
  if (get_object_type(e) == type_file) {
    c = fgetc(FILE_FP(e));
    ungetc(c, FILE_FP(e));
    return c != EOF;
  } else {
    switch (get_object_type(ENUMERATOR_SOURCE(e))) {
      case type_dynamic_byte_array:
      case type_string:
        return ENUMERATOR_INDEX(e) < DYNAMIC_BYTE_ARRAY_LENGTH(ENUMERATOR_SOURCE(e));
      default:
        printf("BC: byte stream has is not implemented for type %s.",
               get_object_type_name(ENUMERATOR_SOURCE(e)));
        exit(1);
    }
  }
  return c;
}

char byte_stream_do_read_byte(struct object *e, char peek) {
  char c;
  TC2("byte_steam_get_char", 0, e, type_enumerator, type_file);
  if (get_object_type(e) == type_file) {
    c = fgetc(FILE_FP(e));
    if (peek) ungetc(c, FILE_FP(e));
  } else {
    switch (get_object_type(ENUMERATOR_SOURCE(e))) {
      case type_dynamic_byte_array:
      case type_string:
        c = DYNAMIC_BYTE_ARRAY_BYTES(ENUMERATOR_SOURCE(e))[ENUMERATOR_INDEX(e)];
        if (!peek) ++ENUMERATOR_INDEX(e);
        break;
      default:
        printf("BC: byte stream get char is not implemented for type %s.",
               get_object_type_name(ENUMERATOR_SOURCE(e)));
        exit(1);
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

/** creates a new global interpreter state with one package (called "lisp")
 *  which is set to the current package.
 */
struct gis *gis_alloc() {
  struct gis *g;

  g = malloc(sizeof(struct gis));
  NC(g, "Failed to allocate global interpreter state.");
  g->stack = NULL;
  g->call_stack = NULL;
  g->sp = ufixnum(0);
  g->lisp_package = package(string("lisp"), NULL);
  g->keyword_package = package(string("keyword"), NULL);
  g->user_package = package(string("user"), cons(g->lisp_package, NULL));
  g->package = g->user_package;
  g->packages = cons(g->user_package, cons(g->lisp_package, cons(g->keyword_package, NULL)));

  /* initialize keywords that are used internally */
  g->value_keyword = intern(string("value"), g, g->keyword_package);
  g->function_keyword = intern(string("function"), g, g->keyword_package);

  return g;
}

void add_package(struct gis *g, struct object *package) {
  g->packages = cons(package, g->packages);
}

struct object *find_package(struct gis *g, struct object *name) {
  struct object *package = g->packages;
  while (package != NULL) {
    if (equals(PACKAGE_NAME(CONS_CAR(package)), name)) {
      return CONS_CAR(package);
    }
  }
  return NULL;
} 


/*===============================*
 *===============================*
 * Marshaling                    *
 *===============================*
 *===============================*/
struct object *marshal(struct object *o);

struct object *marshal_fixnum_t(fixnum_t n) {
  struct object *ba;
  unsigned char byte;

  ba = dynamic_byte_array(4);
  dynamic_byte_array_push_char(ba, marshaled_type_integer);
  dynamic_byte_array_push_char(ba, n < 0 ? 1 : 0);

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
  
  if (include_header) {
    dynamic_byte_array_push_char(ba, marshaled_type_integer);
    dynamic_byte_array_push_char(ba, 0);
  }

  do {
    byte = n & 0x7F;
    if (n > 0x7F) /* flip the continuation bit if there are more bytes */
      byte |= 0x80;
    dynamic_byte_array_push_char(ba, byte);
    n >>= 7;
  } while (n > 0);

  return ba;
}

struct object *marshal_string(struct object *str) {
  struct object *ba;

  TC("marshal_string", 0, str, type_string);

  ba = dynamic_byte_array(1);
  dynamic_byte_array_push_char(ba, marshaled_type_string);
  ba = dynamic_byte_array_concat(ba, marshal_fixnum_t(STRING_LENGTH(str)));
  ba = dynamic_byte_array_concat(ba, str);
  return ba;
}

struct object *marshal_symbol(struct object *sym) {
  struct object *ba;

  TC("marshal_symbol", 0, sym, type_symbol);

  ba = dynamic_byte_array(1);
  dynamic_byte_array_push_char(ba, marshaled_type_symbol);
  ba = dynamic_byte_array_concat(ba, marshal_fixnum_t(STRING_LENGTH(SYMBOL_NAME(sym))));
  ba = dynamic_byte_array_concat(ba, SYMBOL_NAME(sym));
  if (SYMBOL_PACKAGE(sym) != NULL) {
    dynamic_byte_array_push_char(ba, 1);
    ba = dynamic_byte_array_concat(ba, marshal_fixnum_t(STRING_LENGTH(PACKAGE_NAME(SYMBOL_PACKAGE(sym)))));
    ba = dynamic_byte_array_concat(ba, PACKAGE_NAME(SYMBOL_PACKAGE(sym)));
  } else {
    dynamic_byte_array_push_char(ba, 0);
  }
  return ba;
}

struct object *marshal_dynamic_byte_array(struct object *ba0) {
  struct object *ba1;

  TC("marshal_dynamic_byte_array", 0, ba0, type_dynamic_byte_array);

  ba1 = dynamic_byte_array(1);
  dynamic_byte_array_push_char(ba1, marshaled_type_dynamic_byte_array);
  ba1 = dynamic_byte_array_concat(
      ba1, marshal_fixnum_t(DYNAMIC_BYTE_ARRAY_LENGTH(ba0)));
  ba1 = dynamic_byte_array_concat(ba1, ba0);
  return ba1;
}

struct object *marshal_nil() {
  struct object *ba;
  ba = dynamic_byte_array(1);
  dynamic_byte_array_push_char(ba, marshaled_type_nil);
  return ba;
}

struct object *marshal_cons(struct object *o) {
  struct object *ba;

  TC("marshal_cons", 0, o, type_cons);

  ba = dynamic_byte_array(10);
  dynamic_byte_array_push_char(ba, marshaled_type_cons);
  ba = dynamic_byte_array_concat(ba, marshal(CONS_CAR(o)));
  ba = dynamic_byte_array_concat(ba, marshal(CONS_CDR(o)));
  return ba;
}

struct object *marshal_dynamic_array(struct object *arr) {
  struct object *ba;
  fixnum_t arr_length;
  fixnum_t i;
  struct object **c_arr;

  TC("marshal_dynamic_array", 0, arr, type_dynamic_array);

  arr_length = DYNAMIC_ARRAY_LENGTH(arr);
  c_arr = DYNAMIC_ARRAY_VALUES(arr);

  ba = dynamic_byte_array(1);
  dynamic_byte_array_push_char(ba, marshaled_type_dynamic_array);
  ba = dynamic_byte_array_concat(ba, marshal_fixnum_t(arr_length));

  for (i = 0; i < arr_length; ++i) {
    ba = dynamic_byte_array_concat(ba, marshal(c_arr[i]));
  }

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
struct object *marshal_fixnum(struct object *n) {
  TC("marshal_fixnum", 0, n, type_fixnum);
  return marshal_fixnum_t(FIXNUM_VALUE(n));
}

struct object *marshal_ufixnum(struct object *n) {
  TC("marshal_ufixnum", 0, n, type_ufixnum);
  return marshal_ufixnum_t(UFIXNUM_VALUE(n), NULL, 1);
}

struct object *marshal_flonum(struct object *n) {
  struct object *ba;
  ufixnum_t mantissa_fix;
  flonum_t mantissa;
  int exponent;

  TC("marshal_flonum", 0, n, type_flonum);

  ba = dynamic_byte_array(1);
  dynamic_byte_array_push_char(ba, marshaled_type_float);
  dynamic_byte_array_push_char(ba, FLONUM_VALUE(n) < 0 ? 1 : 0);

  /* Inefficient in both time and space but work short term.
   *
   * The idea was for the marshaled format to be as exact as possible
   * (arbitrary precision), while have the types used in the program be inexact
   * (e.g. float, double). I chose to use the base 2 floating point format
   * (mantissa * 2^exponent = value) using arbitrary long mantissas and
   * exponents. This is probably naive and might not work for NaN or Infinity.
   */
  mantissa = frexp(FLONUM_VALUE(n), &exponent);
  /* frexp keeps sign information on the mantissa, we convert it to a unsigned
   * fixnum (hence the abs(...)). */
  /* we already have the sign information in the marshaled flonum */
  mantissa = mantissa < 0 ? -mantissa : mantissa;
  mantissa_fix =
      mantissa *
      pow(2, DBL_MANT_DIG); /* TODO: make it choose between DBL/FLT properly */
  ba = dynamic_byte_array_concat(ba, marshal_ufixnum_t(mantissa_fix, NULL, 1));
  ba = dynamic_byte_array_concat(ba, marshal_fixnum_t((fixnum_t)exponent));

  return ba;
}

/**
 * turns bytecode into a byte-array that can be stored to a file
 */
struct object *marshal_bytecode(struct object *bc) {
  struct object *ba;
  TC("marshal_bytecode", 0, bc, type_bytecode);
  ba = dynamic_byte_array(1);
  dynamic_byte_array_push_char(ba, marshaled_type_bytecode);
  ba = dynamic_byte_array_concat(ba,
                                 marshal_dynamic_array(BYTECODE_CONSTANTS(bc)));
  ba = dynamic_byte_array_concat(ba,
                                 marshal_dynamic_byte_array(BYTECODE_CODE(bc)));
  return ba;
}

/**
 * Takes any object and returns a byte-array containing the binary
 * representation of the object.
 */
struct object *marshal(struct object *o) {
  if (o == NULL) return marshal_nil();
  switch (get_object_type(o)) {
    case type_dynamic_byte_array:
      return marshal_dynamic_byte_array(o);
    case type_dynamic_array:
      return marshal_dynamic_array(o);
    case type_cons:
      return marshal_cons(o);
    case type_fixnum:
      return marshal_fixnum(o);
    case type_ufixnum:
      return marshal_ufixnum(o);
    case type_flonum:
      return marshal_flonum(o);
    case type_string:
      return marshal_string(o);
    case type_symbol:
      return marshal_symbol(o);
    case type_bytecode:
      return marshal_bytecode(o);
    default:
      printf("BC: cannot marshal type %s.\n", get_object_type_name(o));
      return NULL;
  }
}

/*===============================*
 *===============================*
 * Unmarshaling                  *
 *===============================*
 *===============================*/
struct object *unmarshal(struct object *s, struct gis *g);

struct object *unmarshal_integer(struct object *s) {
  unsigned char t, sign, is_flo, is_init_flo;
  ufixnum_t ufix, next_ufix, byte_count,
      byte; /* the byte must be a ufixnum because it is used in operations that
               result in ufixnum */
  fixnum_t fix, next_fix;
  flonum_t flo;

  s = byte_stream_lift(s);

  TC2("unmarshal_integer", 0, s, type_enumerator, type_file);

  t = byte_stream_read_byte(s);
  if (t != marshaled_type_integer) {
    printf(
        "BC: unmarshaling fixnum expected either marshaled_integer type, but "
        "was %d.",
        t);
    exit(1);
  }

  sign = byte_stream_read_byte(s);

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

struct object *unmarshal_float(struct object *s) {
  unsigned char t, sign;
  struct object *mantissa_fix, *exponent;
  flonum_t flo, mantissa;

  s = byte_stream_lift(s);

  TC2("unmarshal_float", 0, s, type_file, type_enumerator);

  t = byte_stream_read_byte(s);
  if (t != marshaled_type_float) {
    printf("BC: unmarshaling float expected float type, but was %d.", t);
    exit(1);
  }

  sign = byte_stream_read_byte(s);

  mantissa_fix = unmarshal_integer(s);
  if (get_object_type(mantissa_fix) == type_flonum) {
    printf(
        "BC: expected mantissa part of float to fit into a fixnum or ufixnum "
        "during float unmarshaling");
    exit(1);
  }

  mantissa =
      (flonum_t)UFIXNUM_VALUE(mantissa_fix) /
      pow(2, DBL_MANT_DIG); /* TODO: make it choose between DBL/FLT properly */

  exponent = unmarshal_integer(s);
  if (get_object_type(exponent) != type_fixnum) {
    printf(
        "BC: expected exponent part of float to fit into a fixnum during float "
        "unmarshaling (was %s).", get_object_type_name(exponent));
    exit(1);
  }

  flo = ldexp(mantissa, FIXNUM_VALUE(exponent));
  return flonum(sign == 1 ? -flo : flo);
}

struct object *unmarshal_string(struct object *s) {
  unsigned char t;
  struct object *str, *length;

  s = byte_stream_lift(s);

  TC2("unmarshal_string", 0, s, type_file, type_enumerator);

  t = byte_stream_read_byte(s);
  if (t != marshaled_type_string) {
    printf("BC: unmarshaling string expected string type, but was %d.", t);
    exit(1);
  }

  length = unmarshal_integer(s);
  if (get_object_type(length) != type_fixnum) {
    printf(
        "BC: expected string length to fit into a fixnum during string "
        "unmarshal");
    exit(1);
  }

  str = byte_stream_read(s, FIXNUM_VALUE(length));
  OBJECT_TYPE(str) = type_string;
  return str;
}

/* this won't work, what if you have a symbol in the keyword package, and in the user package?
   like this (:abc 3 'thing 4) ? need to store package name along with it. and have some way
   of looking up packages by name. */
struct object *unmarshal_symbol(struct object *s, struct gis *g) {
  unsigned char t, has_home_package;
  struct object *symbol_name, *package_name, *length;

  s = byte_stream_lift(s);

  TC2("unmarshal_symbol", 0, s, type_file, type_enumerator);

  t = byte_stream_read_byte(s);
  if (t != marshaled_type_symbol) {
    printf("BC: unmarshaling symbol expected symbol type, but was %d.", t);
    exit(1);
  }

  length = unmarshal_integer(s);
  if (get_object_type(length) != type_fixnum) {
    printf(
        "BC: expected symbol length to fit into a fixnum during symbol "
        "unmarshal");
    exit(1);
  }
  symbol_name = byte_stream_read(s, FIXNUM_VALUE(length));
  OBJECT_TYPE(symbol_name) = type_string;
  has_home_package = byte_stream_do_read_byte(s, 0);
  if (has_home_package) {
    length = unmarshal_integer(s);
    if (get_object_type(length) != type_fixnum) {
      printf(
          "BC: expected symbol length to fit into a fixnum during symbol "
          "unmarshal");
      exit(1);
    }
    package_name = byte_stream_read(s, FIXNUM_VALUE(length));
    OBJECT_TYPE(package_name) = type_string;
    return intern(symbol_name, g, find_package(g, package_name));
  }
  return symbol(symbol_name);
}

struct object *unmarshal_cons(struct object *s, struct gis *g) {
  unsigned char t;
  struct object *car, *cdr;

  s = byte_stream_lift(s);

  t = byte_stream_read_byte(s);
  if (t != marshaled_type_cons) {
    printf("BC: unmarshaling cons expected cons type, but was %d.", t);
    exit(1);
  }

  car = unmarshal(s, g);
  cdr = unmarshal(s, g);
  return cons(car, cdr);
}

struct object *unmarshal_dynamic_byte_array(struct object *s) {
  unsigned char t;
  struct object *length;

  s = byte_stream_lift(s);

  t = byte_stream_read_byte(s);
  if (t != marshaled_type_dynamic_byte_array) {
    printf(
        "BC: unmarshaling dynamic-byte-array expected dynamic-byte-array type, "
        "but was %d.",
        t);
    exit(1);
  }

  length = unmarshal_integer(s); /* TODO: make sure this is not a float */
  return byte_stream_read(s, UFIXNUM_VALUE(length));
}

struct object *unmarshal_dynamic_array(struct object *s, struct gis *g) {
  unsigned char t;
  struct object *length, *darr;
  ufixnum_t i;

  s = byte_stream_lift(s);

  t = byte_stream_read_byte(s);
  if (t != marshaled_type_dynamic_array) {
    printf(
        "BC: unmarshaling dynamic-array expected dynamic-array type, but was "
        "%d.",
        t);
    exit(1);
  }

  length = unmarshal_integer(s); /* TODO: make sure this is not a float */
  darr = dynamic_array(UFIXNUM_VALUE(length));

  i = UFIXNUM_VALUE(length);
  /* unmarshal i items: */
  while (i != 0) {
    dynamic_array_push(darr, unmarshal(s, g));
    --i;
  }
  return darr;
}

struct object *unmarshal_bytecode(struct object *s, struct gis *g) {
  unsigned char t;
  struct object *constants, *code;

  s = byte_stream_lift(s);
  t = byte_stream_read_byte(s);
  if (t != marshaled_type_bytecode) {
    printf("BC: unmarshaling bytecode expected bytecode type, but was %d.", t);
    exit(1);
  }

  constants = unmarshal_dynamic_array(s, g);
  code = unmarshal_dynamic_byte_array(s);

  return bytecode(constants, code);
}

struct object *unmarshal_nil(struct object *s) {
  unsigned char t;
  s = byte_stream_lift(s);
  t = byte_stream_read_byte(s);
  if (t != marshaled_type_nil) {
    printf("BC: unmarshaling nil expected nil type, but was %d.", t);
    exit(1);
  }
  return NULL;
}

struct object *unmarshal(struct object *s, struct gis *g) {
  enum marshaled_type t;

  s = byte_stream_lift(s);
  t = byte_stream_peek_byte(s);

  switch (t) {
    case marshaled_type_dynamic_byte_array:
      return unmarshal_dynamic_byte_array(s);
    case marshaled_type_dynamic_array:
      return unmarshal_dynamic_array(s, g);
    case marshaled_type_cons:
      return unmarshal_cons(s, g);
    case marshaled_type_nil:
      return unmarshal_nil(s);
    case marshaled_type_integer:
      return unmarshal_integer(s);
    case marshaled_type_float:
      return unmarshal_float(s);
    case marshaled_type_string:
      return unmarshal_string(s);
    case marshaled_type_symbol:
      return unmarshal_symbol(s, g);
    case marshaled_type_bytecode:
      return unmarshal_bytecode(s, g);
    default:
      printf("BC: cannot unmarshal marshaled type %d.", t);
      return NULL;
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
  ba = dynamic_byte_array_concat(ba, marshal_fixnum_t(BC_VERSION));
  return ba;
}

struct object *write_file(struct object *file, struct object *o) {
  fixnum_t nmembers;
  TC("write_file", 0, file, type_file);
  switch (get_object_type(o)) {
    case type_dynamic_byte_array:
    case type_string:
      nmembers = fwrite(DYNAMIC_BYTE_ARRAY_BYTES(o), sizeof(char),
                        DYNAMIC_BYTE_ARRAY_LENGTH(o), FILE_FP(file));
      if (nmembers != DYNAMIC_BYTE_ARRAY_LENGTH(o)) {
        printf("BC: failed to write dynamic-byte-array to file.");
        exit(1);
      }
      break;
    default:
      printf("BC: can not write object of type %s to a file.",
             get_object_type_name(o));
      exit(1);
  }
  return NULL;
}

/* reads the entire file contents */
struct object *read_file(struct object *file) {
  struct object *dba;
  long file_size;

  TC("read_file", 0, file, type_file);

  fseek(FILE_FP(file), 0, SEEK_END);
  file_size = ftell(FILE_FP(file));
  fseek(FILE_FP(file), 0, SEEK_SET);

  dba = dynamic_byte_array(file_size);
  fread(DYNAMIC_BYTE_ARRAY_BYTES(dba), 1, file_size, FILE_FP(file));
  return dba;
}

/**
 * Writes bytecode to a file
 * @param bc the bytecode to write
 */
struct object *write_bytecode_file(struct object *file, struct object *bc) {
  TC("write_bytecode_file", 0, file, type_file);
  TC("write_bytecode_file", 1, bc, type_bytecode);

  write_file(file, make_bytecode_file_header());
  write_file(file, marshal_bytecode(bc));

  return NULL;
}

struct object *read_bytecode_file(struct object *s, struct gis *g) {
  struct object *version, *bc;

  s = byte_stream_lift(s);

  if (g == NULL)
    g = gis;

  if (byte_stream_read_byte(s) != 'b' || byte_stream_read_byte(s) != 'u' ||
      byte_stream_read_byte(s) != 'g') {
    printf("BC: Invalid magic string\n");
    exit(1);
  }

  version = unmarshal_integer(s);
  if (UFIXNUM_VALUE(version) != BC_VERSION) {
    printf(
        "BC: Version mismatch (this interpreter has version %d, the file has "
        "version %d).\n",
        BC_VERSION, (unsigned int)UFIXNUM_VALUE(version));
    exit(1);
  }

  bc = unmarshal_bytecode(s, g);

  if (get_object_type(bc) != type_bytecode) {
    printf(
        "BC: Bytecode file requires a marshalled bytecode object immediately "
        "after the header, but found a %s.\n",
        get_object_type_name(bc));
    exit(1);
  }
  return bc;
}

/*===============================*
 *===============================*
 * Read                          *
 *===============================*
 *===============================*/
char is_digit(char c) {
  return c >= '0' && c <= '9';
}

char is_whitespace(char c) {
  return c == ' ' || c == '\n' || c == '\r' || c == '\t';
}

char is_priority_character(char c) {
  return c == '"' || c == ')';
}

char skip_whitespace(struct object *s) {
  char c;
  while (byte_stream_has(s) && is_whitespace(c = byte_stream_peek_byte(s)))
    byte_stream_read_byte(s); /* throw away the whitespace */
  return c;
}

/* "(1 2 3)" -> (cons 1 (cons 2 (cons 3 nil))) */
/** s -> the stream to read from
 *  package -> the package symbols should be interned into
 *  g -> the global interpreter state
*/
struct object *read(struct gis *g, struct object *s, struct object *package) {
  char c;
  struct object *buf, *sexpr;

  /* for checking if is numeric */
  char is_numeric, is_flo, has_mantissa, has_e, sign, exponent_sign;
  fixnum_t fix;
  flonum_t flo;
  ufixnum_t byte_count;
  ufixnum_t i;
  char escape_next;
  struct object *integral_part;
  struct object *exponent_part;
  struct object *mantissa_part;

  if (package == NULL)
    package = gis->package;

  s = byte_stream_lift(s);

  if (!byte_stream_has(s)) {
    printf("Read was given empty input.");
    exit(1);
  }

  c = skip_whitespace(s);
  if (!byte_stream_has(s)) {
    printf("Read was given empty input (only contained whitespace).");
    exit(1);
  }

  if (c == '"') { /* beginning of string */
    byte_stream_read_byte(s); /* throw the quotation mark away */
    buf = dynamic_byte_array(10);
    while ((c = byte_stream_read_byte(s)) != '"' || escape_next) {
      if (!byte_stream_has(s)) {
        printf("Unexpected end of input during read of string.");
        exit(1);
      }
      if (escape_next) {
        if (c == '\\' || c == '"') {
          dynamic_byte_array_push_char(buf, c);
        } else if (c == 'n') {
          dynamic_byte_array_push_char(buf, '\n');
        } else if (c == 'r') {
          dynamic_byte_array_push_char(buf, '\r');
        } else if (c == 't') {
          dynamic_byte_array_push_char(buf, '\t');
        } else {
          printf("Invalid escape sequence \"\\%c\".", c);
          exit(1);
        }
        escape_next = 0;
      } else if (c == '\\') {
        escape_next = 1;
      } else {
        dynamic_byte_array_push_char(buf, c);
      }
    }
    if (escape_next) {
      printf("Expected escape sequence, but string ended.");
      exit(1);
    }
    OBJECT_TYPE(buf) = type_string;
    return buf;
  } else if (c == '(') { /* beginning of sexpr */
    buf = dynamic_array(10);
    sexpr = NULL;
    byte_stream_read_byte(s); /* throw away the opening paren */
    c = skip_whitespace(s);
    while (byte_stream_has(s) && (c = byte_stream_peek_byte(s)) != ')') {
      dynamic_array_push(buf, read(g, s, package));
      c = skip_whitespace(s);
    }
    if (!byte_stream_has(s)) {
      printf("Unexpected end of input during sexpr.");
      exit(1);
    }
    byte_stream_read_byte(s); /* throw away the closing paren */
    for (fix = DYNAMIC_ARRAY_LENGTH(buf) - 1; fix >= 0; --fix) sexpr = cons(DYNAMIC_ARRAY_VALUES(buf)[fix], sexpr);
    return sexpr;
  } else if (c == ':') { /* keyword */
    byte_stream_read_byte(s); /* throw away the colon */
    return read(g, s, g->keyword_package);
  } else if (c == '\'') { /* quoted expression */
    byte_stream_read_byte(s); /* throw away the quote */
    return cons(intern(string("quote"), g, package), cons(read(g, s, package), NULL));
  } else { /* either a number or a symbol */
    buf = dynamic_byte_array(10);
    is_numeric = 1; /* assume it is numeric unless proven otherwise */
    has_mantissa = 0; /* this is flipped to true when a period is found */
    is_flo = 0; /* assume the number is a fixnum unless proven otherwise */
    has_e = 0; /* indicates if we are in the exponent section of a flonum (e.g. 1e9) */
    sign = 0; /* assume the number is positive unless proven otherwise */
    exponent_sign = 0;
    fix = 0;
    flo = 0;
    byte_count = 0;
    while (byte_stream_has(s) && !is_whitespace(c = byte_stream_peek_byte(s))) {
      if (is_priority_character(c)) {
        /* these characters are special and can break the flow of an identifier or number */
        break;
      }
      if (is_numeric) { /* if we still believe we are parsing a number */
        if (byte_count == 0 && c == '+') { /* TODO: allow for signs in exponent part of flonum */
          /* pass */
        } else if (byte_count == 0 && c == '-') {
          sign = 1;
        } else if (has_e && STRING_LENGTH(exponent_part) == 0 && c == '-') {
          exponent_sign = 1;
        } else if (has_e && STRING_LENGTH(exponent_part) == 0 && c == '+') {
          /* pass */
        } else if (!is_digit(c)) { /* if this isn't a digit, then either it is 'e' or '.' (so long as we haven't alerady seen those). otherwise it isn't a number at all */
          if (c == '.') {
            if (has_e) {
              is_numeric = 0; /* no decimal numbers for exponents (e.g. 1e3.4) */
            } else if (has_mantissa == 0) {
              has_mantissa = is_flo = 1;
              integral_part = dynamic_byte_array_concat(buf, string(""));
              OBJECT_TYPE(integral_part) = type_string;
              exponent_part = string("");
              mantissa_part = string("");
            } else {
              is_numeric = 0; /* this isn't a number -- continue as a symbol */
            }
          } else if (c == 'e') {
            if (has_e == 0) {
              has_e = is_flo = 1;
              exponent_part = string("");
              if (!has_mantissa) {
                integral_part = dynamic_byte_array_concat(buf, string(""));
                OBJECT_TYPE(integral_part) = type_string;
                mantissa_part = string("");
              }
            } else {
              is_numeric = 0; /* this isn't a number -- continue as a symbol */
            }
          } else {
            is_numeric = 0; /* this isn't a number -- continue as a symbol */
          }
        } else {
          if (is_flo) {
            if (has_e) {
              dynamic_byte_array_push_char(exponent_part, c);
            } else if (has_mantissa) {
              dynamic_byte_array_push_char(mantissa_part, c);
            }
          }
        }
      }
      dynamic_byte_array_push_char(buf, c);
      ++byte_count;
      byte_stream_read_byte(s); /* throw this byte away */
    }
    /* check for dot, plus, minus -- don't mistake a lone dot for a float when it should really be a symbol */
    if (STRING_LENGTH(buf) == 1 && (STRING_CONTENTS(buf)[0] == '.' || STRING_CONTENTS(buf)[0] == '+' || STRING_CONTENTS(buf)[0] == '-' || STRING_CONTENTS(buf)[0] == 'e'))
      is_numeric = 0;
    if (is_numeric) {
      if (is_flo) {
        /* error prone way of converting to convert string to float 
           bad because each multiplication introduces more and more error */
        for (i = 0; i < STRING_LENGTH(integral_part); ++i)
          flo += (STRING_CONTENTS(integral_part)[i] - '0') * pow(10, STRING_LENGTH(integral_part) - i - 1);
        for (i = 0; i < STRING_LENGTH(mantissa_part); ++i) {
          flo += (STRING_CONTENTS(mantissa_part)[i] - '0') * pow(10, -((fixnum_t)i) - 1);
        }
        if (STRING_LENGTH(exponent_part) > 0) {
          for (i = 0; i < STRING_LENGTH(exponent_part); ++i)
            fix += (STRING_CONTENTS(exponent_part)[i] - '0') *
                   pow(10, STRING_LENGTH(exponent_part) - i - 1);
          flo = (exponent_sign ? 1/pow(10, fix) : pow(10, fix)) * flo;
        }
        return flonum(sign ? -flo : flo);
      } else {
        for (i = 0; i < DYNAMIC_BYTE_ARRAY_LENGTH(buf); ++i)
          fix += (DYNAMIC_BYTE_ARRAY_BYTES(buf)[i] - '0') * pow(10, DYNAMIC_BYTE_ARRAY_LENGTH(buf) - i - 1);
        return fixnum(sign ? -fix : fix);
      }
    }
    OBJECT_TYPE(buf) = type_string;
    return intern(buf, g, package);
  }
  return NULL;
}

/*===============================*
 *===============================*
 * Compile                       *
 *===============================*
 *===============================*/
struct object *compile(struct object *ast, struct object *bc, struct object *st);

void gen_load_constant(struct object *bc, struct object *value) {
  TC("gen_load_constant", 0, bc, type_bytecode);
  dynamic_array_push(BYTECODE_CONSTANTS(bc), value);
  dynamic_byte_array_push_char(BYTECODE_CODE(bc), op_const);
  marshal_ufixnum_t(DYNAMIC_ARRAY_LENGTH(BYTECODE_CONSTANTS(bc)) - 1, BYTECODE_CODE(bc), 0);
}

/*
struct object *compile_function(struct object *ast, struct object *bc, struct object *st) {
  struct object *cdr, *cddr, *a0, *a1, *t0, *t1;
  cdr = CONS_CDR(ast);

  a0 = CONS_CAR(cdr);
  t0 = get_object_type(a0);

  if (t0 == type_symbol) { a named function 
    dynamic_byte_array_push_char(BYTECODE_CODE(bc), op_);
  } else if (t0 == type_cons || t0 == type_nil) { anonymous function 
    printf("anonymous functions not supported");
    exit(1);
  } else {
    printf("Invalid function");
    exit(1);
  }
}
*/

/*
 *
 * Examples:
 *  (+ 1 2)
 *  constants=[1 2]
 *  code=
 *   load-constant 0
 *   load-constant 1
 *   add
 */
struct object *compile(struct object *ast, struct object *bc, struct object *st) {
  struct object *value, *car, *cursor, *constants, *code;
  ufixnum_t length;

  if (bc == NULL) {
    constants = dynamic_array(10);
    code = dynamic_byte_array(10);
    bc = bytecode(constants, code);
  }

/* compiles each item in the cons list, and runs "f" after each is compiled 
 * 
 * Good for compiling things like "print":
 * 
 * (print "a" "b" "c")
 * 
 *    load 0
 *    print
 *    load 1
 *    print
 *    load 2
 *    print
 * 
 * It could also have loaded then printed.
 * 
 */
#define COMPILE_DO_EACH(f)               \
  cursor = CONS_CDR(value);              \
  while (cursor != NULL) {               \
    compile(CONS_CAR(cursor), bc, NULL); \
    cursor = CONS_CDR(cursor);           \
    f;                                   \
  }

/* compiles each item in the cons list, starting with the first two, then after each item. 
 * 
 * Good for compiling operators like + and -.
 * 
 * (- 1 2 3 4)
 * 
 *    load 0
 *    load 1
 *    subtract
 *    load 2
 *    subtract
 *    load 3
 *    subtract 
 */
#define COMPILE_DO_AGG_EACH(f)           \
  length = 0;                            \
  cursor = CONS_CDR(value);              \
  while (cursor != NULL) {               \
    compile(CONS_CAR(cursor), bc, NULL); \
    cursor = CONS_CDR(cursor);           \
    ++length;                            \
    if (length >= 2) {                   \
      f;                                 \
    }                                    \
  }

  value = ast;
  switch (get_object_type(value)) {
    case type_nil:
    case type_flonum:
    case type_fixnum:
    case type_ufixnum:
    case type_string:
    case type_dynamic_byte_array:
    case type_package:
    case type_enumerator:
      gen_load_constant(bc, value);
      break;
    case type_symbol:
      dynamic_array_push(BYTECODE_CONSTANTS(bc), value);
      dynamic_byte_array_push_char(BYTECODE_CODE(bc), op_const);
      marshal_ufixnum_t(DYNAMIC_ARRAY_LENGTH(BYTECODE_CONSTANTS(bc)) - 1, BYTECODE_CODE(bc), 0);
      dynamic_byte_array_push_char(BYTECODE_CODE(bc), op_symbol_value);
      break;
    case type_file:
      printf("A object of type file cannot be compiled.");
      exit(1);
    case type_function:
      printf("A object of type function cannot be compiled.");
      exit(1);
    case type_cons:
      car = CONS_CAR(value);
      if (get_object_type(car) == type_symbol) {
        if (alist_get_value(st, car) != NULL) { /* if the value exists in the symbol table */
        } else { /* either it is undefined or a special form */
          /* TODO: */
          if (equals(SYMBOL_NAME(car), string("cons"))) {
            SF_REQ_N(2, value);
            compile(CONS_CAR(CONS_CDR(value)), bc, st);
            compile(CONS_CAR(CONS_CDR(CONS_CDR(value))), bc, st);
            dynamic_byte_array_push_char(BYTECODE_CODE(bc), op_cons);
            break;
          } else if (equals(SYMBOL_NAME(car), string("progn"))) {
            cursor = CONS_CDR(value);
            while (cursor != NULL) {
              compile(CONS_CAR(cursor), bc, st);
              cursor = CONS_CDR(cursor);
              if (cursor != NULL) {
                dynamic_byte_array_push_char(BYTECODE_CODE(bc), op_drop);
              }
            }
            break;
          } else if (equals(SYMBOL_NAME(car), string("quote"))) {
            SF_REQ_N(1, value);
            gen_load_constant(bc, CONS_CAR(CONS_CDR(value)));
            break;
          } else if (equals(SYMBOL_NAME(car), string("set"))) {
            /* if this is lexical, set the variable on the stack
               otherwise, set the symbol value slot */
            SF_REQ_N(2, value);
            compile(CONS_CAR(CONS_CDR(value)), bc, st);
            compile(CONS_CAR(CONS_CDR(CONS_CDR(value))), bc, st);
            dynamic_byte_array_push_char(BYTECODE_CODE(bc), op_set_symbol_value);
            break;
          } else if (equals(SYMBOL_NAME(car), string("print"))) {
            COMPILE_DO_EACH(
                { dynamic_byte_array_push_char(BYTECODE_CODE(bc), op_print); });
            break;
          } else if (equals(SYMBOL_NAME(car), string("+"))) {
            COMPILE_DO_AGG_EACH(
                { dynamic_byte_array_push_char(BYTECODE_CODE(bc), op_add); });
            break;
          } else if (equals(SYMBOL_NAME(car), string("-"))) {
            COMPILE_DO_AGG_EACH(
                { dynamic_byte_array_push_char(BYTECODE_CODE(bc), op_sub); });
            break;
          } else if (equals(SYMBOL_NAME(car), string("*"))) {
            COMPILE_DO_AGG_EACH(
                { dynamic_byte_array_push_char(BYTECODE_CODE(bc), op_mul); });
            break;
          } else if (equals(SYMBOL_NAME(car), string("/"))) {
            COMPILE_DO_AGG_EACH(
                { dynamic_byte_array_push_char(BYTECODE_CODE(bc), op_div); });
            break;
          } else if (equals(SYMBOL_NAME(car), string("and"))) {
            /* TODO: AND, OR, and EQ should short circuit. currently only allowing
             * two arguments, because later this could be impelmented as a macro
             */
            SF_REQ_N(2, value);
            compile(CONS_CAR(CONS_CDR(value)), bc, st);
            compile(CONS_CAR(CONS_CDR(CONS_CDR(value))), bc, st);
            dynamic_byte_array_push_char(BYTECODE_CODE(bc), op_and); 
            break;
          } else if (equals(SYMBOL_NAME(car), string("or"))) {
            SF_REQ_N(2, value);
            compile(CONS_CAR(CONS_CDR(value)), bc, st);
            compile(CONS_CAR(CONS_CDR(CONS_CDR(value))), bc, st);
            dynamic_byte_array_push_char(BYTECODE_CODE(bc), op_or); 
            break;
          } else if (equals(SYMBOL_NAME(car), string("="))) {
            SF_REQ_N(2, value);
            compile(CONS_CAR(CONS_CDR(value)), bc, st);
            compile(CONS_CAR(CONS_CDR(CONS_CDR(value))), bc, st);
            dynamic_byte_array_push_char(BYTECODE_CODE(bc), op_eq); 
            break;
          } else if (equals(SYMBOL_NAME(car), string("function"))) {
            /* (function asdf () ) */
            /*compile_function(value, bc, st);*/
            break;
          } else {
            printf("Undefined symbol\n");
            exit(1);
          }
        }
      }
      /*
      cursor = CONS_CDR(value);
      while (cursor != NULL) {
        compile(CONS_CAR(cursor), bc, NULL);
        cursor = CONS_CDR(cursor);
      }
      */
      break;
    case type_record:
      break;
    case type_dynamic_array:
    case type_bytecode:
      /* these need special cases because they could include non-constants */
      break;
  }

  return bc;
} 

/* 
 * Bytecode Interpreter 
 */
void push(struct object *object) {
  gis->stack = cons(object, gis->stack);
}

struct object *pop() {
  struct object *value;
  SC("pop", 1);
  value = gis->stack->w0.car;
  gis->stack = CONS_CDR(gis->stack);
  return value;
}

struct object *peek() {
  SC("peek", 1);
  return CONS_CAR(gis->stack);
}

void dup() {
  SC("dup", 1);
  gis->stack = cons(CONS_CAR(gis->stack), gis->stack);
}

/* (7F)_16 is (0111 1111)_2, it extracts the numerical value from the temporary
 */
/* (80)_16 is (1000 0000)_2, it extracts the flag from the temporary */
#define READ_OP_ARG()                                                  \
  if (i >= byte_count) {                                               \
    printf("BC: expected an op code argument, but bytecode ended.\n"); \
    break;                                                             \
  }                                                                    \
  t0 = code->bytes[++i];                                               \
  a0 = t0 & 0x7F;                                                      \
  while (t0 & 0x80) {                                                  \
    if (i >= byte_count) {                                             \
      printf(                                                          \
          "BC: expected an extended op code argument, but bytecode "   \
          "ended.\n");                                                 \
      break;                                                           \
    }                                                                  \
    t0 = code->bytes[++i];                                             \
    a0 = (a0 << 7) | (t0 & 0x7F);                                      \
  }

#define READ_CONST_ARG()                                             \
  READ_OP_ARG()                                                      \
  if (a0 >= constants_length) {                                      \
    printf(                                                          \
        "BC: bytecode attempted to access an out of bounds index "   \
        "%ld in the constants vector, but only have %ld constants.\n", \
        a0, constants_length);                                       \
    break;                                                           \
  }                                                                  \
  c0 = constants->values[a0];

/* returns the top of the stack for convience */
struct object *eval(struct object *bc) {
  unsigned long i,        /* the current byte being evaluated */
      byte_count;         /* number of  bytes in this bytecode */
  unsigned char t0;       /* temporary for reading bytecode arguments */
  unsigned long a0;       /* the arguments for bytecode parameters */
  struct object *v0, *v1, *temp0; /* temps for values popped off the stack */
  struct object *c0; /* temps for constants (used for bytecode arguments) */
  struct dynamic_byte_array
      *code; /* the byte array containing the bytes in the bytecode */
  struct dynamic_array *constants; /* the constants array */
  unsigned long constants_length;  /* the length of the constants array */

  TC("eval", 1, bc, type_bytecode);
  i = 0;
  code = bc->w1.value.bytecode->code->w1.value.dynamic_byte_array;
  constants = bc->w1.value.bytecode->constants->w1.value.dynamic_array;
  constants_length = constants->length;
  byte_count = code->length;

  while (i < byte_count) {
    switch (code->bytes[i]) {
      case op_drop: /* drop ( x -- ) */
        SC("drop", 1);
        pop();
        break;
      case op_dup: /* dup ( x -- x x ) */
        SC("dup", 1);
        dup();
        break;
      case op_cons: /* cons ( car cdr -- (cons car cdr) ) */
        SC("cons", 2);
        v1 = pop(); /* cdr */
        v0 = pop(); /* car */
        push(cons(v0, v1));
        break;
      case op_intern: /* intern ( string -- symbol ) */
        SC("intern", 1);
        v0 = pop();
        TC("intern", 0, v0, type_string);
        push(intern(pop(), gis, NULL));
        break;
      case op_dynamic_array: /* dynamic-array ( length -- array[length] ) */
        SC("dynamic-array", 1);
        v0 = pop(); /* length */
        TC("dynamic-array", 0, v0, type_fixnum);
        push(dynamic_array(FIXNUM_VALUE(v0)));
        break;
      case op_dynamic_array_get: /* array_ref ( array index -- object ) */
        SC("dynamic-array-get", 2);
        v0 = pop(); /* index */
        TC("dynamic-array-get", 0, v0, type_fixnum);
        v1 = pop(); /* array */
        TC("dynamic-array-get", 1, v1, type_dynamic_array);
        push(dynamic_array_get(v1, v0));
        break;
      case op_car: /* car ( (cons car cdr) -- car ) */
        SC("car", 1);
        push(pop()->w0.car);
        break;
      case op_cdr: /* cdr ( (cons car cdr) -- cdr ) */
        SC("cdr", 1);
        push(pop()->w1.cdr);
        break;
      case op_add: /* add ( x y -- x+y ) */
        SC("add", 2);
        v1 = pop(); /* y */
        v0 = pop(); /* x */
        push(fixnum(FIXNUM_VALUE(v0) + FIXNUM_VALUE(v1))); /* TODO: support flonum/ufixnum */
        break;
      case op_sub: /* sub ( x y -- x-y ) */
        SC("sub", 2);
        v1 = pop(); /* y */
        v0 = pop(); /* x */
        push(fixnum(FIXNUM_VALUE(v0) - FIXNUM_VALUE(v1))); /* TODO: support flonum/ufixnum */
        break;
      case op_mul: /* mul ( x y -- x*y ) */
        SC("mul", 2);
        v1 = pop(); /* y */
        v0 = pop(); /* x */
        push(fixnum(FIXNUM_VALUE(v0) * FIXNUM_VALUE(v1))); /* TODO: support flonum/ufixnum */
        break;
      case op_div: /* div ( x y -- x/y ) */
        SC("div", 2);
        v1 = pop(); /* y */
        v0 = pop(); /* x */
        push(fixnum(FIXNUM_VALUE(v0) / FIXNUM_VALUE(v1))); /* TODO: support flonum/ufixnum */
        break;
      case op_eq: /* eq ( x y -- z ) */
        SC("eq", 2);
        v1 = pop(); /* y */
        v0 = pop(); /* x */
        /* TODO: add t */
        push(equals(v0, v1) ? fixnum(1) : NULL);
        break;
      case op_and: /* and ( x y -- z ) */
        SC("and", 2);
        v1 = pop(); /* y */
        v0 = pop(); /* x */
        if (v0 != NULL && v1 != NULL) push(v1);
        else push(NULL);
        break;
      case op_or: /* or ( x y -- z ) */
        SC("or", 2);
        v1 = pop(); /* y */
        v0 = pop(); /* x */
        if (v0 != NULL && v1 != NULL) push(v1);
        else if (v0 != NULL) push(v0);
        else if (v1 != NULL) push(v1);
        else push(NULL);
        break;
      case op_const: /* const ( -- x ) */
        READ_CONST_ARG();
        push(c0);
        break;
      case op_print: /* print ( x -- ) */
        SC("print", 1);
        print(pop(), 0);
        break;
      case op_symbol_value: /* symbol-value ( sym -- ) */
        SC("symbol-value", 1);
        v0 = pop(); /* sym */
        temp0 = symbol_get_slot(v0, gis->value_keyword);
        if (temp0 == NULL) {
          printf("Symbol ");
          if (SYMBOL_PACKAGE(v0) != NULL) {
            print(PACKAGE_NAME(SYMBOL_PACKAGE(v0)), 0);
            printf(":");
          }
          print(SYMBOL_NAME(v0), 0);
          printf(" has no value.");
          exit(1);
        }
        push(CONS_CDR(temp0));
        break;
      case op_set_symbol_value: /* set-symbol-value ( sym val -- ) */
        SC("set-symbol-value", 1);
        v1 = pop(); /* val */
        v0 = pop(); /* sym */
        TC("set-symbol-value", 0, v0, type_symbol);
        symbol_set(v0, gis->value_keyword, v1);
        push(NULL);
        break;
      default:
        printf("No cases matched\n");
        exit(1);
        break;
    }
    ++i;
  }
  return gis->stack == NULL ? NULL : CONS_CAR(gis->stack);
}

/*
 * Creates the default global interpreter state
 */
void init() {
  gis = gis_alloc();
}

/*===============================*
 *===============================*
 * Tests                         *
 *===============================*
 *===============================*/
void run_tests() {
  struct object *darr, *o0, *o1, *dba, *dba1, *bc, *bc0, *bc1, *da, *code, *constants, *code0, *consts0, *code1, *consts1;
  struct gis *g;

  printf("Running tests...\n");

#define assert_string_eq(str1, str2) \
  assert(strcmp(bstring_to_cstring(str1), bstring_to_cstring(str2)) == 0);

#define END_TESTS()                  \
  printf("Tests were successful\n"); \
  return;

  /*
   * String - bug string to c string
   */
  assert(strcmp(bstring_to_cstring(string("asdf")), "asdf") == 0);
  assert(strcmp(bstring_to_cstring(string("")), "") == 0);

  /*
   * Dynamic Array
   */
  darr = dynamic_array(2);
  assert(FIXNUM_VALUE(dynamic_array_length(darr)) == 0);
  dynamic_array_push(darr, fixnum(5));
  assert(FIXNUM_VALUE(dynamic_array_length(darr)) == 1);
  assert(FIXNUM_VALUE(dynamic_array_get(darr, fixnum(0))) == 5);
  dynamic_array_push(darr, fixnum(9));
  assert(FIXNUM_VALUE(dynamic_array_length(darr)) == 2);
  assert(FIXNUM_VALUE(dynamic_array_get(darr, fixnum(1))) == 9);
  assert(DYNAMIC_ARRAY_CAPACITY(darr) == 2);
  dynamic_array_push(darr, fixnum(94));
  assert(FIXNUM_VALUE(dynamic_array_length(darr)) == 3);
  assert(FIXNUM_VALUE(dynamic_array_get(darr, fixnum(2))) == 94);
  assert(DYNAMIC_ARRAY_CAPACITY(darr) == 4);

  /*
   * Dynamic byte Array
   */
  dba = dynamic_byte_array(2);
  assert(FIXNUM_VALUE(dynamic_byte_array_length(dba)) == 0);
  dynamic_byte_array_push(dba, fixnum(5));
  assert(FIXNUM_VALUE(dynamic_byte_array_length(dba)) == 1);
  assert(dynamic_byte_array_get(dba, 0) == 5);
  dynamic_byte_array_push(dba, fixnum(9));
  assert(FIXNUM_VALUE(dynamic_byte_array_length(dba)) == 2);
  assert(dynamic_byte_array_get(dba, 1) == 9);
  assert(DYNAMIC_BYTE_ARRAY_CAPACITY(dba) == 2);
  dynamic_byte_array_push(dba, fixnum(94));
  assert(FIXNUM_VALUE(dynamic_byte_array_length(dba)) == 3);
  assert(dynamic_byte_array_get(dba, 2) == 94);
  assert(DYNAMIC_BYTE_ARRAY_CAPACITY(dba) == 4);

  dynamic_byte_array_insert_char(dba, 0, 96);
  assert(UFIXNUM_VALUE(dynamic_byte_array_length(dba)) == 4);
  assert(dynamic_byte_array_get(dba, 0) == 96);
  assert(dynamic_byte_array_get(dba, 1) == 5);
  assert(dynamic_byte_array_get(dba, 2) == 9);
  assert(dynamic_byte_array_get(dba, 3) == 94);

  dynamic_byte_array_insert_char(dba, 2, 99);
  assert(UFIXNUM_VALUE(dynamic_byte_array_length(dba)) == 5);
  assert(dynamic_byte_array_get(dba, 0) == 96);
  assert(dynamic_byte_array_get(dba, 1) == 5);
  assert(dynamic_byte_array_get(dba, 2) == 99);
  assert(dynamic_byte_array_get(dba, 3) == 9);
  assert(dynamic_byte_array_get(dba, 4) == 94);

  dba = dynamic_byte_array(2);
  dynamic_byte_array_insert_char(dba, 0, 11);
  assert(UFIXNUM_VALUE(dynamic_byte_array_length(dba)) == 1);
  assert(dynamic_byte_array_get(dba, 0) == 11);

  dynamic_byte_array_insert_char(dba, 1, 32);
  assert(UFIXNUM_VALUE(dynamic_byte_array_length(dba)) == 2);
  assert(dynamic_byte_array_get(dba, 0) == 11);
  assert(dynamic_byte_array_get(dba, 1) == 32);

  dba = dynamic_byte_array(2);
  dynamic_byte_array_push_char(dba, 4);
  dynamic_byte_array_push_char(dba, 9);
  dba1 = dynamic_byte_array(2);
  dynamic_byte_array_push_char(dba1, 77);
  dynamic_byte_array_push_char(dba1, 122);
  dba = dynamic_byte_array_concat(dba, dba1);
  assert(UFIXNUM_VALUE(dynamic_byte_array_length(dba)) == 4);
  assert(DYNAMIC_BYTE_ARRAY_CAPACITY(dba) == 4);
  assert(dynamic_byte_array_get(dba, 0) == 4);
  assert(dynamic_byte_array_get(dba, 1) == 9);
  assert(dynamic_byte_array_get(dba, 2) == 77);
  assert(dynamic_byte_array_get(dba, 3) == 122);

  /*
   * Marshaling/Unmarshaling
   */

  /* Fixnum marshaling */
  g = gis_alloc();
  /* no bytes */
  assert(FIXNUM_VALUE(unmarshal_integer(marshal_fixnum(fixnum(0)))) == 0);
  /* one byte */
  assert(FIXNUM_VALUE(unmarshal_integer(marshal_fixnum(fixnum(9)))) == 9);
  assert(FIXNUM_VALUE(unmarshal_integer(marshal_fixnum(fixnum(-23)))) == -23);
  assert(FIXNUM_VALUE(unmarshal_integer(marshal_fixnum(fixnum(-76)))) == -76);
  assert(FIXNUM_VALUE(unmarshal(marshal_fixnum(fixnum(-76)), g)) == -76);
  /* two bytes */
  assert(FIXNUM_VALUE(unmarshal_integer(marshal_fixnum(fixnum(256)))) == 256);
  assert(FIXNUM_VALUE(unmarshal_integer(marshal_fixnum(fixnum(257)))) == 257);
  assert(FIXNUM_VALUE(unmarshal_integer(marshal_fixnum(fixnum(-342)))) == -342);
  assert(FIXNUM_VALUE(unmarshal_integer(marshal_fixnum(fixnum(2049)))) == 2049);
  /* three bytes */
  assert(FIXNUM_VALUE(unmarshal_integer(marshal_fixnum(fixnum(123456)))) ==
         123456);
  assert(FIXNUM_VALUE(unmarshal_integer(marshal_fixnum(fixnum(-123499)))) ==
         -123499);
  assert(FIXNUM_VALUE(unmarshal_integer(marshal_fixnum(fixnum(20422)))) ==
         20422);
  /* four bytes */
  assert(FIXNUM_VALUE(unmarshal_integer(marshal_fixnum(fixnum(123456789)))) ==
         123456789);
  assert(FIXNUM_VALUE(unmarshal_integer(marshal_fixnum(fixnum(-123456789)))) ==
         -123456789);
  /* Should test fixnums that are from platforms that have larger word sizes
   * (e.g. read in a fixnum larger than four bytes on a machine with two byte
   * words) */
  assert(UFIXNUM_VALUE(unmarshal_integer(
             marshal_ufixnum(ufixnum(MAX_UFIXNUM)))) == MAX_UFIXNUM);

  /* flonum marshaling */
  assert(FLONUM_VALUE(unmarshal_float(marshal_flonum(flonum(0.0)))) == 0.0);
  assert(FLONUM_VALUE(unmarshal_float(marshal_flonum(flonum(1.0)))) == 1.0);
  assert(FLONUM_VALUE(unmarshal_float(marshal_flonum(flonum(1.23)))) == 1.23);
  assert(FLONUM_VALUE(unmarshal_float(marshal_flonum(flonum(1e23)))) == 1e23);
  assert(FLONUM_VALUE(unmarshal_float(marshal_flonum(flonum(1e-23)))) == 1e-23);
  assert(FLONUM_VALUE(unmarshal_float(
             marshal_flonum(flonum(123456789123456789123456789123456789.0)))) ==
         123456789123456789123456789123456789.0);
  assert(FLONUM_VALUE(unmarshal_float(marshal_flonum(flonum(-0.0)))) == -0.0);
  assert(FLONUM_VALUE(unmarshal_float(marshal_flonum(flonum(-1.23)))) == -1.23);
  /* TODO: test that NaN and +/- Infinity work */

  /* string marshaling */
  assert_string_eq(unmarshal_string(marshal_string(string("abcdef"))),
                   string("abcdef"));
  assert(strcmp(bstring_to_cstring(unmarshal_string(
                    marshal_string(string("nerEW)F9nef09n230(N#EOINWEogiwe")))),
                "nerEW)F9nef09n230(N#EOINWEogiwe") == 0);
  assert(
      strcmp(bstring_to_cstring(unmarshal_string(marshal_string(string("")))),
             "") == 0);

  g = gis_alloc();
  assert(unmarshal_symbol(marshal_symbol(intern(string("abcdef"), g, g->package)), g) ==
                   intern(string("abcdef"), g, g->package));
  /* uninterned symbol */
  o0 = unmarshal_symbol(marshal_symbol(symbol(string("fwe"))), g);
  assert(SYMBOL_PACKAGE(o0) == NULL);
  add_package(g, package(string("peep"), NULL));
  assert(unmarshal_symbol(marshal_symbol(intern(string("dinkle"), g, find_package(g, string("peep")))), g) ==
                   intern(string("dinkle"), g, find_package(g, string("peep"))));
  assert(unmarshal_symbol(marshal_symbol(intern(string("dinkle"), g, find_package(g, string("peep")))), g) !=
                   intern(string("dinkle"), g, g->package));

  /* nil marshaling */
  assert(unmarshal_nil(marshal_nil()) == NULL);

  /* cons filled with nils */
  o0 = unmarshal_cons(marshal_cons(cons(NULL, NULL)), g);
  assert(CONS_CAR(o0) == NULL);
  assert(CONS_CDR(o0) == NULL);
  /* cons with strings */
  o0 = unmarshal_cons(marshal_cons(cons(string("A"), string("B"))), g);
  assert(strcmp(bstring_to_cstring(CONS_CAR(o0)), "A") == 0);
  assert(strcmp(bstring_to_cstring(CONS_CDR(o0)), "B") == 0);
  /* cons list with fixnums */
  o0 = unmarshal_cons(marshal_cons(cons(fixnum(35), cons(fixnum(99), NULL))), g);
  assert(FIXNUM_VALUE(CONS_CAR(o0)) == 35);
  assert(FIXNUM_VALUE(CONS_CAR(CONS_CDR(o0))) == 99);
  assert(CONS_CDR(CONS_CDR(o0)) == NULL);

  /* marshal/unmarshal dynamic byte array */
  dba = dynamic_byte_array(10);
  dynamic_byte_array_push_char(dba, 54);
  dynamic_byte_array_push_char(dba, 99);
  dynamic_byte_array_push_char(dba, 23);
  dynamic_byte_array_push_char(dba, 9);
  o0 = unmarshal_dynamic_byte_array(marshal_dynamic_byte_array(dba));
  assert(DYNAMIC_BYTE_ARRAY_LENGTH(dba) == DYNAMIC_BYTE_ARRAY_LENGTH(o0));
  assert(strcmp(bstring_to_cstring(dba), bstring_to_cstring(o0)) == 0);

  /* marshal/unmarshal dynamic array */
  darr = dynamic_array(10);
  dynamic_array_push(darr, fixnum(3));
  dynamic_array_push(darr, string("e2"));
  dynamic_array_push(darr, NULL);
  o0 = unmarshal_dynamic_array(marshal_dynamic_array(darr), g);
  assert(DYNAMIC_ARRAY_LENGTH(darr) == DYNAMIC_ARRAY_LENGTH(o0));
  assert(FIXNUM_VALUE(DYNAMIC_ARRAY_VALUES(darr)[0]) == 3);
  assert(strcmp(bstring_to_cstring(DYNAMIC_ARRAY_VALUES(darr)[1]), "e2") == 0);
  assert(DYNAMIC_ARRAY_VALUES(darr)[2] == NULL);

  /* marshal bytecode */
  darr = dynamic_array(10); /* constants vector */
  dynamic_array_push(darr, string("blackhole"));
  dynamic_array_push(darr, flonum(3.234));
  dynamic_array_push(darr, ufixnum(234234234));
  dba = dynamic_byte_array(10); /* the code vector */
  dynamic_byte_array_push_char(dba, 0x62);
  dynamic_byte_array_push_char(dba, 0x39);
  dynamic_byte_array_push_char(dba, 0x13);
  dynamic_byte_array_push_char(dba, 0x23);
  bc = bytecode(darr, dba);
  o0 = unmarshal_bytecode(marshal_bytecode(bc), g);
  /* check constants vector */
  assert(DYNAMIC_ARRAY_LENGTH(darr) ==
         DYNAMIC_ARRAY_LENGTH(BYTECODE_CONSTANTS(o0)));
  assert(strcmp(bstring_to_cstring(
                    DYNAMIC_ARRAY_VALUES(BYTECODE_CONSTANTS(o0))[0]),
                "blackhole") == 0);
  assert(FLONUM_VALUE(DYNAMIC_ARRAY_VALUES(BYTECODE_CONSTANTS(o0))[1]) ==
         3.234);
  assert(UFIXNUM_VALUE(DYNAMIC_ARRAY_VALUES(BYTECODE_CONSTANTS(o0))[2]) ==
         234234234);
  /* check code vector */
  assert(DYNAMIC_BYTE_ARRAY_LENGTH(dba) ==
         DYNAMIC_BYTE_ARRAY_LENGTH(BYTECODE_CODE(o0)));
  assert(strcmp(bstring_to_cstring(dba),
                bstring_to_cstring(BYTECODE_CODE(o0))) == 0);

  /* to-string */
  assert_string_eq(to_string(fixnum(0)), string("0"));
  assert_string_eq(to_string(fixnum(1)), string("1"));
  assert_string_eq(to_string(fixnum(2)), string("2"));
  assert_string_eq(to_string(fixnum(3)), string("3"));
  assert_string_eq(to_string(fixnum(9)), string("9"));
  assert_string_eq(to_string(fixnum(10)), string("10"));

  assert_string_eq(to_string(string("ABC")), string("ABC"));
  assert_string_eq(to_string(string("")), string(""));

  assert_string_eq(to_string(cons(string("ABC"), fixnum(43))),
                   string("(\"ABC\" 43)"));
  assert_string_eq(
      to_string(cons(string("ABC"), cons(ufixnum(24234234), NULL))),
      string("(\"ABC\" (24234234 nil))"));

  assert_string_eq(to_string(flonum(0.0)), string("0.0"));
  assert_string_eq(to_string(flonum(1)), string("1.0"));
  assert_string_eq(to_string(flonum(0.01)), string("0.01"));
  assert_string_eq(to_string(flonum(56789.0)), string("56789.0"));
  assert_string_eq(to_string(flonum(56789.43215)), string("56789.43215"));
  assert_string_eq(to_string(flonum(9999999.41)), string("9999999.41"));
  assert_string_eq(to_string(flonum(9000000000000000.52)), string("9000000000000001.0"));
  assert_string_eq(to_string(flonum(9e15)), string("9000000000000000.0"));
  assert_string_eq(to_string(flonum(9e15 * 10)), string("9e+16"));
  assert_string_eq(to_string(flonum(0.123456789123456789123456789)), string("0.123456789"));
  assert_string_eq(to_string(flonum(1234123412341123499.123412341234)), string("1.234123412e+18"));
  assert_string_eq(to_string(flonum(1234123412941123499.123412341234)), string("1.234123413e+18")); /* should round last digit */
  assert_string_eq(to_string(flonum(0.000001)), string("0.000001"));
  assert_string_eq(to_string(flonum(0.0000001)), string("1e-7"));
  assert_string_eq(to_string(flonum(0.00000001)), string("1e-8"));
  assert_string_eq(to_string(flonum(0.000000059)), string("5.9e-8"));
  assert_string_eq(to_string(flonum(0.00000000000000000000000000000000000000000000034234234232942362341239123412312348)), string("3.423423423e-46"));
  assert_string_eq(to_string(flonum(0.00000000000000000000000000000000000000000000034234234267942362341239123412312348)), string("3.423423427e-46")); /* should round last digit */

  /* TODO: something is wrong with 1.0003 (try typing it into the REPL) not sure if its an issue during read or to-string. any leading zeros are cut off */
  /* TODO: something wrong with 23532456346234623462346236. */

  dba = dynamic_byte_array(10);
  dynamic_byte_array_push_char(dba, 60);
  dynamic_byte_array_push_char(dba, 71);
  dynamic_byte_array_push_char(dba, 1);
  assert_string_eq(to_string(dba), string("[0x3C 0x47 0x01]"));

  dba = dynamic_byte_array(10);
  assert_string_eq(to_string(dba), string("[]"));

  da = dynamic_array(10);
  dynamic_array_push(da, string("bink"));
  dynamic_array_push(da, flonum(3.245));
  assert_string_eq(to_string(da), string("[\"bink\" 3.245]"));

  da = dynamic_array(10);
  assert_string_eq(to_string(da), string("[]"));

  /*
   * equals 
   */

  /* fixnum */
  assert(equals(fixnum(-2), fixnum(-2)));
  assert(!equals(fixnum(-2), fixnum(2)));
  /* ufixnum */
  assert(equals(ufixnum(242344238L), ufixnum(242344238L)));
  assert(!equals(ufixnum(242344231L), ufixnum(242344238L)));
  /* flonum */
  assert(equals(flonum(1.2), flonum(1.2)));
  assert(equals(flonum(1.234), flonum(1.234)));
  assert(!equals(flonum(-1.234), flonum(1.234)));
  assert(equals(flonum(1e-3), flonum(0.001)));
  /* string */
  assert(equals(string("abcdefghi"), string("abcdefghi")));
  assert(!equals(string("e"), string("")));
  /* nil */
  assert(equals(NULL, NULL));
  /* cons */
  assert(equals(cons(fixnum(7), NULL), cons(fixnum(7), NULL)));
  assert(!equals(cons(fixnum(7), cons(fixnum(8), NULL)), cons(fixnum(7), cons(fixnum(10), NULL))));
  assert(!equals(cons(fixnum(7), cons(fixnum(8), NULL)), cons(fixnum(9), cons(fixnum(8), NULL))));
  assert(!equals(cons(fixnum(7), cons(fixnum(8), NULL)), cons(fixnum(7), cons(fixnum(8), cons(fixnum(9), NULL)))));
  assert(equals(cons(fixnum(7), cons(fixnum(8), cons(fixnum(9), NULL))), cons(fixnum(7), cons(fixnum(8), cons(fixnum(9), NULL)))));
  assert(equals(cons(cons(string("a"), cons(string("b"), NULL)), cons(fixnum(8), cons(fixnum(9), NULL))), 
                cons(cons(string("a"), cons(string("b"), NULL)), cons(fixnum(8), cons(fixnum(9), NULL)))));
  assert(!equals(cons(cons(string("a"), cons(string("b"), NULL)), cons(fixnum(8), cons(fixnum(9), NULL))), 
                cons(cons(string("a"), cons(string("x"), NULL)), cons(fixnum(8), cons(fixnum(9), NULL)))));
  /* dynamic array */
  o0 = dynamic_array(10);
  o1 = dynamic_array(10);
  assert(equals(o0, o1));

  dynamic_array_push(o0, fixnum(1));
  assert(!equals(o0, o1));
  dynamic_array_push(o1, fixnum(1));
  assert(equals(o0, o1));

  dynamic_array_push(o0, fixnum(2));
  dynamic_array_push(o1, fixnum(2));
  assert(equals(o0, o1));
  /* dynamic byte array */
  o0 = dynamic_byte_array(10);
  o1 = dynamic_byte_array(10);
  assert(equals(o0, o1));

  dynamic_byte_array_push(o0, fixnum(1));
  assert(!equals(o0, o1));
  dynamic_byte_array_push(o1, fixnum(1));
  assert(equals(o0, o1));

  dynamic_byte_array_push(o0, fixnum(2));
  dynamic_byte_array_push(o1, fixnum(2));
  assert(equals(o0, o1));

  /* enumerator */
  o0 = enumerator(string("abcdefg"));
  o1 = enumerator(string("abcdefg"));
  assert(equals(o0, o1));
  byte_stream_read(o1, 1);
  assert(!equals(o0, o1));
  byte_stream_read(o0, 1);
  assert(equals(o0, o1));
  o0 = enumerator(string("91"));
  o1 = enumerator(string("93"));
  assert(!equals(o0, o1));

  /* TODO: add equals tests for package, symbol and file */

  /* bytecode */
  code0 = dynamic_byte_array(100);
  dynamic_byte_array_push_char(code0, op_const);
  dynamic_byte_array_push_char(code0, 0);
  consts0 = dynamic_array(100);
  dynamic_array_push(consts0, string("ab"));
  dynamic_array_push(consts0, cons(string("F"), cons(string("E"), NULL)));
  dynamic_array_push(consts0, flonum(8));
  o0 = bytecode(consts0, code0);

  code1 = dynamic_byte_array(100);
  dynamic_byte_array_push_char(code1, op_const);
  dynamic_byte_array_push_char(code1, 0);
  consts1 = dynamic_array(100);
  dynamic_array_push(consts1, string("ab"));
  dynamic_array_push(consts1, cons(string("F"), cons(string("E"), NULL)));
  dynamic_array_push(consts1, flonum(8));
  o1 = bytecode(consts1, code1);
  assert(equals(o0, o1));

  dynamic_array_push(consts1, flonum(10));
  assert(!equals(o0, o1));

  dynamic_array_push(consts0, flonum(10));
  assert(equals(o0, o1));

  dynamic_byte_array_push_char(code1, 9);
  assert(!equals(o0, o1));

  dynamic_byte_array_push_char(code0, 9);
  assert(equals(o0, o1));

  /* To be equal the types must be the same */
  assert(!equals(NULL, fixnum(2)));
  assert(!equals(fixnum(8), string("g")));

  /******* read ***********/
#define T_READ_TEST(i, o) assert(equals(read(g, string(i), g->package), o))
#define T_SYM(s) intern(string(s), g, g->package)
  g = gis_alloc();
  /* reading fixnums */
  T_READ_TEST("3", fixnum(3));
  T_READ_TEST("12345", fixnum(12345));
  T_READ_TEST("   \r\n12345", fixnum(12345));
  /* reading strings */
  T_READ_TEST("\"a\"", string("a"));
  T_READ_TEST("\"\"", string(""));
  T_READ_TEST("\"12\\n34\"", string("12\n34"));
  T_READ_TEST("\"\\r\\n\\t\"", string("\r\n\t"));
  T_READ_TEST("\"\\\"abcd\\\"\"", string("\"abcd\""));
  /* reading symbols */
  T_READ_TEST("1.2.", T_SYM("1.2."));
  T_READ_TEST("1ee", T_SYM("1ee"));
  T_READ_TEST("a", T_SYM("a"));
  T_READ_TEST("abc", T_SYM("abc"));
  T_READ_TEST("abc ", T_SYM("abc"));
  T_READ_TEST("    abc ", T_SYM("abc"));
  /* symbols that could be mistaken for floats */
  T_READ_TEST("    1ee ", T_SYM("1ee"));
  T_READ_TEST("    1e3e ", T_SYM("1e3e"));
  T_READ_TEST("    1e1.2 ", T_SYM("1e1.2"));
  T_READ_TEST("    1.2.3e3 ", T_SYM("1.2.3e3"));
  T_READ_TEST("    ++1 ", T_SYM("++1"));
  T_READ_TEST("    1+ ", T_SYM("1+"));
  T_READ_TEST("    -+3 ", T_SYM("-+3"));
  T_READ_TEST("    . ", T_SYM("."));
  T_READ_TEST("+", T_SYM("+"));
  /* reading flonums */
  T_READ_TEST("1.2", flonum(1.2));
  T_READ_TEST("1.234567", flonum(1.234567));
  T_READ_TEST("1e9", flonum(1e9));
  T_READ_TEST("1.2345e9", flonum(1.2345e9));

  /* there was something bizarre going on for this test where:
    
      double x = -3;
      assert(pow(10, -3) == pow(10, x));

     was failing. Apparently this is because pow(10, -3) is handled at compile time (by GCC)
     and converted to the precise constant 0.001 (the closest number to 0.001 that can
     be represented in floating point). But the pow(10, x) was calling the linked math library's
     pow on my Windows machine which apparently is imprecise pow(10, x) returned a number larger than
     0.001. 

     To fix this, instead of doing pow(10, -3), I now do 1/pow(10, 3). this gives the same result even
     if a variable is used (to force the linked library to be called). To ensure the math library is linked
     , I also added the '-lm' compile flag.
  */
  T_READ_TEST("1e-3", flonum(0.001));
  T_READ_TEST("1e-5", flonum(0.00001));
  T_READ_TEST("1e-8", flonum(0.00000001));
  T_READ_TEST("1.234e-8", flonum(0.00000001234));
  T_READ_TEST(".1", flonum(0.1));
  T_READ_TEST(".93", flonum(0.93));
  T_READ_TEST("3.", flonum(3.0));
  T_READ_TEST("3.e4", flonum(3.0e4));
  T_READ_TEST("3.e0", flonum(3.0e0));

  /* sexprs */
  T_READ_TEST("()", NULL);
  T_READ_TEST("(      )", NULL);
  T_READ_TEST("( \t\n\r     )", NULL);
  T_READ_TEST("(         \t1\t    )", cons(fixnum(1), NULL));
  T_READ_TEST("\n  (         \t1\t    )\n\n  \t", cons(fixnum(1), NULL));
  T_READ_TEST("(1)", cons(fixnum(1), NULL));
  T_READ_TEST("(1 2 3)", cons(fixnum(1), cons(fixnum(2), cons(fixnum(3), NULL))));
  T_READ_TEST("(\"dinkle\" 1.234 1e-3)", cons(string("dinkle"), cons(flonum(1.234), cons(flonum(1e-3), NULL))));
  T_READ_TEST("(())", cons(NULL, NULL));
  T_READ_TEST("(() ())", cons(NULL, cons(NULL, NULL)));
  T_READ_TEST("((1 2) (\"a\" \"b\"))", cons(cons(fixnum(1), cons(fixnum(2), NULL)), cons(cons(string("a"), cons(string("b"), NULL)), NULL)));
  T_READ_TEST("(((1) (2)))", cons(cons(cons(fixnum(1), NULL), cons(cons(fixnum(2), NULL), NULL)), NULL));

  /* ensure special characters have priority */
  T_READ_TEST("(1\"b\")", cons(fixnum(1), cons(string("b"), NULL)));

  /********* symbol interning ************/ 
  g = gis_alloc();
  assert(count(PACKAGE_SYMBOLS(g->package)) == 0);
  o0 = intern(string("a"), g, g->package); /* this should create a new symbol and add it to the package */
  assert(count(PACKAGE_SYMBOLS(g->package)) == 1);
  assert(equals(SYMBOL_NAME(CONS_CAR(PACKAGE_SYMBOLS(g->package))), string("a")));
  assert(o0 == intern(string("a"), g, g->package)); /* intern should find the existing symbol */
  o0 = intern(string("b"), g, g->package); /* this should create a new symbol and add it to the package */
  assert(count(PACKAGE_SYMBOLS(g->package)) == 2);

  /********* alist ***************/
  o0 = NULL;
  assert(alist_get_slot(o0, string("a")) == NULL);
  assert(alist_get_value(o0, string("a")) == NULL);
  o0 = alist_extend(o0, string("b"), fixnum(4));
  assert(equals(alist_get_value(o0, string("b")), fixnum(4)));
  o0 = alist_extend(o0, string("b"), fixnum(11));
  assert(equals(alist_get_value(o0, string("b")), fixnum(11)));
  o0 = alist_extend(o0, string("v"), string("abc"));
  assert(equals(alist_get_value(o0, string("v")), string("abc")));

  /********** compilation ****************/
#define BEGIN_BC_TEST(input_string)                      \
  bc0 = compile(read(g, string(input_string), g->package), NULL, NULL); \
  code = dynamic_byte_array(10);                         \
  constants = dynamic_array(10);
#define END_BC_TEST()              \
  bc1 = bytecode(constants, code); \
  assert(equals(bc0, bc1));
#define DEBUG_END_BC_TEST()        \
  bc1 = bytecode(constants, code); \
  print(bc0, 1);                      \
  print(bc1, 1);                      \
  assert(equals(bc0, bc1));
#define T_BYTE(op) dynamic_byte_array_push_char(code, op);
#define T_CONST(c) dynamic_array_push(constants, c);

  g = gis_alloc();

  BEGIN_BC_TEST("3");
  T_BYTE(op_const); T_BYTE(0);
  T_CONST(fixnum(3));  
  END_BC_TEST();

  BEGIN_BC_TEST("\"dink\"");
  T_BYTE(op_const); T_BYTE(0);
  T_CONST(string("dink"));  
  END_BC_TEST();

  BEGIN_BC_TEST("(cons 1 2.3)");
  T_BYTE(op_const); T_BYTE(0);
  T_BYTE(op_const); T_BYTE(1);
  T_BYTE(op_cons);
  T_CONST(fixnum(1)); T_CONST(flonum(2.3));  
  END_BC_TEST();

  BEGIN_BC_TEST("(cons 1 (cons \"a\" 9))");
  T_BYTE(op_const); T_BYTE(0);
  T_BYTE(op_const); T_BYTE(1);
  T_BYTE(op_const); T_BYTE(2);
  T_BYTE(op_cons);
  T_BYTE(op_cons);
  T_CONST(fixnum(1)); T_CONST(string("a"));  
  T_CONST(fixnum(9));
  END_BC_TEST();

  BEGIN_BC_TEST("(print \"a\" \"b\" \"c\")");
  T_BYTE(op_const); T_BYTE(0);
  T_BYTE(op_print);
  T_BYTE(op_const); T_BYTE(1);
  T_BYTE(op_print);
  T_BYTE(op_const); T_BYTE(2);
  T_BYTE(op_print);
  T_CONST(string("a")); T_CONST(string("b"));  
  T_CONST(string("c"));
  END_BC_TEST();

  BEGIN_BC_TEST("(+ 1 2 3 4 5)");
  T_BYTE(op_const); T_BYTE(0);
  T_BYTE(op_const); T_BYTE(1);
  T_BYTE(op_add);
  T_BYTE(op_const); T_BYTE(2);
  T_BYTE(op_add);
  T_BYTE(op_const); T_BYTE(3);
  T_BYTE(op_add);
  T_BYTE(op_const); T_BYTE(4);
  T_BYTE(op_add);
  T_CONST(fixnum(1)); T_CONST(fixnum(2));
  T_CONST(fixnum(3)); T_CONST(fixnum(4));
  T_CONST(fixnum(5));
  END_BC_TEST();

  BEGIN_BC_TEST("(+ 1 (- 8 9 33) 4 5)");
  T_BYTE(op_const); T_BYTE(0);
  T_BYTE(op_const); T_BYTE(1);
  T_BYTE(op_const); T_BYTE(2);
  T_BYTE(op_sub);
  T_BYTE(op_const); T_BYTE(3);
  T_BYTE(op_sub);
  T_BYTE(op_add);
  T_BYTE(op_const); T_BYTE(4);
  T_BYTE(op_add);
  T_BYTE(op_const); T_BYTE(5);
  T_BYTE(op_add);
  T_CONST(fixnum(1)); T_CONST(fixnum(8));
  T_CONST(fixnum(9)); T_CONST(fixnum(33));
  T_CONST(fixnum(4)); T_CONST(fixnum(5));
  END_BC_TEST();

/*
  eval(compile(read(string("(print \"IT WORKS\")"), g->package), NULL, NULL));
  */

  END_TESTS();
}

/*
 * Compile Mode:
 *   > bug -c myfile.bug -o myfile.bc
 * Interpret Mode:
 *   > bug myfile.bc 
 *     OR
 *   > bug myfile.bug
 *     If it is a bytecode file, interpret as bytecode. Otherwise, attempt to compile it, then interpret the results. 
 * Repl Mode:
 *   > bug
 */
int main(int argc, char **argv) {
  char interpret_mode, compile_mode, repl_mode;
  struct object *bc, *input_file, *output_file, *input_filepath, *output_filepath, *temp;

  interpret_mode = compile_mode = repl_mode = 0;
  output_filepath = input_filepath = NULL;

  if (argc == 1) {
    repl_mode = 1;
  } else if (argc == 2) {
    interpret_mode = 1;

    if (strcmp(argv[1], "--help") == 0) {
      printf("Bug LISP\n");
      printf("  --run-tests\tRuns the interpreter and compiler's test suite.\n");
      printf("  -c\tCompiles the given file.\n");
      printf("  -o\tChooses a name for the compiled file.\n");
      return 0;
    }

    if (strcmp(argv[1], "--run-tests") == 0) {
      run_tests();
      return 0;
    }

    if (strcmp(argv[1], "-c") == 0) {
      printf("You must provide a file name to the -c parameter (the source file to compile).");
      return 0;
    }
    if (strcmp(argv[1], "-o") == 0) {
      printf("You may only use the -o parameter when compiling a source file.");
      return 0;
    }

    if (argv[1][0] == '-') {
      printf("Invalid command argument \"%s\". Use --help flag to see options.", argv[1]);
      return 0;
    }

    input_filepath = string(argv[1]);
  } else {
    compile_mode = 1;
    if (strcmp(argv[1], "-c") != 0) {
      /* TODO */
      printf("help message");
      return 0;
    }
    input_filepath = string(argv[2]);

    if (argc < 5 || strcmp(argv[3], "-o") != 0) {
      /* TODO */
      printf("You must specify an output file name with the -o argument.");
      return 0;
    }
    output_filepath = string(argv[4]);
  }

  init();

  if (compile_mode) {
    input_file = open_file(input_filepath, string("r"));
    output_file = open_file(output_filepath, string("wb"));
    temp = NULL;
    while (byte_stream_has(input_file)) {
      temp = cons(read(gis, input_file, gis->package), temp);
    }
    temp = cons_reverse(temp);
    temp = cons(intern(string("progn"), gis, gis->lisp_package), temp);
    print(temp, 1);
    bc = compile(temp, NULL, NULL);
    close_file(input_file);
    write_bytecode_file(output_file, bc);
    close_file(output_file);
  } else if (interpret_mode) { /* add logic to compile the file if given a source file */
    input_file = open_file(input_filepath, string("r"));
    bc = read_bytecode_file(input_file, NULL);
    eval(bc);
    close_file(input_file);
  } else if (repl_mode) {
    input_file = file_stdin();
    output_file = file_stdout();
    while (1) {
      write_file(output_file, string("b> "));
      eval(compile(read(gis, input_file, NULL), NULL, NULL));
      print(gis->stack == NULL ? NULL : pop(), 1);
    }
  }

  return 0;
}