#include "ffi.h"

ffi_type *ffi_type_designator_to_ffi_type(struct object *o, char within_another_struct) {
  struct object *lhs, *t, *rhs; 

  t = type_of(o);
  if (t == gis->symbol_type) {
    o = symbol_get_type(o);
    t = gis->type_type;
  }

  if (t == gis->cons_type) { /* must be of form (* <any>) */
    lhs = CONS_CAR(o);

    if (CONS_CDR(o) == NIL) {
      printf("Takes at least argument.");
      PRINT_STACK_TRACE_AND_QUIT();
    }

    rhs = CONS_CAR(CONS_CDR(o));

    if (lhs == gis->impl_pass_by_value_sym) {
      if (type_of(rhs) == gis->symbol_type) {
        rhs = symbol_get_type(rhs);
      }
      if (type_of(rhs) != gis->type_type) {
        printf("pass-by-value requires a type or symbol.\n");
        PRINT_STACK_TRACE_AND_QUIT();
      }
      return TYPE_FFI_TYPE(rhs);
    }

    print(o);
    printf("not impl\n");
    PRINT_STACK_TRACE_AND_QUIT();
  } else if (t == gis->type_type) {
    if (o == gis->char_type) return &ffi_type_schar;
    else if (o == gis->int_type) {
      return &ffi_type_sint;
    }
    else if (o == gis->uint_type) return &ffi_type_uint;
    else if (o == gis->int_type) return &ffi_type_sint;
    else if (o == gis->string_type) return &ffi_type_pointer;
    else if (o == gis->uint8_type) {
      return &ffi_type_uint8;
    }
    else if (o == gis->uint16_type) {
      return &ffi_type_uint16;
    }
    else if (o == gis->void_type) return &ffi_type_void;
    else if (o == gis->pointer_type) {
      return &ffi_type_pointer;
    } else if (o == gis->struct_type) {
      printf("Passing struct directly is not supported");
      print(o);
      PRINT_STACK_TRACE_AND_QUIT();
    } else if (TYPE_STRUCT_NFIELDS(o) > 0) {
      return &ffi_type_pointer;
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
