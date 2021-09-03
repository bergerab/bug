#include <stdlib.h>
#include <stdio.h>

/* fix and flo should be the same size, and be less than or equal to the machine's word size. 
 * this way all values can take up two words. */
#define nil 0

typedef int   fix_t;
typedef float flo_t;

/* The types defined below are not the same as the types that will
   be defined within the language. */
/* keeps the two right most bits as 0 to keep it as a flag */
enum type {
  type_number = 4,
  type_function = 8,
  type_package = 12,
  type_symbol = 16,
  type_array = 20,
  /* don't need nil type because the whole value would be equal to 0 */
};

union value {
  fix_t fixnum; /** Whole number */
  flo_t flonum; /** Floating point number */
  struct package *package; /** Package */
  struct function *function; /** Function */
  struct macro *macro; /** Macro */
  struct symbol *symbol; /** Symbol */
};

/* Each object takes up two words. 
 * 
 * The first word is either the type or the car (a pointer to another object).
 * The second word is either a val or the cdr (a pointer to another object).
 * 
 * The only two valid combinations are having a car/cdr or type/val.
 *   - car/cdr are what is known as a "cons cell"
 *   - type/val is just an ordinary value such as the number 5 or the string "23"
 * 
 * car/cdr values are differentiated from the type/val values by the LSB in the car. 
 * It should be flipped to 1. This means all types must not use the LSB. 
 * Types could be: 0000, 0010, 0100, 0110, 1000, etc...
*/
union w0 { /** The first word of an object. */
  struct obj *type;
  struct obj *car;
};

union w1 { /** The second word of an object */
  union value value;
  struct obj *cdr;
};

struct object {
  union w0 w0;
  union w1 w1;
};

struct symbol { /** A symbol */
  char *name; /** The name of the symbol */
  struct object *values; /** a list from namespace to value */
};

struct array {
  struct object *length;
  struct object **values;
};

struct function { /** A function */
  struct object *name; /** */
  struct object *params; /** the methods's parameters */
  struct object *body; /** the body of the method */
  struct object *doc; /** the doc string */
};

struct package { /** A package */
  struct obj *symbol; /** All defined symbols */
  struct obj *types;
};

struct array { /** */
  struct obj *length; /** */
  struct obj **items;
};

struct gis { /** The global interpretor state */
  struct obj *package; /** The active module (any definitions will add to this module) */
};

/* Call - regular cons list */
/* Continuation - regular cons list */

struct obj *alloc_obj(struct obj *type) {
  struct obj *obj = malloc(sizeof(struct obj));
  obj->wd0.ctp = ctp;
  return obj;
}

struct obj *cons(struct obj *car, struct obj *cdr) {
  struct obj *obj = alloc_obj(type_cons);
  obj->wd0.car = car;
  obj->wd1.cdr = cdr;
  return obj;
}

/**
 * Allocates a whole number.
 * 
 * @param fix the C number to lift into a fix object
 * @returns a fix object
 */
struct obj *fixnum(fix_t fix) {
  struct obj *obj = alloc_obj(type_fix);
  obj->wd1.val.fix = fix;
  return obj;
}

/**
 * Allocates a floating point number.
 * 
 * @param flo the C number to lift into a flo object
 * @returns a flo object
 */
struct obj *flonum(flo_t flo) {
  struct obj *obj = alloc_obj(type_flo);
  obj->wd1.val.flo = flo;
  return obj;
}

struct sym *intern(struct mod *mod, char *name) {
  struct sym *sym = malloc(sizeof(struct sym));
  sym->name = name;
  sym->val = nil;
  sym->fun = nil;
  sym->atp = nil;
  return sym;
}

struct obj *sym(char *name, struct obj *val, struct obj *fun, struct obj *atp, struct obj *mod) {
  struct obj *obj = alloc_obj(type_sym);

  /* TODO: intern symbols*/
  struct sym *sym = malloc(sizeof(struct sym));
  sym->name = name;
  sym->fun = fun;
  sym->val = val;
  sym->atp = atp;
  sym->mod = mod;

  obj->wd1.val.sym = sym;

  /* link the ATP back to the symbol (circular reference! make sure GC is OK with this) */
  atp->wd1.val.atp->sym = obj;
  return obj;
}

/**
 */
struct obj *call_fun(struct gis *gis, struct obj *fun, struct obj *args) {

}

/** Creates a new module */
struct obj *alloc_mod() {
  struct mod *mod = malloc(sizeof(struct mod));
  struct obj *obj = alloc_obj(type_mod);
  obj->wd1.val.mod = mod;
  return obj;
}

struct obj *alloc_default_mod() {
  struct obj *mod = alloc_mod();
  mod->wd1.val.mod->syms =
    cons(sym("syms", mod->syms, nil, nil), 
    cons(sym("mod", mod, nil, nil),
    cons(sym("str", nil, nil, nil),
    cons(sym("", nil, nil, nil), nil)));
  return mod;
}

/** 
 * Creates the default global interpretor state.
 */
struct gis *alloc_gis() {
  struct gis *gis = malloc(sizeof(struct gis));
  struct mod *mod = default_mod();
  gis->mod = mod;
}

/**
 * Looks up a symbol 
 * 
 * Looks in the current module
 */
void lookup(struct gis *gis, struct obj *sym) {

}

void eval() {

}

/*
 * Types
 * atp
 * ctp
 * sym
 * num
 * fix - num
 * flo - num
 * cns
 */

/* trampoline function */
void tramp() {
  struct obj *call;
  while (1)
    call = call_cont();
}