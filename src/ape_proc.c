/***
  @module ape
*/
#include <stddef.h>
#include <lua.h>
#include <lauxlib.h>
#include <apr_thread_proc.h>
#include <apr_strings.h>
#include <apr_tables.h>
#include "moon.h"
#include "ape.h"

/***
  Process handling.
  @section procs
*/
/***
  Userdata type for process attributes.
  @type apr_procattr_t
*/
/***
  Userdata type for processes.
  @type apr_proc_t
*/

static int ape_tokenize_to_argv( lua_State* L ) {
  char const* s = luaL_checkstring( L, 1 );
  apr_pool_t** pool = ape_opt_pool( L, 2 );
  char** argv = NULL;
  apr_status_t rv = apr_tokenize_to_argv( s, &argv, *pool );
  if( rv == APR_SUCCESS ) {
    size_t i = 0;
    lua_newtable( L );
    while( argv[ i ] != NULL ) {
      lua_pushstring( L, argv[ i ] );
      lua_rawseti( L, -2, i+1 );
      ++i;
    }
  }
  return ape_status( L, 1, rv );
}


static int ape_procattr_io_set( lua_State* L ) {
  static char const* const names[] = {
    "file", "closed", "child", "parent",
    "full", "none", NULL
  };
  static apr_int32_t const flags[] = {
    APR_NO_PIPE, APR_NO_FILE, APR_CHILD_BLOCK, APR_PARENT_BLOCK,
    APR_FULL_BLOCK, APR_FULL_NONBLOCK
  };
  apr_procattr_t** pa = moon_checkudata( L, 1, APE_PROCATTR_NAME );
  apr_int32_t in = flags[ luaL_checkoption( L, 2, NULL, names ) ];
  apr_int32_t out = flags[ luaL_checkoption( L, 3, NULL, names ) ];
  apr_int32_t err = flags[ luaL_checkoption( L, 4, NULL, names ) ];
  return ape_status( L, -1, apr_procattr_io_set( *pa, in, out, err ) );
}


static int ape_procattr_child_in_set( lua_State* L ) {
  apr_procattr_t** pa = moon_checkudata( L, 1, APE_PROCATTR_NAME );
  apr_file_t** c_in = moon_checkudata( L, 2, APE_FILE_NAME );
  apr_file_t** p_in = moon_checkudata( L, 3, APE_FILE_NAME );
  apr_status_t rv = APR_EBADF;
  if( *c_in != NULL && *p_in != NULL )
    rv = apr_procattr_child_in_set( *pa, *c_in, *p_in );
  return ape_status( L, -1, rv );
}


static int ape_procattr_child_out_set( lua_State* L ) {
  apr_procattr_t** pa = moon_checkudata( L, 1, APE_PROCATTR_NAME );
  apr_file_t** c_out = moon_checkudata( L, 2, APE_FILE_NAME );
  apr_file_t** p_out = moon_checkudata( L, 3, APE_FILE_NAME );
  apr_status_t rv = APR_EBADF;
  if( *c_out != NULL && *p_out != NULL )
    rv = apr_procattr_child_out_set( *pa, *c_out, *p_out );
  return ape_status( L, -1, rv );
}


static int ape_procattr_child_err_set( lua_State* L ) {
  apr_procattr_t** pa = moon_checkudata( L, 1, APE_PROCATTR_NAME );
  apr_file_t** c_err = moon_checkudata( L, 2, APE_FILE_NAME );
  apr_file_t** p_err = moon_checkudata( L, 3, APE_FILE_NAME );
  apr_status_t rv = APR_EBADF;
  if( *c_err != NULL && *p_err != NULL )
    rv = apr_procattr_child_err_set( *pa, *c_err, *p_err );
  return ape_status( L, -1, rv );
}


static int ape_procattr_dir_set( lua_State* L ) {
  apr_procattr_t** pa = moon_checkudata( L, 1, APE_PROCATTR_NAME );
  char const* dir = luaL_checkstring( L, 2 );
  return ape_status( L, -1, apr_procattr_dir_set( *pa, dir ) );
}


static int ape_procattr_cmdtype_set( lua_State* L ) {
  static char const* const names[] = {
    "shellcmd", "shellcmd/env", "program", "program/env",
    "program/path", NULL
  };
  static apr_cmdtype_e const flags[] = {
    APR_SHELLCMD, APR_SHELLCMD_ENV, APR_PROGRAM, APR_PROGRAM_ENV,
    APR_PROGRAM_PATH
  };
  apr_procattr_t** pa = moon_checkudata( L, 1, APE_PROCATTR_NAME );
  apr_cmdtype_e ct = flags[ luaL_checkoption( L, 2, NULL, names ) ];
  return ape_status( L, -1, apr_procattr_cmdtype_set( *pa, ct ) );
}


static int ape_procattr_detach_set( lua_State* L ) {
  apr_procattr_t** pa = moon_checkudata( L, 1, APE_PROCATTR_NAME );
  apr_int32_t detach = lua_toboolean( L, 2 );
  return ape_status( L, -1, apr_procattr_detach_set( *pa, detach ) );
}


static int ape_procattr_error_check_set( lua_State* L ) {
  apr_procattr_t** pa = moon_checkudata( L, 1, APE_PROCATTR_NAME );
  apr_int32_t flag = lua_toboolean( L, 2 );
  return ape_status( L, -1, apr_procattr_error_check_set( *pa, flag ) );
}


static int ape_procattr_addrspace_set( lua_State* L ) {
  apr_procattr_t** pa = moon_checkudata( L, 1, APE_PROCATTR_NAME );
  apr_int32_t as = lua_toboolean( L, 2 );
  return ape_status( L, -1, apr_procattr_addrspace_set( *pa, as ) );
}


static int ape_procattr_user_set( lua_State* L ) {
  apr_procattr_t** pa = moon_checkudata( L, 1, APE_PROCATTR_NAME );
  char const* user = luaL_checkstring( L, 2 );
#if APR_PROCATTR_USER_SET_REQUIRES_PASSWORD
  char const* pw = luaL_checkstring( L, 3 );
#else
  char const* pw = luaL_optstring( L, 3, NULL );
#endif
  return ape_status( L, -1, apr_procattr_user_set( *pa, user, pw ) );
}


static int ape_procattr_group_set( lua_State* L ) {
  apr_procattr_t** pa = moon_checkudata( L, 1, APE_PROCATTR_NAME );
  char const* group = luaL_checkstring( L, 2 );
  return ape_status( L, -1, apr_procattr_group_set( *pa, group ) );
}


static int ape_proc_in_get( lua_State* L ) {
  moon_checkudata( L, 1, APE_PROC_NAME );
  lua_settop( L, 1 );
  moon_getuvfield( L, 1, "in" );
  return 1;
}


static int ape_proc_out_get( lua_State* L ) {
  moon_checkudata( L, 1, APE_PROC_NAME );
  lua_settop( L, 1 );
  moon_getuvfield( L, 1, "out" );
  return 1;
}


static int ape_proc_err_get( lua_State* L ) {
  moon_checkudata( L, 1, APE_PROC_NAME );
  lua_settop( L, 1 );
  moon_getuvfield( L, 1, "err" );
  return 1;
}


static char const* const wait_names[] = {
  "wait", "nowait", NULL
};
static apr_wait_how_e const wait_flags[] = {
  APR_WAIT, APR_NOWAIT
};

static int ape_proc_wait( lua_State* L ) {
  apr_proc_t* proc = moon_checkudata( L, 1, APE_PROC_NAME );
  apr_wait_how_e f = wait_flags[ luaL_checkoption( L, 2, "wait", wait_names ) ];
  int exitcode = 0;
  apr_exit_why_e why = 0;
  apr_status_t rv = apr_proc_wait( proc, &exitcode, &why, f );
  if( rv == APR_CHILD_DONE ) {
    if( APR_PROC_CHECK_EXIT( why ) ) {
      lua_pushboolean( L, exitcode == 0 );
      lua_pushliteral( L, "exit" );
    } else { /* SIGNAL */
      lua_pushboolean( L, 0 );
      lua_pushliteral( L, "signal" );
    }
    lua_pushnumber( L, exitcode );
    return 3;
  } else if( rv == APR_CHILD_NOTDONE ) {
    lua_pushnil( L );
    return 1;
  }
  return ape_status( L, 0, rv );
}


#if 1
static int ape_proc_wait_all_procs( lua_State* L ) {
  apr_wait_how_e f = APR_WAIT;
  apr_pool_t** pool = NULL;
  apr_status_t rv = APR_SUCCESS;
  luaL_checktype( L, 1, LUA_TTABLE );
  f = wait_flags[ luaL_checkoption( L, 2, "wait", wait_names ) ];
  pool = ape_opt_pool( L, 3 );
  while( 1 ) {
    apr_proc_t proc;
    int exitcode = 0;
    apr_exit_why_e why = 0;
    rv = apr_proc_wait_all_procs( &proc, &exitcode, &why, f, *pool );
    if( rv == APR_CHILD_DONE ) {
      size_t n = moon_rawlen( L, 1 );
      size_t i = 1;
      for( i = 1; i <= n; ++i ) {
        apr_proc_t* p = NULL;
        lua_rawgeti( L, 1, i );
        p = moon_testudata( L, -1, APE_PROC_NAME );
        if( p != NULL && p->pid == proc.pid ) {
          if( APR_PROC_CHECK_EXIT( why ) ) {
            lua_pushboolean( L, exitcode == 0 );
            lua_pushliteral( L, "exit" );
          } else { /* SIGNAL */
            lua_pushboolean( L, 0 );
            lua_pushliteral( L, "signal" );
          }
          lua_pushnumber( L, exitcode );
          return 4;
        }
        lua_pop( L, 1 );
      }
    } else if( rv == APR_CHILD_NOTDONE ) {
      lua_pushnil( L );
      return 1;
    } else
      break;
  }
  return ape_status( L, 0, rv );
}
#else
static int ape_proc_wait_all_procs( lua_State* L ) {
  apr_wait_how_e f = wait_flags[ luaL_checkoption( L, 1, "wait", wait_names ) ];
  apr_pool_t** pool = ape_opt_pool( L, 2 );
  apr_proc_t proc = { 0 };
  int exitcode = 0;
  apr_exit_why_e why = 0;
  apr_status_t rv = apr_proc_wait_all_procs( &proc, &exitcode, &why, f, *pool );
  if( rv == APR_CHILD_DONE ) {
    if( APR_PROC_CHECK_EXIT( why ) ) {
      lua_pushboolean( L, exitcode == 0 );
      lua_pushliteral( L, "exit" );
    } else { /* SIGNAL */
      lua_pushboolean( L, 0 );
      lua_pushliteral( L, "signal" );
    }
    lua_pushnumber( L, exitcode );
    return 3;
  } else if( rv == APR_CHILD_NOTDONE ) {
    lua_pushnil( L );
    return 1;
  }
  return ape_status( L, 0, rv );
}
#endif


static int ape_proc_detach( lua_State* L ) {
  int daemonize = lua_toboolean( L, 1 );
  return ape_status( L, -1, apr_proc_detach( daemonize ) );
}


static int ape_proc_kill( lua_State* L ) {
  apr_proc_t* proc = moon_checkudata( L, 1, APE_PROC_NAME );
  int sig = luaL_checkinteger( L, 2 );
  return ape_status( L, -1, apr_proc_kill( proc, sig ) );
}


static int ape_pool_note_subprocess( lua_State* L ) {
  static char const* const names[] = {
    "never", "always", "timeout", "wait", "once", NULL
  };
  apr_kill_conditions_e const flags[] = {
    APR_KILL_NEVER, APR_KILL_ALWAYS, APR_KILL_AFTER_TIMEOUT,
    APR_JUST_WAIT, APR_KILL_ONLY_ONCE
  };
  apr_pool_t** pool = moon_checkudata( L, 1, APE_POOL_NAME );
  apr_proc_t* proc = moon_checkudata( L, 2, APE_PROC_NAME );
  apr_kill_conditions_e f = flags[ luaL_checkoption( L, 3, NULL, names ) ];
  apr_pool_note_subprocess( *pool, proc, f );
  lua_pushboolean( L, 1 );
  return 1;
}


static int ape_procattr_create( lua_State* L ) {
  apr_pool_t** pool = moon_checkudata( L, 1, APE_POOL_NAME );
  apr_procattr_t** pa = moon_newobject_ref( L, APE_PROCATTR_NAME, 1 );
  lua_pushvalue( L, 1 );
  moon_setuvfield( L, -2, "pool" );
  return ape_status( L, 1, apr_procattr_create( pa, *pool ) );
}


static int ape_proc_create( lua_State* L ) {
  char const* name = luaL_checkstring( L, 1 );
  apr_status_t rv = APR_ENOMEM;
  char const** argv = NULL;
  char const** env = NULL;
  apr_procattr_t** pa = NULL;
  apr_pool_t** pool = NULL;
  luaL_checktype( L, 2, LUA_TTABLE );
  if( !lua_isnoneornil( L, 3 ) )
    luaL_checktype( L, 3, LUA_TTABLE );
  pa = moon_checkudata( L, 4, APE_PROCATTR_NAME );
  lua_settop( L, 4 );
  moon_getuvfield( L, 4, "pool" );
  pool = lua_touserdata( L, 5 );
  if( ape_table2argv( L, 2, &argv, *pool ) == APR_SUCCESS &&
      (lua_isnoneornil( L, 3 ) ||
       ape_table2env( L, 3, &env, *pool ) == APR_SUCCESS) ) {
    apr_proc_t* proc = moon_newobject_ref( L, APE_PROC_NAME, 5 );
    rv = apr_proc_create( proc, name, argv, env, *pa, *pool );
    if( rv == APR_SUCCESS ) {
      if( proc->in != NULL ) {
        ape_file_userdata( L, proc->in, 5 );
        moon_setuvfield( L, 6, "in" );
      }
      if( proc->out != NULL ) {
        ape_file_userdata( L, proc->out, 5 );
        moon_setuvfield( L, 6, "out" );
      }
      if( proc->err != NULL ) {
        ape_file_userdata( L, proc->err, 5 );
        moon_setuvfield( L, 6, "err" );
      }
    }
  }
  return ape_status( L, 1, rv );
}



APE_API void ape_proc_setup( lua_State* L ) {
  /***
    Userdata type for process attributes.
    @type apr_procattr_t
  */
  luaL_Reg const ape_procattr_methods[] = {
  /***
    @function io_set
  */
    { "io_set", ape_procattr_io_set },
  /***
    @function child_in_set
  */
    { "child_in_set", ape_procattr_child_in_set },
  /***
    @function child_out_set
  */
    { "child_out_set", ape_procattr_child_out_set },
  /***
    @function child_err_set
  */
    { "child_err_set", ape_procattr_child_err_set },
  /***
    @function dir_set
  */
    { "dir_set", ape_procattr_dir_set },
  /***
    @function cmdtype_set
  */
    { "cmdtype_set", ape_procattr_cmdtype_set },
  /***
    @function detach_set
  */
    { "detach_set", ape_procattr_detach_set },
  /***
    @function error_check_set
  */
    { "error_check_set", ape_procattr_error_check_set },
  /***
    @function addrspace_set
  */
    { "addrspace_set", ape_procattr_addrspace_set },
  /***
    @function user_set
  */
    { "user_set", ape_procattr_user_set },
  /***
    @function group_set
  */
    { "group_set", ape_procattr_group_set },
    { NULL, NULL }
  };
  moon_object_type const ape_procattr_type = {
    APE_PROCATTR_NAME,
    sizeof( apr_procattr_t* ),
    0, /* NULL (function) pointer */
    NULL,
    ape_procattr_methods
  };
  /***
    Userdata type for processes.
    @type apr_proc_t
  */
  luaL_Reg const ape_proc_methods[] = {
  /***
    @function inp
  */
    { "inp", ape_proc_in_get },
  /***
    @function out
  */
    { "out", ape_proc_out_get },
  /***
    @function err
  */
    { "err", ape_proc_err_get },
  /***
    @function wait
  */
    { "wait", ape_proc_wait },
  /***
    @function detach
  */
    { "detach", ape_proc_detach },
  /***
    @function kill
  */
    { "kill", ape_proc_kill },
    { NULL, NULL }
  };
  moon_object_type const ape_proc_type = {
    APE_PROC_NAME,
    sizeof( apr_proc_t ),
    0, /* NULL (function) pointer */
    NULL,
    ape_proc_methods
  };
  /***
    Process handling.
    @section procs
  */
  luaL_Reg const ape_proc_functions[] = {
  /***
    @function procattr_create
  */
    { "procattr_create", ape_procattr_create },
  /***
    @function procattr_io_set
  */
    { "procattr_io_set", ape_procattr_io_set },
  /***
    @function procattr_child_in_set
  */
    { "procattr_child_in_set", ape_procattr_child_in_set },
  /***
    @function procattr_child_out_set
  */
    { "procattr_child_out_set", ape_procattr_child_out_set },
  /***
    @function procattr_child_err_set
  */
    { "procattr_child_err_set", ape_procattr_child_err_set },
  /***
    @function procattr_dir_set
  */
    { "procattr_dir_set", ape_procattr_dir_set },
  /***
    @function procattr_cmdtype_set
  */
    { "procattr_cmdtype_set", ape_procattr_cmdtype_set },
  /***
    @function procattr_detach_set
  */
    { "procattr_detach_set", ape_procattr_detach_set },
  /***
    @function procattr_error_check_set
  */
    { "procattr_error_check_set", ape_procattr_error_check_set },
  /***
    @function procattr_addrspace_set
  */
    { "procattr_addrspace_set", ape_procattr_addrspace_set },
  /***
    @function procattr_user_set
  */
    { "procattr_user_set", ape_procattr_user_set },
  /***
    @function procattr_group_set
  */
    { "procattr_group_set", ape_procattr_group_set },
  /***
    @function proc_create
  */
    { "proc_create", ape_proc_create },
  /***
    @function proc_wait
  */
    { "proc_wait", ape_proc_wait },
  /***
    @function proc_wait_all_procs
  */
    { "proc_wait_all_procs", ape_proc_wait_all_procs },
  /***
    @function proc_detach
  */
    { "proc_detach", ape_proc_detach },
  /***
    @function proc_kill
  */
    { "proc_kill", ape_proc_kill },
  /***
    @function pool_note_subprocess
  */
    { "pool_note_subprocess", ape_pool_note_subprocess },
  /***
    @function tokenize_to_argv
  */
    { "tokenize_to_argv", ape_tokenize_to_argv },
    { NULL, NULL }
  };
  moon_defobject( L, &ape_procattr_type, 0 );
  moon_defobject( L, &ape_proc_type, 0 );
  moon_register( L, ape_proc_functions );
}


APE_API apr_status_t ape_table2argv( lua_State* L, int index,
                                     char const*** argv,
                                     apr_pool_t* pool ) {
  apr_array_header_t* array = NULL;
  size_t len = moon_rawlen( L, index );
  int top = lua_gettop( L );
  lua_pushvalue( L, index );
  index = top+1;
  *argv = NULL;
  array = apr_array_make( pool, len+1, sizeof( char const* ) );
  if( array != NULL ) {
    size_t i = 0;
    for( i = 0; i < len; ++i ) {
      lua_rawgeti( L, index, i+1 );
      if( lua_isstring( L, -1 ) )
        *(char const**)apr_array_push( array ) = lua_tostring( L, -1 );
      lua_pop( L, 1 );
    }
    *(char const**)apr_array_push( array ) = NULL;
    *argv = (char const**)array->elts;
    lua_settop( L, top );
    return APR_SUCCESS;
  }
  lua_settop( L, top );
  return APR_ENOMEM;
}


APE_API apr_status_t ape_table2env( lua_State* L, int index,
                                    char const*** env,
                                    apr_pool_t* pool ) {
  apr_array_header_t* array = NULL;
  int top = lua_gettop( L );
  lua_pushvalue( L, index );
  index = top+1;
  *env = NULL;
  array = apr_array_make( pool, 16, sizeof( char const* ) );
  if( array != NULL ) {
    void* p = NULL;
    lua_pushnil( L );
    while( lua_next( L, index ) != 0 ) {
      if( lua_isstring( L, -2 ) && lua_isstring( L, -1 ) ) {
        char const* s1 = lua_tostring( L, -2 );
        char const* s2 = lua_tostring( L, -1 );
        p = apr_array_push( array );
        if( p == NULL ) {
          lua_settop( L, top );
          return APR_ENOMEM;
        }
        *(char**)p = apr_pstrcat( pool, s1, "=", s2, NULL );
        if( *(char**)p == NULL ) {
          lua_settop( L, top );
          return APR_ENOMEM;
        }
      }
      lua_pop( L, 1 );
    }
    p = apr_array_push( array );
    if( p != NULL ) {
      *(char**)p = NULL;
      *env = (char const**)array->elts;
      lua_settop( L, top );
      return APR_SUCCESS;
    }
  }
  lua_settop( L, top );
  return APR_ENOMEM;
}

