#ifndef APE_H_
#define APE_H_

#include <stddef.h>
#include <assert.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <apr.h>
#include <apr_errno.h>
#include <apr_pools.h>
#include <apr_file_io.h>
#include <apr_time.h>
#include <apr_random.h>
#include "moon.h"

#ifndef APE_API
#define APE_API MOON_API
#endif


#define ape_assert( L, rv, msg ) \
  do { \
    apr_status_t _rv = (rv); \
    if( _rv != APR_SUCCESS ) { \
      char _buf[ 200 ] = { 0 }; \
      apr_strerror( _rv, _buf, sizeof( _buf ) ); \
      luaL_error( (L), "[%s] %s", (msg), _buf ); \
    } \
  } while( 0 )


/* various names of APR userdata */
#define APE_POOL_NAME        "apr_pool_t"
#define APE_FILE_NAME        "apr_file_t"
#define APE_DIR_NAME         "apr_dir_t"
#define APE_UID_NAME         "apr_uid_t"
#define APE_GID_NAME         "apr_gid_t"
#define APE_TIME_NAME        "apr_time_t"
#define APE_PROCATTR_NAME    "apr_procattr_t"
#define APE_PROC_NAME        "apr_proc_t"
#define APE_CRYPTOHASH_NAME  "apr_crypto_hash_t"


APE_API int ape_status( lua_State* L, int n, apr_status_t rv );
APE_API apr_pool_t** ape_opt_pool( lua_State* L, int i );
APE_API void ape_pool_setup( lua_State* L );
APE_API void ape_errno_setup( lua_State* L );
APE_API void ape_env_setup( lua_State* L );
APE_API void ape_fnmatch_setup( lua_State* L );
APE_API void ape_fpath_setup( lua_State* L );
APE_API void ape_file_userdata( lua_State* L, apr_file_t* f, int index );
APE_API void ape_file_setup( lua_State* L );
#ifdef APR_HAS_USER
#  include "apr_user.h"
APE_API void ape_uid_userdata( lua_State* L, apr_uid_t uid, int index );
APE_API void ape_gid_userdata( lua_State* L, apr_gid_t gid, int index );
#endif
APE_API void ape_user_setup( lua_State* L );
APE_API void ape_time_userdata( lua_State* L, apr_time_t t );
APE_API void ape_time_setup( lua_State* L );
APE_API apr_status_t ape_table2argv( lua_State* L, int index,
                                     char const*** argv,
                                     apr_pool_t* pool );
APE_API apr_status_t ape_table2env( lua_State* L, int index,
                                    char const*** env,
                                    apr_pool_t* pool );
APE_API void ape_proc_setup( lua_State* L );
APE_API apr_crypto_hash_t* ape_check_hash( lua_State* L, int index );
APE_API void ape_random_setup( lua_State* L );
APE_API void ape_extra_setup( lua_State* L );
APE_API int luaopen_ape( lua_State* L );


#endif /* APE_H_ */

