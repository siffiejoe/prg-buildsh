/***
@module ape
*/
#include <stddef.h>
#include "lua.h"
#include "lauxlib.h"
#include "moon.h"
#include "ape.h"
#include "apr_file_info.h"
#include "apr_portable.h"
#include "apr_tables.h"
#include "apr_lib.h"


#define APE_FLAG_NAME "apr_fpflags_t"
#define APE_FLAG_TYPE apr_int32_t
#define APE_FLAG_SUFFIX fpflags
#include "ape_flag.h"


static int ape_filepath_root( lua_State* L ) {
  char const* fpath = luaL_checkstring( L, 1 );
  apr_int32_t flags = ape_flag_get_fpflags( L, 2 );
  apr_pool_t** pool = ape_opt_pool( L, 3 );
  char const* rpath = NULL;
  apr_status_t rv = apr_filepath_root( &rpath, &fpath, flags, *pool );
  if( rv == APR_SUCCESS || APR_STATUS_IS_EINCOMPLETE( rv ) ) {
    lua_pushstring( L, rpath != NULL ? rpath : "" );
    lua_pushstring( L, fpath != NULL ? fpath : "" );
    rv = APR_SUCCESS;
  }
  return ape_status( L, 2, rv );
}

static int ape_filepath_merge( lua_State* L ) {
  char const* rpath = luaL_checkstring( L, 1 );
  char const* apath = luaL_checkstring( L, 2 );
  apr_int32_t flags = ape_flag_get_fpflags( L, 3 );
  apr_pool_t** pool = ape_opt_pool( L, 4 );
  char* newpath = NULL;
  apr_status_t rv = apr_filepath_merge( &newpath, rpath, apath, flags, *pool );
  if( rv == APR_SUCCESS )
    lua_pushstring( L, newpath != NULL ? newpath : "" );
  return ape_status( L, 1, rv );
}

static int ape_filepath_list_split( lua_State* L ) {
  char const* s = luaL_checkstring( L, 1 );
  apr_pool_t** pool = ape_opt_pool( L, 2 );
  apr_array_header_t* array = NULL;
  apr_status_t rv = apr_filepath_list_split( &array, s, *pool );
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

static int ape_filepath_list_merge( lua_State* L ) {
  apr_status_t rv = APR_ENOMEM;
  luaL_checktype( L, 1, LUA_TTABLE );
  {
    apr_pool_t** pool = ape_opt_pool( L, 2 );
    size_t n = moon_compat_rawlen( L, 1 );
    apr_array_header_t* array = apr_array_make( *pool, n, sizeof( char const* ) );
    if( array != NULL ) {
      size_t i = 0;
      char* fp = NULL;
      for( i = 0; i < n; ++i ) {
        lua_rawgeti( L, 1, i+1 );
        if( lua_isstring( L, -1 ) )
          *(char const**)apr_array_push( array ) = lua_tostring( L, -1 );
        lua_pop( L, 1 );
      }
      rv = apr_filepath_list_merge( &fp, array, *pool );
      if( rv == APR_SUCCESS )
        lua_pushstring( L, fp != NULL ? fp : "" );
    }
  }
  return ape_status( L, 1, rv );
}

static int ape_filepath_get( lua_State* L ) {
  apr_int32_t flags = ape_flag_get_fpflags( L, 1 );
  apr_pool_t** pool = ape_opt_pool( L, 2 );
  char* res = NULL;
  apr_status_t rv = apr_filepath_get( &res, flags, *pool );
  if( rv == APR_SUCCESS )
    lua_pushstring( L, res != NULL ? res : "" );
  return ape_status( L, 1, rv );
}

static int ape_filepath_set( lua_State* L ) {
  char const* path = luaL_checkstring( L, 1 );
  apr_pool_t** pool = ape_opt_pool( L, 2 );
  return ape_status( L, -1, apr_filepath_set( path, *pool ) );
}

static int ape_filepath_encoding( lua_State* L ) {
  apr_pool_t** pool = ape_opt_pool( L, 1 );
  int enc = APR_FILEPATH_ENCODING_UNKNOWN;
  apr_status_t rv = apr_filepath_encoding( &enc, *pool );
  if( rv == APR_SUCCESS ) {
    switch( enc ) {
      case APR_FILEPATH_ENCODING_UTF8:
        lua_pushliteral( L, "UTF-8" );
        break;
      case APR_FILEPATH_ENCODING_LOCALE:
        lua_pushstring( L, apr_os_locale_encoding( *pool ) );
        break;
      default:
        lua_pushliteral( L, "unknown" );
        break;
    }
  }
  return ape_status( L, 1, rv );
}

static int ape_filepath_name_get( lua_State* L ) {
  char const* path = luaL_checkstring( L, 1 );
  lua_pushstring( L, apr_filepath_name_get( path ) );
  return 1;
}


/***
File path manipulation.
@section filepath
*/
static luaL_Reg const ape_fpath_functions[] = {
/***
Extracts the root of a given file path.
@function filepath_root
@tparam string path a file path
@param flags combination of `FILEPATH_*` flags
@tparam[opt] apr_pool_t pool a memory pool for temporary allocations
@treturn string,string the root path and a file path relative to that
@treturn nil,string nil and an error message in case of an error
*/
  { "filepath_root", ape_filepath_root },
/***
Merges two file paths together.
@function filepath_merge
@tparam string path1 a file path
@tparam string path2 another file path
@param flags combination of `FILEPATH_*` flags
@tparam[opt] apr_pool_t pool a memory pool for temporary allocations
@treturn string the merged file path
@treturn nil,string nil and an error message in case of an error
*/
  { "filepath_merge", ape_filepath_merge },
/***
Splits an encoded list of file paths (like in the `PATH` environment
variable) into its components.
@function filepath_list_split
@tparam string list encoded file path list
@tparam[opt] apr_pool_t pool a memory pool for temporary allocations
@treturn table an array of file paths
@treturn nil,string nil and an error message in case of an error
*/
  { "filepath_list_split", ape_filepath_list_split },
/***
Merges an array of file paths into a single string.
@function filepath_list_merge
@tparam table fpaths an array of file paths
@tparam[opt] apr_pool_t pool a memory pool for temporary allocations
@treturn string the encoded list of file paths
@treturn nil,string nil and an error message in case of an error
*/
  { "filepath_list_merge", ape_filepath_list_merge },
/***
Returns the current working directory (cwd).
@function filepath_get
@param flags combination of `FILEPATH_*` flags
@tparam[opt] apr_pool_t pool a memory pool for temporary allocations
@treturn string the current working directory
@treturn nil,string nil and an error message in case of an error
*/
  { "filepath_get", ape_filepath_get },
/***
Changes the current working directory (cwd).
@function filepath_set
@tparam string path the path to chdir to
@tparam[opt] apr_pool_t pool a memory pool for temporary allocations
@treturn boolean true on success
@treturn nil,string nil and an error message in case of an error
*/
  { "filepath_set", ape_filepath_set },
/***
Returns the encoding file paths on this OS.
@function filepath_encoding
@tparam[opt] apr_pool_t pool a memory pool for temporary allocations
@treturn string the name of the encoding
*/
  { "filepath_encoding", ape_filepath_encoding },
/***
Strips the directory part of a file path and returns the file name.
@function filepath_name_get
@tparam string path a file path
@treturn string the file part of the path
*/
  { "filepath_name_get", ape_filepath_name_get },
  { NULL, NULL }
};


#define define_flag( L, name, suffix ) \
  do { \
    ape_flag_new_ ## suffix( (L), APR_ ## name ); \
    lua_setfield( (L), -2, #name ); \
  } while( 0 )

APE_API void ape_fpath_setup( lua_State* L ) {
/***
@class field
@name FILEPATH_NULL
*/
  ape_flag_new_fpflags( L, 0 );
  lua_setfield( L, -2, "FILEPATH_NULL" );
/***
@class field
@name FILEPATH_NOTABOVEROOT
*/
  define_flag( L, FILEPATH_NOTABOVEROOT, fpflags );
/***
@class field
@name FILEPATH_SECUREROOT
*/
  define_flag( L, FILEPATH_SECUREROOT, fpflags );
/***
@class field
@name FILEPATH_NOTRELATIVE
*/
  define_flag( L, FILEPATH_NOTRELATIVE, fpflags );
/***
@class field
@name FILEPATH_NOTABSOLUTE
*/
  define_flag( L, FILEPATH_NOTABSOLUTE, fpflags );
/***
@class field
@name FILEPATH_NATIVE
*/
  define_flag( L, FILEPATH_NATIVE, fpflags );
/***
@class field
@name FILEPATH_TRUENAME
*/
  define_flag( L, FILEPATH_TRUENAME, fpflags );
  moon_compat_register( L, ape_fpath_functions );
}

#undef define_flag

