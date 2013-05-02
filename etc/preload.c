/*
 * compile with:
 *   gcc -fpic -shared -Os -o preload.so preload.c -ldl
 */

#define _GNU_SOURCE

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <limits.h>

/* for dlsym */
#include <dlfcn.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>


/* open a log file for writing */
static int open_log_file( void );

/* file locking */
static int my_lock( int fd );
static int my_unlock( int fd );

/* write a complete buffer to the given filedescriptor */
static int write_all( int fd, void const* buf, size_t len );

/* write an integral value using the given format string */
#define write_int( fd, fmt, v ) \
  do { \
    char _buf[ (sizeof( (v) )*CHAR_BIT)/3+2 ] = { 0 }; \
    int _ret = sprintf( _buf, (fmt), (v) ); \
    if( _ret >= 0 ) \
      write_all( (fd), _buf, _ret ); \
  } while( 0 )

/* write a string using hex escapes */
static int write_name( int fd, char const* s );

/* write a literal string */
#define write_literal( fd, s ) \
  write_all( (fd), "" s, sizeof( (s) )-1 )


/* define an init function for a named function pointer */
#define def_init( f ) \
  static void init_##f( void ) { \
    if( !orig_##f ) { \
      orig_##f = dlsym( RTLD_NEXT, #f ); \
      if( !orig_##f ) \
        abort(); \
    } \
  }


/*
 * redefinition of some library functions for system calls
 */

/* function pointers to store original functions */
static FILE* (*orig_fopen)( char const*, char const* ) = 0;
static DIR* (*orig_opendir)( char const* ) = 0;
static int (*orig_open)( char const*, int, ... ) = 0;
static int (*orig_openat)( int, char const*, int, ... ) = 0;
static int (*orig_creat)( char const*, mode_t ) = 0;
static int (*orig_mkdir)( char const*, mode_t ) = 0;
static int (*orig_mkdirat)( int, char const*, mode_t ) = 0;
static int (*orig_chdir)( char const* ) = 0;
static int (*orig_fchdir)( int ) = 0;
static int (*orig_rename)( char const*, char const* ) = 0;
static int (*orig_renameat) ( int, char const*, int, char const* ) = 0;
static pid_t (*orig_fork)( void ) = 0;
static int (*orig_execve)( char const*, char* const*, char* const* ) = 0;
static int (*orig_execvpe)( char const*, char* const*, char* const* ) = 0;


/* TODO:
 *  clone?
 *  posix_spawn?
 *
 */

/* init functions for the orig function pointers above */
def_init( fopen )
def_init( opendir )
def_init( open )
def_init( openat )
def_init( creat )
def_init( mkdir )
def_init( mkdirat )
def_init( chdir )
def_init( fchdir )
def_init( rename )
def_init( renameat )
def_init( fork )
def_init( execve )
def_init( execvpe )


static FILE* my_fopen( char const* file, char const* mode ) {
  FILE* retval = NULL;
  int myerrno = 0;

  init_fopen();

  retval = orig_fopen( file, mode );
  myerrno = errno;

  /* do logging for successful calls only! */
  if( retval != NULL ) {
    int fd = open_log_file();
    if( fd >= 0 && my_lock( fd ) ) {
      write_int( fd, "%d", getpid() );
      write_literal( fd, " open " );
      write_name( fd, file );
      if( mode[ 0 ] == 'r' && !strchr( mode, '+' ) )
        write_literal( fd, " in " );
      else
        write_literal( fd, " out " );
      write_int( fd, "%d", fileno( retval ) );
      write_literal( fd, "\n" );
      my_unlock( fd );
    }
  }

  /* restore errno and return */
  errno = myerrno;
  return retval;
}

FILE* fopen( char const*, char const*) __attribute__((__alias__("my_fopen")));
FILE* fopen64( char const*, char const* ) __attribute__((__alias__("my_fopen")));


static DIR* my_opendir( char const* name ) {
  DIR* retval = NULL;
  int myerrno = 0;

  init_opendir();

  retval = orig_opendir( name );
  myerrno = errno;

  /* do logging for successful calls only! */
  if( retval != NULL ) {
    int fd = open_log_file();
    if( fd >= 0 && my_lock( fd ) ) {
      write_int( fd, "%d", getpid() );
      write_literal( fd, " open " );
      write_name( fd, name );
      write_literal( fd, " in " );
      write_int( fd, "%d", dirfd( retval ) );
      write_literal( fd, "\n" );
      my_unlock( fd );
    }
  }

  /* restore errno and return */
  errno = myerrno;
  return retval;
}

DIR* opendir( char const* name ) __attribute__((__alias__("my_opendir")));


static int my_open( char const* file, int oflags, ... ) {
  int retval = 0;
  int myerrno = 0;

  init_open();

  /* call original open function (2- or 3-arg version) */
  if( oflags & O_CREAT ) {
    mode_t mode;
    va_list ap;
    va_start( ap, oflags );
    mode = va_arg( ap, mode_t );
    va_end( ap );
    retval = orig_open( file, oflags, mode );
  } else
    retval = orig_open( file, oflags );
  myerrno = errno;

  /* do logging for successful calls only! */
  if( retval >= 0 ) {
    int fd = open_log_file();
    if( fd >= 0 && my_lock( fd ) ) {
      write_int( fd, "%d", getpid() );
      write_literal( fd, " open " );
      write_name( fd, file );
      if( (oflags & O_WRONLY) || (oflags & O_RDWR) )
        write_literal( fd, " out " );
      else
        write_literal( fd, " in " );
      write_int( fd, "%d", retval );
      write_literal( fd, "\n" );
      my_unlock( fd );
    }
  }

  /* restore errno and return */
  errno = myerrno;
  return retval;
}

int open( char const*, int, ... ) __attribute__((__alias__("my_open")));
int open64( char const*, int, ... ) __attribute__((__alias__("my_open")));
int __open_2( char const*, int ) __attribute__((__alias__("my_open")));
int __open64_2( char const*, int ) __attribute__((__alias__("my_open")));


static int my_openat( int dirfd, char const* file, int oflags, ... ) {
  int retval = 0;
  int myerrno = 0;

  init_openat();

  /* call original openat function (2- or 3-arg version) */
  if( oflags & O_CREAT ) {
    mode_t mode;
    va_list ap;
    va_start( ap, oflags );
    mode = va_arg( ap, mode_t );
    va_end( ap );
    retval = orig_openat( dirfd, file, oflags, mode );
  } else
    retval = orig_openat( dirfd, file, oflags );
  myerrno = errno;

  /* do logging for successful calls only! */
  if( retval >= 0 ) {
    int fd = open_log_file();
    if( fd >= 0 && my_lock( fd ) ) {
      write_int( fd, "%d", getpid() );
      write_literal( fd, " openat " );
      if( dirfd == AT_FDCWD )
        write_literal( fd, "AT_FDCWD" );
      else
        write_int( fd, "%d", dirfd );
      write_literal( fd, " " );
      write_name( fd, file );
      if( (oflags & O_WRONLY) || (oflags & O_RDWR) )
        write_literal( fd, " out " );
      else
        write_literal( fd, " in " );
      write_int( fd, "%d", retval );
      write_literal( fd, "\n" );
      my_unlock( fd );
    }
  }

  /* restore errno and return */
  errno = myerrno;
  return retval;
}

int openat( int, char const*, int, ... ) __attribute__((__alias__("my_openat")));
int openat64( int, char const*, int, ... ) __attribute__((__alias__("my_openat")));
int __openat_2( int, char const*, int ) __attribute__((__alias__("my_openat")));
int __openat64_2( int, char const*, int ) __attribute__((__alias__("my_openat")));


static int my_creat( char const* file, mode_t mode ) {
  int retval = 0;
  int myerrno = 0;

  init_creat();

  retval = orig_creat( file, mode );
  myerrno = errno;

  /* do logging for successful calls only! */
  if( retval >= 0 ) {
    int fd = open_log_file();
    if( fd >= 0 && my_lock( fd ) ) {
      write_int( fd, "%d", getpid() );
      write_literal( fd, " creat " );
      write_name( fd, file );
      write_all( fd, " ", 1 );
      write_int( fd, "%d", retval );
      write_literal( fd, "\n" );
      my_unlock( fd );
    }
  }

  /* restore errno and return */
  errno = myerrno;
  return retval;
}

int creat( char const*, mode_t ) __attribute__((__alias__("my_creat")));
int creat64( char const*, mode_t ) __attribute__((__alias__("my_creat")));


static int my_mkdir( char const* dir, mode_t mode ) {
  int retval = 0;
  int myerrno = 0;

  init_mkdir();

  retval = orig_mkdir( dir, mode );
  myerrno = errno;

  /* do logging for successful calls only! */
  if( retval >= 0 ) {
    int fd = open_log_file();
    if( fd >= 0 && my_lock( fd ) ) {
      write_int( fd, "%d", getpid() );
      write_literal( fd, " mkdir " );
      write_name( fd, dir );
      write_literal( fd, "\n" );
      my_unlock( fd );
    }
  }

  /* restore errno and return */
  errno = myerrno;
  return retval;
}

int mkdir( char const*, mode_t ) __attribute__((__alias__("my_mkdir")));


static int my_mkdirat( int dirfd, char const* dir, mode_t mode ) {
  int retval = 0;
  int myerrno = 0;

  init_mkdirat();

  retval = orig_mkdirat( dirfd, dir, mode );
  myerrno = errno;

  /* do logging for successful calls only! */
  if( retval >= 0 ) {
    int fd = open_log_file();
    if( fd >= 0 && my_lock( fd ) ) {
      write_int( fd, "%d", getpid() );
      write_literal( fd, " mkdirat " );
      if( dirfd == AT_FDCWD )
        write_literal( fd, "AT_FDCWD" );
      else
        write_int( fd, "%d", dirfd );
      write_literal( fd, " " );
      write_name( fd, dir );
      write_literal( fd, "\n" );
      my_unlock( fd );
    }
  }

  /* restore errno and return */
  errno = myerrno;
  return retval;
}

int mkdirat( int, char const*, mode_t ) __attribute__((__alias__("my_mkdirat")));


static int my_chdir( char const* dir ) {
  int retval = 0;
  int myerrno = 0;

  init_chdir();

  retval = orig_chdir( dir );
  myerrno = errno;

  /* do logging for successful calls only! */
  if( retval >= 0 ) {
    int fd = open_log_file();
    if( fd >= 0 && my_lock( fd ) ) {
      write_int( fd, "%d", getpid() );
      write_literal( fd, " chdir " );
      write_name( fd, dir );
      write_literal( fd, "\n" );
      my_unlock( fd );
    }
  }

  /* restore errno and return */
  errno = myerrno;
  return retval;
}

int chdir( char const* ) __attribute__((__alias__("my_chdir")));


static int my_fchdir( int dirfd ) {
  int retval = 0;
  int myerrno = 0;

  init_fchdir();

  retval = orig_fchdir( dirfd );
  myerrno = errno;

  /* do logging for successful calls only! */
  if( retval >= 0 ) {
    int fd = open_log_file();
    if( fd >= 0 && my_lock( fd ) ) {
      write_int( fd, "%d", getpid() );
      write_literal( fd, " fchdir " );
      write_int( fd, "%d", dirfd );
      write_literal( fd, "\n" );
      my_unlock( fd );
    }
  }

  /* restore errno and return */
  errno = myerrno;
  return retval;
}

int fchdir( int ) __attribute__((__alias__("my_fchdir")));


static int my_rename( char const* old, char const* newn ) {
  int retval = 0;
  int myerrno = 0;

  init_rename();

  retval = orig_rename( old, newn );
  myerrno = errno;

  /* do logging for successful calls only! */
  if( retval >= 0 ) {
    int fd = open_log_file();
    if( fd >= 0 && my_lock( fd ) ) {
      write_int( fd, "%d", getpid() );
      write_literal( fd, " rename " );
      write_name( fd, old );
      write_literal( fd, " " );
      write_name( fd, newn );
      write_literal( fd, "\n" );
      my_unlock( fd );
    }
  }

  /* restore errno and return */
  errno = myerrno;
  return retval;
}

int rename( char const*, char const* ) __attribute__((__alias__("my_rename")));


static int my_renameat( int ofd, char const* old, int nfd, char const* newn ) {
  int retval = 0;
  int myerrno = 0;

  init_renameat();

  retval = orig_renameat( ofd, old, nfd, newn );
  myerrno = errno;

  /* do logging for successful calls only! */
  if( retval >= 0 ) {
    int fd = open_log_file();
    if( fd >= 0 && my_lock( fd ) ) {
      write_int( fd, "%d", getpid() );
      write_literal( fd, " renameat " );
      if( ofd == AT_FDCWD )
        write_literal( fd, "AT_FDCWD" );
      else
        write_int( fd, "%d", ofd );
      write_literal( fd, " " );
      write_name( fd, old );
      write_literal( fd, " " );
      if( nfd == AT_FDCWD )
        write_literal( fd, "AT_FDCWD" );
      else
        write_int( fd, "%d", nfd );
      write_literal( fd, " " );
      write_name( fd, newn );
      write_literal( fd, "\n" );
      my_unlock( fd );
    }
  }

  /* restore errno and return */
  errno = myerrno;
  return retval;
}

int renameat( int, char const*, int, char const* ) __attribute__((__alias__("my_renameat")));


static void report_fork( pid_t pid );

static pid_t my_fork( void ) {
  pid_t retval = 0;

  init_fork();

  retval = orig_fork();

  /* do logging for successful calls in parent only! */
  if( retval > 0 )
    report_fork( retval );

  return retval;
}

pid_t fork( void ) __attribute__((__alias__("my_fork")));
pid_t vfork( void ) __attribute__((__alias__("my_fork")));


static void report_exec( char const* prog );
static char** clone_env( char* const* env, size_t*, size_t*, size_t*,
                         size_t*, size_t* );
static void free_env( char**, size_t, size_t, size_t, size_t, size_t );
static size_t count_arglist( va_list* ap );
static char** make_argv( size_t argc, char const* arg, va_list* ap );

static int my_execve( char const* prog, char* const* argv, char* const* env ) {
  int retval = -1;
  char** new_env = NULL;
  size_t ldp_pos, dil_pos, dffns_pos, lmtf_pos, lmpl_pos;
  init_execve();

  /* do logging before actual call this time */
  report_exec( prog );

  new_env = clone_env( env, &ldp_pos, &dil_pos, &dffns_pos, &lmtf_pos,
                       &lmpl_pos );
  if( new_env == NULL ) {
    errno = ENOMEM;
    goto error;
  }

  retval = orig_execve( prog, argv, new_env );

error:
  free_env( new_env, ldp_pos, dil_pos, dffns_pos, lmtf_pos, lmpl_pos );
  return retval;
}

int execve( char const*, char* const*, char* const* ) __attribute__((__alias__("my_execve")));


static int my_execvpe( char const* prog, char* const* argv, char* const* env ) {
  int retval = -1;
  char** new_env = NULL;
  size_t ldp_pos, dil_pos, dffns_pos, lmtf_pos, lmpl_pos;
  init_execvpe();

  /* do logging before actual call this time */
  report_exec( prog );

  new_env = clone_env( env, &ldp_pos, &dil_pos, &dffns_pos, &lmtf_pos,
                       &lmpl_pos );
  if( new_env == NULL ) {
    errno = ENOMEM;
    goto error;
  }

  retval = orig_execvpe( prog, argv, new_env );

error:
  free_env( new_env, ldp_pos, dil_pos, dffns_pos, lmtf_pos, lmpl_pos );
  return retval;
}

int execvpe( char const*, char* const*, char* const* ) __attribute__((__alias__("my_execvpe")));


static int my_execv( char const* prog, char* const* argv ) {
  init_execve();

  /* do logging before actual call this time */
  report_exec( prog );

  return orig_execve( prog, argv, environ );
}

int execv( char const*, char* const* ) __attribute__((__alias__("my_execv")));


static int my_execvp( char const* prog, char* const* argv ) {
  init_execvpe();

  /* do logging before actual call this time */
  report_exec( prog );

  return orig_execvpe( prog, argv, environ );
}

int execvp( char const*, char* const* ) __attribute__((__alias__("my_execvp")));


static int my_execl( char const* prog, char const* arg, ... ) {
  int retval = -1;
  size_t argc = 0;
  char** argv = NULL;
  va_list ap;

  init_execve();

  /* do logging before actual call this time */
  report_exec( prog );

  va_start( ap, arg );
  argc = count_arglist( &ap );
  va_end( ap );
  va_start( ap, arg );
  argv = make_argv( argc, arg, &ap );
  va_end( ap );
  if( argv == NULL ) {
    errno = ENOMEM;
    goto error;
  }

  retval = orig_execve( prog, argv, environ );

error:
  free( argv );
  return retval;
}

int execl( char const*, char const*, ... ) __attribute__((__alias__("my_execl")));


static int my_execlp( char const* prog, char const* arg, ... ) {
  int retval = -1;
  size_t argc = 0;
  char** argv = NULL;
  va_list ap;

  init_execvpe();

  /* do logging before actual call this time */
  report_exec( prog );

  va_start( ap, arg );
  argc = count_arglist( &ap );
  va_end( ap );
  va_start( ap, arg );
  argv = make_argv( argc, arg, &ap );
  va_end( ap );
  if( argv == NULL ) {
    errno = ENOMEM;
    goto error;
  }

  retval = orig_execvpe( prog, argv, environ );

error:
  free( argv );
  return retval;
}

int execlp( char const*, char const*, ... ) __attribute__((__alias__("my_execlp")));


static int my_execle( char const* prog, char const* arg, ... ) {
  int retval = -1;
  size_t argc = 0;
  char** argv = NULL;
  char** env = NULL;
  size_t ldp_pos, dil_pos, dffns_pos, lmtf_pos, lmpl_pos;
  va_list ap;

  init_execvpe();

  /* do logging before actual call this time */
  report_exec( prog );

  va_start( ap, arg );
  argc = count_arglist( &ap );
  env = clone_env( va_arg( ap, char* const* ), &ldp_pos, &dil_pos,
                   &dffns_pos, &lmtf_pos, &lmpl_pos );
  va_end( ap );
  if( env == NULL ) {
    errno = ENOMEM;
    goto error;
  }
  va_start( ap, arg );
  argv = make_argv( argc, arg, &ap );
  va_end( ap );
  if( argv == NULL ) {
    errno = ENOMEM;
    goto error;
  }

  retval = orig_execvpe( prog, argv, env );

error:
  free( argv );
  free_env( env, ldp_pos, dil_pos, dffns_pos, lmtf_pos, lmpl_pos );
  return retval;
}

int execle( char const*, char const*, ... ) __attribute__((__alias__("my_execle")));



/*
 * definition of the helper functions declared above
 */


#ifndef BUILDSH_TEMPFILE_DEFAULT
#  define BUILDSH_TEMPFILE_DEFAULT "/tmp/preload.log"
#endif

static int log_fd = -1;

/* helper functions */
static int open_log_file( void ) {
  if( log_fd < 0 ) {
    char const* fname = getenv( "BUILDSH_TEMPFILE" );
    init_open();
    if( fname == NULL )
      fname = BUILDSH_TEMPFILE_DEFAULT;
    log_fd =  orig_open( fname, O_CREAT|O_WRONLY|O_APPEND,
                         S_IRUSR|S_IWUSR );
  }
  return log_fd;
}

static void close_log_file( void ) __attribute__((__destructor__));

static void close_log_file( void ) {
  int fd = log_fd;
  log_fd = -1;
  if( fd >= 0 )
    close( fd );
}

static int my_lock( int fd ) {
  int ret = 0;
  static struct flock lock = {
    .l_type = F_WRLCK,
    .l_whence = SEEK_SET,
    .l_start = 0,
    .l_len = 0
  };
  do {
    errno = 0;
    ret = fcntl( fd, F_SETLKW, &lock );
  } while( ret < 0 && errno == EINTR );
  return ret >= 0;
}

static int my_unlock( int fd ) {
  static struct flock lock = {
    .l_type = F_UNLCK,
    .l_whence = SEEK_SET,
    .l_start = 0,
    .l_len = 0
  };
  return fcntl( fd, F_SETLK, &lock ) >= 0;
}

static int write_all( int fd, void const* buf, size_t len ) {
  ssize_t written = 0;
  ssize_t ret = 0;
  do {
    errno = 0;
    ret = write( fd, (((char const*)buf)+written), len-written);
  } while( (ret < 0 && errno == EINTR) ||
           (ret >= 0 && ((size_t)(written+=ret)) < len) );
  return ret >= 0;
}

static int write_name( int fd, char const* s ) {
  static char const* hexdigits = "0123456789abcdef";
  char buf[ 5 ] = "\\x00";
  while( *s != '\0' ) {
    buf[ 2 ] = hexdigits[ (((unsigned char)*s) >> 4) & 0xF ];
    buf[ 3 ] = hexdigits[  ((unsigned char)*s)       & 0xF ];
    if( !write_all( fd, buf, sizeof( buf )-1 ) )
      return 0;
    ++s;
  }
  return 1;
}

static void report_fork( pid_t cpid ) {
  int myerrno = errno;
  int fd = open_log_file();
  if( fd >= 0 && my_lock( fd ) ) {
    write_int( fd, "%d", getpid() );
    write_literal( fd, " fork " );
    write_int( fd, "%d", cpid );
    write_literal( fd, "\n" );
    my_unlock( fd );
  }
  errno = myerrno;
}

static void report_exec( char const* prog ) {
  int fd = open_log_file();
  if( fd >= 0 && my_lock( fd ) ) {
    write_int( fd, "%d", getpid() );
    write_literal( fd, " exec " );
    write_name( fd, prog );
    write_literal( fd, "\n" );
    my_unlock( fd );
  }
}

static size_t count_arglist( va_list* ap ) {
  size_t n = 1;
  while( va_arg( *ap, char const* ) != NULL )
    ++n;
  return n;
}

static char** make_argv( size_t argc, char const* arg, va_list* ap ) {
  char** argv = malloc( (argc+1) * sizeof( char* ) );
  if( argv != NULL ) {
    size_t i = 1;
    argv[ 0 ] = (char*)arg;
    while( (argv[ i++ ] = va_arg( *ap, char* )) != NULL )
      ;
  }
  return argv;
}

/* on every exec with explicit given environment, we need to modify
 * the following five variables */

#define LDP "LD_PRELOAD="
#define LMTF "BUILDSH_TEMPFILE="
#define LMPL "BUILDSH_PRELOAD="
#define DIL "DYLD_INSERT_LIBRARIES="
#define DFFNS "DYLD_FORCE_FLAT_NAMESPACE="

static size_t scan_env( char* const* env, size_t* ldp_pos,
                        size_t* dil_pos, size_t* dffns_pos,
                        size_t* lmtf_pos, size_t* lmpl_pos ) {
  size_t n = 0;
  *ldp_pos = *dil_pos = *dffns_pos = *lmtf_pos = *lmpl_pos = -1;
  /* count and remember positions for variables to replace */
  while( env[ n ] != NULL ) {
    if( *ldp_pos != (size_t)-1 &&
        !strncmp( env[ n ], LDP, sizeof( LDP ) ) )
      *ldp_pos = n;
    else if( *lmtf_pos != (size_t)-1 &&
             !strncmp( env[ n ], LMTF, sizeof( LMTF ) ) )
      *lmtf_pos = n;
    else if( *lmpl_pos != (size_t)-1 &&
             !strncmp( env[ n ], LMPL, sizeof( LMPL ) ) )
      *lmpl_pos = n;
    else if( *dil_pos != (size_t)-1 &&
             !strncmp( env[ n ], DIL, sizeof( DIL ) ) )
      *dil_pos = n;
    else if( *dffns_pos != (size_t)-1 &&
             !strncmp( env[ n ], DFFNS, sizeof( DFFNS ) ) )
      *dffns_pos = n;
    ++n;
  }
  return n;
}

static void free_env( char** env, size_t ldp_pos, size_t dil_pos,
                      size_t dffns_pos, size_t lmtf_pos,
                      size_t lmpl_pos ) {
  if( env != NULL ) {
    if( ldp_pos != (size_t)-1 )
      free( env[ ldp_pos ] );
    if( dil_pos != (size_t)-1 )
      free( env[ dil_pos ] );
    if( dffns_pos != (size_t)-1 )
      free( env[ dffns_pos ] );
    if( lmtf_pos != (size_t)-1 )
      free( env[ lmtf_pos ] );
    if( lmpl_pos != (size_t)-1 )
      free( env[ lmpl_pos ] );
    free( env );
  }
}

static char* concat( char const* s1, size_t len1, char const* s2 ) {
  size_t len2 = strlen( s2 );
  char* mem = malloc( len1 + len2 + 1 );
  if( mem != NULL ) {
    memcpy( mem, s1, len1 );
    memcpy( mem+len1, s2, len2+1 );
  }
  return mem;
}

static char dffns_default[] = DFFNS "y";
static char lmtf_default[] = LMTF BUILDSH_TEMPFILE_DEFAULT;

static int handle_lmtf( char** dest, char* cur, size_t* pos ) {
  char const* fname = getenv( "BUILDSH_TEMPFILE" );
  if( cur != NULL &&
      ((fname && !strcmp( cur + sizeof( LMTF )-1, fname )) ||
       (!fname && !strcmp( cur + sizeof( LMTF )-1,
                           BUILDSH_TEMPFILE_DEFAULT ))) ) {
    dest[ *pos ] = cur; /* existing value is good enough */
    *pos = -1; /* no need for freeing memory */
    return 1;
  } else if( fname == NULL ) {
    dest[ *pos ] = lmtf_default; /* use default in static memory */
    *pos = -1; /* no need for freeing memory */
    return 1;
  } else {
    char* mem = concat( LMTF, sizeof( LMTF )-1, fname );
    dest[ *pos ] = mem;
    return mem != NULL;
  }
}

static int handle_lmpl( char** dest, char* cur, size_t* pos ) {
  char const* fname = getenv( "BUILDSH_PRELOAD" );
  if( fname == NULL )
    abort();
  if( cur != NULL && !strcmp( cur + sizeof( LMPL )-1, fname ) ) {
    dest[ *pos ] = cur;
    *pos = -1;
    return 1;
  } else {
    char* mem = concat( LMPL, sizeof( LMPL )-1, fname );
    dest[ *pos ] = mem;
    return mem != NULL;
  }
}

static int handle_ldp( char** dest, char* cur, size_t* pos,
                       char const* prefix, size_t prefixlen ) {
  char const* fname = getenv( "BUILDSH_PRELOAD" );
  if( fname == NULL )
    abort();
  if( cur != NULL ) {
    if( strstr( cur + prefixlen, fname ) ) {
      dest[ *pos ] = cur;
      *pos = -1;
      return 1;
    } else {
      size_t len1 = strlen( cur+prefixlen );
      size_t len2 = strlen( fname );
      char* mem = malloc( prefixlen+len1+len2+2 );
      if( mem ) {
        memcpy( mem, prefix, prefixlen );
        memcpy( mem+prefixlen, fname, len2 );
        mem[ prefixlen+len2 ] = ':';
        memcpy( mem+prefixlen+len2+1, cur+prefixlen, len1 );
      }
      dest[ *pos ] = mem;
      return mem != NULL;
    }
  } else {
    char* mem = concat( prefix, prefixlen, fname );
    dest[ *pos ] = mem;
    return mem != NULL;
  }
}

static char** clone_env( char* const* env, size_t* ldp_pos,
                         size_t* dil_pos, size_t* dffns_pos,
                         size_t* lmtf_pos, size_t* lmpl_pos ) {
  size_t i = 0;
  char** new_env = NULL;
  char* temp = NULL;
  size_t n = scan_env( env, ldp_pos, dil_pos, dffns_pos, lmtf_pos,
                       lmpl_pos );
  new_env = malloc( (n+6)*sizeof( char* ) );
  if( new_env == NULL )
    goto error;
  /* copy everything _except_ the values we want to replace */
  for( i = 0; i < n; ++i ) {
    if( i == *ldp_pos || i == *dil_pos || i == *dffns_pos ||
        i == *lmtf_pos )
      new_env[ i ] = NULL; /* fill in later */
    else
      new_env[ i ] = env[ i ];
  }
  /* replace (or add) the important variables */
  i = *dffns_pos != (size_t)-1 ? *dffns_pos : ++n;
  new_env[ i ] = dffns_default;
  *dffns_pos = -1; /* no need to free it later, it's static! */
  if( *lmtf_pos != (size_t)-1 )
    temp = env[ *lmtf_pos ];
  else {
    temp = NULL;
    *lmtf_pos = ++n;
  }
  if( !handle_lmtf( new_env, temp, lmtf_pos ) )
    goto error;
  if( *lmpl_pos != (size_t)-1 )
    temp = env[ *lmpl_pos ];
  else {
    temp = NULL;
    *lmpl_pos = ++n;
  }
  if( !handle_lmpl( new_env, temp, lmpl_pos ) )
    goto error;
  if( *ldp_pos != (size_t)-1 )
    temp = env[ *ldp_pos ];
  else {
    temp = NULL;
    *ldp_pos = ++n;
  }
  if( !handle_ldp( new_env, temp, ldp_pos, LDP, sizeof( LDP )-1 ) )
    goto error;
  if( *dil_pos != (size_t)-1 )
    temp = env[ *dil_pos ];
  else {
    temp = NULL;
    *dil_pos = ++n;
  }
  if( !handle_ldp( new_env, temp, dil_pos, DIL, sizeof( DIL )-1 ) )
    goto error;

  new_env[ n+1 ] = NULL;
  return new_env;

error:
  free_env( new_env, *ldp_pos, *dil_pos, *dffns_pos, *lmtf_pos,
            *lmpl_pos );
  return NULL;
}

