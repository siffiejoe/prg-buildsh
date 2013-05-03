#                               buildsh                              #

A build tool based on [Lua][1] and inspired by [fabricate][4]

##                             The Idea                             ##

Most people write some build tool during their programming career,
this is my attempt. Its goals are:

*   Portability

    A build tool should support not only different operating systems,
    but also as much different programming languages and tools as
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
basically get what you see during your build.

The problem with batch files and shell scripts is, that they are not
portable at all. Not only do different operating systems use different
shells or command interpreters, but shells are often quite limited in
what they can do without external programs, and different operating
systems have different tools available.

The solution to this was to base the build tool on the most portable
scripting language available (which IMHO is [Lua][1]). Lua is
basically as portable (and as a consequence as limited in its
operating system interface) as ISO C, so additionally a binding to an
operating system abstraction layer ([APR][2]) is used for process
handling, file operations, etc. For general programming tasks like
string manipulation you get the power and portability of Lua, as far
as the interface to the operating system is concerned you get the
portability of APR, which powers e.g. the Apache web server and the
Subversion version control system.

But if you rely on external programs (which you obviously must do for
a general build tool), you still need to take the availability of
executables and different filesystem layouts into account. You could
do this using OS detection and clever use of variables in your build
scripts, or some form of OS abstraction in the build tool itself. The
former would seriously affect the readability and simplicity of the
build description, the latter must necessarily use a least common
denominator approach, and therefore limit the flexibility of the build
tool, as well as hide most details of the command executions. The
approach taken by `buildsh` is to separate the build descriptions for
different operating system environments into different files and let
the build tool figure out, which one can be executed on a given
system. It does so by checking for the executables used in a build
description (before running the script), and by providing means of
checking for file locations, etc. in the initial setup phase of a
build script. If not all prerequisites in a build script can be met,
`buildsh` moves on the next build script (after telling you what was
missing).

So far `buildsh` would handle your builds pretty well, if you only
intend to build your software *once* (like as an end-user). If you are
a developer, you typically change parts of the source code and
recompile, and you don't want your build tool to recompile the whole
thing unless it is really necessary.

For rebuild only the necessary parts of a whole software package, you
need to somehow handle dependencies. Explicit dependencies like in
`make` blow up your build scripts, are hard to keep up-to-date, or
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

##                               Links                              ##

*   Lua programming language ([Lua][1])
*   Apache Portable Runtime library ([APR][2])
*   Lua Bytecode Inspector library ([lbci][3])
*   Fabricate build tool ([fabricate][4])
*   Naturaldocs documentation generator ([naturaldocs][5])


  [1]:  http://www.lua.org/
  [2]:  http://apr.apache.org/docs/apr/1.4/files.html
  [3]:  http://www.tecgraf.puc-rio.br/~lhf/ftp/lua/#lbci
  [4]:  http://code.google.com/p/fabricate/
  [5]:  http://www.naturaldocs.org/

