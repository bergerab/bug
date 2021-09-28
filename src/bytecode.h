#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>

#define RUN_TIME_CHECKS

#define DEFAULT_INITIAL_CAPACITY 100

#ifdef RUN_TIME_CHECKS
#define TC(name, argument, o, type) type_check(name, argument, o, type)
#define TC2(name, argument, o, type0, type1) type_check_or2(name, argument, o, type0, type1)
#define SC(name, n) stack_check(name, n)
#else
#define TC(name, argument, o, type) \
  {}
#define TC2(name, argument, o, type0, type1) \
  {}
#define SC(name, n) \
  {}
#endif

#define NC(x, message) \
  if (x == NULL) {     \
    printf(message);   \
    return NULL;       \
  }

typedef int64_t fixnum_t;
typedef uint64_t ufixnum_t;
typedef double flonum_t;

#define MAX_UFIXNUM UINT64_MAX

#define OBJECT_TYPE(o)\
  o->w0.type

#define FIXNUM_VALUE(o)\
 o->w1.value.fixnum

#define UFIXNUM_VALUE(o)\
 o->w1.value.ufixnum

#define FLONUM_VALUE(o)\
 o->w1.value.flonum

#define SYMBOL_NAME(o)\
  o->w1.value.symbol->name

#define SYMBOL_VALUES(o)\
  o->w1.value.symbol->values

#define ARRAY_LENGTH(o)\
  o->w1.value.array->length

#define ARRAY_VALUES(o)\
  o->w1.value.array->values

#define CONS_CAR(o)\
  o->w0.car

#define CONS_CDR(o)\
  o->w1.cdr

#define DYNAMIC_BYTE_ARRAY_LENGTH(o)\
  o->w1.value.dynamic_byte_array->length

#define DYNAMIC_BYTE_ARRAY_CAPACITY(o)\
  o->w1.value.dynamic_byte_array->capacity

#define DYNAMIC_BYTE_ARRAY_BYTES(o)\
  o->w1.value.dynamic_byte_array->bytes

#define DYNAMIC_ARRAY_LENGTH(o)\
  o->w1.value.dynamic_array->length

#define DYNAMIC_ARRAY_CAPACITY(o)\
  o->w1.value.dynamic_array->capacity

#define DYNAMIC_ARRAY_VALUES(o)\
  o->w1.value.dynamic_array->values

#define BYTECODE_CONSTANTS(o)\
  o->w1.value.bytecode->constants

#define BYTECODE_CODE(o)\
  o->w1.value.bytecode->code

#define STRING_LENGTH(o)\
  DYNAMIC_BYTE_ARRAY_LENGTH(o)

#define STRING_CONTENTS(o)\
  DYNAMIC_BYTE_ARRAY_BYTES(o)

#define FILE_FP(o)\
 o->w1.value.file->fp

#define FILE_MODE(o)\
 o->w1.value.file->mode

#define FILE_PATH(o)\
 o->w1.value.file->path

#define ENUMERATOR_VALUE(o)\
 o->w1.value.enumerator->value

#define ENUMERATOR_SOURCE(o)\
 o->w1.value.enumerator->source

#define ENUMERATOR_INDEX(o)\
 o->w1.value.enumerator->index

#define BC_VERSION 2

enum ops {
  op_drop,
  op_dup,
  op_intern,
  op_dynamic_array,
  op_dynamic_array_get,
  op_dynamic_array_set,
  op_dynamic_array_push,
  op_dynamic_array_pop,
  op_dynamic_array_concat,
  op_dynamic_byte_array_get,
  op_dynamic_byte_array_set,
  op_dynamic_byte_array_push,
  op_dynamic_byte_array_pop,
  op_dynamic_byte_array_concat,
  op_cons,
  op_car,
  op_cdr,
  op_add,
  op_sub,
  op_mul,
  op_div,
  op_const
};

/* The types defined below are not the same as the types that will
   be defined within the language. */
/* keeps the two right most bits as 0 to keep it as a flag */
enum type {
  type_cons = 0,    /* ... 0000 0000 */
  type_fixnum = 2,  /* ... 0000 0010 */
  type_ufixnum = 6, /* ... 0000 0110 */
  type_flonum = 10, /* ... 0000 1010 */
  type_symbol = 14, /* ... 0001 1110 */
  type_dynamic_array =
      18, /* etc... the lsb is always 0, the second lsb is always 1 */
  type_string = 22,
  type_package = 26,
  type_dynamic_byte_array = 30,
  type_bytecode = 34,
  type_file = 38,
  type_enumerator = 42,
  type_nil = 46 /* type_nil won't appear on an object, only from calls to
                   get_object_type(...) */
};

union value {
  fixnum_t fixnum;
  ufixnum_t ufixnum;
  flonum_t flonum;
  struct dynamic_array *dynamic_array;
  struct dynamic_byte_array *dynamic_byte_array;
  struct symbol *symbol;
  struct bytecode *bytecode;
  struct package *package;
  struct enumerator *enumerator;
  struct file *file;
};

union w0 { /** The first word of an object. */
  enum type type;
  struct object *car;
};

union w1 { /** The second word of an object */
  union value value;
  struct object *cdr;
};

struct object {
  union w0 w0;
  union w1 w1;
};

struct dynamic_array {
  ufixnum_t length; /** the number of items in the array (a fixnum) */
  ufixnum_t capacity;
  struct object **values; /** the contents of the array (arbitrary objects) */
};

struct dynamic_byte_array {
  ufixnum_t length; /** the number of items in the byte-array (a fixnum) */
  ufixnum_t capacity;
  char *bytes; /** the contents of the byte-array */
};

struct symbol {
  struct object *name; /** the name of the symbol (a string) */
  struct object *values; /** an alist that maps from namespace name to value (a cons list) */
};

struct package {
  struct object *name; /** the name of the package (a string) */
  struct object *symbols; /** all top-level symbols the package has created (a cons list) */
};

struct bytecode {
  struct object *code; /** the code (a byte-array) */
  struct object *constants; /** an array of constants used within the code (an array) */
};

struct file {
  struct object *path; /** the path to the file that was opened */
  struct object *mode; /** the mode the file was opened as */
  FILE *fp;            /** the file pointer */
};

struct enumerator {
  struct object *source;
  struct object *value;
  fixnum_t index;
};

/**
 * The global interpreter state.
 */
struct gis {
  struct object *stack; /** the data stack (a cons list) */
  struct object *package; /** the current package being evaluated */
};

enum marshaled_type {
  marshaled_type_integer,
  marshaled_type_float,
  marshaled_type_symbol,
  marshaled_type_string,
  marshaled_type_nil,
  marshaled_type_cons,
  marshaled_type_dynamic_array,
  marshaled_type_dynamic_byte_array,
  marshaled_type_bytecode
};