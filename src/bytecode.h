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
#define SC(name, n) stack_check(name, n, UFIXNUM_VALUE(gis->i))
#else
#define TC(name, argument, o, type) \
  {}
#define TC2(name, argument, o, type0, type1) \
  {}
#define SC(name, n) \
  {}
#endif

#define NIL gis->nil_symbol
#define T gis->t_symbol

#define NC(x, message) \
  if (x == NULL) {     \
    printf(message);   \
    return NULL;       \
  }

#define SF_REQ_N(n, cons)                                                      \
  if (count(CONS_CDR(cons)) != n) {                                             \
    printf("Special form \"%s\" takes %d arguments, but was given %d.",        \
           bstring_to_cstring(SYMBOL_NAME(CONS_CAR(cons))), n, (int)count(CONS_CDR(cons))); \
    exit(1);                                                                   \
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

#define SYMBOL_PLIST(o)\
  o->w1.value.symbol->plist

#define SYMBOL_PACKAGE(o)\
  o->w1.value.symbol->package

#define SYMBOL_IS_EXTERNAL(o)\
  o->w1.value.symbol->is_external

#define SYMBOL_VALUE_IS_SET(o)\
  o->w1.value.symbol->value_is_set

#define SYMBOL_FUNCTION_IS_SET(o)\
  o->w1.value.symbol->function_is_set

#define SYMBOL_VALUE(o)\
  o->w1.value.symbol->value

#define SYMBOL_FUNCTION(o)\
  o->w1.value.symbol->function

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

#define FUNCTION_CONSTANTS(o)\
  o->w1.value.function->constants

#define FUNCTION_CODE(o)\
  o->w1.value.function->code

#define FUNCTION_STACK_SIZE(o)\
  o->w1.value.function->stack_size

#define FUNCTION_NARGS(o)\
  o->w1.value.function->nargs

#define FUNCTION_NAME(o)\
  o->w1.value.function->name

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

#define VEC2_X(o)\
 o->w1.value.vec2->x

#define VEC2_Y(o)\
 o->w1.value.vec2->y

#define ENUMERATOR_VALUE(o)\
 o->w1.value.enumerator->value

#define ENUMERATOR_SOURCE(o)\
 o->w1.value.enumerator->source

#define ENUMERATOR_INDEX(o)\
 o->w1.value.enumerator->index

#define PACKAGE_NAME(o)\
 o->w1.value.package->name

#define PACKAGE_SYMBOLS(o)\
 o->w1.value.package->symbols

#define PACKAGE_PACKAGES(o)\
 o->w1.value.package->packages

#define FUNCTION_NAME(o)\
  o->w1.value.function->name

#define BC_VERSION 1

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
  op_const,
  op_print,
  op_print_nl,
  op_eq,
  op_and,
  op_or,
  op_not,
  op_gt,
  op_gte,
  op_lt,
  op_lte,
  op_set_symbol_value,
  op_set_symbol_function,
  op_symbol_value,
  op_symbol_function,
  op_jump,
  op_jump_when_nil,
  op_load_from_stack,
  op_store_to_stack,
  op_call_function,
  op_return_function
};

/* The types defined below are not the same as the types that will
   be defined within the language. */
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
  type_function = 34,
  type_file = 38,
  type_enumerator = 42,
  type_nil = 46, /* type_nil won't appear on an object, only from calls to
                   get_object_type(...) */
  type_record = 50,
  type_vec2 = 54
};

union value {
  fixnum_t fixnum;
  ufixnum_t ufixnum;
  flonum_t flonum;
  struct dynamic_array *dynamic_array;
  struct dynamic_byte_array *dynamic_byte_array;
  struct symbol *symbol;
  struct function *function;
  struct package *package;
  struct enumerator *enumerator;
  struct file *file;
  struct record *record;
  struct vec2 *vec2;
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
  struct object *package; /** the package this symbol is defined in (the "home package") */
  struct object *value; /** the value slot */
  struct object *function; /** the function value slot */
  struct object *plist; /** a plist that maps from namespace name to value */
  char is_external;
  char value_is_set;
  char function_is_set;
};

struct package {
  struct object *name; /** the name of the package (a string) */
  struct object *symbols; /** all top-level symbols the package has created (a cons list) */
  struct object *packages; /** packages this package uses -- all external symbols of these packages become internal symbols (can be exported) */
};

struct function {
  struct object *name; /** a symbol - optional */
  struct object *code; /** the code (a byte-array) */
  struct object *constants; /** an array of constants used within the code (an array) */
  ufixnum_t stack_size; /** how many items to reserve on the stack - includes */
  ufixnum_t nargs; /** how many arguments does this require? */
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

struct record {
  struct object *type; /** a type descriptor */
  struct object **fields;
};

/* two unboxed floats - an optimization over using just a cons or dynamic array */
struct vec2 {
  flonum_t x;
  flonum_t y;
};

/**
 * The global interpreter state.
 */
struct gis {
  struct object *stack; /** the data stack (a cons list) */
  struct object *call_stack; /** stack for saving stack pointers and values for function calls (a cons list) */

  struct object *i; /** the index of the next instruction in bc to execute */
  struct object *f; /** the currently executing function */

  struct object *sp; /** the call stack pointer (a ufixnum) */
  struct object *package; /** the current package being evaluated */
  struct object *packages; /** all packages */

  struct object *interned_strings; /** maybe it shouldn't be in the global interpreter state (originally for marshaling/unmarshaling), 
                                       but it was easier to put it here.
                                       used for any internal strings that shouldn't be repeated in function
                                       for example, frequent use of the same symbol name, or package name would
                                       be put here. */

  struct object *keyword_package;
  struct object *lisp_package;
  struct object *user_package;
  struct object *impl_package;

  /* keywords */
  struct object *value_keyword;
  struct object *function_keyword;
  struct object *internal_keyword;
  struct object *external_keyword;
  struct object *inherited_keyword;

  /* symbols from lisp package */
  struct object *car_symbol;
  struct object *cdr_symbol;
  struct object *symbol_value_symbol;
  struct object *symbol_function_symbol;
  struct object *set_symbol;
  struct object *quote_symbol;
  struct object *cons_symbol;
  struct object *progn_symbol;
  struct object *add_symbol;
  struct object *sub_symbol;
  struct object *mul_symbol;
  struct object *div_symbol;
  struct object *gt_symbol;
  struct object *lt_symbol;
  struct object *gte_symbol;
  struct object *lte_symbol;
  struct object *print_symbol;
  struct object *and_symbol;
  struct object *or_symbol;
  struct object *equals_symbol;
  struct object *function_symbol;
  struct object *let_symbol;
  struct object *nil_symbol;
  struct object *t_symbol;
  struct object *if_symbol;

  /* cached strings that should be used internally */
  struct object *value_string;
  struct object *internal_string;
  struct object *external_string;
  struct object *inherited_string;

  struct object *user_string;
  struct object *lisp_string;
  struct object *keyword_string;
  struct object *impl_string;

  struct object *car_string;
  struct object *cdr_string;
  struct object *symbol_value_string;
  struct object *symbol_function_string;
  struct object *set_string;
  struct object *quote_string;
  struct object *cons_string;
  struct object *progn_string;
  struct object *add_string;
  struct object *sub_string;
  struct object *mul_string;
  struct object *div_string;
  struct object *gt_string;
  struct object *lt_string;
  struct object *gte_string;
  struct object *lte_string;
  struct object *print_string;
  struct object *and_string;
  struct object *or_string;
  struct object *equals_string;
  struct object *function_string;
  struct object *let_string;
  struct object *nil_string;
  struct object *t_string;
  struct object *if_string;

  struct object *var_string;
  struct object *x_string;
  struct object *y_string;
  struct object *a_string;
  struct object *b_string;
  struct object *temp_string;
  struct object *list_string;

  /* symbols from impl package */
  struct object *push_symbol;
  struct object *drop_symbol;
  struct object *pop_symbol;

  struct object *push_string;
  struct object *drop_string;
  struct object *pop_string;
};

enum marshaled_type {
  marshaled_type_integer,
  marshaled_type_negative_integer,
  marshaled_type_float,
  marshaled_type_negative_float,
  marshaled_type_symbol,
  marshaled_type_uninterned_symbol, /** a symbol with no home package */
  marshaled_type_string,
  marshaled_type_nil,
  marshaled_type_cons,
  marshaled_type_dynamic_array,
  marshaled_type_dynamic_string_array,
  marshaled_type_dynamic_byte_array,
  marshaled_type_function,
  marshaled_type_vec2
};