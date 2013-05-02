#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>



int main( void ) {
  pid_t cpid;
  int fd = open( "unixsys.c", O_RDONLY );
  if( fd >= 0 )
    close( fd );
  else
    perror( "open( \"unixsys.c\", O_RDONLY ) = " );

  if( mkdir( "dir", 0777 ) < 0 ) {
    perror( "mkdir( \"dir\", 0777 ) = " );
  } else {

    fd = open( "dir/temp_out1.txt", O_WRONLY|O_CREAT, 0666 );
    if( fd >= 0 )
      close( fd );
    else
      perror( "open( \"dir/temp_out1.txt\", O_WRONLY|O_CREAT, 0666 ) = " );

    fd = open( "dir/temp_out2.txt", O_RDWR|O_CREAT, 0666 );
    if( fd >= 0 )
      close( fd );
    else
      perror( "open( \"dir/temp_out2.txt\", O_RDWR|O_CREAT, 0666 ) = " );

    fd = openat( AT_FDCWD, "dir/temp_out3.txt", O_RDWR|O_CREAT, 0666 );
    if( fd >= 0 )
      close( fd );
    else
      perror( "openat( AT_FDCWD, \"dir/temp_out3.txt\", O_RDWR|O_CREAT, 0666 ) = " );

    if( mkdirat( AT_FDCWD, "dir/dir2", 0777 ) < 0 )
      perror( "mkdirat( AT_FDCWD, \"dir/dir2\", 0777 ) = " );

    if( renameat( AT_FDCWD, "dir/temp_out1.txt", AT_FDCWD, "dir/temp_out4.txt" ) < 0 )
      perror( "renameat( AT_FDCWD, \"dir/temp_out1.txt\", AT_FDCWD, \"dir/temp_out4.txt\" ) = " );

    fd = open( "dir", O_RDONLY );
    if( fd < 0 ) {
      perror( "open( \"dir\", O_RDONLY ) = " );
    } else {
      int fd2;

      fd2 = openat( fd, "temp_out5.txt", O_WRONLY|O_CREAT, 0666 );
      if( fd2 )
        close( fd2 );
      else
        perror( "openat( fd, \"temp_out5.txt\", O_WRONLY|O_CREAT, 0666 ) = " );

      if( mkdirat( fd, "dir3", 0777 ) < 0 )
        perror( "mkdirat( fd, \"dir3\", 0777 ) = " );

      if( renameat( fd, "temp_out2.txt", fd, "temp_out6.txt" ) < 0 )
        perror( "renameat( fd, \"temp_out2.txt\", fd, \"temp_out6.txt\" ) = " );

      if( fchdir( fd ) >= 0 ) {

        fd2 = open( "temp_out7.txt", O_WRONLY|O_CREAT, 0666 );
        if( fd2 >= 0 )
          close( fd2 );
        else
          perror( "open( \"temp_out7.txt\", O_WRONLY|O_CREAT, 0666 ) = " );

        if( rename( "temp_out7.txt", "temp_out8.txt" ) < 0 )
          perror( "rename( \"temp_out7.txt\", \"temp_out8.txt\" ) = " );

        if( chdir( ".." ) >= 0 ) {
          fd2 = open( "dir/temp_out9.txt", O_RDWR|O_CREAT, 0666 );
          if( fd2 >= 0 )
            close( fd2 );
          else
            perror( "open( \"dir/temp_out9.txt\", O_RDWR|O_CREAT, 0666 ) = " );
        } else
          perror( "chdir( \"..\" ) = " );

      } else
        perror( "fchdir( fd ) = " );

      close( fd );
    }

    fd = creat( "dir/temp_out10.txt", 0666 );
    if( fd >= 0 )
      close( fd );
    else
      perror( "creat( \"dir/temp_out10.txt\", 0666 ) = " );
  }

  if( (cpid = fork()) == 0 ) { /* child */

    execlp( "cat", "cat", "hello.lua", (char*)NULL );
    perror( "execlp( \"cat\", \"cat\", \"hello.lua\", (char*)NULL ) = " );
    _exit( EXIT_FAILURE );

  } else if( cpid < 0 )
    perror( "fork() = " );
  else {
    if( waitpid( cpid, NULL, 0 ) < 0 )
      perror( "waitpid( cpid, NULL, 0 ) = " );
  }

  return EXIT_SUCCESS;
}


