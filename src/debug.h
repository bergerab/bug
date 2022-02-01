#ifndef _DEBUG_H
#define _DEBUG_H

#include "bug.h"

#define DEBUG 1
#define IF_DEBUG() if (DEBUG)
#define RUN_TIME_CHECKS

#ifdef RUN_TIME_CHECKS
#define TC(name, argument, o, type) type_check(name, argument, o, type)
#define OT(name, argument, o, type) object_type_check(name, argument, o, type)
#define TC2(name, argument, o, type0, type1) type_check_or2(name, argument, o, type0, type1)
#define OT2(name, argument, o, type0, type1) object_type_check_or2(name, argument, o, type0, type1)
#define OT_LIST(name, argument, o) object_type_check_list(name, argument, o)
#define SC(name, n) stack_check(name, n, UFIXNUM_VALUE(i))
#else
#define TC(name, argument, o, type) \
  {}
#define TC2(name, argument, o, type0, type1) \
  {}
#define SC(name, n) \
  {}
#define OT(name, n) \
  {}
#define OT2(name, n) \
  {}
#endif

#define FUNCTION_TRACE 0

void type_check(char *name, unsigned int argument, struct object *o,
                struct object *t);
void type_check_or2(char *name, unsigned int argument, struct object *o,
                    struct object *t0, struct object *t1);
void stack_check(char *name, int n, unsigned long i);
void object_type_check_list(char *name, unsigned int argument, struct object *o);
void object_type_check(char *name, unsigned int argument, struct object *o,
                enum object_type t);
void object_type_check_or2(char *name, unsigned int argument, struct object *o,
                enum object_type t0, enum object_type t1);

#endif