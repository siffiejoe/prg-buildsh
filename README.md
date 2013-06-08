#                               buildsh                              #

A build tool based on [Lua][1] and inspired by [fabricate][4]


##                             The Idea                             ##

Most people write some build tool during their programming career,
this is my attempt. Its goals are:

*   Portability

    A build tool should support not only different operating systems,
    but also as many different programming languages and tools as
    possible.

*   Simplicity

    By reading the build description you should get a good idea, what
    is going on. Preferably you can see the exact commandlines of the
    programming tools used to build the software and the order in
    which they are executed.

*   Flexibility

    You should not be limited by your build tool in what you can do
    during your software build. That means that you should have access
    to every option of your programming tools, and every program that
    might be installed on your system.

The simplest and most explicit build description so far have been
batch files or shell scripts. Every program invocation and every
option is visible in the file, no magic is going on. Typically some
forms of variables and loops are used to make the build description
shorter and more maintainable, but if you don't take it too far, you
basically get what you see in your build scripts.

The problem with batch files and shell scripts is, that they are not
portable at all. Not only do different operating systems use different
shells or command interpreters, but shells are often quite limited in
what they can do without external programs, and different operating
systems have different tools available.

The solution to this is to base the build tool on the most portable
scripting language available (which IMHO is [Lua][1]). Lua is
basically as portable (and as a consequence as limited in its
operating system interface) as ISO C, so additionally a binding to an
operating system abstraction layer ([APR][2]) is used for process
handling, file operations, etc. For general programming tasks like
string manipulation you get the expressiveness and portability of Lua,
and as far as the interface to the operating system is concerned you
get the portability of APR, which powers e.g. the [Apache][6] web
server and the [Subversion][7] version control system.

But if you rely on external programs (which you obviously must do for
a general build tool), you still need to take the availability of
executables and different filesystem layouts into account. You could
do this using OS detection and clever use of variables in your build
scripts, or some form of OS abstraction in the build tool itself. The
former would seriously affect the readability and simplicity of the
build description, the latter must necessarily use a least common
denominator approach, and therefore limit the flexibility of the build
tool, as well as hide most details of the command executions from the
build scripts. The approach taken by `buildsh` is to separate the
build descriptions for different operating system environments into
different files and let the build tool figure out, which one can be
executed on a given system. It does so by checking for the executables
used in a build description (before running the script), and by
providing means of checking for file locations, etc. in the initial
setup phase of a build script. If not all prerequisites in a build
script can be met, `buildsh` moves on the next one (after telling you
what was missing).

So far `buildsh` would handle your builds pretty well, if you only
intend to build your software *once* (like as an end-user). If you are
a developer, you typically change parts of the source code and
recompile, and you don't want your build tool to recompile the whole
thing unless it is really necessary.

For rebuilding only the necessary parts of a whole software package,
you need to handle dependencies somehow. Explicit dependencies like in
`make` blow up your build scripts, and are hard to keep up-to-date, or
even to identify in the first place (e.g. documentation generators
like [naturaldocs][5] scan entire directories for inputs and generate
entire directories full of output files). As a general/portable build
tool, `buildsh` also cannot simply choose to rely on builtin support
for certain project types (like include file scanning in C projects),
therefore a similar approach as in the [fabricate][4] build tool is
taken, which uses system call tracing to automatically identify input
files and output files during the first build. The following methods
of system call tracing are supported at the moment (with varying
degrees of maturity):

*   `strace` command (available on Linux)
*   `ktrace`/`kdump` (available on (e.g.) FreeBSD)
*   `tracker.exe` (available on Windows, if a recent .NET-framework is
    installed).
*   `LD_PRELOAD` (available on many Unixes, but very platform specific
    and at the moment only tested on Linux). This is probably also the
    only sane way to support recent MacOSes.

If none of those methods are available, `buildsh` falls back to
building everything everytime.


##             Differences/Enhancements Compared to Lua             ##

`buildsh` is basically a Lua 5.1 interpreter with the following
differences:

*   The invocation of the main executable is different. There is no
    interactive mode, there are no option switches.

    `buildsh [make.<xxx>.lua] [targets ...]`

    You can give an optional explicit build script if you don't want
    to rely on the automatic detection (the name of the build script
    must start with `make.` and end with `.lua`), and zero or more
    build targets (default is `build`) which identify functions
    exported from the build script. The special `clean` target removes
    all output dependencies, the special `list` target lists all
    exported targets. Both special targets can be redefined.

*   Identifiers can begin with a dollar character (`$`). Globals with
    such a name are reserved for external tools, though. They are
    checked at load time in the `PATH` environment variable as a
    prerequisite to the build script. For executables with
    incompatible names or complete executable paths, you can use the
    `$"name"` syntax instead, which will do the same thing.

*   You are not allowed to set globals in build scripts. This is a
    safety measure (although there are still many ways to break the
    following build scripts).

*   The Bytecode Inspector library ([lbci][3]) is available for you to
    `require` (in addition to any Lua library already installed on
    your system).

*   If you really need it, you can `require` a (limited) binding to
    the Apache Portable Runtime library, called `ape`.

*   The Lua `string` library contains two additional functions:
    `dirname(path)` and `basename(path [, suffix, ...])`. See below.
    They can also be used via the method syntax.

*   An additional library `make` is available as a global variable. It
    contains the following functions:

    *   `qw(string)`

        Splits the words in `string` at whitespace boundaries and
        returns these words in an array. This is for conveniently
        specifying program options/arguments.

    *   `gsub(s, pattern, replacement)`

        Similar to the function in the string library, but `s` may
        also be an array, in which case this function is recursively
        applied to all string/table elements in this array.

    *   `assert(check, reason)`

        Raises a special error which indicates a missing prerequisite
        for the current build script, so that `buildsh` moves on to
        the next available script.

    *   `have_exec(...)`

        Checks all arguments as file paths, or as file names in the
        `PATH` environment variable. Returns the first existing
        executable as a string or nil.

    *   `assert_exec(...)`

        Same as `have_exec(...)` but raises a special error that
        indicates missing dependencies for the current build script in
        case no matching executable can be found.

    *   `have_file(...)`

        Checks all arguments as file paths and returns the first
        existing path or nil.

    *   `assert_file(...)`

        Same as `have_file(...)` but raises a special error that
        indicates missing dependencies for the current build script in
        case no matching file can be found.

    *   `glob(...)`

        Returns an array of files matching the file path pattern(s)
        given as arguments. Only the filename part of a path may
        contain glob characters.

    *   `dofile(filename)`

        Runs the given Lua file in an empty environment and passes all
        return values or errors. You can use it to generate data for
        your build scripts via some external tool.

    *   `opt_dofile(filename)`

        Same as `dofile(filename)`, but does not raise an error if the
        file is not there.

    *   `basename(path [, suffix, ...])`

        Strips all directories (and any of the given filename
        suffixes) from the given path.

    *   `dirname(path)`

        Strips the last path component (the filename part) and the
        last directory separator from a file path.

    *   `argv(string)`

        Parses the given string into an array of arguments like a
        shell would do, including quoting. Due to a limitation in the
        APR library only the first line of the `string` is parsed.

    *   `pipe(program)`

        Checks for the executable `program` and returns a function
        that will run the program and capture is output (`stdout`).
        The resulting function takes its arguments via a table (which
        will be `flatten`ed), or the vararg parameter (`...`), or any
        combination thereof.

        If the program cannot be found, a special error is raised,
        that indicates a missing dependency for the current build
        script.

        This (and the `argv(string)` function above) is intended for
        cases like `make.argv( make.pipe"pkg-config"( "--cflags" ) )`,
        and therefore should only be used during the setup phase of a
        build script.

    *   `run(program)`

        Returns a function that will run the given `program` if its
        recorded dependencies have changed (in case a means of system
        call tracing is available). The returned function takes
        arguments similar to the `pipe(program)` case above, but if
        the first argument is a table, the `dir` field can be used to
        `chdir` to another directory before executing the program, and
        the `echo` field causes abbreviated output. The updated
        dependencies are saved if the program execution is successful.

        This function does not raise a dependency error, so you must
        use `assert_exec(...)` explicitly, or the special `$program`
        or `$"program"` syntaxes, if that is intended.

    *   `autoclean()`

        Removes all output dependencies. Can be used if you want to
        redefine the special `clean` target in your build script.


##                    Writing Build Descriptions                    ##

A build script for `buildsh` must return a table which maps target
names to Lua functions. This table is later used to run the functions
for the requested targets. You should call programs intended for
building something only inside those target functions. Outside you
should check for dependencies, build argument/options lists, etc.
(this is called initial setup phase above).

`buildsh` provides a special syntax for calling external programs:

*   `$program`, where `program` is a Lua identifier, resolves to a
    function that will execute `program` when called (see the
    `run(program)` function above. Additionally it will check for
    `program` in the `PATH` environment variable, and report a missing
    dependency via a special error, so that `buildsh` can report it
    and move on to the next build script.

*   `$"program"` can be used instead in case `program` is not a valid
    Lua identifier. If `program` looks like a file path, it is checked
    directly without consulting `PATH`. If a program might be found
    via different names you must fall back to something like this:

    ```lua
    local prog = make.assert_exec( name1, path2, name3 )
    -- later in a target function:
    make.run( prog )( args )
    ```


###                             Example                            ###

```lua
#!/usr/bin/env buildsh

local cflags = make.qw[[
  -Wall -Wextra -Wfatal-errors -fno-common -Os -fpic
  -Imoon -DMOON_PREFIX=ape -DNDEBUG
]]

local apr_cflags = make.argv( make.pipe"apr-1-config"( "--includes", "--cppflags", "--cflags" ) )
local apr_ldflags = make.argv( make.pipe"apr-1-config"( "--link-ld" ) )

local luainc = "-I"..make.assert_file( "/usr/include/lua5.2/lua.h",
                                       "/usr/local/include/lua52/lua.h",
                                       "/usr/include/lua.h",
                                       "/usr/local/include/lua.h",
                                       "/usr/include/lua5.1/lua.h",
                                       "/usr/local/include/lua51/lua.h" ):dirname()

local sources = make.glob( "src/*.c", "moon/*.c" )
local ofiles = make.gsub( sources, "%.c$", ".o" )

local function build()
  for _, cf in ipairs( sources ) do
    $gcc{ cflags, luainc, apr_cflags, "-c", "-o", cf:gsub( "%.c$", ".o" ), cf, echo=cf }
  end
  $gcc{ cflags, apr_cflags, "-shared", "-o", "ape.so", ofiles, apr_ldflags, echo="ape.so" }
end

local function documentation()
  $"ldoc.lua"{ "--quiet", ".", dir="doc", echo="doc/config.ld" }
end

local function all()
  build()
  documentation()
end

return {
  build = build,
  documentation = documentation,
  all = all,
}
```


##                               Links                              ##

*   Lua programming language ([Lua][1])
*   Apache Portable Runtime library ([APR][2])
*   Lua Bytecode Inspector library ([lbci][3])
*   Fabricate build tool ([fabricate][4])
*   Naturaldocs documentation generator ([naturaldocs][5])
*   Apache web server ([Apache][6])
*   Subversion version control system ([Subversion][7])


  [1]:  http://www.lua.org/
  [2]:  http://apr.apache.org/docs/apr/1.4/files.html
  [3]:  http://www.tecgraf.puc-rio.br/~lhf/ftp/lua/#lbci
  [4]:  http://code.google.com/p/fabricate/
  [5]:  http://www.naturaldocs.org/
  [6]:  http://httpd.apache.org/
  [7]:  http://subversion.apache.org/

