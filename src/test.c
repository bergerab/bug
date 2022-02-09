/*===============================*
 *===============================*
 * Tests                         *
 *===============================*
 *===============================*/
void reinit(char load_core) {
  free(gis); /* TODO: write gis_free */
  gis_init(load_core);
}

/* Unused for now... */
void run_tests() {
  struct object *darr, *o0, *o1, *dba, *dba1, *bc, *da,
      *code0, *consts0, *code1, *consts1;
  ufixnum_t uf0;

  printf("============ Running tests... =============\n");

#define assert_string_eq(str1, str2) \
  assert(strcmp(bstring_to_cstring(str1), bstring_to_cstring(str2)) == 0);

#define END_TESTS()                                        \
  printf("============ Tests were successful ========\n"); \
  return;

  /*
   * String - bug string to c string
   */
  assert(strcmp(bstring_to_cstring(string("asdf")), "asdf") == 0);
  assert(strcmp(bstring_to_cstring(string("")), "") == 0);

  /*
   * Dynamic Array
   */
  darr = dynamic_array(2);
  assert(FIXNUM_VALUE(dynamic_array_length(darr)) == 0);
  dynamic_array_push(darr, fixnum(5));
  assert(FIXNUM_VALUE(dynamic_array_length(darr)) == 1);
  assert(FIXNUM_VALUE(dynamic_array_get(darr, fixnum(0))) == 5);
  dynamic_array_push(darr, fixnum(9));
  assert(FIXNUM_VALUE(dynamic_array_length(darr)) == 2);
  assert(FIXNUM_VALUE(dynamic_array_get(darr, fixnum(1))) == 9);
  assert(DYNAMIC_ARRAY_CAPACITY(darr) == 2);
  dynamic_array_push(darr, fixnum(94));
  assert(FIXNUM_VALUE(dynamic_array_length(darr)) == 3);
  assert(FIXNUM_VALUE(dynamic_array_get(darr, fixnum(2))) == 94);
  assert(DYNAMIC_ARRAY_CAPACITY(darr) == 4);
  dynamic_array_push(darr, fixnum(111));
  assert(FIXNUM_VALUE(dynamic_array_length(darr)) == 4);
  assert(FIXNUM_VALUE(dynamic_array_get(darr, fixnum(3))) == 111);
  assert(DYNAMIC_ARRAY_CAPACITY(darr) == 4);
  dynamic_array_push(darr, fixnum(3));
  assert(FIXNUM_VALUE(dynamic_array_length(darr)) == 5);
  assert(FIXNUM_VALUE(dynamic_array_get(darr, fixnum(4))) == 3);
  assert(DYNAMIC_ARRAY_CAPACITY(darr) == 7);

  /*
   * Dynamic byte Array
   */
  dba = dynamic_byte_array(2);
  assert(FIXNUM_VALUE(dynamic_byte_array_length(dba)) == 0);
  dynamic_byte_array_push(dba, fixnum(5));
  assert(FIXNUM_VALUE(dynamic_byte_array_length(dba)) == 1);
  assert(dynamic_byte_array_get(dba, 0) == 5);
  dynamic_byte_array_push(dba, fixnum(9));
  assert(FIXNUM_VALUE(dynamic_byte_array_length(dba)) == 2);
  assert(dynamic_byte_array_get(dba, 1) == 9);
  assert(DYNAMIC_BYTE_ARRAY_CAPACITY(dba) == 2);
  dynamic_byte_array_push(dba, fixnum(94));
  assert(FIXNUM_VALUE(dynamic_byte_array_length(dba)) == 3);
  assert(dynamic_byte_array_get(dba, 2) == 94);
  assert(DYNAMIC_BYTE_ARRAY_CAPACITY(dba) == 4);

  dynamic_byte_array_insert_char(dba, 0, 96);
  assert(UFIXNUM_VALUE(dynamic_byte_array_length(dba)) == 4);
  assert(dynamic_byte_array_get(dba, 0) == 96);
  assert(dynamic_byte_array_get(dba, 1) == 5);
  assert(dynamic_byte_array_get(dba, 2) == 9);
  assert(dynamic_byte_array_get(dba, 3) == 94);

  dynamic_byte_array_insert_char(dba, 2, 99);
  assert(UFIXNUM_VALUE(dynamic_byte_array_length(dba)) == 5);
  assert(dynamic_byte_array_get(dba, 0) == 96);
  assert(dynamic_byte_array_get(dba, 1) == 5);
  assert(dynamic_byte_array_get(dba, 2) == 99);
  assert(dynamic_byte_array_get(dba, 3) == 9);
  assert(dynamic_byte_array_get(dba, 4) == 94);

  dba = dynamic_byte_array(2);
  dynamic_byte_array_insert_char(dba, 0, 11);
  assert(UFIXNUM_VALUE(dynamic_byte_array_length(dba)) == 1);
  assert(dynamic_byte_array_get(dba, 0) == 11);

  dynamic_byte_array_insert_char(dba, 1, 32);
  assert(UFIXNUM_VALUE(dynamic_byte_array_length(dba)) == 2);
  assert(dynamic_byte_array_get(dba, 0) == 11);
  assert(dynamic_byte_array_get(dba, 1) == 32);

  dba = dynamic_byte_array(2);
  dynamic_byte_array_push_char(dba, 4);
  dynamic_byte_array_push_char(dba, 9);
  dba1 = dynamic_byte_array(2);
  dynamic_byte_array_push_char(dba1, 77);
  dynamic_byte_array_push_char(dba1, 122);
  dba = dynamic_byte_array_concat(dba, dba1);
  assert(UFIXNUM_VALUE(dynamic_byte_array_length(dba)) == 4);
  assert(DYNAMIC_BYTE_ARRAY_CAPACITY(dba) == 4);
  assert(dynamic_byte_array_get(dba, 0) == 4);
  assert(dynamic_byte_array_get(dba, 1) == 9);
  assert(dynamic_byte_array_get(dba, 2) == 77);
  assert(dynamic_byte_array_get(dba, 3) == 122);

  /*
   * Marshaling/Unmarshaling
   */

  /* Fixnum marshaling */
  reinit(0);

#define T_MAR_FIXNUM(n) \
  assert(FIXNUM_VALUE(unmarshal_integer(marshal_fixnum(fixnum(n), NULL))) == n);
#define T_MAR_UFIXNUM(n) \
  assert(UFIXNUM_VALUE(  \
             unmarshal_integer(marshal_ufixnum(ufixnum(n), NULL, 1))) == n);
#define T_MAR_FLONUM(n) \
  assert(FLONUM_VALUE(unmarshal_float(marshal_flonum(n, NULL))) == n);
#define T_MAR_STR(x)                                                          \
  assert_string_eq(                                                           \
      unmarshal_string(marshal_string(string(x), NULL, 1, NULL), 1, NULL, 1), \
      string(x));
  /* test marshaling with the default package */
#define T_MAR_SYM_DEF(x)                                                 \
  assert(unmarshal_symbol(                                               \
             marshal_symbol(intern(string(x), GIS_PACKAGE), NULL, NULL), \
             NULL) == intern(string(x), GIS_PACKAGE));
#define T_MAR_SYM(x, p)                                                       \
  assert(unmarshal_symbol(                                                    \
             marshal_symbol(intern(string(x), find_package(string(p))), NULL, \
                            NULL),                                            \
             NULL) == intern(string("dinkle"), find_package(string(p))));
#define T_MAR(x) assert(equals(unmarshal(marshal(x, NULL, NULL), NULL), x));

  assert(unmarshal_ufixnum_t(marshal_ufixnum_t(1, NULL, 0)) == 1);
  /* three bytes long */
  assert(unmarshal_ufixnum_t(marshal_ufixnum_t(122345, NULL, 0)) == 122345);
  assert(unmarshal_ufixnum_t(marshal_ufixnum_t(1223450, NULL, 0)) == 1223450);
  /* four bytes long */
  assert(unmarshal_ufixnum_t(marshal_ufixnum_t(122345000, NULL, 0)) ==
         122345000);

  /* no bytes */
  T_MAR_FIXNUM(0);
  /* one byte */
  T_MAR_FIXNUM(9);
  T_MAR_FIXNUM(-23);
  T_MAR_FIXNUM(-76);
  T_MAR(fixnum(
      -76)); /* make sure it is included in main marshal/unmarshal switches */
  /* two bytes */
  T_MAR_FIXNUM(256);
  T_MAR_FIXNUM(257);
  T_MAR_FIXNUM(-342);
  T_MAR_FIXNUM(2049);
  /* three bytes */
  T_MAR_FIXNUM(123456);
  T_MAR_FIXNUM(-123499);
  T_MAR_FIXNUM(20422);
  /* four bytes */
  T_MAR_FIXNUM(123456789);
  T_MAR_FIXNUM(-123456789);
  /* Should test fixnums that are from platforms that have larger word sizes
   * (e.g. read in a fixnum larger than four bytes on a machine with two byte
   * words) */
  T_MAR_UFIXNUM(MAX_UFIXNUM);

  /* flonum marshaling */
  T_MAR_FLONUM(0.0);
  T_MAR_FLONUM(1.0);
  T_MAR_FLONUM(1.23);
  T_MAR_FLONUM(1e23);
  T_MAR_FLONUM(1e-23);
  T_MAR_FLONUM(123456789123456789123456789123456789.0);
  T_MAR_FLONUM(-0.0);
  T_MAR_FLONUM(-1.23);

  /* TODO: test that NaN and +/- Infinity work */

  /* string marshaling */
  T_MAR_STR("abcdef");
  T_MAR_STR("nerEW)F9nef09n230(N#EOINWEogiwe");
  T_MAR_STR("");

  reinit(0);
  T_MAR_SYM_DEF("abcdef");

  /* uninterned symbol */
  o0 =
      unmarshal_symbol(marshal_symbol(symbol(string("fwe")), NULL, NULL), NULL);
  assert(SYMBOL_PACKAGE(o0) == NIL);
  add_package(package(string("peep")));
  T_MAR_SYM("dinkle", "peep");
  /* make sure the symbol isn't interned into the wrong package */
  assert(unmarshal_symbol(marshal_symbol(intern(string("dinkle"),
                                                find_package(string("peep"))),
                                         NULL, NULL),
                          NULL) != intern(string("dinkle"), GIS_PACKAGE));

  /* nil marshaling */
  assert(unmarshal_nil(marshal_nil(NULL)) == NIL);

  /* cons filled with nils */
  o0 = unmarshal_cons(marshal_cons(cons(NIL, NIL), NULL, 1, NULL), 1, NULL);
  assert(CONS_CAR(o0) == NIL);
  assert(CONS_CDR(o0) == NIL);
  /* cons with strings */
  o0 = unmarshal_cons(
      marshal_cons(cons(string("A"), string("B")), NULL, 1, NULL), 1, NULL);
  assert(strcmp(bstring_to_cstring(CONS_CAR(o0)), "A") == 0);
  assert(strcmp(bstring_to_cstring(CONS_CDR(o0)), "B") == 0);
  /* cons list with fixnums */
  o0 = unmarshal_cons(
      marshal_cons(cons(fixnum(35), cons(fixnum(99), NIL)), NULL, 1, NULL), 1,
      NULL);
  assert(FIXNUM_VALUE(CONS_CAR(o0)) == 35);
  assert(FIXNUM_VALUE(CONS_CAR(CONS_CDR(o0))) == 99);
  assert(CONS_CDR(CONS_CDR(o0)) == NIL);

  /* marshal/unmarshal dynamic byte array */
  dba = dynamic_byte_array(10);
  dynamic_byte_array_push_char(dba, 54);
  dynamic_byte_array_push_char(dba, 99);
  dynamic_byte_array_push_char(dba, 23);
  dynamic_byte_array_push_char(dba, 9);
  o0 =
      unmarshal_dynamic_byte_array(marshal_dynamic_byte_array(dba, NULL, 1), 1);
  assert(DYNAMIC_BYTE_ARRAY_LENGTH(dba) == DYNAMIC_BYTE_ARRAY_LENGTH(o0));
  assert(strcmp(bstring_to_cstring(dba), bstring_to_cstring(o0)) == 0);

#define T_DBA_PUSH(n) dynamic_byte_array_push_char(dba, n);

  dba = dynamic_byte_array(10);
  T_DBA_PUSH(0);
  T_DBA_PUSH(1);
  T_DBA_PUSH(2);
  T_DBA_PUSH(3);
  T_DBA_PUSH(4);
  T_DBA_PUSH(5);
  T_DBA_PUSH(6);
  T_DBA_PUSH(7);
  T_DBA_PUSH(8);
  T_DBA_PUSH(9);
  T_DBA_PUSH(10);
  T_DBA_PUSH(0);
  T_DBA_PUSH(12);
  T_DBA_PUSH(13);
  T_DBA_PUSH(14);
  T_DBA_PUSH(15);
  T_DBA_PUSH(16);
  T_DBA_PUSH(17);
  T_DBA_PUSH(18);
  T_DBA_PUSH(19);
  T_DBA_PUSH(20);
  T_DBA_PUSH(21);
  T_DBA_PUSH(22);
  T_DBA_PUSH(23);
  T_DBA_PUSH(24);
  T_DBA_PUSH(25);
  T_DBA_PUSH(26);
  T_DBA_PUSH(27);
  T_DBA_PUSH(28);
  T_DBA_PUSH(29);
  T_DBA_PUSH(30);
  T_DBA_PUSH(31);
  T_DBA_PUSH(32);
  T_DBA_PUSH(33);
  T_DBA_PUSH(34);
  T_DBA_PUSH(35);
  T_DBA_PUSH(36);
  T_DBA_PUSH(37);
  T_DBA_PUSH(38);
  T_DBA_PUSH(39);
  T_DBA_PUSH(40);
  o0 =
      unmarshal_dynamic_byte_array(marshal_dynamic_byte_array(dba, NULL, 1), 1);
  assert(DYNAMIC_BYTE_ARRAY_LENGTH(dba) == DYNAMIC_BYTE_ARRAY_LENGTH(o0));
  assert(equals(dba, o0));

  /* marshal/unmarshal dynamic array */
#define T_DA_PUSH(n) dynamic_array_push(darr, n);

  darr = dynamic_array(10);
  dynamic_array_push(darr, fixnum(3));
  dynamic_array_push(darr, string("e2"));
  dynamic_array_push(darr, NIL);
  o0 = unmarshal_dynamic_array(marshal_dynamic_array(darr, NULL, 1, NULL), 1,
                               NULL);
  assert(DYNAMIC_ARRAY_LENGTH(darr) == DYNAMIC_ARRAY_LENGTH(o0));
  assert(FIXNUM_VALUE(DYNAMIC_ARRAY_VALUES(darr)[0]) == 3);
  assert(strcmp(bstring_to_cstring(DYNAMIC_ARRAY_VALUES(darr)[1]), "e2") == 0);
  assert(DYNAMIC_ARRAY_VALUES(darr)[2] == NIL);

  darr = dynamic_array(10);
  T_DA_PUSH(fixnum(0));
  T_DA_PUSH(fixnum(1));
  T_DA_PUSH(fixnum(2));
  T_DA_PUSH(fixnum(3));
  T_DA_PUSH(fixnum(4));
  T_DA_PUSH(fixnum(5));
  T_DA_PUSH(fixnum(6));
  T_DA_PUSH(fixnum(7));
  T_DA_PUSH(fixnum(8));
  T_DA_PUSH(fixnum(9));
  T_DA_PUSH(fixnum(10));
  T_DA_PUSH(fixnum(11));
  T_DA_PUSH(fixnum(12));
  T_DA_PUSH(fixnum(13));
  T_DA_PUSH(fixnum(14));
  T_DA_PUSH(fixnum(15));
  T_DA_PUSH(fixnum(16));
  T_DA_PUSH(fixnum(17));
  T_DA_PUSH(fixnum(18));
  T_DA_PUSH(fixnum(19));
  T_DA_PUSH(fixnum(20));
  T_DA_PUSH(fixnum(21));
  T_DA_PUSH(fixnum(22));
  T_DA_PUSH(fixnum(23));
  T_DA_PUSH(fixnum(24));
  T_DA_PUSH(fixnum(25));
  T_DA_PUSH(fixnum(26));
  T_DA_PUSH(fixnum(27));
  T_DA_PUSH(fixnum(28));
  T_DA_PUSH(fixnum(29));
  T_DA_PUSH(fixnum(30));
  T_DA_PUSH(fixnum(31));
  o0 = unmarshal_dynamic_array(marshal_dynamic_array(darr, NULL, 1, NULL), 1,
                               NULL);
  assert(equals(darr, o0));

  /* marshal function */
  darr = dynamic_array(10); /* constants vector */
  dynamic_array_push(darr, string("blackhole"));
  dynamic_array_push(darr, flonum(3.234));
  dynamic_array_push(darr, ufixnum(234234234));
  dba = dynamic_byte_array(10); /* the code vector */
  dynamic_byte_array_push_char(dba, 0x62);
  dynamic_byte_array_push_char(dba, 0x39);
  dynamic_byte_array_push_char(dba, 0x13);
  dynamic_byte_array_push_char(dba, 0x23);
  bc = function(darr, dba, 12); /* bogus stack size */
  o0 = unmarshal_function(marshal_function(bc, NULL, 1, NULL), 1, NULL);
  /* check constants vector */
  assert(DYNAMIC_ARRAY_LENGTH(darr) ==
         DYNAMIC_ARRAY_LENGTH(FUNCTION_CONSTANTS(o0)));
  assert(strcmp(bstring_to_cstring(
                    DYNAMIC_ARRAY_VALUES(FUNCTION_CONSTANTS(o0))[0]),
                "blackhole") == 0);
  assert(FLONUM_VALUE(DYNAMIC_ARRAY_VALUES(FUNCTION_CONSTANTS(o0))[1]) ==
         3.234);
  assert(UFIXNUM_VALUE(DYNAMIC_ARRAY_VALUES(FUNCTION_CONSTANTS(o0))[2]) ==
         234234234);
  /* check code vector */
  assert(DYNAMIC_BYTE_ARRAY_LENGTH(dba) ==
         DYNAMIC_BYTE_ARRAY_LENGTH(FUNCTION_CODE(o0)));
  assert(strcmp(bstring_to_cstring(dba),
                bstring_to_cstring(FUNCTION_CODE(o0))) == 0);

  darr = dynamic_array(10); /* constants vector */
  T_DA_PUSH(intern(string("g"), gis->user_package));
  T_DA_PUSH(fixnum(1));
  T_DA_PUSH(fixnum(2));
  T_DA_PUSH(fixnum(3));
  T_DA_PUSH(fixnum(4));
  T_DA_PUSH(intern(string("t"), gis->lisp_package));
  T_DA_PUSH(string("g = "));
  T_DA_PUSH(intern(string("g"), gis->user_package));
  T_DA_PUSH(string("wow it works!"));
  T_DA_PUSH(fixnum(7));
  T_DA_PUSH(fixnum(3));
  T_DA_PUSH(fixnum(2));
  T_DA_PUSH(fixnum(9));
  T_DA_PUSH(string("didn't work"));
  T_DA_PUSH(string("fff"));
  dba = dynamic_byte_array(10); /* the code vector */
  T_DBA_PUSH(0x15);
  T_DBA_PUSH(0x00);
  T_DBA_PUSH(0x15);
  T_DBA_PUSH(0x01);
  T_DBA_PUSH(0x15);
  T_DBA_PUSH(0x02);
  T_DBA_PUSH(0x11);
  T_DBA_PUSH(0x15);
  T_DBA_PUSH(0x03);
  T_DBA_PUSH(0x11);
  T_DBA_PUSH(0x15);
  T_DBA_PUSH(0x04);
  T_DBA_PUSH(0x11);
  T_DBA_PUSH(0x1F);
  T_DBA_PUSH(0x00);
  T_DBA_PUSH(0x15);
  T_DBA_PUSH(0x05);
  T_DBA_PUSH(0x20);
  T_DBA_PUSH(0x22);
  T_DBA_PUSH(0x00);
  T_DBA_PUSH(0x1A);
  T_DBA_PUSH(0x15);
  T_DBA_PUSH(0x06);
  T_DBA_PUSH(0x16);
  T_DBA_PUSH(0x15);
  T_DBA_PUSH(0x07);
  T_DBA_PUSH(0x20);
  T_DBA_PUSH(0x16);
  T_DBA_PUSH(0x15);
  T_DBA_PUSH(0x08);
  T_DBA_PUSH(0x16);
  T_DBA_PUSH(0x15);
  T_DBA_PUSH(0x09);
  T_DBA_PUSH(0x15);
  T_DBA_PUSH(0x0A);
  T_DBA_PUSH(0x12);
  T_DBA_PUSH(0x15);
  T_DBA_PUSH(0x0B);
  T_DBA_PUSH(0x12);
  T_DBA_PUSH(0x15);
  T_DBA_PUSH(0x0C);
  T_DBA_PUSH(0x12);
  T_DBA_PUSH(0x16);
  T_DBA_PUSH(0x21);
  T_DBA_PUSH(0x00);
  T_DBA_PUSH(0x08);
  T_DBA_PUSH(0x15);
  T_DBA_PUSH(0x0D);
  T_DBA_PUSH(0x16);
  T_DBA_PUSH(0x00);
  T_DBA_PUSH(0x15);
  T_DBA_PUSH(0x0E);
  bc = function(darr, dba, 8); /* bogus stack size */
  o0 = unmarshal_function(marshal_function(bc, NULL, 1, NULL), 1, NULL);
  assert(equals(bc, o0));

  /* string cache */
  o0 = string_marshal_cache_get_default();
  assert(string_marshal_cache_intern_cstr(o0, "lisp", &uf0) ==
         gis->lisp_str);
  assert(string_marshal_cache_intern_cstr(o0, "lisp", &uf0) != string("lisp"));
  assert(string_marshal_cache_intern_cstr(o0, "mips", &uf0) ==
         string_marshal_cache_intern_cstr(o0, "mips", &uf0));
  string_marshal_cache_intern_cstr(o0, "mips", &uf0);
  assert(uf0 == DYNAMIC_ARRAY_LENGTH(o0) - 1);

  /* to-string */
  assert_string_eq(to_string(fixnum(0)), string("0"));
  assert_string_eq(to_string(fixnum(1)), string("1"));
  assert_string_eq(to_string(fixnum(2)), string("2"));
  assert_string_eq(to_string(fixnum(3)), string("3"));
  assert_string_eq(to_string(fixnum(9)), string("9"));
  assert_string_eq(to_string(fixnum(10)), string("10"));

  assert_string_eq(to_string(string("ABC")), string("ABC"));
  assert_string_eq(to_string(string("")), string(""));

  assert_string_eq(to_string(cons(string("ABC"), fixnum(43))),
                   string("(\"ABC\" . 43)"));
  assert_string_eq(to_string(cons(string("ABC"), cons(ufixnum(24234234), NIL))),
                   string("(\"ABC\" 24234234)"));

  assert_string_eq(to_string(flonum(0.0)), string("0.0"));
  assert_string_eq(to_string(flonum(1)), string("1.0"));
  assert_string_eq(to_string(flonum(0.01)), string("0.01"));
  assert_string_eq(to_string(flonum(56789.0)), string("56789.0"));
  assert_string_eq(to_string(flonum(56789.43215)), string("56789.43215"));
  assert_string_eq(to_string(flonum(9999999.41)), string("9999999.41"));
  assert_string_eq(to_string(flonum(9000000000000000.52)),
                   string("9000000000000001.0"));
  assert_string_eq(to_string(flonum(9e15)), string("9000000000000000.0"));
  assert_string_eq(to_string(flonum(9e15 * 10)), string("9e+16"));
  assert_string_eq(to_string(flonum(0.123456789123456789123456789)),
                   string("0.123456789"));
  assert_string_eq(to_string(flonum(1234123412341123499.123412341234)),
                   string("1.234123412e+18"));
  assert_string_eq(to_string(flonum(1234123412941123499.123412341234)),
                   string("1.234123413e+18")); /* should round last digit */
  assert_string_eq(to_string(flonum(0.000001)), string("0.000001"));
  assert_string_eq(to_string(flonum(0.0000001)), string("1e-7"));
  assert_string_eq(to_string(flonum(0.00000001)), string("1e-8"));
  assert_string_eq(to_string(flonum(0.000000059)), string("5.9e-8"));
  assert_string_eq(
      to_string(flonum(
          0.00000000000000000000000000000000000000000000034234234232942362341239123412312348)),
      string("3.423423423e-46"));
  assert_string_eq(
      to_string(flonum(
          0.00000000000000000000000000000000000000000000034234234267942362341239123412312348)),
      string("3.423423427e-46")); /* should round last digit */

  assert_string_eq(to_string(vec2(1, 2)), string("<1.0 2.0>"));

  /* TODO: something is wrong with 1.0003 (try typing it into the REPL) not sure
   * if its an issue during read or to-string. any leading zeros are cut off */
  /* TODO: something wrong with 23532456346234623462346236. */

  dba = dynamic_byte_array(10);
  dynamic_byte_array_push_char(dba, 60);
  dynamic_byte_array_push_char(dba, 71);
  dynamic_byte_array_push_char(dba, 1);
  assert_string_eq(to_string(dba), string("[0x3C 0x47 0x01]"));

  dba = dynamic_byte_array(10);
  assert_string_eq(to_string(dba), string("[]"));

  da = dynamic_array(10);
  dynamic_array_push(da, string("bink"));
  dynamic_array_push(da, flonum(3.245));
  assert_string_eq(to_string(da), string("[\"bink\" 3.245]"));

  da = dynamic_array(10);
  assert_string_eq(to_string(da), string("[]"));

  /*
   * equals
   */

  /* fixnum */
  assert(equals(fixnum(-2), fixnum(-2)));
  assert(!equals(fixnum(-2), fixnum(2)));
  /* ufixnum */
  assert(equals(ufixnum(242344238L), ufixnum(242344238L)));
  assert(!equals(ufixnum(242344231L), ufixnum(242344238L)));
  /* flonum */
  assert(equals(flonum(1.2), flonum(1.2)));
  assert(equals(flonum(1.234), flonum(1.234)));
  assert(!equals(flonum(-1.234), flonum(1.234)));
  assert(equals(flonum(1e-3), flonum(0.001)));
  /* string */
  assert(equals(string("abcdefghi"), string("abcdefghi")));
  assert(!equals(string("e"), string("")));
  /* nil */
  assert(equals(NIL, NIL));
  /* cons */
  assert(equals(cons(fixnum(7), NIL), cons(fixnum(7), NIL)));
  assert(!equals(cons(fixnum(7), cons(fixnum(8), NIL)),
                 cons(fixnum(7), cons(fixnum(10), NIL))));
  assert(!equals(cons(fixnum(7), cons(fixnum(8), NIL)),
                 cons(fixnum(9), cons(fixnum(8), NIL))));
  assert(!equals(cons(fixnum(7), cons(fixnum(8), NIL)),
                 cons(fixnum(7), cons(fixnum(8), cons(fixnum(9), NIL)))));
  assert(equals(cons(fixnum(7), cons(fixnum(8), cons(fixnum(9), NIL))),
                cons(fixnum(7), cons(fixnum(8), cons(fixnum(9), NIL)))));
  assert(equals(cons(cons(string("a"), cons(string("b"), NIL)),
                     cons(fixnum(8), cons(fixnum(9), NIL))),
                cons(cons(string("a"), cons(string("b"), NIL)),
                     cons(fixnum(8), cons(fixnum(9), NIL)))));
  assert(!equals(cons(cons(string("a"), cons(string("b"), NIL)),
                      cons(fixnum(8), cons(fixnum(9), NIL))),
                 cons(cons(string("a"), cons(string("x"), NIL)),
                      cons(fixnum(8), cons(fixnum(9), NIL)))));
  /* dynamic array */
  o0 = dynamic_array(10);
  o1 = dynamic_array(10);
  assert(equals(o0, o1));

  dynamic_array_push(o0, fixnum(1));
  assert(!equals(o0, o1));
  dynamic_array_push(o1, fixnum(1));
  assert(equals(o0, o1));

  dynamic_array_push(o0, fixnum(2));
  dynamic_array_push(o1, fixnum(2));
  assert(equals(o0, o1));
  /* dynamic byte array */
  o0 = dynamic_byte_array(10);
  o1 = dynamic_byte_array(10);
  assert(equals(o0, o1));

  dynamic_byte_array_push(o0, fixnum(1));
  assert(!equals(o0, o1));
  dynamic_byte_array_push(o1, fixnum(1));
  assert(equals(o0, o1));

  dynamic_byte_array_push(o0, fixnum(2));
  dynamic_byte_array_push(o1, fixnum(2));
  assert(equals(o0, o1));

  /* enumerator */
  o0 = enumerator(string("abcdefg"));
  o1 = enumerator(string("abcdefg"));
  assert(equals(o0, o1));
  byte_stream_read(o1, 1);
  assert(!equals(o0, o1));
  byte_stream_read(o0, 1);
  assert(equals(o0, o1));
  o0 = enumerator(string("91"));
  o1 = enumerator(string("93"));
  assert(!equals(o0, o1));

  /* TODO: add equals tests for package, symbol and file */

  /* bytecode */
  code0 = dynamic_byte_array(100);
  dynamic_byte_array_push_char(code0, op_const);
  dynamic_byte_array_push_char(code0, 0);
  consts0 = dynamic_array(100);
  dynamic_array_push(consts0, string("ab"));
  dynamic_array_push(consts0, cons(string("F"), cons(string("E"), NIL)));
  dynamic_array_push(consts0, flonum(8));
  o0 = function(consts0, code0, 99); /* bogus stack size */

  code1 = dynamic_byte_array(100);
  dynamic_byte_array_push_char(code1, op_const);
  dynamic_byte_array_push_char(code1, 0);
  consts1 = dynamic_array(100);
  dynamic_array_push(consts1, string("ab"));
  dynamic_array_push(consts1, cons(string("F"), cons(string("E"), NIL)));
  dynamic_array_push(consts1, flonum(8));
  o1 = function(consts1, code1, 99); /* bogus stack size */
  assert(equals(o0, o1));

  dynamic_array_push(consts1, flonum(10));
  assert(!equals(o0, o1));

  dynamic_array_push(consts0, flonum(10));
  assert(equals(o0, o1));

  dynamic_byte_array_push_char(code1, 9);
  assert(!equals(o0, o1));

  dynamic_byte_array_push_char(code0, 9);
  assert(equals(o0, o1));

  /* To be equal the types must be the same */
  assert(!equals(NIL, fixnum(2)));
  assert(!equals(fixnum(8), string("g")));

  /* there was something bizarre going on for this test where:

      double x = -3;
      assert(pow(10, -3) == pow(10, x));

     was failing. Apparently this is because pow(10, -3) is handled at compile
     time (by GCC) and converted to the precise constant 0.001 (the closest
     number to 0.001 that can be represented in floating point). But the pow(10,
     x) was calling the linked math library's pow on my Windows machine which
     apparently is imprecise pow(10, x) returned a number larger than 0.001.

     To fix this, instead of doing pow(10, -3), I now do 1/pow(10, 3). this
     gives the same result even if a variable is used (to force the linked
     library to be called). To ensure the math library is linked , I also added
     the '-lm' compile flag.
  
    T_READ_TEST("1e-3", flonum(0.001));
  */

  /********* symbol interning ************/
  reinit(0);
  assert(count(PACKAGE_SYMBOLS(GIS_PACKAGE)) == 0);
  o0 = intern(string("a"), GIS_PACKAGE); /* this should create a new symbol and
                                            add it to the package */
  assert(count(PACKAGE_SYMBOLS(GIS_PACKAGE)) == 1);
  assert(
      equals(SYMBOL_NAME(CONS_CAR(PACKAGE_SYMBOLS(GIS_PACKAGE))), string("a")));
  assert(o0 ==
         intern(string("a"),
                GIS_PACKAGE)); /* intern should find the existing symbol */
  o0 = intern(string("b"), GIS_PACKAGE); /* this should create a new symbol and
                                            add it to the package */
  assert(count(PACKAGE_SYMBOLS(GIS_PACKAGE)) == 2);

  /********* alist ***************/
  o0 = NIL;
  assert(alist_get_slot(o0, string("a")) == NIL);
  assert(alist_get_value(o0, string("a")) == NIL);
  o0 = alist_extend(o0, string("b"), fixnum(4));
  assert(equals(alist_get_value(o0, string("b")), fixnum(4)));
  o0 = alist_extend(o0, string("b"), fixnum(11));
  assert(equals(alist_get_value(o0, string("b")), fixnum(11)));
  o0 = alist_extend(o0, string("v"), string("abc"));
  assert(equals(alist_get_value(o0, string("v")), string("abc")));

  END_TESTS();
}
