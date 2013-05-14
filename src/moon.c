#include <stddef.h>
#include "lua.h"
#include "lauxlib.h"
#include "moon.h"


static int meta_is_complete( lua_State* L, int index ) {
  int res = 0;
  lua_getfield( L, index, "__type" );
  res = lua_toboolean( L, -1 );
  lua_pop( L, 1 );
  return res;
}

static int obj_tostring( lua_State* L ) {
  void* ptr = lua_touserdata( L, 1 );
  char const* name = luaL_checkstring( L, lua_upvalueindex( 1 ) );
  lua_pushfstring( L, "[%s]: %p", name, ptr );
  return 1;
}


MOON_API void* moon_make_object( lua_State* L, moon_object_type const* t,
                                 int env_index ) {

  void* ptr = NULL;
  if( env_index != 0 ) {
    /* make env_index an absolute stack index */
    lua_pushvalue( L, env_index );
    env_index = lua_gettop( L );
  }
  ptr = lua_newuserdata( L, t->size );
  if( t->initializer )
    t->initializer( ptr );
  if( luaL_newmetatable( L, t->name ) || !meta_is_complete( L, -1 ) ) {
    /* populate metatable */
    lua_pushstring( L, t->name );
    lua_pushcclosure( L, obj_tostring, 1 );
    lua_setfield( L, -2, "__tostring" );
    if( t->metamethods )
      moon_compat_register( L, t->metamethods );
    if( t->methods || t->propindex ) {
      moon_propindex( L, t->methods, 0, t->propindex );
      lua_setfield( L, -2, "__index" );
    }
    if( t->protect ) {
      lua_pushboolean( L, 0 );
      lua_setfield( L, -2, "__metatable" );
    }
    /* used as a marker to check for fully constructed metatable */
    lua_pushstring( L, t->name );
    lua_setfield( L, -2, "__type" );
  }
  lua_setmetatable( L, -2 );
  if( env_index != 0 ) {
    lua_pushvalue( L, env_index );
    moon_compat_setuservalue( L, -2 );
    lua_replace( L, env_index );
  }
  return ptr;
}


static int dispatch( lua_State* L ) {
  lua_CFunction pindex;
  /* try method table first */
  if( lua_istable( L, lua_upvalueindex( 1 ) ) ) {
    lua_pushvalue( L, 2 ); /* duplicate key */
    lua_rawget( L, lua_upvalueindex( 1 ) );
    if( !lua_isnil( L, -1 ) )
      return 1;
    lua_pop( L, 1 );
  }
  pindex = lua_tocfunction( L, lua_upvalueindex( 2 ) );
  return pindex( L );
}

MOON_API void moon_propindex( lua_State* L, luaL_Reg const* methods,
                              int nups, lua_CFunction pindex ) {
  int top = lua_gettop( L );
  if( methods != NULL ) {
    luaL_checkstack( L, nups + 3, NULL );
    lua_newtable( L );
    for( ; methods->func; ++methods ) {
      int i = 0;
      for( i = 0; i < nups; ++i )
        lua_pushvalue( L, top-nups+i+1 );
      lua_pushcclosure( L, methods->func, nups );
      lua_setfield( L, top+1, methods->name );
    }
    if( nups > 0 ) {
      lua_replace( L, top-nups+1 );
      lua_pop( L, nups-1 );
    }
  } else {
    lua_pop( L, nups );
    lua_pushnil( L );
  }
  if( pindex ) {
    lua_pushcfunction( L, pindex );
    lua_pushcclosure( L, dispatch, 2 );
  }
}


MOON_API void moon_add2env( lua_State* L, int index ) {
  lua_pushvalue( L, index );
  moon_compat_getuservalue( L, -1 );
  if( lua_isnil( L, -1 ) ) {
    lua_pop( L, 1 );
    lua_newtable( L );
    lua_pushvalue( L, -1 );
    moon_compat_setuservalue( L, -3 );
  }
  lua_pushvalue( L, -3 );
  lua_rawseti( L, -2, moon_compat_rawlen( L, -2 )+1 );
  lua_pop( L, 3 );
}

#if LUA_VERSION_NUM < 502
MOON_API void* moon_compat_testudata( lua_State* L, int i, char const* tname ) {
  void* p = lua_touserdata( L, i );
  if( p == NULL || !lua_getmetatable( L, i ) )
    return NULL;
  else {
    int res = 0;
    luaL_getmetatable( L, tname );
    res = lua_rawequal( L, -1, -2 );
    lua_pop( L, 2 );
    if( !res )
      p = NULL;
  }
  return p;
}
#endif


static int type_error( lua_State* L, int i, char const* t1,
                       char const* t2 ) {
  char const* msg = lua_pushfstring( L, "%s expected, got %s", t1, t2 );
  return luaL_argerror( L, i, msg );
}

MOON_API void* moon_checkudata( lua_State* L, int i, char const* tname ) {
  void* p = lua_touserdata( L, i );
  if( p == NULL || !lua_getmetatable( L, i ) ) {
    type_error( L, i, tname, luaL_typename( L, i ) );
  } else {
    luaL_getmetatable( L, tname );
    if( !lua_rawequal( L, -1, -2 ) ) {
      char const* t2 = luaL_typename( L, i );
      lua_getfield( L, -2, "__type" );
      if( lua_isstring( L, -1 ) )
        t2 = lua_tostring( L, -1 );
      type_error( L, i, tname, t2 );
    }
    lua_pop( L, 2 );
  }
  return p;
}


MOON_API int* moon_finalizer( lua_State* L, lua_CFunction func ) {
  int* flag = lua_newuserdata( L, sizeof( int ) );
  *flag = 0;
  /* setmetatable( flag, { __gc = func } ) */
  lua_newtable( L );
  lua_pushcfunction( L, func );
  lua_setfield( L, -2, "__gc" );
  lua_setmetatable( L, -2 );
  /* registry[ flag ] = flag */
  lua_pushvalue( L, -1 );
  lua_pushvalue( L, -2 );
  lua_settable( L, LUA_REGISTRYINDEX );
  return flag;
}


MOON_API void moon_preload_c( lua_State* L, luaL_Reg const* libs ) {
  int top = lua_gettop( L );
  lua_getglobal( L, "package" );
  lua_getfield( L, -1, "preload" );
  for( ; libs->func; libs++ ) {
    lua_pushcfunction( L, libs->func );
    lua_setfield( L, -2, libs->name );
  }
  lua_settop( L, top );
}


MOON_API void moon_preload_lua( lua_State* L, moon_lua_reg const* libs ) {
  int top = lua_gettop( L );
  lua_getglobal( L, "package" );
  lua_getfield( L, -1, "preload" );
  for( ; libs->buf; libs++ ) {
    if( 0 != luaL_loadbuffer( L, libs->buf, libs->len, libs->errname ) )
      lua_error( L );
    lua_setfield( L, -2, libs->reqname );
  }
  lua_settop( L, top );
}


