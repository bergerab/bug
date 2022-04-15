/** @file interpreter.c
 *
 * The Bug Bytecode interpreter
 */

#include "bug.h"

/* used as a reference to null as a variable */
void *ffi_null = NULL;

struct gis *gis;

/*===============================*
 *===============================*
 * Types                         *
 *===============================*
 *===============================*/

struct object *type_name(struct object *o) {
  return TYPE_NAME(o);
}

char *type_name_cstr(struct object *o) {
  struct object *name;
  name = TYPE_NAME(o);
  dynamic_byte_array_force_cstr(name);
  return STRING_CONTENTS(name);
}

#define HAS_OBJECT_TYPE(o) (OBJECT_TYPE(o) & 2)
#define TYPE_INDEX(o) (OBJECT_TYPE(o) >> 2)
#define IS_TYPE_USER_DEFINED(o) (HAS_OBJECT_TYPE(o) && OBJECT_TYPE(o) > HIGHEST_TYPE)

/* returns object of type type. */
struct object *type_of(struct object *o) {
  if (o == NIL) return gis->symbol_type;
  if (HAS_OBJECT_TYPE(o)) {
    /* cannot use validation here, because validation calls type_of and infinitely recurses */
    return dynamic_array_get_ufixnum_t(gis->types, TYPE_INDEX(o));
  }
  return gis->cons_type;
}

struct object *type_name_of(struct object *o) {
  return type_name(type_of(o));
}

char *type_name_of_cstr(struct object *o) {
  return type_name_cstr(type_of(o));
}

enum object_type object_type_of(struct object *o) {
  if (HAS_OBJECT_TYPE(o)) {
    /* cannot use validation here, because validation calls type_of and infinitely recurses */
    return OBJECT_TYPE(o);
  }
  return type_cons;
}

/**
 * Converts a bug string to a c string
 */
char *bstring_to_cstring(struct object *str) {
  fixnum_t length;
  char *buf;
  OT2("bstring_to_cstring", 0, str, type_dynamic_byte_array, type_string);
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
  fixnum_t count;
  OT_LIST("count", 0, list);
  count = 0;
  while (list != NIL) {
    ++count;
    list = list->w1.cdr;
  }
  return count;
}

/* number of elements in a null terminated array */
size_t count_nta(void **arr) {
  void *cursor;
  size_t size;
  size = 0;
  cursor = arr[size];
  while (cursor != NULL) {
    ++size;
    cursor = arr[size];
  }
  return size;
}

/*
 * Value constructors
 */
struct object *object(enum object_type t) {
  struct object *o = malloc(sizeof(struct object));
  if (o == NULL) return NULL;
  OBJECT_TYPE(o) = t;
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

struct object *vec2(flonum_t x, flonum_t y) {
  struct object *o;
  o = object(type_vec2);
  NC(o, "Failed to allocate 2d-vector object.");
  o->w1.value.vec2 = malloc(sizeof(struct vec2));
  NC(o->w1.value.vec2, "Failed to allocate 2d-vector.");
  VEC2_X(o) = x;
  VEC2_Y(o) = y;
  return o;
}

struct object *dlib(struct object *path) {
  struct object *o;
  OT2("dlib", 0, path, type_dynamic_byte_array, type_string);
  o = object(type_dlib);
  NC(o, "Failed to allocate dlib object.");
  o->w1.value.dlib = malloc(sizeof(struct dlib));
  NC(o->w1.value.dlib, "Failed to allocate dlib.");
  DLIB_PATH(o) = path;
  dynamic_byte_array_force_cstr(path);
  DLIB_PTR(o) = LoadLibrary(STRING_CONTENTS(path));
  if (DLIB_PTR(o) == NULL) {
    printf("Failed to load dynamic library %s.\n", STRING_CONTENTS(path));
    PRINT_STACK_TRACE_AND_QUIT();
  }
  return o;
}

/* can_instantiate indicates if this type supports having values with this type.
   e.g.: you can make value of a type string, type fixnum, etc... but having a value of type
   uint8 is not supported. Same with something like "void". Those values are just used for FFI. */
struct object *type(struct object *sym, struct object *struct_fields, char can_instantiate, char builtin) {
  ufixnum_t i;
  struct object *cursor, *t;
  struct object *o;
  struct object *field_type;
  OT("type", 0, sym, type_symbol);
  OT_LIST("type", 1, struct_fields);
  o = object(type_type);
  NC(o, "Failed to allocate type object.");
  o->w1.value.type = malloc(sizeof(struct type));
  NC(o->w1.value.type, "Failed to allocate type.");
  TYPE_NAME(o) = SYMBOL_NAME(sym);

  if (can_instantiate) {
    TYPE_ID(o) = DYNAMIC_ARRAY_LENGTH(gis->types);
    dynamic_array_push(gis->types, o);
  } else {
    TYPE_ID(o) = -1;
  }

  TYPE_STRUCT_NFIELDS(o) = 0;

  TYPE_BUILTIN(o) = builtin;

  /* if this is a struct, */
  if (struct_fields != NIL) {
    TYPE_STRUCT_NFIELDS(o) = count(struct_fields);

    TYPE_STRUCT_FIELD_NAMES(o) = malloc(sizeof(struct object*) * TYPE_STRUCT_NFIELDS(o)); /* TODO: GC free this */
    TYPE_STRUCT_FIELD_TYPES(o) = malloc(sizeof(struct object*) * TYPE_STRUCT_NFIELDS(o)); /* TODO: GC free this */

    cursor = struct_fields;
    i = 0;
    while (cursor != NIL) {
      TYPE_STRUCT_FIELD_NAMES(o)[i] = CONS_CAR(CONS_CAR(cursor));
      field_type = CONS_CAR(CONS_CDR(CONS_CAR(cursor)));
      if (type_of(field_type) == gis->symbol_type)
        field_type = symbol_get_type(field_type);
      TYPE_STRUCT_FIELD_TYPES(o)[i] = field_type;
      cursor = CONS_CDR(cursor);
      i++;
    }

    TYPE_FFI_TYPE(o) = malloc(sizeof(ffi_type));

    /* make a new ffi_type object */
    TYPE_FFI_TYPE(o)->size = TYPE_FFI_TYPE(o)->alignment = 0;
    TYPE_FFI_TYPE(o)->type = FFI_TYPE_STRUCT;
    TYPE_FFI_TYPE(o)->elements =
        malloc(sizeof(ffi_type *) *
               (count(struct_fields) + 1)); /* TODO: make sure to free this */

    i = 0;
    cursor = struct_fields;
    while (cursor != NIL) {
      t = CONS_CAR(CONS_CDR(CONS_CAR(cursor)));
      TYPE_FFI_TYPE(o)->elements[i] = ffi_type_designator_to_ffi_type(t, 1);
      ++i;
      cursor = CONS_CDR(cursor);
    }

    TYPE_FFI_TYPE(o)->elements[i] = NULL; /* must be null terminated */
    TYPE_STRUCT_NFIELDS(o) = i;
    TYPE_STRUCT_OFFSETS(o) = malloc(sizeof(size_t) * i); /* TODO: GC clean up */ 
    ffi_get_struct_offsets(FFI_DEFAULT_ABI, TYPE_FFI_TYPE(o),
                           TYPE_STRUCT_OFFSETS(o));
  }

  symbol_set_type(sym, o);
  return o;
}

struct object *dynamic_array(fixnum_t initial_capacity) {
  struct object *o = object(type_dynamic_array);
  NC(o, "Failed to allocate dynamic-array object.");
  o->w1.value.dynamic_array = malloc(sizeof(struct dynamic_array));
  NC(o->w1.value.dynamic_array, "Failed to allocate dynamic-array.");
  DYNAMIC_ARRAY_CAPACITY(o) = initial_capacity < 0 ? DEFAULT_INITIAL_CAPACITY : initial_capacity;
  DYNAMIC_ARRAY_VALUES(o) = malloc(DYNAMIC_ARRAY_CAPACITY(o) * sizeof(struct object*));
  NC(DYNAMIC_ARRAY_VALUES(o), "Failed to allocate dynamic-array values array.");
  DYNAMIC_ARRAY_LENGTH(o) = 0;
  return o;
}

struct object *dynamic_byte_array(ufixnum_t initial_capacity) {
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

struct object *foreign_function(struct object *dlib, struct object *ffname, struct object* ret_type, struct object *params) {
  ffi_status status;
  struct object *o;

  OT("foreign_function", 0, dlib, type_dlib);
  ffname = string_designator(ffname);
  OT2("foreign_function", 1, ffname, type_dynamic_byte_array, type_string);
  /* OT("foreign_function", 2, ret_type, type_symbol); accepts a symbol or a list now. */
  OT_LIST("foreign_function", 3, params);

  o = object(type_ffun);
  NC(o, "Failed to allocate ffun object.");
  o->w1.value.ffun = malloc(sizeof(struct ffun));
  FFUN_FFNAME(o) = string_designator(ffname);
  FFUN_DLIB(o) = dlib;
  dynamic_byte_array_force_cstr(FFUN_FFNAME(o));
  FFUN_PTR(o) = GetProcAddress(DLIB_PTR(dlib), STRING_CONTENTS(FFUN_FFNAME(o)));
  if (FFUN_PTR(o) == NULL) {
    printf("Failed to load foreign function %s.", STRING_CONTENTS(FFUN_FFNAME(o)));
    PRINT_STACK_TRACE_AND_QUIT();
  }
  FFUN_PARAM_TYPES(o) = params;

  FFUN_RET_TYPE(o) = ret_type;
  if (type_of(FFUN_RET_TYPE(o)) == gis->symbol_type) {
    FFUN_RET_TYPE(o) = symbol_get_type(FFUN_RET_TYPE(o));
  }
  /* FFUN_RET_TYPE can also be (pointer void), (pointer int), etc...
  if (type_of(FFUN_RET_TYPE(o)) != gis->type_type) {
    printf("Foreign function requires a type.\n");
    PRINT_STACK_TRACE_AND_QUIT();
  }
  */

  FFUN_ARGTYPES(o) = malloc(sizeof(ffi_type*) * MAX_FFI_NARGS);
  FFUN_NARGS(o) = 0;

  while (params != NIL) {
    FFUN_ARGTYPES(o)[FFUN_NARGS(o)++] = ffi_type_designator_to_ffi_type(CONS_CAR(params), 1); /* TODO: within_another_struct=1 -- bad name */
    if (FFUN_NARGS(o) > MAX_FFI_NARGS) {
      printf("Too many arguments for foreign function.");
      PRINT_STACK_TRACE_AND_QUIT();
    }
    params = CONS_CDR(params);
  }

  FFUN_FFI_RET_TYPE(o) = ffi_type_designator_to_ffi_type(FFUN_RET_TYPE(o), 1);

  FFUN_CIF(o) = malloc(sizeof(ffi_cif));
  if ((status = ffi_prep_cif(FFUN_CIF(o), FFI_DEFAULT_ABI, FFUN_NARGS(o), FFUN_FFI_RET_TYPE(o), FFUN_ARGTYPES(o))) != FFI_OK) {
      printf("PRINT_STACK_TRACE_AND_QUIT preparing CIF.\n");
      PRINT_STACK_TRACE_AND_QUIT();
  }

  return o;
}

struct object *pointer(void *ptr) {
  struct object *o = object(type_ptr);
  NC(o, "Failed to allocate pointer object.");
  OBJECT_POINTER(o) = ptr;
  return o;
}

void set_struct_type(struct object *type, struct object *instance) {
  OBJECT_TYPE(instance) = (TYPE_ID(type) << 2) | 2;
}

/* get a field's value from a type instance */
struct object *struct_field(struct object *instance, struct object *sdes) {
  struct object *type, *field_type, *temp;
  void *ptr;
  ufixnum_t i;

  type = type_of(instance);

  if (TYPE_STRUCT_NFIELDS(type) == 0) {
    printf("Type is not a structure.\n");
    print(type);
    PRINT_STACK_TRACE_AND_QUIT();
  }

  field_type = NIL;
  for (i = 0; i < TYPE_STRUCT_NFIELDS(type); ++i) {
    if (equals(sdes, TYPE_STRUCT_FIELD_NAMES(type)[i])) {
      field_type = TYPE_STRUCT_FIELD_TYPES(type)[i];
      break;
    }
  }

  if (field_type == NIL) {
    printf("Field does not exist on structure.\n");
    PRINT_STACK_TRACE_AND_QUIT();
  }

  ptr = &((char *)OBJECT_POINTER(instance))[TYPE_STRUCT_OFFSETS(type)[i]];

  if (field_type == gis->int_type) {
    return fixnum(*(int *)ptr);
  } else if (field_type == gis->char_type) {
    return fixnum(*(char *)ptr);
  } else if (field_type == gis->uint8_type) {
    return fixnum(*(uint8_t *)ptr);
  } else if (field_type == gis->uint16_type) {
    return fixnum(*(uint16_t *)ptr);
  } else if (field_type == gis->uint32_type) {
    return fixnum(*(uint32_t *)ptr);
  } else if (field_type == gis->pointer_type) {
    return pointer((void *)ptr);
  } else if (field_type == gis->object_type) {
    return *(struct object **)ptr;
  } else if (!TYPE_BUILTIN(field_type)) {
    temp = pointer(*(void **)ptr);
    set_struct_type(field_type, temp);
    return temp;
  } else {
    printf("Unsupported field type.\n");
    print(field_type);
    PRINT_STACK_TRACE_AND_QUIT();
  }
}

struct object *alloc_struct_inner(struct object *type, char init_defaults) {
  struct object *instance;
  struct object *field_type;
  struct object *field_name;
  void *struct_value;
  size_t i, nfields;
  ffi_type *t;
  uint8_t default_uint8_val;
  uint16_t default_uint16_val;
  uint32_t default_uint32_val;
  int default_sint_val;

  /* iterate over struct ffi types and input */
  t = TYPE_FFI_TYPE(type);
  instance = malloc(t->size); /* TODO: GC clean this up */
  if (instance == NULL) {
    printf("failed to malloc struct for ffi.");
    PRINT_STACK_TRACE_AND_QUIT();
  }

  if (init_defaults) {
    /* fill struct with default values */
    i = 0;
    nfields = TYPE_STRUCT_NFIELDS(type);
    for (; i < nfields; ++i) {
      field_type = TYPE_STRUCT_FIELD_TYPES(type)[i];
      field_name = TYPE_STRUCT_FIELD_NAMES(type)[i];
      if (t->elements[i] == &ffi_type_sint) {
        default_sint_val = 0;
        /* arithemtic can't be done on void* (size of elements is unknown) so
         * cast to a char* (because we know a4 is # of bytes) */
        memcpy(&((char *)instance)[TYPE_STRUCT_OFFSETS(type)[i]], &default_sint_val, sizeof(int));
      } else if (t->elements[i] == &ffi_type_uint32) {
        default_uint32_val = 0;
        /* arithemtic can't be done on void* (size of elements is unknown) so
         * cast to a char* (because we know a4 is # of bytes) */
        memcpy(&((char *)instance)[TYPE_STRUCT_OFFSETS(type)[i]], &default_uint32_val, sizeof(uint32_t));
      } else if (t->elements[i] == &ffi_type_uint16) {
        default_uint16_val = 0;
        /* arithemtic can't be done on void* (size of elements is unknown) so
         * cast to a char* (because we know a4 is # of bytes) */
        memcpy(&((char *)instance)[TYPE_STRUCT_OFFSETS(type)[i]], &default_uint16_val, sizeof(uint16_t));
      } else if (t->elements[i] == &ffi_type_uint8) {
        default_uint8_val = 0;
        /* arithemtic can't be done on void* (size of elements is unknown) so
         * cast to a char* (because we know a4 is # of bytes) */
        memcpy(&((char *)instance)[TYPE_STRUCT_OFFSETS(type)[i]], &default_uint8_val, sizeof(uint8_t));
        /*((char*)instance)[offsets[a3]] = FIXNUM_VALUE(CONS_CAR(cursor2));*/
        /* why didn't this work?  */
      } else if (field_type == gis->object_type) {
        struct_value = NIL;
        /* arithemtic can't be done on void* (size of elements is unknown) so
         * cast to a char* (because we know a4 is # of bytes) */
        memcpy(&((char *)instance)[TYPE_STRUCT_OFFSETS(type)[i]], &struct_value, sizeof(void *));
      } else if (!TYPE_BUILTIN(field_type)) {
        struct_value = alloc_struct_inner(field_type, init_defaults);
        memcpy(&((char *)instance)[TYPE_STRUCT_OFFSETS(type)[i]], &struct_value, sizeof(void *));
      } else {
        printf("Unknown value when populating struct\n");
        print(field_name);
        print(field_type);
        printf("%d\n", TYPE_STRUCT_NFIELDS(field_type) > 0);
        print(type_of(field_type));
        PRINT_STACK_TRACE_AND_QUIT();
      }
    }
  }
  return instance;
}

struct object *alloc_struct(struct object *type, char init_defaults) {
  struct object *o;
  if (type_of(type) == gis->symbol_type) {
    type = symbol_get_type(type);
  }
  o = pointer(alloc_struct_inner(type, init_defaults));
  set_struct_type(type, o);
  return o;
}

/* set a field's value from a structure instance */
void set_struct_field(struct object *instance, struct object *sdes, struct object *value) {
  struct object *type, *field_type;
  ufixnum_t i;

  type = type_of(instance);

  if (type == NIL) {
    printf("Type is not a structure.\n");
    PRINT_STACK_TRACE_AND_QUIT();
  }

  field_type = NIL;
  for (i = 0; i < TYPE_STRUCT_NFIELDS(type); ++i) {
    if (equals(sdes, TYPE_STRUCT_FIELD_NAMES(type)[i])) {
      field_type = TYPE_STRUCT_FIELD_TYPES(type)[i];
      break;
    }
  }

  if (field_type == NULL) {
    printf("Field does not exist on structure.\n");
    PRINT_STACK_TRACE_AND_QUIT();
  }

  /*
  * TODO: handle different types here:
  */
 if (field_type == gis->fixnum_type || field_type == gis->int_type) {
  memcpy(&((char *)OBJECT_POINTER(instance))[TYPE_STRUCT_OFFSETS(type)[i]], &FIXNUM_VALUE(value), sizeof(int));
 } else if (field_type == gis->object_type) {
  memcpy(&((char *)OBJECT_POINTER(instance))[TYPE_STRUCT_OFFSETS(type)[i]], &value, sizeof(struct object *));
 } else {
   printf("Unsupported set-struct-field type");
   print(field_type);
 }
}

struct object *function(struct object *constants, struct object *code, ufixnum_t stack_size) {
  struct object *o;
  o = object(type_function);
  NC(o, "Failed to allocate function object.");
  o->w1.value.function = malloc(sizeof(struct function));
  NC(o->w1.value.function, "Failed to allocate function.");
  FUNCTION_CODE(o) = code;
  FUNCTION_CONSTANTS(o) = constants;
  FUNCTION_STACK_SIZE(o) = stack_size;
  FUNCTION_NAME(o) = NIL;
  FUNCTION_DOCSTRING(o) = NIL;
  FUNCTION_NARGS(o) = 0;
  FUNCTION_IS_BUILTIN(o) = 0;
  FUNCTION_IS_MACRO(o) = 0;
  FUNCTION_ACCEPTS_ALL(o) = 0;
  return o;
}

struct object *string(char *contents) {
  struct object *o;
  ufixnum_t length;
  length = strlen(contents);
  o = dynamic_byte_array(length); /* passing length as the capacity so capacity will always be just enough 
                                     -- it might be worth adding some to the capacity here. 
                                     I would have to test that to confirm its worth. */
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
  ENUMERATOR_VALUE(o) = NIL;
  ENUMERATOR_INDEX(o) = 0;
  return o;
}

struct object *package(struct object *name) {
  struct object *o;
  OT("package", 0, name, type_string);
  o = object(type_package);
  NC(o, "Failed to allocate package object.");
  o->w1.value.package = malloc(sizeof(struct package));
  NC(o->w1.value.package, "Failed to allocate package object.");
  PACKAGE_NAME(o) = name;
  PACKAGE_SYMBOLS(o) = NIL;
  return o;
}

struct object *cons(struct object *car, struct object *cdr) {
  struct object *o;
  o = object(type_cons);
  NC(o, "Failed to allocate cons object.");
  o->w0.car = car;
  o->w1.cdr = cdr;
  return o;
}

/* Creates a new symbol. Does NOT intern or add to the current package. */
struct object *symbol(struct object *name) {
  struct object *o;
  OT("symbol", 0, name, type_string);
  o = object(type_symbol);
  NC(o, "Failed to allocate symbol object.");
  o->w1.value.symbol = malloc(sizeof(struct symbol));
  NC(o->w1.value.symbol, "Failed to allocate symbol.");

  SYMBOL_NAME(o) = name;
  SYMBOL_PLIST(o) = NIL;
  SYMBOL_PACKAGE(o) = NIL;
  SYMBOL_IS_EXTERNAL(o) = 0;

  SYMBOL_VALUE_IS_SET(o) = 0;
  SYMBOL_FUNCTION_IS_SET(o) = 0;
  SYMBOL_TYPE_IS_SET(o) = 0;

  SYMBOL_FUNCTION(o) = NIL;
  SYMBOL_VALUE(o) = NIL;
  SYMBOL_TYPE(o) = NIL;

  return o;
}

struct object *file_stdin() {
  struct object *o;
  o = object(type_file);
  o->w1.value.file = malloc(sizeof(struct file));
  NC(o->w1.value.file, "Failed to allocate file value.");
  FILE_FP(o) = stdin;
  FILE_MODE(o) = NIL;
  FILE_PATH(o) = NIL;
  return o;
}

struct object *file_stdout() {
  struct object *o;
  o = object(type_file);
  o->w1.value.file = malloc(sizeof(struct file));
  NC(o->w1.value.file, "Failed to allocate file value.");
  FILE_FP(o) = stdout;
  FILE_MODE(o) = NIL;
  FILE_PATH(o) = NIL;
  return o;
}

struct object *open_file(struct object *path, struct object *mode) {
  FILE *fp;
  struct object *o;

  OT2("open_file", 0, path, type_dynamic_byte_array, type_string);
  OT2("open_file", 1, mode, type_dynamic_byte_array, type_string);

  dynamic_byte_array_force_cstr(path);
  dynamic_byte_array_force_cstr(mode);

  fp = fopen(STRING_CONTENTS(path), STRING_CONTENTS(mode));

  if (fp == NULL) {
    printf("BC: Failed to open file at path \"%s\" with mode \"%s\".\n", STRING_CONTENTS(path),
           STRING_CONTENTS(mode));
    PRINT_STACK_TRACE_AND_QUIT();
  }

  o = object(type_file);

  o->w1.value.file = malloc(sizeof(struct file));
  NC(o->w1.value.file, "Failed to allocate file value.");

  FILE_FP(o) = fp;
  FILE_PATH(o) = string(STRING_CONTENTS(path)); /* clones the string */
  FILE_MODE(o) = string(STRING_CONTENTS(mode)); /* clones the string */

  return o;
}

void close_file(struct object *file) {
  OT("close_file", 0, file, type_file);
  if (fclose(FILE_FP(file)) != 0) {
    printf("failed to close file\n");
    PRINT_STACK_TRACE_AND_QUIT();
  }
}

/*===============================*
 *===============================*
 * error handling                *
 *===============================*
 *===============================*/
int print_stack_did_recurse = 0;
void print_stack() {
  fixnum_t i, j, len;
  struct object *f, *next_f;
  ++print_stack_did_recurse;
  len = DYNAMIC_ARRAY_LENGTH(gis->call_stack);
  i = len - 1;
  if (print_stack_did_recurse > 1) { /* prevent infinite recursion if the source of the error was a function called within this function */
    exit(1);
  }
  f = symbol_get_value(gis->impl_f_sym);
  printf("Stack Trace\n");
  while (i > 0) { /* TODO: this prints the stack in a revered order */
    if (f == NIL) {
      printf("Top level\n");
      break;
    } else if (type_of(f) != gis->function_type) {
      printf("Error while printing call stack. Expected a function but found: ");
      print(f);
      print(gis->call_stack);
      exit(1);
    } else if (FUNCTION_NAME(f) != NIL) {
      dynamic_byte_array_force_cstr(SYMBOL_NAME(FUNCTION_NAME(f)));
      printf("(%s", STRING_CONTENTS(SYMBOL_NAME(FUNCTION_NAME(f))));
      j = 0;
      for (j = 0; j < FUNCTION_NARGS(f); ++j) {
        printf(" ");
        print_no_newline(do_to_string(dynamic_array_get_ufixnum_t(gis->call_stack, i - 2 - FUNCTION_STACK_SIZE(f) + j + 1), 1));
      }
    } else {
      printf("(<anonymous-function>");
    }
    next_f = dynamic_array_get_ufixnum_t(gis->call_stack, i);
    if (f != NIL) {
      i -= 2; /* skip instruction index for now -- should translate this to line
                 number later */
      i -= FUNCTION_STACK_SIZE(f);
      printf(")\n");
    }
    f = next_f;
  }
  --print_stack_did_recurse;
}

/*===============================*
 *===============================*
 * Print                         *
 *===============================*
 *===============================*/
void do_print(struct object *o, char newline) {
  ufixnum_t i;
  o = to_string(o);
  for (i = 0; i < STRING_LENGTH(o); ++i) {
    printf("%c", STRING_CONTENTS(o)[i]);
  }
  if (newline)
    printf("\n");
}
void print(struct object *o) {
  do_print(o, 1);
}
void print_no_newline(struct object *o) {
  do_print(o, 0);
}

/*===============================*
 *===============================*
 * Equality                      *
 *     Structural equality
 *===============================*
 *===============================*/
char equals(struct object *o0, struct object *o1) {
  ufixnum_t i;
  enum object_type t0, t1;

  t0 = object_type_of(o0);
  t1 = object_type_of(o1);

  if (t0 != t1) return 0;
  switch (t0) {
    case type_cons:
      return equals(CONS_CAR(o0), CONS_CAR(o1)) && equals(CONS_CDR(o0), CONS_CDR(o1));
    case type_string:
    case type_dynamic_byte_array:
      if (DYNAMIC_BYTE_ARRAY_LENGTH(o0) != DYNAMIC_BYTE_ARRAY_LENGTH(o1))
        return 0;
      for (i = 0; i < DYNAMIC_BYTE_ARRAY_LENGTH(o0); ++i)
        if (DYNAMIC_BYTE_ARRAY_BYTES(o0)[i] != DYNAMIC_BYTE_ARRAY_BYTES(o1)[i])
          return 0;
      return 1;
    case type_flonum:
      return FLONUM_VALUE(o0) == FLONUM_VALUE(o1);
    case type_fixnum:
      return FIXNUM_VALUE(o0) == FIXNUM_VALUE(o1);
    case type_ufixnum:
      return UFIXNUM_VALUE(o0) == UFIXNUM_VALUE(o1);
    case type_vec2:
      return VEC2_X(o0) == VEC2_X(o1) && VEC2_Y(o0) == VEC2_Y(o1);
    case type_dynamic_array:
      if (DYNAMIC_ARRAY_LENGTH(o0) != DYNAMIC_ARRAY_LENGTH(o1))
        return 0;
      for (i = 0; i < DYNAMIC_ARRAY_LENGTH(o0); ++i)
        if (!equals(DYNAMIC_ARRAY_VALUES(o0)[i], DYNAMIC_ARRAY_VALUES(o1)[i]))
          return 0;
      return 1;
    case type_file:
      return o0 == o1;
    case type_enumerator:
      return equals(ENUMERATOR_SOURCE(o0), ENUMERATOR_SOURCE(o1)) &&
             ENUMERATOR_INDEX(o0) == ENUMERATOR_INDEX(o1);
    case type_package:
      return o0 == o1;
    case type_dlib:
      return DLIB_PTR(o0) == DLIB_PTR(o1);
    case type_ffun:
      return FFUN_PTR(o0) == FFUN_PTR(o1);
    case type_ptr:
      return OBJECT_POINTER(o0) == OBJECT_POINTER(o1);
    case type_symbol:
      return o0 == o1;
    case type_function:
      return equals(FUNCTION_CONSTANTS(o0), FUNCTION_CONSTANTS(o1)) &&
             equals(FUNCTION_CODE(o0), FUNCTION_CODE(o1)) &&
             FUNCTION_STACK_SIZE(o0) == FUNCTION_STACK_SIZE(o1) &&
             FUNCTION_NARGS(o0) == FUNCTION_NARGS(o1);
    case type_record: /* TODO */
    case type_type:
      return o0 == o1;
    default:
      /* TODO: allow for overriding */
      return OBJECT_POINTER(o0) == OBJECT_POINTER(o1);
  }
}

/*===============================*
 *===============================*
 * alist                         *
 *===============================*
 *===============================*/
struct object *cons_reverse(struct object *cursor) {
  struct object *ret;
  ret = NIL;
  while (cursor != NIL) {
    ret = cons(CONS_CAR(cursor), ret);
    cursor = CONS_CDR(cursor);
  }
  return ret;
}

struct object *alist_get_slot(struct object *alist, struct object *key) {
  OT_LIST("alist_get", 0, alist);
  while (alist != NIL) {
    if (equals(CONS_CAR(CONS_CAR(alist)), key))
      return CONS_CAR(alist);
    alist = CONS_CDR(alist);
  }
  return NIL;
}

struct object *alist_get_value(struct object *alist, struct object *key) {
  alist = alist_get_slot(alist, key);
  if (alist == NIL)
    return NIL;
  return CONS_CDR(alist);
}

struct object *alist_extend(struct object *alist, struct object *key, struct object *value) {
  OT_LIST("alist_extend", 0, alist);
  return cons(cons(key, value), alist);
}


/*===============================*
 *===============================*
 * Symbol Interning and access   *
 *===============================*
 *===============================*/
struct object *symbol_get_function(struct object *sym) {
  if (SYMBOL_FUNCTION_IS_SET(sym)) {
    return SYMBOL_FUNCTION(sym);
  } else {
    printf("Symbol ");
    if (SYMBOL_PACKAGE(sym) != NIL) {
      print_no_newline(PACKAGE_NAME(SYMBOL_PACKAGE(sym)));
      if (SYMBOL_IS_EXTERNAL(sym))
        printf(":");
      else
        printf("::");
    }
    print_no_newline(SYMBOL_NAME(sym));
    printf(" has no function.\n");
    PRINT_STACK_TRACE_AND_QUIT();
  }
}

struct object *symbol_get_type(struct object *sym) {
  if (SYMBOL_TYPE_IS_SET(sym)) {
    return SYMBOL_TYPE(sym);
  } else {
    printf("Symbol ");
    if (SYMBOL_PACKAGE(sym) != NIL) {
      print_no_newline(PACKAGE_NAME(SYMBOL_PACKAGE(sym)));
      if (SYMBOL_IS_EXTERNAL(sym))
        printf(":");
      else
        printf("::");
    }
    print_no_newline(SYMBOL_NAME(sym));
    printf(" has no type.\n");
    PRINT_STACK_TRACE_AND_QUIT();
  }
}

struct object *symbol_get_value(struct object *sym) {
  if (SYMBOL_VALUE_IS_SET(sym)) {
    return SYMBOL_VALUE(sym);
  } else {
    printf("Symbol ");
    if (SYMBOL_PACKAGE(sym) != NIL) {
      print_no_newline(PACKAGE_NAME(SYMBOL_PACKAGE(sym)));
      if (SYMBOL_IS_EXTERNAL(sym))
        printf(":");
      else
        printf("::");
    }
    print_no_newline(SYMBOL_NAME(sym));
    printf(" has no value.");
    PRINT_STACK_TRACE_AND_QUIT();
  }
}

void symbol_set_type(struct object *sym, struct object *t) {
  SYMBOL_TYPE(sym) = t;
  SYMBOL_TYPE_IS_SET(sym) = 1;
}

void symbol_set_function(struct object *sym, struct object *f) {
  SYMBOL_FUNCTION(sym) = f;
  SYMBOL_FUNCTION_IS_SET(sym) = 1;
}

void symbol_set_value(struct object *sym, struct object *value) {
  SYMBOL_VALUE(sym) = value;
  SYMBOL_VALUE_IS_SET(sym) = 1;
}

void symbol_export(struct object *sym) {
  SYMBOL_IS_EXTERNAL(sym) = 1;
}

/* uses NULL as a sentinel value for "did not find" 
  include_internal means the reader was given something like "my-package::my-symbol"
  otherwise it is as if the reader were given "my-package:my-symbol" */
struct object *do_find_symbol(struct object *string, struct object *package, char include_internal) {
  struct object *cursor, *sym;

  OT("find_symbol", 0, string, type_string);
  OT("find_symbol", 1, package, type_package);

  /* look in current package for the symbol
     if we are only looking for inherited symbols with this name, don't look in
     the actual package.
     */
  if (include_internal) { 
    /* "my-package::my-symbol" 
      Look in all symbols in "my-package" */
    cursor = PACKAGE_SYMBOLS(package);
    while (cursor != NIL) {
      sym = CONS_CAR(cursor);
      if (equals(SYMBOL_NAME(sym), string)) {
        return sym;
      }
      cursor = CONS_CDR(cursor);
    }
  } else {
    /* "my-package:my-symbol"
        look in all symbols of my-package that have the home-package of my-package
        and are marked as external. */
    cursor = PACKAGE_SYMBOLS(package);
    while (cursor != NIL) {
      sym = CONS_CAR(cursor);
      if (SYMBOL_PACKAGE(sym) == package &&
          SYMBOL_IS_EXTERNAL(sym) &&
          equals(SYMBOL_NAME(sym), string))
        return sym;
      cursor = CONS_CDR(cursor);
    }
  }

  return NULL;
}

struct object *find_symbol(struct object *string, struct object *package, char include_internal) {
  struct object *sym = do_find_symbol(string, package, include_internal);
  if (sym == NULL) return NIL;
  return sym;
}

struct object *intern(struct object *string, struct object *package) {
  struct object *sym;
  OT("intern", 1, package, type_package);

  sym = do_find_symbol(string, package, 1);
  if (sym != NULL)
    return sym;

  /* If no existing symbol was found, create a new one and add it to the current package. */
  sym = symbol(string);
  SYMBOL_PACKAGE(sym) = package; /* set the home package */
  PACKAGE_SYMBOLS(package) = cons(sym, PACKAGE_SYMBOLS(package));
  if (package == gis->keyword_package) { /* all symbols in keyword package have
                                          the value of themselves*/
    symbol_set_value(sym, sym);
    symbol_export(sym); /* all symbols in the keyword package are exported */
  }

  return sym;
}

/** creates a new global interpreter state with one package (called "lisp")
 *  which is set to the current package.
 */
void gis_init(char load_core) {
  struct object *repl;
  char is_reload = 0;
  if (gis == NULL) {
    gis = malloc(sizeof(struct gis));
    if (gis == NULL) {
      printf("Failed to allocate global interpreter state.");
      exit(1);
    }
  } else {
    is_reload = 1;
  }

  /* Initialize all strings */
/* Creates a string on the GIS. */
#define GIS_STR(str, cstr) str = string(cstr);

  if (!is_reload) {
    GIS_STR(gis->macro_str, "macro")
    GIS_STR(gis->function_str, "function")
  }

  /* Bootstrap NIL */
  NIL = NULL;
  NIL = symbol(string("nil"));
  /* All fields that are initialied to NIL must be re-initialized to NIL here because we just defined what NIL is */
  SYMBOL_PLIST(NIL) = NIL;
  SYMBOL_FUNCTION(NIL) = NIL;
  SYMBOL_PACKAGE(NIL) = NIL;
  SYMBOL_TYPE(NIL) = NIL;

  /* NIL evaluates to itself */
  symbol_set_value(NIL, NIL);

  /* Initialize packages */
  gis->type_package = package(string("type"));

  /* final NIL bootstrapping step (required the type package -- add nil to the type package) */
  SYMBOL_PACKAGE(NIL) = gis->type_package;
  PACKAGE_SYMBOLS(gis->type_package) = cons(NIL, NIL); /* add to the type package */
  symbol_export(NIL); /* export nil */

#define M_PAC(pac, cstr) gis->pac##_package = package(string(cstr))

  M_PAC(lisp, "lisp");
  M_PAC(keyword, "keyword");
  M_PAC(user, "user");
  M_PAC(impl, "impl");

  /* Initialize Symbols */
#define GIS_SYM(sym, str, pack)                        \
  sym = symbol(str);                                   \
  SYMBOL_PACKAGE(sym) = gis->pack##_package;           \
  PACKAGE_SYMBOLS(gis->pack##_package) =               \
      cons(sym, PACKAGE_SYMBOLS(gis->pack##_package)); \
  symbol_export(sym);

#define M_SYM(sym, cstr, pack) GIS_SYM(gis->pack##_##sym##_sym, string(cstr), pack)

  M_SYM(alloc_struct, "alloc-struct", impl);
  M_SYM(and, "and", impl);
  M_SYM(byte_stream, "byte-stream", impl);
  M_SYM(byte_stream_peek, "byte-stream-peek", impl);
  M_SYM(byte_stream_peek_byte, "byte-stream-peek-byte", impl);
  M_SYM(byte_stream_read, "byte-stream-read", impl);
  M_SYM(byte_stream_has, "byte-stream-has", impl);
  M_SYM(call, "call", impl);
  M_SYM(call_stack, "call-stack", impl); /** stack for saving stack pointers and values for function calls (a cons list) */
  M_SYM(change_directory, "change-directory", impl);
  M_SYM(close_file, "close-file", impl); 
  M_SYM(continue, "continue", impl); 
  M_SYM(data_stack, "data-stack", impl); /** the data stack (a cons list) */
  M_SYM(debugger, "debugger", impl);
  M_SYM(define_function, "define-function", impl);
  M_SYM(define_struct, "define-struct", impl);
  M_SYM(dynamic_array_set, "dynamic-array-set", lisp);
  M_SYM(dynamic_array_length, "dynamic-array-length", lisp);
  M_SYM(dynamic_array_push, "dynamic-array-push", lisp);
  M_SYM(dynamic_array_pop, "dynamic-array-pop", lisp);
  M_SYM(dynamic_array_concat, "dynamic-array-concat", lisp);
  M_SYM(dynamic_byte_array_as_string, "dynamic-byte-array-as-string", impl);
  M_SYM(dynamic_byte_array_concat, "dynamic-byte-array-concat", impl);
  M_SYM(dynamic_byte_array_get, "dynamic-byte-array-get", impl);
  M_SYM(dynamic_byte_array_insert, "dynamic-byte-array-insert", impl);
  M_SYM(dynamic_byte_array_length, "dynamic-byte-array-length", impl);
  M_SYM(dynamic_byte_array_set, "dynamic-byte-array-set", impl);
  M_SYM(dynamic_byte_array_push, "dynamic-byte-array-push", impl);
  M_SYM(dynamic_byte_array_pop, "dynamic-byte-array-pop", impl);
  M_SYM(drop, "drop", impl);
  M_SYM(f, "f", impl); /** the currently executing function */
  M_SYM(function_code, "function-code", impl);
  M_SYM(get_current_working_directory, "get-current-working-directory", impl); 
  M_SYM(struct_field, "struct-field", impl);
  M_SYM(i, "i", impl); /** the index of the next instruction in bc to execute */
  M_SYM(macro, "macro", impl);
  M_SYM(make_function, "make-function", impl);
  M_SYM(marshal, "marshal", impl);
  M_SYM(marshal_integer, "marshal-integer", impl);
  M_SYM(open_file, "open-file", impl); 
  M_SYM(packages, "*packages*", impl); /** all packages */
  M_SYM(pop, "pop", impl);
  M_SYM(push, "push", impl);
  M_SYM(read_bytecode_file, "read-bytecode-file", impl);
  M_SYM(read_file, "read-file", impl);
  M_SYM(strings, "strings", impl);
  M_SYM(string_concat, "string-concat", impl);
  M_SYM(set_struct_field, "set-struct-field", impl);
  M_SYM(symbol_type, "symbol-type", impl);
  M_SYM(type_of, "type-of", impl);
  M_SYM(unmarshal, "unmarshal", impl);
  M_SYM(use_package, "use-package", impl);
  M_SYM(write_bytecode_file, "write-bytecode-file", impl);
  M_SYM(write_file, "write-file", impl);
  M_SYM(write_image, "write-image", impl);
  M_SYM(external, "external", keyword);
  M_SYM(function, "function", keyword);
  M_SYM(inherited, "inherited", keyword);
  M_SYM(internal, "internal", keyword);
  M_SYM(value, "value", keyword);
  M_SYM(add, "+", lisp);
  M_SYM(apply, "apply", lisp);
  M_SYM(bin_and, "&", lisp);
  M_SYM(bin_or, "|", lisp);
  M_SYM(car, "car", lisp);
  M_SYM(cdr, "cdr", lisp);
  M_SYM(div, "/", lisp);
  M_SYM(equals, "=", lisp);
  M_SYM(find_package, "find-package", lisp);
  M_SYM(find_symbol, "find-symbol", lisp);
  M_SYM(gensym, "gensym", lisp);
  M_SYM(gt, ">", lisp);
  M_SYM(gte, ">=", lisp);
  M_SYM(fbound, "fbound?", lisp);
  M_SYM(function_macro, "function-macro?", lisp);
  M_SYM(if, "if", lisp);
  M_SYM(intern, "intern", lisp);
  M_SYM(lt, "<", lisp);
  M_SYM(list, "list", lisp);
  M_SYM(let, "let", lisp);
  M_SYM(lte, "<=", lisp);
  M_SYM(make_symbol, "make-symbol", lisp);
  M_SYM(make_package, "make-package", lisp);
  M_SYM(mul, "*", lisp);
  M_SYM(or, "or", lisp);
  M_SYM(package, "*package*", lisp); 
  M_SYM(package_symbols, "package-symbols", lisp); 
  M_SYM(package_name, "package-name", lisp);
  M_SYM(progn, "progn", lisp);
  M_SYM(print, "print", lisp);
  M_SYM(quasiquote, "quasiquote", lisp);
  M_SYM(quote, "quote", lisp);
  M_SYM(standard_input, "*standard-input*", lisp);
  M_SYM(standard_output, "*standard-output*", lisp);
  M_SYM(set, "set", lisp);
  M_SYM(set_local, "set-local", lisp);
  M_SYM(set_symbol_function, "set-symbol-function", lisp);
  M_SYM(shift_left, "<<", lisp);
  M_SYM(shift_right, ">>", lisp);
  M_SYM(symbol_name, "symbol-name", lisp);
  M_SYM(symbol_function, "symbol-function", lisp);
  M_SYM(symbol_value, "symbol-value", lisp);
  M_SYM(symbol_value_set, "symbol-value?", lisp);
  M_SYM(sub, "-", lisp);
  M_SYM(to_string, "to-string", lisp);
  M_SYM(unquote_splicing, "unquote-splicing", lisp);
  M_SYM(unquote, "unquote", lisp);
  M_SYM(while, "while", lisp);
  M_SYM(object, "object", type);
  M_SYM(char, "char", type);
  M_SYM(dynamic_array, "dynamic-array", type);
  M_SYM(dynamic_byte_array, "dynamic-byte-array", type);
  M_SYM(dynamic_library, "dynamic-library", type);
  M_SYM(enumerator, "enumerator", type);
  M_SYM(file, "file", type);
  M_SYM(fixnum, "fixnum", type);
  M_SYM(flonum, "flonum", type);
  M_SYM(foreign_function, "foreign-function", type);
  M_SYM(function, "function", type);
  M_SYM(cons, "cons", type);
  M_SYM(int, "int", type);
  M_SYM(object, "object", type);
  /* Do NOT initialize type_nil_sym again. It has been bootstrapped already -- re-initializing will cause segfaults. */
  /* Keep this warning in the code in alphabetical order wherever type_nil_sym would have been. */
  M_SYM(package, "package", type);
  M_SYM(pointer, "pointer", type);
  M_SYM(record, "record", type);
  M_SYM(string, "string", type);
  M_SYM(struct, "struct", type);
  M_SYM(symbol, "symbol", type);
  M_SYM(t, "t", type);
  M_SYM(type, "type", type);
  M_SYM(ufixnum, "ufixnum", type);
  M_SYM(uint, "uint", type);
  M_SYM(uint16, "uint16", type);
  M_SYM(uint32, "uint32", type);
  M_SYM(uint8, "uint8", type);
  M_SYM(vec2, "vec2", type);
  M_SYM(void, "void", type);

  /* add symbols to lisp package to make them visible */
  PACKAGE_SYMBOLS(gis->lisp_package) = cons(gis->type_function_sym, PACKAGE_SYMBOLS(gis->lisp_package));
  PACKAGE_SYMBOLS(gis->lisp_package) = cons(gis->impl_macro_sym, PACKAGE_SYMBOLS(gis->lisp_package));
  PACKAGE_SYMBOLS(gis->lisp_package) = cons(gis->impl_open_file_sym, PACKAGE_SYMBOLS(gis->lisp_package));
  PACKAGE_SYMBOLS(gis->lisp_package) = cons(gis->impl_close_file_sym, PACKAGE_SYMBOLS(gis->lisp_package));
  PACKAGE_SYMBOLS(gis->lisp_package) = cons(gis->impl_call_sym, PACKAGE_SYMBOLS(gis->lisp_package));
  PACKAGE_SYMBOLS(gis->lisp_package) = cons(gis->impl_symbol_type_sym, PACKAGE_SYMBOLS(gis->lisp_package));
  PACKAGE_SYMBOLS(gis->lisp_package) = cons(gis->impl_alloc_struct_sym, PACKAGE_SYMBOLS(gis->lisp_package));
  PACKAGE_SYMBOLS(gis->lisp_package) = cons(gis->impl_set_struct_field_sym, PACKAGE_SYMBOLS(gis->lisp_package));
  PACKAGE_SYMBOLS(gis->lisp_package) = cons(gis->impl_struct_field_sym, PACKAGE_SYMBOLS(gis->lisp_package));
  PACKAGE_SYMBOLS(gis->lisp_package) = cons(gis->impl_type_of_sym, PACKAGE_SYMBOLS(gis->lisp_package));
  PACKAGE_SYMBOLS(gis->lisp_package) = cons(gis->impl_and_sym, PACKAGE_SYMBOLS(gis->lisp_package));
  PACKAGE_SYMBOLS(gis->lisp_package) = cons(gis->impl_dynamic_byte_array_as_string_sym, PACKAGE_SYMBOLS(gis->lisp_package));
  PACKAGE_SYMBOLS(gis->lisp_package) = cons(gis->impl_dynamic_byte_array_concat_sym, PACKAGE_SYMBOLS(gis->lisp_package));
  PACKAGE_SYMBOLS(gis->lisp_package) = cons(gis->impl_dynamic_byte_array_get_sym, PACKAGE_SYMBOLS(gis->lisp_package));
  PACKAGE_SYMBOLS(gis->lisp_package) = cons(gis->impl_dynamic_byte_array_insert_sym, PACKAGE_SYMBOLS(gis->lisp_package));
  PACKAGE_SYMBOLS(gis->lisp_package) = cons(gis->impl_dynamic_byte_array_length_sym, PACKAGE_SYMBOLS(gis->lisp_package));
  PACKAGE_SYMBOLS(gis->lisp_package) = cons(gis->impl_dynamic_byte_array_set_sym, PACKAGE_SYMBOLS(gis->lisp_package));
  PACKAGE_SYMBOLS(gis->lisp_package) = cons(gis->impl_dynamic_byte_array_push_sym, PACKAGE_SYMBOLS(gis->lisp_package));
  PACKAGE_SYMBOLS(gis->lisp_package) = cons(gis->impl_dynamic_byte_array_pop_sym, PACKAGE_SYMBOLS(gis->lisp_package));
  PACKAGE_SYMBOLS(gis->lisp_package) = cons(gis->impl_byte_stream_sym, PACKAGE_SYMBOLS(gis->lisp_package));
  PACKAGE_SYMBOLS(gis->lisp_package) = cons(gis->impl_byte_stream_peek_sym, PACKAGE_SYMBOLS(gis->lisp_package));
  PACKAGE_SYMBOLS(gis->lisp_package) = cons(gis->impl_byte_stream_peek_byte_sym, PACKAGE_SYMBOLS(gis->lisp_package));
  PACKAGE_SYMBOLS(gis->lisp_package) = cons(gis->impl_byte_stream_read_sym, PACKAGE_SYMBOLS(gis->lisp_package));
  PACKAGE_SYMBOLS(gis->lisp_package) = cons(gis->impl_byte_stream_has_sym, PACKAGE_SYMBOLS(gis->lisp_package));

  symbol_set_value(BSYM(impl, continue), BSYM(impl, continue));
  symbol_set_value(BSYM(type, t), BSYM(type, t)); /* t has itself as its value */

  use_package(gis->impl_package, gis->type_package);
  use_package(gis->lisp_package, gis->type_package);
  use_package(gis->user_package, gis->lisp_package);

  /* Initialize types */
  /* IMPORTANT -- the ordering these types are added to the types array must match the order they are defined in bug.h for the type enum */
  /* type_of cannot be called before this is setup */
  gis->types = dynamic_array(64);
#define GIS_TYPE(t, sym, can_instantiate) \
  t = type(sym, NIL, can_instantiate, 1); \
  symbol_set_type(sym, t);
#define GIS_UNINSTANTIATABLE_TYPE(type, sym) \
  GIS_TYPE(type, sym, 0)
#define GIS_INSTANTIATABLE_TYPE(type, sym) \
  GIS_TYPE(type, sym, 1)

  GIS_INSTANTIATABLE_TYPE(gis->fixnum_type, gis->type_fixnum_sym);
  GIS_INSTANTIATABLE_TYPE(gis->ufixnum_type, gis->type_ufixnum_sym);
  GIS_INSTANTIATABLE_TYPE(gis->flonum_type, gis->type_flonum_sym);
  GIS_INSTANTIATABLE_TYPE(gis->symbol_type, gis->type_symbol_sym);
  GIS_INSTANTIATABLE_TYPE(gis->dynamic_array_type, gis->type_dynamic_array_sym);
  GIS_INSTANTIATABLE_TYPE(gis->string_type, gis->type_string_sym);
  GIS_INSTANTIATABLE_TYPE(gis->package_type, gis->type_package_sym);
  GIS_INSTANTIATABLE_TYPE(gis->dynamic_byte_array_type, gis->type_dynamic_byte_array_sym);
  GIS_INSTANTIATABLE_TYPE(gis->function_type, gis->type_function_sym);
  GIS_INSTANTIATABLE_TYPE(gis->file_type, gis->type_file_sym);
  GIS_INSTANTIATABLE_TYPE(gis->enumerator_type, gis->type_enumerator_sym);
  /* TODO: remove "nil-type" ? */
  GIS_UNINSTANTIATABLE_TYPE(gis->nil_type, gis->type_nil_sym); /* nil is uninstantiatable there is only one value of nil, that is pre-defined. We don't want users to be able to make more than one nil. */
  GIS_INSTANTIATABLE_TYPE(gis->record_type, gis->type_record_sym);
  GIS_INSTANTIATABLE_TYPE(gis->vec2_type, gis->type_vec2_sym);
  GIS_INSTANTIATABLE_TYPE(gis->dynamic_library_type, gis->type_dynamic_library_sym);
  GIS_INSTANTIATABLE_TYPE(gis->foreign_function_type, gis->type_foreign_function_sym);
  GIS_INSTANTIATABLE_TYPE(gis->pointer_type, gis->type_pointer_sym);
  GIS_INSTANTIATABLE_TYPE(gis->type_type, gis->type_type_sym);

  /* cons must be defined as the last of the object_types -- it has a special form for the w0 part of an object */
  /* cons is uninstantiatable because there is no type id that maps to it */
  GIS_UNINSTANTIATABLE_TYPE(gis->cons_type, gis->type_cons_sym);

  GIS_UNINSTANTIATABLE_TYPE(gis->void_type, gis->type_void_sym);
  GIS_UNINSTANTIATABLE_TYPE(gis->char_type, gis->type_char_sym);
  GIS_UNINSTANTIATABLE_TYPE(gis->int_type, gis->type_int_sym);
  GIS_UNINSTANTIATABLE_TYPE(gis->uint8_type, gis->type_uint8_sym);
  GIS_UNINSTANTIATABLE_TYPE(gis->uint16_type, gis->type_uint16_sym);
  GIS_UNINSTANTIATABLE_TYPE(gis->uint32_type, gis->type_uint32_sym);
  GIS_UNINSTANTIATABLE_TYPE(gis->uint_type, gis->type_uint_sym);
  GIS_UNINSTANTIATABLE_TYPE(gis->object_type, gis->type_object_sym);

#define GIS_BUILTIN(builtin, sym, nargs) \
  builtin = function(NIL, NIL, nargs);   \
  FUNCTION_IS_BUILTIN(builtin) = 1;      \
  FUNCTION_NARGS(builtin) = nargs;       \
  FUNCTION_NAME(builtin) = sym;          \
  symbol_set_function(FUNCTION_NAME(builtin), builtin);

#define GIS_BUILTIN_S(builtin, package, symbol_name, nargs)   \
  GIS_BUILTIN(builtin, symbol(string(symbol_name)), nargs);   \
  SYMBOL_PACKAGE(FUNCTION_NAME(builtin)) = package;                              \
  PACKAGE_SYMBOLS(package) =                                  \
      cons(FUNCTION_NAME(builtin), PACKAGE_SYMBOLS(package)); \
  symbol_export(FUNCTION_NAME(builtin));

  /* all builtin functions go here */
  GIS_BUILTIN(gis->alloc_struct_builtin, gis->impl_alloc_struct_sym, 1);
  GIS_BUILTIN(gis->apply_builtin, gis->lisp_apply_sym, 2);
  GIS_BUILTIN(gis->byte_stream_builtin, gis->impl_byte_stream_sym, 1);
  GIS_BUILTIN(gis->byte_stream_peek_builtin, gis->impl_byte_stream_peek_sym, 2);
  GIS_BUILTIN(gis->byte_stream_peek_byte_builtin, gis->impl_byte_stream_peek_byte_sym, 1);
  GIS_BUILTIN(gis->byte_stream_read_builtin, gis->impl_byte_stream_read_sym, 2);
  GIS_BUILTIN(gis->byte_stream_has_builtin, gis->impl_byte_stream_has_sym, 1);
  GIS_BUILTIN(gis->debugger_builtin, gis->impl_debugger_sym, 0);
  GIS_BUILTIN(gis->dynamic_byte_array_as_string_builtin, gis->impl_dynamic_byte_array_as_string_sym, 1);
  GIS_BUILTIN(gis->dynamic_array_builtin, gis->type_dynamic_array_sym, 1);
  GIS_BUILTIN_S(gis->dynamic_array_get_builtin, gis->impl_package, "dynamic-array-get", 2);
  PACKAGE_SYMBOLS(gis->lisp_package) = cons(FUNCTION_NAME(gis->dynamic_array_get_builtin), PACKAGE_SYMBOLS(gis->lisp_package));
  GIS_BUILTIN(gis->dynamic_array_set_builtin, gis->lisp_dynamic_array_set_sym, 3);
  GIS_BUILTIN(gis->dynamic_array_length_builtin, gis->lisp_dynamic_array_length_sym, 1);
  GIS_BUILTIN(gis->dynamic_array_push_builtin, gis->lisp_dynamic_array_push_sym, 2);
  GIS_BUILTIN(gis->dynamic_array_pop_builtin, gis->lisp_dynamic_array_pop_sym, 1);
  GIS_BUILTIN(gis->dynamic_array_concat_builtin, gis->lisp_dynamic_array_concat_sym, 2);
  GIS_BUILTIN(gis->dynamic_byte_array_builtin, gis->type_dynamic_byte_array_sym, 1);
  GIS_BUILTIN(gis->dynamic_byte_array_concat_builtin, gis->impl_dynamic_byte_array_concat_sym, 2);
  GIS_BUILTIN(gis->dynamic_byte_array_get_builtin, gis->impl_dynamic_byte_array_get_sym, 2);
  GIS_BUILTIN(gis->dynamic_byte_array_insert_builtin, gis->impl_dynamic_byte_array_insert_sym, 3);
  GIS_BUILTIN(gis->dynamic_byte_array_length_builtin, gis->impl_dynamic_byte_array_length_sym, 1);
  GIS_BUILTIN(gis->dynamic_byte_array_set_builtin, gis->impl_dynamic_byte_array_set_sym, 3);
  GIS_BUILTIN(gis->dynamic_byte_array_push_builtin, gis->impl_dynamic_byte_array_push_sym, 2);
  GIS_BUILTIN(gis->dynamic_byte_array_pop_builtin, gis->impl_dynamic_byte_array_pop_sym, 1);
  GIS_BUILTIN(gis->change_directory_builtin, gis->impl_change_directory_sym, 1) /* takes the new directory */
  GIS_BUILTIN(gis->close_file_builtin, gis->impl_close_file_sym, 1);
  GIS_BUILTIN(gis->dynamic_library_builtin, gis->type_dynamic_library_sym, 1)  /* takes the path */
  GIS_BUILTIN(gis->get_current_working_directory_builtin, gis->impl_get_current_working_directory_sym, 0)
  GIS_BUILTIN(gis->gensym_builtin, gis->lisp_gensym_sym, 0)
  GIS_BUILTIN(gis->fbound_builtin, gis->lisp_fbound_sym, 1);
  GIS_BUILTIN(gis->function_macro_builtin, gis->lisp_function_macro_sym, 1);
  GIS_BUILTIN(gis->find_package_builtin, gis->lisp_find_package_sym, 1);
  GIS_BUILTIN(gis->find_symbol_builtin, gis->lisp_find_symbol_sym, 2);
  GIS_BUILTIN(gis->foreign_function_builtin, gis->type_foreign_function_sym, 4) /* takes the dlib, the name, and the parameter types */
  GIS_BUILTIN(gis->function_code_builtin, gis->impl_function_code_sym, 1)
  GIS_BUILTIN(gis->intern_builtin, gis->lisp_intern_sym, 2);
  GIS_BUILTIN(gis->make_function_builtin, gis->impl_make_function_sym, 8);
  GIS_BUILTIN(gis->make_symbol_builtin, gis->lisp_make_symbol_sym, 1);
  GIS_BUILTIN(gis->make_package_builtin, gis->lisp_make_package_sym, 3);
  GIS_BUILTIN(gis->marshal_builtin, gis->impl_marshal_sym, 2);
  GIS_BUILTIN(gis->marshal_integer_builtin, gis->impl_marshal_integer_sym, 3);
  GIS_BUILTIN(gis->open_file_builtin, gis->impl_open_file_sym, 2);
  GIS_BUILTIN(gis->package_symbols_builtin, gis->lisp_package_symbols_sym, 1);
  GIS_BUILTIN(gis->package_name_builtin, gis->lisp_package_name_sym, 1);
  GIS_BUILTIN(gis->type_of_builtin, gis->impl_type_of_sym, 1)
  GIS_BUILTIN(gis->read_bytecode_file_builtin, gis->impl_read_bytecode_file_sym, 1);
  GIS_BUILTIN(gis->read_file_builtin, gis->impl_read_file_sym, 1);
  GIS_BUILTIN(gis->define_struct_builtin, gis->impl_define_struct_sym, 2);
  GIS_BUILTIN(gis->symbol_name_builtin, gis->lisp_symbol_name_sym, 1);
  GIS_BUILTIN(gis->symbol_type_builtin, gis->impl_symbol_type_sym, 1);
  GIS_BUILTIN(gis->symbol_value_set_builtin, gis->lisp_symbol_value_set_sym, 1);
  GIS_BUILTIN(gis->string_concat_builtin, gis->impl_string_concat_sym, 2);
  GIS_BUILTIN(gis->struct_field_builtin, gis->impl_struct_field_sym, 2);
  GIS_BUILTIN(gis->set_struct_field_builtin, gis->impl_set_struct_field_sym, 3);
  GIS_BUILTIN(gis->to_string_builtin, gis->lisp_to_string_sym, 1);
  GIS_BUILTIN(gis->unmarshal_builtin, gis->impl_unmarshal_sym, 1);
  GIS_BUILTIN(gis->use_package_builtin, gis->impl_use_package_sym, 1) /* takes name of package */
  GIS_BUILTIN(gis->write_bytecode_file_builtin, gis->impl_write_bytecode_file_sym, 2);
  GIS_BUILTIN(gis->write_file_builtin, gis->impl_write_file_sym, 2);
  GIS_BUILTIN(gis->write_image_builtin, gis->impl_write_image_sym, 1);

  /* initialize set interpreter state */
  gis->data_stack = dynamic_array(10);
  symbol_set_value(BSYM(impl, data_stack), gis->data_stack);
  gis->call_stack = dynamic_array(10);
  symbol_set_value(BSYM(impl, call_stack), gis->call_stack); /* using a dynamic array to make looking up arguments on the stack faster */

  symbol_set_value(BSYM(lisp, package), BPAC(user));
  symbol_set_value(BSYM(impl, packages),
                    cons(BPAC(lisp),
                      cons(BPAC(user),
                        cons(BPAC(keyword),
                          cons(BPAC(impl),
                            cons(BPAC(type), NIL))))));

  symbol_set_value(BSYM(impl, f), NIL); /* the function being executed */
  symbol_set_value(BSYM(impl, i), ufixnum(0)); /* the instruction index */

  /* i've found this to be an easy solution to solving an issue with looking up locals. I put the function at the top of the call-stack (makes it easy to pop off items) but it makes it harder to handle set_locals get_locals */
  dynamic_array_push(gis->call_stack, NIL);
    dynamic_array_push(gis->call_stack, NIL);

  gis->standard_out = file_stdout();
  gis->standard_in = file_stdin();

  symbol_set_value(BSYM(lisp, standard_input), gis->standard_in);
  symbol_set_value(BSYM(lisp, standard_output), gis->standard_out);

  gis->gensym_counter = 0;

  /* Load core.bug */
  gis->loaded_core = 0;
  if (load_core) {
    eval(read_bytecode_file(open_file(string("../bootstrap/compiler.bc"), string("rb"))), NIL);
    repl = find_symbol(string("repl"), gis->user_package, 1);
    if (!SYMBOL_FUNCTION_IS_SET(repl)) {
      printf("You the compiler must have a function called 'repl'.");
      exit(1);
    }
    call_function(symbol_get_function(repl), NIL);
    gis->loaded_core = 1;
  }
}

/* For handling flonums/fixnums/ufixnums */
flonum_t flonum_t_value(struct object *o) {
  struct object *t;
  t = type_of(o);
  if (t == gis->flonum_type)
    return FLONUM_VALUE(o);
  if (t == gis->fixnum_type)
    return FIXNUM_VALUE(o);
  if (t == gis->ufixnum_type)
    return UFIXNUM_VALUE(o);
  printf("Cannot get flonum_t value of given type.");
  PRINT_STACK_TRACE_AND_QUIT();
}

void flonum_convert(struct object *o) {
  struct object *t;
  t = type_of(o);
  if (t == gis->flonum_type) {
    return;
  } else if (t == gis->fixnum_type) {
    OBJECT_TYPE(o) = type_flonum;
    FLONUM_VALUE(o) = FIXNUM_VALUE(o);
    return;
  } else if (t == gis->ufixnum_type) {
    OBJECT_TYPE(o) = type_flonum;
    FLONUM_VALUE(o) = UFIXNUM_VALUE(o);
    return;
  }
  printf("Cannot get flonum_t value of given type.");
  PRINT_STACK_TRACE_AND_QUIT();
}

void add_package(struct object *package) {
  symbol_set_value(gis->impl_packages_sym, cons(package, symbol_get_value(gis->impl_packages_sym)));
}

void use_package(struct object *p0, struct object *p1) {
  struct object *cursor, *symbols;
  symbols = PACKAGE_SYMBOLS(p0);
  cursor = PACKAGE_SYMBOLS(p1);
  while (cursor != NIL) {
    symbols = cons(CONS_CAR(cursor), symbols);
    cursor = CONS_CDR(cursor);
  }
  PACKAGE_SYMBOLS(p0) = symbols;
}

struct object *find_package(struct object *name) {
  struct object *package = symbol_get_value(gis->impl_packages_sym);
  while (package != NIL) {
    if (equals(PACKAGE_NAME(CONS_CAR(package)), name)) {
      return CONS_CAR(package);
    }
    package = CONS_CDR(package);
  }
  return NIL;
} 

struct object *write_file(struct object *file, struct object *o) {
  struct object *t;
  fixnum_t nmembers;
  OT("write_file", 0, file, type_file);
  t = type_of(o);
  if (t == gis->dynamic_byte_array_type || t == gis->string_type) {
    nmembers = fwrite(DYNAMIC_BYTE_ARRAY_BYTES(o), sizeof(char),
                      DYNAMIC_BYTE_ARRAY_LENGTH(o), FILE_FP(file));
    if (nmembers != DYNAMIC_BYTE_ARRAY_LENGTH(o)) {
      printf("BC: failed to write dynamic-byte-array to file.");
      PRINT_STACK_TRACE_AND_QUIT();
    }
  } else {
    printf("BC: can not write object of type %s to a file.",
           type_name_of_cstr(o));
    PRINT_STACK_TRACE_AND_QUIT();
  }
  return NIL;
}

/* reads the entire file contents */
struct object *read_file(struct object *file) {
  struct object *dba;
  long file_size;

  OT("read_file", 0, file, type_file);

  fseek(FILE_FP(file), 0, SEEK_END);
  file_size = ftell(FILE_FP(file));
  fseek(FILE_FP(file), 0, SEEK_SET);

  dba = dynamic_byte_array(file_size);
  fread(DYNAMIC_BYTE_ARRAY_BYTES(dba), 1, file_size, FILE_FP(file));
  DYNAMIC_BYTE_ARRAY_LENGTH(dba) = file_size;
  return dba;
}

/* evaluates the given bytecode starting at the given instruction index */
struct object *eval_at_instruction(struct object *f, ufixnum_t i, struct object *args) {
  ufixnum_t j;
  struct object *cursor;
  symbol_set_value(gis->impl_f_sym, f);
  UFIXNUM_VALUE(symbol_get_value(gis->impl_i_sym)) = i;
  j = 0;
  cursor = args;
  while (j < FUNCTION_NARGS(f)) {
    if (cursor == NIL) {
      printf("Not enough arguments provided. Expected %lu but got %lu.\n", (unsigned long)FUNCTION_NARGS(f), (unsigned long)j); 
      PRINT_STACK_TRACE_AND_QUIT();
    }
    dynamic_array_push(gis->call_stack, CONS_CAR(cursor));
    cursor = CONS_CDR(cursor);
    ++j;
  }
  /* prepare the call stack by making room for stack args */
  /* TODO: this can be optimized to just increment the length instead of pushing NILs */
  for (j = 0; j < FUNCTION_STACK_SIZE(f) - FUNCTION_NARGS(f); ++j)
    dynamic_array_push(gis->call_stack, NIL);
  return run(gis);
}
/* evaluates the bytecode starting at instruction index 0 */
struct object *eval(struct object *bc, struct object* args) {
  return eval_at_instruction(bc, 0, args);
}

/*
 * Bytecode Interpreter
 */
void push(struct object *object) {
  dynamic_array_push(gis->data_stack, object);
}

struct object *pop() {
  return dynamic_array_pop(gis->data_stack);
}

struct object *peek() {
  return DYNAMIC_ARRAY_VALUES(
      gis->data_stack)[DYNAMIC_ARRAY_LENGTH(gis->data_stack) - 1];
}

void dup() {
  dynamic_array_push(
      gis->data_stack,
      DYNAMIC_ARRAY_VALUES(
          gis->data_stack)[DYNAMIC_ARRAY_LENGTH(gis->data_stack) - 1]);
}

#define GET_LOCAL(n)                          \
  dynamic_array_get_ufixnum_t(                \
      gis->call_stack,                        \
      DYNAMIC_ARRAY_LENGTH(gis->call_stack) - \
          (FUNCTION_STACK_SIZE(symbol_get_value(gis->impl_f_sym)) + 2 - (n)))

/* minus two to skip the f and i */
#define SET_LOCAL(n, x)                                                     \
  dynamic_array_set_ufixnum_t(                                              \
      gis->call_stack,                                                      \
      DYNAMIC_ARRAY_LENGTH(gis->call_stack) -                               \
          (FUNCTION_STACK_SIZE(symbol_get_value(gis->impl_f_sym)) + 2 - (n)), \
      (x))

/** evaluates a builtin function */
void eval_builtin(struct object *f) {
  struct object *t0, *t1;
  if (f == gis->use_package_builtin) {
    /* TODO */
  } else if (f == gis->fbound_builtin) {
    OT("fbound?", 0, GET_LOCAL(0), type_symbol);
    push(SYMBOL_FUNCTION_IS_SET(GET_LOCAL(0)) ? T : NIL);
  } else if (f == gis->function_macro_builtin) {
    push((OBJECT_TYPE(f) == type_function && FUNCTION_IS_MACRO(GET_LOCAL(0))) ? T : NIL);
  } else if (f == gis->find_package_builtin) {
    push(find_package(string_designator(GET_LOCAL(0))));
  } else if (f == gis->symbol_type_builtin) {
    push(symbol_get_type(GET_LOCAL(0)));
  } else if (f == gis->type_of_builtin) {
    push(type_of(GET_LOCAL(0)));
  } else if (f == gis->dynamic_byte_array_as_string_builtin) {
    OBJECT_TYPE(GET_LOCAL(0)) = type_string;
    push(GET_LOCAL(0));
  } else if (f == gis->dynamic_array_builtin) {
    OT("dynamic-array", 0, GET_LOCAL(0), type_fixnum);
    push(dynamic_array(FIXNUM_VALUE(GET_LOCAL(0))));
  } else if (f == gis->dynamic_byte_array_builtin) {
    OT("dynamic-byte-array", 0, GET_LOCAL(0), type_fixnum);
    push(dynamic_byte_array(FIXNUM_VALUE(GET_LOCAL(0))));
  } else if (f == gis->to_string_builtin) {
    push(to_string(GET_LOCAL(0)));
  } else if (f == gis->symbol_value_set_builtin) {
    OT("symbol-value?", 0, GET_LOCAL(0), type_symbol);
    push(SYMBOL_VALUE_IS_SET(GET_LOCAL(0)) ? T : NIL);
  } else if (f == gis->dynamic_array_get_builtin) {
    push(dynamic_array_get(GET_LOCAL(0), GET_LOCAL(1)));
  } else if (f == gis->dynamic_array_set_builtin) {
    dynamic_array_set(GET_LOCAL(0), GET_LOCAL(1), GET_LOCAL(2));
    push(NIL);
  } else if (f == gis->dynamic_array_length_builtin) {
    push(dynamic_array_length(GET_LOCAL(0)));
  } else if (f == gis->write_image_builtin) {
    write_image(GET_LOCAL(0));
    push(NIL);
  } else if (f == gis->dynamic_array_push_builtin) {
    dynamic_array_push(GET_LOCAL(0), GET_LOCAL(1));
    push(NIL);
  } else if (f == gis->dynamic_array_pop_builtin) {
    push(dynamic_array_pop(GET_LOCAL(0)));
  } else if (f == gis->dynamic_array_concat_builtin) {
    push(dynamic_array_concat(GET_LOCAL(0), GET_LOCAL(1)));
  } else if (f == gis->dynamic_byte_array_concat_builtin) {
    push(dynamic_byte_array_concat(GET_LOCAL(0), GET_LOCAL(1)));
  } else if (f == gis->dynamic_byte_array_get_builtin) {
    OT("dynamic-byte-array-get", 1, GET_LOCAL(1), type_fixnum);
    push(fixnum(dynamic_byte_array_get(GET_LOCAL(0), FIXNUM_VALUE(GET_LOCAL(1)))));
  } else if (f == gis->dynamic_byte_array_insert_builtin) {
    OT("dynamic-byte-array-insert", 1, GET_LOCAL(1), type_fixnum);
    OT("dynamic-byte-array-insert", 2, GET_LOCAL(2), type_fixnum);
    dynamic_byte_array_insert_char(GET_LOCAL(0), FIXNUM_VALUE(GET_LOCAL(1)), FIXNUM_VALUE(GET_LOCAL(2)));
    push(NIL);
  } else if (f == gis->dynamic_byte_array_length_builtin) {
    push(dynamic_byte_array_length(GET_LOCAL(0)));
  } else if (f == gis->dynamic_byte_array_set_builtin) {
    OT("dynamic-byte-array-set", 1, GET_LOCAL(1), type_fixnum);
    OT("dynamic-byte-array-set", 2, GET_LOCAL(2), type_fixnum);
    dynamic_byte_array_set(GET_LOCAL(0), FIXNUM_VALUE(GET_LOCAL(1)), FIXNUM_VALUE(GET_LOCAL(2)));
    push(NIL);
  } else if (f == gis->dynamic_byte_array_push_builtin) {
    push(dynamic_byte_array_push(GET_LOCAL(0), GET_LOCAL(1)));
  } else if (f == gis->dynamic_byte_array_pop_builtin) {
    push(dynamic_byte_array_pop(GET_LOCAL(0)));
  } else if (f == gis->define_struct_builtin) {
    push(type(GET_LOCAL(0), GET_LOCAL(1), 1, 0));
  } else if (f == gis->change_directory_builtin) {
    change_directory(GET_LOCAL(0));
    push(NIL);
  } else if (f == gis->get_current_working_directory_builtin) {
    push(get_current_working_directory());
  } else if (f == gis->alloc_struct_builtin) {
    push(alloc_struct(GET_LOCAL(0), 1));
  } else if (f == gis->gensym_builtin) {
    push(symbol(string_concat(string("gensym-"), to_string(ufixnum(gis->gensym_counter)))));
    ++gis->gensym_counter;
  } else if (f == gis->function_code_builtin) {
    OT("function-code", 0, GET_LOCAL(0), type_function);
    push(FUNCTION_CODE(GET_LOCAL(0)));
  } else if (f == gis->apply_builtin) {
    OT_LIST("apply", 1, GET_LOCAL(1));
    t0 = GET_LOCAL(0);
    if (type_of(t0) == gis->symbol_type) {
      t0 = symbol_get_function(t0);
    }
    if (type_of(t0) != gis->function_type) {
      printf("Apply must be given a function or symbol that has a function.\n");
      PRINT_STACK_TRACE_AND_QUIT();
    }
    t1 = GET_LOCAL(1); /* we must store the local now -- as we will be setting f/i to NIL */
    symbol_set_value(gis->impl_f_sym, NIL);
    symbol_set_value(gis->impl_i_sym, NIL);
    push(call_function(t0, t1));
    symbol_set_value(gis->impl_f_sym, gis->apply_builtin); /* we must restore this before returning so that op_return knows we are returning from apply */
    /* (render-html `(div ((a 3) (b 1)) (span) (span "hello")))
      There is a big call-stack leak and a small data-stack leak. why?!
      it seems to only happen when  a string is passed 
    */
  } else if (f == gis->struct_field_builtin) {
    push(struct_field(GET_LOCAL(0), GET_LOCAL(1)));
  } else if (f == gis->set_struct_field_builtin) {
    set_struct_field(GET_LOCAL(0), GET_LOCAL(1), GET_LOCAL(2));
    push(NIL);
  } else if (f == gis->symbol_name_builtin) {
    OT("symbol-name", 0, GET_LOCAL(0), type_symbol);
    push(SYMBOL_NAME(GET_LOCAL(0)));
  } else if (f == gis->dynamic_library_builtin) {
    push(dlib(GET_LOCAL(0)));
  } else if (f == gis->byte_stream_builtin) {
    push(byte_stream_lift(GET_LOCAL(0)));
  } else if (f == gis->byte_stream_peek_builtin) {
    push(byte_stream_peek(GET_LOCAL(0), FIXNUM_VALUE(GET_LOCAL(1)))); /* TODO: support more than just fixnum */
  } else if (f == gis->byte_stream_peek_byte_builtin) {
    push(fixnum(byte_stream_peek_byte(GET_LOCAL(0))));
  } else if (f == gis->byte_stream_has_builtin) {
    push(byte_stream_has(GET_LOCAL(0)) ? T : NIL);
  } else if (f == gis->byte_stream_read_builtin) {
    push(byte_stream_read(GET_LOCAL(0), FIXNUM_VALUE(GET_LOCAL(1)))); /* TODO: support more than just fixnum */
  } else if (f == gis->close_file_builtin) {
    close_file(GET_LOCAL(0));
    push(NIL);
  } else if (f == gis->open_file_builtin) {
    push(open_file(GET_LOCAL(0), GET_LOCAL(1)));
  } else if (f == gis->read_bytecode_file_builtin) {
    push(read_bytecode_file(GET_LOCAL(0)));
  } else if (f == gis->read_file_builtin) {
    push(read_file(GET_LOCAL(0)));
  } else if (f == gis->marshal_builtin) {
    push(marshal(GET_LOCAL(0), GET_LOCAL(1), NULL));
  } else if (f == gis->marshal_integer_builtin) {
    push(marshal_ufixnum(GET_LOCAL(0), GET_LOCAL(1), GET_LOCAL(2) == NIL ? 0 : 1));
  } else if (f == gis->unmarshal_builtin) {
    push(unmarshal(GET_LOCAL(0), NULL));
  } else if (f == gis->write_bytecode_file_builtin) {
    write_bytecode_file(GET_LOCAL(0), GET_LOCAL(1));
    push(NIL);
  } else if (f == gis->write_file_builtin) {
    write_file(GET_LOCAL(0), GET_LOCAL(1));
    push(NIL);
  } else if (f == gis->string_concat_builtin) {
    push(string_concat_external(GET_LOCAL(0), GET_LOCAL(1)));
  } else if (f == gis->foreign_function_builtin) {
    push(foreign_function(GET_LOCAL(0), GET_LOCAL(1), GET_LOCAL(2), GET_LOCAL(3)));
  } else if (f == gis->package_symbols_builtin) {
    OT("package-symbols", 0, GET_LOCAL(0), type_package);
    push(PACKAGE_SYMBOLS(GET_LOCAL(0)));
  } else if (f == gis->intern_builtin) {
    OT("intern", 0, GET_LOCAL(0), type_string);
    OT("intern", 1, GET_LOCAL(1), type_package);
    push(intern(GET_LOCAL(0), GET_LOCAL(1)));
  } else if (f == gis->make_symbol_builtin) {
    OT("make-symbol", 0, GET_LOCAL(0), type_string);
    push(symbol(GET_LOCAL(0)));
  } else if (f == gis->package_name_builtin) {
    OT("package-name", 0, GET_LOCAL(0), type_package);
    push(PACKAGE_NAME(GET_LOCAL(0)));
  } else if (f == gis->package_symbols_builtin) {
    OT("package-symbols", 0, GET_LOCAL(0), type_package);
    push(PACKAGE_SYMBOLS(GET_LOCAL(0)));
  } else if (f == gis->make_function_builtin) {
    OT("make-function", 0, GET_LOCAL(0), type_symbol); /* name */
    OT2("make-function", 1, GET_LOCAL(1), type_string, type_symbol); /* docstring - string or nil*/
    OT("make-function", 2, GET_LOCAL(2), type_fixnum); /* stack size */
    OT("make-function", 3, GET_LOCAL(3), type_fixnum); /* nargs */
    /* local 4 - accepts all */
    /* local 5 - is macro */
    OT("make-function", 6, GET_LOCAL(6), type_dynamic_byte_array); /* code */
    OT("make-function", 7, GET_LOCAL(7), type_dynamic_array); /* constants */

    t0 = function(GET_LOCAL(7), GET_LOCAL(6), FIXNUM_VALUE(GET_LOCAL(2)));
    FUNCTION_NAME(t0) = GET_LOCAL(0);
    FUNCTION_DOCSTRING(t0) = GET_LOCAL(1);
    FUNCTION_NARGS(t0) = FIXNUM_VALUE(GET_LOCAL(3));
    FUNCTION_IS_MACRO(t0) = GET_LOCAL(5) == NIL ? 0 : 1;
    FUNCTION_ACCEPTS_ALL(t0) = GET_LOCAL(4) == NIL ? 0 : 1;
    push(t0);
  } else if (f == gis->find_symbol_builtin) {
    OT("find-symbol", 0, GET_LOCAL(0), type_string);
    OT("find-symbol", 0, GET_LOCAL(1), type_package);
    push(find_symbol(GET_LOCAL(0), GET_LOCAL(1), 1));
  } else if (f == gis->make_package_builtin) {
    OT("make-package", 0, GET_LOCAL(0), type_string);
    /* arg 1 is not used - reserved for implementing package nicknames later */
    OT_LIST("make-package", 2, GET_LOCAL(2)); /* use package list */
    /* TODO check for naming conflicts */
    t0 = package(GET_LOCAL(0));
    t1 = GET_LOCAL(2);
    while (t1 != NIL) {
      use_package(t0, CONS_CAR(t1));
      t1 = CONS_CDR(t1);
    }
    push(t0);
  } else {
    printf("Unknown builtin\n");
    print(f);
    PRINT_STACK_TRACE_AND_QUIT();
  }
}

/* (7F)_16 is (0111 1111)_2, it extracts the numerical value from the temporary
 */
/* (80)_16 is (1000 0000)_2, it extracts the flag from the temporary */
#define READ_OP_ARG()                                                  \
  if (UFIXNUM_VALUE(i) >= byte_count) {                                \
    printf("BC: expected an op code argument, but bytecode ended.\n"); \
    break;                                                             \
  }                                                                    \
  t0 = code->bytes[++UFIXNUM_VALUE(i)];                                \
  a0 = t0 & 0x7F;                                                      \
  op_arg_byte_count = 1;                                               \
  while (t0 & 0x80) {                                                  \
    if (UFIXNUM_VALUE(i) >= byte_count) {                              \
      printf(                                                          \
          "BC: expected an extended op code argument, but bytecode "   \
          "ended.\n");                                                 \
      break;                                                           \
    }                                                                  \
    t0 = code->bytes[++UFIXNUM_VALUE(i)];                              \
    a0 = ((t0 & 0x7F) << (7 * op_arg_byte_count++)) | a0;              \
  }

#define READ_OP_JUMP_ARG()                                     \
  sa0 = code->bytes[++UFIXNUM_VALUE(i)] << 8; \
  sa0 = code->bytes[++UFIXNUM_VALUE(i)] | sa0;

#define READ_CONST_ARG()                                               \
  READ_OP_ARG()                                                        \
  if (a0 >= constants_length) {                                        \
    printf(                                                            \
        "BC: bytecode attempted to access an out of bounds index "     \
        "%ld in the constants vector, but only have %ld constants.\n", \
        a0, constants_length);                                         \
    break;                                                             \
  }                                                                    \
  c0 = constants->values[a0];

/* Gets the item in the data stack at the index i. (e.g. STACK_I(0) is the top
 * of the stack) */
#define STACK_I(i)                      \
  DYNAMIC_ARRAY_VALUES(gis->data_stack) \
  [DYNAMIC_ARRAY_LENGTH(gis->data_stack) - 1 - (i)]

void prepare_function_call(unsigned long nargs, struct object *old_f, struct object *old_i, struct object *new_f) {
  unsigned long argi;
  struct object *args;

  symbol_set_value(gis->impl_f_sym, new_f);

  /* transfer arguments from data stack to call stack */
  if (FUNCTION_ACCEPTS_ALL(new_f)) {
    args = NIL;
    for (argi = 0; argi < nargs; ++argi) {
      args = cons(STACK_I(argi + 1), args);
    }
    dynamic_array_push(gis->call_stack, args);
  } else {
    if (nargs != FUNCTION_NARGS(new_f)) {
      printf("Function was passed invalid number of arguments.\n");
      print(new_f);
      PRINT_STACK_TRACE_AND_QUIT();
    }
    for (argi = nargs; argi > 0; --argi) {
      dynamic_array_push(gis->call_stack, STACK_I(argi));
    }
  }

  /* remove all arguments and the function from the data stack at once */
  DYNAMIC_ARRAY_LENGTH(gis->data_stack) -= nargs + 1;

  /* initialize any temps with NIL -- could be improved by just adding to
   * the call stack's length */
  for (argi = 0; argi < FUNCTION_STACK_SIZE(new_f) - FUNCTION_NARGS(new_f); ++argi) {
    dynamic_array_push(gis->call_stack, NIL);
  }

  /* save the instruction index, and function */
  if (old_i == NIL) {
    printf("Not supported\n");
    PRINT_STACK_TRACE_AND_QUIT();
  }
  UFIXNUM_VALUE(old_i) += 1; /* resume at the next instruction */
  dynamic_array_push(gis->call_stack, old_i);
  dynamic_array_push(gis->call_stack, old_f);

  symbol_set_value(gis->impl_i_sym,
                   ufixnum(0)); /* start the bytecode interpreter at the first instruction */
}

struct object *call_function(struct object *f, struct object *args) {
  struct object *cursor;
  ufixnum_t i, nargs;

  if (FUNCTION_ACCEPTS_ALL(f)) {
    nargs = 1;
    dynamic_array_push(gis->call_stack, args);
  } else {
    nargs = 0;
    cursor = args;
    while (cursor != NIL) {
      dynamic_array_push(gis->call_stack, CONS_CAR(cursor));
      cursor = CONS_CDR(cursor);
      ++nargs;
    }
    if (nargs != FUNCTION_NARGS(f)) {
      printf("Function was given %d, but takes %d.\n", (int)nargs, (int)FUNCTION_NARGS(f));
      print(f);
      PRINT_STACK_TRACE_AND_QUIT();
    }
  }

  /* initialize any temps with NIL -- could be improved by just adding to
   * the call stack's length */
  for (i = 0; i < FUNCTION_STACK_SIZE(f) - FUNCTION_NARGS(f); ++i) {
    dynamic_array_push(gis->call_stack, NIL);
  }

  /* save the instruction index, and function */
  /* if there is anything to return to --
     if this is a macro that is being called from the top level, there would be nothing to return to. */
  if (symbol_get_value(gis->impl_f_sym) != NIL) {
    /* resume at the next instruction */
    if (symbol_get_value(gis->impl_i_sym) != NIL) { /* impl_i_sym's value can be null if a builtin was just called */
      UFIXNUM_VALUE(symbol_get_value(gis->impl_i_sym)) += 1;
      dynamic_array_push(gis->call_stack, symbol_get_value(gis->impl_i_sym));
    } else {
      dynamic_array_push(gis->call_stack, NIL);
    }
    dynamic_array_push(gis->call_stack, symbol_get_value(gis->impl_f_sym));
  } else {
    /* there is no where to return to, but because we put the function/instruction-index at the top
       of the stack, we need to fill it with something (because GET_LOCAL(...) depends on that) */
    dynamic_array_push(gis->call_stack, NIL);
    dynamic_array_push(gis->call_stack, NIL);
  }

  /* start the bytecode interpreter at the first instruction */
  symbol_set_value(gis->impl_i_sym, ufixnum(0));
  symbol_set_value(gis->impl_f_sym, f);
  run(gis);
  return pop(); /* discard the result so it doesn't leak into the data-stack */
}

/* returns the top of the stack for convience */
  /* evaluates gis->function starting at instruction gis->i */
  /* assumes gis->i and gis->f is set and call_stack has been initialized with
   * space for all stack args */
  struct object *run(struct gis * gis) {
    unsigned long byte_count,
        op_arg_byte_count; /* number of  bytes in this bytecode */
    unsigned char t0, op;  /* temporary for reading bytecode arguments */
    unsigned long a0, a1;  /* the arguments for bytecode parameters */
    unsigned long a2;
    unsigned long sa0;               /* argument for jumps */
    struct object *v0, *v1; /* temps for values popped off the stack */
    struct object *c0; /* temps for constants (used for bytecode arguments) */
    struct object *temp_i, *temp_f, *f;
    struct object *cursor;
    struct dynamic_byte_array
        *code; /* the byte array containing the bytes in the bytecode */
    struct dynamic_array *constants; /* the constants array */
    unsigned long constants_length;  /* the length of the constants array */
    struct object *i;
    ufixnum_t ufix0;
    void *arg_values[MAX_FFI_NARGS]; /* for calling foreign functions */
    ffi_sarg sresult;                /* signed result */
    void *ptr_result;
    /* jump to this label after setting a new gis->i and gis->f
       (and pushing the old i and bc to the call-stack) */

  eval_restart:
    f = symbol_get_value(gis->impl_f_sym);
    code = FUNCTION_CODE(f)->w1.value.dynamic_byte_array;
    constants = FUNCTION_CONSTANTS(f)->w1.value.dynamic_array;
    constants_length = constants->length;
    byte_count = code->length;
    i = symbol_get_value(gis->impl_i_sym);

    while (UFIXNUM_VALUE(i) < byte_count) {
      op = code->bytes[UFIXNUM_VALUE(i)];
      switch (op) {
        case op_drop: /* drop ( x -- ) */
          SC("drop", 1);
          pop();
          break;
        case op_dup: /* dup ( x -- x x ) */
          SC("dup", 1);
          dup();
          break;
        case op_cons: /* cons ( car cdr -- (cons car cdr) ) */
          SC("cons", 2);
          v1 = pop(); /* cdr */
          v0 = STACK_I(0); /* car */
          STACK_I(0) = cons(v0, v1);
          break;
        case op_intern: /* intern ( string -- symbol ) */
          SC("intern", 1);
          v0 = STACK_I(0);
          printf("op_intern is not implemented.");
          PRINT_STACK_TRACE_AND_QUIT();
          OT("intern", 0, v0, type_string);
          STACK_I(0) = intern(pop(), NIL);
          break;
        case op_car: /* car ( (cons car cdr) -- car ) */
          SC("car", 1);
          v0 = STACK_I(0);
          if (v0 == NIL) {
            STACK_I(0) = NIL;
          } else if (type_of(v0) == gis->cons_type) {
            STACK_I(0) = CONS_CAR(v0);
          } else {
            printf("Can only car a list (was given a %s).\n",
                   type_name_of_cstr(v0));
            PRINT_STACK_TRACE_AND_QUIT();
          }
          break;
        case op_cdr: /* cdr ( (cons car cdr) -- cdr ) */
          SC("cdr", 1);
          v0 = STACK_I(0);
          if (v0 == NIL) {
            STACK_I(0) = NIL;
          } else if (type_of(v0) == gis->cons_type) {
            STACK_I(0) = CONS_CDR(v0);
          } else {
            printf("Can only cdr a list (was given a %s).\n",
                   type_name_of_cstr(v0));
            PRINT_STACK_TRACE_AND_QUIT();
          }
          break;
        case op_gt: /* gt ( x y -- x>y ) */
          SC("gt", 2);
          v1 = pop(); /* y */
          STACK_I(0) = FIXNUM_VALUE(STACK_I(0)) > FIXNUM_VALUE(v1)
                           ? T
                           : NIL; /* TODO: support flonum/ufixnum */
          break;
        case op_lt: /* lt ( x y -- x<y ) */
          SC("lt", 2);
          v1 = pop(); /* y */
          STACK_I(0) = FIXNUM_VALUE(STACK_I(0)) < FIXNUM_VALUE(v1)
                           ? T
                           : NIL; /* TODO: support flonum/ufixnum */
          break;
        case op_lti: /* lti <n> ( x -- x<n ) */
          SC("lti", 1);
          READ_OP_ARG();
          STACK_I(0) = FIXNUM_VALUE(STACK_I(0)) < a0
                           ? T
                           : NIL; /* TODO: support flonum/ufixnum */
          break;
        case op_gte: /* gte ( x y -- x>=y ) */
          SC("gte", 2);
          v1 = pop(); /* y */
          STACK_I(0) = FIXNUM_VALUE(STACK_I(0)) >= FIXNUM_VALUE(v1)
                           ? T
                           : NIL; /* TODO: support flonum/ufixnum */
          break;
        case op_lte: /* gte ( x y -- x<=y ) */
          SC("lte", 2);
          v1 = pop(); /* y */
          STACK_I(0) = FIXNUM_VALUE(STACK_I(0)) <= FIXNUM_VALUE(v1)
                           ? T
                           : NIL; /* TODO: support flonum/ufixnum */
          break;
        case op_bin_or:
          SC("bin_or", 2);
          v1 = pop(); /* y */
          STACK_I(0) =
              fixnum(FIXNUM_VALUE(STACK_I(0)) | FIXNUM_VALUE(v1)); /* TODO: support flonum/ufixnum */
          break;
        case op_bin_and:
          SC("bin_and", 2);
          v1 = pop(); /* y */
          STACK_I(0) =
              fixnum(FIXNUM_VALUE(STACK_I(0)) & FIXNUM_VALUE(v1)); /* TODO: support flonum/ufixnum */
          break;
        case op_shift_right:
          SC("shift-right", 2);
          v1 = pop(); /* y */
          STACK_I(0) =
              fixnum(FIXNUM_VALUE(STACK_I(0)) >> FIXNUM_VALUE(v1)); /* TODO: support flonum/ufixnum */
          break;
        case op_shift_left:
          SC("shift-left", 2);
          v1 = pop(); /* y */
          STACK_I(0) =
              fixnum(FIXNUM_VALUE(STACK_I(0)) << FIXNUM_VALUE(v1)); /* TODO: support flonum/ufixnum */
          break;
        case op_add: /* add ( x y -- x+y ) */
          SC("add", 2);
          v1 = pop(); /* y */
          STACK_I(0) =
              fixnum(FIXNUM_VALUE(STACK_I(0)) +
                     FIXNUM_VALUE(v1)); /* TODO: support flonum/ufixnum */
          break;
        case op_addi: /* add-i <n> ( x -- x+n ) */
          READ_OP_ARG();
          STACK_I(0) = fixnum(FIXNUM_VALUE(STACK_I(0)) + a0);
          break;
        case op_subi: /* add-i <n> ( x -- ) */
          READ_OP_ARG();
          STACK_I(0) = fixnum(FIXNUM_VALUE(STACK_I(0)) - a0);
          break;
        case op_sub: /* sub ( x y -- x-y ) */
          SC("sub", 2);
          v1 = pop(); /* y */
          STACK_I(0) =
              fixnum(FIXNUM_VALUE(STACK_I(0)) -
                     FIXNUM_VALUE(v1)); /* TODO: support flonum/ufixnum */
          break;
        case op_mul: /* mul ( x y -- x*y ) */
          SC("mul", 2);
          v1 = pop(); /* y */
          STACK_I(0) =
              fixnum(FIXNUM_VALUE(STACK_I(0)) *
                     FIXNUM_VALUE(v1)); /* TODO: support flonum/ufixnum */
          break;
        case op_div: /* div ( x y -- x/y ) */
          SC("div", 2);
          v1 = pop(); /* y */
          STACK_I(0) =
              fixnum(FIXNUM_VALUE(STACK_I(0)) /
                     FIXNUM_VALUE(v1)); /* TODO: support flonum/ufixnum */
          break;
        case op_eq: /* eq ( x y -- z ) */
          SC("eq", 2);
          v1 = pop(); /* y */
          /* TODO: add t */
          STACK_I(0) = equals(STACK_I(0), v1) ? gis->type_t_sym : NIL;
          break;
        case op_and: /* and ( x y -- z ) */
          SC("and", 2);
          v1 = pop(); /* y */
          if (STACK_I(0) != NIL && v1 != NIL)
            STACK_I(0) = v1;
          else
            STACK_I(0) = NIL;
          break;
        case op_list: /* list <n> ( ...n... -- ) */
          READ_OP_ARG();
          if (a0 == 0)
            push(NIL);
          else if (a0 == 1)
            STACK_I(0) = cons(STACK_I(0), NIL);
          else {
            STACK_I(0) = cons(STACK_I(0), NIL);
            for (a1 = 1; a1 < a0; ++a1)
              STACK_I(0) = cons(STACK_I(a1), STACK_I(0));
            /* set the earliest stack item to be the result, because so we can
             * simply update the data stack's count to pop the args */
            STACK_I(a0 - 1) = STACK_I(0);
            gis->data_stack->w1.value.dynamic_array->length -= a0 - 1;
          }
          break;
        case op_or: /* or ( x y -- z ) */
          SC("or", 2);
          v1 = pop(); /* y */
          if (STACK_I(0) != NIL && v1 != NIL) {
            STACK_I(0) = v1;
          } else if (STACK_I(0) != NIL) {
            /* do nothing - v0 is already on the stack */
          } else if (v1 != NIL) {
            STACK_I(0) = v1;
          } else {
            STACK_I(0) = NIL;
          }
          break;
        case op_load_nil:
          push(NIL);
          break;
        case op_const_0:
#ifdef RUN_TIME_CHECKS
        if (constants->length < 1) {
          printf("Out of bounds of constants vector.");
          PRINT_STACK_TRACE_AND_QUIT();
        }
#endif
        push(constants->values[0]);
        break;
      case op_const_1:
#ifdef RUN_TIME_CHECKS
        if (constants->length < 2) {
          printf("Out of bounds of constants vector.");
          PRINT_STACK_TRACE_AND_QUIT();
        }
#endif
        push(constants->values[1]);
        break;
      case op_const_2:
#ifdef RUN_TIME_CHECKS
        if (constants->length < 3) {
          printf("Out of bounds of constants vector.");
          PRINT_STACK_TRACE_AND_QUIT();
        }
#endif
        push(constants->values[2]);
        break;
      case op_const_3:
#ifdef RUN_TIME_CHECKS
        if (constants->length < 4) {
          printf("Out of bounds of constants vector.");
          PRINT_STACK_TRACE_AND_QUIT();
        }
#endif
        push(constants->values[3]);
        break;
      case op_const: /* const ( -- x ) */
        READ_CONST_ARG();
        push(c0);
        break;
      case op_push_arg: /* push-arg ( x -- ) */
        dynamic_array_push(gis->call_stack, pop());
        break;
      case op_push_args: /* push-args <n> ( x_0...x_n -- ) */
        READ_OP_ARG();
        for (a1 = 0; a1 < a0; ++a1) dynamic_array_push(gis->call_stack, pop());
        break;
      case op_load_from_stack: /* load-from-stack ( -- ) */
        READ_OP_ARG();
        /* minus two to skip the f and i */
        push(GET_LOCAL(a0));
        break;
      /* two hard coded index versions for the most common cases (functions that
       * take one or two arguments) */
      case op_load_from_stack_0:
        /* minus two to skip the f and i */
        push(GET_LOCAL(0));
        break;
      case op_load_from_stack_1:
        /* minus two to skip the f and i */
        push(GET_LOCAL(1));
        break;
      case op_store_to_stack: /* store-to-stack ( -- ) */
        SC("store-to-stack", 1);
        READ_OP_ARG();
        SET_LOCAL(a0, pop());
        break;
      /* two hard coded index versions for the most common cases (functions that
       * take one or two arguments) */
      case op_store_to_stack_0:
        SET_LOCAL(0, pop());
        break;
      case op_store_to_stack_1:
        SET_LOCAL(1, pop());
        break;
      /* this could be specified as  */
      /* TODO; this doesn't need to be its own op, we can tell by the argumen
       * type */
      case op_call_symbol_function: /* an optimization for calling a function
                                       given a symbol */
      case op_call_function: /* call-function <n> ( arg_0...arg_n fun -- ) */
        READ_OP_ARG();
        SC("call_function", a0);
        temp_i = i;
        temp_f = f;
        /* set the new function */
        if (op == op_call_symbol_function) {
          f = symbol_get_function(STACK_I(0));
        } else {
          f = STACK_I(0);
        }

        /* experimental -- what if any given symbol we just attempt to get the function? */
        if (type_of(f) == gis->symbol_type) {
          f = symbol_get_function(STACK_I(0));
        }

        /* if this is a foreign function */
        if (type_of(f) == gis->foreign_function_type) {
          cursor = FFUN_PARAM_TYPES(f);
          if (a0 > FFUN_NARGS(f)) {
            printf("Insufficient arguments were passed to foreign function.");
            PRINT_STACK_TRACE_AND_QUIT();
          }

          a1 = a0; /* for indexing arguments */
          a2 = 0;  /* for indexing arg_values */
          while (cursor != NIL) {
            if (is_type_designator_a_pointer(CONS_CAR(cursor)) != NULL) { /* IMPORTANT TODO: validate that it is the same type from the ffun signature */
              arg_values[a2] = &OBJECT_POINTER(STACK_I(a1));
            } else if (STACK_I(a1) == NIL) {
              arg_values[a2] = &ffi_null;
            } else if (type_of(STACK_I(a1)) == gis->fixnum_type) {
              arg_values[a2] = &FIXNUM_VALUE(STACK_I(a1));
            } else if (type_of(STACK_I(a1)) == gis->string_type) {
              dynamic_byte_array_force_cstr(STACK_I(a1));
              arg_values[a2] = &DYNAMIC_BYTE_ARRAY_BYTES(STACK_I(a1));
            } else if (type_of(STACK_I(a1)) == gis->pointer_type) {
              arg_values[a2] = &OBJECT_POINTER(STACK_I(a1));
            } else if (!TYPE_BUILTIN(type_of(STACK_I(a1)))) { /* IMPORTANT TODO: validate that it is the same type from the ffun signature */
              arg_values[a2] = OBJECT_POINTER(STACK_I(a1));
            } else {
              printf("Argument not supported for foreign functions.");
              print(STACK_I(a1));
            }
            --a1;
            ++a2;
            cursor = CONS_CDR(cursor);
          }

          /* remove all arguments and the function from the data stack at once
           */
          DYNAMIC_ARRAY_LENGTH(gis->data_stack) -= a0 + 1;

          if (FFUN_RET_TYPE(f) == gis->pointer_type) {
            ffi_call(FFUN_CIF(f), FFI_FN(FFUN_PTR(f)), &ptr_result, arg_values);
            push(pointer(ptr_result));
          } else if (FFUN_RET_TYPE(f) == gis->string_type) {
            ffi_call(FFUN_CIF(f), FFI_FN(FFUN_PTR(f)), &ptr_result, arg_values);
            push(string(ptr_result));
          } else {
            ffi_call(FFUN_CIF(f), FFI_FN(FFUN_PTR(f)), &sresult, arg_values);
            push(fixnum(sresult));
          }

          UFIXNUM_VALUE(i) += 1; /* advance to the next instruction */
          goto eval_restart;     /* restart the evaluation loop */
        } else if (type_of(f) != gis->function_type) {
          printf("Attempted to call a non-function object.\n");
          print(type_of(f));
          PRINT_STACK_TRACE_AND_QUIT();
        }

#if FUNCTION_TRACE
        if (gis->loaded_core) {
          printf("=== CALL FUNCTION ===\n");
          print(f);
        }
#endif

        prepare_function_call(a0, temp_f, temp_i, f);

#if FUNCTION_TRACE
        if (gis->loaded_core) {
          print(gis->call_stack);
          printf("=====================\n");
        }
#endif

        if (FUNCTION_IS_BUILTIN(f)) {
          symbol_set_value(
              gis->impl_i_sym,
              NIL); /* a NIL instruction index -- for consistency gis->i of NIL
                       should be set for all builtins */
          eval_builtin(f);
          goto return_function_label; /* return */
        } else {
          goto eval_restart;            /* restart the evaluation loop */
        }
      case op_return_function: /* return-function ( x -- ) */
      return_function_label:
#ifdef RUN_TIME_CHECKS
        if (DYNAMIC_ARRAY_LENGTH(gis->call_stack) == FUNCTION_STACK_SIZE(f)) {
          printf("Attempted to return from top-level.");
          PRINT_STACK_TRACE_AND_QUIT();
        }
#endif

          ufix0 = FUNCTION_STACK_SIZE(f) + 2;
        /* this indicates that we returned to the top level -- used when calling functions during compile time (starts with macros) */
        if (DYNAMIC_ARRAY_VALUES(gis->call_stack)[DYNAMIC_ARRAY_LENGTH(gis->call_stack) - 1] == NIL) {
          DYNAMIC_ARRAY_LENGTH(gis->call_stack) -= ufix0;
          symbol_set_value(gis->impl_f_sym, NIL);
          symbol_set_value(gis->impl_i_sym, ufixnum(0)); /* TODO: reuse fixnums so one is not created between call_function calls */
          goto quit_run;
        } else {

#if FUNCTION_TRACE
          if (gis->loaded_core) {
            printf("=== RETURN FUNCTION ===\n");
            printf("FROM: ");
            print(f);
          }
#endif

          symbol_set_value(
              gis->impl_f_sym,
              DYNAMIC_ARRAY_VALUES(
                  gis->call_stack)[DYNAMIC_ARRAY_LENGTH(gis->call_stack) - 1]);

          /* if we are returning from a builtin (a builtin called some other function -- this
             happens with APPLY) return twice. */
            symbol_set_value(
                gis->impl_i_sym,
                DYNAMIC_ARRAY_VALUES(
                    gis->call_stack)[DYNAMIC_ARRAY_LENGTH(gis->call_stack) -
                                     2]);
          /* pop off all stack arguments, then pop bc, then pop instruction
           * index */
          DYNAMIC_ARRAY_LENGTH(gis->call_stack) -= ufix0;

#if FUNCTION_TRACE
          if (gis->loaded_core) {
            printf("TO: ");
            print(symbol_get_value(gis->impl_f_sym));
            print(gis->call_stack);
            printf("====================\n");
          }
#endif

          goto eval_restart; /* restart the evaluation loop */
        }
      case op_jump_forward: /* jump-forward ( -- ) */
        READ_OP_JUMP_ARG();
        UFIXNUM_VALUE(i) += sa0;
        continue; /* continue so the usual increment to i doesn't happen */
      case op_jump_backward: /* jump-forward ( -- ) */
        READ_OP_JUMP_ARG();
        UFIXNUM_VALUE(i) -= sa0;
        continue; /* continue so the usual increment to i doesn't happen */
      case op_jump_forward_when_nil: /* jump_when_nil ( cond -- ) */
        READ_OP_JUMP_ARG();
        v0 = pop(); /* cond */
        if (v0 == NIL)
          UFIXNUM_VALUE(i) += sa0;
        else
          ++UFIXNUM_VALUE(i);
        continue;    /* continue so the usual increment to i doesn't happen */
      case op_print: /* print ( x -- NIL ) */
        SC("print", 1);
        print_no_newline(STACK_I(0));
        pop();
        break;
      case op_print_nl: /* print-nl ( -- ) */
        printf("\n");
        break;
      case op_symbol_value: /* symbol-value ( sym -- ) */
        SC("symbol-value", 1);
        STACK_I(0) = symbol_get_value(STACK_I(0));
        break;
      case op_symbol_function: /* symbol-function ( sym -- ) */
        SC("symbol-function", 1);
        STACK_I(0) = symbol_get_function(STACK_I(0));
        break;
      case op_set_symbol_value: /* set-symbol-value ( sym val -- ) */
        SC("set-symbol-value", 1);
        v1 = pop(); /* val */
        OT("set-symbol-value", 0, STACK_I(0), type_symbol);
        symbol_set_value(STACK_I(0), v1);
        break;
      case op_set_symbol_function: /* set-symbol-function ( sym val -- ) */
        SC("set-symbol-function", 1);
        /* TODO: this only needs 1 pop */
        v1 = pop(); /* val */
        v0 = pop(); /* sym */
        symbol_set_function(v0, v1);
        push(v1);
        break;
      case op_symbol_type:
        SC("symbol-type", 1);
        STACK_I(0) = symbol_get_type(STACK_I(0));
        break;
      case op_type_of:
        SC("type-of", 1);
        STACK_I(0) = type_of(STACK_I(0));
        break;
      case op_dynamic_array:
        SC("dynamic-array", 1);
        OT("dynamic-array", 0, STACK_I(0), type_fixnum);
        STACK_I(0) = dynamic_array(FIXNUM_VALUE(STACK_I(0)));
        break;
      case op_dynamic_byte_array:
        SC("dynamic-byte-array", 1);
        OT("dynamic-byte-array", 0, STACK_I(0), type_fixnum);
        STACK_I(0) = dynamic_byte_array(FIXNUM_VALUE(STACK_I(0)));
        break;
      case op_to_string:
        SC("to-string", 1);
        STACK_I(0) = to_string(STACK_I(0));
        break;
      case op_dynamic_array_get:
        SC("dynamic-array-get", 2);
        STACK_I(0) = dynamic_array_get(STACK_I(1), pop());
        break;
      case op_dynamic_array_set:
        SC("dynamic-array-set", 3);
        dynamic_array_set(STACK_I(2), STACK_I(1), pop());
        pop();
        STACK_I(0) = NIL;
        break;
      case op_dynamic_array_length:
        SC("dynamic-array-length", 1);
        STACK_I(0) = dynamic_array_length(STACK_I(0));
        break;
      case op_dynamic_array_push:
        SC("dynamic-array-push", 2);
        dynamic_array_push(STACK_I(1), pop());
        STACK_I(0) = NIL;
        break;
      case op_dynamic_array_pop:
        SC("dynamic-array-pop", 1);
        STACK_I(0) = dynamic_array_pop(STACK_I(0));
        break;
      case op_dynamic_array_concat:
        SC("dynamic-array-concat", 2);
        STACK_I(0) = dynamic_array_concat(STACK_I(1), pop());
        break;
      case op_dynamic_byte_array_concat:
        SC("dynamic-byte-array-concat", 2);
        STACK_I(0) = dynamic_byte_array_concat(STACK_I(1), pop());
        break;
      case op_dynamic_byte_array_get:
        SC("dynamic-byte-array-get", 2);
        OT("dynamic-byte-array-get", 1, STACK_I(0), type_fixnum);
        STACK_I(0) = fixnum(dynamic_byte_array_get(STACK_I(1), FIXNUM_VALUE(pop())));
        break;
      case op_dynamic_byte_array_insert:
        SC("dynamic-byte-array-insert", 3);
        OT("dynamic-byte-array-insert", 1, STACK_I(1), type_fixnum);
        OT("dynamic-byte-array-insert", 2, STACK_I(0), type_fixnum);
        dynamic_byte_array_insert_char(STACK_I(2), FIXNUM_VALUE(STACK_I(1)), FIXNUM_VALUE(pop()));
        pop();
        STACK_I(0) = NIL;
        break;
      case op_dynamic_byte_array_length:
        SC("dynamic-byte-array-length", 1);
        STACK_I(0) = dynamic_byte_array_length(STACK_I(0));
        break;
      case op_dynamic_byte_array_set:
        SC("dynamic-byte-array-set", 3);
        OT("dynamic-byte-array-set", 1, STACK_I(1), type_fixnum);
        OT("dynamic-byte-array-set", 2, STACK_I(0), type_fixnum);
        dynamic_byte_array_set(STACK_I(2), FIXNUM_VALUE(STACK_I(1)), FIXNUM_VALUE(pop()));
        pop();
        STACK_I(0) = NIL;
        break;
      case op_dynamic_byte_array_push:
        SC("dynamic-array-push", 2);
        dynamic_byte_array_push(STACK_I(1), pop());
        STACK_I(0) = NIL;
        break;
      case op_dynamic_byte_array_pop:
        SC("dynamic-byte-array-pop", 1);
        STACK_I(0) = dynamic_byte_array_pop(STACK_I(0));
        break;
      case op_alloc_struct:
        SC("alloc-struct", 1);
        STACK_I(0) = alloc_struct(STACK_I(0), 1);
        break;
      case op_gensym:
        push(symbol(string_concat(string("gensym-"), to_string(ufixnum(gis->gensym_counter)))));
        ++gis->gensym_counter;
        break;
      case op_function_code:
        SC("function-code", 1);
        OT("function-code", 0, STACK_I(0), type_function);
        push(FUNCTION_CODE(GET_LOCAL(0)));
        break;
      case op_struct_field:
        SC("struct-field", 2);
        STACK_I(0) = struct_field(STACK_I(1), pop());
        break;
      case op_set_struct_field:
        SC("set-struct-field", 3);
        set_struct_field(STACK_I(2), STACK_I(1), pop());
        pop();
        STACK_I(0) = NIL;
        break;
      case op_symbol_name:
        SC("symbol-name", 1);
        OT("symbol-name", 0, STACK_I(0), type_symbol);
        STACK_I(0) = SYMBOL_NAME(STACK_I(0));
        break;
      case op_byte_stream:
        SC("byte-stream", 1);
        STACK_I(0) = byte_stream_lift(STACK_I(0));
        break;
      case op_byte_stream_peek:
        SC("byte-stream-peek", 2);
        v1 = pop();
        if (v1 == NIL)
          STACK_I(0) = byte_stream_peek(STACK_I(0), 1);
        else
          STACK_I(0) = byte_stream_peek(STACK_I(0), FIXNUM_VALUE(v1));
        break;
      case op_byte_stream_read:
        SC("byte-stream-read", 2);
        v1 = pop();
        if (v1 == NIL)
          STACK_I(0) = byte_stream_read(STACK_I(0), 1);
        else
          STACK_I(0) = byte_stream_read(STACK_I(0), FIXNUM_VALUE(v1));
        break;
      case op_write_file:
        SC("write-file", 2);
        write_file(STACK_I(1), pop());
        STACK_I(0) = NIL;
        break;
      case op_string_concat:
        SC("string-concat", 2);
        STACK_I(0) = string_concat_external(STACK_I(1), pop());
        break;
      default:
        printf("Invalid op.\n");
        PRINT_STACK_TRACE_AND_QUIT();
        break;
    }
    ++UFIXNUM_VALUE(i);
  }
  quit_run:
  return DYNAMIC_ARRAY_LENGTH(gis->data_stack) ? peek() : NIL;
}

int main(int argc, char **argv) {
  gis_init(1);
  return 0;
}