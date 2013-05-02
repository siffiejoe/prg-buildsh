/* Copyright 2013 Philipp Janda <siffiejoe@gmx.net>
 *
 * You may do anything with this work that copyright law would normally
 * restrict, so long as you retain the above notice(s) and this license
 * in all redistributed copies and derived works.  There is no warranty.
 */

#ifndef MOON_H_
#define MOON_H_

/* file: moon.h
 * Utility functions/macros for binding C code to Lua.
 */

#include <stddef.h>
#include "lua.h"
#include "lauxlib.h"


#ifndef MOON_API
#  define MOON_API
#endif


/* enable changing prefix */
#ifndef MOON_PREFIX
#  define MOON_PREFIX moon
#endif
#define MOON_CONCAT_HELPER( a, b ) a##b
#define MOON_CONCAT( a, b ) MOON_CONCAT_HELPER( a, b )
#define moon_make_object  MOON_CONCAT( MOON_PREFIX, _make_object )
#define moon_add2env      MOON_CONCAT( MOON_PREFIX, _add2env )
#define moon_checkudata   MOON_CONCAT( MOON_PREFIX, _checkudata )
#define moon_finalizer    MOON_CONCAT( MOON_PREFIX, _finalizer )
#define moon_preload_c    MOON_CONCAT( MOON_PREFIX, _preload_c )
#define moon_preload_lua  MOON_CONCAT( MOON_PREFIX, _preload_lua )


typedef struct {
  char const* name;
  size_t size;
  void (*initializer)( void* );
  int protect;
  luaL_Reg const* metamethods;
  luaL_Reg const* methods;
} moon_object_type;


typedef struct {
  char const* reqname;
  char const* errname;
  char const* buf;
  size_t len;
} moon_lua_reg;


MOON_API void* moon_make_object( lua_State* L, moon_object_type const* t,
                                 int env_index );
MOON_API void moon_add2env( lua_State* L, int udindex );
MOON_API void* moon_checkudata( lua_State* L, int i, char const* tname );
MOON_API int* moon_finalizer( lua_State* L, lua_CFunction func );
MOON_API void moon_preload_c( lua_State* L, luaL_Reg const* libs );
MOON_API void moon_preload_lua( lua_State* L, moon_lua_reg const* libs );

/* Lua 5.1/5.2 compatibility macros/functions */
#if LUA_VERSION_NUM < 502
#  define moon_compat_register( L, libs ) luaL_register( (L), NULL, (libs) )
#  define moon_compat_setuservalue( L, i ) lua_setfenv( (L), (i) )
#  define moon_compat_getuservalue( L, i ) lua_getfenv( (L), (i) )
#  define moon_compat_rawlen( L, i ) lua_objlen( (L), (i) )
#  define moon_compat_testudata    MOON_CONCAT( MOON_PREFIX, _compat_testudata )
MOON_API void* moon_compat_testudata( lua_State* L, int i, char const* tname );
#else
#  define moon_compat_register( L, libs ) luaL_setfuncs( (L), (libs), 0 )
#  define moon_compat_setuservalue( L, i ) lua_setuservalue( (L), (i) )
#  define moon_compat_getuservalue( L, i ) lua_getuservalue( (L), (i) )
#  define moon_compat_rawlen( L, i ) lua_rawlen( (L), (i) )
#  define moon_compat_testudata( L, i, t ) luaL_testudata( (L), (i), (t) )
#endif

#endif /* MOON_H_ */

