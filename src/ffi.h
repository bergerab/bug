#ifndef _FFI_H
#define _FFI_H

#include "bug.h"

ffi_type *ffi_type_designator_to_ffi_type(struct object *o, char within_another_struct);
ffi_type *struct_ffi_type_designator_to_ffi_type(struct object *o);
char ffi_type_designator_is_string(struct object *o);
char ffi_type_designator_is_struct(struct object *o);
ffi_type *is_type_designator_a_pointer(struct object *o);

#endif