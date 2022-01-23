#include "ffi.h"

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
  struct object *lhs, *t, *t1; /* , *rhs; */

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
  } else if (t == gis->symbol_type) {
    t1 = symbol_get_type(o);
    if (t1 == gis->char_type) return &ffi_type_schar;
    else if (t1 == gis->int_type) {
      return &ffi_type_sint;
    }
    else if (t1 == gis->uint_type) return &ffi_type_uint;
    else if (t1 == gis->int_type) return &ffi_type_sint;
    else if (t1 == gis->uint8_type) {
      return &ffi_type_uint8;
    }
    else if (t1 == gis->void_type) return &ffi_type_void;
    else if (t1 == gis->pointer_type) {
      return &ffi_type_pointer;
    } else if (t1 == gis->struct_type) {
      printf("Passing struct directly is not supported");
      print(o);
      PRINT_STACK_TRACE_AND_QUIT();
    } else if (TYPE_STRUCT_NFIELDS(t1) > 0) {
      return TYPE_FFI_TYPE(o);
    } else {
      printf("Invalid FFI type designator symbol: \n");
      print(o);
      PRINT_STACK_TRACE_AND_QUIT();
    }
  } else {
    printf("Invalid FFI type designator: \n");
    print(o);
    print(type_of(o));
    PRINT_STACK_TRACE_AND_QUIT();
  }
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
