/***
  @module ape
*/
#include <stddef.h>
#include <lua.h>
#include <lauxlib.h>
#include <apr_pools.h>
#include "moon.h"
#include "ape.h"

/***
  Memory pools.
  @section pools
*/
/***
  Userdata type for APR memory pools.
  @type apr_pool_t
*/

static char const ape_pool_sentinel = 0;
#define POOL_REG_KEY ((void*)&ape_pool_sentinel)


static int ape_pool_child( lua_State* L ) {
  apr_pool_t** parent = moon_checkudata( L, 1, APE_POOL_NAME );
  apr_pool_t** pool = moon_newobject_ref( L, APE_POOL_NAME, 1 );
  ape_assert( L, apr_pool_create( pool, *parent ), "APR memory pool" );
  return 1;
}


static int ape_pool_gc( lua_State* L ) {
  apr_pool_t** pool = moon_checkudata( L, 1, APE_POOL_NAME );
  if( *pool != NULL )
    apr_pool_destroy( *pool );
  return 0;
}


static void ape_pool_init( void* p ) {
  apr_pool_t** pool = p;
  *pool = NULL;
}


static int ape_pool_create( lua_State* L ) {
  apr_pool_t** pool = moon_newobject( L, APE_POOL_NAME, 0 );
  apr_allocator_t* allocator = NULL;
  ape_assert( L, apr_pool_create( pool, NULL ), "APR memory pool" );
  allocator = apr_pool_allocator_get( *pool );
  if( allocator )
    apr_allocator_max_free_set( allocator, 32 );
  return 1;
}



APE_API void ape_pool_setup( lua_State* L ) {
  luaL_Reg const ape_pool_metamethods[] = {
    { "__gc", ape_pool_gc },
    { NULL, NULL }
  };
  /***
    Userdata type for APR memory pools.
    @type apr_pool_t
  */
  luaL_Reg const ape_pool_methods[] = {
  /***
    Creates a new APR memory pool which is a child of self.
    @function child
    @treturn apr_pool_t the newly created APR memory pool
  */
    { "child", ape_pool_child },
    { NULL, NULL }
  };
  moon_object_type const ape_pool_type = {
    APE_POOL_NAME,
    sizeof( apr_pool_t* ),
    ape_pool_init,
    ape_pool_metamethods,
    ape_pool_methods
  };
  /***
    Memory pools.
    @section pools
  */
  luaL_Reg const ape_pool_functions[] = {
  /***
    Creates a new top-level memory pool.
    @function pool_create
    @treturn apr_pool_t the newly created APR memory pool
  */
    { "pool_create", ape_pool_create },
    { NULL, NULL },
  };
  moon_defobject( L, &ape_pool_type, 0 );
  lua_pushlightuserdata( L, POOL_REG_KEY );
  ape_pool_create( L );
  lua_rawset( L, LUA_REGISTRYINDEX );
  moon_register( L, ape_pool_functions );
}


APE_API apr_pool_t** ape_opt_pool( lua_State* L, int index ) {
  apr_pool_t** pool = NULL;
  if( lua_isnoneornil( L, index ) ) {
    lua_pushlightuserdata( L, POOL_REG_KEY );
    lua_rawget( L, LUA_REGISTRYINDEX );
    pool = lua_touserdata( L, -1 );
    apr_pool_clear( *pool );
    lua_pop( L, 1 );
  } else
    pool = moon_checkudata( L, index, APE_POOL_NAME );
  return pool;
}

