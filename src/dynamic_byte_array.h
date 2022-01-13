#ifndef _DYNAMIC_BYTE_ARRAY_H
#define _DYNAMIC_BYTE_ARRAY_H

#include "bug.h"

char dynamic_byte_array_get(struct object *dba, fixnum_t index);
void dynamic_byte_array_set(struct object *dba, ufixnum_t index, char value);
struct object *dynamic_byte_array_length(struct object *dba);
void dynamic_byte_array_ensure_capacity(struct object *dba);
struct object *dynamic_byte_array_push(struct object *dba, struct object *value);
void dynamic_byte_array_push_char(struct object *dba, char x);
void dynamic_byte_array_force_cstr(struct object *dba);
struct object *dynamic_byte_array_push_all(struct object *dba0, struct object *dba1);
void dynamic_byte_array_insert_char(struct object *dba, ufixnum_t i, char x);
struct object *dynamic_byte_array_pop(struct object *dba);
struct object *dynamic_byte_array_concat(struct object *dba0, struct object *dba1);
struct object *dynamic_byte_array_from_array(ufixnum_t length, unsigned char *arr);

struct object *string_concat(struct object *s0, struct object *s1);
struct object *string_clone(struct object *str0);

#endif