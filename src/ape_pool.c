/***
@module ape
*/
#include <stddef.h>
#include "lua.h"
#include "lauxlib.h"
#include "moon.h"
#include "ape.h"
#include "apr_pools.h"

/***
Memory pools.
@section pools
*/
/***
Userdata type for APR memory pools.
@type apr_pool_t
*/

#define POOL_REG_KEY ((void*)&ape_pool_type)

static int ape_pool_child( lua_State* L );


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

static luaL_Reg const ape_pool_metamethods[] = {
  { "__gc", ape_pool_gc },
  { NULL, NULL }
};

/***
Userdata type for APR memory pools.
@type apr_pool_t
*/
static luaL_Reg const ape_pool_methods[] = {
/***
Creates a new APR memory pool which is a child of self.
@function child
@treturn apr_pool_t the newly created APR memory pool
*/
  { "child", ape_pool_child },
  { NULL, NULL }
};

static ape_object_type const ape_pool_type = {
  APE_POOL_NAME,
  sizeof( apr_pool_t* ),
  ape_pool_init,
  1,
  ape_pool_metamethods,
  ape_pool_methods
};


static int ape_pool_child( lua_State* L ) {
  apr_pool_t** parent = moon_checkudata( L, 1, APE_POOL_NAME );
  apr_pool_t** pool = ape_new_object( L, &ape_pool_type, 1 );
  ape_assert( L, apr_pool_create( pool, *parent ), "APR memory pool" );
  return 1;
}


static int ape_pool_create( lua_State* L ) {
  apr_pool_t** pool = ape_new_object( L, &ape_pool_type, 0 );
  apr_allocator_t* allocator = NULL;
  ape_assert( L, apr_pool_create( pool, NULL ), "APR memory pool" );
  allocator = apr_pool_allocator_get( *pool );
  if( allocator )
    apr_allocator_max_free_set( allocator, 32 );
  return 1;
}


/***
Memory pools.
@section pools
*/
static luaL_Reg const ape_pool_functions[] = {
/***
Creates a new top-level memory pool.
@function pool_create
@treturn apr_pool_t the newly created APR memory pool
*/
  { "pool_create", ape_pool_create },
  { NULL, NULL },
};


APE_API void ape_pool_setup( lua_State* L ) {
  lua_pushlightuserdata( L, POOL_REG_KEY );
  ape_pool_create( L );
  lua_rawset( L, LUA_REGISTRYINDEX );
  moon_compat_register( L, ape_pool_functions );
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


