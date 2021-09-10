/** @file interpreter.c
 *
 * The Bug Bytecode interpreter
 */

#include "bytecode.h"

/* UTIL */
double log2(double n) { return log(n) / log(2); }

char *get_type_name(enum type t) {
  switch (t) {
    case type_cons:
      return "cons";
    case type_fixnum:
      return "fixnum";
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
  }
  printf("BC: Given type number has no implemented name %d", t);
  exit(1);
}

#ifdef RUN_TIME_CHECKS
void type_check(char *name, unsigned int argument, struct object *o,
                enum type type) {
  if (o->w0.type != type) {
    printf(
        "BC: Function \"%s\" was called with an invalid argument "
        "at index %d. Expected type %s, but was %s.\n",
        name, argument, get_type_name(type), get_type_name(o->w0.type));
    exit(1);
  }
}

void type_check_or2(char *name, unsigned int argument, struct object *o,
                enum type type0, enum type type1) {
  if (o->w0.type != type0 && o->w0.type != type1) {
    printf(
        "BC: Function \"%s\" was called with an invalid argument "
        "at index %d. Expected either %s or %s, but was %s.\n",
        name, argument, get_type_name(type0), get_type_name(type1), get_type_name(o->w0.type));
    exit(1);
  }
}

/**
 * Converts a bug string to a c string
 */
char *bstring_to_cstring(struct object *str) {
  fixnum_t length;
  char *buf;
  TC("bstring_to_cstring", 0, str, type_string);
  length = STRING_LENGTH(str);
  buf = malloc(sizeof(char) * (length + 1));
  memcpy(buf, STRING_CONTENTS(str), length);
  buf[length] = '\0';
  return buf;
}

fixnum_t count(struct object *list) {
  fixnum_t count = 0;
  while (list != 0) {
    ++count;
    list = list->w1.cdr;
  }
  return count;
}

void stack_check(struct gis *g, char *name, fixnum_t n) {
  fixnum_t stack_count = count(g->stack);
  if (stack_count < n) {
    printf(
        "BC: Operation \"%s\" was called when the stack had too "
        "few items. Expected %d items on the stack, but were %d.\n",
        name, n, stack_count);
    exit(1);
  }
}
#endif

struct object *object(enum type type) {
  struct object *o = malloc(sizeof(struct object));
  if (o == NULL) return NULL;
  o->w0.type = type;
  return o;
}

struct object *fixnum(fixnum_t fixnum) {
  struct object *o = object(type_fixnum);
  NC(o, "Failed to allocate fixnum object.");
  o->w1.value.fixnum = fixnum;
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
  DYNAMIC_BYTE_ARRAY_CAPACITY(o) = initial_capacity < 0 ? DEFAULT_INITIAL_CAPACITY : initial_capacity;
  DYNAMIC_BYTE_ARRAY_BYTES(o) = malloc(DYNAMIC_BYTE_ARRAY_CAPACITY(o) * sizeof(char));
  NC(DYNAMIC_BYTE_ARRAY_BYTES(o), "Failed to allocate dynamic-byte-array bytes.");
  DYNAMIC_BYTE_ARRAY_LENGTH(o) = 0;
  return o;
}

struct object *bytecode(struct object *code, struct object *constants) {
  struct object *o;
  TC("bytecode", 0, code, type_dynamic_byte_array);
  TC("bytecode", 1, constants, type_dynamic_array);
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
  o->w0.type = type_string;
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
    DYNAMIC_ARRAY_CAPACITY(da) = DYNAMIC_ARRAY_CAPACITY(da) * 3/2.0;
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
    DYNAMIC_BYTE_ARRAY_CAPACITY(dba) = DYNAMIC_BYTE_ARRAY_CAPACITY(dba) * 3/2.0;
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
/*
struct object *dynamic_byte_array_splice(struct object *dba, fixnum_t i, fixnum_t j) {
  struct object *o;

  TC2("dynamic_byte_array_splice", 0, dba, type_dynamic_byte_array, type_string);
  o = dynamic_byte_array(j - i);
  memcpy(DYNAMIC_BYTE_ARRAY_BYTES(o), &DYNAMIC_BYTE_ARRAY_BYTES(dba)[i], (j - i) * sizeof(char));
  return o;
}
*/

void push(struct gis *g, struct object *object) {
  g->stack = cons(object, g->stack);
}

struct object *pop(struct gis *g) {
  struct object *value;
  SC(g, "pop", 1);
  value = g->stack->w0.car;
  g->stack = g->stack->w1.cdr;
  return value;
}

struct object *peek(struct gis *g) {
  SC(g, "peek", 1);
  return g->stack->w0.car;
}

void dup(struct gis *g) {
  SC(g, "dup", 1);
  g->stack = cons(g->stack->w0.car, g->stack);
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
        "%d in the constants vector, but only have %d constants.\n", \
        a0, constants_length);                                       \
    break;                                                           \
  }                                                                  \
  c0 = constants->values[a0];

void eval(struct gis *g, struct object *bc) {
  fixnum_t i,             /* the current byte being evaluated */
      byte_count;         /* number of  bytes in this bytecode */
  char t0;                /* temporary for reading bytecode arguments */
  fixnum_t a0;            /* the arguments for bytecode parameters */
  struct object *v0, *v1; /* temps for values popped off the stack */
  struct object *c0; /* temps for constants (used for bytecode arguments) */
  struct dynamic_byte_array
      *code; /* the byte array containing the bytes in the bytecode */
  struct dynamic_array *constants; /* the constants array */
  fixnum_t constants_length; /* the length of the constants array */

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

struct object *byte_stream_lift(struct object *e) {
  if (OBJECT_TYPE(e) == type_dynamic_byte_array || OBJECT_TYPE(e) == type_string) {
    return enumerator(e);
  } else if (OBJECT_TYPE(e) == type_file || OBJECT_TYPE(e) == type_enumerator) {
    return e;
  } else {
    printf("BC: attempted to lift unsupported type %s into a byte-stream", get_type_name(OBJECT_TYPE(e)));
    exit(1);
  }
}

struct object *byte_stream_do_read(struct object *e, fixnum_t n, char peek) {
  struct object *ret;

  TC2("byte_stream_get", 0, e, type_enumerator, type_file);

  ret = dynamic_byte_array(n);
  DYNAMIC_ARRAY_LENGTH(ret) = n;

  if (e->w0.type == type_file) {
    fread(DYNAMIC_BYTE_ARRAY_BYTES(ret), sizeof(char), n, FILE_FP(e));
    if (peek) fseek(FILE_FP(e), -n, SEEK_CUR);
  } else {
    switch (ENUMERATOR_SOURCE(e)->w0.type) {
      case type_dynamic_byte_array:
      case type_string:
        memcpy(DYNAMIC_BYTE_ARRAY_BYTES(ret), &DYNAMIC_BYTE_ARRAY_BYTES(ENUMERATOR_SOURCE(e))[ENUMERATOR_INDEX(e)], sizeof(char) * n);
        if (!peek) ENUMERATOR_INDEX(e) += n;
        break;
      default:
        printf("BC: byte stream get is not implemented for type %s.",
               get_type_name(e->w0.type));
        exit(1);
    }
  }
  return ret;
}

char byte_stream_do_read_byte(struct object *e, char peek) {
  char c;
  TC2("byte_steam_get_char", 0, e, type_enumerator, type_file);
  if (e->w0.type == type_file) {
    c = fgetc(FILE_FP(e)); 
    if (peek) ungetc(c, FILE_FP(e));
  } else {
    switch (OBJECT_TYPE(ENUMERATOR_SOURCE(e))) {
      case type_dynamic_byte_array:
      case type_string:
        c = DYNAMIC_BYTE_ARRAY_BYTES(ENUMERATOR_SOURCE(e))[ENUMERATOR_INDEX(e)];
        if (!peek) ++ENUMERATOR_INDEX(e);
        break;
      default:
        printf("BC: byte stream get char is not implemented for type %s.",
               get_type_name(OBJECT_TYPE(ENUMERATOR_SOURCE(e))));
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


struct object *to_string(struct object *o) {
  enum type t = o->w0.type;
  switch (t) {
    case type_fixnum:
      return string("<fixnum>");
    case type_flonum:
      return string("<flonum>");
    case type_string:
      return o;
    default:
      printf("BC: type %s does not support to-string", get_type_name(t));
      exit(1);
  }
}

/* Marshaling  */
struct object *marshal(struct object *o);

struct object *marshal_fixnum_t(fixnum_t n) {
  struct object *ba;
  char byte;

  ba = dynamic_byte_array(4);
  dynamic_byte_array_push_char(ba, type_fixnum);
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

struct object *marshal_string(struct object *str) {
  struct object *ba;

  TC("marshal_string", 0, str, type_string);

  ba = dynamic_byte_array(1);
  dynamic_byte_array_push_char(ba, type_string);
  ba = dynamic_byte_array_concat(ba, marshal_fixnum_t(STRING_LENGTH(str)));
  ba = dynamic_byte_array_concat(ba, str);
  return ba;
}

struct object *marshal_dynamic_byte_array(struct object *ba0) {
  struct object *ba1;

  TC("marshal_dynamic_byte_array", 0, ba0, type_dynamic_byte_array);

  ba1 = dynamic_byte_array(1);
  dynamic_byte_array_push_char(ba1, type_dynamic_byte_array);
  ba1 = dynamic_byte_array_concat(ba1, marshal_fixnum_t(DYNAMIC_BYTE_ARRAY_LENGTH(ba0)));
  ba1 = dynamic_byte_array_concat(ba1, ba0);
  return ba1;
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
  dynamic_byte_array_push_char(ba, type_dynamic_array);
  ba = dynamic_byte_array_concat(ba, marshal_fixnum_t(arr_length));

  for (i = 0; i < arr_length; ++i) {
    ba = dynamic_byte_array_concat(ba, marshal(c_arr[i]));
  }

  return ba;
}

/** 
 * The format used for marshaling fixnums is platform independent
 * it has the following form (each is one byte):
 *     0x04 <sign> <continuation_bit=1|7bits> <continuation_bit=1|7bits> etc... <continutation_bit=0|7bits>
 * a sign of 1 means it is a negative number, a sign of 0 means positive number
 * the continuation bit of 1 means the next byte is still part of the number
 * once the continuation bit of 0 is reached, the number is complete
 * 
 * I'm not sure if there is any advantage between using sign magnitude vs 1 or 2s compliment for
 * the marshaling format besides the memory cost of the sign byte (1 byte per number).
 */
struct object *marshal_fixnum(struct object *n) {
  TC("marshal_fixnum", 0, n, type_fixnum);
  return marshal_fixnum_t(FIXNUM_VALUE(n));
}

struct object *marshal_flonum(struct object *n) {
  struct object *ba;

  TC("marshal_flonum", 0, n, type_flonum);

  ba = dynamic_byte_array(1);
  dynamic_byte_array_push_char(ba, type_flonum);
  ba = dynamic_byte_array_concat(ba, marshal_fixnum_t(3)); /* TODO: convert lhs and rhs */
  ba = dynamic_byte_array_concat(ba, marshal_fixnum_t(9));

  return ba;
}

/**
 * turns bytecode into a byte-array that can be stored to a file
 */
struct object *marshal_bytecode(struct object *bc) {
  struct object *ba;
  TC("marshal_bytecode", 0, bc, type_bytecode);
  ba = dynamic_byte_array(1);
  dynamic_byte_array_push_char(ba, type_bytecode);
  ba = dynamic_byte_array_concat(ba, marshal_dynamic_array(BYTECODE_CONSTANTS(bc)));
  ba = dynamic_byte_array_concat(ba, marshal_dynamic_byte_array(BYTECODE_CODE(bc)));
  return ba;
}

/**
 * Takes any object and returns a byte-array containing the binary
 * representation of the object.
 */
struct object *marshal(struct object *o) {
  switch (o->w0.type) {
    case type_dynamic_byte_array:
      return NULL;
    case type_dynamic_array:
      return NULL;
    case type_cons:
      return NULL;
    case type_fixnum:
      return marshal_fixnum(o);
    case type_flonum:
      return marshal_flonum(o);
    case type_string:
      return marshal_string(o);
    case type_bytecode:
      return marshal_bytecode(o);
    default:
      printf("BC: cannot marshal type %s.\n", get_type_name(o->w0.type));
      return NULL;
  }
}

fixnum_t unmarshal_fixnum_t(struct object *s) {
  char byte, sign;
  fixnum_t n, t, byte_count;

  s = byte_stream_lift(s);

  TC2("unmarshal_fixnum", 0, s, type_enumerator, type_file);

  t = byte_stream_read_byte(s);
  if (t != type_fixnum) {
    printf("BC: unmarshaling fixnum expected fixnum type, but got %s.", get_type_name(t));
    exit(1);
  }

  sign = byte_stream_read_byte(s);

  n = 0;
  byte_count = 0;
  do { 
    byte = byte_stream_read_byte(s);
    n |= (byte & 0x7F) << (7 * byte_count); /* -1 because i counts the type byte */
    ++byte_count;
  } while (byte & 0x80);

  return sign ? -n : n;
}

struct object *unmarshal_fixnum(struct object *s) {
  return fixnum(unmarshal_fixnum_t(s));
}

struct object *unmarshal_string(struct object *s) {
  fixnum_t t, length;
  struct object *str;

  s = byte_stream_lift(s);

  TC2("unmarshal_string", 0, s, type_file, type_enumerator);

  t = byte_stream_read_byte(s);
  if (t != type_string) {
    printf("BC: unmarshaling string expected string type, but got %s.", get_type_name(t));
    exit(1);
  }

  length = unmarshal_fixnum_t(s);

  str = byte_stream_read(s, length);
  OBJECT_TYPE(str) = type_string;
  return str;
}

struct object *unmarshal(struct object *dba) {
  char t;
  TC("unmarshal", 0, dba, type_dynamic_byte_array);
  t = dynamic_byte_array_get(dba, 0);
  switch (t) {
    case type_dynamic_byte_array:
      return NULL;
    case type_dynamic_array:
      return NULL;
    case type_cons:
      return NULL;
    case type_fixnum:
      return unmarshal_fixnum(dba);
    case type_flonum:
      return NULL;
    case type_string:
      return unmarshal_string(dba);
    default:
      printf("BC: cannot unmarshal type %s.", get_type_name(t));
      return NULL;
  }
}

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
  switch (o->w0.type) {
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
             get_type_name(o->w0.type));
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

struct object *read_bytecode_file(FILE *file) {
  /*
  struct object *header;
  struct object *bc;
  */

  return NULL;
  /* read the header - TODO: make sure the file is large enough to read the file
   */
  /*
  memcpy(&header, file, sizeof(struct bytecode_file_header));

  file = &file[sizeof(
      struct bytecode_file_header)];

  if (header.magic[0] != 'b' && header.magic[1] != 'u' &&
      header.magic[2] != 'g' && header.magic[3] != 0) {
    printf("BC: Invalid magic string\n");
    exit(1);
  }

  if (header.version != BC_VERSION) {
    printf("BC: Invalid version (interpreter is %d, bytecode is %d).\n",
           BC_VERSION, header.version);
    exit(1);
  }

  bc = unmarshal(file);

  if (bc->w0.type != type_bytecode) {
    printf(
        "BC: Bytecode file requires a marshalled bytecode object immediately "
        "after the header, but found a %s.\n",
        get_type_name(bc->w0.type));
    exit(1);
  }

  return bc;
  */
}

void run_tests() {
  struct object *darr;

  printf("Running tests...\n");

  /*
   * Strings 
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
  assert(DYNAMIC_ARRAY_CAPACITY(darr) == 3);

  /* 
   * Marshaling/Unmarshaling
   */

  /* Fixnum marshaling */
  /* no bytes */
  assert(FIXNUM_VALUE(unmarshal_fixnum(marshal_fixnum(fixnum(0)))) == 0);
  /* one byte */
  assert(FIXNUM_VALUE(unmarshal_fixnum(marshal_fixnum(fixnum(9)))) == 9);
  assert(FIXNUM_VALUE(unmarshal_fixnum(marshal_fixnum(fixnum(-23)))) == -23);
  /* two bytes */
  assert(FIXNUM_VALUE(unmarshal_fixnum(marshal_fixnum(fixnum(256)))) == 256);
  assert(FIXNUM_VALUE(unmarshal_fixnum(marshal_fixnum(fixnum(257)))) == 257);
  assert(FIXNUM_VALUE(unmarshal_fixnum(marshal_fixnum(fixnum(-342)))) == -342);
  assert(FIXNUM_VALUE(unmarshal_fixnum(marshal_fixnum(fixnum(2049)))) == 2049);
  /* three bytes */
  assert(FIXNUM_VALUE(unmarshal_fixnum(marshal_fixnum(fixnum(123456)))) == 123456);
  assert(FIXNUM_VALUE(unmarshal_fixnum(marshal_fixnum(fixnum(-123499)))) == -123499);
  assert(FIXNUM_VALUE(unmarshal_fixnum(marshal_fixnum(fixnum(20422)))) == 20422);
  /* four bytes */
  assert(FIXNUM_VALUE(unmarshal_fixnum(marshal_fixnum(fixnum(123456789)))) == 123456789);
  assert(FIXNUM_VALUE(unmarshal_fixnum(marshal_fixnum(fixnum(-123456789)))) == -123456789);

  /* String marshaling */
  assert(strcmp(bstring_to_cstring(unmarshal_string(marshal_string(string("abcdef")))), "abcdef") == 0);
  assert(strcmp(bstring_to_cstring(unmarshal_string(marshal_string(string("nerEW)F9nef09n230(N#EOINWEogiwe")))), "nerEW)F9nef09n230(N#EOINWEogiwe") == 0);
  assert(strcmp(bstring_to_cstring(unmarshal_string(marshal_string(string("")))), "") == 0);
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
  dynamic_array_push(consts, fixnum(123456));

  bc = bytecode(code, consts);

  eval(g, bc);

  assert(strcmp(bstring_to_cstring(peek(g)), "ab") == 0);

  file = open_file(string("file.txt"), string("wb"));
  write_bytecode_file(file, bc);
  close_file(file);

  return 0;
}