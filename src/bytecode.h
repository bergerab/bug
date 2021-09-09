typedef int fixnum_t;
typedef float flonum_t;

#define FIXNUM_VALUE(o)\
 o->w1.value.fixnum

#define FLONUM_VALUE(o)\
 o->w1.value.flonum

#define ARRAY_LENGTH(o)\
  o->w1.value.array->length

#define ARRAY_VALUES(o)\
  o->w1.value.array->values

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

#define BC_VERSION 0

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
  type_cons = 0,
  type_fixnum = 4,
  type_flonum = 8,
  type_symbol = 20,
  type_dynamic_array = 28,
  type_string = 32,
  type_package = 36,
  type_dynamic_byte_array = 40,
  type_bytecode = 44,
  type_file = 48
};

union value {
  fixnum_t fixnum;
  flonum_t flonum;
  struct dynamic_array *dynamic_array;
  struct dynamic_byte_array *dynamic_byte_array;
  struct symbol *symbol;
  struct bytecode *bytecode;
  struct package *package;
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
  fixnum_t length; /** the number of items in the array (a fixnum) */
  fixnum_t capacity;
  struct object **values; /** the contents of the array (arbitrary objects) */
};

struct dynamic_byte_array {
  fixnum_t length; /** the number of items in the byte-array (a fixnum) */
  fixnum_t capacity;
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
  FILE *fp; /** the file pointer */
};

/* Not an object in the bytecode, just used for documentation */
struct bytecode_file_header {
  /* header section - 128 bits */
  uint8_t magic[4]; /** the ascii string "bug\0" */
  uint32_t version; /** the version of bytecode this is */
  uint8_t word_size; /** how many bytes are in a word */
  uint8_t pad[3 + 4];

  /* bytecode */
  struct object bytecode;
};

/**
 * The global interpreter state.
 */
struct gis {
  struct object *stack; /** the data stack (a cons list) */
  struct object *package; /** the current package being evaluated */
};