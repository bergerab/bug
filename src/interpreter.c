/** @file interpreter.c
 *
 * The Bug Bytecode interpreter
 */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bytecode.h"
#include "debug.h"

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

  i = 0;
  length_fix = length->w1.value.fixnum;

  o = object(type_byte_array);
  NC(o, "Failed to allocate byte-array object.");
  o->w1.value.byte_array = malloc(sizeof(struct byte_array));
  NC(o->w1.value.byte_array, "Failed to allocate byte-array.");
  o->w1.value.byte_array->length =
      fixnum(length_fix); /* create a new fix the given one could be mutated */
  o->w1.value.byte_array->bytes = malloc(length_fix * sizeof(uint8_t));
  NC(o->w1.value.byte_array->bytes, "Failed to allocate byte-array bytes.");
  /* fill the byte-array with nulls */
  for (; i < length_fix; ++i) o->w1.value.byte_array->bytes[i] = 0;
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

/* (7F)_16 is (0111 1111)_2, it extracts the numerical value from the temporary */
/* (80)_16 is (1000 0000)_2, it extracts the flag from the temporary */
#define READ_OP_ARG()                                                          \
  if (i >= byte_count) {                                                       \
    printf("Bug Bytecode: expected an op code argument, but bytecode ended."); \
    break;                                                                     \
  }                                                                            \
  t0 = code->bytes[++i];                                                       \
  a0 = t0 & 0x7F;                                                              \
  while (t0 & 0x80) {                                                          \
    if (i >= byte_count) {                                                     \
      printf(                                                                  \
          "Bug Bytecode: expected an extended op code argument, but bytecode " \
          "ended.");                                                           \
      break;                                                                   \
    }                                                                          \
    t0 = code->bytes[++i];                                                     \
    a0 = (a0 << 7) | (t0 & 0x7F);                                              \
  }

#define READ_CONST_ARG()                                                     \
  READ_OP_ARG()                                                              \
  if (a0 >= constants_length) {                                              \
    printf(                                                                  \
        "Bug Bytecode: bytecode attempted to access an out of bounds index " \
        "%d in the constants vector, but only have %d constants.",           \
        a0, constants_length);                                               \
    break;                                                                   \
  }                                                                          \
  c0 = constants->values[a0];

void eval(struct gis *g, struct object *bc) {
  fixnum_t i, /* the current byte being evaluated */
        byte_count; /* number of  bytes in this bytecode */
  uint8_t t0; /* temporary for reading bytecode arguments */
  fixnum_t a0; /* the arguments for bytecode parameters */
  struct object *v0, *v1; /* temps for values popped off the stack */
  struct object *c0; /* temps for constants (used for bytecode arguments) */
  struct byte_array *code; /* the byte array containing the bytes in the bytecode */
  struct array *constants; /* the constants array */
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
        ++i;
        READ_CONST_ARG();
        push(g, c0);
        break;
      default:
        printf("No cases matched");
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

int main() {
  struct object *bc, *code, *consts;
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
  assert(1);

  printf("Done");

  return 0;
}