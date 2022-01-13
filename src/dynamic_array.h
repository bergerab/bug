#ifndef _DYNAMIC_ARRAY_H
#define _DYNAMIC_ARRAY_H

#include "bug.h"

struct object *dynamic_array_get_ufixnum_t(struct object *da, ufixnum_t index);
struct object *dynamic_array_get(struct object *da, struct object *index);
struct object *dynamic_array_set_ufixnum_t(struct object *da, ufixnum_t index, struct object *value);
void dynamic_array_set(struct object *da, struct object *index, struct object *value);
struct object *dynamic_array_length(struct object *da);
void dynamic_array_ensure_capacity(struct object *da);
void dynamic_array_push(struct object *da, struct object *value);
struct object *dynamic_array_pop(struct object *da);
struct object *dynamic_array_concat(struct object *da0, struct object *da1);

#endif