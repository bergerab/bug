/** @file interpreter.c
 *
 * The Bug Bytecode interpreter
 */
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bytecode.h"

#define RUN_TIME_CHECKS

#ifdef RUN_TIME_CHECKS
#define TC(name, argument, o, type) type_check(name, argument, o, type)
#define SC(g, name, n) stack_check(g, name, n)
#else
#define TC(name, argument, o, type) \
  {}
#define SC(g, name, n) \
  {}
#endif

#define NC(x, message) \
  if (x == NULL) {     \
    printf(message);   \
    return NULL;       \
  }

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
    case type_array:
      return "array";
    case type_package:
      return "package";
    case type_string:
      return "string";
    case type_bytecode:
      return "bytecode";
    case type_byte_array:
      return "byte-array";
    case type_file:
      return "file";
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

struct object *array(struct object *length) {
  fixnum_t i = 0;
  fixnum_t length_fix = length->w1.value.fixnum;
  struct object *o = object(type_array);
  NC(o, "Failed to allocate array object.");
  o->w1.value.array = malloc(sizeof(struct array));
  NC(o->w1.value.array, "Failed to allocate array.");
  /* create a new fix the given one could be mutated: */
  o->w1.value.array->length = fixnum(length_fix);
  o->w1.value.array->values = malloc(length_fix * sizeof(struct object));
  /* fill the array with nulls: */
  for (; i < length_fix; ++i) o->w1.value.array->values[i] = 0;
  return o;
}

struct object *array_ref(struct object *array, struct object *index) {
  TC("array_ref", 0, array, type_array);
  TC("array_ref", 1, index, type_fixnum);
  return array->w1.value.array->values[index->w1.value.fixnum];
}

struct object *byte_array(struct object *length) {
  fixnum_t i;
  fixnum_t length_fix;
  struct object *o;

  TC("byte_array", 0, length, type_fixnum);

  length_fix = length->w1.value.fixnum;

  o = object(type_byte_array);
  NC(o, "Failed to allocate byte-array object.");
  o->w1.value.byte_array = malloc(sizeof(struct byte_array));
  NC(o->w1.value.byte_array, "Failed to allocate byte-array.");
  o->w1.value.byte_array->length =
      fixnum(length_fix); /* create a new fix the given one could be mutated */
  o->w1.value.byte_array->bytes = malloc(length_fix * sizeof(char));
  NC(o->w1.value.byte_array->bytes, "Failed to allocate byte-array bytes.");
  /* fill the byte-array with nulls */
  for (i = 0; i < length_fix; ++i) o->w1.value.byte_array->bytes[i] = 0;
  return o;
}

struct object *bytecode(struct object *code, struct object *constants) {
  struct object *o;
  TC("bytecode", 0, code, type_byte_array);
  TC("bytecode", 1, constants, type_array);
  o = object(type_bytecode);
  NC(o, "Failed to allocate bytecode object.");
  o->w1.value.bytecode = malloc(sizeof(struct bytecode));
  NC(o->w1.value.bytecode, "Failed to allocate bytecode.");
  o->w1.value.bytecode->code = code;
  o->w1.value.bytecode->constants = constants;
  return o;
}

struct object *string(char *contents) {
  fixnum_t i = 0;
  struct object *o;
  fixnum_t length_fix = strlen(contents);
  struct object *length = fixnum(length_fix);
  NC(length, "Failed to allocate string object's length.");
  o = byte_array(length);
  o->w0.type = type_string;
  /* fill the byte-array with the characters from the string */
  for (; i < length_fix; ++i) o->w1.value.byte_array->bytes[i] = contents[i];
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
  o->w1.value.file->fp = fp;
  o->w1.value.file->mode = string(mode_cs);
  o->w1.value.file->path = string(path_cs);
  free(path_cs);
  free(mode_cs);
  return o;
}

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
  struct byte_array
      *code; /* the byte array containing the bytes in the bytecode */
  struct array *constants;   /* the constants array */
  fixnum_t constants_length; /* the length of the constants array */

  TC("eval", 1, bc, type_bytecode);
  i = 0;
  code = bc->w1.value.bytecode->code->w1.value.byte_array;
  constants = bc->w1.value.bytecode->constants->w1.value.array;
  constants_length = constants->length->w1.value.fixnum;
  byte_count = code->length->w1.value.fixnum;

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
      case op_array: /* array ( length -- array[length] ) */
        SC(g, "array", 1);
        v0 = pop(g); /* length */
        TC("array", 0, v0, type_fixnum);
        push(g, array(v0));
        break;
      case op_array_ref: /* array_ref ( array index -- object ) */
        SC(g, "array-ref", 2);
        v0 = pop(g); /* index */
        TC("array-ref", 0, v0, type_fixnum);
        v1 = pop(g); /* array */
        TC("array-ref", 1, v1, type_array);
        push(g, array_ref(v1, v0));
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

double log2(double n) { return log(n) / log(2); }

/* Marshaling  */
struct object *marshal(struct object *o);

/** returns how many bytes the given fixnum takes to store */
fixnum_t marshal_length_fixnum_t(fixnum_t n) {
  return ceil(log2(n) / 8) + 1;
}
fixnum_t write_marshaled_fixnum_t(char *buf, fixnum_t n) {
  uint8_t byte_count;
  uint8_t cursor;

  cursor = 0;
  byte_count = ceil(log2(n) / 8);
  buf[cursor++] = byte_count;

  while (n > 0) {
    buf[cursor++] = byte_count & 0xFF;
    n = n >> 8;
  }

  assert(marshal_length_fixnum_t(n) == cursor);
  return cursor;
}

/*
 *
 */
void write_marshaled_flonum_t(char *buf, flonum_t n) {
  memcpy(buf, &n, sizeof(flonum_t));
  /*fixnum_t i, j = 0;
  for (i = sizeof(fixnum_t)-1; i >= 0; ++i)
    buf[j++] = fixnum >> (8 * i); */
}

/**
 * turns bytecode into a byte-array that can be stored to a file
 */
struct object *marshal_bytecode(struct object *bc) {
  /*struct object *ba;*/
  TC("marshal_bytecode", 0, bc, type_bytecode);
  return NULL;
}

fixnum_t marshal_length_string(fixnum_t length) {
  return marshal_length_fixnum_t(type_string) + marshal_length_fixnum_t(length) + length;
}
struct object *marshal_string(struct object *str) {
  struct object *ba;
  fixnum_t length;
  char *buf;
  fixnum_t cursor;

  TC("marshal_string", 0, str, type_string);
  cursor = 0;
  length = STRING_LENGTH(str);
  ba = byte_array(fixnum(marshal_length_string(length)));
  buf = ba->w1.value.byte_array->bytes;

  cursor += write_marshaled_fixnum_t(&buf[cursor], type_string);
  cursor += write_marshaled_fixnum_t(&buf[cursor], length);
  memcpy(&buf[cursor], STRING_CONTENTS(str), length);

  return ba;
}

struct object *marshal_array(struct object *arr) {
  struct object *ba;
  fixnum_t arr_length;
  fixnum_t byte_count;
  fixnum_t i;
  /*fixnum_t cursor;*/
  struct object **c_arr;

  TC("marshal_array", 0, arr, type_array);

  arr_length = FIXNUM_VALUE(ARRAY_LENGTH(arr));
  c_arr = ARRAY_VALUES(arr);

  /* bad way of checking how many bytes needed for the buffer.
     a better way is to use a vector. */
  byte_count = 0;
  for (i = 0; i < arr_length; ++i) {
    byte_count += BYTE_ARRAY_LENGTH(marshal(c_arr[i]));
  }

  ba = byte_array(fixnum(byte_count));
  return ba;
}

fixnum_t marshal_length_fixnum(fixnum_t n) {
  return marshal_length_fixnum_t(type_fixnum) + marshal_length_fixnum_t(n);
}
struct object *marshal_fixnum(struct object *n) {
  struct object *ba;
  char *buf;
  fixnum_t cursor;

  TC("marshal_fixnum", 0, n, type_fixnum);
  cursor = 0;
  ba = byte_array(fixnum(marshal_length_fixnum(FIXNUM_VALUE(n))));
  buf = ba->w1.value.byte_array->bytes;

  cursor += write_marshaled_fixnum_t(buf, type_fixnum);
  write_marshaled_fixnum_t(&buf[cursor], FIXNUM_VALUE(n));

  return ba;
}

struct object *marshal_flonum(struct object *n) {
  struct object *ba;
  char *buf;
  fixnum_t cursor;

  TC("marshal_flonum", 0, n, type_fixnum);
  cursor = 0;
  ba = byte_array(fixnum(sizeof(fixnum_t) * 2));
  buf = ba->w1.value.byte_array->bytes;

  cursor += write_marshaled_fixnum_t(buf, type_flonum);
  write_marshaled_flonum_t(&buf[cursor], FLONUM_VALUE(n));

  return ba;
}

/**
 * Takes any object and returns a byte-array containing the binary
 * representation of the object.
 */
struct object *marshal(struct object *o) {
  switch (o->w0.type) {
    case type_byte_array:
      return NULL;
    case type_array:
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
      printf("BC: unmarshalable type %s.\n", get_type_name(o->w0.type));
      return NULL;
  }
}

struct object *unmarshal(FILE *file) {
  return NULL;
  /*
  switch (o->w0.type) {
    case type_byte_array:
      return NULL;
    case type_array:
      return NULL;
    case type_cons:
      return NULL;
    case type_fixnum:
      return NULL;
    case type_flonum:
      return NULL;
    case type_string:
      return NULL;
    default:
      printf("BC: unmarshalable type.");
      return NULL;
  }
  */
}

/**
 * Writes bytecode to a file for the given word size.
 * @param bc the bytecode to write
 * @param word_size_o the number of bytes in the target's machine word
 */
struct object *write_bytecode_file(struct object *file, struct object *bc) {
  struct bytecode_file_header header;
  struct object *ba;

  TC("write_bytecode_file", 0, bc, type_bytecode);

  header.magic[0] = 'b';
  header.magic[1] = 'u';
  header.magic[2] = 'g';
  header.magic[3] = 0;
  header.version = BC_VERSION;
  header.word_size = sizeof(fixnum_t);
  header.pad[0] = header.pad[1] = header.pad[2] = 0;
  header.pad[3] = header.pad[4] = header.pad[5] = 0;
  header.pad[6] = 0;

  /* write the header */
  fwrite(&header, sizeof(struct bytecode_file_header), 1, FILE_FP(file));

  /* write the bytecode object */
  ba = marshal(bc);
  fwrite(BYTE_ARRAY_BYTES(ba), sizeof(char), BYTE_ARRAY_LENGTH(ba),
         FILE_FP(file));
  return 0;
}

struct object *read_bytecode_file(FILE *file) {
  struct bytecode_file_header header;
  struct object *bc;

  /* read the header - TODO: make sure the file is large enough to read the file
   */
  memcpy(&header, file, sizeof(struct bytecode_file_header));

  file = &file[sizeof(
      struct bytecode_file_header)]; /* advance the file pointer to point at the
                                        beginning of unmarshalable data */

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

  /* write the bytecode object */
  bc = unmarshal(file);

  if (bc->w0.type != type_bytecode) {
    printf(
        "BC: Bytecode file requires a marshalled bytecode object immediately "
        "after the header, but found a %s.\n",
        get_type_name(bc->w0.type));
    exit(1);
  }

  return bc;
}

int main() {
  struct object *bc, *code, *consts, *file;
  struct gis *g;

  g = gis();
  g->package = package(string("user"));

  code = byte_array(fixnum(2));
  code->w1.value.byte_array->bytes[0] = op_const;
  code->w1.value.byte_array->bytes[1] = 0;

  consts = array(fixnum(2));
  consts->w1.value.array->values[0] = string("a");
  consts->w1.value.array->values[1] = fixnum(4);
  bc = bytecode(code, consts);

  eval(g, bc);

  file = open_file(string("myfile.txt"), string("wb"));
  write_bytecode_file(file, bc);

  assert(strcmp(bstring_to_cstring(string("asdf")), "asdf") == 0);

  return 0;
}