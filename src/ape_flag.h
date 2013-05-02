/* this include file is a macro file which could be included
 * multiple times with different settings.
 */

/* "parameter checking" */
#ifndef APE_FLAG_NAME
#  error APE_FLAG_NAME is not defined
#endif

#ifndef APE_FLAG_TYPE
#  error APE_FLAG_TYPE is not defined
#endif

#ifndef APE_FLAG_SUFFIX
#  error APE_FLAG_SUFFIX is not defined
#endif

#define APE_CONCAT_HELPER( c, d ) c ## d
#define APE_CONCAT( a, b ) APE_CONCAT_HELPER( a, b )
#define APE_FLAG_ADD APE_CONCAT( ape_flag_add_, APE_FLAG_SUFFIX )
#define APE_FLAG_SUB APE_CONCAT( ape_flag_sub_, APE_FLAG_SUFFIX )
#define APE_FLAG_CALL APE_CONCAT( ape_flag_call_, APE_FLAG_SUFFIX )
#define APE_FLAG_EQ APE_CONCAT( ape_flag_eq_, APE_FLAG_SUFFIX )
#define APE_FLAG_NEW APE_CONCAT( ape_flag_new_, APE_FLAG_SUFFIX )
#define APE_FLAG_GET APE_CONCAT( ape_flag_get_, APE_FLAG_SUFFIX )
#define APE_FLAG_METAMETHODS APE_CONCAT( ape_flag_metamethods_, APE_FLAG_SUFFIX )
#define APE_FLAG_CLASS APE_CONCAT( ape_flag_type_, APE_FLAG_SUFFIX )
#ifndef APE_FLAG_EQMETHOD
#  define APE_FLAG_EQMETHOD( a, b ) ((a) == (b))
#endif

#ifndef APE_FLAG_NOBITOPS
static int APE_FLAG_ADD( lua_State* L );
static int APE_FLAG_SUB( lua_State* L );
static int APE_FLAG_CALL( lua_State* L );
#endif
#ifndef APE_FLAG_NORELOPS
static int APE_FLAG_EQ( lua_State* L );
#endif

static luaL_Reg const APE_FLAG_METAMETHODS[] = {
#ifndef APE_FLAG_NOBITOPS
  { "__add", APE_FLAG_ADD },
  { "__sub", APE_FLAG_SUB },
  { "__call", APE_FLAG_CALL },
#endif
#ifndef APE_FLAG_NORELOPS
  { "__eq", APE_FLAG_EQ },
#endif
  { NULL, NULL }
};


static ape_object_type const APE_FLAG_CLASS = {
  APE_FLAG_NAME,
  sizeof( APE_FLAG_TYPE ),
  0, /* NULL (function) pointer */
  1,
  APE_FLAG_METAMETHODS,
  NULL
};


static void APE_FLAG_NEW( lua_State* L, APE_FLAG_TYPE v ) {
  APE_FLAG_TYPE* f = ape_new_object( L, &APE_FLAG_CLASS, 0 );
  *f = v;
}

static APE_FLAG_TYPE APE_FLAG_GET( lua_State* L, int index ) {
  APE_FLAG_TYPE* f = moon_checkudata( L, index, APE_FLAG_NAME );
  return *f;
}

#ifndef APE_FLAG_NOBITOPS
static int APE_FLAG_ADD( lua_State* L ) {
  APE_FLAG_TYPE* a = moon_checkudata( L, 1, APE_FLAG_NAME );
  APE_FLAG_TYPE* b = moon_checkudata( L, 2, APE_FLAG_NAME );
  APE_FLAG_TYPE* c = ape_new_object( L, &APE_FLAG_CLASS, 0 );
  *c = *a | *b;
  return 1;
}
static int APE_FLAG_SUB( lua_State* L ) {
  APE_FLAG_TYPE* a = moon_checkudata( L, 1, APE_FLAG_NAME );
  APE_FLAG_TYPE* b = moon_checkudata( L, 2, APE_FLAG_NAME );
  APE_FLAG_TYPE* c = ape_new_object( L, &APE_FLAG_CLASS, 0 );
  *c = *a & ~(*b);
  return 1;
}
static int APE_FLAG_CALL( lua_State* L ) {
  APE_FLAG_TYPE* a = moon_checkudata( L, 1, APE_FLAG_NAME );
  APE_FLAG_TYPE* b = moon_checkudata( L, 2, APE_FLAG_NAME );
  lua_pushboolean( L, !(*b & ~(*a)) );
  return 1;
}
#endif

#ifndef APE_FLAG_NORELOPS
static int APE_FLAG_EQ( lua_State* L ) {
  APE_FLAG_TYPE* a = moon_checkudata( L, 1, APE_FLAG_NAME );
  APE_FLAG_TYPE* b = moon_checkudata( L, 2, APE_FLAG_NAME );
  lua_pushboolean( L, APE_FLAG_EQMETHOD( *a, *b ) );
  return 1;
}
#endif


#undef APE_FLAG_ADD
#undef APE_FLAG_SUB
#undef APE_FLAG_CALL
#undef APE_FLAG_EQ
#undef APE_FLAG_NEW
#undef APE_FLAG_GET
#undef APE_FLAG_METAMETHODS
#undef APE_FLAG_CLASS
#undef APE_CONCAT_HELPER
#undef APE_CONCAT

#undef APE_FLAG_NAME
#undef APE_FLAG_TYPE
#undef APE_FLAG_SUFFIX
#undef APE_FLAG_NOBITOPS
#undef APE_FLAG_NORELOPS
#undef APE_FLAG_EQMETHOD

