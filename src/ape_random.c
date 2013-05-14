/***
@module ape
*/
#include <stddef.h>
#include "lua.h"
#include "lauxlib.h"
#include "moon.h"
#include "ape.h"
#include "apr_random.h"

/***
Crypto and random numbers.
@section random
*/
/***
Userdata type for crypto hashes.
@type apr_crypto_hash_t
*/

typedef struct {
  apr_crypto_hash_t* hash;
  size_t digest_len;
} crypto_hash;


static int ape_crypto_hash_reset( lua_State* L ) {
  crypto_hash* h = moon_checkudata( L, 1, APE_CRYPTOHASH_NAME );
  h->hash->init( h->hash );
  lua_settop( L, 1 );
  return 1;
}

static int ape_crypto_hash_update( lua_State* L ) {
  crypto_hash* h = moon_checkudata( L, 1, APE_CRYPTOHASH_NAME );
  int i = 0;
  int n = lua_gettop( L );
  for( i = 2; i <= n; ++i ) {
    size_t len = 0;
    char const* s = luaL_checklstring( L, i, &len );
    h->hash->add( h->hash, s, len );
  }
  lua_settop( L, 1 );
  return 1;
}

static int ape_crypto_hash_digest( lua_State* L ) {
  crypto_hash* h = moon_checkudata( L, 1, APE_CRYPTOHASH_NAME );
  int raw = lua_toboolean( L, 2 );
  apr_byte_t rbuffer[ 64 ] = { 0 };
  apr_byte_t* rres = rbuffer;
  if( h->digest_len > sizeof( rbuffer ) )
    rres = lua_newuserdata( L, h->digest_len * sizeof( apr_byte_t ) );
  h->hash->finish( h->hash, rres );
  if( raw )
    lua_pushlstring( L, (char const*)rres, h->digest_len );
  else {
    static char const hexdigits[] = "0123456789abcdef";
    char hbuffer[ 128 ] = { 0 };
    char* hres = hbuffer;
    size_t i = 0;
    if( h->digest_len*2 > sizeof( hbuffer ) )
      hres = lua_newuserdata( L, h->digest_len * 2 );
    for( i = 0; i < h->digest_len; ++i ) {
      hres[ 2*i ] = hexdigits[ (rres[ i ] >> 4) & 0x0F ];
      hres[ 2*i+1 ] = hexdigits[ rres[ i ] & 0x0F ];
    }
    lua_pushlstring( L, hres, h->digest_len*2 );
  }
  return 1;
}


/***
Userdata type for crypto hashes.
@type apr_crypto_hash_t
*/
static luaL_Reg const ape_cryptohash_methods[] = {
/***
Resets the internal state.
@function reset
@treturn apr_crypto_hash_t the object itself for method chaining
*/
  { "reset", ape_crypto_hash_reset },
/***
Adds strings to the hash.
@function update
@tparam string,... ... the strings to add to the internal hash state
@treturn apr_crypto_hash_t the object itself for method chaining
*/
  { "update", ape_crypto_hash_update },
/***
Returns a digest (either binary or in hex format).
@function digest
@tparam[opt] boolean binary use binary or hex format output
@treturn string the hash digest
*/
  { "digest", ape_crypto_hash_digest },
  { NULL, NULL }
};

static ape_object_type const ape_cryptohash_type = {
  APE_CRYPTOHASH_NAME,
  sizeof( crypto_hash ),
  0, /* NULL (function) pointer */
  1,
  NULL,
  ape_cryptohash_methods,
  0 /* NULL (function) pointer */
};


static int ape_crypto_sha256_new( lua_State* L ) {
  apr_pool_t** pool = moon_checkudata( L, 1, APE_POOL_NAME );
  crypto_hash* ch = ape_new_object( L, &ape_cryptohash_type, 1 );
  ch->hash = apr_crypto_sha256_new( *pool );
  ch->digest_len = 32u;
  if( ch->hash == NULL )
    ape_assert( L, APR_ENOMEM, "APR crypto hash" );
  ch->hash->init( ch->hash );
  return 1;
}


/* ape module functions */
/***
Crypto and random numbers.
@section random
*/
static luaL_Reg const ape_random_functions[] = {
/***
Creates a new hash object for the SHA256 algorithm.
@function sha256_new
@tparam apr_pool_t pool the pool to use for internal memory allocation
@treturn apr_crypto_hash_t a new hash object
*/
  { "sha256_new", ape_crypto_sha256_new },
  { NULL, NULL }
};


APE_API void ape_random_setup( lua_State* L ) {
  moon_compat_register( L, ape_random_functions );
}


APE_API apr_crypto_hash_t* ape_check_hash( lua_State* L, int index ) {
  crypto_hash* h = moon_checkudata( L, index, APE_CRYPTOHASH_NAME );
  return h->hash;
}

