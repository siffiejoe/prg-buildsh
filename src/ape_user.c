/***
  @module ape
*/
#include <stddef.h>
#include <lua.h>
#include <lauxlib.h>
#include <apr.h>
#include <apr_user.h>
#include "moon.h"
#include "ape.h"

/***
  User/group information.
  @section user
*/
/***
  Userdata type representing a user ID.
  @type apr_uid_t
*/
/***
  Userdata type representing a group ID.
  @type apr_gid_t
*/

#ifdef APR_HAS_USER

static int ape_uid_eq( lua_State* L ) {
  apr_uid_t* a = moon_checkudata( L, 1, APE_UID_NAME );
  apr_uid_t* b = moon_checkudata( L, 2, APE_UID_NAME );
  lua_pushboolean( L, apr_uid_compare( *a, *b ) == APR_SUCCESS );
  return 1;
}


static int ape_uid_name_get( lua_State* L ) {
  apr_uid_t* uid = moon_checkudata( L, 1, APE_UID_NAME );
  apr_pool_t** pool = ape_opt_pool( L, 2 );
  char* name = NULL;
  apr_status_t rv = apr_uid_name_get( &name, *uid, *pool );
  if( rv == APR_SUCCESS )
    lua_pushstring( L, name != NULL ? name : "" );
  return ape_status( L, 1, rv );
}


static int ape_uid_homepath_get( lua_State* L ) {
  char const* name = luaL_checkstring( L, 1 );
  apr_pool_t** pool = ape_opt_pool( L, 2 );
  char* path = NULL;
  apr_status_t rv = apr_uid_homepath_get( &path, name, *pool );
  if( rv == APR_SUCCESS )
    lua_pushstring( L, path != NULL ? path : "" );
  return ape_status( L, 1, rv );
}


static int ape_gid_eq( lua_State* L ) {
  apr_uid_t* a = moon_checkudata( L, 1, APE_GID_NAME );
  apr_uid_t* b = moon_checkudata( L, 2, APE_GID_NAME );
  lua_pushboolean( L, apr_gid_compare( *a, *b ) == APR_SUCCESS );
  return 1;
}


static int ape_gid_name_get( lua_State* L ) {
  apr_gid_t* gid = moon_checkudata( L, 1, APE_GID_NAME );
  apr_pool_t** pool = ape_opt_pool( L, 2 );
  char* name = NULL;
  apr_status_t rv = apr_gid_name_get( &name, *gid, *pool );
  if( rv == APR_SUCCESS )
    lua_pushstring( L, name != NULL ? name : "" );
  return ape_status( L, 1, rv );
}


static int ape_uid_current( lua_State* L ) {
  apr_pool_t** pool = moon_checkudata( L, 1, APE_POOL_NAME );
  apr_uid_t* uid = moon_newobject_ref( L, APE_UID_NAME, 1 );
  apr_gid_t* gid = moon_newobject_ref( L, APE_GID_NAME, 1 );
  return ape_status( L, 2, apr_uid_current( uid, gid, *pool ) );
}


static int ape_uid_get( lua_State* L ) {
  char const* name = luaL_checkstring( L, 1 );
  apr_pool_t** pool = moon_checkudata( L, 2, APE_POOL_NAME );
  apr_uid_t* uid = moon_newobject_ref( L, APE_UID_NAME, 2 );
  apr_gid_t* gid = moon_newobject_ref( L, APE_GID_NAME, 2 );
  return ape_status( L, 2, apr_uid_get( uid, gid, name, *pool ) );
}


static int ape_gid_get( lua_State* L ) {
  char const* name = luaL_checkstring( L, 1 );
  apr_pool_t** pool = moon_checkudata( L, 2, APE_POOL_NAME );
  apr_gid_t* gid = moon_newobject_ref( L, APE_GID_NAME, 2 );
  return ape_status( L, 1, apr_gid_get( gid, name, *pool ) );
}


APE_API void ape_uid_userdata( lua_State* L, apr_uid_t uid, int index ) {
  apr_uid_t* newuid = moon_newobject_ref( L, APE_UID_NAME, index );
  *newuid = uid;
}


APE_API void ape_gid_userdata( lua_State* L, apr_gid_t gid, int index ) {
  apr_gid_t* newgid = moon_newobject_ref( L, APE_GID_NAME, index );
  *newgid = gid;
}

#endif


APE_API void ape_user_setup( lua_State* L ) {
#ifdef APR_HAS_USER
  luaL_Reg const ape_uid_metamethods[] = {
    { "__eq", ape_uid_eq },
    { NULL, NULL }
  };
  /***
    Userdata type representing a user ID.
    @type apr_uid_t
  */
  luaL_Reg const ape_uid_methods[] = {
  /***
    Looks up the login name belonging to the user ID.
    @function name
    @tparam[opt] apr_pool_t pool a memory pool for temporary
      allocations
    @treturn string the login name of the user
    @treturn nil,string,number nil, an error message, and an error
      code in case of an error
  */
    { "name", ape_uid_name_get },
    { NULL, NULL }
  };
  moon_object_type const ape_uid_type = {
    APE_UID_NAME,
    sizeof( apr_uid_t ),
    0, /* NULL (function) pointer */
    ape_uid_metamethods,
    ape_uid_methods
  };
  luaL_Reg const ape_gid_metamethods[] = {
    { "__eq", ape_gid_eq },
    { NULL, NULL }
  };
  /***
    Userdata type representing a group ID.
    @type apr_gid_t
  */
  luaL_Reg const ape_gid_methods[] = {
  /***
    Looks up the group name belonging to the group ID.
    @function name
    @tparam[opt] apr_pool_t pool a memory pool for temporary
      allocations
    @treturn string the name of the group
    @treturn nil,string,number nil, an error message, and an error
      code in case of an error
  */
    { "name", ape_gid_name_get },
    { NULL, NULL }
  };
  moon_object_type const ape_gid_type = {
    APE_GID_NAME,
    sizeof( apr_gid_t ),
    0, /* NULL (function) pointer */
    ape_gid_metamethods,
    ape_gid_methods
  };
  /***
    User/group information.
    @section user
  */
  luaL_Reg const ape_user_functions[] = {
  /***
    Returns user and group ID of the currently active user.
    @function uid_current
    @tparam apr_pool_t pool a memory pool for memory allocations
    @treturn apr_uid_t,apr_gid_t user and group ID of the current user
    @treturn nil,string,number nil, an error message, and an error
      code in case of an error
  */
    { "uid_current", ape_uid_current },
  /***
    Looks up the login name belonging to a given user ID.
    @function uid_name_get
    @tparam apr_uid_t uid a user ID
    @tparam[opt] apr_pool_t pool a memory pool for temporary
      allocations
    @treturn string the login name of the user
    @treturn nil,string,number nil, an error message, and an error
      code in case of an error
  */
    { "uid_name_get", ape_uid_name_get },
  /***
    Looks up user and group ID for a given login name.
    @function uid_get
    @tparam string name the name of the user
    @tparam apr_pool_t pool a memory pool for memory allocations
    @treturn apr_uid_t,apr_gid_t user and group ID of the given user
    @treturn nil,string,number nil, an error message, and an error
      code in case of an error
  */
    { "uid_get", ape_uid_get },
  /***
    Looks up the home directory of the user with the given login name.
    @function uid_homepath_get
    @tparam string name the user's login name
    @tparam[opt] apr_pool_t pool a memory pool for temporary
      allocations
    @treturn string the path of the user's home directory
    @treturn nil,string,number nil, an error message, and an error
      code in case of an error
  */
    { "uid_homepath_get", ape_uid_homepath_get },
  /***
    Looks up the group name belonging to a given group ID.
    @function gid_name_get
    @tparam apr_gid_t gid a group ID
    @tparam[opt] apr_pool_t pool a memory pool for temporary
      allocations
    @treturn string the name of the group
    @treturn nil,string,number nil, an error message, and an error
      code in case of an error
  */
    { "gid_name_get", ape_gid_name_get },
  /***
    Looks up the group ID for a given group name.
    @function gid_get
    @tparam string name the group name
    @tparam apr_pool_t pool a memory pool for memory allocations
    @treturn apr_gid_t the group ID of the given group
    @treturn nil,string,number nil, an error message, and an error
      code in case of an error
  */
    { "gid_get", ape_gid_get },
    { NULL, NULL }
  };
  moon_defobject( L, &ape_uid_type, 0 );
  moon_defobject( L, &ape_gid_type, 0 );
  moon_register( L, ape_user_functions );
#endif
}

