#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>

/* for DLL support: */
#include <windows.h>
#include <winbase.h>
#include <windef.h>

#include <ffi.h>

#define DEBUG 1

#define IF_DEBUG() if (DEBUG)

#define RUN_TIME_CHECKS

#define DEFAULT_INITIAL_CAPACITY 100

#ifdef RUN_TIME_CHECKS
#define TC(name, argument, o, type) type_check(name, argument, o, type)
#define TC2(name, argument, o, type0, type1) type_check_or2(name, argument, o, type0, type1)
#define SC(name, n) stack_check(name, n, UFIXNUM_VALUE(i))
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

#define OBJECT_TYPE(o) o->w0.type

#define FIXNUM_VALUE(o) o->w1.value.fixnum

#define UFIXNUM_VALUE(o) o->w1.value.ufixnum

#define FLONUM_VALUE(o) o->w1.value.flonum

#define OBJECT_POINTER(o) o->w1.value.ptr

#define SYMBOL_NAME(o) o->w1.value.symbol->name
#define SYMBOL_PLIST(o) o->w1.value.symbol->plist
#define SYMBOL_PACKAGE(o) o->w1.value.symbol->package
#define SYMBOL_IS_EXTERNAL(o) o->w1.value.symbol->is_external
#define SYMBOL_VALUE_IS_SET(o) o->w1.value.symbol->value_is_set
#define SYMBOL_FUNCTION_IS_SET(o) o->w1.value.symbol->function_is_set
#define SYMBOL_STRUCTURE_IS_SET(o) o->w1.value.symbol->structure_is_set
#define SYMBOL_VALUE(o) o->w1.value.symbol->value
#define SYMBOL_FUNCTION(o) o->w1.value.symbol->function
#define SYMBOL_STRUCTURE(o) o->w1.value.symbol->structure

#define ARRAY_LENGTH(o) o->w1.value.array->length
#define ARRAY_VALUES(o) o->w1.value.array->values

#define CONS_CAR(o) o->w0.car
#define CONS_CDR(o) o->w1.cdr

#define DYNAMIC_BYTE_ARRAY_LENGTH(o) o->w1.value.dynamic_byte_array->length
#define DYNAMIC_BYTE_ARRAY_CAPACITY(o) o->w1.value.dynamic_byte_array->capacity
#define DYNAMIC_BYTE_ARRAY_BYTES(o) o->w1.value.dynamic_byte_array->bytes
#define DYNAMIC_ARRAY_LENGTH(o) o->w1.value.dynamic_array->length
#define DYNAMIC_ARRAY_CAPACITY(o) o->w1.value.dynamic_array->capacity
#define DYNAMIC_ARRAY_VALUES(o) o->w1.value.dynamic_array->values

#define FUNCTION_CONSTANTS(o) o->w1.value.function->constants
#define FUNCTION_CODE(o) o->w1.value.function->code
#define FUNCTION_STACK_SIZE(o) o->w1.value.function->stack_size
#define FUNCTION_NARGS(o) o->w1.value.function->nargs
#define FUNCTION_NAME(o) o->w1.value.function->name
#define FUNCTION_IS_BUILTIN(o) o->w1.value.function->is_builtin
#define FUNCTION_IS_MACRO(o) o->w1.value.function->is_macro

#define STRING_LENGTH(o) DYNAMIC_BYTE_ARRAY_LENGTH(o)
#define STRING_CONTENTS(o) DYNAMIC_BYTE_ARRAY_BYTES(o)

#define FILE_FP(o) o->w1.value.file->fp
#define FILE_MODE(o) o->w1.value.file->mode
#define FILE_PATH(o) o->w1.value.file->path

#define VEC2_X(o) o->w1.value.vec2->x
#define VEC2_Y(o) o->w1.value.vec2->y

#define ENUMERATOR_VALUE(o) o->w1.value.enumerator->value
#define ENUMERATOR_SOURCE(o) o->w1.value.enumerator->source
#define ENUMERATOR_INDEX(o) o->w1.value.enumerator->index

#define PACKAGE_NAME(o) o->w1.value.package->name
#define PACKAGE_SYMBOLS(o) o->w1.value.package->symbols
#define PACKAGE_PACKAGES(o) o->w1.value.package->packages

#define DLIB_PATH(o) o->w1.value.dlib->path
#define DLIB_PTR(o) o->w1.value.dlib->ptr

#define STRUCTURE_NAME(o) o->w1.value.structure->name
#define STRUCTURE_FIELDS(o) o->w1.value.structure->fields
#define STRUCTURE_TYPE(o) o->w1.value.structure->type
#define STRUCTURE_NFIELDS(o) o->w1.value.structure->nfields
#define STRUCTURE_OFFSETS(o) o->w1.value.structure->offsets

#define FFUN_FFNAME(o) o->w1.value.ffun->ffname
#define FFUN_DLIB(o) o->w1.value.ffun->dlib
#define FFUN_PTR(o) o->w1.value.ffun->ptr
#define FFUN_RET(o) o->w1.value.ffun->ret
#define FFUN_PARAMS(o) o->w1.value.ffun->params
#define FFUN_CIF(o) o->w1.value.ffun->cif
#define FFUN_ARGTYPES(o) o->w1.value.ffun->arg_types
#define FFUN_NARGS(o) o->w1.value.ffun->nargs

/* don't allow more than 255 arguments (an arbitrary number). This is to avoid having to malloc when making the arg_types/arg_values arrays -- they can be made on the stack. */
#define MAX_FFI_NARGS 255

#define BC_VERSION 1

enum ops {
  op_drop,
  op_dup,
  op_intern,
  op_cons,
  op_car,
  op_cdr,
  op_add,
  op_addi,
  op_sub,
  op_subi,
  op_mul,
  op_div,
  op_list,
  op_load_nil,
  op_const,
  op_const_0,
  op_const_1,
  op_const_2,
  op_const_3,
  op_push_arg,
  op_push_args,
  op_print,
  op_print_nl,
  op_eq,
  op_and,
  op_or,
  op_not,
  op_gt,
  op_gte,
  op_lt,
  op_lti,
  op_lte,
  op_set_symbol_value,
  op_set_symbol_function,
  op_symbol_value,
  op_symbol_function,
  op_jump,
  op_jump_when_nil,
  op_load_from_stack,
  op_load_from_stack_0,
  op_load_from_stack_1,
  op_store_to_stack,
  op_store_to_stack_0,
  op_store_to_stack_1,
  op_call_function,
  op_call_symbol_function,
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
  type_vec2 = 54,
  type_dlib = 58, /** dynamic library */
  type_ffun = 62, /** foreign function (function from a dynamic library) */
  type_struct = 66, /** used in FFI */
  type_ptr = 70
};
/* ATTENTION! when adding a new type, make sure to update this define below!
   this defines the border between types defined as builtins and the user. */
#define HIGHEST_TYPE type_ptr

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
  struct structure *structure;
  struct vec2 *vec2;
  struct dlib *dlib;
  struct ffun *ffun;
  void *ptr;
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
  struct object *structure; /** the struct value slot */
  struct object *plist; /** a plist that maps from namespace name to value */
  char is_external;
  char value_is_set;
  char function_is_set;
  char structure_is_set;
};

struct package {
  struct object *name; /** the name of the package (a string) */
  struct object *symbols; /** all top-level symbols the package has created (a cons list) */
  struct object *packages; /** packages this package uses -- all external symbols of these packages become internal symbols (can be exported) */
};

struct structure {
  struct object *name;
  struct object *fields;
  ffi_type *type;
  size_t *offsets;
  ufixnum_t nfields;
};

struct dlib {
  struct object *path; /** path to the dynamic library (a string) */
  HINSTANCE ptr;
};

struct ffun {
  struct object *ffname; /** the foreign function name */
  struct object *dlib; /** the dynamic library this function is from */
  struct object *ret; /** the return type */
  struct object *params; /** the type parameters this function takes */
  FARPROC ptr; /* a pointer to the foreign function */
  ffi_cif *cif;
  ffi_type **arg_types;
  ufixnum_t nargs;
};

struct function {
  struct object *name; /** a symbol - optional */
  struct object *code; /** the code (a byte-array) */
  struct object *constants; /** an array of constants used within the code (an array) */
  ufixnum_t stack_size; /** how many items to reserve on the stack - includes arguments */
  ufixnum_t nargs; /** how many arguments does this require? */
  char is_builtin; /** is this a builtin function? */
  char is_macro; /** is this a macro? */
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
  struct object *data_stack; /** same as the value in data_stack_symbol */
  struct object *data_stack_symbol; /** the data stack (a cons list) */

  struct object *call_stack; /** same as the value in call_stack_symbol */
  struct object *call_stack_symbol; /** stack for saving stack pointers and values for function calls (a cons list) */

  struct object *i_symbol; /** the index of the next instruction in bc to execute */
  struct object *f_symbol; /** the currently executing function */

  struct object *package_symbol; /** the current package being evaluated */
  struct object *packages_symbol; /** all packages */

  struct object *interned_strings; /** maybe it shouldn't be in the global interpreter state (originally for marshaling/unmarshaling), 
                                       but it was easier to put it here.
                                       used for any internal strings that shouldn't be repeated in function
                                       for example, frequent use of the same symbol name, or package name would
                                       be put here. */

  struct object *structures;

  struct object *cons_type;
  struct object *fixnum_type;
  struct object *ufixnum_type;
  struct object *flonum_type;
  struct object *symbol_type;
  struct object *dynamic_array_type;
  struct object *string_type;
  struct object *package_type;
  struct object *dynamic_byte_array_type;
  struct object *function_type;
  struct object *file_type;
  struct object *enumerator_type;
  struct object *nil_type;
  struct object *record_type;
  struct object vec2_type;
  struct object dlib_type;
  struct object ffun_type;
  struct object struct_type;
  struct object ptr_type;

  struct object *keyword_package;
  struct object *lisp_package;
  struct object *user_package;
  struct object *impl_package;
  struct object *ffi_package;

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
  struct object *set_symbol_function_symbol;
  struct object *quote_symbol;
  struct object *unquote_symbol;
  struct object *unquote_splicing_symbol;
  struct object *quasiquote_symbol;
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

  struct object *fixnum_symbol;
  struct object *ufixnum_symbol;
  struct object *flonum_symbol;
  struct object *string_symbol;
  struct object *symbol_symbol;

  struct object *fixnum_string;
  struct object *ufixnum_string;
  struct object *flonum_string;
  struct object *string_string;
  struct object *symbol_string;

  struct object *strings_symbol;
  struct object *find_package_symbol;
  struct object *use_package_symbol;
  struct object *package_symbols_symbol; 

  struct object *compile_symbol;
  struct object *compile_string;
  struct object *compile_builtin;

  struct object *eval_symbol;
  struct object *eval_string;
  struct object *eval_builtin;

  struct object *macro_symbol;
  struct object *macro_string;
  struct object *macro_builtin;

  struct object *list_symbol;
  struct object *list_string;

  struct object *call_symbol;
  struct object *call_string;
  struct object *call_builtin;

  struct object *dynamic_library_symbol;
  struct object *dynamic_library_string;
  struct object *dynamic_library_builtin;

  struct object *foreign_function_symbol;
  struct object *foreign_function_string;
  struct object *foreign_function_builtin;

  struct object *type_of_symbol;
  struct object *type_of_string;
  struct object *type_of_builtin;

  /* FFI */
  struct object *ffi_string;

  struct object *ffi_void_symbol;
  struct object *ffi_void_string;

  struct object *ffi_ptr_symbol;
  struct object *ffi_ptr_string;

  struct object *ffi_char_symbol;
  struct object *ffi_char_string;

  struct object *ffi_int_symbol;
  struct object *ffi_int_string;

  struct object *ffi_struct_symbol;
  struct object *ffi_struct_string;

  struct object *ffi_uint_symbol;
  struct object *ffi_uint_string;

  struct object *ffi_uint8_symbol;
  struct object *ffi_uint8_string;

  struct object *struct_symbol;
  struct object *struct_string;
  struct object *struct_builtin;

  struct object *alloc_struct_symbol;
  struct object *alloc_struct_string;
  struct object *alloc_struct_builtin;

  struct object *get_struct_symbol;
  struct object *get_struct_string;
  struct object *get_struct_builtin;

  struct object *set_struct_symbol;
  struct object *set_struct_string;
  struct object *set_struct_builtin;

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
  struct object *set_symbol_function_string;
  struct object *quote_string;
  struct object *unquote_string;
  struct object *unquote_splicing_string;
  struct object *quasiquote_string;
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

  struct object *f_string;
  struct object *i_string;
  struct object *stack_string;
  struct object *call_stack_string;
  struct object *data_stack_string;

  struct object *package_string; 
  struct object *packages_string; 
  struct object *strings_string;
  struct object *find_package_string; 
  struct object *use_package_string; 
  struct object *package_symbols_string; 

  struct object *var_string;
  struct object *x_string;
  struct object *y_string;
  struct object *a_string;
  struct object *b_string;
  struct object *temp_string;

  /* symbols from impl package */
  struct object *push_symbol;
  struct object *drop_symbol;
  struct object *pop_symbol;

  struct object *push_string;
  struct object *drop_string;
  struct object *pop_string;

  struct object *use_package_builtin;
  struct object *find_package_builtin;
  struct object *package_symbols_builtin;
};

#define GIS_PACKAGE symbol_get_value(gis->package_symbol)

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