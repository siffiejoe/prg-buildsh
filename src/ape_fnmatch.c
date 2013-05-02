/***
@module ape
*/
#include <stddef.h>
#include "lua.h"
#include "lauxlib.h"
#include "moon.h"
#include "ape.h"
#include "apr_fnmatch.h"
#include "apr_tables.h"


#define APE_FLAG_NAME "apr_fnmflags_t"
#define APE_FLAG_TYPE int
#define APE_FLAG_SUFFIX fnmflags
#include "ape_flag.h"


static int ape_fnmatch( lua_State* L ) {
  char const* pattern = luaL_checkstring( L, 1 );
  char const* name = luaL_checkstring( L, 2 );
  int flags = ape_flag_get_fnmflags( L, 3 );
  lua_pushboolean( L, apr_fnmatch( pattern, name, flags ) == APR_SUCCESS );
  return 1;
}

static int ape_fnmatch_test( lua_State* L ) {
  char const* pattern = luaL_checkstring( L, 1 );
  lua_pushboolean( L, apr_fnmatch_test( pattern ) );
  return 1;
}

static int ape_match_glob( lua_State* L ) {
  char const* pattern = luaL_checkstring( L, 1 );
  apr_pool_t** pool = ape_opt_pool( L, 2 );
  apr_array_header_t* array = NULL;
  apr_status_t rv = apr_match_glob( pattern, &array, *pool );
  if( rv == APR_SUCCESS ) {
    int i = 0;
    lua_createtable( L, array != NULL ? array->nelts : 0, 0 );
    if( array != NULL ) {
      for( i = 0; i < array->nelts; ++i ) {
        lua_pushstring( L, ((char const**)array->elts)[ i ] );
        lua_rawseti( L, -2, i+1 );
      }
    }
  }
  return ape_status( L, 1, rv );
}


/***
File name matching.
@section fnmatch
*/
static luaL_Reg const ape_fnmatch_functions[] = {
/***
Checks if the given pattern matches the given name.
@function fnmatch
@tparam string pattern the pattern
@tparam string name the name to try
@param flags flags for changing the matching behavior
@treturn boolean true if the pattern matches the name, false otherwise
*/
  { "fnmatch", ape_fnmatch },
/***
Checks if the given string is a pattern.
@function fnmatch_test
@tparam string s the string to check
@treturn boolean true if the string is a pattern, false otherwise
*/
  { "fnmatch_test", ape_fnmatch_test },
/***
Creates an array of file names that match the given pattern.
@function match_glob
@tparam string pattern the pattern to search for
@tparam[opt] apr_pool_t pool a memory pool for temporary allocations
@treturn table an array of matching file names
*/
  { "match_glob", ape_match_glob },
  { NULL, NULL }
};


#define define_flag( L, name, suffix ) \
  do { \
    ape_flag_new_ ## suffix( (L), APR_ ## name ); \
    lua_setfield( (L), -2, #name ); \
  } while( 0 )

APE_API void ape_fnmatch_setup( lua_State* L ) {
/***
@class field
@name FNM_NOESCAPE
*/
  define_flag( L, FNM_NOESCAPE, fnmflags );
/***
@class field
@name FNM_PATHNAME
*/
  define_flag( L, FNM_PATHNAME, fnmflags );
/***
@class field
@name FNM_PERIOD
*/
  define_flag( L, FNM_PERIOD, fnmflags );
/***
@class field
@name FNM_CASE_BLIND
*/
  define_flag( L, FNM_CASE_BLIND, fnmflags );
  moon_compat_register( L, ape_fnmatch_functions );
}

#undef define_flag

