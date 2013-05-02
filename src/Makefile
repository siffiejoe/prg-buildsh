# makefile for building Lua
# see ../INSTALL for installation instructions
# see ../Makefile and luaconf.h for further customization

# == CHANGE THE SETTINGS BELOW TO SUIT YOUR ENVIRONMENT =======================

# Your platform. See PLATS for possible values.
PLAT= none

CC= gcc
CFLAGS= -O2 -Wall $(MYCFLAGS)
AR= ar rcu
RANLIB= ranlib
RM= rm -f
LIBS= -lm $(MYLIBS)

MYCFLAGS=
MYLDFLAGS=
MYLIBS=

# == END OF USER SETTINGS. NO NEED TO CHANGE ANYTHING BELOW THIS LINE =========

PLATS= aix ansi bsd freebsd generic linux macosx mingw posix solaris

LUA_A=	liblua.a
CORE_O=	lapi.o lcode.o ldebug.o ldo.o ldump.o lfunc.o lgc.o llex.o lmem.o \
	lobject.o lopcodes.o lparser.o lstate.o lstring.o ltable.o ltm.o  \
	lundump.o lvm.o lzio.o
LIB_O=	lauxlib.o lbaselib.o ldblib.o liolib.o lmathlib.o loslib.o ltablib.o \
	lstrlib.o loadlib.o linit.o
EXT_O=	lbci.o ape.o ape_env.o ape_extra.o ape_file.o ape_fnmatch.o \
	ape_fpath.o ape_pool.o ape_proc.o ape_time.o ape_user.o ape_random.o \
	moon.o

LUA_T=	lua
LUA_O=	lua.o

LUAC_T=	luac
LUAC_O=	luac.o print.o

BUILDSH_T= buildsh
BUILDSH_O= buildsh.o $(EXT_O)

ALL_O= $(CORE_O) $(LIB_O) $(LUA_O) $(LUAC_O) $(BUILDSH_O)
ALL_T= $(LUA_A) $(LUA_T) $(LUAC_T) $(BUILDSH_T)
ALL_A= $(LUA_A)
ALL_H=	build.lua.h make.lua.h base.lua.h strace.lua.h ktrace.lua.h \
	preload.lua.h tracker.lua.h

default: $(PLAT)

all:	$(ALL_T)

o:	$(ALL_O)

a:	$(ALL_A)

$(LUA_A): $(CORE_O) $(LIB_O)
	$(AR) $@ $(CORE_O) $(LIB_O)
	$(RANLIB) $@

$(LUA_T): $(LUA_O) $(LUA_A)
	$(CC) -o $@ $(MYLDFLAGS) $(LUA_O) $(LUA_A) $(LIBS)

$(LUAC_T): $(LUAC_O) $(LUA_A)
	$(CC) -o $@ $(MYLDFLAGS) $(LUAC_O) $(LUA_A) $(LIBS)

$(BUILDSH_T): $(BUILDSH_O) $(LUA_A)
	$(CC) -o $@ $(MYLDFLAGS) $(BUILDSH_O) $(LUA_A) $(LIBS)

build.lua.h: build.lua $(LUA_T)
	../src/$(LUA_T) lua2inc.lua build.lua

make.lua.h: make.lua $(LUA_T)
	../src/$(LUA_T) lua2inc.lua make.lua

base.lua.h: base.lua $(LUA_T)
	../src/$(LUA_T) lua2inc.lua base.lua

strace.lua.h: strace.lua $(LUA_T)
	../src/$(LUA_T) lua2inc.lua strace.lua

ktrace.lua.h: ktrace.lua $(LUA_T)
	../src/$(LUA_T) lua2inc.lua ktrace.lua

preload.lua.h: preload.lua $(LUA_T)
	../src/$(LUA_T) lua2inc.lua preload.lua

tracker.lua.h: tracker.lua $(LUA_T)
	../src/$(LUA_T) lua2inc.lua tracker.lua

clean:
	$(RM) $(ALL_T) $(ALL_O) $(ALL_H)

depend:
	@$(CC) $(CFLAGS) -MG -MM l*.c print.c moon.c ape*.c

echo:
	@echo "PLAT = $(PLAT)"
	@echo "CC = $(CC)"
	@echo "CFLAGS = $(CFLAGS)"
	@echo "AR = $(AR)"
	@echo "RANLIB = $(RANLIB)"
	@echo "RM = $(RM)"
	@echo "MYCFLAGS = $(MYCFLAGS)"
	@echo "MYLDFLAGS = $(MYLDFLAGS)"
	@echo "MYLIBS = $(MYLIBS)"

# convenience targets for popular platforms

none:
	@echo "Please choose a platform:"
	@echo "   $(PLATS)"

aix:
	$(MAKE) all CC="xlc" CFLAGS="-O2 -DLUA_USE_POSIX -DLUA_USE_DLOPEN `apr-1-config --includes --cppflags --cflags`" MYLIBS="-ldl `apr-1-config --cflags --link-ld`" MYLDFLAGS="-brtl -bexpall"

ansi:
	$(MAKE) all MYCFLAGS=-DLUA_ANSI

bsd:
	$(MAKE) all MYCFLAGS="-DLUA_USE_POSIX -DLUA_USE_DLOPEN `apr-1-config --includes --cppflags --cflags`" MYLIBS="-Wl,-E `apr-1-config --cflags --link-ld`"

freebsd:
	$(MAKE) all MYCFLAGS="-DLUA_USE_LINUX `apr-1-config --includes --cppflags --cflags`" MYLIBS="-Wl,-E `apr-1-config --link-ld`"

generic:
	$(MAKE) all MYCFLAGS=

linux:
	$(MAKE) all MYCFLAGS="-DLUA_USE_LINUX `apr-1-config --includes --cppflags --cflags`" MYLIBS="-Wl,-E -ldl `apr-1-config --cflags --link-ld`"

macosx:
	$(MAKE) all MYCFLAGS="-DLUA_USE_LINUX `apr-1-config --includes --cppflags --cflags`" MYLIBS="`apr-1-config --cflags --link-ld`"
# use this on Mac OS X 10.3-
#	$(MAKE) all MYCFLAGS=-DLUA_USE_MACOSX

mingw:
	$(MAKE) "LUA_A=lua51.dll" "LUA_T=lua.exe" \
	"AR=$(CC) -shared -o" "RANLIB=strip --strip-unneeded" \
	"MYCFLAGS=-DLUA_BUILD_AS_DLL" "MYLIBS=" "MYLDFLAGS=-s" lua.exe
	$(MAKE) "LUA_A=lua51.dll" "BUILDSH_T=buildsh.exe" \
	"AR=$(CC) -shared -o" "RANLIB=strip --strip-unneeded" \
	"MYCFLAGS=-DLUA_BUILD_AS_DLL -Iapr-win32/include" "MYLIBS=-Lapr-win32 -lapr-1" "MYLDFLAGS=-s" buildsh.exe
	$(MAKE) "LUAC_T=luac.exe" luac.exe

posix:
	$(MAKE) all MYCFLAGS="-DLUA_USE_POSIX `apr-1-config --includes --cppflags --cflags`" MYLIBS="`apr-1-config --cflags --link-ld`"

solaris:
	$(MAKE) all MYCFLAGS="-DLUA_USE_POSIX -DLUA_USE_DLOPEN `apr-1-config --includes --cppflags --cflags`" MYLIBS="-ldl `apr-1-config --cflags --link-ld`"

# list targets that do not create files (but not all makes understand .PHONY)
.PHONY: all $(PLATS) default o a clean depend echo none

# DO NOT DELETE

lapi.o: lapi.c lua.h luaconf.h lapi.h lobject.h llimits.h ldebug.h \
  lstate.h ltm.h lzio.h lmem.h ldo.h lfunc.h lgc.h lstring.h ltable.h \
  lundump.h lvm.h
lauxlib.o: lauxlib.c lua.h luaconf.h lauxlib.h
lbaselib.o: lbaselib.c lua.h luaconf.h lauxlib.h lualib.h
lbci.o: lbci.c lua.h luaconf.h lauxlib.h ldebug.h lstate.h lobject.h \
  llimits.h ltm.h lzio.h lmem.h lopcodes.c lopcodes.h
lcode.o: lcode.c lua.h luaconf.h lcode.h llex.h lobject.h llimits.h \
  lzio.h lmem.h lopcodes.h lparser.h ldebug.h lstate.h ltm.h ldo.h lgc.h \
  ltable.h
ldblib.o: ldblib.c lua.h luaconf.h lauxlib.h lualib.h
ldebug.o: ldebug.c lua.h luaconf.h lapi.h lobject.h llimits.h lcode.h \
  llex.h lzio.h lmem.h lopcodes.h lparser.h ldebug.h lstate.h ltm.h ldo.h \
  lfunc.h lstring.h lgc.h ltable.h lvm.h
ldo.o: ldo.c lua.h luaconf.h ldebug.h lstate.h lobject.h llimits.h ltm.h \
  lzio.h lmem.h ldo.h lfunc.h lgc.h lopcodes.h lparser.h lstring.h \
  ltable.h lundump.h lvm.h
ldump.o: ldump.c lua.h luaconf.h lobject.h llimits.h lstate.h ltm.h \
  lzio.h lmem.h lundump.h
lfunc.o: lfunc.c lua.h luaconf.h lfunc.h lobject.h llimits.h lgc.h lmem.h \
  lstate.h ltm.h lzio.h
lgc.o: lgc.c lua.h luaconf.h ldebug.h lstate.h lobject.h llimits.h ltm.h \
  lzio.h lmem.h ldo.h lfunc.h lgc.h lstring.h ltable.h
linit.o: linit.c lua.h luaconf.h lualib.h lauxlib.h
liolib.o: liolib.c lua.h luaconf.h lauxlib.h lualib.h
llex.o: llex.c lua.h luaconf.h ldo.h lobject.h llimits.h lstate.h ltm.h \
  lzio.h lmem.h llex.h lparser.h lstring.h lgc.h ltable.h
lmathlib.o: lmathlib.c lua.h luaconf.h lauxlib.h lualib.h
lmem.o: lmem.c lua.h luaconf.h ldebug.h lstate.h lobject.h llimits.h \
  ltm.h lzio.h lmem.h ldo.h
loadlib.o: loadlib.c lua.h luaconf.h lauxlib.h lualib.h
lobject.o: lobject.c lua.h luaconf.h ldo.h lobject.h llimits.h lstate.h \
  ltm.h lzio.h lmem.h lstring.h lgc.h lvm.h
lopcodes.o: lopcodes.c lopcodes.h llimits.h lua.h luaconf.h
loslib.o: loslib.c lua.h luaconf.h lauxlib.h lualib.h
lparser.o: lparser.c lua.h luaconf.h lcode.h llex.h lobject.h llimits.h \
  lzio.h lmem.h lopcodes.h lparser.h ldebug.h lstate.h ltm.h ldo.h \
  lfunc.h lstring.h lgc.h ltable.h
lstate.o: lstate.c lua.h luaconf.h ldebug.h lstate.h lobject.h llimits.h \
  ltm.h lzio.h lmem.h ldo.h lfunc.h lgc.h llex.h lstring.h ltable.h
lstring.o: lstring.c lua.h luaconf.h lmem.h llimits.h lobject.h lstate.h \
  ltm.h lzio.h lstring.h lgc.h
lstrlib.o: lstrlib.c lua.h luaconf.h lauxlib.h lualib.h
ltable.o: ltable.c lua.h luaconf.h ldebug.h lstate.h lobject.h llimits.h \
  ltm.h lzio.h lmem.h ldo.h lgc.h ltable.h
ltablib.o: ltablib.c lua.h luaconf.h lauxlib.h lualib.h
ltm.o: ltm.c lua.h luaconf.h lobject.h llimits.h lstate.h ltm.h lzio.h \
  lmem.h lstring.h lgc.h ltable.h
lua.o: lua.c lua.h luaconf.h lauxlib.h lualib.h
luac.o: luac.c lua.h luaconf.h lauxlib.h ldo.h lobject.h llimits.h \
  lstate.h ltm.h lzio.h lmem.h lfunc.h lopcodes.h lstring.h lgc.h \
  lundump.h
buildsh.o: buildsh.c lua.h luaconf.h lauxlib.h lualib.h moon.h \
  $(ALL_H)
lundump.o: lundump.c lua.h luaconf.h ldebug.h lstate.h lobject.h \
  llimits.h ltm.h lzio.h lmem.h ldo.h lfunc.h lstring.h lgc.h lundump.h
lvm.o: lvm.c lua.h luaconf.h ldebug.h lstate.h lobject.h llimits.h ltm.h \
  lzio.h lmem.h ldo.h lfunc.h lgc.h lopcodes.h lstring.h ltable.h lvm.h
lzio.o: lzio.c lua.h luaconf.h llimits.h lmem.h lstate.h lobject.h ltm.h \
  lzio.h
print.o: print.c ldebug.h lstate.h lua.h luaconf.h lobject.h llimits.h \
  ltm.h lzio.h lmem.h lopcodes.h lundump.h
moon.o: moon.c lua.h lauxlib.h moon.h
ape.o: ape.c lua.h lualib.h lauxlib.h moon.h ape.h
ape_env.o: ape_env.c lua.h lauxlib.h moon.h ape.h lualib.h
ape_extra.o: ape_extra.c lua.h lauxlib.h moon.h ape.h lualib.h
ape_file.o: ape_file.c lua.h lauxlib.h moon.h ape.h lualib.h ape_flag.h
ape_fnmatch.o: ape_fnmatch.c lua.h lauxlib.h moon.h ape.h lualib.h \
  ape_flag.h
ape_fpath.o: ape_fpath.c lua.h lauxlib.h moon.h ape.h lualib.h ape_flag.h
ape_pool.o: ape_pool.c lua.h lauxlib.h moon.h ape.h lualib.h
ape_proc.o: ape_proc.c lua.h lauxlib.h moon.h ape.h lualib.h
ape_random.o: ape_random.c lua.h lauxlib.h moon.h ape.h lualib.h
ape_time.o: ape_time.c lua.h lauxlib.h moon.h ape.h lualib.h
ape_user.o: ape_user.c lua.h lauxlib.h moon.h ape.h lualib.h

# (end of Makefile)
