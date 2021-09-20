/** @file interpreter.c
 *
 * The Bug Bytecode interpreter
 */

#include "bytecode.h"

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
void stack_check(struct gis *g, char *name, int n) {
  unsigned int stack_count = count(g->stack);
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

struct object *intern(struct gis *g, struct object *string) {
  TC("intern", 0, string, type_string);
  return 0;
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

struct object *dynamic_byte_array(fixnum_t initial_capacity) {
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
  o->w1.value.bytecode->code = code;
  o->w1.value.bytecode->constants = constants;
  return o;
}

struct object *string(char *contents) {
  struct object *o;
  fixnum_t length = strlen(contents);
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

struct object *package(struct object *name) {
  struct object *o = object(type_package);
  NC(o, "Failed to allocate package object.");
  o->w1.value.package = malloc(sizeof(struct package));
  NC(o->w1.value.package, "Failed to allocate package object.");
  o->w1.value.package->name = name;
  o->w1.value.package->symbols = 0;
  return o;
}

struct object *cons(struct object *car, struct object *cdr) {
  struct object *o = object(type_cons);
  o->w0.car = car;
  o->w1.cdr = cdr;
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

/* 
 * Bytecode Interpreter 
 */
void push(struct gis *g, struct object *object) {
  g->stack = cons(object, g->stack);
}

struct object *pop(struct gis *g) {
  struct object *value;
  SC(g, "pop", 1);
  value = g->stack->w0.car;
  g->stack = CONS_CDR(g->stack);
  return value;
}

struct object *peek(struct gis *g) {
  SC(g, "peek", 1);
  return CONS_CAR(g->stack);
}

void dup(struct gis *g) {
  SC(g, "dup", 1);
  g->stack = cons(CONS_CAR(g->stack), g->stack);
}

struct gis *gis() {
  struct gis *g = malloc(sizeof(struct gis));
  NC(g, "Failed to allocate global interpreter state.");
  g->stack = 0;
  g->package = 0;
  return g;
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

void eval(struct gis *g, struct object *bc) {
  unsigned long i,        /* the current byte being evaluated */
      byte_count;         /* number of  bytes in this bytecode */
  unsigned char t0;       /* temporary for reading bytecode arguments */
  unsigned long a0;       /* the arguments for bytecode parameters */
  struct object *v0, *v1; /* temps for values popped off the stack */
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
        SC(g, "drop", 1);
        pop(g);
        break;
      case op_dup: /* dup ( x -- x x ) */
        SC(g, "dup", 1);
        dup(g);
        break;
      case op_cons: /* cons ( car cdr -- (cons car cdr) ) */
        SC(g, "cons", 2);
        v1 = pop(g); /* cdr */
        v0 = pop(g); /* car */
        push(g, cons(v0, v1));
        break;
      case op_intern: /* intern ( string -- symbol ) */
        SC(g, "intern", 1);
        v0 = pop(g);
        TC("intern", 0, v0, type_string);
        push(g, intern(g, pop(g)));
        break;
      case op_dynamic_array: /* dynamic-array ( length -- array[length] ) */
        SC(g, "dynamic-array", 1);
        v0 = pop(g); /* length */
        TC("dynamic-array", 0, v0, type_fixnum);
        push(g, dynamic_array(FIXNUM_VALUE(v0)));
        break;
      case op_dynamic_array_get: /* array_ref ( array index -- object ) */
        SC(g, "dynamic-array-get", 2);
        v0 = pop(g); /* index */
        TC("dynamic-array-get", 0, v0, type_fixnum);
        v1 = pop(g); /* array */
        TC("dynamic-array-get", 1, v1, type_dynamic_array);
        push(g, dynamic_array_get(v1, v0));
        break;
      case op_car: /* car ( (cons car cdr) -- car ) */
        SC(g, "car", 1);
        push(g, pop(g)->w0.car);
        break;
      case op_cdr: /* cdr ( (cons car cdr) -- cdr ) */
        SC(g, "cdr", 1);
        push(g, pop(g)->w1.cdr);
        break;
      case op_add: /* add ( x y -- sum ) */
        SC(g, "add", 2);
        push(g, 0); /* TODO */
        break;
      case op_const: /* const ( -- x ) */
        READ_CONST_ARG();
        push(g, c0);
        break;
      default:
        printf("No cases matched\n");
        exit(1);
        break;
    }
    ++i;
  }
}

/*
 * Creates the default global interpreter state
 */
void init() {
  struct gis *g = gis();
  g->package = package(string("user"));
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
 * Equality                      *
 *===============================*
 *===============================*/

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

struct object *marshal_ufixnum_t(ufixnum_t n) {
  struct object *ba;
  unsigned char byte;

  ba = dynamic_byte_array(4);
  dynamic_byte_array_push_char(ba, marshaled_type_integer);
  dynamic_byte_array_push_char(ba, 0);

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
  return marshal_ufixnum_t(UFIXNUM_VALUE(n));
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
  ba = dynamic_byte_array_concat(ba, marshal_ufixnum_t(mantissa_fix));
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
struct object *unmarshal(struct object *s);

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

struct object *unmarshal_cons(struct object *s) {
  unsigned char t;
  struct object *car, *cdr;

  s = byte_stream_lift(s);

  t = byte_stream_read_byte(s);
  if (t != marshaled_type_cons) {
    printf("BC: unmarshaling cons expected cons type, but was %d.", t);
    exit(1);
  }

  car = unmarshal(s);
  cdr = unmarshal(s);
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

struct object *unmarshal_dynamic_array(struct object *s) {
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
    dynamic_array_push(darr, unmarshal(s));
    --i;
  }
  return darr;
}

struct object *unmarshal_bytecode(struct object *s) {
  unsigned char t;
  struct object *constants, *code;

  s = byte_stream_lift(s);
  t = byte_stream_read_byte(s);
  if (t != marshaled_type_bytecode) {
    printf("BC: unmarshaling bytecode expected bytecode type, but was %d.", t);
    exit(1);
  }

  constants = unmarshal_dynamic_array(s);
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

struct object *unmarshal(struct object *s) {
  enum marshaled_type t;

  s = byte_stream_lift(s);
  t = byte_stream_peek_byte(s);

  switch (t) {
    case marshaled_type_dynamic_byte_array:
      return unmarshal_dynamic_byte_array(s);
    case marshaled_type_dynamic_array:
      return unmarshal_dynamic_array(s);
    case marshaled_type_cons:
      return unmarshal_cons(s);
    case marshaled_type_nil:
      return unmarshal_nil(s);
    case marshaled_type_integer:
      return unmarshal_integer(s);
    case marshaled_type_float:
      return unmarshal_float(s);
    case marshaled_type_string:
      return unmarshal_string(s);
    case marshaled_type_bytecode:
      return unmarshal_bytecode(s);
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

struct object *read_file(struct object *file) {
  struct object *dba;
  long file_size;

  TC("read_file", 0, file, type_file);

  fseek(FILE_FP(file), 0, SEEK_END);
  file_size = ftell(FILE_FP(file));
  fseek(FILE_FP(file), 0, SEEK_SET);

  dba = dynamic_byte_array(file_size);
  fread(DYNAMIC_BYTE_ARRAY_BYTES(dba), 1, file_size, FILE_FP(file));
  fclose(FILE_FP(file));
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

struct object *read_bytecode_file(struct object *s) {
  struct object *version, *bc;

  s = byte_stream_lift(s);

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

  bc = unmarshal_bytecode(s);

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
 * Tests                         *
 *===============================*
 *===============================*/
void run_tests() {
  struct object *darr, *o0, *dba, *dba1, *bc, *da;

  printf("Running tests...\n");

#define assert_string_eq(str1, str2) \
  assert(strcmp(bstring_to_cstring(str1), bstring_to_cstring(str2)) == 0);

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
  /* no bytes */
  assert(FIXNUM_VALUE(unmarshal_integer(marshal_fixnum(fixnum(0)))) == 0);
  /* one byte */
  assert(FIXNUM_VALUE(unmarshal_integer(marshal_fixnum(fixnum(9)))) == 9);
  assert(FIXNUM_VALUE(unmarshal_integer(marshal_fixnum(fixnum(-23)))) == -23);
  assert(FIXNUM_VALUE(unmarshal_integer(marshal_fixnum(fixnum(-76)))) == -76);
  assert(FIXNUM_VALUE(unmarshal(marshal_fixnum(fixnum(-76)))) == -76);
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

  /* nil marshaling */
  assert(unmarshal_nil(marshal_nil()) == NULL);

  /* cons filled with nils */
  o0 = unmarshal_cons(marshal_cons(cons(NULL, NULL)));
  assert(CONS_CAR(o0) == NULL);
  assert(CONS_CDR(o0) == NULL);
  /* cons with strings */
  o0 = unmarshal_cons(marshal_cons(cons(string("A"), string("B"))));
  assert(strcmp(bstring_to_cstring(CONS_CAR(o0)), "A") == 0);
  assert(strcmp(bstring_to_cstring(CONS_CDR(o0)), "B") == 0);
  /* cons list with fixnums */
  o0 = unmarshal_cons(marshal_cons(cons(fixnum(35), cons(fixnum(99), NULL))));
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
  o0 = unmarshal_dynamic_array(marshal_dynamic_array(darr));
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
  o0 = unmarshal_bytecode(marshal_bytecode(bc));
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

  printf("Tests were successful\n");
}

int main() {
  struct object *bc, *code, *consts, *file;
  struct gis *g;

  run_tests();

  g = gis();
  g->package = package(string("user"));

  code = dynamic_byte_array(100);
  dynamic_byte_array_push_char(code, op_const);
  dynamic_byte_array_push_char(code, 0);

  consts = dynamic_array(100);
  dynamic_array_push(consts, string("ab"));
  dynamic_array_push(consts, cons(string("F"), cons(string("E"), NULL)));
  dynamic_array_push(consts, flonum(8));

  bc = bytecode(consts, code);

  eval(g, bc);

  assert(strcmp(bstring_to_cstring(peek(g)), "ab") == 0);

  file = open_file(string("file.txt"), string("wb"));
  write_bytecode_file(file, bc);
  close_file(file);

  file = open_file(string("file.txt"), string("r"));
  bc = read_bytecode_file(file);
  close_file(file);

  eval(g, bc);

  return 0;
}