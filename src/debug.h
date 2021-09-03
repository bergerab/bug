/* All code that is only run during debugging/development
   e.g. checking types and checking stack sizes */

#define RUN_TIME_CHECKS

#ifdef RUN_TIME_CHECKS
#define TC(name, argument, o, type) type_check(name, argument, o, type)
#define SC(g, name, n) stack_check(g, name, n)
#else
#define TC(name, argument, o, type) \
  {}
#define SC(g, name, n) \
  {}
#endif

#define NC(x, message) \
  if (x == NULL) {     \
    printf(message);   \
    return NULL;       \
  }

#ifdef RUN_TIME_CHECKS
char *type_to_name(enum type t) {
  switch (t) {
  case type_cons:
    return "cons";
  case type_fixnum:
    return "fixnum";
  case type_flonum:
    return "flonum";
  case type_symbol:
    return "symbol";
  case type_array:
    return "array";
  case type_package:
    return "package";
  case type_string:
    return "string";
  case type_bytecode:
    return "bytecode";
  case type_byte_array:
    return "byte-array";
  }
  printf("Bug Bytecode: Given type number has no implemented name %d", t);
  exit(1);
} 

void type_check(char *name, unsigned int argument, struct object *o,
                enum type type) {
  if (o->w0.type != type) {
    printf(
        "Bug Bytecode: Function \"%s\" was called with an invalid argument "
        "at index %d. Expected type %s, but was %s.",
        name, argument, type_to_name(type), type_to_name(o->w0.type));
    exit(1);
  }
}

fixnum_t count(struct object *list) {
  fixnum_t count = 0;
  while (list != 0) {
    ++count;
    list = list->w1.cdr;
  }
  return count;
}

void stack_check(struct gis *g, char *name, fixnum_t n) {
  fixnum_t stack_count = count(g->stack);
  if (stack_count < n) {
    printf(
        "Bug Bytecode: Operation \"%s\" was called when the stack had too "
        "few items. Expected %d items on the stack, but were %d.",
        name, n, stack_count);
    exit(1);
  }
}
#endif