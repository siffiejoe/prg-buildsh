@echo off & setlocal
if not exist apr-win32\include\apr.h goto :setuperror
if not exist apr-win32\libapr-1.lib  goto :setuperror

if exist *.obj del *.obj

set CFLAGS=/nologo /MD /W3 /O2 /I ".\apr-win32\include" /I ".\apr-win32\include\arch" /I ".\apr-win32\include\arch\win32" /I ".\apr-win32\include\arch\unix" /c /D "NDEBUG" /D "LUA_BUILD_AS_DLL" /D _CRT_SECURE_NO_WARNINGS /D WIN32 /D WINNT /D _WINDOWS /D MOON_API=extern

cl.exe %CFLAGS% lapi.c
cl.exe %CFLAGS% lauxlib.c
cl.exe %CFLAGS% lbaselib.c
cl.exe %CFLAGS% lbci.c
cl.exe %CFLAGS% lcode.c
cl.exe %CFLAGS% ldblib.c
cl.exe %CFLAGS% ldebug.c
cl.exe %CFLAGS% ldo.c
cl.exe %CFLAGS% ldump.c
cl.exe %CFLAGS% lfunc.c
cl.exe %CFLAGS% lgc.c
cl.exe %CFLAGS% linit.c
cl.exe %CFLAGS% liolib.c
cl.exe %CFLAGS% llex.c
cl.exe %CFLAGS% lmathlib.c
cl.exe %CFLAGS% lmem.c
cl.exe %CFLAGS% loadlib.c
cl.exe %CFLAGS% lobject.c
cl.exe %CFLAGS% lopcodes.c
cl.exe %CFLAGS% loslib.c
cl.exe %CFLAGS% lparser.c
cl.exe %CFLAGS% lstate.c
cl.exe %CFLAGS% lstring.c
cl.exe %CFLAGS% lstrlib.c
cl.exe %CFLAGS% ltable.c
cl.exe %CFLAGS% ltablib.c
cl.exe %CFLAGS% ltm.c
cl.exe %CFLAGS% lundump.c
cl.exe %CFLAGS% lvm.c
cl.exe %CFLAGS% lzio.c

link.exe /nologo /DLL /IMPLIB:lua5.1.lib /OUT:lua5.1.dll *.obj
lib.exe /nologo /out:lua5.1-static.lib *.obj

cl.exe %CFLAGS% lua.c
link.exe /nologo /OUT:lua.exe lua.obj lua5.1.lib

cl.exe %CFLAGS% luac.c
cl.exe %CFLAGS% print.c
link.exe /nologo /OUT:luac.exe luac.obj print.obj lua5.1-static.lib


:buildsh

.\lua.exe lua2inc.lua build.lua make.lua base.lua strace.lua ktrace.lua preload.lua tracker.lua

cl.exe %CFLAGS% ape.c
cl.exe %CFLAGS% ape_env.c
cl.exe %CFLAGS% ape_extra.c
cl.exe %CFLAGS% ape_file.c
cl.exe %CFLAGS% ape_fnmatch.c
cl.exe %CFLAGS% ape_fpath.c
cl.exe %CFLAGS% ape_pool.c
cl.exe %CFLAGS% ape_proc.c
cl.exe %CFLAGS% ape_random.c
cl.exe %CFLAGS% ape_time.c
cl.exe %CFLAGS% ape_user.c
cl.exe %CFLAGS% moon.c
cl.exe %CFLAGS% buildsh.c
link.exe /nologo /OUT:buildsh.exe buildsh.obj ape*.obj moon.obj lua5.1.lib ".\apr-win32\libapr-1.lib"
if exist buildsh.exe (
  @echo To install put buildsh.exe and libapr-1.dll somewhere in your PATH.
  @echo Have fun!
)
exit /B 0


:setuperror
@echo The APR include files and libraries are not setup correctly for
@echo this project. You need to create a subdirectory called
@echo "apr-win32" and copy the import library "libapr-1.lib" and APR's
@echo complete "include" directory into it.
@echo.
@echo If you don't already have the APR installed, you can download
@echo the *UNIX* source code (don't use the windows source code) from
@echo http://apr.apache.org/. Unpack it somewhere (I recommend 7-zip
@echo for extracting .tar.gz archives), copy the file "include\apr.hw"
@echo to "include\apr.h" and execute "nmake /F libapr.mak" in the
@echo extracted directory. The import library should end up in the
@echo "Release" subdirectory as "libapr-1.lib" (you will need
@echo "libapr-1.dll" as well later), and the "include" subdirectory is
@echo the one you need in "apr-win32".
@echo.
@echo Tested with VC++ 2010 SP1 (pre-SP1 gives COFF-conversion errors)
exit /B 1

