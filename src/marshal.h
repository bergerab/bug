#ifndef _MARSHAL_H
#define _MARSHAL_H

#include "bug.h"

#define BC_VERSION 1

enum marshaled_type {
  marshaled_type_integer,
  marshaled_type_negative_integer,
  marshaled_type_float,
  marshaled_type_negative_float,
  marshaled_type_symbol,
  marshaled_type_uninterned_symbol, /** a symbol with no home package */
  marshaled_type_string,
  marshaled_type_nil,
  marshaled_type_cons,
  marshaled_type_dynamic_array,
  marshaled_type_dynamic_string_array,
  marshaled_type_dynamic_byte_array,
  marshaled_type_function,
  marshaled_type_vec2
};

struct object *string_marshal_cache_get_default();
struct object *string_marshal_cache_intern_cstr(struct object *cache, char *str, ufixnum_t *index);
struct object *string_marshal_cache_intern(struct object *cache, struct object *str, ufixnum_t *index);

struct object *marshal(struct object *o, struct object *ba, struct object *cache);
struct object *marshal_vec2(struct object *vec2, struct object *ba, char include_header);
struct object *marshal_function(struct object *bc, struct object *ba, char include_header, struct object *cache);
struct object *marshal_flonum(flonum_t n, struct object *ba);
struct object *marshal_16_bit_fix(fixnum_t n, struct object *ba);
struct object *marshal_ufixnum(struct object *n, struct object *ba, char include_header);
struct object *marshal_dynamic_string_array(struct object *arr, struct object *ba, char include_header, struct object *cache, ufixnum_t start_index);
struct object *marshal_dynamic_array(struct object *arr, struct object *ba, char include_header, struct object *cache);
struct object *marshal_cons(struct object *o, struct object *ba, char include_header, struct object *cache);
struct object *marshal_nil(struct object *ba);
struct object *marshal_dynamic_byte_array(struct object *ba0, struct object *ba, char include_header);
struct object *marshal_symbol(struct object *sym, struct object *ba, struct object *cache);
struct object *marshal_string(struct object *str, struct object *ba, char include_header, struct object *cache);
struct object *marshal_ufixnum_t(ufixnum_t n, struct object *ba, char include_header);
struct object *marshal_fixnum(struct object *n, struct object *ba);
struct object *marshal_fixnum_t(fixnum_t n, struct object *ba);

struct object *unmarshal(struct object *s, struct object *cache);
struct object *unmarshal_nil(struct object *s);
struct object *unmarshal_function(struct object *s, char includes_header, struct object *cache);
struct object *unmarshal_vec2(struct object *s, char includes_header);
struct object *unmarshal_dynamic_string_array(struct object *s, char includes_header, struct object *cache);
struct object *unmarshal_dynamic_array(struct object *s, char includes_header, struct object *cache);
struct object *unmarshal_dynamic_byte_array(struct object *s, char includes_header);
struct object *unmarshal_cons(struct object *s, char includes_header, struct object *cache);
struct object *unmarshal_symbol(struct object *s, struct object *cache);
struct object *unmarshal_string(struct object *s, char includes_header, struct object *cache, char clone_from_cache);
struct object *unmarshal_float(struct object *s);
flonum_t unmarshal_float_t(struct object *s);
struct object *unmarshal_integer(struct object *s);
fixnum_t unmarshal_16_bit_fix(struct object *s);
ufixnum_t unmarshal_ufixnum_t(struct object *s);

struct object *read_bytecode_file(struct object *s);
void write_bytecode_file(struct object *file, struct object *bc);

struct object *byte_stream_lift(struct object *e);
char byte_stream_has(struct object *e);
struct object *byte_stream_read(struct object *e, fixnum_t n);
struct object *byte_stream_peek(struct object *e, fixnum_t n);
char byte_stream_read_byte(struct object *e);
char byte_stream_peek_byte(struct object *e);

#endif