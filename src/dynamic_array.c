#include "dynamic_array.h"

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