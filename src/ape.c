/***
  An Apache Portable Runtime binding which is very close to the C API.
  @module ape
*/
#include <stddef.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <apr_general.h>
#include "moon.h"
#include "ape.h"


APE_API int ape_status( lua_State* L, int n, apr_status_t rv ) {
  if( rv == APR_SUCCESS ) {
    if( n < 0 ) {
      lua_pushboolean( L, 1 );
      n = 1;
    }
  } else {
    char buf[ 200 ] = { 0 };
    lua_pushnil( L );
    apr_strerror( rv, buf, sizeof( buf ) );
    lua_pushstring( L, buf );
    lua_pushinteger( L, rv );
    n = 3;
  }
  return n;
}


static int ape_type( lua_State* L ) {
  if( !lua_isuserdata( L, 1 ) || !luaL_getmetafield( L, 1, "__name" ) )
    return 0;
  return 1;
}


static int ape_platform( lua_State* L ) {
#if defined( _WIN32 ) || defined( _WIN64 ) || defined( __WINDOWS__ ) || \
    defined( __WIN32__ ) || defined( __TOS_WIN__ )
  lua_pushliteral( L, "WINDOWS" );
#elif defined( __BEOS__ )
  lua_pushliteral( L, "BEOS" );
#elif defined( __APPLE__ ) && defined( __MACH__ )
  lua_pushliteral( L, "MACOSX" );
#elif defined( OS2 ) || defined( _OS2 ) || defined( __OS2__ ) || \
      defined( __TOS_OS2__ )
  lua_pushliteral( L, "OS2" );
#else
  lua_pushliteral( L, "UNIX" );
#endif
  return 1;
}


static int terminate_apr( lua_State* L ) {
  int* flag = lua_touserdata( L, 1 );
  if( *flag )
    apr_terminate();
  return 0;
}



APE_API int luaopen_ape( lua_State* L ) {
  /***
    Basic functions.
    @section base
  */
  luaL_Reg const functions[] = {
  /***
    Returns the type names of APE objects.
    @function type
    @param value a Lua value to check
    @treturn string the name of the type if the value is an APE object
    @return nothing if the given value is not an APE object
  */
    { "type", ape_type },
  /***
    Returns the current platform.
    @function platform
    @treturn string the name of the current platform
  */
    { "platform", ape_platform },
    { NULL, NULL }
  };
  int* flag = moon_finalizer( L, terminate_apr );
  ape_assert( L, apr_initialize(), "APR initialization" );
  *flag = 1;
  lua_pop( L, 1 ); /* pop finalizer flag */
  lua_newtable( L ); /* create module table */
  ape_pool_setup( L );
  ape_errno_setup( L );
  ape_env_setup( L );
  ape_fnmatch_setup( L );
  ape_fpath_setup( L );
  ape_file_setup( L );
  ape_user_setup( L );
  ape_time_setup( L );
  ape_proc_setup( L );
  ape_random_setup( L );
  ape_extra_setup( L );
  moon_register( L, functions );
  return 1;
}

