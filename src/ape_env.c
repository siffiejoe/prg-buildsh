/***
@module ape
*/
#include <stddef.h>
#include "lua.h"
#include "lauxlib.h"
#include "moon.h"
#include "ape.h"
#include "apr_env.h"


static int ape_env_get( lua_State* L ) {
  char const* name = luaL_checkstring( L, 1 );
  apr_pool_t** pool = ape_opt_pool( L, 2 );
  char* value = NULL;
  apr_status_t rv = apr_env_get( &value, name, *pool );
  if( rv == APR_SUCCESS )
    lua_pushstring( L, value != NULL ? value : "" );
  return ape_status( L, 1, rv );
}

static int ape_env_set( lua_State* L ) {
  char const* name = luaL_checkstring( L, 1 );
  char const* value = luaL_checkstring( L, 2 );
  apr_pool_t** pool = ape_opt_pool( L, 3 );
  return ape_status( L, -1, apr_env_set( name, value, *pool ) );
}

static int ape_env_delete( lua_State* L ) {
  char const* name = luaL_checkstring( L, 1 );
  apr_pool_t** pool = ape_opt_pool( L, 2 );
  return ape_status( L, -1, apr_env_delete( name, *pool ) );
}


/***
Environment manipulation.
@section envs
*/
static luaL_Reg const ape_env_functions[] = {
/***
Retrieves the value of a given environment variable.
@function env_get
@tparam string name the name of the environment variable
@tparam[opt] apr_pool_t pool a memory pool for temporary storage
@treturn string the value of the enviroment variable on success
@treturn nil,string nil and an error message in case of an error
*/
  { "env_get", ape_env_get },
/***
Sets an environment variable to the given value.
@function env_set
@tparam string name the name of the environment variable
@tparam string value the new value for the environment variable
@tparam[opt] apr_pool_t pool a memory pool for temporary storage
@treturn boolean true in case of success
@treturn nil,string nil and an error message in case of an error
*/
  { "env_set", ape_env_set },
/***
Deletes a given variable from the environment.
@function env_delete
@tparam string name the name of the environment variable
@tparam[opt] apr_pool_t pool a memory pool for temporary storage
@treturn boolean true in case of success
@treturn nil,string nil and an error message in case of an error
*/
  { "env_delete", ape_env_delete },
  { NULL, NULL }
};


APE_API void ape_env_setup( lua_State* L ) {
  moon_compat_register( L, ape_env_functions );
}

