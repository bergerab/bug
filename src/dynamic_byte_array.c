#include "dynamic_byte_array.h"

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