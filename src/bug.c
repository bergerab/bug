/** @file interpreter.c
 *
 * The Bug Bytecode interpreter
 */

#include "bug.h"

/**
 * The global interpreter state
 * 
 * Initialized using init()
 */
struct gis *gis;

void *ffi_null = NULL;

struct object *compile(struct object *ast, struct object *bc, struct object *st, struct object *fst);
struct object *compile_entire_file(struct object *input_file);
struct object *eval(struct object *bc, struct object* args);
struct object *read(struct object *s, struct object *package);
struct object *type(struct object *name, char can_instantiate);

char equals(struct object *o0, struct object *o1);

void dynamic_byte_array_force_cstr(struct object *dba);
void dynamic_array_push(struct object *da, struct object *value);
struct object *dynamic_array_get_ufixnum_t(struct object *da, ufixnum_t index);

void symbol_set_structure(struct object *sym, struct object *s);

enum object_type object_type_of(struct object *o);
struct object *type_name_of(struct object *o);
char *type_name_of_cstr(struct object *o);
char *type_name_cstr(struct object *o);
struct object *type_of(struct object *o);

struct object *symbol_get_value(struct object *sym);

struct object *get_string_designator(struct object *sd);
void print(struct object *o);

void print_stack();

/*===============================*
 *===============================*
 * Utility Procedures            *
 *===============================*
 *===============================*/
double log2(double n) { return log(n) / log(2); }

/*===============================*
 *===============================*
 * Types                         *
 *===============================*
 *===============================*/
#ifdef RUN_TIME_CHECKS
void type_check(char *name, unsigned int argument, struct object *o,
                struct object *t) {
  struct object *t1;
  t1 = type_of(o);
  if (t1 != t) {
    printf(
        "BC: Function \"%s\" was called with an invalid argument "
        "at index %d. Expected type %s, but was %s.\n",
        name, argument, type_name_cstr(t), type_name_cstr(t1));
    PRINT_STACK_TRACE_AND_QUIT();
  }
}

void type_check_or2(char *name, unsigned int argument, struct object *o,
                struct object *t0, struct object *t1) {
  if (type_of(o) != t0 && type_of(o) != t1) {
    printf(
        "BC: Function \"%s\" was called with an invalid argument "
        "at index %d. Expected either %s or %s, but was %s.\n",
        name, argument, type_name_cstr(t0), type_name_cstr(t1), type_name_of_cstr(o));
    PRINT_STACK_TRACE_AND_QUIT();
  }
}

/**
 * Checks the number of items on the stack (for bytecode interpreter)
 */
void stack_check(char *name, int n, unsigned long i) {
  unsigned int stack_count = DYNAMIC_ARRAY_LENGTH(gis->data_stack);
  if (stack_count < n) {
    printf(
        "BC: Operation \"%s\" was called when the stack had too "
        "few items. Expected %d items on the stack, but were %d (at instruction %lu).\n",
        name, n, stack_count, i);
    PRINT_STACK_TRACE_AND_QUIT();
  }
}

void object_type_check_list(char *name, unsigned int argument, struct object *o) {
  enum object_type t1;
  t1 = object_type_of(o);
  if (t1 != type_cons && o != NIL) {
    printf(
        "BC: Function \"%s\" was called with an invalid argument "
        "at index %d. Expected a list, but was %d.\n",
        name, argument, (int)t1);
    PRINT_STACK_TRACE_AND_QUIT();
  }
}

void object_type_check(char *name, unsigned int argument, struct object *o,
                enum object_type t) {
  enum object_type t1;
  t1 = object_type_of(o);
  if (t1 != t) {
    printf(
        "BC: Function \"%s\" was called with an invalid argument "
        "at index %d. Expected type %d, but was %d.\n",
        name, argument, (int)t, (int)t1);
    PRINT_STACK_TRACE_AND_QUIT();
  }
}

void object_type_check_or2(char *name, unsigned int argument, struct object *o,
                enum object_type t0, enum object_type t1) {
  if (object_type_of(o) != t0 && object_type_of(o) != t1) {
    printf(
        "BC: Function \"%s\" was called with an invalid argument "
        "at index %d. Expected either %d or %d, but was %d.\n",
        name, argument, (int)t0, (int)t1, (int)object_type_of(o));
    PRINT_STACK_TRACE_AND_QUIT();
  }
}
#endif

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
  if (o == NIL) return gis->nil_type;
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
    printf("Failed to load dynamic library %s.", STRING_CONTENTS(o));
    PRINT_STACK_TRACE_AND_QUIT();
  }
  return o;
}

/* can_instantiate indicates if this type supports having values with this type.
   e.g.: you can make value of a type string, type fixnum, etc... but having a value of type
   uint8 is not supported. Same with something like "void". Those values are just used for FFI. */
struct object *type(struct object *name, char can_instantiate) {
  struct object *o;
  OT2("type", 0, name, type_dynamic_byte_array, type_string);
  o = object(type_type);
  NC(o, "Failed to allocate type object.");
  o->w1.value.type = malloc(sizeof(struct type));
  NC(o->w1.value.type, "Failed to allocate type.");
  TYPE_NAME(o) = name;
  TYPE_STRUCT(o) = NIL;
  if (can_instantiate) {
    TYPE_ID(o) = DYNAMIC_ARRAY_LENGTH(gis->types);
    dynamic_array_push(gis->types, o);
  } else {
    TYPE_ID(o) = -1;
  }
  return o;
}

char ffi_type_designator_is_string(struct object *o) {
  return type_of(o) == gis->cons_type &&
         CONS_CAR(o) == gis->pointer_type &&
         CONS_CDR(o) != NIL &&
         CONS_CAR(CONS_CDR(o)) == gis->char_type;
}

char ffi_type_designator_is_struct(struct object *o) {
  return type_of(o) == gis->cons_type &&
         CONS_CAR(o) == gis->struct_type;
}

ffi_type *ffi_type_designator_to_ffi_type(struct object *o);

/* TODO: require structs to be separated into definitions/not inlined */
ffi_type *struct_ffi_type_designator_to_ffi_type(struct object *o) {
  ffi_type *ft;
  unsigned int i;
  struct object *cursor;

  /* make a new ffi_type object */
  ft = malloc(sizeof(ffi_type)); /* TODO GC clean this up */
  ft->size = ft->alignment = 0;
  ft->type = FFI_TYPE_STRUCT;
  ft->elements = malloc(sizeof(ffi_type *) * (count(CONS_CDR(o)) + 1));
      /* TODO: GC */ /* plus one for null termination */

  i = 0;
  cursor = CONS_CDR(o);
  while (cursor != NIL) {
    ft->elements[i] = ffi_type_designator_to_ffi_type(CONS_CAR(cursor));
    ++i;
    cursor = CONS_CDR(cursor);
  }
  ft->elements[i] = NULL; /* must be null terminated */
  return ft;
}

ffi_type *ffi_type_designator_to_ffi_type(struct object *o) {
  struct object *lhs, *t; /* , *rhs; */

  t = type_of(o);
  if (t == gis->cons_type) { /* must be of form (* <any>) */
    lhs = CONS_CAR(o);
    if (lhs != gis->pointer_type && lhs != gis->struct_type) {
      printf("First item in FFI designator cons list must be ffi:* or ffi:struct.");
      PRINT_STACK_TRACE_AND_QUIT();
    }
    if (CONS_CDR(o) == NIL) {
      printf("Takes at least argument.");
      PRINT_STACK_TRACE_AND_QUIT();
    }
    if (ffi_type_designator_is_string(o)) {
      return &ffi_type_pointer;
    }
    if (ffi_type_designator_is_struct(o)) {
      return struct_ffi_type_designator_to_ffi_type(o);
    }
    printf("not impl\n");
    PRINT_STACK_TRACE_AND_QUIT();
  } else if (t == gis->symbol_type) { /* must be a ffi symbol */
    if (o == gis->char_type) return &ffi_type_schar;
    else if (o == gis->int_type) {
      return &ffi_type_sint;
    }
    else if (o == gis->uint_type) return &ffi_type_uint;
    else if (o == gis->uint8_type) {
      return &ffi_type_uint8;
    }
    else if (o == gis->void_type) return &ffi_type_void;
    else if (o == gis->pointer_type) {
      return &ffi_type_pointer;
    } else if (o == gis->struct_type) {
      printf("Passing struct directly is not supported");
      print(o);
      PRINT_STACK_TRACE_AND_QUIT();
    } else if (SYMBOL_STRUCTURE_IS_SET(o)) {
      return STRUCTURE_FFI_TYPE(SYMBOL_STRUCTURE(o));
    } else {
      printf("Invalid FFI type designator symbol: \n");
      print(o);
      PRINT_STACK_TRACE_AND_QUIT();
    }
  } else {
    printf("Invalid FFI type designator: \n");
    print(o);
    PRINT_STACK_TRACE_AND_QUIT();
  }
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

struct object *ffun(struct object *dlib, struct object *ffname, struct object* ret_type, struct object *params) {
  ffi_status status;
  struct object *o;

  OT("ffun", 0, dlib, type_dlib);
  OT2("ffun", 1, ffname, type_dynamic_byte_array, type_string);
  OT2("ffun", 2, ret_type, type_dynamic_byte_array, type_string);
  OT_LIST("ffun", 3, params);

  o = object(type_ffun);
  NC(o, "Failed to allocate ffun object.");
  o->w1.value.ffun = malloc(sizeof(struct ffun));
  FFUN_FFNAME(o) = get_string_designator(ffname);
  FFUN_DLIB(o) = dlib;
  dynamic_byte_array_force_cstr(FFUN_FFNAME(o));
  FFUN_PTR(o) = GetProcAddress(DLIB_PTR(dlib), STRING_CONTENTS(FFUN_FFNAME(o)));
  if (FFUN_PTR(o) == NULL) {
    printf("Failed to load foreign function %s.", STRING_CONTENTS(FFUN_FFNAME(o)));
    PRINT_STACK_TRACE_AND_QUIT();
  }
  FFUN_PARAMS(o) = params;
  FFUN_RET(o) = ret_type;

  FFUN_ARGTYPES(o) = malloc(sizeof(ffi_type*) * MAX_FFI_NARGS);
  FFUN_NARGS(o) = 0;
  while (params != NIL) {
    FFUN_ARGTYPES(o)[FFUN_NARGS(o)++] = ffi_type_designator_to_ffi_type(CONS_CAR(params));

    if (FFUN_NARGS(o) > MAX_FFI_NARGS) {
      printf("Too many arguments for foreign function.");
      PRINT_STACK_TRACE_AND_QUIT();
    }
    params = CONS_CDR(params);
  }

  FFUN_CIF(o) = malloc(sizeof(ffi_cif));
  if ((status = ffi_prep_cif(FFUN_CIF(o), FFI_DEFAULT_ABI, FFUN_NARGS(o), ffi_type_designator_to_ffi_type(FFUN_RET(o)), FFUN_ARGTYPES(o))) != FFI_OK) {
      printf("PRINT_STACK_TRACE_AND_QUIT preparing CIF.\n");
      PRINT_STACK_TRACE_AND_QUIT();
  }

  return o;
}

struct object *structure(struct object *name, struct object *fields) {
  unsigned int i;
  struct object *cursor, *t;
  struct object *o = object(type_struct);

  OT2("structure", 0, name, type_symbol, type_string);
  OT_LIST("structure", 1, fields);

  NC(o, "Failed to allocate struct object.");
  o->w1.value.structure = malloc(sizeof(struct structure));
  if (o->w1.value.structure == NULL) {
    printf("Failed to allocate struct.");
    PRINT_STACK_TRACE_AND_QUIT();
  }
  STRUCTURE_NAME(o) = get_string_designator(name);
  STRUCTURE_FIELDS(o) = fields;

  STRUCTURE_FFI_TYPE(o) = malloc(sizeof(ffi_type));

  /* make a new ffi_type object */
  STRUCTURE_FFI_TYPE(o)->size = STRUCTURE_FFI_TYPE(o)->alignment = 0;
  STRUCTURE_FFI_TYPE(o)->type = FFI_TYPE_STRUCT;
  STRUCTURE_FFI_TYPE(o)->elements = malloc(sizeof(ffi_type *) * (count(fields) + 1)); /* TODO: make sure to free this */

  i = 0;
  cursor = fields;
  while (cursor != NIL) {
    t = CONS_CAR(CONS_CDR(CONS_CAR(cursor)));
    STRUCTURE_FFI_TYPE(o)->elements[i] = ffi_type_designator_to_ffi_type(t);
    ++i;
    cursor = CONS_CDR(cursor);
  }
  STRUCTURE_FFI_TYPE(o)->elements[i] = NULL; /* must be null terminated */

  STRUCTURE_NFIELDS(o) = i;

  STRUCTURE_OFFSETS(o) = malloc(sizeof(size_t) * i); /* TODO: GC clean up */ /* TODO: does this need a +1? I removed it not sure why it was ever there */
  ffi_get_struct_offsets(FFI_DEFAULT_ABI, STRUCTURE_FFI_TYPE(o), STRUCTURE_OFFSETS(o));

  /* make new type for this struct */
  STRUCTURE_TYPE(o) = type(get_string_designator(name), 1);
  TYPE_STRUCT(STRUCTURE_TYPE(o)) = o; /* add a reference back to this struct from the type */

  symbol_set_structure(name, o);

  return o;
}

struct object *pointer(void *ptr) {
  struct object *o = object(type_ptr);
  NC(o, "Failed to allocate pointer object.");
  OBJECT_POINTER(o) = ptr;
  return o;
}

struct object *alloc_struct(struct object *structure, char init_defaults) {
  struct object *o;
  struct object *instance;
  size_t i, nfields;
  ffi_type *t;
  uint8_t default_uint8_val;
  int default_sint_val;

  /* iterate over struct ffi types and input */
  t = STRUCTURE_FFI_TYPE(structure);
  instance = malloc(t->size); /* TODO: GC clean this up */
  if (instance == NULL) {
    printf("failed to malloc struct for ffi.");
    PRINT_STACK_TRACE_AND_QUIT();
  }

  o = pointer(instance);

  if (init_defaults) {
    /* fill struct with default values */
    i = 0;
    nfields = STRUCTURE_NFIELDS(structure);
    for (; i < nfields; ++i) {
      if (t->elements[i] == &ffi_type_sint) {
        default_sint_val = 0;
        /* arithemtic can't be done on void* (size of elements is unknown) so
         * cast to a char* (because we know a4 is # of bytes) */
        memcpy(&((char *)instance)[STRUCTURE_OFFSETS(structure)[i]], &default_sint_val, sizeof(int));
      } else if (t->elements[i] == &ffi_type_uint8) {
        default_uint8_val = 0;
        /* arithemtic can't be done on void* (size of elements is unknown) so
         * cast to a char* (because we know a4 is # of bytes) */
        memcpy(&((char *)instance)[STRUCTURE_OFFSETS(structure)[i]], &default_uint8_val, sizeof(uint8_t));
        /*((char*)instance)[offsets[a3]] = FIXNUM_VALUE(CONS_CAR(cursor2));*/
        /* why didn't this work?  */
      } else {
        printf("Unknown value when populating struct");
        PRINT_STACK_TRACE_AND_QUIT();
      }
    }
  }

  OBJECT_TYPE(o) = (TYPE_ID(STRUCTURE_TYPE(structure)) << 2) | 2;

  return o;
}

/* get a field's value from a structure instance */
struct object *struct_field(struct object *instance, struct object *sdes) {
  struct object *cursor, *field, *field_name, *structure;
  void *ptr;
  ufixnum_t i;
  fixnum_t fix;
  ufixnum_t ufix;
  flonum_t flo;

  OT2("struct_field", 1, sdes, type_dynamic_byte_array, type_string);

  structure = TYPE_STRUCT(type_of(instance));

  if (structure == NIL) {
    printf("Type is not a structure.\n");
    PRINT_STACK_TRACE_AND_QUIT();
  }

  fix = ufix = flo = 0;

  i = 0;
  field = NULL;
  field_name = get_string_designator(sdes);
  cursor = STRUCTURE_FIELDS(structure);
  while (cursor != NIL) {
    if (equals(field_name, get_string_designator(CONS_CAR(CONS_CAR(cursor))))) {
      field = CONS_CAR(CONS_CDR(CONS_CAR(cursor)));
      break;
    }
    cursor = CONS_CDR(cursor);
    ++i;
  }

  if (field == NULL) {
    printf("Field does not exist on structure.\n");
    PRINT_STACK_TRACE_AND_QUIT();
  }

  ptr = &((char *)OBJECT_POINTER(instance))[STRUCTURE_OFFSETS(structure)[i]];
  if (field == gis->int_type) {
    return fixnum(*(int*)ptr);
  } else if (field == gis->char_type) {
    return fixnum(*(char*)ptr);
  } else if (field == gis->uint8_type) {
    return fixnum(*(uint8_t*)ptr);
  } else if (field == gis->pointer_type) {
    return pointer((void*)ptr);
  } else {
    printf("Unsupported field type.");
    PRINT_STACK_TRACE_AND_QUIT();
  }
}

/* set a field's value from a structure instance */
void set_struct_field(struct object *instance, struct object *sdes, struct object *value) {
  struct object *cursor, *field, *field_name, *structure;
  ufixnum_t i;
  fixnum_t fix;
  ufixnum_t ufix;
  flonum_t flo;

  OT2("set_struct_field", 1, sdes, type_dynamic_byte_array, type_string);

  structure = TYPE_STRUCT(type_of(instance));

  if (structure == NIL) {
    printf("Type is not a structure.\n");
    PRINT_STACK_TRACE_AND_QUIT();
  }

  fix = ufix = flo = 0;

  i = 0;
  field = NULL;
  field_name = get_string_designator(sdes);
  cursor = STRUCTURE_FIELDS(structure);
  while (cursor != NIL) {
    if (equals(field_name, get_string_designator(CONS_CAR(CONS_CAR(cursor))))) {
      field = CONS_CAR(CONS_CDR(CONS_CAR(cursor)));
      break;
    }
    cursor = CONS_CDR(cursor);
    ++i;
  }

  if (field == NULL) {
    printf("Field does not exist on structure.\n");
    PRINT_STACK_TRACE_AND_QUIT();
  }

  memcpy(&((char *)OBJECT_POINTER(instance))[STRUCTURE_OFFSETS(structure)[i]], &FIXNUM_VALUE(value), sizeof(int));
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
  FUNCTION_NARGS(o) = 0;
  FUNCTION_IS_BUILTIN(o) = 0;
  FUNCTION_IS_MACRO(o) = 0;
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

struct object *package(struct object *name, struct object *packages) {
  struct object *o;
  OT("package", 0, name, type_string);
  OT_LIST("package", 1, packages);
  o = object(type_package);
  NC(o, "Failed to allocate package object.");
  o->w1.value.package = malloc(sizeof(struct package));
  NC(o->w1.value.package, "Failed to allocate package object.");
  PACKAGE_NAME(o) = name;
  PACKAGE_SYMBOLS(o) = NIL;
  PACKAGE_PACKAGES(o) = packages;
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
  SYMBOL_STRUCTURE_IS_SET(o) = 0;
  SYMBOL_TYPE_IS_SET(o) = 0;

  SYMBOL_FUNCTION(o) = NIL;
  SYMBOL_VALUE(o) = NIL;
  SYMBOL_STRUCTURE(o) = NIL;
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
    printf("BC: Failed to open file at path %s with mode %s.\n", STRING_CONTENTS(path),
           STRING_CONTENTS(path));
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
  if (!fclose(FILE_FP(file))) {
    printf("failed to close file\n");
    PRINT_STACK_TRACE_AND_QUIT();
  }
}

/*
 * Dynamic Array
 */
struct object *dynamic_array_get_ufixnum_t(struct object *da, ufixnum_t index) {
  OT("dynamic_array_get", 0, da, type_dynamic_array);
  #ifdef RUN_TIME_CHECKS
    if (index >= DYNAMIC_ARRAY_LENGTH(da)) {
      printf("Index out of bounds.\n");
      PRINT_STACK_TRACE_AND_QUIT();
    }
  #endif
  return DYNAMIC_ARRAY_VALUES(da)[index];
}
struct object *dynamic_array_get(struct object *da, struct object *index) {
  OT("dynamic_array_get", 0, da, type_dynamic_array);
  OT("dynamic_array_get", 1, index, type_fixnum);
  #ifdef RUN_TIME_CHECKS
    if (FIXNUM_VALUE(index) >= DYNAMIC_ARRAY_LENGTH(da)) {
      printf("Index out of bounds.\n");
      PRINT_STACK_TRACE_AND_QUIT();
    }
  #endif
  return DYNAMIC_ARRAY_VALUES(da)[FIXNUM_VALUE(index)];
}
struct object *dynamic_array_set_ufixnum_t(struct object *da, ufixnum_t index, struct object *value) {
  OT("dynamic_array_set", 0, da, type_dynamic_array);
  #ifdef RUN_TIME_CHECKS
    if (index >= DYNAMIC_ARRAY_LENGTH(da)) {
      printf("Index out of bounds.\n");
      PRINT_STACK_TRACE_AND_QUIT();
    }
  #endif
  DYNAMIC_ARRAY_VALUES(da)[index] = value;
  return NIL;
}
void dynamic_array_set(struct object *da, struct object *index, struct object *value) {
  OT("dynamic_array_set", 0, da, type_dynamic_array);
  OT("dynamic_array_set", 1, index, type_fixnum);
  OT("dynamic_array_set", 2, value, type_fixnum);
  #ifdef RUN_TIME_CHECKS
    if (FIXNUM_VALUE(index) >= DYNAMIC_ARRAY_LENGTH(da)) {
      printf("Index out of bounds.\n");
      PRINT_STACK_TRACE_AND_QUIT();
    }
  #endif
  DYNAMIC_ARRAY_VALUES(da)[FIXNUM_VALUE(index)] = value;
}
struct object *dynamic_array_length(struct object *da) {
  OT("dynamic_array_length", 0, da, type_dynamic_array);
  return fixnum(DYNAMIC_ARRAY_LENGTH(da));
}
void dynamic_array_ensure_capacity(struct object *da) {
  struct object **values, **nv;
  size_t size; 
  if (DYNAMIC_ARRAY_LENGTH(da) >= DYNAMIC_ARRAY_CAPACITY(da)) {
    DYNAMIC_ARRAY_CAPACITY(da) = (DYNAMIC_ARRAY_LENGTH(da) + 1) * 3/2.0;
    values = DYNAMIC_ARRAY_VALUES(da);
    size = DYNAMIC_ARRAY_CAPACITY(da) * sizeof(struct object*);
    nv = realloc(values, size);
    DYNAMIC_ARRAY_VALUES(da) = nv;
    if (DYNAMIC_ARRAY_VALUES(da) == NULL) {
      printf("BC: Failed to realloc dynamic-array.");
      PRINT_STACK_TRACE_AND_QUIT();
    }
  }
}
void dynamic_array_push(struct object *da, struct object *value) {
  OT("dynamic_array_push", 0, da, type_dynamic_array);
  dynamic_array_ensure_capacity(da);
  DYNAMIC_ARRAY_VALUES(da)[DYNAMIC_ARRAY_LENGTH(da)] = value;
  ++DYNAMIC_ARRAY_LENGTH(da);
}
struct object *dynamic_array_pop(struct object *da) {
  OT("dynamic_array_pop", 0, da, type_dynamic_array);
#ifdef RUN_TIME_CHECKS
  if (DYNAMIC_ARRAY_LENGTH(da) < 1) {
    printf("BC: Attempted to pop an empty dynamic-array");
    PRINT_STACK_TRACE_AND_QUIT();
  }
#endif
  return DYNAMIC_ARRAY_VALUES(da)[--DYNAMIC_ARRAY_LENGTH(da)];
}
struct object *dynamic_array_concat(struct object *da0, struct object *da1) {
  struct object *da2; 

  OT("dynamic_array_concat", 0, da0, type_dynamic_array);
  OT("dynamic_array_concat", 1, da1, type_dynamic_array);

  da2 = dynamic_array(DYNAMIC_ARRAY_CAPACITY(da0) + DYNAMIC_ARRAY_CAPACITY(da1));
  memcpy(DYNAMIC_ARRAY_VALUES(da2), DYNAMIC_ARRAY_VALUES(da0), DYNAMIC_ARRAY_LENGTH(da0) * sizeof(char));
  memcpy(&DYNAMIC_ARRAY_VALUES(da2)[DYNAMIC_ARRAY_LENGTH(da0)], DYNAMIC_ARRAY_VALUES(da1), DYNAMIC_ARRAY_LENGTH(da1) * sizeof(char));
  return da2;
}

/*
 * Dynamic Byte Array
 */
char dynamic_byte_array_get(struct object *dba, fixnum_t index) {
  OT2("dynamic_byte_array_get", 0, dba, type_dynamic_byte_array, type_string);
  #ifdef RUN_TIME_CHECKS
    if (index >= DYNAMIC_BYTE_ARRAY_LENGTH(dba)) {
      printf("Index out of bounds.\n");
      PRINT_STACK_TRACE_AND_QUIT();
    }
  #endif
  return DYNAMIC_BYTE_ARRAY_BYTES(dba)[index];
}
void dynamic_byte_array_set(struct object *dba, ufixnum_t index, char value) {
  OT2("dynamic_byte_array_set", 0, dba, type_dynamic_byte_array, type_string);
  #ifdef RUN_TIME_CHECKS
    if (index >= DYNAMIC_BYTE_ARRAY_LENGTH(dba)) {
      printf("Index out of bounds.\n");
      PRINT_STACK_TRACE_AND_QUIT();
    }
  #endif
  DYNAMIC_BYTE_ARRAY_BYTES(dba)[index] = value;
}
struct object *dynamic_byte_array_length(struct object *dba) {
  OT2("dynamic_byte_array_length", 0, dba, type_dynamic_byte_array, type_string);
  return fixnum(DYNAMIC_BYTE_ARRAY_LENGTH(dba));
}
void dynamic_byte_array_ensure_capacity(struct object *dba) {
  if (DYNAMIC_BYTE_ARRAY_LENGTH(dba) >= DYNAMIC_BYTE_ARRAY_CAPACITY(dba)) {
    DYNAMIC_BYTE_ARRAY_CAPACITY(dba) = (DYNAMIC_BYTE_ARRAY_LENGTH(dba) + 1) * 3/2.0;
    DYNAMIC_BYTE_ARRAY_BYTES(dba) = realloc(DYNAMIC_BYTE_ARRAY_BYTES(dba), DYNAMIC_BYTE_ARRAY_CAPACITY(dba) * sizeof(char));
    if (DYNAMIC_BYTE_ARRAY_BYTES(dba) == NULL) {
      printf("BC: Failed to realloc dynamic-byte-array.");
      PRINT_STACK_TRACE_AND_QUIT();
    }
  }
}
struct object *dynamic_byte_array_push(struct object *dba, struct object *value) {
  OT2("dynamic_byte_array_push", 0, dba, type_dynamic_byte_array, type_string);
  OT("dynamic_byte_array_push", 1, value, type_fixnum);
  dynamic_byte_array_ensure_capacity(dba);
  DYNAMIC_BYTE_ARRAY_BYTES(dba)[DYNAMIC_BYTE_ARRAY_LENGTH(dba)++] = FIXNUM_VALUE(value);
  return NIL;
}
/** pushes a single byte (to avoid wrapping in a fixnum object) */
void dynamic_byte_array_push_char(struct object *dba, char x) {
  OT2("dynamic_byte_array_push_char", 0, dba, type_dynamic_byte_array, type_string);
  dynamic_byte_array_ensure_capacity(dba);
  DYNAMIC_BYTE_ARRAY_BYTES(dba)[DYNAMIC_BYTE_ARRAY_LENGTH(dba)++] = x;
}
/* It is required that there is no validation done on the dba that is passed here, otherwise it will cause an infinite loop with type_of */
void dynamic_byte_array_force_cstr(struct object *dba) { /* uses the data outside of the dynamic array to work as a null terminated string */
  OT2("dynamic_byte_array_force_cstr", 0, dba, type_dynamic_byte_array, type_string);
  dynamic_byte_array_push_char(dba, '\0'); /* TODO: could be improved to check if there is already a null byte at the end */
  DYNAMIC_BYTE_ARRAY_LENGTH(dba) -= 1;
}
/** pushes all items from dba1 to the end of dba0 */
struct object *dynamic_byte_array_push_all(struct object *dba0, struct object *dba1) {
  ufixnum_t i;
  OT2("dynamic_byte_array_push_all", 0, dba0, type_dynamic_byte_array, type_string);
  OT2("dynamic_byte_array_push_all", 1, dba1, type_dynamic_byte_array, type_string);
  for (i = 0; i < DYNAMIC_ARRAY_LENGTH(dba1); ++i)
    dynamic_byte_array_push_char(dba0, DYNAMIC_BYTE_ARRAY_BYTES(dba1)[i]);
  return NIL;
}

void dynamic_byte_array_insert_char(struct object *dba, ufixnum_t i, char x) {
  ufixnum_t j;
  
  OT2("dynamic_byte_array_insert_char", 0, dba, type_dynamic_byte_array, type_string);

  dynamic_byte_array_ensure_capacity(dba);
  for (j = DYNAMIC_BYTE_ARRAY_LENGTH(dba); j > i; --j) {
    DYNAMIC_BYTE_ARRAY_BYTES(dba)[j] = DYNAMIC_BYTE_ARRAY_BYTES(dba)[j-1];
  }
  DYNAMIC_BYTE_ARRAY_BYTES(dba)[i] = x;
  ++DYNAMIC_BYTE_ARRAY_LENGTH(dba);
}

struct object *dynamic_byte_array_pop(struct object *dba) {
  OT2("dynamic_byte_array_pop", 0, dba, type_dynamic_byte_array, type_string);
  if (DYNAMIC_BYTE_ARRAY_LENGTH(dba) < 1) {
    printf("BC: Attempted to pop an empty dynamic-byte-array");
    PRINT_STACK_TRACE_AND_QUIT();
  }
  DYNAMIC_BYTE_ARRAY_LENGTH(dba)--;
  return NIL;
}
struct object *dynamic_byte_array_concat(struct object *dba0, struct object *dba1) {
  struct object *dba2; 

  OT2("dynamic_byte_array_concat", 0, dba0, type_dynamic_byte_array, type_string);
  OT2("dynamic_byte_array_concat", 1, dba1, type_dynamic_byte_array, type_string);

  dba2 = dynamic_byte_array(DYNAMIC_BYTE_ARRAY_CAPACITY(dba0) + DYNAMIC_BYTE_ARRAY_CAPACITY(dba1));
  DYNAMIC_BYTE_ARRAY_LENGTH(dba2) = DYNAMIC_BYTE_ARRAY_LENGTH(dba0) + DYNAMIC_BYTE_ARRAY_LENGTH(dba1);
  memcpy(DYNAMIC_BYTE_ARRAY_BYTES(dba2), DYNAMIC_BYTE_ARRAY_BYTES(dba0), DYNAMIC_BYTE_ARRAY_LENGTH(dba0) * sizeof(char));
  memcpy(&DYNAMIC_BYTE_ARRAY_BYTES(dba2)[DYNAMIC_BYTE_ARRAY_LENGTH(dba0)], DYNAMIC_BYTE_ARRAY_BYTES(dba1), DYNAMIC_BYTE_ARRAY_LENGTH(dba1) * sizeof(char));
  return dba2;
}
struct object *dynamic_byte_array_from_array(ufixnum_t length, unsigned char *arr) {
  struct object *dba;
  dba = dynamic_byte_array(length);
  DYNAMIC_BYTE_ARRAY_LENGTH(dba) = length;
  memcpy(DYNAMIC_BYTE_ARRAY_BYTES(dba), arr, sizeof(unsigned char));
  return dba;
}
struct object *string_concat(struct object *s0, struct object *s1) {
  struct object *s2; 

  OT2("string_concat", 0, s0, type_string, type_dynamic_byte_array);
  OT2("string_concat", 1, s1, type_string, type_dynamic_byte_array);

  s2 = dynamic_byte_array_concat(s0, s1);
  OBJECT_TYPE(s2) = type_string;
  return s2;
}

/*===============================*
 *===============================*
 * error handling                *
 *===============================*
 *===============================*/
void print_stack() {
  ufixnum_t i, j, len;
  struct object *f;
  len = DYNAMIC_ARRAY_LENGTH(gis->call_stack);
  i = 0;
  while (i < len) {
    f = dynamic_array_get_ufixnum_t(gis->call_stack, i);
    if (f == NIL) { /* top level */
      printf("Top level\n");
      i += 2;
      continue;
    }
    dynamic_byte_array_force_cstr(FUNCTION_NAME(f));
    printf("(%s ", STRING_CONTENTS(FUNCTION_NAME(f)));
    i += 1; /* got to instruction index */
    i += 1; /* skip instruction index for now -- should translate this to line number later */
    for (j = 0; j < FUNCTION_NARGS(f); ++j) {
      print(dynamic_array_get_ufixnum_t(gis->call_stack, i));
      i += 1;
    }
    printf(")\n");
  }
  printf("instruction-index = ");
  print(symbol_get_value(gis->impl_i_sym));
  printf("\n");
}
/*===============================*
 *===============================*
 * to-string                     *
 *===============================*
 *===============================*/
struct object *do_to_string(struct object *o, char repr);

void reverse_string(struct object *o) {
  ufixnum_t i;
  char temp;

  OT("reverse_string", 0, o, type_string);

  for (i = 0; i < STRING_LENGTH(o)/2; ++i) {
    temp = STRING_CONTENTS(o)[i];
    STRING_CONTENTS(o)[i] = STRING_CONTENTS(o)[STRING_LENGTH(o)-1-i];
    STRING_CONTENTS(o)[STRING_LENGTH(o)-1-i] = temp;
  }
}

struct object *to_string_ufixnum_t(ufixnum_t n) {
  struct object *str;
  ufixnum_t cursor;

  if (n == 0) return string("0");
  str = string("");
  cursor = n;
  while (cursor != 0) {
    dynamic_byte_array_push_char(str, '0' + cursor % 10);
    cursor /= 10;
  }
  reverse_string(str);
  return str;
}

struct object *to_string_fixnum_t(fixnum_t n) {
  if (n < 0)
    return dynamic_byte_array_concat(string("-"), to_string_ufixnum_t(-n));
  return to_string_ufixnum_t(n);
}

/* a buggy float to string implementation 
   shows a maximum of 9 decimal digits even if the double contains more */
struct object *to_string_flonum_t(flonum_t n) {
  ufixnum_t integral_part, decimal_part;
  ufixnum_t ndecimal, nintegral; /* how many digits in the decimal and integral part to keep */
  flonum_t nexp, nexp2;
  fixnum_t i, leading_zero_threshold;
  struct object *str;

  if (n == 0) return string("0.0");
  if (n != n) return string("NaN");
  if (n > DBL_MAX) return string("Infinity"); /* TODO: update to be FLONUM_MAX/FLONUM_MIN*/
  if (n < DBL_MIN) return string("-Infinity");

  ndecimal = 9;
  nintegral = 16; /* the threshold where scientific notation is used */
  leading_zero_threshold = 7; /* the threshold when scientific notation is used */

  nexp = n == 0 ? 0 : log10(n);

  integral_part = n; /* casting to a ufixnum extracts the integral part */
  decimal_part = (n - integral_part) * pow(10, ndecimal);

  while (decimal_part > 0 && decimal_part % 10 == 0) decimal_part /= 10;

  if (nexp > nintegral) {
    /* trim all right most 0s (those will be the decimal part of the normalized output) */
    while (integral_part > 0 && integral_part % 10 == 0) integral_part /= 10;

    nexp2 = log10(integral_part);
    if (nexp2 > ndecimal + 1) {
      for (i = nexp2 - ndecimal; i > 0;
           --i) { /* keep ndecimal + 1 digits (plus one is to keep the first
                     part of the scientific notation 9.2342341e+4) */
        if (i == 1 && integral_part % 10 > 4) {
          integral_part += 10; /* round if this is the last digit and 5 or more */
        }
        integral_part /= 10;
      }
    }
    str = to_string_ufixnum_t(integral_part);
    if (integral_part >= 10)
      dynamic_byte_array_insert_char(str, 1, '.');
    dynamic_byte_array_push_char(str, 'e');
    dynamic_byte_array_push_char(str, '+');
    str = dynamic_byte_array_concat(str, to_string_fixnum_t(nexp));
    return str;
  }

  if (nexp <= -leading_zero_threshold) { /* check if this could be a 0.{...0...}n
                                           number */
    /* first is to normalize 0.00000000nnnn to 0.nnnn, second is to convert to unsigned fixnum (plus one because one extra digit will be in the integral part another plus one so we can have one digit for rounding) */
    decimal_part = n * pow(10, (fixnum_t)-nexp) * pow(10, ndecimal + 1 + 1);
    if (decimal_part % 10 > 4) {
      decimal_part += 10; /* round if this is the last digit and 5 or more */
    }
    decimal_part /= 10; /* drop the extra digit we grabbed for rounding */
    while (decimal_part > 0 && decimal_part % 10 == 0) decimal_part /= 10; /* trim all rhs zeros */
    str = to_string_ufixnum_t(decimal_part);
    if (decimal_part >= 10) dynamic_byte_array_insert_char(str, 1, '.');
    dynamic_byte_array_push_char(str, 'e');
    str = dynamic_byte_array_concat(str, to_string_fixnum_t(floor(nexp)));
    return str;
  }

  str = to_string_ufixnum_t(integral_part);
  dynamic_byte_array_push_char(str, '.');
  while (nexp < -1) {
    dynamic_byte_array_push_char(str, '0');
    ++nexp;
  }
  str = dynamic_byte_array_concat(str, to_string_fixnum_t(decimal_part));
  return str;
}

struct object *to_string_dynamic_byte_array(struct object *dba) {
  struct object *str;
  ufixnum_t i;
  unsigned char byte, nib0, nib1;

  OT("to_string_dynamic_byte_array", 0, dba, type_dynamic_byte_array);

  str = string("[");
  for (i = 0; i < DYNAMIC_BYTE_ARRAY_LENGTH(dba); ++i) {
    byte = DYNAMIC_BYTE_ARRAY_BYTES(dba)[i];
    nib0 = byte & 0x0F;
    nib1 = byte >> 4;

    dynamic_byte_array_push_char(str, '0');
    dynamic_byte_array_push_char(str, 'x');
    if (nib1 > 9) dynamic_byte_array_push_char(str, 'A' + (nib1 - 10));
    else dynamic_byte_array_push_char(str, '0' + nib1);
    if (nib0 > 9) dynamic_byte_array_push_char(str, 'A' + (nib0 - 10));
    else dynamic_byte_array_push_char(str, '0' + nib0);

    dynamic_byte_array_push_char(str, ' ');
  }
  if (DYNAMIC_BYTE_ARRAY_LENGTH(dba) > 0)
    dynamic_byte_array_pop(str); /* remove the extra space */
  dynamic_byte_array_push_char(str, ']');
  return str;
}

struct object *to_string_vec2(struct object *vec2) {
  struct object *str;

  OT("to_string_vec2", 0, vec2, type_vec2);

  str = string("<");
  str = dynamic_byte_array_concat(str, to_string_flonum_t(VEC2_X(vec2)));
  dynamic_byte_array_push_char(str, ' ');
  str = dynamic_byte_array_concat(str, to_string_flonum_t(VEC2_Y(vec2)));
  dynamic_byte_array_push_char(str, '>');
  OBJECT_TYPE(str) = type_string;
  return str;
}

struct object *to_string_dynamic_array(struct object *da) {
  struct object *str;
  ufixnum_t i;

  OT("to_string_dynamic_array", 0, da, type_dynamic_array);

  str = string("[");
  for (i = 0; i < DYNAMIC_ARRAY_LENGTH(da); ++i) {
    str = string_concat(str, do_to_string(dynamic_array_get_ufixnum_t(da, i), 1));
    dynamic_byte_array_push_char(str, ' ');
  }
  if (DYNAMIC_ARRAY_LENGTH(da) > 0)
    dynamic_byte_array_pop(str); /* remove the extra space */
  dynamic_byte_array_push_char(str, ']');
  return str;
}

struct object *do_to_string(struct object *o, char repr) {
  struct object *str;
  char buf[100];
  enum object_type t;

  t = object_type_of(o);

  switch (t) {
    case type_cons:
      str = string("(");
      str = string_concat(str, do_to_string(CONS_CAR(o), 1));
      while (type_of(CONS_CDR(o)) == gis->cons_type) {
        o = CONS_CDR(o);
        str = string_concat(str, string(" "));
        str = string_concat(str, do_to_string(CONS_CAR(o), 1));
      }
      if (CONS_CDR(o) == NIL) {
        dynamic_byte_array_push_char(str, ')');
      } else {
        str = string_concat(str, string(" "));
        str = string_concat(str, do_to_string(CONS_CDR(o), 1));
        dynamic_byte_array_push_char(str, ')');
      }
      return str;
    case type_string:
      if (repr) {
        o = string_concat(string("\""), o);
        dynamic_byte_array_push_char(o, '\"');
      }
      return o;
    case type_flonum:
      return to_string_flonum_t(FLONUM_VALUE(o));
    case type_ufixnum:
      return to_string_ufixnum_t(UFIXNUM_VALUE(o));
    case type_fixnum:
      return to_string_fixnum_t(FIXNUM_VALUE(o));
    case type_dynamic_byte_array:
      return to_string_dynamic_byte_array(o);
    case type_vec2:
      return to_string_vec2(o);
    case type_dynamic_array:
      return to_string_dynamic_array(o);
    case type_function:
      if (0 && FUNCTION_NAME(o) != NIL) {
        str = string("<function ");
        str = string_concat(str, SYMBOL_NAME(FUNCTION_NAME(o)));
        dynamic_byte_array_push_char(str, '>');
        return str;
      }
      str = string("<function ");
      str = string_concat(str, do_to_string(FUNCTION_CONSTANTS(o), 1));
      dynamic_byte_array_push_char(str, ' ');
      str = string_concat(str, do_to_string(FUNCTION_CODE(o), 1));
      dynamic_byte_array_push_char(str, '>');
      return str;
    case type_record:
      return string("<record>");
    case type_package:
      str = string("<package ");
      str = string_concat(str, do_to_string(PACKAGE_NAME(o), 1));
      dynamic_byte_array_push_char(str, '>');
      return str;
    case type_symbol:
      return SYMBOL_NAME(o);
    case type_dlib:
      str = string("<dynamic-library \"");
      str = string_concat(str, DLIB_PATH(o));
      dynamic_byte_array_push_char(str, '"');
      dynamic_byte_array_push_char(str, '>');
      return str;
    case type_ffun:
      str = string("<foreign-function \"");
      str = string_concat(str, FFUN_FFNAME(o));
      dynamic_byte_array_push_char(str, '"');
      dynamic_byte_array_push_char(str, ' ');
      str = string_concat(str, do_to_string(FFUN_DLIB(o), 1));
      dynamic_byte_array_push_char(str, '>');
      return str;
    case type_ptr:
      str = string("<pointer ");
      sprintf(buf, "%p", OBJECT_POINTER(o));
      dynamic_byte_array_push_char(str, '0');
      dynamic_byte_array_push_char(str, 'x');
      str = string_concat(str, string(buf));
      dynamic_byte_array_push_char(str, '>');
      return str;
    case type_struct:
      str = string("<struct ");
      str = string_concat(str, STRUCTURE_NAME(o));
      dynamic_byte_array_push_char(str, '>');
      return str;
    case type_type:
      str = string("<type ");
      str = string_concat(str, TYPE_NAME(o));
      dynamic_byte_array_push_char(str, '>');
      return str;
    case type_enumerator:
      return string("<enumerator>");
    case type_file:
      return string("<file>");
  }

  /* handle user defined type */
  str = string("<");
  str = string_concat(str, TYPE_NAME(type_of(o)));
  dynamic_byte_array_push_char(str, ' ');
  sprintf(buf, "%p", OBJECT_POINTER(o));
  str = string_concat(str, string(buf));
  dynamic_byte_array_push_char(str, '>');
  return str;
}

struct object *to_string(struct object *o) {
  return do_to_string(o, 0);
}

struct object *to_repr(struct object *o) {
  return do_to_string(o, 1);
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
    case type_struct:
      return STRUCTURE_NAME(o0) == STRUCTURE_NAME(o1) && STRUCTURE_FFI_TYPE(o0) == STRUCTURE_FFI_TYPE(o1);
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
  }

  /* TODO: handle user defined types */

  printf("Equality is not supported for the given type.");
  PRINT_STACK_TRACE_AND_QUIT();
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
    printf(" has no function.");
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
    printf(" has no type.");
    PRINT_STACK_TRACE_AND_QUIT();
  }
}

struct object *symbol_get_struct(struct object *sym) {
  if (SYMBOL_STRUCTURE_IS_SET(sym)) {
    return SYMBOL_STRUCTURE(sym);
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
    printf(" has no struct.");
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

void symbol_set_structure(struct object *sym, struct object *s) {
  SYMBOL_STRUCTURE(sym) = s;
  SYMBOL_STRUCTURE_IS_SET(sym) = 1;
}

void symbol_set_value(struct object *sym, struct object *value) {
  SYMBOL_VALUE(sym) = value;
  SYMBOL_VALUE_IS_SET(sym) = 1;
}

void symbol_export(struct object *sym) {
  SYMBOL_IS_EXTERNAL(sym) = 1;
}

struct object *get_string_designator(struct object *sd) {
  struct object *t = type_of(sd);
  if (t == gis->string_type)
    return sd;
  else if (t == gis->symbol_type)
    return SYMBOL_NAME(sd);
  else {
    printf("Invalid string designator %s.\n", type_name_cstr(t));
    PRINT_STACK_TRACE_AND_QUIT();
  }
}

/* uses NULL as a sentinel value for "did not find" */
struct object *do_find_symbol(struct object *string, struct object *package, char include_internal) {
  struct object *cursor, *package_cursor, *pack, *sym;

  OT("find_symbol", 0, string, type_string);
  OT("find_symbol", 1, package, type_package);

  /* look in current package for the symbol
     if we are only looking for inherited symbols with this name, don't look in
     the actual package */
  if (include_internal) {
    cursor = PACKAGE_SYMBOLS(package);
    while (cursor != NIL) {
      sym = CONS_CAR(cursor);
      if (equals(SYMBOL_NAME(sym), string)) {
        return sym;
      }
      cursor = CONS_CDR(cursor);
    }

    /* look in all exported symbols of used packages */
    package_cursor = PACKAGE_PACKAGES(package);
    while (package_cursor != NIL) {
      pack = CONS_CAR(package_cursor);
      sym = do_find_symbol(string, pack, 0);
      if (sym != NULL)
        return sym;
      package_cursor = CONS_CDR(package_cursor);
    }
  } else {
    cursor = PACKAGE_SYMBOLS(package);
    while (cursor != NIL) {
      sym = CONS_CAR(cursor);
      if (SYMBOL_IS_EXTERNAL(sym) &&
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

/*===============================*
 *===============================*
 * Byte Streams (for marshaling) *
 *===============================*
 *===============================*/

/**
 * Idempotently lift the value into a stream.
 */
struct object *byte_stream_lift(struct object *e) {
  if (type_of(e) == gis->dynamic_byte_array_type||
      type_of(e) == gis->string_type) {
    return enumerator(e);
  } else if (type_of(e) == gis->file_type ||
             type_of(e) == gis->enumerator_type) {
    return e;
  } else {
    printf("BC: attempted to lift unsupported type %s into a byte-stream",
           type_name_of_cstr(e));
    PRINT_STACK_TRACE_AND_QUIT();
  }
}

struct object *byte_stream_do_read(struct object *e, fixnum_t n, char peek) {
  struct object *ret, *t;

  OT2("byte_stream_get", 0, e, type_enumerator, type_file);

  ret = dynamic_byte_array(n);
  DYNAMIC_ARRAY_LENGTH(ret) = n;

  if (type_of(e) == gis->file_type) {
    fread(DYNAMIC_BYTE_ARRAY_BYTES(ret), sizeof(char), n, FILE_FP(e));
    if (peek) fseek(FILE_FP(e), -n, SEEK_CUR);
  } else {
    t = type_of(ENUMERATOR_SOURCE(e));
    if (t == gis->dynamic_byte_array_type || t == gis->string_type) {
        memcpy(DYNAMIC_BYTE_ARRAY_BYTES(ret),
               &DYNAMIC_BYTE_ARRAY_BYTES(
                   ENUMERATOR_SOURCE(e))[ENUMERATOR_INDEX(e)],
               sizeof(char) * n);
        if (!peek) ENUMERATOR_INDEX(e) += n;
    } else {
        printf("BC: byte stream get is not implemented for type %s.",
               type_name_of_cstr(e));
        PRINT_STACK_TRACE_AND_QUIT();
    }
  }
  return ret;
}

char byte_stream_has(struct object *e) {
  char c;
  struct object *t;
  OT2("byte_steam_has", 0, e, type_enumerator, type_file);
  if (type_of(e) == gis->file_type) {
    c = fgetc(FILE_FP(e));
    ungetc(c, FILE_FP(e));
    return c != EOF;
  } else {
    t = type_of(ENUMERATOR_SOURCE(e));
    if (t == gis->dynamic_byte_array_type || t == gis->string_type) {
        return ENUMERATOR_INDEX(e) < DYNAMIC_BYTE_ARRAY_LENGTH(ENUMERATOR_SOURCE(e));
    } else {
      print(ENUMERATOR_SOURCE(e));
        printf("BC: byte_stream_has is not implemented for type %s.\n",
               type_name_of_cstr(ENUMERATOR_SOURCE(e)));
        PRINT_STACK_TRACE_AND_QUIT();
    }
  }
  return c;
}

char byte_stream_do_read_byte(struct object *e, char peek) {
  struct object *t;
  char c;
  OT2("byte_steam_get_char", 0, e, type_enumerator, type_file);
  if (type_of(e) == gis->file_type) {
    c = fgetc(FILE_FP(e));
    if (peek) ungetc(c, FILE_FP(e));
  } else {
    t = type_of(ENUMERATOR_SOURCE(e));
    if (t == gis->dynamic_byte_array_type || t == gis->string_type) {
        c = DYNAMIC_BYTE_ARRAY_BYTES(ENUMERATOR_SOURCE(e))[ENUMERATOR_INDEX(e)];
        if (!peek) ++ENUMERATOR_INDEX(e);
    } else {
        printf("BC: byte stream get char is not implemented for type %s.",
               type_name_of_cstr(ENUMERATOR_SOURCE(e)));
        PRINT_STACK_TRACE_AND_QUIT();
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

/** creates a new global interpreter state with one package (called "lisp")
 *  which is set to the current package.
 */
void gis_init(char load_core) {
  gis = malloc(sizeof(struct gis));
  if (gis == NULL) {
    printf("Failed to allocate global interpreter state.");
    exit(1);
  }

  /* Initialize all strings */
/* Creates a string on the GIS. */
#define GIS_STR(str, cstr) str = string(cstr);
  GIS_STR(gis->a_str, "a");
  GIS_STR(gis->and_str, "and");
  GIS_STR(gis->add_str, "add");
  GIS_STR(gis->alloc_struct_str, "alloc-struct");
  GIS_STR(gis->b_str, "b");
  GIS_STR(gis->call_stack_str, "call-stack");
  GIS_STR(gis->call_str, "call");
  GIS_STR(gis->call_str, "call");
  GIS_STR(gis->car_str, "car");
  GIS_STR(gis->cdr_str, "cdr");
  GIS_STR(gis->char_str, "char");
  GIS_STR(gis->compile_str, "compile");
  GIS_STR(gis->cons_str, "cons");
  GIS_STR(gis->data_stack_str, "data-stack");
  GIS_STR(gis->div_str, "/");
  GIS_STR(gis->drop_str, "drop");
  GIS_STR(gis->dynamic_array_str, "dynamic-array");
  GIS_STR(gis->dynamic_byte_array_str, "dynamic_byte_array_str");
  GIS_STR(gis->dynamic_library_str, "dynamic-library");
  GIS_STR(gis->enumerator_str, "enumerator");
  GIS_STR(gis->equals_str, "=");
  GIS_STR(gis->eval_str, "eval");
  GIS_STR(gis->external_str, "external");
  GIS_STR(gis->f_str, "f");
  GIS_STR(gis->file_str, "file");
  GIS_STR(gis->find_package_str, "find-package");
  GIS_STR(gis->fixnum_str, "fixnum");
  GIS_STR(gis->flonum_str, "flonum");
  GIS_STR(gis->foreign_function_str, "foreign_function");
  GIS_STR(gis->function_str, "function");
  GIS_STR(gis->struct_field_str, "struct-field");
  GIS_STR(gis->gt_str, ">");
  GIS_STR(gis->gte_str, ">=");
  GIS_STR(gis->i_str, "i");
  GIS_STR(gis->if_str, "if");
  GIS_STR(gis->impl_str, "impl");
  GIS_STR(gis->inherited_str, "inherited");
  GIS_STR(gis->internal_str, "internal");
  GIS_STR(gis->int_str, "int");
  GIS_STR(gis->keyword_str, "keyword");
  GIS_STR(gis->list_str, "list");
  GIS_STR(gis->let_str, "let");
  GIS_STR(gis->lisp_str, "lisp");
  GIS_STR(gis->lt_str, "<");
  GIS_STR(gis->lte_str, "<=");
  GIS_STR(gis->macro_str, "macro");
  GIS_STR(gis->mul_str, "*");
  GIS_STR(gis->nil_str, "nil");
  GIS_STR(gis->or_str, "or");
  GIS_STR(gis->package_str, "package");
  GIS_STR(gis->package_symbols_str, "package-symbols");
  GIS_STR(gis->packages_str, "packages");
  GIS_STR(gis->pointer_str, "pointer");
  GIS_STR(gis->pop_str, "pop");
  GIS_STR(gis->progn_str, "progn");
  GIS_STR(gis->print_str, "print");
  GIS_STR(gis->push_str, "push");
  GIS_STR(gis->quasiquote_str, "quasiquote");
  GIS_STR(gis->quote_str, "quote");
  GIS_STR(gis->record_str, "record");
  GIS_STR(gis->set_str, "set");
  GIS_STR(gis->set_symbol_function_str, "set-symbol-function");
  GIS_STR(gis->set_struct_field_str, "set-struct-field");
  GIS_STR(gis->stack_str, "stack");
  GIS_STR(gis->string_str, "string");
  GIS_STR(gis->strings_str, "strings");
  GIS_STR(gis->struct_str, "struct");
  GIS_STR(gis->sub_str, "sub");
  GIS_STR(gis->symbol_function_str, "symbol-function");
  GIS_STR(gis->symbol_str, "symbol");
  GIS_STR(gis->symbol_struct_str, "symbol-struct");
  GIS_STR(gis->symbol_value_str, "symbol-value");
  GIS_STR(gis->t_str, "t");
  GIS_STR(gis->temp_str, "temp");
  GIS_STR(gis->type_of_str, "type-of");
  GIS_STR(gis->type_str, "type");
  GIS_STR(gis->use_package_str, "use-package"); 
  GIS_STR(gis->user_str, "user");
  GIS_STR(gis->unquote_str, "unquote");
  GIS_STR(gis->unquote_splicing_str, "unquote-splicing");
  GIS_STR(gis->ufixnum_str, "ufixnum");
  GIS_STR(gis->uint_str, "uint");
  GIS_STR(gis->uint8_str, "uint8");
  GIS_STR(gis->uint16_str, "uint16");
  GIS_STR(gis->value_str, "value");
  GIS_STR(gis->var_str, "var");
  GIS_STR(gis->void_str, "void");
  GIS_STR(gis->vec2_str, "vec2");
  GIS_STR(gis->x_str, "x");
  GIS_STR(gis->y_str, "y");

  /* Bootstrap NIL */
  NIL = symbol(gis->nil_str);
  /* All fields that are initialied to NIL must be re-initialized to NIL here because we just defined what NIL is */
  SYMBOL_PLIST(NIL) = NIL;
  SYMBOL_FUNCTION(NIL) = NIL;
  SYMBOL_PACKAGE(NIL) = NIL;
  SYMBOL_STRUCTURE(NIL) = NIL;
  SYMBOL_TYPE(NIL) = NIL;

  /* NIL evaluates to itself */
  SYMBOL_VALUE_IS_SET(NIL) = 1;
  SYMBOL_VALUE(NIL) = NIL;

  /* Initialize packages */
  gis->type_package = package(gis->type_str, NIL);

  /* final NIL bootstrapping step (required the type package -- add nil to the type package) */
  SYMBOL_PACKAGE(NIL) = gis->type_package;
  PACKAGE_SYMBOLS(gis->type_package) = cons(NIL, NIL); /* add to the type package */
  symbol_export(NIL); /* export nil */

  /* initialize other packages */
  gis->lisp_package = package(gis->lisp_str, cons(gis->type_package, NIL));
  gis->keyword_package = package(gis->keyword_str, NIL);
  gis->user_package = package(gis->user_str, cons(gis->lisp_package, NIL));
  gis->impl_package = package(gis->impl_str, NIL);

  /* Initialize Symbols */
#define GIS_SYM(sym, str, pack)                             \
  sym = symbol(str);                                        \
  SYMBOL_PACKAGE(sym) = pack;                               \
  PACKAGE_SYMBOLS(pack) = cons(sym, PACKAGE_SYMBOLS(pack)); \
  symbol_export(sym);

  GIS_SYM(gis->impl_add_sym, gis->add_str, gis->impl_package);
  GIS_SYM(gis->impl_alloc_struct_sym, gis->alloc_struct_str, gis->impl_package);
  GIS_SYM(gis->impl_and_sym, gis->and_str, gis->impl_package);
  GIS_SYM(gis->impl_call_sym, gis->call_str, gis->impl_package);
  GIS_SYM(gis->impl_call_stack_sym, gis->call_stack_str, gis->impl_package); /** stack for saving stack pointers and values for function calls (a cons list) */
  GIS_SYM(gis->impl_compile_sym, gis->compile_str, gis->impl_package);
  GIS_SYM(gis->impl_data_stack_sym, gis->data_stack_str, gis->impl_package); /** the data stack (a cons list) */
  GIS_SYM(gis->impl_drop_sym, gis->drop_str, gis->impl_package);
  GIS_SYM(gis->impl_dynamic_library_sym, gis->dynamic_library_str, gis->impl_package);
  GIS_SYM(gis->impl_eval_sym, gis->eval_str, gis->impl_package);
  GIS_SYM(gis->impl_f_sym, gis->f_str, gis->impl_package); /** the currently executing function */
  GIS_SYM(gis->impl_find_package_sym, gis->find_package_str, gis->impl_package);
  GIS_SYM(gis->impl_foreign_function_sym, gis->foreign_function_str, gis->impl_package);
  GIS_SYM(gis->impl_function_sym, gis->function_str, gis->impl_package);
  GIS_SYM(gis->impl_struct_field_sym, gis->struct_field_str, gis->impl_package);
  GIS_SYM(gis->impl_i_sym, gis->i_str, gis->impl_package); /** the index of the next instruction in bc to execute */
  GIS_SYM(gis->impl_list_sym, gis->list_str, gis->impl_package);
  GIS_SYM(gis->impl_macro_sym, gis->macro_str, gis->impl_package);
  GIS_SYM(gis->impl_package_sym, gis->package_str, gis->impl_package); /** the current package being evaluated */
  GIS_SYM(gis->impl_package_symbols_sym, gis->package_symbols_str, gis->impl_package); 
  GIS_SYM(gis->impl_packages_sym, gis->packages_str, gis->impl_package); /** all packages */
  GIS_SYM(gis->impl_pop_sym, gis->pop_str, gis->impl_package);
  GIS_SYM(gis->impl_push_sym, gis->push_str, gis->impl_package);
  GIS_SYM(gis->impl_strings_sym, gis->strings_str, gis->impl_package);
  GIS_SYM(gis->impl_set_struct_field_sym, gis->set_struct_field_str, gis->impl_package);
  GIS_SYM(gis->impl_symbol_struct_sym, gis->symbol_struct_str, gis->impl_package);
  GIS_SYM(gis->impl_type_of_sym, gis->type_of_str, gis->impl_package);
  GIS_SYM(gis->impl_use_package_sym, gis->use_package_str, gis->impl_package);
  GIS_SYM(gis->keyword_external_sym, gis->external_str, gis->keyword_package);
  GIS_SYM(gis->keyword_function_sym, gis->function_str, gis->keyword_package);
  GIS_SYM(gis->keyword_inherited_sym, gis->inherited_str, gis->keyword_package);
  GIS_SYM(gis->keyword_internal_sym, gis->internal_str, gis->keyword_package);
  GIS_SYM(gis->keyword_value_sym, gis->value_str, gis->keyword_package);
  GIS_SYM(gis->lisp_car_sym, gis->car_str, gis->lisp_package);
  GIS_SYM(gis->lisp_cdr_sym, gis->cdr_str, gis->lisp_package);
  GIS_SYM(gis->lisp_cons_sym, gis->cons_str, gis->lisp_package);
  GIS_SYM(gis->lisp_div_sym, gis->div_str, gis->lisp_package);
  GIS_SYM(gis->lisp_equals_sym, gis->equals_str, gis->lisp_package);
  GIS_SYM(gis->lisp_function_sym, gis->function_str, gis->lisp_package);
  GIS_SYM(gis->lisp_gt_sym, gis->gt_str, gis->lisp_package);
  GIS_SYM(gis->lisp_gte_sym, gis->gte_str, gis->lisp_package);
  GIS_SYM(gis->lisp_if_sym, gis->if_str, gis->lisp_package);
  GIS_SYM(gis->lisp_let_sym, gis->let_str, gis->lisp_package);
  GIS_SYM(gis->lisp_lt_sym, gis->lt_str, gis->lisp_package);
  GIS_SYM(gis->lisp_lte_sym, gis->lte_str, gis->lisp_package);
  GIS_SYM(gis->lisp_mul_sym, gis->mul_str, gis->lisp_package);
  GIS_SYM(gis->lisp_or_sym, gis->or_str, gis->lisp_package);
  GIS_SYM(gis->lisp_progn_sym, gis->progn_str, gis->lisp_package);
  GIS_SYM(gis->lisp_print_sym, gis->print_str, gis->lisp_package);
  GIS_SYM(gis->lisp_quasiquote_sym, gis->quasiquote_str, gis->lisp_package);
  GIS_SYM(gis->lisp_quote_sym, gis->quote_str, gis->lisp_package);
  GIS_SYM(gis->lisp_set_sym, gis->set_str, gis->lisp_package);
  GIS_SYM(gis->lisp_set_symbol_function_sym, gis->set_symbol_function_str, gis->lisp_package);
  GIS_SYM(gis->lisp_symbol_function_sym, gis->symbol_function_str, gis->lisp_package);
  GIS_SYM(gis->lisp_symbol_value_sym, gis->symbol_value_str, gis->lisp_package);
  GIS_SYM(gis->lisp_sub_sym, gis->sub_str, gis->lisp_package);
  GIS_SYM(gis->lisp_unquote_splicing_sym, gis->unquote_splicing_str, gis->lisp_package);
  GIS_SYM(gis->lisp_unquote_sym, gis->unquote_str, gis->lisp_package);
  GIS_SYM(gis->type_char_sym, gis->char_str, gis->type_package);
  GIS_SYM(gis->type_dynamic_array_sym, gis->dynamic_array_str, gis->type_package);
  GIS_SYM(gis->type_dynamic_byte_array_sym, gis->dynamic_byte_array_str, gis->type_package);
  GIS_SYM(gis->type_dynamic_library_sym, gis->dynamic_library_str, gis->type_package);
  GIS_SYM(gis->type_enumerator_sym, gis->enumerator_str, gis->type_package);
  GIS_SYM(gis->type_file_sym, gis->file_str, gis->type_package);
  GIS_SYM(gis->type_fixnum_sym, gis->fixnum_str, gis->type_package);
  GIS_SYM(gis->type_flonum_sym, gis->flonum_str, gis->type_package);
  GIS_SYM(gis->type_foreign_function_sym, gis->foreign_function_str, gis->type_package);
  GIS_SYM(gis->type_function_sym, gis->function_str, gis->type_package);
  GIS_SYM(gis->type_cons_sym, gis->cons_str, gis->type_package);
  GIS_SYM(gis->type_int_sym, gis->int_str, gis->type_package);
  GIS_SYM(gis->type_nil_sym, gis->nil_str, gis->type_package);
  GIS_SYM(gis->type_package_sym, gis->package_str, gis->type_package);
  GIS_SYM(gis->type_pointer_sym, gis->pointer_str, gis->type_package);
  GIS_SYM(gis->type_record_sym, gis->record_str, gis->type_package);
  GIS_SYM(gis->type_string_sym, gis->string_str, gis->type_package);
  GIS_SYM(gis->type_struct_sym, gis->struct_str, gis->type_package);
  GIS_SYM(gis->type_symbol_sym, gis->symbol_str, gis->type_package);
  GIS_SYM(gis->type_t_sym, gis->t_str, gis->type_package);
  GIS_SYM(gis->type_type_sym, gis->type_str, gis->type_package);
  GIS_SYM(gis->type_ufixnum_sym, gis->ufixnum_str, gis->type_package);
  GIS_SYM(gis->type_uint_sym, gis->uint_str, gis->type_package);
  GIS_SYM(gis->type_uint16_sym, gis->uint16_str, gis->type_package);
  GIS_SYM(gis->type_uint8_sym, gis->uint8_str, gis->type_package);
  GIS_SYM(gis->type_vec2_sym, gis->vec2_str, gis->type_package);
  GIS_SYM(gis->type_void_sym, gis->void_str, gis->type_package);

  symbol_set_value(gis->type_t_sym, gis->type_t_sym); /* t has itself as its value */

  /* Initialize types */
  /* IMPORTANT -- the ordering these types are added to the types array must match the order they are defined in bug.h for the type enum */
  /* type_of cannot be called before this is setup */
  gis->types = dynamic_array(64);

#define GIS_TYPE(t, sym, can_instantiate)    \
  t = type(SYMBOL_NAME(sym), can_instantiate); \
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
  GIS_UNINSTANTIATABLE_TYPE(gis->nil_type, gis->type_nil_sym); /* nil is uninstantiatable there is only one value of nil, that is pre-defined. We don't want users to be able to make more than one nil. */
  GIS_INSTANTIATABLE_TYPE(gis->record_type, gis->type_record_sym);
  GIS_INSTANTIATABLE_TYPE(gis->vec2_type, gis->type_vec2_sym);
  GIS_INSTANTIATABLE_TYPE(gis->dynamic_library_type, gis->type_dynamic_library_sym);
  GIS_INSTANTIATABLE_TYPE(gis->foreign_function_type, gis->type_foreign_function_sym);
  GIS_INSTANTIATABLE_TYPE(gis->struct_type, gis->type_struct_sym);
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
  GIS_UNINSTANTIATABLE_TYPE(gis->uint_type, gis->type_uint_sym);

#define GIS_BUILTIN(builtin, sym, nargs)             \
  builtin = function(NIL, NIL, nargs); \
  FUNCTION_IS_BUILTIN(builtin) = 1;    \
  FUNCTION_NARGS(builtin) = nargs;     \
  symbol_set_function(sym, builtin);

  /* all builtin functions go here */
  GIS_BUILTIN(gis->use_package_builtin, gis->impl_use_package_sym, 1) /* takes name of package */
  GIS_BUILTIN(gis->find_package_builtin, gis->impl_find_package_sym, 1) /* takes name of package */

  GIS_BUILTIN(gis->package_symbols_builtin, gis->impl_package_symbols_sym, 1) /* takes package object */
  GIS_BUILTIN(gis->dynamic_library_builtin, gis->impl_dynamic_library_sym, 1)  /* takes the path */
  GIS_BUILTIN(gis->foreign_function_builtin, gis->impl_foreign_function_sym, 4) /* takes the dlib, the name, and the parameter types */
  GIS_BUILTIN(gis->type_of_builtin, gis->impl_type_of_sym, 1) /* takes the package object */

  GIS_BUILTIN(gis->call_builtin, gis->impl_call_sym, 1) /* takes either a function value or a symbol */

  GIS_BUILTIN(gis->compile_builtin, gis->impl_compile_sym, 4) /* (compile <expr> <?bytecode> <?symbol-value-table> <?function-value-table>) */

  GIS_BUILTIN(gis->struct_builtin, gis->type_struct_sym, 2);
  GIS_BUILTIN(gis->struct_builtin, gis->lisp_struct_sym, 2);
  GIS_BUILTIN(gis->alloc_struct_builtin, gis->impl_alloc_struct_sym, 1);
  GIS_BUILTIN(gis->symbol_struct_builtin, gis->impl_symbol_struct_sym, 1);
  GIS_BUILTIN(gis->struct_field_builtin, gis->impl_struct_field_sym, 2);
  GIS_BUILTIN(gis->set_struct_field_builtin, gis->impl_set_struct_field_sym, 3);

  /* TODO: implement eval..? no, just make the compiler in bug */
  GIS_BUILTIN(gis->eval_builtin, gis->impl_eval_sym, 2); /* (eval <fun> <?i>) */

  /* initialize set interpreter state */
  gis->data_stack = dynamic_array(10);
  symbol_set_value(gis->impl_data_stack_sym, gis->data_stack);
  gis->call_stack = dynamic_array(10);
  symbol_set_value(gis->impl_call_stack_sym, gis->call_stack); /* using a dynamic array to make looking up arguments on the stack faster */
  /* by default the top level call stack has a function of NIL, the instruction index doesn't matter so it is also nil */
  dynamic_array_push(gis->call_stack, NIL);
  dynamic_array_push(gis->call_stack, NIL);

  symbol_set_value(gis->impl_package_sym, gis->user_package);
  symbol_set_value(gis->impl_packages_sym,
                  cons(gis->user_package, 
                    cons(gis->lisp_package, 
                      cons(gis->keyword_package, 
                        cons(gis->impl_package, 
                          cons(gis->type_package, NIL))))));

  symbol_set_value(gis->impl_f_sym, NIL); /* the function being executed */
  symbol_set_value(gis->impl_i_sym, ufixnum(0)); /* the instruction index */

  /* Load core.bug */
  if (load_core) {
    IF_DEBUG() printf("============ Loading core... ==============\n");
    eval(compile_entire_file(open_file(string("./src/core.bug"), string("rb"))), NIL);
    IF_DEBUG() printf("============ Core loaded... ===============\n");
  }
}

/* 
  TODO: this would be nice to have. it might have to take a bug string instead of cstring though.
        all symbol-names can be in here. 

  Only strings that are known to be immutable should be interned.

  looks up the string in the gis's interned strings - if one already exists, return it
  otherwise, create a new string and add it to the list.
   
  If is_builtin, the cstr must name a builtin. */
struct object *string_intern(char *cstr, char is_builtin) {
  return NULL;
}

struct object *string_clone(struct object *str0) {
  struct object *str1;
  OT("string_clone", 0, str0, type_string);
  str1 = dynamic_byte_array(STRING_LENGTH(str0));
  memcpy(DYNAMIC_BYTE_ARRAY_BYTES(str1), DYNAMIC_BYTE_ARRAY_BYTES(str0), STRING_LENGTH(str0));
  OBJECT_TYPE(str1) = type_string;
  STRING_LENGTH(str1) = STRING_LENGTH(str0);
  return str1;
}

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

/*===============================*
 *===============================*
 * String cache                  *
 *===============================*
 *===============================*/
struct object *string_marshal_cache_get_default() {
  struct object *cache;
  cache = dynamic_array(20);
  /* call out a bunch of names that could be frequently used in the bytecode.
     if a symbol (used as a value or looked up directly) uses one of the names below
     for its symbol-name or the home package's name. */
  dynamic_array_push(cache, gis->user_str);
  dynamic_array_push(cache, gis->lisp_str);
  dynamic_array_push(cache, gis->keyword_str);
  dynamic_array_push(cache, gis->impl_str);
  dynamic_array_push(cache, gis->t_str);
  dynamic_array_push(cache, gis->var_str);
  dynamic_array_push(cache, gis->list_str);
  dynamic_array_push(cache, gis->cons_str);
  return cache;
}

struct object *do_string_marshal_cache_intern_cstr(struct object *cache, char *str, ufixnum_t length, ufixnum_t *index, struct object *existing_str) {
  ufixnum_t i, j;
  struct object *ostr;
  for (i = 0; ; ++i) {
    skip_item:
    if (i >= DYNAMIC_ARRAY_LENGTH(cache)) break;
    ostr = DYNAMIC_ARRAY_VALUES(cache)[i];
    /* if the lengths don't match, the string will never match */
    if (length == STRING_LENGTH(ostr)) {
      /* check each byte of the strings */
      for (j = 0; j < length; ++j) {
        if (str[j] != STRING_CONTENTS(ostr)[j]) {
          ++i;
          goto skip_item;
        }
      }
      *index = i;
      return ostr;
    }
  }
  /* add to cache */
  if (existing_str == NULL) { 
    existing_str = dynamic_byte_array(length);
    memcpy(DYNAMIC_BYTE_ARRAY_BYTES(existing_str), str, length);
    STRING_LENGTH(existing_str) = length;
    OBJECT_TYPE(existing_str) = type_string;
  }
  dynamic_array_push(cache, existing_str);
  *index = DYNAMIC_ARRAY_LENGTH(cache) - 1;
  return existing_str;
}

struct object *string_marshal_cache_intern_cstr(struct object *cache, char *str, ufixnum_t *index) {
  return do_string_marshal_cache_intern_cstr(cache, str, strlen(str), index, NULL);
}

struct object *string_marshal_cache_intern(struct object *cache, struct object *str, ufixnum_t *index) {
  return do_string_marshal_cache_intern_cstr(cache, STRING_CONTENTS(str), STRING_LENGTH(str), index, str);
}

/*===============================*
 *===============================*
 * Marshaling                    *
 *===============================*
 *===============================*/
struct object *marshal(struct object *o, struct object *ba, struct object *cache);

struct object *marshal_fixnum_t(fixnum_t n, struct object *ba) {
  unsigned char byte;

  if (ba == NULL)
    ba = dynamic_byte_array(4);
  /* the header must be included, because it includes sign information */
  dynamic_byte_array_push_char(ba, n < 0 ? marshaled_type_negative_integer : marshaled_type_integer);
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

struct object *marshal_ufixnum_t(ufixnum_t n, struct object *ba, char include_header) {
  unsigned char byte;
  if (ba == NULL)
    ba = dynamic_byte_array(4);
  /* the header is optional, because the code that unmarshals might already know that the number must be positive (e.g. string lengths). */
  if (include_header) dynamic_byte_array_push_char(ba, marshaled_type_integer);
  do {
    byte = n & 0x7F;
    if (n > 0x7F) /* flip the continuation bit if there are more bytes */
      byte |= 0x80;
    dynamic_byte_array_push_char(ba, byte);
    n >>= 7;
  } while (n > 0);
  return ba;
}

struct object *marshal_string(struct object *str, struct object *ba, char include_header, struct object *cache) {
  ufixnum_t i;
  OT("marshal_string", 0, str, type_string);
  if (ba == NULL)
    ba = dynamic_byte_array(10);
  if (include_header)
    dynamic_byte_array_push_char(ba, marshaled_type_string);
  if (cache == NULL) { /* if not using a cache, use the string directly */
    marshal_ufixnum_t(STRING_LENGTH(str), ba, 0);
    dynamic_byte_array_push_all(ba, str);
  } else { /* otherwise, just the cache index value */
    string_marshal_cache_intern(cache, str, &i);
    marshal_ufixnum_t(i, ba, 0);
  }
  return ba;
}

/** there is no option to disable to header, because the header contains important information
    (if the symbol has a home package or not) */
struct object *marshal_symbol(struct object *sym, struct object *ba, struct object *cache) {
  OT("marshal_symbol", 0, sym, type_symbol);
  if (ba == NULL)
    ba = dynamic_byte_array(10);
  if (SYMBOL_PACKAGE(sym) == NIL) {
    dynamic_byte_array_push_char(ba, marshaled_type_uninterned_symbol);
  } else {
    dynamic_byte_array_push_char(ba, marshaled_type_symbol);
    marshal_string(PACKAGE_NAME(SYMBOL_PACKAGE(sym)), ba, 0, cache);
  }
  marshal_string(SYMBOL_NAME(sym), ba, 0, cache);
  return ba;
}

struct object *marshal_dynamic_byte_array(struct object *ba0, struct object *ba, char include_header) {
  OT("marshal_dynamic_byte_array", 0, ba0, type_dynamic_byte_array);
  if (ba == NULL)
    ba = dynamic_byte_array(10);
  if (include_header)
    dynamic_byte_array_push_char(ba, marshaled_type_dynamic_byte_array);
  marshal_ufixnum_t(DYNAMIC_BYTE_ARRAY_LENGTH(ba0), ba, 0);
  dynamic_byte_array_push_all(ba, ba0);
  return ba;
}

struct object *marshal_nil(struct object *ba) {
  if (ba == NULL)
    ba = dynamic_byte_array(1);
  dynamic_byte_array_push_char(ba, marshaled_type_nil);
  return ba;
}

struct object *marshal_cons(struct object *o, struct object *ba, char include_header, struct object *cache) {
  OT("marshal_cons", 0, o, type_cons);
  if (ba == NULL)
    ba = dynamic_byte_array(10);
  if (include_header)
    dynamic_byte_array_push_char(ba, marshaled_type_cons);
  marshal(CONS_CAR(o), ba, cache);
  marshal(CONS_CDR(o), ba, cache);
  return ba;
}

struct object *marshal_dynamic_array(struct object *arr, struct object *ba, char include_header, struct object *cache) {
  ufixnum_t arr_length, i;
  struct object **c_arr;
  OT("marshal_dynamic_array", 0, arr, type_dynamic_array);
  if (ba == NULL)
    ba = dynamic_byte_array(10);
  if (include_header)
    dynamic_byte_array_push_char(ba, marshaled_type_dynamic_array);
  arr_length = DYNAMIC_ARRAY_LENGTH(arr);
  c_arr = DYNAMIC_ARRAY_VALUES(arr);
  marshal_ufixnum_t(arr_length, ba, 0);
  for (i = 0; i < arr_length; ++i) marshal(c_arr[i], ba, cache);
  return ba;
}

struct object *marshal_dynamic_string_array(struct object *arr, struct object *ba, char include_header, struct object *cache, ufixnum_t start_index) {
  OT("marshal_dynamic_string_array", 0, arr, type_dynamic_array);
  if (ba == NULL)
    ba = dynamic_byte_array(10);
  if (include_header)
    dynamic_byte_array_push_char(ba, marshaled_type_dynamic_string_array);
  marshal_ufixnum_t(DYNAMIC_ARRAY_LENGTH(arr) - start_index, ba, 0);
  for (; start_index < DYNAMIC_ARRAY_LENGTH(arr); ++start_index) marshal_string(DYNAMIC_ARRAY_VALUES(arr)[start_index], ba, 0, cache);
  return ba;
}

/**
 * The format used for marshaling fixnums is platform independent
 * it has the following form (each is one byte):
 *     0x04 <sign> <continuation_bit=1|7bits> <continuation_bit=1|7bits> etc...
 * <continutation_bit=0|7bits> a sign of 1 means it is a negative number, a sign
 * of 0 means positive number the continuation bit of 1 means the next byte is
 * still part of the number once the continuation bit of 0 is reached, the
 * number is complete
 *
 * I'm not sure if there is any advantage between using sign magnitude vs 1 or
 * 2s compliment for the marshaling format besides the memory cost of the sign
 * byte (1 byte per number).
 */
struct object *marshal_fixnum(struct object *n, struct object *ba) {
  OT("marshal_fixnum", 0, n, type_fixnum);
  return marshal_fixnum_t(FIXNUM_VALUE(n), ba);
}

struct object *marshal_ufixnum(struct object *n, struct object *ba, char include_header) {
  OT("marshal_ufixnum", 0, n, type_ufixnum);
  return marshal_ufixnum_t(UFIXNUM_VALUE(n), ba, include_header);
}

struct object *marshal_16_bit_fix(fixnum_t n, struct object *ba) {
  if (ba == NULL)
    ba = dynamic_byte_array(4);
  dynamic_byte_array_push_char(ba, n >> 8);
  dynamic_byte_array_push_char(ba, n & 0xFF);
  return ba;
}

struct object *marshal_flonum(flonum_t n, struct object *ba) {
  ufixnum_t mantissa_fix;
  flonum_t mantissa;
  int exponent;

  if (ba == NULL)
    ba = dynamic_byte_array(10);
  dynamic_byte_array_push_char(ba, n < 0 ? marshaled_type_negative_float : marshaled_type_float);
  /* Inefficient in both time and space but work short term.
   *
   * The idea was for the marshaled format to be as exact as possible
   * (arbitrary precision), while have the types used in the program be inexact
   * (e.g. float, double). I chose to use the base 2 floating point format
   * (mantissa * 2^exponent = value) using arbitrary long mantissas and
   * exponents. This is probably naive and might not work for NaN or Infinity.
   */
  mantissa = frexp(n, &exponent);
  /* frexp keeps sign information on the mantissa, we convert it to a unsigned
   * fixnum (hence the abs(...)). */
  /* we already have the sign information in the marshaled flonum */
  mantissa = mantissa < 0 ? -mantissa : mantissa;
  mantissa_fix = mantissa * pow(2, DBL_MANT_DIG);
  /* TODO: make it choose between DBL/FLT properly */
  marshal_ufixnum_t(mantissa_fix, ba, 0);
  marshal_16_bit_fix(exponent, ba);
  return ba;
}

/**
 * turns function into a byte-array that can be stored to a file
 */
struct object *marshal_function(struct object *bc, struct object *ba, char include_header, struct object *cache) {
  OT("marshal_function", 0, bc, type_function);
  if (ba == NULL)
    ba = dynamic_byte_array(10);
  if (include_header)
    dynamic_byte_array_push_char(ba, marshaled_type_function);
  marshal_dynamic_array(FUNCTION_CONSTANTS(bc), ba, 0, cache);
  marshal_ufixnum_t(FUNCTION_STACK_SIZE(bc), ba, 0);
  marshal_dynamic_byte_array(FUNCTION_CODE(bc), ba, 0);
  marshal_ufixnum_t(FUNCTION_NARGS(bc), ba, 0);
  marshal_ufixnum_t(FUNCTION_NAME(bc) != NIL, ba, 0);
  if (FUNCTION_NAME(bc) != NIL) 
    marshal_symbol(FUNCTION_NAME(bc), ba, 0);
  return ba;
}

struct object *marshal_vec2(struct object *vec2, struct object *ba, char include_header) {
  OT("marshal_vec2", 0, vec2, type_vec2);
  if (ba == NULL)
    ba = dynamic_byte_array(10);
  if (include_header)
    dynamic_byte_array_push_char(ba, marshaled_type_vec2);
  marshal_flonum(VEC2_X(vec2), ba);
  marshal_flonum(VEC2_Y(vec2), ba);
  return ba;
}

/**
 * Takes any object and returns a byte-array containing the binary
 * representation of the object.
 */
struct object *marshal(struct object *o, struct object *ba, struct object *cache) {
  struct object *t;
  if (ba == NULL) ba = dynamic_byte_array(10);
  if (o == NIL) return marshal_nil(ba);
  t = type_of(o);
  if (t == gis->dynamic_byte_array_type) {
      return marshal_dynamic_byte_array(o, ba, 1);
  } else if (t == gis->dynamic_array_type) {
      return marshal_dynamic_array(o, ba, 1, cache);
  } else if (t == gis->cons_type) {
      return marshal_cons(o, ba, 1, cache);
  } else if (t == gis->vec2_type) {
      return marshal_vec2(o, ba, 1);
  } else if (t == gis->fixnum_type) {
      return marshal_fixnum(o, ba);
  } else if (t == gis->ufixnum_type) {
      return marshal_ufixnum(o, ba, 1);
  } else if (t == gis->flonum_type) {
      return marshal_flonum(FLONUM_VALUE(o), ba);
  } else if (t == gis->string_type) {
      return marshal_string(o, ba, 1, cache);
  } else if (t == gis->symbol_type) {
      return marshal_symbol(o, ba, cache);
  } else if (t == gis->function_type) {
      return marshal_function(o, ba, 1, cache);
  } else {
      printf("BC: cannot marshal type %s.\n", type_name_of_cstr(o));
      PRINT_STACK_TRACE_AND_QUIT();
  }
}

/*===============================*
 *===============================*
 * Unmarshaling                  *
 *===============================*
 *===============================*/
struct object *unmarshal(struct object *s, struct object *cache);

/* these are used for lengths -- this will never be used for numbers used in the code
   those use unmarshal_integer, because it is uncertain if they will fit into a fix/ufix/flo.
   But these are required to fit into a ufixnum on this machine. It would be an error if the number
   doesn't fit into the ufix. 
   
   this always assumes the data was written with include_header=0. */
ufixnum_t unmarshal_ufixnum_t(struct object *s) {
  ufixnum_t n;
  ufixnum_t byte;
  char byte_count;
  s = byte_stream_lift(s);
  OT2("unmarshal_ufixnum_t", 0, s, type_enumerator, type_file);
  byte = byte_stream_read_byte(s);
  n = byte & 0x7F;
  byte_count = 1;
  while (byte & 0x80) {
    byte = byte_stream_read_byte(s);
    n = ((byte & 0x7F) << (7 * byte_count)) | n;
    ++byte_count;
    /* TODO: error if trying to read more bytes than what fits into a ufix */
  }
  return n;
}

fixnum_t unmarshal_16_bit_fix(struct object *s) {
  fixnum_t n;
  s = byte_stream_lift(s);
  OT2("unmarshal_16_bit_fix", 0, s, type_enumerator, type_file);
  n = byte_stream_read_byte(s) << 8;
  n = byte_stream_read_byte(s) | n;
  return n;
}

struct object *unmarshal_integer(struct object *s) {
  unsigned char t, sign, is_flo, is_init_flo;
  ufixnum_t ufix, next_ufix, byte_count,
      byte; /* the byte must be a ufixnum because it is used in operations that
               result in ufixnum */
  fixnum_t fix, next_fix;
  flonum_t flo;

  s = byte_stream_lift(s);
  OT2("unmarshal_integer", 0, s, type_enumerator, type_file);
  t = byte_stream_read_byte(s);
  if (t != marshaled_type_integer && t != marshaled_type_negative_integer) {
    printf(
        "BC: unmarshal expected marshaled_integer or "
        "marsahled_negative_interger type, but "
        "was %d.",
        t);
    PRINT_STACK_TRACE_AND_QUIT();
  }

  sign = t == marshaled_type_negative_integer;

  is_flo = 0;
  is_init_flo = 0;
  ufix = 0;
  fix = 0;
  flo = 0;
  byte_count = 0; /* number of bytes we have read (each byte contains 7 bits of
                     the number) */
  do {
    byte = byte_stream_read_byte(s);

    /* if the number fit into a ufixnum or a fixnum, use a flonum */
    if (is_flo) {
      /* if the fixnum was just exceeded */
      if (!is_init_flo) {
        flo = sign == 1 ? fix : ufix;
        is_init_flo = 1;
      }
      flo += pow(2, 7 * byte_count) * (byte & 0x7F);
    } else {
      if (sign == 1) { /* if the number is negative, it must be a signed fixnum */
        next_fix = fix | (byte & 0x7F) << (7 * byte_count);
        if (next_fix < fix ||
            -next_fix >
                -fix) { /* if overflow occurred, or underflow occurred */
          is_flo = 1;
          --byte_count; /* force the next iteration to process the number as a
                           flonum re-use the same byte, because this one
                           couldn't fit. */
        } else {
          fix = next_fix;
        }
      } else { /* otherwise try to fit it into a unsigned fixnum */
        next_ufix = ufix | ((byte & 0x7F) << (7 * byte_count));
        if (next_ufix < ufix) { /* if overflow occurred */
          is_flo = 1;
          --byte_count; /* force the next iteration to process the number as a
                           flonum re-use the same byte, because this one
                           couldn't fit. */
        } else {
          ufix = next_ufix;
        }
      }
    }
    ++byte_count;
  } while (byte & 0x80);

  if (is_flo) return flonum(sign ? -flo : flo);
  if (sign == 1) {
    return fixnum(sign ? -fix : fix);
  }
  /* if the ufix fits into a fix, use it as a fix */
  if (ufix < INT64_MAX) { /* TODO: define FIXNUM_MAX */
    return fixnum(ufix);
  }
  return ufixnum(ufix);
}

flonum_t unmarshal_float_t(struct object *s) {
  unsigned char t;
  fixnum_t exponent;
  flonum_t flo, mantissa;
  ufixnum_t mantissa_fix;
  s = byte_stream_lift(s);
  OT2("unmarshal_float", 0, s, type_file, type_enumerator);
  t = byte_stream_read_byte(s);
  if (t != marshaled_type_float && t != marshaled_type_negative_float) {
    printf("BC: unmarshal expected float type, but was %d.", t);
    PRINT_STACK_TRACE_AND_QUIT();
  }
  mantissa_fix = unmarshal_ufixnum_t(s);
  mantissa = (flonum_t)mantissa_fix / pow(2, DBL_MANT_DIG);
  /* TODO: make it choose between DBL/FLT properly */
  exponent = unmarshal_16_bit_fix(s);
  flo = ldexp(mantissa, exponent);
  /* the sign could also be embedded in the "exponent" part -- use one bit for
     it causing the exponent to use 15 bits. But this won't actually save space
     because all floats current need the header anyway, so may as well use the
     header for signing information (it is easier to code that). */
  return t == marshaled_type_negative_float ? -flo : flo;
}

struct object *unmarshal_float(struct object *s) {
  return flonum(unmarshal_float_t(s));
}

/* clone_from_cache -- if cache is not NULL, this parameter is used. If clone_from_cache=1, when
   the string is taken from the cache, it will immediately clone it (intended for strings that can be
   mutated). */
struct object *unmarshal_string(struct object *s, char includes_header, struct object *cache, char clone_from_cache) {
  unsigned char t;
  struct object *str;
  ufixnum_t length;
  s = byte_stream_lift(s);
  OT2("unmarshal_string", 0, s, type_file, type_enumerator);
  if (includes_header) {
    t = byte_stream_read_byte(s);
    if (t != marshaled_type_string) {
      printf("BC: unmarshal expected string type, but was %d.", t);
      PRINT_STACK_TRACE_AND_QUIT();
    }
  }
  if (cache == NULL) {
    length = unmarshal_ufixnum_t(s);
    str = byte_stream_read(s, length);
    OBJECT_TYPE(str) = type_string;
  } else {
    length = unmarshal_ufixnum_t(s); /* this is the index in the cache where the string is found */
    return clone_from_cache ? string_clone(DYNAMIC_ARRAY_VALUES(cache)[length]) : DYNAMIC_ARRAY_VALUES(cache)[length];
  }
  return str;
}

struct object *unmarshal_symbol(struct object *s, struct object *cache) {
  unsigned char t;
  struct object *symbol_name, *package_name;
  s = byte_stream_lift(s);
  OT2("unmarshal_symbol", 0, s, type_file, type_enumerator);
  t = byte_stream_read_byte(s);
  if (t != marshaled_type_symbol && t != marshaled_type_uninterned_symbol) {
    printf("BC: unmarshal expected symbol or uninterned symbol type, but was %d.", t);
    PRINT_STACK_TRACE_AND_QUIT();
  }
  if (t == marshaled_type_symbol) {
    package_name = unmarshal_string(s, 0, cache, 0); /* clone_from_cache is 0 because modifying symbol names is not allowed */
    symbol_name = unmarshal_string(s, 0, cache, 0);
    return intern(symbol_name, find_package(package_name));
  } else {
    symbol_name = unmarshal_string(s, 0, cache, 0);
    return symbol(symbol_name);
  }
}

struct object *unmarshal_cons(struct object *s, char includes_header, struct object *cache) {
  unsigned char t;
  struct object *car, *cdr;
  s = byte_stream_lift(s);
  if (includes_header) {
    t = byte_stream_read_byte(s);
    if (t != marshaled_type_cons) {
      printf("BC: unmarshaling cons expected cons type, but was %d.", t);
      PRINT_STACK_TRACE_AND_QUIT();
    }
  }
  car = unmarshal(s, cache);
  cdr = unmarshal(s, cache);
  return cons(car, cdr);
}

struct object *unmarshal_dynamic_byte_array(struct object *s, char includes_header) {
  unsigned char t;
  ufixnum_t length;
  s = byte_stream_lift(s);
  if (includes_header) {
    t = byte_stream_read_byte(s);
    if (t != marshaled_type_dynamic_byte_array) {
      printf(
          "BC: unmarshaling dynamic-byte-array expected dynamic-byte-array "
          "type, "
          "but was %d.",
          t);
      PRINT_STACK_TRACE_AND_QUIT();
    }
  }
  length = unmarshal_ufixnum_t(s);
  return byte_stream_read(s, length);
}

struct object *unmarshal_dynamic_array(struct object *s, char includes_header, struct object *cache) {
  unsigned char t;
  struct object *darr;
  ufixnum_t length;
  s = byte_stream_lift(s);
  if (includes_header) {
    t = byte_stream_read_byte(s);
    if (t != marshaled_type_dynamic_array) {
      printf(
          "BC: unmarshal expected dynamic-array type, but was "
          "%d.",
          t);
      PRINT_STACK_TRACE_AND_QUIT();
    }
  }
  length = unmarshal_ufixnum_t(s);
  darr = dynamic_array(length);
  /* unmarshal all items: */
  while (length-- > 0) dynamic_array_push(darr, unmarshal(s, cache));
  return darr;
}

/* if given an existing dynamic array, it will push all items to that. otherwise makes a new one. */
struct object *unmarshal_dynamic_string_array(struct object *s, char includes_header, struct object *cache, struct object *darr) {
  unsigned char t;
  ufixnum_t length;
  s = byte_stream_lift(s);
  if (includes_header) {
    t = byte_stream_read_byte(s);
    if (t != marshaled_type_dynamic_string_array) {
      printf(
          "BC: unmarshal expected dynamic-string-array type, but was "
          "%d.",
          t);
      PRINT_STACK_TRACE_AND_QUIT();
    }
  }
  length = unmarshal_ufixnum_t(s);
  if (darr == NULL)
    darr = dynamic_array(length);
  while (length-- > 0) dynamic_array_push(darr, unmarshal_string(s, 0, cache, 0));
  return darr;
}

struct object *unmarshal_vec2(struct object *s, char includes_header) {
  unsigned char t;
  flonum_t x, y;
  s = byte_stream_lift(s);
  if (includes_header) {
    t = byte_stream_read_byte(s);
    if (t != marshaled_type_vec2) {
      printf("BC: unmarshal expected vec2 type, but was %d.",
             t);
      PRINT_STACK_TRACE_AND_QUIT();
    }
  }
  x = unmarshal_float_t(s);
  y = unmarshal_float_t(s);
  return vec2(x, y);
}

struct object *unmarshal_function(struct object *s, char includes_header, struct object *cache) {
  unsigned char t;
  struct object *constants, *code, *f;
  ufixnum_t stack_size;
  s = byte_stream_lift(s);
  if (includes_header) {
    t = byte_stream_read_byte(s);
    if (t != marshaled_type_function) {
      printf("BC: unmarshaling function expected function type, but was %d.",
             t);
      PRINT_STACK_TRACE_AND_QUIT();
    }
  }
  constants = unmarshal_dynamic_array(s, 0, cache);
  stack_size = unmarshal_ufixnum_t(s);
  code = unmarshal_dynamic_byte_array(s, 0);
  f = function(constants, code, stack_size);
  FUNCTION_NARGS(f) = unmarshal_ufixnum_t(s);
  if (unmarshal_ufixnum_t(s)) {
    FUNCTION_NAME(f) = unmarshal_symbol(s, cache);
  }
  return f;
}

struct object *unmarshal_nil(struct object *s) {
  unsigned char t;
  s = byte_stream_lift(s);
  t = byte_stream_read_byte(s);
  if (t != marshaled_type_nil) {
    printf("BC: unmarshaling nil expected nil type, but was %d.", t);
    PRINT_STACK_TRACE_AND_QUIT();
  }
  return NIL;
}

struct object *unmarshal(struct object *s, struct object *cache) {
  enum marshaled_type t;

  s = byte_stream_lift(s);
  t = byte_stream_peek_byte(s);

  switch (t) {
    case marshaled_type_dynamic_byte_array:
      return unmarshal_dynamic_byte_array(s, 1);
    case marshaled_type_dynamic_array:
      return unmarshal_dynamic_array(s, 1, cache);
    case marshaled_type_cons:
      return unmarshal_cons(s, 1, cache);
    case marshaled_type_nil:
      return unmarshal_nil(s);
    case marshaled_type_integer:
    case marshaled_type_negative_integer:
      return unmarshal_integer(s);
    case marshaled_type_float:
    case marshaled_type_negative_float:
      return unmarshal_float(s);
    case marshaled_type_string:
      return unmarshal_string(s, 1, cache, 1);
    case marshaled_type_vec2:
      return unmarshal_vec2(s, 1);
    case marshaled_type_symbol:
      return unmarshal_symbol(s, cache);
    case marshaled_type_function:
      return unmarshal_function(s, 1, cache);
    default:
      printf("BC: cannot unmarshal marshaled type %d.", t);
      return NIL;
  }
}

/*===============================*
 *===============================*
 * Bytecode File Formatting      *
 *===============================*
 *===============================*/
struct object *make_bytecode_file_header() {
  struct object *ba = dynamic_byte_array(10);
  dynamic_byte_array_push_char(ba, 'b');
  dynamic_byte_array_push_char(ba, 'u');
  dynamic_byte_array_push_char(ba, 'g');
  marshal_ufixnum_t(BC_VERSION, ba, 0);
  return ba;
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

/**
 * Writes bytecode to a file
 * @param bc the bytecode to write
 */
 void write_bytecode_file(struct object *file, struct object *bc) {
  struct object *cache, *ba;
  ufixnum_t user_cache_start_index;
  OT("write_bytecode_file", 0, file, type_file);
  OT("write_bytecode_file", 1, bc, type_function);
  write_file(file, make_bytecode_file_header());
  cache = string_marshal_cache_get_default();
  user_cache_start_index = DYNAMIC_ARRAY_LENGTH(cache);
  ba = marshal_function(bc, NULL, 0, cache);
  write_file(file, marshal_dynamic_string_array(cache, NULL, 0, NULL, user_cache_start_index));
  write_file(file, ba);
}

struct object *read_bytecode_file(struct object *s) {
  struct object *bc, *cache;
  ufixnum_t version;

  s = byte_stream_lift(s);

  if (byte_stream_read_byte(s) != 'b' || byte_stream_read_byte(s) != 'u' ||
      byte_stream_read_byte(s) != 'g') {
    printf("BC: Invalid magic string\n");
    PRINT_STACK_TRACE_AND_QUIT();
  }

  version = unmarshal_ufixnum_t(s);
  if (version != BC_VERSION) {
    printf(
        "BC: Version mismatch (this interpreter has version %d, the file has "
        "version %u).\n",
        BC_VERSION, (unsigned int)version);
    PRINT_STACK_TRACE_AND_QUIT();
  }

  /* load cache with defaults, then fill with additional from file */
  cache = string_marshal_cache_get_default();
  unmarshal_dynamic_string_array(s, 0, NULL, cache);

  bc = unmarshal_function(s, 0, cache);

  if (type_of(bc) != gis->function_type) {
    printf(
        "BC: function file requires a marshalled function object immediately "
        "after the header, but found a %s.\n",
        type_name_of_cstr(bc));
    PRINT_STACK_TRACE_AND_QUIT();
  }
  return bc;
}

/*===============================*
 *===============================*
 * Read                          *
 *===============================*
 *===============================*/
char is_digit(char c) {
  return c >= '0' && c <= '9';
}

char is_whitespace(char c) {
  return c == ' ' || c == '\n' || c == '\r' || c == '\t';
}

char is_priority_character(char c) {
  return c == '"' || c == ')' || c == '\'';
}

char skip_whitespace(struct object *s) {
  char c;
  while (byte_stream_has(s) && is_whitespace(c = byte_stream_peek_byte(s)))
    byte_stream_read_byte(s); /* throw away the whitespace */
  return c;
}

/* "(1 2 3)" -> (cons 1 (cons 2 (cons 3 nil))) */
/** s -> the stream to read from
 *  package -> the package symbols should be interned into
 *  g -> the global interpreter state
*/
struct object *read(struct object *s, struct object *package) {
  char c;
  struct object *buf, *sexpr, *package_name;

  /* for checking if is numeric */
  char is_numeric, is_flo, has_mantissa, has_e, sign, exponent_sign;
  fixnum_t fix;
  flonum_t flo;
  ufixnum_t byte_count;
  ufixnum_t i;
  char escape_next;
  struct object *integral_part;
  struct object *exponent_part;
  struct object *mantissa_part;

  char is_internal; /* if we are looking for an internal symbol if is_numeric = false */

  is_internal = 0;

  if (package == NIL)
    package = GIS_PACKAGE;

  package_name = NIL;

  s = byte_stream_lift(s);

  if (!byte_stream_has(s)) {
    printf("Read was given empty input.");
    PRINT_STACK_TRACE_AND_QUIT();
  }

  c = skip_whitespace(s);
  if (!byte_stream_has(s)) {
    printf("Read was given empty input (only contained whitespace).");
    PRINT_STACK_TRACE_AND_QUIT();
  }

  if (c == '"') { /* beginning of string */
    byte_stream_read_byte(s); /* throw the quotation mark away */
    buf = dynamic_byte_array(10);
    while ((c = byte_stream_read_byte(s)) != '"' || escape_next) {
      if (!byte_stream_has(s)) {
        printf("Unexpected end of input during read of string.");
        PRINT_STACK_TRACE_AND_QUIT();
      }
      if (escape_next) {
        if (c == '\\' || c == '"') {
          dynamic_byte_array_push_char(buf, c);
        } else if (c == 'n') {
          dynamic_byte_array_push_char(buf, '\n');
        } else if (c == 'r') {
          dynamic_byte_array_push_char(buf, '\r');
        } else if (c == 't') {
          dynamic_byte_array_push_char(buf, '\t');
        } else {
          printf("Invalid escape sequence \"\\%c\".", c);
          PRINT_STACK_TRACE_AND_QUIT();
        }
        escape_next = 0;
      } else if (c == '\\') {
        escape_next = 1;
      } else {
        dynamic_byte_array_push_char(buf, c);
      }
    }
    if (escape_next) {
      printf("Expected escape sequence, but string ended.");
      PRINT_STACK_TRACE_AND_QUIT();
    }
    OBJECT_TYPE(buf) = type_string;
    return buf;
  } else if (c == '(') { /* beginning of sexpr */
    buf = dynamic_array(10);
    sexpr = NIL;
    byte_stream_read_byte(s); /* throw away the opening paren */
    c = skip_whitespace(s);
    while (byte_stream_has(s) && (c = byte_stream_peek_byte(s)) != ')') {
      dynamic_array_push(buf, read(s, package));
      c = skip_whitespace(s);
    }
    if (!byte_stream_has(s)) {
      printf("Unexpected end of input during sexpr.");
      PRINT_STACK_TRACE_AND_QUIT();
    }
    byte_stream_read_byte(s); /* throw away the closing paren */
    for (fix = DYNAMIC_ARRAY_LENGTH(buf) - 1; fix >= 0; --fix) sexpr = cons(DYNAMIC_ARRAY_VALUES(buf)[fix], sexpr);
    return sexpr;
  } else if (c == ':') { /* keyword */
    byte_stream_read_byte(s); /* throw away the colon */
    return read(s, gis->keyword_package);
  } else if (c == '\'') { /* quoted expression */
    byte_stream_read_byte(s); /* throw away the quote */
    return cons(gis->lisp_quote_sym, cons(read(s, package), NIL));
  } else if (c == '`') { /* quasiquoted expression */
    byte_stream_read_byte(s); /* throw away the quasiquote */
    return cons(gis->lisp_quasiquote_sym, cons(read(s, package), NIL));
  } else if (c == ',') { /* unquote expression */
    byte_stream_read_byte(s); /* throw away the ',' */
    if (byte_stream_peek_byte(s) == '@') { /* pretty bad -- doesn't let you do whitespace between... but this is fine for now */
      byte_stream_read_byte(s); /* throw away the '@' */
      return cons(gis->lisp_unquote_splicing_sym, cons(read(s, package), NIL));
    }
    return cons(gis->lisp_unquote_sym, cons(read(s, package), NIL));
  } else { /* either a number or a symbol */
    buf = dynamic_byte_array(10);
    is_numeric = 1; /* assume it is numeric unless proven otherwise */
    has_mantissa = 0; /* this is flipped to true when a period is found */
    is_flo = 0; /* assume the number is a fixnum unless proven otherwise */
    has_e = 0; /* indicates if we are in the exponent section of a flonum (e.g. 1e9) */
    sign = 0; /* assume the number is positive unless proven otherwise */
    exponent_sign = 0;
    fix = 0;
    flo = 0;
    byte_count = 0;
    while (byte_stream_has(s) && !is_whitespace(c = byte_stream_peek_byte(s))) {
      if (is_priority_character(c)) {
        /* these characters are special and can break the flow of an identifier or number */
        break;
      }
      if (is_numeric) { /* if we still believe we are parsing a number */
        if (byte_count == 0 && c == '+') { /* TODO: allow for signs in exponent part of flonum */
          /* pass */
        } else if (byte_count == 0 && c == '-') {
          sign = 1;
        } else if (has_e && STRING_LENGTH(exponent_part) == 0 && c == '-') {
          exponent_sign = 1;
        } else if (has_e && STRING_LENGTH(exponent_part) == 0 && c == '+') {
          /* pass */
        } else if (!is_digit(c)) { /* if this isn't a digit, then either it is 'e' or '.' (so long as we haven't alerady seen those). otherwise it isn't a number at all */
          if (c == '.') {
            if (has_e) {
              is_numeric = 0; /* no decimal numbers for exponents (e.g. 1e3.4) */
            } else if (has_mantissa == 0) {
              has_mantissa = is_flo = 1;
              integral_part = dynamic_byte_array_concat(buf, string(""));
              OBJECT_TYPE(integral_part) = type_string;
              exponent_part = string("");
              mantissa_part = string("");
            } else {
              is_numeric = 0; /* this isn't a number -- continue as a symbol */
            }
          } else if (c == 'e') {
            if (has_e == 0) {
              has_e = is_flo = 1;
              exponent_part = string("");
              if (!has_mantissa) {
                integral_part = dynamic_byte_array_concat(buf, string(""));
                OBJECT_TYPE(integral_part) = type_string;
                mantissa_part = string("");
              }
            } else {
              is_numeric = 0; /* this isn't a number -- continue as a symbol */
            }
          } else {
            is_numeric = 0; /* this isn't a number -- continue as a symbol */
          }
        } else {
          if (is_flo) {
            if (has_e) {
              dynamic_byte_array_push_char(exponent_part, c);
            } else if (has_mantissa) {
              dynamic_byte_array_push_char(mantissa_part, c);
            }
          }
        }
      }
      if (!is_numeric) {
        /* if we have not already read a package name, interpret ':' as a
         * keyword */
        if (c == ':') {
          if (package_name !=
              NIL) { /* we have already read a colon, this is invalid! */
            printf("Too many colons in symbol\n");
            PRINT_STACK_TRACE_AND_QUIT();
          }
          ++byte_count;
          byte_stream_read_byte(s); /* throw away the colon */
          if (byte_stream_peek_byte(s) == ':') {
            is_internal = 1;
            /* trying to read an internal symbol */
            ++byte_count;
            byte_stream_read_byte(s); /* throw away the second colon */
          }
          package_name = buf;
          buf = dynamic_byte_array(10);
          continue;
        }
      }
      dynamic_byte_array_push_char(buf, c);
      ++byte_count;
      byte_stream_read_byte(s); /* throw this byte away */
    }
    /* check for dot, plus, minus -- don't mistake a lone dot for a float when it should really be a symbol */
    if (STRING_LENGTH(buf) == 1 && (STRING_CONTENTS(buf)[0] == '.' || STRING_CONTENTS(buf)[0] == '+' || STRING_CONTENTS(buf)[0] == '-' || STRING_CONTENTS(buf)[0] == 'e'))
      is_numeric = 0;
    if (is_numeric) {
      if (is_flo) {
        /* error prone way of converting to convert string to float 
           bad because each multiplication introduces more and more error */
        for (i = 0; i < STRING_LENGTH(integral_part); ++i)
          flo += (STRING_CONTENTS(integral_part)[i] - '0') * pow(10, STRING_LENGTH(integral_part) - i - 1);
        for (i = 0; i < STRING_LENGTH(mantissa_part); ++i) {
          flo += (STRING_CONTENTS(mantissa_part)[i] - '0') * pow(10, -((fixnum_t)i) - 1);
        }
        if (STRING_LENGTH(exponent_part) > 0) {
          for (i = 0; i < STRING_LENGTH(exponent_part); ++i)
            fix += (STRING_CONTENTS(exponent_part)[i] - '0') *
                   pow(10, STRING_LENGTH(exponent_part) - i - 1);
          flo = (exponent_sign ? 1/pow(10, fix) : pow(10, fix)) * flo;
        }
        return flonum(sign ? -flo : flo);
      } else {
        for (i = 0; i < DYNAMIC_BYTE_ARRAY_LENGTH(buf); ++i)
          fix += (DYNAMIC_BYTE_ARRAY_BYTES(buf)[i] - '0') * pow(10, DYNAMIC_BYTE_ARRAY_LENGTH(buf) - i - 1);
        return fixnum(sign ? -fix : fix);
      }
    }
    OBJECT_TYPE(buf) = type_string;
    if (package_name != NIL) { /* if the symbol has a package specifier in it, use that one */
      OBJECT_TYPE(package_name) = type_string;
      package = find_package(package_name);
      if (package == NIL) {
        printf("There's no such package \"%s\".", bstring_to_cstring(package_name));
        PRINT_STACK_TRACE_AND_QUIT();
      }
      if (is_internal && package != GIS_PACKAGE) { /* if this is an internal keyword (written as package::symbol-name, and "package" is not the current one) */
        /* if the "package" is the current package, the normal process will continue (it will be interned) */
        sexpr = find_symbol(buf, package, 1);
        if (sexpr == NIL) { /* using sexpr as a temp var  :| */
          printf("Package \"%s\" has no symbol named \"%s\".\n", bstring_to_cstring(package_name), bstring_to_cstring(buf));
          PRINT_STACK_TRACE_AND_QUIT();
        }
        return sexpr;
      } else if (!is_internal) {
        sexpr = find_symbol(buf, package, 1);
        if (sexpr == NIL) { /* using sexpr as a temp var  :| */
          printf("Package \"%s\" has no external symbol named \"%s\".\n",
                 bstring_to_cstring(package_name), bstring_to_cstring(buf));
          PRINT_STACK_TRACE_AND_QUIT();
        }
        return sexpr;
      }
    }
    return intern(buf, package);
  }
  return NIL;
}

/*===============================*
 *===============================*
 * Compile                       *
 *===============================*
 *===============================*/

void gen_load_constant(struct object *bc, struct object *value) {
  ufixnum_t i;
  OT("gen_load_constant", 0, bc, type_function);
  dynamic_array_push(FUNCTION_CONSTANTS(bc), value);
  i = DYNAMIC_ARRAY_LENGTH(FUNCTION_CONSTANTS(bc)) - 1;
  switch (i) {
    case 0:
      dynamic_byte_array_push_char(FUNCTION_CODE(bc), op_const_0);
      break;
    case 1:
      dynamic_byte_array_push_char(FUNCTION_CODE(bc), op_const_1);
      break;
    case 2:
      dynamic_byte_array_push_char(FUNCTION_CODE(bc), op_const_2);
      break;
    case 3:
      dynamic_byte_array_push_char(FUNCTION_CODE(bc), op_const_3);
      break;
    default:
      dynamic_byte_array_push_char(FUNCTION_CODE(bc), op_const);
      /* bugs could show up when the number of constants exceeds 127 (will make
         this be > 1 byte). (if jump has issues) */
      marshal_ufixnum_t(DYNAMIC_ARRAY_LENGTH(FUNCTION_CONSTANTS(bc)) - 1,
                        FUNCTION_CODE(bc), 0);
  }
}

struct object *run(struct gis *gis);

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

struct object *compile_entire_file(struct object *input_file) {
  struct object *temp, *f;
  temp = NIL;
  while (byte_stream_has(input_file)) {
    temp = cons(read(input_file, GIS_PACKAGE), temp);
  }
  temp = cons_reverse(temp);
  temp = cons(gis->lisp_progn_sym, temp);
  f = compile(temp, NIL, NIL, NIL);
  /* drop the value returned by progn -- otherwise there will be garbage on the stack after compiling every file */
  dynamic_byte_array_push_char(FUNCTION_CODE(f), op_drop);
  return f;
}

/*
 *
 * Examples:
 *  (+ 1 2)
 *  constants=[1 2]
 *  code=
 *   load-constant 0
 *   load-constant 1
 *   add
 */
/*
 * st => symbol value table
 * fst => function symbol table
 */
struct object *compile(struct object *ast, struct object *f, struct object *st, struct object *fst) {
  struct object *value, *car, *cursor, *constants, *code, *tst, *t; /* temp symbol table */
  struct object *name, *params, *body, *k, *v, *kvp, *fun; /** for compiling functions */
  struct object *lhs, *rhs;
  ufixnum_t length, t0, t1, jump_offset, stack_index;

  if (f == NIL) {
    constants = dynamic_array(10);
    code = dynamic_byte_array(10);
    f = function(constants, code, 0); /* 0 stack size is temporary, it will be updated */
  }

/* compiles each item in the cons list, and runs "f" after each is compiled 
 * 
 * Good for compiling things like "print":
 * 
 * (print "a" "b" "c")
 * 
 *    load 0
 *    print
 *    load 1
 *    print
 *    load 2
 *    print
 * 
 * It could also have loaded then printed.
 * 
 */
#define COMPILE_DO_EACH(block)             \
  cursor = CONS_CDR(value);                \
  length = 0;                              \
  while (cursor != NIL) {                  \
    compile(CONS_CAR(cursor), f, st, fst); \
    cursor = CONS_CDR(cursor);             \
    ++length;                              \
    block;                                 \
  }

/* compiles each item in the cons list, starting with the first two, then after each item. 
 * 
 * Good for compiling operators like + and -.
 * 
 * (- 1 2 3 4)
 * 
 *    load 0
 *    load 1
 *    subtract
 *    load 2
 *    subtract
 *    load 3
 *    subtract 
 */
#define COMPILE_DO_AGG_EACH(block)         \
  length = 0;                              \
  cursor = CONS_CDR(value);                \
  while (cursor != NIL) {                  \
    compile(CONS_CAR(cursor), f, st, fst); \
    cursor = CONS_CDR(cursor);             \
    ++length;                              \
    if (length >= 2) {                     \
      block;                               \
    }                                      \
  }

#define C_ARG0() CONS_CAR(CONS_CDR(value))
#define C_ARG1() CONS_CAR(CONS_CDR(CONS_CDR(value)))
#define C_ARG2() CONS_CAR(CONS_CDR(CONS_CDR(CONS_CDR(value))))
#define C_ARG3() CONS_CAR(CONS_CDR(CONS_CDR(CONS_CDR(CONS_CDR(value)))))
#define C_COMPILE(expr) compile(expr, f, st, fst);
#define C_COMPILE_ARG0 C_COMPILE(C_ARG0())
#define C_COMPILE_ARG1 C_COMPILE(C_ARG1())
#define C_COMPILE_ARG2 C_COMPILE(C_ARG2())
#define C_COMPILE_ARG3 C_COMPILE(C_ARG3())

#define C_EXE1(op)    \
  SF_REQ_N(1, value); \
  C_COMPILE_ARG0;     \
  C_PUSH_CODE(op);
#define C_EXE2(op)    \
  SF_REQ_N(2, value); \
  C_COMPILE_ARG0;     \
  C_COMPILE_ARG1;     \
  C_PUSH_CODE(op);

#define C_CODE FUNCTION_CODE(f)
#define C_CONSTANTS FUNCTION_CONSTANTS(f)
#define C_PUSH_CODE(op) dynamic_byte_array_push_char(C_CODE, op);
#define C_PUSH_CONSTANT(constant) dynamic_array_push(C_CONSTANTS, constant);
#define C_CODE_LEN DYNAMIC_BYTE_ARRAY_LENGTH(C_CODE)

  value = ast;
  t = type_of(value);
  if (t == gis->nil_type || t == gis->flonum_type || t == gis->fixnum_type ||
      t == gis->ufixnum_type || t == gis->string_type ||
      t == gis->dynamic_byte_array_type || t == gis->package_type ||
      t == gis->vec2_type || t == gis->enumerator_type) {
    gen_load_constant(f, value);
  } else if (t == gis->symbol_type) {
    if (alist_get_slot(st, value) != NIL) {
      length = UFIXNUM_VALUE(alist_get_value(
          st, value)); /* using length as temp for stack index */
      if (length == 0) {
        C_PUSH_CODE(op_load_from_stack_0);
      } else if (length == 1) {
        C_PUSH_CODE(op_load_from_stack_1);
      } else {
        C_PUSH_CODE(op_load_from_stack);
        marshal_ufixnum_t(length, C_CODE, 0);
      }
    } else {
      /* if the value isn't in the symbol table (not a lexical variable),
       * evaluate the symbol's value at runtime */
      C_PUSH_CONSTANT(value);
      C_PUSH_CODE(op_const);
      marshal_ufixnum_t(DYNAMIC_ARRAY_LENGTH(C_CONSTANTS) - 1, C_CODE, 0);
      C_PUSH_CODE(op_symbol_value);
    }
    /* TODO: add dynamic scoping */
  } else if (t == gis->file_type) {
    printf("A object of type file cannot be compiled.");
    PRINT_STACK_TRACE_AND_QUIT();
  } else if (t == gis->function_type) {
    printf("A object of type function cannot be compiled.");
    PRINT_STACK_TRACE_AND_QUIT();
  } else if (t == gis->cons_type) {
    /* TODO: in SBCL you can say (symbol-function '+) and it gives you a
     * function. that would be cool to be able to do here. */
    car = CONS_CAR(value);
    if (type_of(car) == gis->symbol_type) {
      if (alist_get_value(st, car) !=
          NIL) { /* if the value exists in the symbol table */
      } else {   /* either it is undefined or a special form */
        if (car == gis->lisp_cons_sym) {
          SF_REQ_N(2, value);
          C_COMPILE_ARG0;
          C_COMPILE_ARG1;
          C_PUSH_CODE(op_cons);
        } else if (car == gis->lisp_progn_sym) {
          cursor = CONS_CDR(value);
          while (cursor != NIL) {
            C_COMPILE(CONS_CAR(cursor));
            cursor = CONS_CDR(cursor);
            if (cursor != NIL) C_PUSH_CODE(op_drop);
          }
        } else if (car == gis->impl_drop_sym) {
          C_PUSH_CODE(op_drop);
        } else if (car == gis->lisp_let_sym) {
          /* (let [params] [body]) */
          params = C_ARG0();

          cursor = params;
          tst = st; /* all compilations in the let will use the symbol table
                       from before evaluating the let */
          while (cursor != NIL) {
            kvp = CONS_CAR(cursor);
            /* TODO: validate that there's actually a key and value in the kvp
             */
            k = CONS_CAR(kvp);
            v = CONS_CAR(CONS_CDR(kvp));
            stack_index = FUNCTION_STACK_SIZE(f);
            ++FUNCTION_STACK_SIZE(f);
            compile(v, f, tst, fst);
            C_PUSH_CODE(op_store_to_stack);
            C_PUSH_CODE(stack_index); /* TODO: make this a marshaled ufixnum */
            st = alist_extend(
                st, k, ufixnum(stack_index)); /* add to the symbol table that
                                                 the body of the let will use */
            cursor = CONS_CDR(cursor);
          }

          cursor = CONS_CDR(CONS_CDR(value)); /* the body of the let */
          while (cursor != NIL) {
            C_COMPILE(CONS_CAR(cursor));
            cursor = CONS_CDR(cursor);
          }
        } else if (car == gis->impl_function_sym || car == gis->impl_macro_sym) {
          /* (function [name] [params] body) */
          name = C_ARG0();

          /* check if the name position is actually filled with the params
           * (anonymous function) */
          if (type_of(name) == gis->cons_type || type_of(name) == gis->nil_type) {
            params = name;
            /* set name to NIL as a flag for later */
            name = NIL;
          } else if (type_of(name) != gis->symbol_type) {
            printf("functions must be given a symbol name.");
            PRINT_STACK_TRACE_AND_QUIT();
          } else {
            /* the params list */
            params = C_ARG1();
          }

          cursor = params;
          length = 0;
          tst = st;
          while (cursor != NIL) {
            k = CONS_CAR(cursor);
            /* add to the symbol table that the body of the function will use */
            tst = alist_extend(tst, k, ufixnum(length));
            cursor = CONS_CDR(cursor);
            ++length;
          }

          fun = function(dynamic_array(10), dynamic_byte_array(10), length);
          FUNCTION_NARGS(fun) = length;
          FUNCTION_NAME(fun) = name;

          /* compile each expression in the function and add it to the
           * function's bytecode */
          if (name == NIL) { /* if this is an anonymous function */
            body = CONS_CDR(CONS_CDR(value));
          } else {
            body = CONS_CDR(CONS_CDR(CONS_CDR(value)));
          }
          cursor = body;
          while (cursor != NIL) {
            compile(CONS_CAR(cursor), fun, tst, fst);
            cursor = CONS_CDR(cursor);
            if (cursor != NIL) { /* implicit progn */
              dynamic_byte_array_push_char(
                  FUNCTION_CODE(fun),
                  op_drop); /* the last value will still be on the stack when
                               the function body completes */
            }
          }
          /* add an implicit return to every function */
          dynamic_byte_array_push_char(FUNCTION_CODE(fun), op_return_function);

          /* macros should not be set at runtime (only exist in the symbol table
           * during compilation) */
          if (car == gis->impl_macro_sym) {
            FUNCTION_IS_MACRO(fun) = 1;
            if (name == NIL) {
              printf("Macros must have a name.");
              PRINT_STACK_TRACE_AND_QUIT();
            }
            symbol_set_function(
                name, fun); /* this is key -- we set the symbol function value
                               inside of the compiler */
            gen_load_constant(
                f, NIL); /* should this be done? shouldn't no evidence of the
                            macro be present at runtime? */
          } else if (name == NIL) { /* if this is an anonymous function */
            gen_load_constant(f, fun);
          } else {
            symbol_set_function(
                name, fun); /* this is key to being able to call functions from
                               a macro-- we set the symbol function value inside
                               of the compiler */
            gen_load_constant(f, name);
            gen_load_constant(f, fun);
            C_PUSH_CODE(op_set_symbol_function);
          }
        } else if (car == gis->lisp_symbol_value_sym) {
          C_COMPILE_ARG0;
          C_PUSH_CODE(op_symbol_value);
        } else if (car == gis->lisp_symbol_function_sym) {
          C_COMPILE_ARG0;
          C_PUSH_CODE(op_symbol_function);
        } else if (car == gis->lisp_quote_sym) {
          SF_REQ_N(1, value);
          gen_load_constant(f, C_ARG0());
        } else if (car == gis->lisp_car_sym) {
          C_EXE1(op_car);
        } else if (car == gis->lisp_cdr_sym) {
          C_EXE1(op_cdr);
        } else if (car == gis->lisp_set_sym) {
          /* in CL, SET doesn't work for lexical variables. setq can do this
           * though */
          /* if this is lexical, set the variable on the stack
             otherwise, set the symbol value slot */
          C_EXE2(op_set_symbol_value);
        } else if (car == gis->lisp_set_symbol_function_sym) {
          C_EXE2(op_set_symbol_function);
        } else if (car == gis->lisp_if_sym) {
          /* compile the condition part if the if */
          C_COMPILE_ARG0;
          C_PUSH_CODE(op_jump_when_nil);
          /* dummy arg of zeros for jump -- will be updated below to go to end
           * of if statement */
          C_PUSH_CODE(0);
          C_PUSH_CODE(0);

          t0 = C_CODE_LEN - 1; /* keep the address of the dummy jump value */

          /* compile the "then" part of the if */
          if (CONS_CDR(CONS_CDR(value)) == NIL) {
            printf("\"if\" requires at least 3 arguments was given 1.");
            PRINT_STACK_TRACE_AND_QUIT();
          }
          C_COMPILE_ARG1;

          C_PUSH_CODE(op_jump);
          /* dummy arg of zeros for jump -- will be updated below to go to end
           * of if statement */
          C_PUSH_CODE(0);
          C_PUSH_CODE(0);

          t1 = C_CODE_LEN - 1; /* keep the address of the dummy jump value */

          /* now we know how far the first jump should have been - update it */
          jump_offset = C_CODE_LEN - t0;
          if (jump_offset > 32767) { /* signed 16-bit number */
            printf(
                "\"then\" part of if special form exceeded maximum jump "
                "range.");
            PRINT_STACK_TRACE_AND_QUIT();
          }
          dynamic_byte_array_set(C_CODE, t0 - 1, jump_offset << 8);
          dynamic_byte_array_set(C_CODE, t0, jump_offset & 0xFF);

          /* compile the "else" part of the if */
          cursor = CONS_CDR(CONS_CDR(CONS_CDR(value)));
          while (cursor != NIL) {
            C_COMPILE(CONS_CAR(cursor));
            cursor = CONS_CDR(cursor);
            if (cursor != NIL) C_PUSH_CODE(op_drop);
          }

          /* now we know how far the first jump should have been - update it */
          jump_offset = C_CODE_LEN - t1;
          if (jump_offset > 32767) {
            printf(
                "\"else\" part of if special form exceeded maximum jump "
                "range.");
            PRINT_STACK_TRACE_AND_QUIT();
          }
          dynamic_byte_array_set(C_CODE, t1 - 1, jump_offset << 8);
          dynamic_byte_array_set(C_CODE, t1, jump_offset & 0xFF);

        } else if (car == gis->lisp_print_sym) {
          COMPILE_DO_EACH({ C_PUSH_CODE(op_print); });
          C_PUSH_CODE(op_print_nl);
          C_PUSH_CODE(op_load_nil);
        } else if (car == gis->impl_list_sym) {
          COMPILE_DO_EACH({});
          C_PUSH_CODE(op_list);
          marshal_ufixnum_t(length, C_CODE, 0);
        } else if (car == gis->impl_call_sym) {
          cursor = CONS_CDR(CONS_CDR(value));
          length = 0;
          while (cursor != NIL) {
            C_COMPILE(CONS_CAR(cursor));
            cursor = CONS_CDR(cursor);
            ++length;
          }

          C_COMPILE_ARG0;

          C_PUSH_CODE(op_call_function);
          marshal_ufixnum_t(length, C_CODE, 0);
        } else if (car == gis->impl_add_sym) {
          length = 0;
          cursor = CONS_CDR(value);
          lhs = NULL;
          rhs = NULL;
          while (cursor != NIL) {
            if (lhs == NULL) {
              lhs = CONS_CAR(cursor);
            } else {
              rhs = CONS_CAR(cursor);
            }
            cursor = CONS_CDR(cursor);
            ++length;
            if (length > 2) {
              if (type_of(rhs) == gis->fixnum_type || type_of(rhs) == gis->ufixnum_type) {
                if (FIXNUM_VALUE(rhs) < 0) {
                  C_PUSH_CODE(op_subi);
                } else {
                  C_PUSH_CODE(op_addi);
                }
                marshal_ufixnum_t(FIXNUM_VALUE(rhs) < 0 ? -FIXNUM_VALUE(rhs)
                                                        : FIXNUM_VALUE(rhs),
                                  C_CODE, 0);
              } else {
                C_COMPILE(rhs);
                C_PUSH_CODE(op_add);
              }
            } else if (length == 2) {
              if (type_of(lhs) == gis->fixnum_type || type_of(lhs) == gis->ufixnum_type) {
                if (type_of(rhs) == gis->fixnum_type ||
                    type_of(rhs) == gis->ufixnum_type) {
                  /* adding two numbers -- constant fold them */
                  C_COMPILE(fixnum(FIXNUM_VALUE(lhs) + FIXNUM_VALUE(rhs)));
                } else {
                  /* lhs is constant but rhs is not */
                  C_COMPILE(rhs);
                  if (FIXNUM_VALUE(lhs) < 0) {
                    C_PUSH_CODE(op_subi);
                  } else {
                    C_PUSH_CODE(op_addi);
                  }
                  marshal_ufixnum_t(FIXNUM_VALUE(lhs) < 0 ? -FIXNUM_VALUE(lhs)
                                                          : FIXNUM_VALUE(lhs),
                                    C_CODE, 0);
                }
              } else {
                if (type_of(rhs) == gis->fixnum_type ||
                    type_of(rhs) == gis->ufixnum_type) {
                  /* rhs is number but lhs is not */
                  C_COMPILE(lhs);
                  if (FIXNUM_VALUE(rhs) < 0) {
                    C_PUSH_CODE(op_subi);
                  } else {
                    C_PUSH_CODE(op_addi);
                  }
                  marshal_ufixnum_t(FIXNUM_VALUE(rhs) < 0 ? -FIXNUM_VALUE(rhs)
                                                          : FIXNUM_VALUE(rhs),
                                    C_CODE, 0);
                } else { /* both are not constant numbers */
                  C_COMPILE(lhs);
                  C_COMPILE(rhs);
                  C_PUSH_CODE(op_add);
                }
              }
            }
          }
        } else if (car == gis->lisp_sub_sym) {
          length = 0;
          cursor = CONS_CDR(value);
          lhs = NULL;
          rhs = NULL;
          while (cursor != NIL) {
            if (lhs == NULL) {
              lhs = CONS_CAR(cursor);
            } else {
              rhs = CONS_CAR(cursor);
            }
            cursor = CONS_CDR(cursor);
            ++length;
            if (length > 2) {
              if (type_of(rhs) == gis->fixnum_type) {
                if (FIXNUM_VALUE(rhs) < 0) {
                  C_PUSH_CODE(op_addi);
                } else {
                  C_PUSH_CODE(op_subi);
                }
                marshal_ufixnum_t(FIXNUM_VALUE(rhs) < 0 ? -FIXNUM_VALUE(rhs)
                                                        : FIXNUM_VALUE(rhs),
                                  C_CODE, 0);
              } else {
                C_COMPILE(rhs);
                C_PUSH_CODE(op_sub);
              }
            } else if (length == 2) {
              if (type_of(lhs) == gis->fixnum_type) {
                if (type_of(rhs) == gis->ufixnum_type) {
                  /* adding two numbers -- constant fold them */
                  C_COMPILE(fixnum(FIXNUM_VALUE(lhs) - FIXNUM_VALUE(rhs)));
                } else {
                  /* lhs is constant but rhs is not */
                  C_COMPILE(lhs);
                  if (FIXNUM_VALUE(rhs) < 0) {
                    C_PUSH_CODE(op_addi);
                  } else {
                    C_PUSH_CODE(op_subi);
                  }
                  marshal_ufixnum_t(FIXNUM_VALUE(rhs) < 0 ? -FIXNUM_VALUE(rhs)
                                                          : FIXNUM_VALUE(rhs),
                                    C_CODE, 0);
                }
              } else {
                if (type_of(rhs) == gis->fixnum_type) {
                  /* rhs is number but lhs is not */
                  C_COMPILE(lhs);
                  if (FIXNUM_VALUE(rhs) < 0) {
                    C_PUSH_CODE(op_addi);
                  } else {
                    C_PUSH_CODE(op_subi);
                  }
                  marshal_ufixnum_t(FIXNUM_VALUE(rhs) < 0 ? -FIXNUM_VALUE(rhs)
                                                          : FIXNUM_VALUE(rhs),
                                    C_CODE, 0);
                } else { /* both are not constant numbers */
                  C_COMPILE(lhs);
                  C_COMPILE(rhs);
                  C_PUSH_CODE(op_sub);
                }
              }
            }
          }
        } else if (car == gis->lisp_mul_sym) {
          COMPILE_DO_AGG_EACH({ C_PUSH_CODE(op_mul); });
        } else if (car == gis->lisp_div_sym) {
          COMPILE_DO_AGG_EACH({ C_PUSH_CODE(op_div); });
        } else if (car == gis->impl_and_sym) {
          /* TODO: AND, OR, and EQ should short circuit. currently only allowing
           * two arguments, because later this could be impelmented as a macro
           */
          C_EXE2(op_and);
        } else if (car == gis->lisp_or_sym) {
          C_EXE2(op_or);
        } else if (car == gis->lisp_gt_sym) {
          C_EXE2(op_gt);
        } else if (car == gis->lisp_lt_sym) {
          SF_REQ_N(2, value);
          /* fold constant in a specific case (testing performance) */
          if (type_of(C_ARG1()) == gis->fixnum_type && FIXNUM_VALUE(C_ARG1()) > 0) {
            C_COMPILE_ARG0;
            C_PUSH_CODE(op_lti);
            marshal_ufixnum_t(FIXNUM_VALUE(C_ARG1()), C_CODE, 0);
          } else {
            C_COMPILE_ARG0;
            C_COMPILE_ARG1;
            C_PUSH_CODE(op_lt);
          }
        } else if (car == gis->lisp_gte_sym) {
          C_EXE2(op_gte);
        } else if (car == gis->lisp_lte_sym) {
          C_EXE2(op_lte);
        } else if (car == gis->lisp_equals_sym) {
          C_EXE2(op_eq);
        } else {
          /* TODO: look up the function value in the function lexical
           * environment */
          /* temp = alist_get_value(fst, car); */

          /* check if there is a macro defined for this, if so execute that */
          if (SYMBOL_FUNCTION_IS_SET(car) &&
              type_of(SYMBOL_FUNCTION(car)) == gis->function_type &&
              FUNCTION_IS_MACRO(SYMBOL_FUNCTION(car))) {
            compile(eval(SYMBOL_FUNCTION(car), CONS_CDR(value)), f, st, fst);
          } else {
            COMPILE_DO_EACH({});
            gen_load_constant(f, car);
            C_PUSH_CODE(op_call_symbol_function);
            marshal_ufixnum_t(length, C_CODE, 0);
          }
        }
      }
    } else {
      printf("Invalid sexpr starts with a %s\n",
             type_name_of_cstr(car));
      print(value);
      PRINT_STACK_TRACE_AND_QUIT();
    }
  } else {
    printf("cannot compile type\n");
    PRINT_STACK_TRACE_AND_QUIT();
  }

  return f;
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
  if (f == gis->use_package_builtin) {
    /* TODO */
  } else if (f == gis->find_package_builtin) {
    push(find_package(get_string_designator(GET_LOCAL(0))));
  } else if (f == gis->compile_builtin) {
    push(compile(GET_LOCAL(0), GET_LOCAL(1), GET_LOCAL(2), GET_LOCAL(3)));
  } else if (f == gis->eval_builtin) {
    push(eval_at_instruction(GET_LOCAL(0), FIXNUM_VALUE(GET_LOCAL(1)), NULL));
  } else if (f == gis->type_of_builtin) {
    push(type_of(GET_LOCAL(0)));
  } else if (f == gis->struct_builtin) {
    push(structure(GET_LOCAL(0), GET_LOCAL(1)));
  } else if (f == gis->symbol_struct_builtin) {
    push(symbol_get_struct(GET_LOCAL(0)));
  } else if (f == gis->alloc_struct_builtin) {
    push(alloc_struct(GET_LOCAL(0), 1));
  } else if (f == gis->struct_field_builtin) {
    push(struct_field(GET_LOCAL(0), GET_LOCAL(1)));
  } else if (f == gis->set_struct_field_builtin) {
    set_struct_field(GET_LOCAL(0), GET_LOCAL(1), GET_LOCAL(2));
    push(NIL);
  } else if (f == gis->dynamic_library_builtin) {
    push(dlib(GET_LOCAL(0)));
  } else if (f == gis->foreign_function_builtin) {
    push(ffun(GET_LOCAL(0), GET_LOCAL(1), GET_LOCAL(2), GET_LOCAL(3)));
  } else if (f == gis->package_symbols_builtin) {
    OT("package-symbols", 0, GET_LOCAL(0), type_package);
    push(PACKAGE_SYMBOLS(GET_LOCAL(0)));
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

#define READ_OP_JUMP_ARG()                    \
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

/* returns the top of the stack for convience */
/* evaluates gis->function starting at instruction gis->i */
/* assumes gis->i and gis->f is set and call_stack has been initialized with
 * space for all stack args */
struct object *run(struct gis *gis) {
  unsigned long byte_count,
      op_arg_byte_count; /* number of  bytes in this bytecode */
  unsigned char t0, op;  /* temporary for reading bytecode arguments */
  unsigned long a0, a1;  /* the arguments for bytecode parameters */
  unsigned long a2;
  long sa0;               /* argument for jumps */
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
        v0 = pop(); /* car */
        push(cons(v0, v1));
        break;
      case op_intern: /* intern ( string -- symbol ) */
        SC("intern", 1);
        v0 = pop();
        printf("op_intern is not implemented.");
        PRINT_STACK_TRACE_AND_QUIT();
        OT("intern", 0, v0, type_string);
        push(intern(pop(), NIL));
        break;
      case op_car: /* car ( (cons car cdr) -- car ) */
        SC("car", 1);
        v0 = pop();
        if (v0 == NIL) {
          push(NIL);
        } else if (type_of(v0) == gis->cons_type) {
          push(CONS_CAR(v0));
        } else {
          printf("Can only car a list (was given a %s).",
                 type_name_of_cstr(v0));
          PRINT_STACK_TRACE_AND_QUIT();
        }
        break;
      case op_cdr: /* cdr ( (cons car cdr) -- cdr ) */
        SC("cdr", 1);
        v0 = pop();
        if (v0 == NIL) {
          push(NIL);
        } else if (type_of(v0) == gis->cons_type) {
          push(CONS_CDR(v0));
        } else {
          printf("Can only cdr a list (was given a %s).",
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
        READ_CONST_ARG();
        /* minus two to skip the f and i */
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

        /* if this is a foreign function */
        if (type_of(f) == gis->ffun_type) {
          cursor = FFUN_PARAMS(f);
          if (a0 > FFUN_NARGS(f)) {
            printf("Insufficient arguments were passed to foreign function.");
            PRINT_STACK_TRACE_AND_QUIT();
          }

          a1 = a0; /* for indexing arguments */
          a2 = 0;  /* for indexing arg_values */
          while (cursor != NIL) {
            if (type_of(STACK_I(a1)) == gis->nil_type) {
              arg_values[a2] = NULL;
            } else if (type_of(STACK_I(a1)) == gis->fixnum_type) {
              arg_values[a2] = &FIXNUM_VALUE(STACK_I(a1));
            } else if (type_of(STACK_I(a1)) == gis->string_type) {
              dynamic_byte_array_force_cstr(STACK_I(a1));
              arg_values[a2] = &STRING_CONTENTS(STACK_I(a1));
            } else if (type_of(STACK_I(a1)) == gis->pointer_type) {
              arg_values[a2] = &OBJECT_POINTER(STACK_I(a1));
            } else if (type_of(STACK_I(a1)) ==
                       gis->cons_type) { /* convert to struct* */
            } else {
              printf("Argument not supported for foreign functions.");
            }
            --a1;
            ++a2;
            cursor = CONS_CDR(cursor);
          }

          /* remove all arguments and the function from the data stack at once
           */
          DYNAMIC_ARRAY_LENGTH(gis->data_stack) -= a0 + 1;

          if (FFUN_RET(f) == gis->pointer_type) {
            ffi_call(FFUN_CIF(f), FFI_FN(FFUN_PTR(f)), &ptr_result, arg_values);
            push(pointer(ptr_result));
          } else if (ffi_type_designator_is_string(FFUN_RET(f))) {
            ffi_call(FFUN_CIF(f), FFI_FN(FFUN_PTR(f)), &ptr_result, arg_values);
            push(string(ptr_result));
          } else {
            ffi_call(FFUN_CIF(f), FFI_FN(FFUN_PTR(f)), &sresult, arg_values);
            push(fixnum(sresult));
          }

          UFIXNUM_VALUE(i) += 1; /* advance to the next instruction */
          goto eval_restart;     /* restart the evaluation loop */
        } else if (type_of(f) != gis->function_type) {
          printf("Attempted to call a non-function object.");
          PRINT_STACK_TRACE_AND_QUIT();
        }

        symbol_set_value(gis->impl_f_sym, f);

        /* transfer arguments from data stack to call stack */
        for (a1 = a0; a1 > 0; --a1) {
          dynamic_array_push(gis->call_stack, STACK_I(a1));
        }

        /* remove all arguments and the function from the data stack at once */
        DYNAMIC_ARRAY_LENGTH(gis->data_stack) -= a0 + 1;

        /* initialize any temps with NIL -- could be improved by just adding to
         * the call stack's length */
        for (a1 = 0; a1 < FUNCTION_STACK_SIZE(f) - FUNCTION_NARGS(f); ++a1) {
          dynamic_array_push(gis->call_stack, NIL);
        }

        /* save the instruction index, and function */
        UFIXNUM_VALUE(i) += 1; /* resume at the next instruction */
        dynamic_array_push(gis->call_stack, temp_i);
        dynamic_array_push(gis->call_stack, temp_f);
        if (FUNCTION_IS_BUILTIN(f)) {
          symbol_set_value(
              gis->impl_i_sym,
              NIL); /* a NIL instruction index -- for consistency gis->i of NIL
                       should be set for all builtins */
          eval_builtin(f);
          goto return_function_label; /* return */
        } else {
          symbol_set_value(gis->impl_i_sym,
                           ufixnum(0)); /* start the bytecode interpreter at the
                                           first instruction */
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
        ufix0 = FUNCTION_STACK_SIZE(f) + 2; /* store the frame size */
        /* this indicates that we returned to the top level -- used when calling
         * macros directly from c */
        if (DYNAMIC_ARRAY_VALUES(
                gis->call_stack)[DYNAMIC_ARRAY_LENGTH(gis->call_stack) - 1] ==
                NIL &&
            DYNAMIC_ARRAY_VALUES(
                gis->call_stack)[DYNAMIC_ARRAY_LENGTH(gis->call_stack) - 2] ==
                NIL) {
          printf("DEAD?\n");
          break; /* TODO: This might be dead code? */
        } else {
          symbol_set_value(
              gis->impl_f_sym,
              DYNAMIC_ARRAY_VALUES(
                  gis->call_stack)[DYNAMIC_ARRAY_LENGTH(gis->call_stack) - 1]);
          symbol_set_value(
              gis->impl_i_sym,
              DYNAMIC_ARRAY_VALUES(
                  gis->call_stack)[DYNAMIC_ARRAY_LENGTH(gis->call_stack) - 2]);
          /* pop off all stack arguments, then pop bc, then pop instruction
           * index */
          DYNAMIC_ARRAY_LENGTH(gis->call_stack) -= ufix0;
          goto eval_restart; /* restart the evaluation loop */
        }
      case op_jump: /* jump ( -- ) */
        /* can jump ~32,000 in both directions */
        READ_OP_JUMP_ARG();
        UFIXNUM_VALUE(i) += sa0;
        continue; /* continue so the usual increment to i doesn't happen */
      case op_jump_when_nil: /* jump_when_nil ( cond -- ) */
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
        /* TODO: this polluting the data stack with nils?! */
        STACK_I(0) = NIL;
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
      default:
        printf("Invalid op.\n");
        PRINT_STACK_TRACE_AND_QUIT();
        break;
    }
    ++UFIXNUM_VALUE(i);
  }
  return DYNAMIC_ARRAY_LENGTH(gis->data_stack) ? peek() : NIL;
}

/*
 * Sets the default interpreter state
 */
void init(char load_core) { gis_init(load_core); }

void reinit(char load_core) {
  free(gis); /* TODO: write gis_free */
  gis_init(load_core);
}

/*===============================*
 *===============================*
 * Tests                         *
 *===============================*
 *===============================*/
void run_tests() {
  struct object *darr, *o0, *o1, *dba, *dba1, *bc, *bc0, *bc1, *da, *code,
      *constants, *code0, *consts0, *code1, *consts1;
  ufixnum_t uf0, stack_size;

  printf("============ Running tests... =============\n");

#define assert_string_eq(str1, str2) \
  assert(strcmp(bstring_to_cstring(str1), bstring_to_cstring(str2)) == 0);

#define END_TESTS()                                        \
  printf("============ Tests were successful ========\n"); \
  return;

  /*
   * String - bug string to c string
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
  assert(DYNAMIC_ARRAY_CAPACITY(darr) == 4);
  dynamic_array_push(darr, fixnum(111));
  assert(FIXNUM_VALUE(dynamic_array_length(darr)) == 4);
  assert(FIXNUM_VALUE(dynamic_array_get(darr, fixnum(3))) == 111);
  assert(DYNAMIC_ARRAY_CAPACITY(darr) == 4);
  dynamic_array_push(darr, fixnum(3));
  assert(FIXNUM_VALUE(dynamic_array_length(darr)) == 5);
  assert(FIXNUM_VALUE(dynamic_array_get(darr, fixnum(4))) == 3);
  assert(DYNAMIC_ARRAY_CAPACITY(darr) == 7);

  /*
   * Dynamic byte Array
   */
  dba = dynamic_byte_array(2);
  assert(FIXNUM_VALUE(dynamic_byte_array_length(dba)) == 0);
  dynamic_byte_array_push(dba, fixnum(5));
  assert(FIXNUM_VALUE(dynamic_byte_array_length(dba)) == 1);
  assert(dynamic_byte_array_get(dba, 0) == 5);
  dynamic_byte_array_push(dba, fixnum(9));
  assert(FIXNUM_VALUE(dynamic_byte_array_length(dba)) == 2);
  assert(dynamic_byte_array_get(dba, 1) == 9);
  assert(DYNAMIC_BYTE_ARRAY_CAPACITY(dba) == 2);
  dynamic_byte_array_push(dba, fixnum(94));
  assert(FIXNUM_VALUE(dynamic_byte_array_length(dba)) == 3);
  assert(dynamic_byte_array_get(dba, 2) == 94);
  assert(DYNAMIC_BYTE_ARRAY_CAPACITY(dba) == 4);

  dynamic_byte_array_insert_char(dba, 0, 96);
  assert(UFIXNUM_VALUE(dynamic_byte_array_length(dba)) == 4);
  assert(dynamic_byte_array_get(dba, 0) == 96);
  assert(dynamic_byte_array_get(dba, 1) == 5);
  assert(dynamic_byte_array_get(dba, 2) == 9);
  assert(dynamic_byte_array_get(dba, 3) == 94);

  dynamic_byte_array_insert_char(dba, 2, 99);
  assert(UFIXNUM_VALUE(dynamic_byte_array_length(dba)) == 5);
  assert(dynamic_byte_array_get(dba, 0) == 96);
  assert(dynamic_byte_array_get(dba, 1) == 5);
  assert(dynamic_byte_array_get(dba, 2) == 99);
  assert(dynamic_byte_array_get(dba, 3) == 9);
  assert(dynamic_byte_array_get(dba, 4) == 94);

  dba = dynamic_byte_array(2);
  dynamic_byte_array_insert_char(dba, 0, 11);
  assert(UFIXNUM_VALUE(dynamic_byte_array_length(dba)) == 1);
  assert(dynamic_byte_array_get(dba, 0) == 11);

  dynamic_byte_array_insert_char(dba, 1, 32);
  assert(UFIXNUM_VALUE(dynamic_byte_array_length(dba)) == 2);
  assert(dynamic_byte_array_get(dba, 0) == 11);
  assert(dynamic_byte_array_get(dba, 1) == 32);

  dba = dynamic_byte_array(2);
  dynamic_byte_array_push_char(dba, 4);
  dynamic_byte_array_push_char(dba, 9);
  dba1 = dynamic_byte_array(2);
  dynamic_byte_array_push_char(dba1, 77);
  dynamic_byte_array_push_char(dba1, 122);
  dba = dynamic_byte_array_concat(dba, dba1);
  assert(UFIXNUM_VALUE(dynamic_byte_array_length(dba)) == 4);
  assert(DYNAMIC_BYTE_ARRAY_CAPACITY(dba) == 4);
  assert(dynamic_byte_array_get(dba, 0) == 4);
  assert(dynamic_byte_array_get(dba, 1) == 9);
  assert(dynamic_byte_array_get(dba, 2) == 77);
  assert(dynamic_byte_array_get(dba, 3) == 122);

  /*
   * Marshaling/Unmarshaling
   */

  /* Fixnum marshaling */
  reinit(0);

#define T_MAR_FIXNUM(n) \
  assert(FIXNUM_VALUE(unmarshal_integer(marshal_fixnum(fixnum(n), NULL))) == n);
#define T_MAR_UFIXNUM(n) \
  assert(UFIXNUM_VALUE(  \
             unmarshal_integer(marshal_ufixnum(ufixnum(n), NULL, 1))) == n);
#define T_MAR_FLONUM(n) \
  assert(FLONUM_VALUE(unmarshal_float(marshal_flonum(n, NULL))) == n);
#define T_MAR_STR(x)                                                          \
  assert_string_eq(                                                           \
      unmarshal_string(marshal_string(string(x), NULL, 1, NULL), 1, NULL, 1), \
      string(x));
  /* test marshaling with the default package */
#define T_MAR_SYM_DEF(x)                                                 \
  assert(unmarshal_symbol(                                               \
             marshal_symbol(intern(string(x), GIS_PACKAGE), NULL, NULL), \
             NULL) == intern(string(x), GIS_PACKAGE));
#define T_MAR_SYM(x, p)                                                       \
  assert(unmarshal_symbol(                                                    \
             marshal_symbol(intern(string(x), find_package(string(p))), NULL, \
                            NULL),                                            \
             NULL) == intern(string("dinkle"), find_package(string(p))));
#define T_MAR(x) assert(equals(unmarshal(marshal(x, NULL, NULL), NULL), x));

  assert(unmarshal_ufixnum_t(marshal_ufixnum_t(1, NULL, 0)) == 1);
  /* three bytes long */
  assert(unmarshal_ufixnum_t(marshal_ufixnum_t(122345, NULL, 0)) == 122345);
  assert(unmarshal_ufixnum_t(marshal_ufixnum_t(1223450, NULL, 0)) == 1223450);
  /* four bytes long */
  assert(unmarshal_ufixnum_t(marshal_ufixnum_t(122345000, NULL, 0)) ==
         122345000);

  /* no bytes */
  T_MAR_FIXNUM(0);
  /* one byte */
  T_MAR_FIXNUM(9);
  T_MAR_FIXNUM(-23);
  T_MAR_FIXNUM(-76);
  T_MAR(fixnum(
      -76)); /* make sure it is included in main marshal/unmarshal switches */
  /* two bytes */
  T_MAR_FIXNUM(256);
  T_MAR_FIXNUM(257);
  T_MAR_FIXNUM(-342);
  T_MAR_FIXNUM(2049);
  /* three bytes */
  T_MAR_FIXNUM(123456);
  T_MAR_FIXNUM(-123499);
  T_MAR_FIXNUM(20422);
  /* four bytes */
  T_MAR_FIXNUM(123456789);
  T_MAR_FIXNUM(-123456789);
  /* Should test fixnums that are from platforms that have larger word sizes
   * (e.g. read in a fixnum larger than four bytes on a machine with two byte
   * words) */
  T_MAR_UFIXNUM(MAX_UFIXNUM);

  /* flonum marshaling */
  T_MAR_FLONUM(0.0);
  T_MAR_FLONUM(1.0);
  T_MAR_FLONUM(1.23);
  T_MAR_FLONUM(1e23);
  T_MAR_FLONUM(1e-23);
  T_MAR_FLONUM(123456789123456789123456789123456789.0);
  T_MAR_FLONUM(-0.0);
  T_MAR_FLONUM(-1.23);

  /* TODO: test that NaN and +/- Infinity work */

  /* string marshaling */
  T_MAR_STR("abcdef");
  T_MAR_STR("nerEW)F9nef09n230(N#EOINWEogiwe");
  T_MAR_STR("");

  reinit(0);
  T_MAR_SYM_DEF("abcdef");

  /* uninterned symbol */
  o0 =
      unmarshal_symbol(marshal_symbol(symbol(string("fwe")), NULL, NULL), NULL);
  assert(SYMBOL_PACKAGE(o0) == NIL);
  add_package(package(string("peep"), NIL));
  T_MAR_SYM("dinkle", "peep");
  /* make sure the symbol isn't interned into the wrong package */
  assert(unmarshal_symbol(marshal_symbol(intern(string("dinkle"),
                                                find_package(string("peep"))),
                                         NULL, NULL),
                          NULL) != intern(string("dinkle"), GIS_PACKAGE));

  /* nil marshaling */
  assert(unmarshal_nil(marshal_nil(NULL)) == NIL);

  /* cons filled with nils */
  o0 = unmarshal_cons(marshal_cons(cons(NIL, NIL), NULL, 1, NULL), 1, NULL);
  assert(CONS_CAR(o0) == NIL);
  assert(CONS_CDR(o0) == NIL);
  /* cons with strings */
  o0 = unmarshal_cons(
      marshal_cons(cons(string("A"), string("B")), NULL, 1, NULL), 1, NULL);
  assert(strcmp(bstring_to_cstring(CONS_CAR(o0)), "A") == 0);
  assert(strcmp(bstring_to_cstring(CONS_CDR(o0)), "B") == 0);
  /* cons list with fixnums */
  o0 = unmarshal_cons(
      marshal_cons(cons(fixnum(35), cons(fixnum(99), NIL)), NULL, 1, NULL), 1,
      NULL);
  assert(FIXNUM_VALUE(CONS_CAR(o0)) == 35);
  assert(FIXNUM_VALUE(CONS_CAR(CONS_CDR(o0))) == 99);
  assert(CONS_CDR(CONS_CDR(o0)) == NIL);

  /* marshal/unmarshal dynamic byte array */
  dba = dynamic_byte_array(10);
  dynamic_byte_array_push_char(dba, 54);
  dynamic_byte_array_push_char(dba, 99);
  dynamic_byte_array_push_char(dba, 23);
  dynamic_byte_array_push_char(dba, 9);
  o0 =
      unmarshal_dynamic_byte_array(marshal_dynamic_byte_array(dba, NULL, 1), 1);
  assert(DYNAMIC_BYTE_ARRAY_LENGTH(dba) == DYNAMIC_BYTE_ARRAY_LENGTH(o0));
  assert(strcmp(bstring_to_cstring(dba), bstring_to_cstring(o0)) == 0);

#define T_DBA_PUSH(n) dynamic_byte_array_push_char(dba, n);

  dba = dynamic_byte_array(10);
  T_DBA_PUSH(0);
  T_DBA_PUSH(1);
  T_DBA_PUSH(2);
  T_DBA_PUSH(3);
  T_DBA_PUSH(4);
  T_DBA_PUSH(5);
  T_DBA_PUSH(6);
  T_DBA_PUSH(7);
  T_DBA_PUSH(8);
  T_DBA_PUSH(9);
  T_DBA_PUSH(10);
  T_DBA_PUSH(0);
  T_DBA_PUSH(12);
  T_DBA_PUSH(13);
  T_DBA_PUSH(14);
  T_DBA_PUSH(15);
  T_DBA_PUSH(16);
  T_DBA_PUSH(17);
  T_DBA_PUSH(18);
  T_DBA_PUSH(19);
  T_DBA_PUSH(20);
  T_DBA_PUSH(21);
  T_DBA_PUSH(22);
  T_DBA_PUSH(23);
  T_DBA_PUSH(24);
  T_DBA_PUSH(25);
  T_DBA_PUSH(26);
  T_DBA_PUSH(27);
  T_DBA_PUSH(28);
  T_DBA_PUSH(29);
  T_DBA_PUSH(30);
  T_DBA_PUSH(31);
  T_DBA_PUSH(32);
  T_DBA_PUSH(33);
  T_DBA_PUSH(34);
  T_DBA_PUSH(35);
  T_DBA_PUSH(36);
  T_DBA_PUSH(37);
  T_DBA_PUSH(38);
  T_DBA_PUSH(39);
  T_DBA_PUSH(40);
  o0 =
      unmarshal_dynamic_byte_array(marshal_dynamic_byte_array(dba, NULL, 1), 1);
  assert(DYNAMIC_BYTE_ARRAY_LENGTH(dba) == DYNAMIC_BYTE_ARRAY_LENGTH(o0));
  assert(equals(dba, o0));

  /* marshal/unmarshal dynamic array */
#define T_DA_PUSH(n) dynamic_array_push(darr, n);

  darr = dynamic_array(10);
  dynamic_array_push(darr, fixnum(3));
  dynamic_array_push(darr, string("e2"));
  dynamic_array_push(darr, NIL);
  o0 = unmarshal_dynamic_array(marshal_dynamic_array(darr, NULL, 1, NULL), 1,
                               NULL);
  assert(DYNAMIC_ARRAY_LENGTH(darr) == DYNAMIC_ARRAY_LENGTH(o0));
  assert(FIXNUM_VALUE(DYNAMIC_ARRAY_VALUES(darr)[0]) == 3);
  assert(strcmp(bstring_to_cstring(DYNAMIC_ARRAY_VALUES(darr)[1]), "e2") == 0);
  assert(DYNAMIC_ARRAY_VALUES(darr)[2] == NIL);

  darr = dynamic_array(10);
  T_DA_PUSH(fixnum(0));
  T_DA_PUSH(fixnum(1));
  T_DA_PUSH(fixnum(2));
  T_DA_PUSH(fixnum(3));
  T_DA_PUSH(fixnum(4));
  T_DA_PUSH(fixnum(5));
  T_DA_PUSH(fixnum(6));
  T_DA_PUSH(fixnum(7));
  T_DA_PUSH(fixnum(8));
  T_DA_PUSH(fixnum(9));
  T_DA_PUSH(fixnum(10));
  T_DA_PUSH(fixnum(11));
  T_DA_PUSH(fixnum(12));
  T_DA_PUSH(fixnum(13));
  T_DA_PUSH(fixnum(14));
  T_DA_PUSH(fixnum(15));
  T_DA_PUSH(fixnum(16));
  T_DA_PUSH(fixnum(17));
  T_DA_PUSH(fixnum(18));
  T_DA_PUSH(fixnum(19));
  T_DA_PUSH(fixnum(20));
  T_DA_PUSH(fixnum(21));
  T_DA_PUSH(fixnum(22));
  T_DA_PUSH(fixnum(23));
  T_DA_PUSH(fixnum(24));
  T_DA_PUSH(fixnum(25));
  T_DA_PUSH(fixnum(26));
  T_DA_PUSH(fixnum(27));
  T_DA_PUSH(fixnum(28));
  T_DA_PUSH(fixnum(29));
  T_DA_PUSH(fixnum(30));
  T_DA_PUSH(fixnum(31));
  o0 = unmarshal_dynamic_array(marshal_dynamic_array(darr, NULL, 1, NULL), 1,
                               NULL);
  assert(equals(darr, o0));

  /* marshal function */
  darr = dynamic_array(10); /* constants vector */
  dynamic_array_push(darr, string("blackhole"));
  dynamic_array_push(darr, flonum(3.234));
  dynamic_array_push(darr, ufixnum(234234234));
  dba = dynamic_byte_array(10); /* the code vector */
  dynamic_byte_array_push_char(dba, 0x62);
  dynamic_byte_array_push_char(dba, 0x39);
  dynamic_byte_array_push_char(dba, 0x13);
  dynamic_byte_array_push_char(dba, 0x23);
  bc = function(darr, dba, 12); /* bogus stack size */
  o0 = unmarshal_function(marshal_function(bc, NULL, 1, NULL), 1, NULL);
  /* check constants vector */
  assert(DYNAMIC_ARRAY_LENGTH(darr) ==
         DYNAMIC_ARRAY_LENGTH(FUNCTION_CONSTANTS(o0)));
  assert(strcmp(bstring_to_cstring(
                    DYNAMIC_ARRAY_VALUES(FUNCTION_CONSTANTS(o0))[0]),
                "blackhole") == 0);
  assert(FLONUM_VALUE(DYNAMIC_ARRAY_VALUES(FUNCTION_CONSTANTS(o0))[1]) ==
         3.234);
  assert(UFIXNUM_VALUE(DYNAMIC_ARRAY_VALUES(FUNCTION_CONSTANTS(o0))[2]) ==
         234234234);
  /* check code vector */
  assert(DYNAMIC_BYTE_ARRAY_LENGTH(dba) ==
         DYNAMIC_BYTE_ARRAY_LENGTH(FUNCTION_CODE(o0)));
  assert(strcmp(bstring_to_cstring(dba),
                bstring_to_cstring(FUNCTION_CODE(o0))) == 0);

  darr = dynamic_array(10); /* constants vector */
  T_DA_PUSH(intern(string("g"), gis->user_package));
  T_DA_PUSH(fixnum(1));
  T_DA_PUSH(fixnum(2));
  T_DA_PUSH(fixnum(3));
  T_DA_PUSH(fixnum(4));
  T_DA_PUSH(intern(string("t"), gis->lisp_package));
  T_DA_PUSH(string("g = "));
  T_DA_PUSH(intern(string("g"), gis->user_package));
  T_DA_PUSH(string("wow it works!"));
  T_DA_PUSH(fixnum(7));
  T_DA_PUSH(fixnum(3));
  T_DA_PUSH(fixnum(2));
  T_DA_PUSH(fixnum(9));
  T_DA_PUSH(string("didn't work"));
  T_DA_PUSH(string("fff"));
  dba = dynamic_byte_array(10); /* the code vector */
  T_DBA_PUSH(0x15);
  T_DBA_PUSH(0x00);
  T_DBA_PUSH(0x15);
  T_DBA_PUSH(0x01);
  T_DBA_PUSH(0x15);
  T_DBA_PUSH(0x02);
  T_DBA_PUSH(0x11);
  T_DBA_PUSH(0x15);
  T_DBA_PUSH(0x03);
  T_DBA_PUSH(0x11);
  T_DBA_PUSH(0x15);
  T_DBA_PUSH(0x04);
  T_DBA_PUSH(0x11);
  T_DBA_PUSH(0x1F);
  T_DBA_PUSH(0x00);
  T_DBA_PUSH(0x15);
  T_DBA_PUSH(0x05);
  T_DBA_PUSH(0x20);
  T_DBA_PUSH(0x22);
  T_DBA_PUSH(0x00);
  T_DBA_PUSH(0x1A);
  T_DBA_PUSH(0x15);
  T_DBA_PUSH(0x06);
  T_DBA_PUSH(0x16);
  T_DBA_PUSH(0x15);
  T_DBA_PUSH(0x07);
  T_DBA_PUSH(0x20);
  T_DBA_PUSH(0x16);
  T_DBA_PUSH(0x15);
  T_DBA_PUSH(0x08);
  T_DBA_PUSH(0x16);
  T_DBA_PUSH(0x15);
  T_DBA_PUSH(0x09);
  T_DBA_PUSH(0x15);
  T_DBA_PUSH(0x0A);
  T_DBA_PUSH(0x12);
  T_DBA_PUSH(0x15);
  T_DBA_PUSH(0x0B);
  T_DBA_PUSH(0x12);
  T_DBA_PUSH(0x15);
  T_DBA_PUSH(0x0C);
  T_DBA_PUSH(0x12);
  T_DBA_PUSH(0x16);
  T_DBA_PUSH(0x21);
  T_DBA_PUSH(0x00);
  T_DBA_PUSH(0x08);
  T_DBA_PUSH(0x15);
  T_DBA_PUSH(0x0D);
  T_DBA_PUSH(0x16);
  T_DBA_PUSH(0x00);
  T_DBA_PUSH(0x15);
  T_DBA_PUSH(0x0E);
  bc = function(darr, dba, 8); /* bogus stack size */
  o0 = unmarshal_function(marshal_function(bc, NULL, 1, NULL), 1, NULL);
  assert(equals(bc, o0));

  /* string cache */
  o0 = string_marshal_cache_get_default();
  assert(string_marshal_cache_intern_cstr(o0, "lisp", &uf0) ==
         gis->lisp_str);
  assert(string_marshal_cache_intern_cstr(o0, "lisp", &uf0) != string("lisp"));
  assert(string_marshal_cache_intern_cstr(o0, "mips", &uf0) ==
         string_marshal_cache_intern_cstr(o0, "mips", &uf0));
  string_marshal_cache_intern_cstr(o0, "mips", &uf0);
  assert(uf0 == DYNAMIC_ARRAY_LENGTH(o0) - 1);

  /* to-string */
  assert_string_eq(to_string(fixnum(0)), string("0"));
  assert_string_eq(to_string(fixnum(1)), string("1"));
  assert_string_eq(to_string(fixnum(2)), string("2"));
  assert_string_eq(to_string(fixnum(3)), string("3"));
  assert_string_eq(to_string(fixnum(9)), string("9"));
  assert_string_eq(to_string(fixnum(10)), string("10"));

  assert_string_eq(to_string(string("ABC")), string("ABC"));
  assert_string_eq(to_string(string("")), string(""));

  assert_string_eq(to_string(cons(string("ABC"), fixnum(43))),
                   string("(\"ABC\" 43)"));
  assert_string_eq(to_string(cons(string("ABC"), cons(ufixnum(24234234), NIL))),
                   string("(\"ABC\" 24234234)"));

  assert_string_eq(to_string(flonum(0.0)), string("0.0"));
  assert_string_eq(to_string(flonum(1)), string("1.0"));
  assert_string_eq(to_string(flonum(0.01)), string("0.01"));
  assert_string_eq(to_string(flonum(56789.0)), string("56789.0"));
  assert_string_eq(to_string(flonum(56789.43215)), string("56789.43215"));
  assert_string_eq(to_string(flonum(9999999.41)), string("9999999.41"));
  assert_string_eq(to_string(flonum(9000000000000000.52)),
                   string("9000000000000001.0"));
  assert_string_eq(to_string(flonum(9e15)), string("9000000000000000.0"));
  assert_string_eq(to_string(flonum(9e15 * 10)), string("9e+16"));
  assert_string_eq(to_string(flonum(0.123456789123456789123456789)),
                   string("0.123456789"));
  assert_string_eq(to_string(flonum(1234123412341123499.123412341234)),
                   string("1.234123412e+18"));
  assert_string_eq(to_string(flonum(1234123412941123499.123412341234)),
                   string("1.234123413e+18")); /* should round last digit */
  assert_string_eq(to_string(flonum(0.000001)), string("0.000001"));
  assert_string_eq(to_string(flonum(0.0000001)), string("1e-7"));
  assert_string_eq(to_string(flonum(0.00000001)), string("1e-8"));
  assert_string_eq(to_string(flonum(0.000000059)), string("5.9e-8"));
  assert_string_eq(
      to_string(flonum(
          0.00000000000000000000000000000000000000000000034234234232942362341239123412312348)),
      string("3.423423423e-46"));
  assert_string_eq(
      to_string(flonum(
          0.00000000000000000000000000000000000000000000034234234267942362341239123412312348)),
      string("3.423423427e-46")); /* should round last digit */

  assert_string_eq(to_string(vec2(1, 2)), string("<1.0 2.0>"));

  /* TODO: something is wrong with 1.0003 (try typing it into the REPL) not sure
   * if its an issue during read or to-string. any leading zeros are cut off */
  /* TODO: something wrong with 23532456346234623462346236. */

  dba = dynamic_byte_array(10);
  dynamic_byte_array_push_char(dba, 60);
  dynamic_byte_array_push_char(dba, 71);
  dynamic_byte_array_push_char(dba, 1);
  assert_string_eq(to_string(dba), string("[0x3C 0x47 0x01]"));

  dba = dynamic_byte_array(10);
  assert_string_eq(to_string(dba), string("[]"));

  da = dynamic_array(10);
  dynamic_array_push(da, string("bink"));
  dynamic_array_push(da, flonum(3.245));
  assert_string_eq(to_string(da), string("[\"bink\" 3.245]"));

  da = dynamic_array(10);
  assert_string_eq(to_string(da), string("[]"));

  /*
   * equals
   */

  /* fixnum */
  assert(equals(fixnum(-2), fixnum(-2)));
  assert(!equals(fixnum(-2), fixnum(2)));
  /* ufixnum */
  assert(equals(ufixnum(242344238L), ufixnum(242344238L)));
  assert(!equals(ufixnum(242344231L), ufixnum(242344238L)));
  /* flonum */
  assert(equals(flonum(1.2), flonum(1.2)));
  assert(equals(flonum(1.234), flonum(1.234)));
  assert(!equals(flonum(-1.234), flonum(1.234)));
  assert(equals(flonum(1e-3), flonum(0.001)));
  /* string */
  assert(equals(string("abcdefghi"), string("abcdefghi")));
  assert(!equals(string("e"), string("")));
  /* nil */
  assert(equals(NIL, NIL));
  /* cons */
  assert(equals(cons(fixnum(7), NIL), cons(fixnum(7), NIL)));
  assert(!equals(cons(fixnum(7), cons(fixnum(8), NIL)),
                 cons(fixnum(7), cons(fixnum(10), NIL))));
  assert(!equals(cons(fixnum(7), cons(fixnum(8), NIL)),
                 cons(fixnum(9), cons(fixnum(8), NIL))));
  assert(!equals(cons(fixnum(7), cons(fixnum(8), NIL)),
                 cons(fixnum(7), cons(fixnum(8), cons(fixnum(9), NIL)))));
  assert(equals(cons(fixnum(7), cons(fixnum(8), cons(fixnum(9), NIL))),
                cons(fixnum(7), cons(fixnum(8), cons(fixnum(9), NIL)))));
  assert(equals(cons(cons(string("a"), cons(string("b"), NIL)),
                     cons(fixnum(8), cons(fixnum(9), NIL))),
                cons(cons(string("a"), cons(string("b"), NIL)),
                     cons(fixnum(8), cons(fixnum(9), NIL)))));
  assert(!equals(cons(cons(string("a"), cons(string("b"), NIL)),
                      cons(fixnum(8), cons(fixnum(9), NIL))),
                 cons(cons(string("a"), cons(string("x"), NIL)),
                      cons(fixnum(8), cons(fixnum(9), NIL)))));
  /* dynamic array */
  o0 = dynamic_array(10);
  o1 = dynamic_array(10);
  assert(equals(o0, o1));

  dynamic_array_push(o0, fixnum(1));
  assert(!equals(o0, o1));
  dynamic_array_push(o1, fixnum(1));
  assert(equals(o0, o1));

  dynamic_array_push(o0, fixnum(2));
  dynamic_array_push(o1, fixnum(2));
  assert(equals(o0, o1));
  /* dynamic byte array */
  o0 = dynamic_byte_array(10);
  o1 = dynamic_byte_array(10);
  assert(equals(o0, o1));

  dynamic_byte_array_push(o0, fixnum(1));
  assert(!equals(o0, o1));
  dynamic_byte_array_push(o1, fixnum(1));
  assert(equals(o0, o1));

  dynamic_byte_array_push(o0, fixnum(2));
  dynamic_byte_array_push(o1, fixnum(2));
  assert(equals(o0, o1));

  /* enumerator */
  o0 = enumerator(string("abcdefg"));
  o1 = enumerator(string("abcdefg"));
  assert(equals(o0, o1));
  byte_stream_read(o1, 1);
  assert(!equals(o0, o1));
  byte_stream_read(o0, 1);
  assert(equals(o0, o1));
  o0 = enumerator(string("91"));
  o1 = enumerator(string("93"));
  assert(!equals(o0, o1));

  /* TODO: add equals tests for package, symbol and file */

  /* bytecode */
  code0 = dynamic_byte_array(100);
  dynamic_byte_array_push_char(code0, op_const);
  dynamic_byte_array_push_char(code0, 0);
  consts0 = dynamic_array(100);
  dynamic_array_push(consts0, string("ab"));
  dynamic_array_push(consts0, cons(string("F"), cons(string("E"), NIL)));
  dynamic_array_push(consts0, flonum(8));
  o0 = function(consts0, code0, 99); /* bogus stack size */

  code1 = dynamic_byte_array(100);
  dynamic_byte_array_push_char(code1, op_const);
  dynamic_byte_array_push_char(code1, 0);
  consts1 = dynamic_array(100);
  dynamic_array_push(consts1, string("ab"));
  dynamic_array_push(consts1, cons(string("F"), cons(string("E"), NIL)));
  dynamic_array_push(consts1, flonum(8));
  o1 = function(consts1, code1, 99); /* bogus stack size */
  assert(equals(o0, o1));

  dynamic_array_push(consts1, flonum(10));
  assert(!equals(o0, o1));

  dynamic_array_push(consts0, flonum(10));
  assert(equals(o0, o1));

  dynamic_byte_array_push_char(code1, 9);
  assert(!equals(o0, o1));

  dynamic_byte_array_push_char(code0, 9);
  assert(equals(o0, o1));

  /* To be equal the types must be the same */
  assert(!equals(NIL, fixnum(2)));
  assert(!equals(fixnum(8), string("g")));

  /******* read ***********/
#define T_READ_TEST(i, o) assert(equals(read(string(i), GIS_PACKAGE), o))
#define T_SYM(s) intern(string(s), GIS_PACKAGE)
  reinit(0);
  /* reading fixnums */
  T_READ_TEST("3", fixnum(3));
  T_READ_TEST("12345", fixnum(12345));
  T_READ_TEST("   \r\n12345", fixnum(12345));
  /* reading strings */
  T_READ_TEST("\"a\"", string("a"));
  T_READ_TEST("\"\"", string(""));
  T_READ_TEST("\"12\\n34\"", string("12\n34"));
  T_READ_TEST("\"\\r\\n\\t\"", string("\r\n\t"));
  T_READ_TEST("\"\\\"abcd\\\"\"", string("\"abcd\""));
  /* reading symbols */
  T_READ_TEST("1.2.", T_SYM("1.2."));
  T_READ_TEST("1ee", T_SYM("1ee"));
  T_READ_TEST("a", T_SYM("a"));
  T_READ_TEST("abc", T_SYM("abc"));
  T_READ_TEST("abc ", T_SYM("abc"));
  T_READ_TEST("    abc ", T_SYM("abc"));
  /* symbols that could be mistaken for floats */
  T_READ_TEST("    1ee ", T_SYM("1ee"));
  T_READ_TEST("    1e3e ", T_SYM("1e3e"));
  T_READ_TEST("    1e1.2 ", T_SYM("1e1.2"));
  T_READ_TEST("    1.2.3e3 ", T_SYM("1.2.3e3"));
  T_READ_TEST("    ++1 ", T_SYM("++1"));
  T_READ_TEST("    1+ ", T_SYM("1+"));
  T_READ_TEST("    -+3 ", T_SYM("-+3"));
  T_READ_TEST("    . ", T_SYM("."));
  T_READ_TEST("+", T_SYM("+"));
  /* reading flonums */
  T_READ_TEST("1.2", flonum(1.2));
  T_READ_TEST("1.234567", flonum(1.234567));
  T_READ_TEST("1e9", flonum(1e9));
  T_READ_TEST("1.2345e9", flonum(1.2345e9));

  /* there was something bizarre going on for this test where:

      double x = -3;
      assert(pow(10, -3) == pow(10, x));

     was failing. Apparently this is because pow(10, -3) is handled at compile
     time (by GCC) and converted to the precise constant 0.001 (the closest
     number to 0.001 that can be represented in floating point). But the pow(10,
     x) was calling the linked math library's pow on my Windows machine which
     apparently is imprecise pow(10, x) returned a number larger than 0.001.

     To fix this, instead of doing pow(10, -3), I now do 1/pow(10, 3). this
     gives the same result even if a variable is used (to force the linked
     library to be called). To ensure the math library is linked , I also added
     the '-lm' compile flag.
  */
  T_READ_TEST("1e-3", flonum(0.001));
  T_READ_TEST("1e-5", flonum(0.00001));
  T_READ_TEST("1e-8", flonum(0.00000001));
  T_READ_TEST("1.234e-8", flonum(0.00000001234));
  T_READ_TEST(".1", flonum(0.1));
  T_READ_TEST(".93", flonum(0.93));
  T_READ_TEST("3.", flonum(3.0));
  T_READ_TEST("3.e4", flonum(3.0e4));
  T_READ_TEST("3.e0", flonum(3.0e0));

  /* sexprs */
  T_READ_TEST("()", NIL);
  T_READ_TEST("(      )", NIL);
  T_READ_TEST("( \t\n\r     )", NIL);
  T_READ_TEST("(         \t1\t    )", cons(fixnum(1), NIL));
  T_READ_TEST("\n  (         \t1\t    )\n\n  \t", cons(fixnum(1), NIL));
  T_READ_TEST("(1)", cons(fixnum(1), NIL));
  T_READ_TEST("(1 2 3)",
              cons(fixnum(1), cons(fixnum(2), cons(fixnum(3), NIL))));
  T_READ_TEST(
      "(\"dinkle\" 1.234 1e-3)",
      cons(string("dinkle"), cons(flonum(1.234), cons(flonum(1e-3), NIL))));
  T_READ_TEST("(())", cons(NIL, NIL));
  T_READ_TEST("(() ())", cons(NIL, cons(NIL, NIL)));
  T_READ_TEST("((1 2) (\"a\" \"b\"))",
              cons(cons(fixnum(1), cons(fixnum(2), NIL)),
                   cons(cons(string("a"), cons(string("b"), NIL)), NIL)));
  T_READ_TEST(
      "(((1) (2)))",
      cons(cons(cons(fixnum(1), NIL), cons(cons(fixnum(2), NIL), NIL)), NIL));

  /* ensure special characters have priority */
  T_READ_TEST("(1\"b\")", cons(fixnum(1), cons(string("b"), NIL)));

  /********* symbol interning ************/
  reinit(0);
  assert(count(PACKAGE_SYMBOLS(GIS_PACKAGE)) == 0);
  o0 = intern(string("a"), GIS_PACKAGE); /* this should create a new symbol and
                                            add it to the package */
  assert(count(PACKAGE_SYMBOLS(GIS_PACKAGE)) == 1);
  assert(
      equals(SYMBOL_NAME(CONS_CAR(PACKAGE_SYMBOLS(GIS_PACKAGE))), string("a")));
  assert(o0 ==
         intern(string("a"),
                GIS_PACKAGE)); /* intern should find the existing symbol */
  o0 = intern(string("b"), GIS_PACKAGE); /* this should create a new symbol and
                                            add it to the package */
  assert(count(PACKAGE_SYMBOLS(GIS_PACKAGE)) == 2);

  /********* alist ***************/
  o0 = NIL;
  assert(alist_get_slot(o0, string("a")) == NIL);
  assert(alist_get_value(o0, string("a")) == NIL);
  o0 = alist_extend(o0, string("b"), fixnum(4));
  assert(equals(alist_get_value(o0, string("b")), fixnum(4)));
  o0 = alist_extend(o0, string("b"), fixnum(11));
  assert(equals(alist_get_value(o0, string("b")), fixnum(11)));
  o0 = alist_extend(o0, string("v"), string("abc"));
  assert(equals(alist_get_value(o0, string("v")), string("abc")));

  /********** compilation ****************/
#define BEGIN_BC_TEST(input_string)                                      \
  bc0 = compile(read(string(input_string), GIS_PACKAGE), NIL, NIL, NIL); \
  code = dynamic_byte_array(10);                                         \
  constants = dynamic_array(10);                                         \
  stack_size = 0;
#define END_BC_TEST()                          \
  bc1 = function(constants, code, stack_size); \
  assert(equals(bc0, bc1));
#define DEBUG_END_BC_TEST()                    \
  bc1 = function(constants, code, stack_size); \
  printf("ACTUAL:\t\t");                       \
  print(bc0);                                  \
  printf("EXPECTED:\t");                       \
  print(bc1);                                  \
  assert(equals(bc0, bc1));
#define T_BYTE(op) dynamic_byte_array_push_char(code, op);
#define T_CONST(c) dynamic_array_push(constants, c);
#define T_STACK_SIZE(c) stack_size = c;

  reinit(0);

  BEGIN_BC_TEST("3");
  T_BYTE(op_const_0);
  T_CONST(fixnum(3));
  END_BC_TEST();

  BEGIN_BC_TEST("\"dink\"");
  T_BYTE(op_const_0);
  T_CONST(string("dink"));
  END_BC_TEST();

  BEGIN_BC_TEST("(cons 1 2.3)");
  T_BYTE(op_const_0);
  T_BYTE(op_const_1);
  T_BYTE(op_cons);
  T_CONST(fixnum(1));
  T_CONST(flonum(2.3));
  END_BC_TEST();

  BEGIN_BC_TEST("(cons 1 (cons \"a\" 9))");
  T_BYTE(op_const_0);
  T_BYTE(op_const_1);
  T_BYTE(op_const_2);
  T_BYTE(op_cons);
  T_BYTE(op_cons);
  T_CONST(fixnum(1));
  T_CONST(string("a"));
  T_CONST(fixnum(9));
  END_BC_TEST();

  BEGIN_BC_TEST("(print \"a\" \"b\" \"c\")");
  T_BYTE(op_const_0);
  T_BYTE(op_print);
  T_BYTE(op_const_1);
  T_BYTE(op_print);
  T_BYTE(op_const_2);
  T_BYTE(op_print);
  T_BYTE(op_print_nl);
  T_BYTE(op_load_nil);
  T_CONST(string("a"));
  T_CONST(string("b"));
  T_CONST(string("c"));
  END_BC_TEST();

  BEGIN_BC_TEST("(+ 1 2 3 4 5)");
  T_BYTE(op_const_0);
  T_BYTE(op_addi);
  T_BYTE(3);
  T_BYTE(op_addi);
  T_BYTE(4);
  T_BYTE(op_addi);
  T_BYTE(5);
  T_CONST(fixnum(3));
  END_BC_TEST();

/*
  BEGIN_BC_TEST("(+ 1 (- 8 9 33) 4 5)");
  T_BYTE(op_const_0);
  T_BYTE(op_subi);
  T_BYTE(33);
  T_BYTE(op_addi);
  T_BYTE(1);
  T_BYTE(op_addi);
  T_BYTE(4);
  T_BYTE(op_addi);
  T_BYTE(5);
  T_CONST(fixnum(-1));
  DEBUG_END_BC_TEST();
  */

  BEGIN_BC_TEST("(if 1 2 3)");
  T_BYTE(op_const_0);
  T_BYTE(op_jump_when_nil);
  T_BYTE(0);
  T_BYTE(5);
  T_BYTE(op_const_1);
  T_BYTE(op_jump);
  T_BYTE(0);
  T_BYTE(2);
  T_BYTE(op_const_2);
  T_CONST(fixnum(1));
  T_CONST(fixnum(2));
  T_CONST(fixnum(3));
  END_BC_TEST();

  BEGIN_BC_TEST("(if 1 2 3 4)");
  T_BYTE(op_const_0);
  T_BYTE(op_jump_when_nil);
  T_BYTE(0);
  T_BYTE(5);
  T_BYTE(op_const_1);
  T_BYTE(op_jump);
  T_BYTE(0);
  T_BYTE(4);
  T_BYTE(op_const_2);
  T_BYTE(op_drop);
  T_BYTE(op_const_3);
  T_CONST(fixnum(1));
  T_CONST(fixnum(2));
  T_CONST(fixnum(3));
  T_CONST(fixnum(4));
  END_BC_TEST();

  BEGIN_BC_TEST("(let ((a 2)) a)");
  T_BYTE(op_const_0); /* load the number 2 */
  T_BYTE(op_store_to_stack);
  T_BYTE(0);                    /* store to index 0 on the stack */
  T_BYTE(op_load_from_stack_0); /* evaluation of "a" comes from the stack, not
                                   symbol-value */
  T_CONST(fixnum(2));
  T_STACK_SIZE(1);
  END_BC_TEST();

  END_TESTS();
}

/*
 * Compile Mode:
 *   > bug -c myfile.bug -o myfile.bc
 * Interpret Mode:
 *   > bug myfile.bc
 *     OR
 *   > bug myfile.bug
 *     If it is a bytecode file, interpret as bytecode. Otherwise, attempt to
 * compile it, then interpret the results. Repl Mode: > bug
 */
int main(int argc, char **argv) {
  char interpret_mode, compile_mode, repl_mode;
  struct object *bc, *input_file, *output_file, *input_filepath,
      *output_filepath, *temp;

  init(1); /* must init before using NIL */

  interpret_mode = compile_mode = repl_mode = 0;
  output_filepath = input_filepath = NIL;

  if (argc == 1) {
    repl_mode = 1;
  } else if (argc == 2) {
    interpret_mode = 1;

    if (strcmp(argv[1], "--help") == 0) {
      printf("Bug LISP\n");
      printf(
          "  --run-tests\tRuns the interpreter and compiler's test suite.\n");
      printf("  -c\tCompiles the given file.\n");
      printf("  -o\tChooses a name for the compiled file.\n");
      return 0;
    }

    if (strcmp(argv[1], "--run-tests") == 0) {
      run_tests();
      return 0;
    }

    if (strcmp(argv[1], "-c") == 0) {
      printf(
          "You must provide a file name to the -c parameter (the source file "
          "to compile).");
      return 0;
    }
    if (strcmp(argv[1], "-o") == 0) {
      printf("You may only use the -o parameter when compiling a source file.");
      return 0;
    }

    if (argv[1][0] == '-') {
      printf("Invalid command argument \"%s\". Use --help flag to see options.",
             argv[1]);
      return 0;
    }

    input_filepath = string(argv[1]);
  } else {
    compile_mode = 1;
    if (strcmp(argv[1], "-c") != 0) {
      /* TODO */
      printf("help message");
      return 0;
    }
    input_filepath = string(argv[2]);

    if (argc < 5 || strcmp(argv[3], "-o") != 0) {
      /* TODO */
      printf("You must specify an output file name with the -o argument.");
      return 0;
    }
    output_filepath = string(argv[4]);
  }

  if (compile_mode) {
    input_file =
        open_file(input_filepath,
                  string("rb")); /* without binary mode, running on Windows
                                    under mingw fseek/fread were out of sync and
                                    was getting garbage at end of read */
    /* read the full file, otherwise a unclosed paren will just cause this to
     * hang like the REPL would */
    temp = read_file(input_file);
    OBJECT_TYPE(temp) = type_string;
    close_file(input_file);
    input_file = byte_stream_lift(temp);
    output_file = open_file(output_filepath, string("wb"));
    bc = compile_entire_file(input_file);
    write_bytecode_file(output_file, bc);
    close_file(output_file);
  } else if (interpret_mode) { /* add logic to compile the file if given a
                                  source file */
    input_file = open_file(input_filepath, string("rb"));
    bc = read_bytecode_file(input_file);
    DYNAMIC_ARRAY_LENGTH(gis->call_stack) = 0; /* clear the call stack */
    eval(bc, NIL);
    close_file(input_file);
  } else if (repl_mode) {
    input_file = file_stdin();
    output_file = file_stdout();
    while (1) {
      write_file(output_file, string("b> "));
      bc = compile(read(input_file, NIL), NIL, NIL, NIL);
      eval(bc, NIL);
      print(DYNAMIC_ARRAY_LENGTH(gis->data_stack) ? pop() : NIL);
    }
  }

  return 0;
}