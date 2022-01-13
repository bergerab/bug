#ifndef _STRING_H
#define _STRING_H

#include "bug.h"

struct object *to_string(struct object *o);
struct object *to_repr(struct object *o);
struct object *to_string_ufixnum_t(ufixnum_t n);
struct object *to_string_fixnum_t(fixnum_t n);
struct object *to_string_flonum_t(flonum_t n);
struct object *to_string_dynamic_byte_array(struct object *dba);
struct object *to_string_vec2(struct object *vec2);
struct object *to_string_dynamic_array(struct object *da);
struct object *do_to_string(struct object *o, char repr);

void string_reverse(struct object *o);
struct object *string_concat(struct object *s0, struct object *s1);
struct object *string_clone(struct object *str0);

struct object *string_designator(struct object *sd);

#endif