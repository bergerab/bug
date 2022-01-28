#include "ffi.h"

ffi_type *is_type_designator_a_pointer(struct object *o) {
  struct object *lhs, *rhs;

  if (type_of(o) != gis->cons_type) return NULL;

  lhs = CONS_CAR(o);

  if (CONS_CDR(o) == NIL) {
    printf("Takes at least argument.");
    PRINT_STACK_TRACE_AND_QUIT();
  }

  rhs = CONS_CAR(CONS_CDR(o));

  if (lhs == gis->type_pointer_sym) {
    if (type_of(rhs) == gis->symbol_type) {
      rhs = symbol_get_type(rhs);
    }
    if (type_of(rhs) != gis->type_type) {
      printf("Pointer requires a type parameter.\n");
      PRINT_STACK_TRACE_AND_QUIT();
    }
    return &ffi_type_pointer;
  }

  return NULL;
}

ffi_type *ffi_type_designator_to_ffi_type(struct object *o, char within_another_struct) {
  struct object *t; 
  ffi_type *temp;

  t = type_of(o);
  if (t == gis->symbol_type) {
    o = symbol_get_type(o);
    t = gis->type_type;
  }

  temp = is_type_designator_a_pointer(o);
  if (temp != NULL) { /* must be of form (* <any>) */
    return temp;
  } else if (t == gis->type_type) {
    if (o == gis->char_type) return &ffi_type_schar;
    else if (o == gis->int_type) {
      return &ffi_type_sint;
    }
    else if (o == gis->uint_type) return &ffi_type_uint;
    else if (o == gis->int_type) return &ffi_type_sint;
    else if (o == gis->string_type) return &ffi_type_pointer;
    else if (o == gis->uint8_type) return &ffi_type_uint8;
    else if (o == gis->uint16_type) return &ffi_type_uint16;
    else if (o == gis->uint32_type) return &ffi_type_uint32;
    else if (o == gis->void_type) return &ffi_type_void;
    else if (o == gis->pointer_type) {
      printf("Pointer takes one argument.\n");
      PRINT_STACK_TRACE_AND_QUIT();
    } else if (o == gis->struct_type) {
      printf("Passing struct directly is not supported");
      print(o);
      PRINT_STACK_TRACE_AND_QUIT();
    } else if (!TYPE_BUILTIN(o)) {
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
