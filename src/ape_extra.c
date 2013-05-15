/***
@module ape
*/
#include <stddef.h>
#include <string.h>
#include "lua.h"
#include "lauxlib.h"
#include "moon.h"
#include "ape.h"
#include "apr.h"
#include "apr_env.h"
#include "apr_lib.h"
#include "apr_strings.h"
#include "apr_fnmatch.h"
#include "apr_file_io.h"
#include "apr_file_info.h"
#include "apr_thread_proc.h"
#include "apr_mmap.h"


static apr_status_t check_read( lua_State* L, char const* path,
                                apr_pool_t* pool ) {
  apr_status_t rv = APR_SUCCESS;
  apr_finfo_t info;
  rv = apr_stat( &info, path, APR_FINFO_UPROT | APR_FINFO_TYPE, pool );
  if( rv != APR_SUCCESS )
    return rv;
  else if( !(info.valid & APR_FINFO_UPROT) ||
           !(info.valid & APR_FINFO_TYPE) )
    return APR_EGENERAL;
  else if( (info.filetype == APR_REG) &&
           (info.protection & APR_FPROT_UREAD) ) {
    lua_pushstring( L, path );
    return APR_SUCCESS;
  }
  return APR_ENOENT;
}

static int ape_extra_find_file( lua_State* L ) {
  int i = 0;
  int n = lua_gettop( L );
  apr_pool_t** pool = NULL;
  apr_status_t rv = APR_SUCCESS;
  if( n == 0 || lua_isstring( L, n ) )
    pool = ape_opt_pool( L, n+1 );
  else {
    pool = moon_checkudata( L, n, APE_POOL_NAME );
    --n;
  }
  for( i = 0; i < n; ++i ) {
    char const* name = luaL_checkstring( L, i+1 );
    rv = check_read( L, name, *pool );
    if( !APR_STATUS_IS_ENOENT( rv ) )
      return ape_status( L, 1, rv );
  }
  lua_pushnil( L );
  return 1;
}


static apr_status_t check_exec( lua_State* L, char const* name,
                                char const* path, apr_pool_t* pool ) {
  apr_status_t rv = APR_SUCCESS;
  apr_finfo_t info;
  rv = apr_stat( &info, path, APR_FINFO_UPROT | APR_FINFO_TYPE, pool );
  if( rv != APR_SUCCESS )
    return rv;
  else if( !(info.valid & APR_FINFO_UPROT) ||
           !(info.valid & APR_FINFO_TYPE) )
    return APR_EGENERAL;
  else if( (info.filetype == APR_REG) &&
           (info.protection & APR_FPROT_UEXECUTE) ) {
    lua_pushstring( L, name );
    lua_pushstring( L, path );
    return APR_SUCCESS;
  }
  return APR_ENOENT;
}

static apr_status_t check_path( lua_State* L, char const* name, int n,
                                char const** path, apr_pool_t* pool ) {
  apr_status_t rv = APR_SUCCESS;
  int i = 0;
  for( i = 0; i < n; ++i ) {
    char* npath = NULL;
    rv = apr_filepath_merge( &npath, path[ i ], name,
                             APR_FILEPATH_TRUENAME | APR_FILEPATH_NATIVE,
                             pool );
    if( rv != APR_SUCCESS )
      return rv;
    else if( npath == NULL )
      return APR_EGENERAL;
    else {
      rv = check_exec( L, name, npath, pool );
      if( !APR_STATUS_IS_ENOENT( rv ) )
        return rv;
    }
  }
  return APR_ENOENT;
}

static int ape_extra_find_exec( lua_State* L ) {
  char* pathenv = NULL;
  int n = lua_gettop( L );
  apr_pool_t** pool = NULL;
  apr_status_t rv = APR_SUCCESS;
  if( n == 0 || lua_isstring( L, n ) )
    pool = ape_opt_pool( L, n+1 );
  else {
    pool = moon_checkudata( L, n, APE_POOL_NAME );
    --n;
  }
  rv = apr_env_get( &pathenv, "PATH", *pool );
  if( rv != APR_SUCCESS )
    return ape_status( L, 0, rv );
  else if( pathenv == NULL )
    return ape_status( L, 0, APR_EGENERAL );
  else {
    apr_array_header_t* array = NULL;
    int have_sep = (strcmp( LUA_DIRSEP, "/" ) != 0);
    rv = apr_filepath_list_split( &array, pathenv, *pool );
    if( rv != APR_SUCCESS )
      return ape_status( L, 0, rv );
    else if( array == NULL )
      return ape_status( L, 0, APR_EGENERAL );
    else {
      int i = 0;
      for( i = 0; i < n; ++i ) {
        char const* name = luaL_checkstring( L, i+1 );
        if( strchr( name, '/' ) != NULL ||
            (have_sep && strstr( name, LUA_DIRSEP ) != NULL) )
          rv = check_exec( L, name, name, *pool );
        else
          rv = check_path( L, name, array->nelts,
                           (char const**)array->elts, *pool );
        if( !APR_STATUS_IS_ENOENT( rv ) )
          return ape_status( L, 2, rv );
      }
    }
    /* not found, but no error either */
    lua_pushnil( L );
    return 1;
  }
}


static int ape_extra_run_collect( lua_State* L ) {
  static char const* const names[] = {
    "shellcmd", "shellcmd/env", "program", "program/env",
    "program/path", NULL
  };
  static apr_cmdtype_e const flags[] = {
    APR_SHELLCMD, APR_SHELLCMD_ENV, APR_PROGRAM, APR_PROGRAM_ENV,
    APR_PROGRAM_PATH
  };
  apr_cmdtype_e ct = flags[ luaL_checkoption( L, 1, NULL, names ) ];
  char const* cmd = luaL_checkstring( L, 2 );
  char const** argv = NULL;
  char const** env = NULL;
  apr_pool_t** pool = NULL;
  apr_procattr_t* pa = NULL;
  apr_status_t rv = APR_SUCCESS;
  luaL_checktype( L, 3, LUA_TTABLE );
  if( !lua_isnoneornil( L, 4 ) )
    luaL_checktype( L, 4, LUA_TTABLE );
  pool = ape_opt_pool( L, 5 );
  rv = apr_procattr_create( &pa, *pool );
  if( rv != APR_SUCCESS )
    return ape_status( L, 0, rv );
  rv = apr_procattr_cmdtype_set( pa, ct );
  if( rv != APR_SUCCESS )
    return ape_status( L, 0, rv );
  rv = apr_procattr_io_set( pa, APR_NO_PIPE, APR_FULL_BLOCK, APR_NO_PIPE );
  if( rv != APR_SUCCESS )
    return ape_status( L, 0, rv );
  if( (rv = ape_table2argv( L, 3, &argv, *pool )) == APR_SUCCESS &&
      (lua_isnoneornil( L, 4 ) ||
       (rv = ape_table2env( L, 4, &env, *pool )) == APR_SUCCESS) ) {
    apr_proc_t proc;
    rv = apr_proc_create( &proc, cmd, argv, env, pa, *pool );
    if( rv == APR_SUCCESS ) {
      int exitstatus = 0;
      apr_exit_why_e ewhy = 0;
      luaL_Buffer buf;
      if( proc.out == NULL )
        return ape_status( L, 0, APR_EGENERAL );
      luaL_buffinit( L, &buf );
      do {
        char* s = luaL_prepbuffer( &buf );
        apr_size_t size = LUAL_BUFFERSIZE;
        rv = apr_file_read( proc.out, s, &size );
        luaL_addsize( &buf, size );
      } while( rv == APR_SUCCESS );
      luaL_pushresult( &buf );
      apr_file_close( proc.out );
      if( !APR_STATUS_IS_EOF( rv ) ) {
        int v = 0;
        apr_proc_wait( &proc, &exitstatus, &ewhy, APR_NOWAIT );
        v = ape_status( L, 0, rv );
        lua_pushnil( L );
        lua_pushvalue( L, -4 );
        return v + 2;
      }
      rv = apr_proc_wait( &proc, &exitstatus, &ewhy, APR_WAIT );
      if( !APR_STATUS_IS_CHILD_DONE( rv ) ) {
        int v = ape_status( L, 0, rv );
        lua_pushnil( L );
        lua_pushvalue( L, -4 );
        return v + 2;
      }
      if( APR_PROC_CHECK_EXIT( ewhy ) ) {
        lua_pushboolean( L, exitstatus == 0 );
        lua_pushliteral( L, "exit" );
      } else {
        lua_pushboolean( L, 0 );
        lua_pushliteral( L, "signal" );
      }
      lua_pushnumber( L, (lua_Number)exitstatus );
      lua_pushvalue( L, -4 );
      return 4;
    }
  }
  return ape_status( L, 0, rv );
}


#define DIRSEPLEN (sizeof( LUA_DIRSEP )-1)

static char const* find_dirsep( char const* path, size_t len,
                                char const** basename ) {
  int have_sep = (strcmp( LUA_DIRSEP, "/" ) != 0);
  *basename = path;
  while( len > 0 ) {
    --len;
    if( (path[ len ] == '/' ) ||
        (have_sep && !strncmp( path+len, LUA_DIRSEP, DIRSEPLEN ) ) ) {
      /* found last directory separator */
      *basename = path + len + 1;
      while( len > 0 ) { /* remove all trailing separators */
        if( path[ len-1 ] == '/' )
          --len;
        else if( have_sep && len >= DIRSEPLEN &&
                 !strncmp( path + len - DIRSEPLEN, LUA_DIRSEP,
                           DIRSEPLEN ) )
          len -= DIRSEPLEN;
        else
          break;
      }
      break;
    }
  }
  return path + len;
}


static int ape_extra_basename( lua_State* L ) {
  size_t len = 0;
  char const* path = luaL_checklstring( L, 1, &len );
  int top = lua_gettop( L );
  int i = 0;
#if 1
  char const* bn = apr_filepath_name_get( path );
  len = strlen( bn );
#else
  char const* bn = NULL;
  find_dirsep( path, len, &bn );
  len -= bn - path;
#endif
  for( i = 2; i <= top; ++i ) {
    size_t slen = 0;
    char const* suffix = luaL_checklstring( L, i, &slen );
    if( len >= slen && strncmp( suffix, bn+len-slen, slen ) == 0 ) {
      lua_pushlstring( L, bn, len - slen );
      return 1;
    }
  }
  lua_pushlstring( L, bn, len );
  return 1;
}


static int ape_extra_dirname( lua_State* L ) {
  char const* path = luaL_checkstring( L, 1 );
  apr_pool_t** pool = ape_opt_pool( L, 2 );
  char const* root = NULL;
  apr_status_t rv = apr_filepath_root( &root, &path,
                                       APR_FILEPATH_NATIVE, *pool );
  if( rv == APR_SUCCESS || APR_STATUS_IS_EINCOMPLETE( rv ) ||
      APR_STATUS_IS_ERELATIVE( rv ) ) {
    char const* basename = NULL;
    char const* separator = find_dirsep( path, strlen( path ), &basename );
    if( APR_STATUS_IS_ERELATIVE( rv ) )
      root = NULL;
    if( root != NULL ) {
      char* reldir = apr_pstrndup( *pool, path, separator-path );
      if( reldir == NULL )
        rv = APR_ENOMEM;
      else {
        char* newpath = NULL;
        rv = apr_filepath_merge( &newpath, root, reldir,
                                 APR_FILEPATH_NATIVE, *pool );
        if( rv == APR_SUCCESS )
          lua_pushstring( L, newpath != NULL ? newpath : "" );
      }
    } else {
      lua_pushlstring( L, path, separator-path );
      rv = APR_SUCCESS;
    }
  }
  return ape_status( L, 1, rv );
}


static int ape_extra_path_glob( lua_State* L ) {
  char const* pattern = NULL;
  apr_pool_t** pool = NULL;
  char const* root = NULL;
  int have_tab = 0;
  int tab_index = 1;
  size_t tab_len = 0;
  apr_status_t rv = APR_SUCCESS;
  if( lua_istable( L, 1 ) ) {
    have_tab = 1;
    tab_len = moon_compat_rawlen( L, 1 );
  }
  pattern = luaL_checkstring( L, 1 + have_tab );
  pool = ape_opt_pool( L, 2 + have_tab );
  rv = apr_filepath_root( &root, &pattern, APR_FILEPATH_NATIVE, *pool );
  if( rv == APR_SUCCESS || APR_STATUS_IS_EINCOMPLETE( rv ) ||
      APR_STATUS_IS_ERELATIVE( rv ) ) {
    char const* fpattern = NULL;
    char const* separator = find_dirsep( pattern, strlen( pattern ), &fpattern );
    char cwd[] = ".";
    char* dirname = cwd;
    apr_dir_t* dir = NULL;
    if( APR_STATUS_IS_ERELATIVE( rv ) )
      root = NULL;
    /* extract directory name (if necessary) and merge it with the
     * root (if necessary) */
    if( root != NULL || separator != pattern ) {
      dirname = apr_pstrndup( *pool, pattern, separator-pattern );
      if( dirname == NULL )
        return ape_status( L, 0, APR_ENOMEM );
      if( root != NULL ) {
        rv = apr_filepath_merge( &dirname, root, dirname,
                                 APR_FILEPATH_NATIVE, *pool );
        if( rv != APR_SUCCESS )
          return ape_status( L, 0, rv );
      }
    }
    if( !have_tab ) {
      lua_newtable( L );
      tab_index = lua_gettop( L );
    }
    /* iterate directory matching all file names */
    rv = apr_dir_open( &dir, dirname, *pool );
    if( rv == APR_SUCCESS ) {
      apr_finfo_t finfo;
      int i = 1;
      while( (rv=apr_dir_read( &finfo, APR_FINFO_NAME, dir )) == APR_SUCCESS ) {
        if( apr_fnmatch( fpattern, finfo.name, 0 ) == APR_SUCCESS &&
            !(finfo.name[ 0 ] == '.' && (finfo.name[ 1 ] == '\0' ||
              (finfo.name[ 1 ] == '.' && finfo.name[ 2 ] == '\0'))) ) {
          if( dirname != cwd ) {
            char* newpath = NULL;
            rv = apr_filepath_merge( &newpath, dirname, finfo.name,
                                     APR_FILEPATH_NATIVE, *pool );
            if( rv != APR_SUCCESS ) {
              apr_dir_close( dir );
              return ape_status( L, 0, rv );
            }
            lua_pushstring( L, newpath );
          } else
            lua_pushstring( L, finfo.name );
          lua_rawseti( L, tab_index, tab_len + i );
          ++i;
        }
      }
      apr_dir_close( dir );
      if( APR_STATUS_IS_ENOENT( rv ) )
        rv = APR_SUCCESS;
      lua_pushvalue( L, tab_index );
    }
  }
  return ape_status( L, 1, rv );
}


static int ape_extra_hash_file( lua_State* L ) {
  apr_crypto_hash_t* h = ape_check_hash( L, 1 );
  char const* fname = luaL_checkstring( L, 2 );
  apr_pool_t** pool = ape_opt_pool( L, 3 );
  apr_file_t* file = NULL;
  apr_finfo_t finfo;
  apr_mmap_t* mmap = NULL;
  apr_status_t rv = APR_SUCCESS;

  rv = apr_file_open( &file, fname, APR_FOPEN_READ,
                      APR_FPROT_OS_DEFAULT, *pool );
  if( rv != APR_SUCCESS )
    return ape_status( L, 0, rv );
  rv = apr_file_info_get( &finfo, APR_FINFO_SIZE|APR_FINFO_TYPE, file );
  if( rv != APR_SUCCESS ) {
    apr_file_close( file );
    return ape_status( L, 0, rv );
  }
  if( finfo.filetype != APR_REG ) {
    apr_file_close( file );
    lua_pushnil( L );
    lua_pushliteral( L, "not a regular file" );
    return 2;
  }
  if( APR_MMAP_CANDIDATE( finfo.size ) ) {
    rv = apr_mmap_create( &mmap, file, 0, (apr_size_t)finfo.size,
                          APR_MMAP_READ, *pool );
    if( rv != APR_SUCCESS ) {
      apr_file_close( file );
      return ape_status( L, 0, rv );
    }
    h->add( h, mmap->mm, mmap->size );
    apr_mmap_delete( mmap );
  } else { /* use normal file io with static buffer */
    char buffer[ 4096 ];
    do {
      apr_size_t size = sizeof( buffer );
      rv = apr_file_read( file, buffer, &size );
      if( rv == APR_SUCCESS && size > 0 )
        h->add( h, buffer, size );
    } while( rv == APR_SUCCESS );
    if( !APR_STATUS_IS_EOF( rv ) ) {
      apr_file_close( file );
      return ape_status( L, 0, rv );
    }
  }
  apr_file_close( file );
  lua_pushboolean( L, 1 );
  return 1;
}


#ifdef APR_HAVE_FCNTL_H
#include <fcntl.h>
#endif


#if (defined( _WIN32 ) || defined( _WIN64 ) || defined( __WINDOWS__ )) && \
     defined( APR_HAS_UNICODE_FS ) && APR_HAS_UNICODE_FS

#include <wctype.h>

typedef apr_uint16_t apr_wchar_t;

APR_DECLARE(apr_status_t) apr_conv_ucs2_to_utf8( apr_wchar_t const* in,
                                                 apr_size_t* inwords,
                                                 char* out,
                                                 apr_size_t* outbytes );

APR_DECLARE(apr_status_t) apr_conv_utf8_to_ucs2( char const* in,
                                                 apr_size_t* inbytes,
                                                 apr_wchar_t* out,
                                                 apr_size_t* outwords );

#define NUM( arr ) (sizeof( (arr) )/sizeof( *(arr) ))

static int ape_extra_ucs2_to_utf8( lua_State* L ) {
  size_t tlen = 0;
  luaL_checktype( L, 1, LUA_TTABLE );
  tlen = moon_compat_rawlen( L, 1 );
  if( tlen > 0 ) {
    size_t done = 0;
    luaL_Buffer outbuf;
    apr_wchar_t inbuf[ 256 ];
    luaL_buffinit( L, &outbuf );
    while( done < tlen ) {
      apr_status_t rv;
      apr_size_t i = 0, j = LUAL_BUFFERSIZE, k = 0;
      char* o = luaL_prepbuffer( &outbuf );
      for( ; i < NUM( inbuf ) && i+done < tlen; ++i ) {
        lua_rawgeti( L, 1, i+done+1 );
        inbuf[ i ] = (wchar_t)towupper( (wchar_t)luaL_checkint( L, -1 ) );
        lua_pop( L, 1 );
      }
      k = i;
      rv = apr_conv_ucs2_to_utf8( inbuf, &i, o, &j );
      if( rv == APR_EINVAL || (rv != APR_SUCCESS && j == LUAL_BUFFERSIZE) )
        return ape_status( L, 0, rv );
      luaL_addsize( &outbuf, LUAL_BUFFERSIZE-j );
      done += k-i;
    }
    luaL_pushresult( &outbuf );
  } else
    lua_pushliteral( L, "" );
  return 1;
}


static int ape_extra_wupper( lua_State* L ) {
  size_t len = 0;
  char const* s = luaL_checklstring( L, 1, &len );
  if( len > 0 ) {
    size_t done = 0;
    luaL_Buffer outbuf;
    apr_wchar_t wcbuf[ 256 ];
    luaL_buffinit( L, &outbuf );
    while( done < len ) {
      apr_status_t rv;
      apr_size_t i = len-done;
      apr_size_t j = NUM( wcbuf );
      apr_size_t k = 0;

      /* fill wcbuf */
      rv = apr_conv_utf8_to_ucs2( s+done, &i, wcbuf, &j );
      if( rv == APR_EINVAL || (rv != APR_SUCCESS && j == NUM( wcbuf )) )
        return ape_status( L, 0, rv );
      done += len-done-i; /* number of bytes consumed */
      j = NUM( wcbuf )-j; /* number of wchars in wcbuf */

      /* run towupper on all elements in wcbuf */
      for( k = 0; k < j; ++k )
        wcbuf[ k ] = (apr_wchar_t)towupper( (wint_t)wcbuf[ k ] );

      /* convert back to utf8 */
      k = 0;
      while( k < j ) {
        apr_size_t m = LUAL_BUFFERSIZE;
        apr_size_t n = j-k;
        char* o = luaL_prepbuffer( &outbuf );
        rv = apr_conv_ucs2_to_utf8( wcbuf+k, &n, o, &m );
        if( rv == APR_EINVAL || (rv != APR_SUCCESS && m == LUAL_BUFFERSIZE) )
          return ape_status( L, 0, rv );
        m = LUAL_BUFFERSIZE-m; /* number of bytes in outbuf */
        luaL_addsize( &outbuf, m );
        k += j-k-n;
      }
    }
    luaL_pushresult( &outbuf );
  } else
    lua_pushliteral( L, "" );
  return 1;
}
#endif


/***
Extra functions.
@section extras
*/
static luaL_Reg const ape_extra_functions[] = {
/***
Checks if a file exists and is readable under any of the given names.
@function find_file
@tparam string,... ... names to check
@treturn string the name of the file found
@treturn nil if the file was not found under any of the given names
@treturn nil,string nil and an error message in case of an error
*/
  { "find_file", ape_extra_find_file },
/***
Searches for executable files (either directly or in PATH).
@function find_exec
@tparam string,... ... names/paths to check
@treturn string,string the name/path and path of the file
@treturn nil if the file was not found under any of the given names
@treturn nil,string nil and an error message in case of an error
*/
  { "find_exec", ape_extra_find_exec },
/***
Executes a program and collects its standard output
@function run_collect
@tparam string type specifies how to execute the program
@tparam string prog the program name
@tparam table argv an array of program arguments
@tparam[opt] table env an env table
@tparam[opt] apr_pool_t pool a memory pool for temporary allocations
@treturn boolean,string,number,string same as @{os.execute} in Lua 5.2 plus the output
@treturn nil,string,nil,string nil and an error message in case of an error (plus partial output if any)
*/
  { "run_collect", ape_extra_run_collect },
/***
Strips the directory part (and file extension) of a path.
@function basename
@tparam string path a file path
@tparam[opt] string,... ... file extensions to strip
@treturn string the filename without directory (and file extension)
*/
  { "basename", ape_extra_basename },
/***
Strips the file name part of a path.
@function dirname
@tparam string path a file path
@treturn string the directory part of the path without the file name
@treturn nil,string nil and an error message in case of an error
*/
  { "dirname", ape_extra_dirname },
/***
Returns a table of matching paths.
@function path_glob
@tparam[opt] table t append path names to this table if given
@tparam string pattern the pattern to search
@tparam[opt] apr_pool_t pool memory pool for temporary allocations
@treturn table an array of path names
@treturn nil,string nil and an error message in case of an error
*/
  { "path_glob", ape_extra_path_glob },
/***
Hashes a whole file using memory mapping if available.
@function hash_file
@tparam apr_crypto_hash_t hash a hash object
@tparam string name the file name
@tparam[opt] apr_pool_t pool a memory pool for temporary allocations
@treturn boolean a true value in case of success
@treturn nil,string nil and an error message in case of an error
*/
  { "hash_file", ape_extra_hash_file },
  { NULL, NULL }
};


APE_API void ape_extra_setup( lua_State* L ) {
  moon_compat_register( L, ape_extra_functions );
  lua_newtable( L );
#ifdef O_RDONLY
  lua_pushnumber( L, O_RDONLY );
  lua_setfield( L, -2, "O_RDONLY" );
#endif
#ifdef O_WRONLY
  lua_pushnumber( L, O_WRONLY );
  lua_setfield( L, -2, "O_WRONLY" );
#endif
#ifdef O_RDWR
  lua_pushnumber( L, O_RDWR );
  lua_setfield( L, -2, "O_RDWR" );
#endif
#ifdef O_ACCMODE
  lua_pushnumber( L, O_ACCMODE );
  lua_setfield( L, -2, "O_ACCMODE" );
#endif
#if (defined( _WIN32 ) || defined( _WIN64 ) || defined( __WINDOWS__ )) && \
     defined( APR_HAS_UNICODE_FS ) && APR_HAS_UNICODE_FS
  lua_pushcfunction( L, ape_extra_ucs2_to_utf8 );
  lua_setfield( L, -2, "ucs2_to_utf8" );
  lua_pushcfunction( L, ape_extra_wupper );
  lua_setfield( L, -2, "wupper" );
#endif
  lua_setfield( L, -2, "hacks" );
}

