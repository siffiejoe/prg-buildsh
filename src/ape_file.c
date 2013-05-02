/***
@module ape
*/
#include <stddef.h>
#include <assert.h>
#include "lua.h"
#include "lauxlib.h"
#include "moon.h"
#include "ape.h"
#include "apr_file_io.h"
#include "apr_file_info.h"
#include "apr_thread_proc.h"
#include "apr_strings.h"

/***
File/directory handling.
@section file
*/
/***
Userdata type for APR file handles.
@type apr_file_t
*/
/***
Userdata type for APR directory handles.
@type apr_dir_t
*/

#define APE_FLAG_NAME "apr_fileattrs_t"
#define APE_FLAG_TYPE apr_fileattrs_t
#define APE_FLAG_SUFFIX fattrs
#include "ape_flag.h"

#define APE_FLAG_NAME "apr_oflags_t"
#define APE_FLAG_TYPE apr_int32_t
#define APE_FLAG_SUFFIX oflags
#include "ape_flag.h"

#define APE_FLAG_NAME "apr_fileperms_t"
#define APE_FLAG_TYPE apr_fileperms_t
#define APE_FLAG_SUFFIX fperms
#include "ape_flag.h"

#define APE_FLAG_NAME "apr_flock_t"
#define APE_FLAG_TYPE int
#define APE_FLAG_SUFFIX flock
#include "ape_flag.h"


#define check_flag( fname, aprname ) \
  do { \
    lua_getfield( L, index, (fname) ); \
    if( !lua_isnil( L, -1 ) ) want |= (aprname); \
    lua_pop( L, 1 ); \
  } while( 0 )

#define push_str_or_bool( L, str ) \
  do { \
    lua_State* _l = (L); \
    char const* _s = (str); \
    if( (str) != NULL ) { \
      lua_pushstring( _l, _s ); \
    } else { \
      lua_pushboolean( _l, 0 ); \
    } \
  } while( 0 )

#define push_if_flag( fname, aprname, push ) \
  do { \
    if( want & (aprname) ) { \
      if( fi->valid & (aprname) ) { \
        push; \
      } else { \
        lua_pushboolean( L, 0 ); \
      } \
      lua_setfield( L, index, (fname) ); \
    } \
  } while( 0 )

static apr_int32_t table2want( lua_State* L, int index ) {
  apr_int32_t want = 0;
  luaL_checktype( L, index, LUA_TTABLE );
  check_flag( "link", APR_FINFO_LINK );
  check_flag( "mtime", APR_FINFO_MTIME );
  check_flag( "ctime", APR_FINFO_CTIME );
  check_flag( "atime", APR_FINFO_ATIME );
  check_flag( "size", APR_FINFO_SIZE );
  check_flag( "csize", APR_FINFO_CSIZE );
  check_flag( "nlink", APR_FINFO_NLINK );
  check_flag( "type", APR_FINFO_TYPE );
  check_flag( "user", APR_FINFO_USER );
  check_flag( "group", APR_FINFO_GROUP );
  check_flag( "prot", APR_FINFO_PROT );
  check_flag( "icase", APR_FINFO_ICASE );
  check_flag( "name", APR_FINFO_NAME );
  return want;
}

static char const* type2string( apr_filetype_e t ) {
  switch( t ) {
    case APR_REG: return "regular file";
    case APR_DIR: return "directory";
    case APR_CHR: return "character device";
    case APR_BLK: return "block device";
    case APR_PIPE: return "pipe";
    case APR_LNK: return "symbolic link";
    case APR_SOCK: return "socket";
    case APR_NOFILE: return "no file";
    default:
      return "unknown";
  }
}

static void finfo2table( lua_State* L, int index, apr_int32_t want,
                         apr_finfo_t const* fi, int pindex ) {
  assert( index > 0 || !"absolute stack index required" );
  assert( pindex > 0 || !"absolute stack index required" );
  push_if_flag( "link", APR_FINFO_LINK, lua_pushboolean( L, 0 ) );
  push_if_flag( "mtime", APR_FINFO_MTIME, ape_time_userdata( L, fi->mtime ) );
  push_if_flag( "ctime", APR_FINFO_CTIME, ape_time_userdata( L, fi->ctime ) );
  push_if_flag( "atime", APR_FINFO_ATIME, ape_time_userdata( L, fi->atime ) );
  push_if_flag( "size", APR_FINFO_SIZE, lua_pushnumber( L, (lua_Number)fi->size ) );
  push_if_flag( "csize", APR_FINFO_CSIZE, lua_pushnumber( L, (lua_Number)fi->csize ) );
  push_if_flag( "nlink", APR_FINFO_NLINK, lua_pushnumber( L, (lua_Number)fi->nlink ) );
  push_if_flag( "type", APR_FINFO_TYPE, lua_pushstring( L, type2string( fi->filetype ) ) );
#ifdef APR_HAS_USER
  push_if_flag( "user", APR_FINFO_USER, ape_uid_userdata( L, fi->user, pindex ) );
  push_if_flag( "group", APR_FINFO_GROUP, ape_gid_userdata( L, fi->group, pindex ) );
#else
  push_if_flag( "user", APR_FINFO_USER, lua_pushboolean( L, 0 ) );
  push_if_flag( "group", APR_FINFO_GROUP, lua_pushboolean( L, 0 ) );
#endif
  push_if_flag( "prot", APR_FINFO_PROT, ape_flag_new_fperms( L, fi->protection ) );
  push_if_flag( "icase", APR_FINFO_ICASE, lua_pushboolean( L, 0 ) );
  push_if_flag( "name", APR_FINFO_NAME, push_str_or_bool( L, fi->name ) );
}

#undef check_flag
#undef push_str_or_bool
#undef push_if_flag

static int ape_file_close( lua_State* L ) {
  apr_file_t** file = moon_checkudata( L, 1, APE_FILE_NAME );
  apr_status_t rv = APR_EBADF;
  if( *file != NULL ) {
    rv = apr_file_close( *file );
    if( rv == APR_SUCCESS )
      *file = NULL;
  }
  return ape_status( L, -1, rv );
}

static int ape_file_remove( lua_State* L ) {
  char const* path = luaL_checkstring( L, 1 );
  apr_pool_t** pool = ape_opt_pool( L, 2 );
  return ape_status( L, -1, apr_file_remove( path, *pool ) );
}

static int ape_file_rename( lua_State* L ) {
  char const* fpath = luaL_checkstring( L, 1 );
  char const* tpath = luaL_checkstring( L, 2 );
  apr_pool_t** pool = ape_opt_pool( L, 3 );
  return ape_status( L, -1, apr_file_rename( fpath, tpath, *pool ) );
}

static int ape_file_link( lua_State* L ) {
  char const* fpath = luaL_checkstring( L, 1 );
  char const* tpath = luaL_checkstring( L, 2 );
  return ape_status( L, -1, apr_file_link( fpath, tpath ) );
}

static int ape_file_copy( lua_State* L ) {
  char const* fpath = luaL_checkstring( L, 1 );
  char const* tpath = luaL_checkstring( L, 2 );
  apr_fileperms_t perms = ape_flag_get_fperms( L, 3 );
  apr_pool_t** pool = ape_opt_pool( L, 4 );
  return ape_status( L, -1, apr_file_copy( fpath, tpath, perms, *pool ) );
}

static int ape_file_append( lua_State* L ) {
  char const* fpath = luaL_checkstring( L, 1 );
  char const* tpath = luaL_checkstring( L, 2 );
  apr_fileperms_t perms = ape_flag_get_fperms( L, 3 );
  apr_pool_t** pool = ape_opt_pool( L, 4 );
  return ape_status( L, -1, apr_file_append( fpath, tpath, perms, *pool ) );
}

static int ape_file_eof( lua_State* L ) {
  apr_file_t** file = moon_checkudata( L, 1, APE_FILE_NAME );
  apr_status_t rv = APR_EBADF;
  if( *file != NULL ) {
    rv = apr_file_eof( *file );
    if( rv == APR_SUCCESS ) {
      lua_pushboolean( L, 0 );
      return 1;
    } else if( APR_STATUS_IS_EOF( rv ) ) {
      lua_pushboolean( L, 1 );
      return 1;
    }
  }
  return ape_status( L, 0, rv );
}

static int ape_file_read( lua_State* L ) {
  apr_file_t** file = moon_checkudata( L, 1, APE_FILE_NAME );
  apr_size_t size = (apr_size_t)luaL_checknumber( L, 2 );
  char buf[ 1024 ];
  char* ptr = buf;
  apr_status_t rv = APR_EBADF;
  if( *file != NULL ) {
    if( size > sizeof( buf ) )
      ptr = lua_newuserdata( L, size );
    rv = apr_file_read( *file, ptr, &size );
    if( rv == APR_SUCCESS )
      lua_pushlstring( L, ptr, size );
  }
  return ape_status( L, 1, rv );
}

static int ape_file_write( lua_State* L ) {
  apr_file_t** file = moon_checkudata( L, 1, APE_FILE_NAME );
  size_t len = 0;
  char const* s = luaL_checklstring( L, 2, &len );
  apr_size_t size = (apr_size_t)luaL_optnumber( L, 3, len );
  apr_status_t rv = APR_EBADF;
  if( size > len )
    size = len;
  if( *file != NULL ) {
    rv = apr_file_write( *file, s, &size );
    if( rv == APR_SUCCESS )
      lua_pushnumber( L, (lua_Number)size );
  }
  return ape_status( L, 1, rv );
}

static int ape_file_puts( lua_State* L ) {
  apr_file_t** file = moon_checkudata( L, 1, APE_FILE_NAME );
  char const* str = luaL_checkstring( L, 2 );
  apr_status_t rv = APR_EBADF;
  if( *file != NULL )
    rv = apr_file_puts( str, *file );
  return ape_status( L, -1, rv );
}

static int ape_file_flush( lua_State* L ) {
  apr_file_t** file = moon_checkudata( L, 1, APE_FILE_NAME );
  apr_status_t rv = APR_EBADF;
  if( *file != NULL )
    rv = apr_file_flush( *file );
  return ape_status( L, -1, rv );
}

static int ape_file_sync( lua_State* L ) {
  apr_file_t** file = moon_checkudata( L, 1, APE_FILE_NAME );
  apr_status_t rv = APR_EBADF;
  if( *file != NULL )
    rv = apr_file_sync( *file );
  return ape_status( L, -1, rv );
}

static int ape_file_datasync( lua_State* L ) {
  apr_file_t** file = moon_checkudata( L, 1, APE_FILE_NAME );
  apr_status_t rv = APR_EBADF;
  if( *file != NULL )
    rv = apr_file_datasync( *file );
  return ape_status( L, -1, rv );
}

static int ape_file_dup( lua_State* L );

static int ape_file_seek( lua_State* L ) {
  static char const* const names[] = {
    "set", "cur", "end", NULL
  };
  static apr_seek_where_t const flags[] = {
    APR_SET, APR_CUR, APR_END
  };
  apr_file_t** file = moon_checkudata( L, 1, APE_FILE_NAME );
  apr_seek_where_t where = flags[ luaL_checkoption( L, 2, NULL, names ) ];
  apr_off_t off = (apr_off_t)luaL_checknumber( L, 3 );
  apr_status_t rv = APR_EBADF;
  if( *file != NULL ) {
    rv = apr_file_seek( *file, where, &off );
    lua_pushnumber( L, (lua_Number)off );
  }
  return ape_status( L, 1, rv );
}

static int ape_file_namedpipe_create( lua_State* L ) {
  char const* name = luaL_checkstring( L, 1 );
  apr_fileperms_t perms = ape_flag_get_fperms( L, 2 );
  apr_pool_t** pool = ape_opt_pool( L, 3 );
  return ape_status( L, -1, apr_file_namedpipe_create( name, perms, *pool ) );
}

static int ape_file_pipe_timeout_get( lua_State* L ) {
  apr_file_t** file = moon_checkudata( L, 1, APE_FILE_NAME );
  apr_interval_time_t t = 0;
  apr_status_t rv = APR_EBADF;
  if( *file != NULL ) {
    rv = apr_file_pipe_timeout_get( *file, &t );
    if( rv == APR_SUCCESS )
      lua_pushnumber( L, (lua_Number)t );
  }
  return ape_status( L, 1, rv );
}

static int ape_file_pipe_timeout_set( lua_State* L ) {
  apr_file_t** file = moon_checkudata( L, 1, APE_FILE_NAME );
  apr_interval_time_t t = (apr_interval_time_t)luaL_checknumber( L, 2 );
  apr_status_t rv = APR_EBADF;
  if( *file != NULL )
    rv = apr_file_pipe_timeout_set( *file, t );
  return ape_status( L, -1, rv );
}

static int ape_file_lock( lua_State* L ) {
  apr_file_t** file = moon_checkudata( L, 1, APE_FILE_NAME );
  int flag = ape_flag_get_flock( L, 2 );
  apr_status_t rv = APR_EBADF;
  if( *file != NULL )
    rv = apr_file_lock( *file, flag );
  return ape_status( L, -1, rv );
}

static int ape_file_unlock( lua_State* L ) {
  apr_file_t** file = moon_checkudata( L, 1, APE_FILE_NAME );
  apr_status_t rv = APR_EBADF;
  if( *file != NULL )
    rv = apr_file_unlock( *file );
  return ape_status( L, -1, rv );
}

static int ape_file_name_get( lua_State* L ) {
  apr_file_t** file = moon_checkudata( L, 1, APE_FILE_NAME );
  char const* name = NULL;
  apr_status_t rv = APR_EBADF;
  if( *file != NULL ) {
    rv = apr_file_name_get( &name, *file );
    if( rv == APR_SUCCESS )
      lua_pushstring( L, name != NULL ? name : "" );
  }
  return ape_status( L, 1, rv );
}

static int ape_file_flags_get( lua_State* L ) {
  apr_file_t** file = moon_checkudata( L, 1, APE_FILE_NAME );
  if( *file == NULL )
    return ape_status( L, 0, APR_EBADF );
  ape_flag_new_oflags( L, apr_file_flags_get( *file ) );
  return 1;
}

static int ape_file_perms_set( lua_State* L ) {
  char const* path = luaL_checkstring( L, 1 );
  apr_fileperms_t perms = ape_flag_get_fperms( L, 2 );
  return ape_status( L, -1, apr_file_perms_set( path, perms ) );
}

static int ape_file_attrs_set( lua_State* L ) {
  char const* path = luaL_checkstring( L, 1 );
  apr_fileattrs_t attr = ape_flag_get_fattrs( L, 2 );
  apr_fileattrs_t amask = ape_flag_get_fattrs( L, 3 );
  apr_pool_t** pool = ape_opt_pool( L, 4 );
  return ape_status( L, -1, apr_file_attrs_set( path, attr, amask, *pool ) );
}

static int ape_file_mtime_set( lua_State* L ) {
  char const* path = luaL_checkstring( L, 1 );
  apr_time_t* t = moon_checkudata( L, 2, APE_TIME_NAME );
  apr_pool_t** pool = ape_opt_pool( L, 3 );
  return ape_status( L, -1, apr_file_mtime_set( path, *t, *pool ) );
}

static int ape_file_info_get( lua_State* L ) {
  apr_file_t** file = moon_checkudata( L, 1, APE_FILE_NAME );
  apr_int32_t wanted = table2want( L, 2 );
  apr_status_t rv = APR_EBADF;
  lua_settop( L, 2 );
  if( *file != NULL ) {
    apr_finfo_t info;
    rv = apr_file_info_get( &info, wanted, *file );
    if( rv == APR_SUCCESS || APR_STATUS_IS_INCOMPLETE( rv ) ) {
      moon_compat_getuservalue( L, 1 );
      lua_rawgeti( L, -1, 1 );
      lua_replace( L, -2 );
      finfo2table( L, 2, wanted, &info, 3 );
      rv = APR_SUCCESS;
    }
  }
  lua_settop( L, 2 );
  return ape_status( L, 1, rv );
}

static int ape_file_trunc( lua_State* L ) {
  apr_file_t** file = moon_checkudata( L, 1, APE_FILE_NAME );
  lua_Number off = luaL_checknumber( L, 2 );
  apr_status_t rv = APR_EBADF;
  if( *file != NULL )
    rv = apr_file_trunc( *file, (apr_off_t)off );
  return ape_status( L, -1, rv );
}

static int ape_file_inherit_set( lua_State* L ) {
  apr_file_t** file = moon_checkudata( L, 1, APE_FILE_NAME );
  apr_status_t rv = APR_EBADF;
  if( *file != NULL )
    rv = apr_file_inherit_set( *file );
  return ape_status( L, -1, rv );
}

static int ape_file_inherit_unset( lua_State* L ) {
  apr_file_t** file = moon_checkudata( L, 1, APE_FILE_NAME );
  apr_status_t rv = APR_EBADF;
  if( *file != NULL )
    rv = apr_file_inherit_unset( *file );
  return ape_status( L, -1, rv );
}

static int ape_dir_make( lua_State* L ) {
  char const* path = luaL_checkstring( L, 1 );
  apr_fileperms_t perms = ape_flag_get_fperms( L, 2 );
  apr_pool_t** pool = ape_opt_pool( L, 3 );
  return ape_status( L, -1, apr_dir_make( path, perms, *pool ) );
}

static int ape_dir_make_recursive( lua_State* L ) {
  char const* path = luaL_checkstring( L, 1 );
  apr_fileperms_t perms = ape_flag_get_fperms( L, 2 );
  apr_pool_t** pool = ape_opt_pool( L, 3 );
  return ape_status( L, -1, apr_dir_make_recursive( path, perms, *pool ) );
}

static int ape_dir_remove( lua_State* L ) {
  char const* path = luaL_checkstring( L, 1 );
  apr_pool_t** pool = ape_opt_pool( L, 2 );
  return ape_status( L, -1, apr_dir_remove( path, *pool ) );
}

static int ape_temp_dir_get( lua_State* L ) {
  apr_pool_t** pool = ape_opt_pool( L, 1 );
  char const* s = NULL;
  apr_status_t rv = apr_temp_dir_get( &s, *pool );
  if( rv == APR_SUCCESS )
    lua_pushstring( L, s != NULL ? s : "" );
  return ape_status( L, 1, rv );
}

static int ape_stat( lua_State* L ) {
  char const* name = luaL_checkstring( L, 1 );
  apr_int32_t wanted = table2want( L, 2 );
  apr_pool_t** pool = moon_checkudata( L, 3, APE_POOL_NAME );
  apr_finfo_t info;
  apr_status_t rv = apr_stat( &info, name, wanted, *pool );
  if( rv == APR_SUCCESS || APR_STATUS_IS_INCOMPLETE( rv ) ) {
    finfo2table( L, 2, wanted, &info, 3 );
    lua_pushvalue( L, 2 );
    rv = APR_SUCCESS;
  }
  return ape_status( L, 1, rv );
}

static int ape_dir_open( lua_State* L );

static int ape_dir_close( lua_State* L ) {
  apr_dir_t** dir = moon_checkudata( L, 1, APE_DIR_NAME );
  apr_status_t rv = APR_EBADF;
  if( *dir != NULL ) {
    rv = apr_dir_close( *dir );
    if( rv == APR_SUCCESS ) {
      *dir = NULL;
      moon_compat_getuservalue( L, 1 );
      if( lua_istable( L, -1 ) ) {
        lua_pushnil( L );
        lua_rawseti( L, -2, 1 );
      }
      lua_pop( L, 1 );
    }
  }
  return ape_status( L, -1, rv );
}

static int ape_dir_read( lua_State* L ) {
  apr_dir_t** dir = moon_checkudata( L, 1, APE_DIR_NAME );
  apr_int32_t wanted = table2want( L, 2 );
  apr_status_t rv = APR_EBADF;
  lua_settop( L, 2 );
  if( *dir != NULL ) {
    apr_finfo_t info;
    rv = apr_dir_read( &info, wanted, *dir );
    if( rv == APR_SUCCESS || APR_STATUS_IS_INCOMPLETE( rv ) ) {
      moon_compat_getuservalue( L, 1 );
      lua_rawgeti( L, -1, 1 );
      lua_replace( L, -2 );
      finfo2table( L, 2, wanted, &info, 3 );
      rv = APR_SUCCESS;
    } else if( APR_STATUS_IS_ENOENT( rv ) ) {
      lua_pushnil( L );
      lua_replace( L, 2 );
      rv = APR_SUCCESS;
    }
  }
  lua_settop( L, 2 );
  return ape_status( L, 1, rv );
}

static int ape_dir_rewind( lua_State* L ) {
  apr_dir_t** dir = moon_checkudata( L, 1, APE_DIR_NAME );
  apr_status_t rv = APR_EBADF;
  if( *dir != NULL )
    rv = apr_dir_rewind( *dir );
  return ape_status( L, -1, rv );
}


/* apr_file_t object*/
static void ape_file_init( void* p ) {
  apr_file_t** file = p;
  *file = NULL;
}

static int ape_file_gc( lua_State* L ) {
  apr_file_t** file = moon_checkudata( L, 1, APE_FILE_NAME );
  if( *file != NULL )
    apr_file_close( *file );
  return 0;
}

static luaL_Reg const ape_file_metamethods[] = {
  { "__gc", ape_file_gc },
  { NULL, NULL }
};

/***
Userdata type for APR file handles.
@type apr_file_t
*/
static luaL_Reg const ape_file_methods[] = {
/***
Closes the file.
@function close
@treturn boolean true on success
@treturn nil,string nil and an error message in case of an error
*/
  { "close", ape_file_close },
/***
Checks if the file stream has reached the end of file (EOF).
@function eof
@treturn boolean true for EOF, false otherwise
@treturn nil,string nil and an error message in case of an error
*/
  { "eof", ape_file_eof },
/***
Reads a number of bytes from the file object.
@function read
@tparam number size number of bytes to read (at most)
@treturn string the bytes read (can be less than `size`)
@treturn nil,string nil and an error message in case of an error
*/
  { "read", ape_file_read },
/***
Writes a number of bytes to the file.
@function write
@tparam string str the bytes to write
@tparam[opt=#str] number number of bytes to write
@treturn number number of bytes actually written
@treturn nil,string nil and an error message in case of an error
*/
  { "write", ape_file_write },
/***
Writes a string to the file.
@function puts
@tparam string str the string to write
@treturn boolean true on success
@treturn nil,string nil and an error message in case of an error
*/
  { "puts", ape_file_puts },
/***
Flushes all buffers for the given file.
@function flush
@treturn boolean true on success
@treturn nil,string nil and an error message in case of an error
*/
  { "flush", ape_file_flush },
/***
Makes sure that all written (meta) data is synced to disk.
@function sync
@treturn boolean true on success
@treturn nil,string nil and an error message in case of an error
*/
  { "sync", ape_file_sync },
/***
Makes sure that all written data is synced to disk.
@function datasync
@treturn boolean true on success
@treturn nil,string nil and an error message in case of an error
*/
  { "datasync", ape_file_datasync },
/***
Duplicates the file object.
@function dup
@treturn apr_file_t the duplicated file object
@treturn nil,string nil and an error message in case of an error
*/
  { "dup", ape_file_dup },
/***
Moves the current read/write position in the file.
@function seek
@tparam string where one of `set`, `cur, `end`
@tparam number offset offset relative to `where`
@treturn number the actual offset after the seek operation
@treturn nil,string nil and an error message in case of an error
*/
  { "seek", ape_file_seek },
/***
Returns the current timeout for the pipe.
@function timeout
@treturn number the current timeout value for this pipe
@treturn nil,string nil and an error message in case of an error
*/
  { "timeout", ape_file_pipe_timeout_get },
/***
@function timeout_set
*/
  { "timeout_set", ape_file_pipe_timeout_set },
/***
Locks the file.
@function lock
@param flags lock flags (`FLOCK_*` flags)
@treturn boolean true on success
@treturn nil,string nil and an error message in case of an error
*/
  { "lock", ape_file_lock },
/***
Unlocks a file.
@function file_unlock
@treturn boolean true on success
@treturn nil,string nil and an error message in case of an error
*/
  { "unlock", ape_file_unlock },
/***
@function name
*/
  { "name", ape_file_name_get },
/***
@function flags
*/
  { "flags", ape_file_flags_get },
/***
@function perms_set
*/
  { "perms_set", ape_file_perms_set },
/***
@function attrs_set
*/
  { "attrs_set", ape_file_attrs_set },
/***
@function mtime_set
*/
  { "mtime_set", ape_file_mtime_set },
/***
@function info
*/
  { "info", ape_file_info_get },
/***
@function trunc
*/
  { "trunc", ape_file_trunc },
/***
@function inherit_set
*/
  { "inherit_set", ape_file_inherit_set },
/***
@function inherit_unset
*/
  { "inherit_unset", ape_file_inherit_unset },
  { NULL, NULL }
};

static ape_object_type const ape_file_type = {
  APE_FILE_NAME,
  sizeof( apr_file_t* ),
  ape_file_init,
  1,
  ape_file_metamethods,
  ape_file_methods
};


static int ape_file_open( lua_State* L ) {
  char const* name = luaL_checkstring( L, 1 );
  apr_int32_t flags = ape_flag_get_oflags( L, 2 );
  apr_fileperms_t perms = ape_flag_get_fperms( L, 3 );
  apr_pool_t** pool = moon_checkudata( L, 4, APE_POOL_NAME );
  apr_file_t** file = ape_new_object( L, &ape_file_type, 4 );
  return ape_status( L, 1, apr_file_open( file, name, flags, perms, *pool ) );
}

static int ape_file_open_stderr( lua_State* L ) {
  apr_pool_t** pool = moon_checkudata( L, 1, APE_POOL_NAME );
  apr_file_t** file = ape_new_object( L, &ape_file_type, 1 );
  return ape_status( L, 1, apr_file_open_stderr( file, *pool ) );
}

static int ape_file_open_stdout( lua_State* L ) {
  apr_pool_t** pool = moon_checkudata( L, 1, APE_POOL_NAME );
  apr_file_t** file = ape_new_object( L, &ape_file_type, 1 );
  return ape_status( L, 1, apr_file_open_stdout( file, *pool ) );
}

static int ape_file_open_stdin( lua_State* L ) {
  apr_pool_t** pool = moon_checkudata( L, 1, APE_POOL_NAME );
  apr_file_t** file = ape_new_object( L, &ape_file_type, 1 );
  return ape_status( L, 1, apr_file_open_stdin( file, *pool ) );
}

static int ape_file_open_flags_stderr( lua_State* L ) {
  apr_int32_t flags = ape_flag_get_oflags( L, 1 );
  apr_pool_t** pool = moon_checkudata( L, 2, APE_POOL_NAME );
  apr_file_t** file = ape_new_object( L, &ape_file_type, 2 );
  return ape_status( L, 1, apr_file_open_flags_stderr( file, flags, *pool ) );
}

static int ape_file_open_flags_stdout( lua_State* L ) {
  apr_int32_t flags = ape_flag_get_oflags( L, 1 );
  apr_pool_t** pool = moon_checkudata( L, 2, APE_POOL_NAME );
  apr_file_t** file = ape_new_object( L, &ape_file_type, 2 );
  return ape_status( L, 1, apr_file_open_flags_stdout( file, flags, *pool ) );
}

static int ape_file_open_flags_stdin( lua_State* L ) {
  apr_int32_t flags = ape_flag_get_oflags( L, 1 );
  apr_pool_t** pool = moon_checkudata( L, 2, APE_POOL_NAME );
  apr_file_t** file = ape_new_object( L, &ape_file_type, 2 );
  return ape_status( L, 1, apr_file_open_flags_stdin( file, flags, *pool ) );
}

static int ape_file_dup( lua_State* L ) {
  apr_file_t** oldf = moon_checkudata( L, 1, APE_FILE_NAME );
  apr_pool_t** pool = moon_checkudata( L, 2, APE_POOL_NAME );
  apr_file_t** newf = ape_new_object( L, &ape_file_type, 2 );
  apr_status_t rv = APR_EBADF;
  if( *oldf != NULL )
    rv = apr_file_dup( newf, *oldf, *pool );
  return ape_status( L, 1, rv );
}

static int ape_file_dup2( lua_State* L ) {
  apr_file_t** newf = moon_checkudata( L, 1, APE_FILE_NAME );
  apr_file_t** oldf = moon_checkudata( L, 2, APE_FILE_NAME );
  apr_status_t rv = APR_EBADF;
  if( *newf && *oldf != NULL ) {
    rv = apr_file_dup2( *newf, *oldf, apr_file_pool_get( *newf ) );
    if( rv == APR_SUCCESS )
      lua_pushvalue( L, 1 );
  }
  return ape_status( L, 1, rv );
}

static int ape_file_pipe_create( lua_State* L ) {
  apr_pool_t** pool = moon_checkudata( L, 1, APE_POOL_NAME );
  apr_file_t** inf = ape_new_object( L, &ape_file_type, 1 );
  apr_file_t** outf = ape_new_object( L, &ape_file_type, 1 );
  return ape_status( L, 2, apr_file_pipe_create( inf, outf, *pool ) );
}

static int ape_file_pipe_create_ex( lua_State* L ) {
  static char const* const names[] = {
    "full", "read", "write", "none", NULL
  };
  static apr_int32_t const flags[] = {
    APR_FULL_BLOCK, APR_READ_BLOCK, APR_WRITE_BLOCK, APR_FULL_NONBLOCK
  };
  apr_int32_t bl = flags[ luaL_checkoption( L, 1, NULL, names ) ];
  apr_pool_t** pool = moon_checkudata( L, 2, APE_POOL_NAME );
  apr_file_t** inf = ape_new_object( L, &ape_file_type, 2 );
  apr_file_t** outf = ape_new_object( L, &ape_file_type, 2 );
  return ape_status( L, 2, apr_file_pipe_create_ex( inf, outf, bl, *pool ) );
}

static int ape_file_mktemp( lua_State* L ) {
  char const* templ = luaL_checkstring( L, 1 );
  apr_int32_t flags = ape_flag_get_oflags( L, 2 );
  apr_pool_t** pool = moon_checkudata( L, 3, APE_POOL_NAME );
  char* templcopy = apr_pstrdup( *pool, templ );
  apr_file_t** file = ape_new_object( L, &ape_file_type, 3 );
  apr_status_t rv = APR_ENOMEM;
  if( templcopy != NULL ) {
    rv = apr_file_mktemp( file, templcopy, flags, *pool );
    if( rv == APR_SUCCESS )
      lua_pushstring( L, templcopy );
  }
  return ape_status( L, 2, rv );
}


/* apr_dir_t object */
static int ape_dir_gc( lua_State* L ) {
  apr_dir_t** dir= moon_checkudata( L, 1, APE_DIR_NAME );
  if( *dir != NULL )
    apr_dir_close( *dir );
  return 0;
}

static void ape_dir_init( void* p ) {
  apr_dir_t** dir = p;
  *dir = NULL;
}

static luaL_Reg const ape_dir_metamethods[] = {
  { "__gc", ape_dir_gc },
  { NULL, NULL }
};

/***
Userdata type for APR directory handles.
@type apr_dir_t
*/
static luaL_Reg const ape_dir_methods[] = {
/***
Closes the directory handle.
@function close
@treturn boolean true on success
@treturn nil,string nil and an error message in case of an error
*/
  { "close", ape_dir_close },
/***
@function read
*/
  { "read", ape_dir_read },
/***
Rewinds the directory stream to the beginning.
@function rewind
@treturn boolean true on success
@treturn nil,string nil and an error message in case of an error
*/
  { "rewind", ape_dir_rewind },
  { NULL, NULL }
};

static ape_object_type const ape_dir_type = {
  APE_DIR_NAME,
  sizeof( apr_dir_t* ),
  ape_dir_init,
  1,
  ape_dir_metamethods,
  ape_dir_methods
};


static int ape_dir_open( lua_State* L ) {
  char const* name = luaL_checkstring( L, 1 );
  apr_pool_t** pool = moon_checkudata( L, 2, APE_POOL_NAME );
  apr_dir_t** dir = ape_new_object( L, &ape_dir_type, 2 );
  return ape_status( L, 1, apr_dir_open( dir, name, *pool ) );
}


/***
File/directory handling.
@section file
*/
static luaL_Reg const ape_file_functions[] = {
/***
Opens a file by name/path.
@function file_open
@tparam string name the file name/path
@param flags open flags (`FOPEN_*` flags)
@param perms permission bits (`FPROT_*` flags)
@tparam apr_pool_t pool a memory pool for memory allocations
@treturn apr_file_t a file object
@treturn nil,string nil and an error message in case of an error
*/
  { "file_open", ape_file_open },
/***
Closes a given file.
@function file_close
@tparam apr_file_t file the file object to close
@treturn boolean true on success
@treturn nil,string nil and an error message in case of an error
*/
  { "file_close", ape_file_close },
/***
Removes a given file from the filesystem.
@function file_remove
@tparam string name the file name/path to remove
@tparam[opt] apr_pool_t pool a memory pool for temporary allocations
@treturn boolean true on success
@treturn nil,string nil and an error message in case of an error
*/
  { "file_remove", ape_file_remove },
/***
Renames a file.
@function file_rename
@tparam string oldname the old file name
@tparam string newname the new file name
@tparam[opt] apr_pool_t pool a memory pool for temporary allocations
@treturn boolean true on success
@treturn nil,string nil and an error message in case of an error
*/
  { "file_rename", ape_file_rename },
  { "file_link", ape_file_link },
/***
Copies a file to a new destination
@function file_copy
@tparam string src the existing source file name
@tparam string dest the destination file name
@param perms the file permissions for the new file
@tparam[opt] apr_pool_t pool a memory pool for temporary allocations
@treturn boolean true on success
@treturn nil,string nil and an error message in case of an error
*/
  { "file_copy", ape_file_copy },
/***
Appends the contents of a given file to anther file.
@function file_append
@tparam string src the source file name
@tparam string dest the file to append to
@param perms the file permissions for a new file
@treturn boolean true on success
@treturn nil,string nil and an error message in case of an error
*/
  { "file_append", ape_file_append },
/***
Checks a given file object for the end-of-file marker.
@function file_eof
@tparam apr_file_t file a file object
@treturn boolean true for EOF, false otherwiseo
@treturn nil,string nil and an error message in case of an error
*/
  { "file_eof", ape_file_eof },
/***
Opens the standard error stream.
@function file_open_stderr
@tparam apr_pool_t pool a memory pool for memory allocations
@treturn apr_file_t the file object for stderr
@treturn nil,string nil and an error message in case of an error
*/
  { "file_open_stderr", ape_file_open_stderr },
/***
Opens the standard output stream.
@function file_open_stdout
@tparam apr_pool_t pool a memory pool for memory allocations
@treturn apr_file_t the file object for stdout
@treturn nil,string nil and an error message in case of an error
*/
  { "file_open_stdout", ape_file_open_stdout },
/***
Opens the standard input stream.
@function file_open_stdin
@tparam apr_pool_t pool a memory pool for memory allocations
@treturn apr_file_t the file object for stdin
@treturn nil,string nil and an error message in case of an error
*/
  { "file_open_stdin", ape_file_open_stdin },
/***
Opens the standard error stream with the given flags.
@function file_open_flags_stderr
@param flags open flags (`FOPEN_*` flags)
@tparam apr_pool_t pool a memory pool for memory allocations
@treturn apr_file_t the file object for stderr
@treturn nil,string nil and an error message in case of an error
*/
  { "file_open_flags_stderr", ape_file_open_flags_stderr },
/***
Opens the standard output stream with the given flags.
@function file_open_flags_stdout
@param flags open flags (`FOPEN_*` flags)
@tparam apr_pool_t pool a memory pool for memory allocations
@treturn apr_file_t the file object for stdout
@treturn nil,string nil and an error message in case of an error
*/
  { "file_open_flags_stdout", ape_file_open_flags_stdout },
/***
Opens the standard input stream with the given flags.
@function file_open_flags_stdin
@param flags open flags (`FOPEN_*` flags)
@tparam apr_pool_t pool a memory pool for memory allocations
@treturn apr_file_t the file object for stdin
@treturn nil,string nil and an error message in case of an error
*/
  { "file_open_flags_stdin", ape_file_open_flags_stdin },
/***
Reads a number of bytes from a given file object.
@function file_read
@tparam apr_file_t file a file object
@tparam number size number of bytes to read (at most)
@treturn string the bytes read (can be less than `size`)
@treturn nil,string nil and an error message in case of an error
*/
  { "file_read", ape_file_read },
/***
Writes a number of bytes to a file.
@function file_write
@tparam apr_file_t file a file object
@tparam string str the bytes to write
@tparam[opt=#str] number number of bytes to write
@treturn number number of bytes actually written
@treturn nil,string nil and an error message in case of an error
*/
  { "file_write", ape_file_write },
/***
Writes a string to a file.
@function file_puts
@tparam apr_file_t file a file object
@tparam string str the string to write
@treturn boolean true on success
@treturn nil,string nil and an error message in case of an error
*/
  { "file_puts", ape_file_puts },
/***
Flushes all buffers for the given file.
@function file_flush
@tparam apr_file_t file a file object
@treturn boolean true on success
@treturn nil,string nil and an error message in case of an error
*/
  { "file_flush", ape_file_flush },
/***
Makes sure that all written (meta) data is synced to disk.
@function file_sync
@tparam apr_file_t file a file object
@treturn boolean true on success
@treturn nil,string nil and an error message in case of an error
*/
  { "file_sync", ape_file_sync },
/***
Makes sure that all written data is synced to disk.
@function file_datasync
@tparam apr_file_t file a file object
@treturn boolean true on success
@treturn nil,string nil and an error message in case of an error
*/
  { "file_datasync", ape_file_datasync },
/***
Duplicates a given file object.
@function file_dup
@tparam apr_file_t file a file object
@treturn apr_file_t the duplicated file object
@treturn nil,string nil and an error message in case of an error
*/
  { "file_dup", ape_file_dup },
/***
Duplicates a given file object replacing an already opened file object.
@function file_dup2
@tparam apr_file_t newfile the destination file object to replace
@tparam apr_file_t oldfile a file object to duplicate
@treturn apr_file_t returns`newfile` on success
@treturn nil,string nil and an error message in case of an error
*/
  { "file_dup2", ape_file_dup2 },
/***
Moves the current read/write position in a file.
@function file_seek
@tparam apr_file_t file a file object
@tparam string where one of `set`, `cur, `end`
@tparam number offset offset relative to `where`
@treturn number the actual offset after the seek operation
@treturn nil,string nil and an error message in case of an error
*/
  { "file_seek", ape_file_seek },
/***
@function file_pipe_create
*/
  { "file_pipe_create", ape_file_pipe_create },
/***
@function file_pipe_create_ex
*/
  { "file_pipe_create_ex", ape_file_pipe_create_ex },
/***
@function file_namedpipe_create
*/
  { "file_namedpipe_create", ape_file_namedpipe_create },
/***
Returns the current timeout for a given pipe.
@function file_pipe_timeout_get
@tparam apr_file_t pipe a file object representing a pipe
@treturn number the current timeout value for this pipe
@treturn nil,string nil and an error message in case of an error
*/
  { "file_pipe_timeout_get", ape_file_pipe_timeout_get },
/***
@function file_pipe_timeout_set
*/
  { "file_pipe_timeout_set", ape_file_pipe_timeout_set },
/***
Locks the file.
@function file_lock
@tparam apr_file_t file a file object
@param flags lock flags (`FLOCK_*` flags)
@treturn boolean true on success
@treturn nil,string nil and an error message in case of an error
*/
  { "file_lock", ape_file_lock },
/***
Unlocks a file.
@function file_unlock
@tparam apr_file_t file a file object
@treturn boolean true on success
@treturn nil,string nil and an error message in case of an error
*/
  { "file_unlock", ape_file_unlock },
/***
@function file_name_get
*/
  { "file_name_get", ape_file_name_get },
/***
@function file_flags_get
*/
  { "file_flags_get", ape_file_flags_get },
/***
@function file_perms_set
*/
  { "file_perms_set", ape_file_perms_set },
/***
@function file_attrs_set
*/
  { "file_attrs_set", ape_file_attrs_set },
/***
@function file_mtime_set
*/
  { "file_mtime_set", ape_file_mtime_set },
/***
@function file_info_get
*/
  { "file_info_get", ape_file_info_get },
/***
@function file_trunc
*/
  { "file_trunc", ape_file_trunc },
/***
@function file_inherit_set
*/
  { "file_inherit_set", ape_file_inherit_set },
/***
@function file_inherit_unset
*/
  { "file_inherit_unset", ape_file_inherit_unset },
/***
@function file_mktemp
*/
  { "file_mktemp", ape_file_mktemp },
/***
Creates a directory with the given name.
@function dir_make
@tparam string path a directory path
@param perms permission flags (`FPROT_*` flags)
@tparam[opt] apr_pool_t pool a memory pool for temporary allocations
@treturn boolean true on success
@treturn nil,string nil and an error message in case of an error
*/
  { "dir_make", ape_dir_make },
/***
Creates a directory with the given name and all necessary path
components before.
@function dir_make_recursive
@tparam string path a directory path
@param perms permission flags (`FPROT_*` flags)
@tparam[opt] apr_pool_t pool a memory pool for temporary allocations
@treturn boolean true on success
@treturn nil,string nil and an error message in case of an error
*/
  { "dir_make_recursive", ape_dir_make_recursive },
/***
Removes a given directory if empty.
@function dir_remove
@tparam string path a directory path
@tparam[opt] apr_pool_t pool a memory pool for temporary allocations
@treturn boolean true on success
@treturn nil,string nil and an error message in case of an error
*/
  { "dir_remove", ape_dir_remove },
/***
Returns a directory path intended for temporary files.
@function temp_dir_get
@tparam[opt] apr_pool_t pool a memory pool for temporary allocations
@treturn string a directory path
@treturn nil,string nil and an error message in case of an error
*/
  { "temp_dir_get", ape_temp_dir_get },
/***
@function stat
*/
  { "stat", ape_stat },
/***
Opens a directory for reading.
@function dir_open
@tparam string path a directory path
@tparam apr_pool_t pool a memory pool for memory allocations
@treturn apr_dir_t a directory handle/object
@treturn nil,string nil and an error message in case of an error
*/
  { "dir_open", ape_dir_open },
/***
Closes a directory handle.
@function dir_close
@tparam apr_dir_t a directory handle
@treturn boolean true on success
@treturn nil,string nil and an error message in case of an error
*/
  { "dir_close", ape_dir_close },
/***
@function dir_read
*/
  { "dir_read", ape_dir_read },
/***
Rewinds the directory stream to the beginning.
@function dir_rewind
@tparam apr_dir_t a directory handle
@treturn boolean true on success
@treturn nil,string nil and an error message in case of an error
*/
  { "dir_rewind", ape_dir_rewind },
  { NULL, NULL }
};


#define define_flag( L, name, suffix ) \
  do { \
    ape_flag_new_ ## suffix( (L), APR_ ## name ); \
    lua_setfield( (L), -2, #name ); \
  } while( 0 )

APE_API void ape_file_setup( lua_State* L ) {
/***
@class field
@name FILE_ATTR_READONLY
*/
  define_flag( L, FILE_ATTR_READONLY, fattrs );
/***
@class field
@name FILE_ATTR_EXECUTABLE
*/
  define_flag( L, FILE_ATTR_EXECUTABLE, fattrs );
/***
@class field
@name FILE_ATTR_HIDDEN
*/
  define_flag( L, FILE_ATTR_HIDDEN, fattrs );
/***
@class field
@name FOPEN_NULL
*/
  ape_flag_new_oflags( L, 0 );
  lua_setfield( L, -2, "FOPEN_NULL" );
/***
@class field
@name FOPEN_READ
*/
  define_flag( L, FOPEN_READ, oflags );
/***
@class field
@name FOPEN_WRITE
*/
  define_flag( L, FOPEN_WRITE, oflags );
/***
@class field
@name FOPEN_CREATE
*/
  define_flag( L, FOPEN_CREATE, oflags );
/***
@class field
@name FOPEN_APPEND
*/
  define_flag( L, FOPEN_APPEND, oflags );
/***
@class field
@name FOPEN_TRUNCATE
*/
  define_flag( L, FOPEN_TRUNCATE, oflags );
/***
@class field
@name FOPEN_BINARY
*/
  define_flag( L, FOPEN_BINARY, oflags );
/***
@class field
@name FOPEN_BUFFERED
*/
  define_flag( L, FOPEN_BUFFERED, oflags );
/***
@class field
@name FOPEN_EXCL
*/
  define_flag( L, FOPEN_EXCL, oflags );
/***
@class field
@name FOPEN_DELONCLOSE
*/
  define_flag( L, FOPEN_DELONCLOSE, oflags );
/***
@class field
@name FOPEN_XTHREAD
*/
  define_flag( L, FOPEN_XTHREAD, oflags );
/***
@class field
@name FOPEN_SHARELOCK
*/
  define_flag( L, FOPEN_SHARELOCK, oflags );
/***
@class field
@name FOPEN_LARGEFILE
*/
  define_flag( L, FOPEN_LARGEFILE, oflags );
/***
@class field
@name FOPEN_SPARSE
*/
  define_flag( L, FOPEN_SPARSE, oflags );
/***
@class field
@name FPROT_USETID
*/
  define_flag( L, FPROT_USETID, fperms );
/***
@class field
@name FPROT_UREAD
*/
  define_flag( L, FPROT_UREAD, fperms );
/***
@class field
@name FPROT_UWRITE
*/
  define_flag( L, FPROT_UWRITE, fperms );
/***
@class field
@name FPROT_UEXECUTE
*/
  define_flag( L, FPROT_UEXECUTE, fperms );
/***
@class field
@name FPROT_GSETID
*/
  define_flag( L, FPROT_GSETID, fperms );
/***
@class field
@name FPROT_GREAD
*/
  define_flag( L, FPROT_GREAD, fperms );
/***
@class field
@name FPROT_GWRITE
*/
  define_flag( L, FPROT_GWRITE, fperms );
/***
@class field
@name FPROT_GEXECUTE
*/
  define_flag( L, FPROT_GEXECUTE, fperms );
/***
@class field
@name FPROT_WSTICKY
*/
  define_flag( L, FPROT_WSTICKY, fperms );
/***
@class field
@name FPROT_WREAD
*/
  define_flag( L, FPROT_WREAD, fperms );
/***
@class field
@name FPROT_WWRITE
*/
  define_flag( L, FPROT_WWRITE, fperms );
/***
@class field
@name FPROT_WEXECUTE
*/
  define_flag( L, FPROT_WEXECUTE, fperms );
/***
@class field
@name FPROT_OS_DEFAULT
*/
  define_flag( L, FPROT_OS_DEFAULT, fperms );
/***
@class field
@name FPROT_FILE_SOURCE_PERMS
*/
  define_flag( L, FPROT_FILE_SOURCE_PERMS, fperms );
/***
@class field
@name FLOCK_SHARED
*/
  define_flag( L, FLOCK_SHARED, flock );
/***
@class field
@name FLOCK_EXCLUSIVE
*/
  define_flag( L, FLOCK_EXCLUSIVE, flock );
/***
@class field
@name FLOCK_NONBLOCK
*/
  define_flag( L, FLOCK_NONBLOCK, flock );
  moon_compat_register( L, ape_file_functions );
}

#undef define_flag


APE_API void ape_file_userdata( lua_State* L, apr_file_t* f, int index ) {
  apr_pool_t** pool = moon_checkudata( L, index, APE_POOL_NAME );
  apr_file_t** nf = ape_new_object( L, &ape_file_type, index );
#if 1
  (void)pool;
  *nf = f;
#else
  apr_status_t rv = APR_EBADF;
  if( f != NULL )
    rv = apr_file_dup( nf, f, *pool );
  ape_assert( L, rv, "APR file dup" );
#endif
}

