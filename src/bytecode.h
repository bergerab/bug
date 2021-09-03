typedef int fixnum_t;
typedef float flonum_t;

enum ops {
  op_drop,
  op_dup,
  op_intern,
  op_array,
  op_array_ref,
  op_aref,
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
  type_array = 28,
  type_string = 32,
  type_package = 36,
  type_byte_array = 40,
  type_bytecode = 44 
};

union value {
  fixnum_t fixnum;
  flonum_t flonum;
  struct array *array;
  struct byte_array *byte_array;
  struct symbol *symbol;
  struct bytecode *bytecode;
  struct package *package;
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

struct array {
  struct object *length; /** the number of items in the array (a fixnum) */
  struct object **values; /** the contents of the array (arbitrary objects) */
};

struct byte_array {
  struct object *length; /** the number of items in the byte-array (a fixnum) */
  uint8_t *bytes; /** the contents of the byte-array */
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

/**
 * The global interpreter state.
 */
struct gis {
  struct object *stack; /** the data stack (a cons list) */
  struct object *package; /** the current package being evaluated */
};