#include "debug.h"

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

