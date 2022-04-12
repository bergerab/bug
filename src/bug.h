#ifndef _BUG_H
#define _BUG_H

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

#define DEFAULT_INITIAL_CAPACITY 100

#define PRINT_STACK_TRACE_AND_QUIT() \
  print_stack();                     \
  exit(1);

#define NIL gis->nil
#define T gis->t

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

#define CSYM(package_name, symbol_name) \
  c_intern(symbol_name, package_name);

#define CPACK(package_name) \
  c_find_package(package_name);

#define CSYM_VAL(package_name, symbol_name) \
  symbol_get_value(c_intern(symbol_name, package_name))

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
#define SYMBOL_TYPE_IS_SET(o) o->w1.value.symbol->type_is_set
#define SYMBOL_VALUE(o) o->w1.value.symbol->value
#define SYMBOL_FUNCTION(o) o->w1.value.symbol->function
#define SYMBOL_TYPE(o) o->w1.value.symbol->type

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
#define FUNCTION_DOCSTRING(o) o->w1.value.function->docstring
#define FUNCTION_IS_BUILTIN(o) o->w1.value.function->is_builtin
#define FUNCTION_ACCEPTS_ALL(o) o->w1.value.function->accepts_all
#define FUNCTION_IS_MACRO(o) o->w1.value.function->is_macro

#define STRING_LENGTH(o) DYNAMIC_BYTE_ARRAY_LENGTH(o)
#define STRING_CONTENTS(o) ((char*)DYNAMIC_BYTE_ARRAY_BYTES(o))

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

#define DLIB_PATH(o) o->w1.value.dlib->path
#define DLIB_PTR(o) o->w1.value.dlib->ptr

#define TYPE_NAME(o) o->w1.value.type->name
#define TYPE_ID(o) o->w1.value.type->id
#define TYPE_BUILTIN(o) o->w1.value.type->builtin
#define TYPE_FFI_TYPE(o) o->w1.value.type->ffi_type
#define TYPE_STRUCT_FIELD_NAMES(o) o->w1.value.type->struct_field_names
#define TYPE_STRUCT_FIELD_TYPES(o) o->w1.value.type->struct_field_types
#define TYPE_STRUCT_NFIELDS(o) o->w1.value.type->struct_nfields
#define TYPE_STRUCT_OFFSETS(o) o->w1.value.type->struct_offsets

#define FFUN_FFNAME(o) o->w1.value.ffun->ffname
#define FFUN_DLIB(o) o->w1.value.ffun->dlib
#define FFUN_PTR(o) o->w1.value.ffun->ptr
#define FFUN_RET_TYPE(o) o->w1.value.ffun->ret_type
#define FFUN_FFI_RET_TYPE(o) o->w1.value.ffun->ffi_ret_type
#define FFUN_PARAM_TYPES(o) o->w1.value.ffun->param_types
#define FFUN_CIF(o) o->w1.value.ffun->cif
#define FFUN_ARGTYPES(o) o->w1.value.ffun->arg_types
#define FFUN_NARGS(o) o->w1.value.ffun->nargs

/* don't allow more than 255 arguments (an arbitrary number). This is to avoid having to malloc when making the arg_types/arg_values arrays -- they can be made on the stack. */
#define MAX_FFI_NARGS 255

#define GIS_PACKAGE symbol_get_value(c_intern("*package*", "lisp"))

#include "ops.h"

/* The types defined below are not the same as the types that will
   be defined within the language. */
enum object_type {
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
  type_record = 46, /* type_nil won't appear on an object, only from calls to
                   get_object_type(...) */
  type_vec2 = 50,
  type_dlib = 54, /** dynamic library */
  type_ffun = 58, /** foreign function (function from a dynamic library) */
  type_ptr = 62, /** used for FFI */
  type_type = 66
};
/* ATTENTION! when adding a new type, make sure to update this define below!
   this defines the border between types defined as builtins and the user. */
#define HIGHEST_TYPE type_type

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
  struct vec2 *vec2;
  struct dlib *dlib;
  struct ffun *ffun;
  struct type *type;
  void *ptr;
};

union w0 { /** The first word of an object. */
  enum object_type type;
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
  unsigned char *bytes; /** the contents of the byte-array */
};

struct symbol {
  struct object *name; /** the name of the symbol (a string) */
  struct object *package; /** the package this symbol is defined in (the "home package") */
  struct object *value; /** the value slot */
  struct object *function; /** the function value slot */
  struct object *type; /** the type value slot */
  struct object *plist; /** a plist that maps from namespace name to value */
  char is_external;
  char value_is_set;
  char function_is_set;
  char type_is_set;
};

struct package {
  struct object *name; /** the name of the package (a string) */
  struct object *symbols; /** all the symbols in this package */
};

struct dlib {
  struct object *path; /** path to the dynamic library (a string) */
  HINSTANCE ptr;
};

struct ffun {
  struct object *ffname; /** the foreign function name */
  struct object *dlib; /** the dynamic library this function is from */
  struct object *ret_type; /** the return type */
  struct object *param_types; /** the type parameters this function takes */
  FARPROC ptr; /* a pointer to the foreign function */
  ffi_cif *cif;
  ffi_type **arg_types;
  ffi_type *ffi_ret_type; /** the return type for libffi */
  ufixnum_t nargs;
};

struct type {
  struct object *name;
  fixnum_t id; /** the id in gis->types of this type */
  ffi_type *ffi_type;
  struct object **struct_field_names;
  struct object **struct_field_types;
  ufixnum_t struct_nfields;
  size_t *struct_offsets;
  char builtin; /** is this type builtin or user defined? */
};

struct function {
  struct object *name; /** a symbol - optional */
  struct object *code; /** the code (a byte-array) */
  struct object *docstring; /** the docstring */
  struct object *constants; /** an array of constants used within the code (an array) */
  ufixnum_t stack_size; /** how many items to reserve on the stack - includes arguments */
  ufixnum_t nargs; /** how many arguments does this require? */
  char is_builtin; /** is this a builtin function? */
  char is_macro; /** is this a macro? */
  char accepts_all; /** is this a (function _ all ...) function? */
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

struct image {
  /*
   Step 1 to getting images working:
      - remove all hardcoded symbols/strings in GIS

   Do uninterned symbols belong in the image? can just interned symbols be included?
   What about constants like types, builtins, etc...?
  */
  struct object *symbols; /* */
};

/**
 * The global interpreter state.
 * 
 * Any symbols that the interpreter needs to reference should be added to the GIS.
 *   If the compiler is written in C, this includes symbols that the compiler uses (e.g. needs to reference add_symbol to reference "+").
 *   This also includes any values that the interpeter manages and exposes to the user such as the list of packages available.
 * Not every symbol ever created needs to be in here. For example, it serves no purpose to add the FIND-PACKAGE symbol to the GIS because
 * builtins are handled through builtin objects, not symbols.
 */
struct gis {
  struct object *data_stack; /** same as the value in data_stack_symbol */
  struct object *call_stack; /** same as the value in call_stack_symbol */
  struct object *types;
  char loaded_core;

  struct object *nil;
  struct object *t;
  struct object *i;
  struct object *f;

  ufixnum_t gensym_counter;

  /* Types */
  /* Common Lisp types are just symbols and lists.
     Maybe having a separate type object will have some benefit?
     For example, if you know something is a type, you can cache (intern) them.
     If it is just a list and symbols you waste memory when referencing the same thing multiple times.
     (e.g. Type specifiers look something like: (type (vector 10)) for a vector of max length 10 and bug
     could replace it with a compile time constant). This would benefit all calls to "type-of" too. The same
     object would be returned, and they would be immutable. Maybe some Common Lisp impls already makes the lists that can
     be returned from type-of immutable? */
  struct object *char_type;
  struct object *cons_type;
  struct object *dlib_type;
  struct object *dynamic_array_type; 
  struct object *dynamic_byte_array_type; 
  struct object *dynamic_library_type;
  struct object *enumerator_type; 
  struct object *file_type; 
  struct object *fixnum_type;
  struct object *flonum_type;
  struct object *foreign_function_type;
  struct object *function_type;
  struct object *int_type;
  struct object *struct_type;
  struct object *symbol_type;
  struct object *type_type;
  struct object *nil_type;
  struct object *object_type; 
  struct object *package_type;
  struct object *pointer_type; 
  struct object *record_type; 
  struct object *string_type;
  struct object *ufixnum_type;
  struct object *uint_type;
  struct object *uint8_type;
  struct object *uint16_type;
  struct object *uint32_type;
  struct object *vec2_type; 
  struct object *void_type;

  /* Builtins */
  struct object *alloc_struct_builtin;
  struct object *apply_builtin;
  struct object *byte_stream_builtin;
  struct object *byte_stream_peek_builtin;
  struct object *byte_stream_peek_byte_builtin;
  struct object *byte_stream_read_builtin;
  struct object *byte_stream_has_builtin;
  struct object *change_directory_builtin;
  struct object *close_file_builtin;
  struct object *debugger_builtin;
  struct object *dynamic_array_builtin;
  struct object *dynamic_array_get_builtin;
  struct object *dynamic_array_set_builtin;
  struct object *dynamic_array_length_builtin;
  struct object *dynamic_array_push_builtin;
  struct object *dynamic_array_pop_builtin;
  struct object *dynamic_array_concat_builtin;
  struct object *dynamic_byte_array_as_string_builtin;
  struct object *dynamic_byte_array_builtin;
  struct object *dynamic_byte_array_concat_builtin;
  struct object *dynamic_byte_array_get_builtin;
  struct object *dynamic_byte_array_insert_builtin;
  struct object *dynamic_byte_array_length_builtin;
  struct object *dynamic_byte_array_set_builtin;
  struct object *dynamic_byte_array_push_builtin;
  struct object *dynamic_byte_array_pop_builtin;
  struct object *dynamic_library_builtin;
  struct object *fbound_builtin;
  struct object *function_macro_builtin;
  struct object *find_package_builtin;
  struct object *find_symbol_builtin;
  struct object *foreign_function_builtin;
  struct object *function_code_builtin;
  struct object *intern_builtin;
  struct object *get_current_working_directory_builtin;
  struct object *gensym_builtin;
  struct object *read_bytecode_file_builtin;
  struct object *read_file_builtin;
  struct object *string_concat_builtin;
  struct object *struct_field_builtin;
  struct object *to_string_builtin;
  struct object *macro_builtin;
  struct object *make_symbol_builtin;
  struct object *make_package_builtin;
  struct object *make_function_builtin;
  struct object *marshal_builtin;
  struct object *marshal_integer_builtin;
  struct object *open_file_builtin;
  struct object *package_symbols_builtin;
  struct object *package_name_builtin;
  struct object *set_struct_field_builtin;
  struct object *define_struct_builtin;
  struct object *symbol_name_builtin;
  struct object *symbol_type_builtin;
  struct object *symbol_value_set_builtin;
  struct object *type_of_builtin;
  struct object *unmarshal_builtin;
  struct object *use_package_builtin;
  struct object *write_bytecode_file_builtin;
  struct object *write_file_builtin;
  struct object *write_image_builtin;
};

#include "string.h"
#include "debug.h"
#include "marshal.h"
#include "dynamic_byte_array.h"
#include "dynamic_array.h"
#include "os.h"
#include "ffi.h"

/**
 * The global interpreter state
 * 
 * (extern because must be used in several C files)
 * 
 * Initialized using init()
 */
extern struct gis *gis;

struct object *run(struct gis *gis);

struct object *eval(struct object *bc, struct object* args);
/* Temporary -- commented out because unistd.h defines read and having this uncommented causes conflicts
struct object *read(struct object *s, struct object *package); 
*/

char equals(struct object *o0, struct object *o1);

enum object_type object_type_of(struct object *o);
struct object *type_name_of(struct object *o);
char *type_name_of_cstr(struct object *o);
char *type_name_cstr(struct object *o);
struct object *type_of(struct object *o);
char *bstring_to_cstring(struct object *str);
fixnum_t count(struct object *list);

struct object *symbol_get_value(struct object *sym);
struct object *symbol_get_type(struct object *sym);

void symbol_set_type(struct object *sym, struct object *t);
void symbol_set_function(struct object *sym, struct object *f);
void symbol_set_value(struct object *sym, struct object *value);

void print(struct object *o);
void print_no_newline(struct object *o);

struct object *object(enum object_type t);
struct object *fixnum(fixnum_t fixnum);
struct object *ufixnum(ufixnum_t ufixnum);
struct object *flonum(flonum_t flo);
struct object *vec2(flonum_t x, flonum_t y);
struct object *dlib(struct object *path);
struct object *dynamic_array(fixnum_t initial_capacity);
struct object *dynamic_byte_array(ufixnum_t initial_capacity);
struct object *ffun(struct object *dlib, struct object *ffname, struct object* ret_type, struct object *param_types);
struct object *pointer(void *ptr);
struct object *function(struct object *constants, struct object *code, ufixnum_t stack_size);
struct object *string(char *contents);
struct object *enumerator(struct object *source);
struct object *package(struct object *name);
struct object *cons(struct object *car, struct object *cdr);
struct object *symbol(struct object *name);
struct object *type(struct object *name, struct object *struct_fields, char can_instantiate, char builtin);
struct object *file_stdin();
struct object *file_stdout();
struct object *open_file(struct object *path, struct object *mode);
void close_file(struct object *file);

void print_stack();

struct object *intern(struct object *string, struct object *package);
struct object *c_intern(char *symbol_name, char *package_name);
struct object *find_package(struct object *name);
void use_package(struct object *p0, struct object *p1);

struct object *write_file(struct object *file, struct object *o);
struct object *read_file(struct object *file);

struct object *call_function(struct object *f, struct object *args);

#endif