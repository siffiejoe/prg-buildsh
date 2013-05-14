/***
@module ape
*/
#include <stddef.h>
#include <stdio.h>
#include "lua.h"
#include "lauxlib.h"
#include "moon.h"
#include "ape.h"
#include "apr_time.h"

/***
Time/date handling.
@section time
*/
/***
Userdata for APR time values.
@type apr_time_t
*/

static int ape_time_eq( lua_State* L ) {
  apr_time_t* a = moon_checkudata( L, 1, APE_TIME_NAME );
  apr_time_t* b = moon_checkudata( L, 2, APE_TIME_NAME );
  lua_pushboolean( L, *a == *b );
  return 1;
}

static int ape_time_lt( lua_State* L ) {
  apr_time_t* a = moon_checkudata( L, 1, APE_TIME_NAME );
  apr_time_t* b = moon_checkudata( L, 2, APE_TIME_NAME );
  lua_pushboolean( L, *a < *b );
  return 1;
}

static int ape_time_add( lua_State* L );

static int ape_time_sub( lua_State* L );

static int ape_time_tostring( lua_State* L ) {
  apr_time_t* t = moon_checkudata( L, 1, APE_TIME_NAME );
  char buf[ APR_CTIME_LEN ] = { 0 };
  apr_status_t rv = apr_ctime( buf, *t );
  if( rv == APR_SUCCESS )
    lua_pushstring( L, buf );
  else {
    char buf2[ sizeof( APE_TIME_NAME ) + 26 ] = { 0 };
    sprintf( buf2, "[" APE_TIME_NAME "]: %" APR_TIME_T_FMT, *t );
    lua_pushstring( L, buf2 );
  }
  return 1;
}

static int ape_rfc822_date( lua_State* L ) {
  apr_time_t* t = moon_checkudata( L, 1, APE_TIME_NAME );
  char buf[ APR_RFC822_DATE_LEN ] = { 0 };
  apr_status_t rv = apr_rfc822_date( buf, *t );
  if( rv == APR_SUCCESS )
    lua_pushstring( L, buf );
  return ape_status( L, 1, rv );
}

static int ape_ctime( lua_State* L ) {
  apr_time_t* t = moon_checkudata( L, 1, APE_TIME_NAME );
  char buf[ APR_CTIME_LEN ] = { 0 };
  apr_status_t rv = apr_ctime( buf, *t );
  if( rv == APR_SUCCESS )
    lua_pushstring( L, buf );
  return ape_status( L, 1, rv );
}

static int ape_sleep( lua_State* L ) {
  apr_interval_time_t t = (apr_interval_time_t)luaL_checknumber( L, 1 );
  apr_sleep( t );
  lua_pushboolean( L, 1 );
  return 1;
}


static luaL_Reg const ape_time_metamethods[] = {
  { "__eq", ape_time_eq },
  { "__lt", ape_time_lt },
  { "__add", ape_time_add },
  { "__sub", ape_time_sub },
  { "__tostring", ape_time_tostring },
  { NULL, NULL }
};

/***
Userdata for APR time values.
@type apr_time_t
*/
static luaL_Reg const ape_time_methods[] = {
/***
Formats the apr_time_t value according to RFC 822.
@function rfc822_date
@treturn string the formatted date string
@treturn nil,string nil and an error message in case of an error
*/
  { "rfc822_date", ape_rfc822_date },
/***
Formats the apr_time_t value like the ctime function.
@function ctime
@treturn string the formatted date string
@treturn nil,string nil and an error message in case of an error
*/
  { "ctime", ape_ctime },
  { NULL, NULL }
};

static ape_object_type const ape_time_type = {
  APE_TIME_NAME,
  sizeof( apr_time_t ),
  0, /* NULL (function) pointer */
  1,
  ape_time_metamethods,
  ape_time_methods,
  0 /* NULL (function) pointer */
};


static int ape_time_make( lua_State* L ) {
  lua_Number sec = luaL_checknumber( L, 1 );
  lua_Number usec = luaL_checknumber( L, 2 );
  ape_time_userdata( L, apr_time_make( sec, usec ) );
  return 1;
}

static int ape_time_now( lua_State* L ) {
  ape_time_userdata( L, apr_time_now() );
  return 1;
}

static int ape_time_add( lua_State* L ) {
  apr_time_t a, b;
  if( lua_type( L, 1 ) == LUA_TNUMBER ) {
    a = (apr_time_t)lua_tonumber( L, 1 );
    b = *(apr_time_t*)moon_checkudata( L, 2, APE_TIME_NAME );
  } else {
    a = *(apr_time_t*)moon_checkudata( L, 1, APE_TIME_NAME );
    b = (apr_time_t)luaL_checknumber( L, 2 );
  }
  ape_time_userdata( L, a + b );
  return 1;
}

static int ape_time_sub( lua_State* L ) {
  apr_time_t* a = moon_checkudata( L, 1, APE_TIME_NAME );
  if( lua_type( L, 2 ) == LUA_TNUMBER ) {
    apr_time_t b = (apr_time_t)lua_tonumber( L, 2 );
    ape_time_userdata( L, *a - b );

  } else {
    apr_time_t* b = moon_checkudata( L, 2, APE_TIME_NAME );
    lua_pushnumber( L, (lua_Number)(*a - *b) );
  }
  return 1;
}


/***
Time/date handling.
@section time
*/
static luaL_Reg const ape_time_functions[] = {
/***
Returns the current date/time as an apr_time_t value which
represents the number of microseconds since epoch (1970/1/1).
@function time_now
@treturn apr_time_t the apr_time_t value representing now
*/
  { "time_now", ape_time_now },
/***
Creates an apr_time_t value.
@function time_make
@tparam number secs number of seconds since 1970/1/1 epoch
@tparam number usecs number of microseconds
@treturn apr_time_t the new apr_time_t value
*/
  { "time_make", ape_time_make },
/***
Formats an apr_time_t value according to RFC 822.
@function rfc822_date
@tparam apr_time_t date the date as an apr_time_t value
@treturn string the formatted date string
@treturn nil,string nil and an error message in case of an error
*/
  { "rfc822_date", ape_rfc822_date },
/***
Formats an apr_time_t value like the ctime function.
@function ctime
@tparam apr_time_t date the date as an apr_time_t value
@treturn string the formatted date string
@treturn nil,string nil and an error message in case of an error
*/
  { "ctime", ape_ctime },
/***
Sleeps the given amount of microseconds.
@function sleep
@tparam number usecs number of microseconds to sleep
@treturn boolean always returns true
*/
  { "sleep", ape_sleep },
  { NULL, NULL }
};


APE_API void ape_time_setup( lua_State* L ) {
  moon_compat_register( L, ape_time_functions );
}


APE_API void ape_time_userdata( lua_State* L, apr_time_t time ) {
  apr_time_t* newtime = ape_new_object( L, &ape_time_type, 0 );
  *newtime = time;
}

