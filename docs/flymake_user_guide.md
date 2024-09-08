# flymake User Manual

Version 1.0.1

Inspired by Rust's Cargo, flymake is a C/C++ project build, test and package manager, all in one.

flymake does not try to replace tools like make or CMake, which build programs and tools in
sophisticated ways, nor does it replace full featured package managers like Brew. However, flymake
will suffice for many C projects and works well with depencency libraries from GitHub or GitLab
and in your site local git repositories.

flymake is a command-line tool (that is, runs in bash or zsh) and can be built using any C99 or
newer C compiler.

flymake is named after <https://en.wikipedia.org/wiki/Firefly_(TV_series)>, an iconic space western.

## 1 - Overview

**flymake Features**

* Quickly create new C or C++ projects with common folders and files
* Easily Build projects (programs) and packages (libraries)
* Specify dependencies for projects, which in turn can have their own dependencies
  * Each Dependency can be in a folder or git repository (e.g. GitHub or GitLab)
  * Each project/dependency can specify version of its dependencies
* Build debug or release versions of projects and packages
* Build and run test suite
* Configure (in flymake.toml) project settings such as compiler, linker and dependencies
* Create a shell script for compiling the project without flymake, make or CMake

### 1.1 - flymake Usage

Below is the `--help` screen for flymake.

```
flymake v1.0

Usage = flymake [options] command [args]

Inspired by the Rust Lang tool Cargo, flymake can create new C/C++projects, build them, run them,
test them and manage project dependencies.

See <https://drewagislason.github.io/flymake-user-manual.html> for more information.

Options:
-B             Rebuild project or package (but not dependencies)
-D[=#]         For build command: add -DDEBUG=1 flag when compiling. Use -D=2 to set -DDEBUG=2
-n             Dry run (don't create any files)
-v[=#]         Verbose level: -v- (error output only), -v (default: some), or -v=2 (more)
--             For run/test commands: all following args/opts are sent to subprogram(s)
--all          Rebuild project or package plus all dependencies
--cpp          For new command: create a C++ project or package
--help         This help screen
--lib          For new command: create library folder
--rN           Force build rules to one of: --rl (lib), --rs (src), --rt (test)
--user-guide   Print flyamke user guide to the screen
--version      Display flymake version
-w-            Turn off warning as errors on compile

Commands:

build  [--all] [-B] [-D] [--rN] [-w] [targets...]       Builds project or specific target(s)
clean  [--all] [-B]                                     Clean all .o and other temporary files
new    [--all] [--cpp] [--lib] folder                   Create a new C or C++ project or package
run    [--all] [-B] [-D] [targets...] [-- arg1 -opt1]   Build and run target program(s)
test   [--all] [-B] [-D] [targets...] [-- arg1 -opt1]   Build and run the program(s) in test/ folder
```

### 1.2 - Flymake Command Examples

This creates and runs your basic hello world program. Use `flymake new world --cpp` to make a C++
hello world program instead of a C program:

```bash
$ flymake new world
$ flymake run world
hello world!
```

Here are some more examples. Do NOT copy the "# comment" part of the command-lines below.

```bash
$ flymake --help                  # get list of commands and options
$ flymake new sample --all        # create a new sample project with all the bells and whistles
$ cd sample
$ flymake build                   # build anything that has changed since last build
$ flymake                         # same as flymake build
$ flymake build -B -D             # rebuild project with debug version
$ flymake build -all              # rebuild project and all dependencies
$ flymake build test/test_sample  # build the single program test/test_sample
$ flymake clean                   # clean all .o (object) files in project
$ flymake clean --all             # clean everything including libraries, programs and dependencies
$ flymake run                     # build and run the main program in the src/ folder
$ flymake test                    # build and run all programs in the test/ folder
```

### 1.3 - Building From Your Own Source Code

Let's say you have written a C project with these source files:

```
proj.h
proj_main.c
proj_mem.c
proj_more.c
proj_other.c
```

To build this project with flymake, simple type:

```
$ cd project
$ flymake build

# flymake v1.0
mkdir out/
cc proj_main.c -c -I. -Wall -Werror -o out/proj_main.o
cc proj_mem.c -c -I. -Wall -Werror -o out/proj_mem.o
cc proj_more.c -c -I. -Wall -Werror -o out/proj_more.o
cc proj_other.c -c -I. -Wall -Werror -o out/proj_other.o
# created program project
```

Then, say you want to make a debug version and you want debug level 2 to get more debugging output
and asserts. Type:

```
$ flymake build -B -D=2
cc proj_main.c -c -I. -DDEBUG=2 -Wall -Werror -o out/proj_main.o
cc proj_mem.c -c -I. -DDEBUG=2 -Wall -Werror -o out/proj_mem.o
cc proj_more.c -c -I. -DDEBUG=2 -Wall -Werror -o out/proj_more.o
cc proj_other.c -c -I. -DDEBUG=2 -Wall -Werror -o out/proj_other.o
# created (debug) program project
```

Flymakes turns on maximum warnings/errors "-Wall -Werror" by default. Why not let the compiler
catch as many errors as possible? This can cause troubles if you are trying to build legacy code
or a 3rd party library. To turn off this feature, use the `-w-` option.

### 1.4 - An Example with Dependencies

Flymake is also a package manager, that is, it can easily add dependencies to any given project
from one or more git repositories.

The project `foo` below contains 3 dependencies, `bar`, `baz` and `qux`. This program performs some
simple math using these dependencies.

* bar contains the function bar_multiply()
* baz contains the function baz_square()
* qux contains the function qux_power()

Note: qux is a dependency of baz. That is, dependencies can have dependencies, all of which can
have versions to git clone the exact right version of each dependency.

You can clone this from the GitHub project <https://github.com/drewagislason/foo>

```
$ git clone git@github.com:drewagislason/foo.git
$ cd foo
$ flymake build --all

# flymake v1.0

# ---- Locating dependencies... ----
# Dependency git     : bar *: git@github.com:drewagislason/bar.git
# Cloning git@github.com:drewagislason/bar.git into deps/bar/
#     found version => 1.0
# Dependency git     : baz 1.1: git@github.com:drewagislason/baz.git
# Cloning git@github.com:drewagislason/baz.git into deps/baz/
#     found version => 1.1
# Dependency git     : qux *: git@github.com:drewagislason/qux.git
# Cloning git@github.com:drewagislason/qux.git into deps/qux/
#     found version => *

# ---- Building dependencies... ----
cc deps/bar/lib/bar.c -c -I. -Ideps/bar/inc/ -Wall -Werror -o deps/bar/lib/out/bar.o
ar -crs deps/bar/lib/bar.a deps/bar/lib/out/*.o
# created library deps/bar/lib/bar.a
cc deps/baz/lib/baz.c -c -I. -Ideps/baz/inc/ -Ideps/qux/ -Wall -Werror -o deps/baz/lib/out/baz.o
ar -crs deps/baz/lib/baz.a deps/baz/lib/out/*.o
# created library deps/baz/lib/baz.a
cc deps/qux/qux.c -c -I. -Ideps/qux/ -Wall -Werror -o deps/qux/out/qux.o
ar -crs deps/qux/qux.a deps/qux/out/*.o
# created library deps/qux/qux.a

# ---- Building project... ----
cc foo.c -c -I. -Ideps/bar/inc/ -Ideps/baz/inc/ -Wall -Werror -o out/foo.o
cc out/*.o deps/bar/lib/bar.a deps/baz/lib/baz.a deps/qux/qux.a -o foo
# created program foo
```

The dependencies are specified in a file called `flymake.toml`. This configuration file not only
specifies dependencies but also many other configurable flymake options.

## 2 - Building flymake

flymake is a command-line program written in C. It relies on the Firefly C Library (flylibc).

To view the source code of flymake, go to: <https://github.com/drewagislason/flymake>  
To view the source code of flylibc, go to: <https://github.com/drewagislason/flylibc>  

flymake can build itself, but of course, that's a chicken-and-egg problem, so to build it the first
time, do the following.

1. Install a C compiler (if not already installed)
2. Install git (if not  already installed)
3. Build flymake with makeall.sh

Installing C and git are beyond the scope of this document. You can search the internet on how to
accomplish these tasks on your OS.

Once installed, verify C and git are installed properly by opening a terminal window and typing:

```
$ cc --version
$ git --version
```

To build flymake from source, clone flymake and the C library flylibc in the same folder (this will
make subfolders flylibc and flymake):

```
$ git clone git@github.com:drewagislason/flylibc.git
$ git clone git@github.com:drewagislason/flymake.git
$ cd flymake/src
$ ./makeall.sh
$ ./flymake --help
```

One way to make flymake accessible to all your projects is to keep it in a folder in your `$PATH`.

```
$ mkdir ~/bin/
# edit ~/.zshrc or ~/.bashrc and add the following line
$ export PATH=$PATH:~/bin
# copy flymake with priveleges for all users
$ sudo cp flymake ~/bin/
```

If you are new to C, git or zsh or bash, consider the following links:

Git: <https://www.atlassian.com/git>  
Bash: <https://linuxhandbook.com/bash/>  
C: <https://www.w3schools.com/c/index.php>  

You might also be interested in some related projects:

flydoc : <https://github.com/drewagislason/flydoc>  
flylibc: <https://github.com/drewagislason/flylibc>  
flymake: <https://github.com/drewagislason/flymake>  
ned    : <https://github.com/drewagislason/ned>  
FireFly C Library documentation: <https://drewagislason.github.io>  

## 3 - Projects vs. Packages

For the purposes of flymake, A project is a collection of source files that make up a computer
program, e.g. `myprog` or `myprog.exe`, usually built from the `src/` or project root folder.

A package is the same, but results in a static library of object files, e.g. `mylib.a`, usually
built from the `lib/` folder.

The simple package below contains all source code in a single folder. This can actually be cloned
using git `git clone git@github.com:drewagislason/bar.git` and built with flymake.

```
bar.h
bar.c
```

As projects grow you'll likely organize them into a variety of files and folders (also known as
directories on Windows). For example:

```
docs\          Documents are placed here
inc\           Public API include files are placed here
lib\           Library source files for package
src\           Source files for the main project program
test\          Test suite is found here
flymake.toml   Project configuration for flymake
LICENSE.txt    MIT license. Or pick your own.
README.md      Default readme for the project, written in markdown text
```

Flymake builds the library, `lib/`, first, as it is usually needed by the program or test suite.

You can also specify a specific program to build, like `test/test_foo` or `src/foo`.

## 4 - flymake.toml

The optional `flymake.toml` file, always found in the root folder of the project, configures how
flymake operates for the given project.

TOML is a general configuration file format and is descriped here: <https://toml.io/en/>.

The default flymake.toml file looks like the following:

```
[package]
name = "myproject"
version = "0.1.0"

[dependencies]
# foo = { path="../foo/lib/foo.a", "../foo/inc"}
# bar = { path="../bar" }
# flylibc = { git="git@github.com:drewagislason/flylibc.git" }

[compiler]
# ".c" = { cc="cc {in} -c {incs}{warn}{debug} -o {out}", ll="cc {in} {libs}{debug}-o {out}" }
# ".cc.cpp.c++" = { cc="c++ {in} -c {incs}{warn}{debug} -o {out}", ll="c++ {in} {libs}{debug}-o {out}" }

[folders]
# "lib" = "--rl"
# "src" = "--rs"
# "test" = "--rt"
```

### 4.1 - flymake.toml `[package]` Section

While not mandatory, the `[package]` section provides the project or package version and name, and
should be included for all projects.

```
[package]
name = "my_project"
version = "1.0"
```

The `name=` field is the name of the project, which can be different than the name of the folder
that encapsulates it. If there is no `flymake.toml` or this field is not included, the name of the
project is assumed to be the same as the enclosing folder.

The `version=` field is in the form of "major.minor.patch", where major, minor and patch are
numbers, and constitutes the version of your project. Version "1" is assumed to be "1.0.0".

For a discussion of versions, see semantic versioning <https://semver.org>.

### 4.2 - flymake.toml `[compiler]` Section

The `[compiler]` section of flymake.toml specifies how to compile and link programs based on file
extension(s).

As a simple example, say you want to compile and old C project using the ansi (aka c89) standard.
Add the following line to the `[compiler]` section of flymake.toml.

```
[compiler]
".c" = { cc="cc {in} -c -ansi {incs}{warn}{debug}-o {out}" }
```

Note that you only need to specify the compile `cc=` key in the inline table, as the other keys
will continue to have their defaults. Only specify those fields you want to override.

In theory, flymake could make programs out of any programming language. In practice, the compiler
you are using must produce object files that can be added to a library (e.g. `lib.a`) file
using "ar" and can be linked together into a program.

The format of the default compiler settings are shown below:

```
[compiler]
".c" = { cc="cc {in} -c {incs}{warn}{debug}-o {out}", ll="cc {in} {libs}{debug}-o {out}" }
".c++.cpp.cxx.cc..C" = { cc="c++ {in} -c {incs}{warn}{debug}-o {out}", ll="c++ {in} {libs}{debug}-o {out}" }
```

The main keys in the `[compiler]` table specify the file extensions, for each compiler. For
example, files ending in `.c` will be compiled with the `cc` compiler, whereas files ending in
`.c++`, `.cpp` or `.cc` will be compiled with the `c++` compiler.

Each TOML inline table (in curly brackets) defines up to 6 keys

TOML Key  | Definition            | Default
--------- | --------------------- | ------------
`cc=`     | compiler command-line | "cc {in} -c {incs}{warn}{debug}-o {out}"
`ll=`     | linker command-line   | "cc {in} {libs}{debug}-o {out}"
`cc_dbg=` | compile debug options | "-g -DDEBUG=1"
`ll_dbg=` | link debug options    | "-g"
`inc=`    | include option        | "-I"
`warn=`   | warning options       | "-Wall -Werror"

For a discussion of C compiler versions, see <https://en.wikipedia.org/wiki/ANSI_C>  
For a discussion of C++ compiler versions, see <https://en.wikipedia.org/wiki/C%2B%2B#History>  

The `{markers}` such as `{in}` or `{out}` must be present in the `cc=` and `ll=` keys. Flymake uses
these markers to know where to put the various values. 

### 4.3 - flymake.toml `[folders]` Section

The flymake.toml file in the root of the project make optionally contain a `[folders]` section.

By default, flymake knows implicitely how to compile `lib/`, `src/` and `test/` folders, built with
`--rl`, `--rs` and `--rt` rule options respectively.

You can add your own folders with appropriate build rules in flymake.toml, for example:

```
[folders]
my_lib = "--rl"
my_src = "--rs"
examples = "--rt"
"sub/folder/" = "--rt"
```

1. Folder paths are relative to the flymake.toml file
2. If any folders aren't found, they are silently ignored
3. If the folder is found it is built with the specified rule
4. If folder path contains anything but (A-Za-z0-9_-), use quotes or single quotes

### 4.4 - flymake.toml `[dependencies]` Section

A dependency, in flymake, is a static library of object files, along with a folder of header files
that define the public API to objects, functions, constants and types.

The `flymake.toml` below shows an example of a project with 3 dependencies:

* Prebuilt depencencies - are not managed by flymake and have no version
* Package dependencies - are a folder tree of source code that can be built into a library
* Git dependencies - are package dependencies that are retrieved from a git repository

```
[dependencies]
foo = { path="../foo/lib/foo.a", inc="../foo/inc" }
bar = { path="../bar" }
flylib = { git="git@github.com:drewagislason/flylibc.git" }
```

Depencency `foo` is prebuilt, `bar` is a package dependency and `flylib` will be cloned from the
given git repository.

In the case of a git dependency, the git repo is checked out into the `deps/` folder created off
the root of the main project, e.g. `myproject/deps/flylib/`.

Note that the `path` to a dependency, if relative, is relative to the `flymake.toml` file that
specified the dependency.

The order of dependencies in the `[dependencies]` section of `flymake.toml` defines the order of
the include folders, and the order in which the packages are built and linked.

Dependencies are recursive, that is a dependency may depend on another set of dependencies and so
forth. They are processed wide first, then deep. That is, all the dependencies in a single
flymake.toml file are processed in order, then any deeper dependencies are processed.

Note: each dependency may have their own flymake.toml file, and so compile with different flags.

#### 4.4.1 Dependency Versions

Flymake assumes the use of Semantic Versioning with projects. See: <https://semver.org>  

The basics of Semantic Versioning:

1. MAJOR version when you make incompatible API changes
2. MINOR version when you add functionality in a backward compatible manner
3. PATCH version when you make backward compatible bug fixes

Most C code in GitHub or GitLab does not have a "version", or at least is not defined in a formal
way. In this case, the version will be `*`, or unspecified.

If version is specified when refering to a git or project dependency, the version is treated as a
range, as shown in the table below.

```
2.1    :=  >=2.1.0, <3.0.0
1.2.3  :=  >=1.2.3, <2.0.0
1.2    :=  >=1.2.0, <2.0.0
1      :=  >=1.0.0, <2.0.0
0.2.3  :=  >=0.2.3, <0.3.0
0.2    :=  >=0.2.0, <0.3.0
0.0.3  :=  >=0.0.3, <0.0.4
0.0    :=  >=0.0.0, <0.1.0
0      :=  >=0.0.0, <1.0.0
*      means the latest version
```

#### 4.4.2 Prebuilt Dependencies

Prebuilt dependencies must specify both a path to a static library and an include folder. As the
name implies, these must be prebuilt before flymake is invoked, as flymake will not compile them.

For example:

```
foo = { path="../foo/lib/foo.a", inc="../foo/inc" }
```

#### 4.4.3 Package Dependencies

A package is a project that flymake will build and which contains source code for a static object
file library. The `path=` is mandatory. `version=` is optional and is assumed to be `*` if not
specified.

```
bar = { path="../bar/", version="1" }
```

The only thing the optional `version=` key/value will do is inform you if the wrong version of the
dependency is at the given path.

#### 4.4.4 Git Dependencies

Flymake can automatically check out source code from git. Simply specify the URL to the git
repository to get the head of the default branch. If you want a specific `branch`, `sha` or
`version`, there are options for that as well.

If you are unfamiliar with git version control, see <https://github.com> or <https://gitlab.com>.
Both are web hosted sites to store your private or company git repositories.

A simple git tutorial can be found at: <https://git-scm.com> or <https://www.atlassian.com/git>.

As an example, say you want your project to depend on flylibc, a C library of useful functions such
as smart strings, generic linked lists and the ability to parse JSON and TOML files.

Simply include the URL as follows:

```
[dependencies]
flylibc = { git="git@github.com:drewagislason/flylibc.git" }
```

This will get the latest commit, default branch of flylibc from GitHub.

Flymake checks out this project into the `deps/` folder within your project, or example in
`myproj/deps/flylibc/`.

Note: git dependencies are generally checked out and built once, so the rebuild flag `-B` only
rebuilds your project, not the dependencies. If you want to rebuild your dependencies, use
`flymake build --all`. To delete all your git dependencies and re-check out and rebuild everying,
use `flymake clean --all`, then `flymake build`.

You must have git access rights to each git repository dependency either with SSH keys or
username/password: that is `git clone` and `git log` must work on that URL. Using SSH keys is
easier and requires no user input during checkout.

The optional `sha=` field let's you specify a specific git SHA, for example `07d2d5d` The SHA for
various commits can be found with `git log --oneline`.

The optional `branch=` field is the git branch. By default, the branch is usually `main`.

Versions are really flexible, but require that the developer uses versions in the commit log. If
not, versions won't work.

The version string specifies a range as shown in the table below:

```
*       match latest (head)
1       match any version 1.0.0 - less than 2.0.0
1.3     match any version 1.3.0 - less than 2.0.0
0.5     match any version 0.5.0 - less than 0.6.0
```

For more discussion on version, see <https://semver.org>

The git log sample below would find versions `1.0.0`, `0.9.1` and `0.1.2`.

```
$ git log --oneline
d5b4f75 (HEAD -> main, origin/main) Added FlyCli example
68bc142 
07d2d5d v1.0 of flymake
987837e flymake: final fixes per test suite
e13f284 flymake: fixed depencies for no build
f979d33 flymake: flymake fixed build targets
1cfa78b feature complete: 0.9.1
1e8cbf5 Completed flymake documentation. Features now fully defined from a user perspective
6df33b8 flymake: added basic build feature. Cannot build dependencies
8d8540e flymake: added "clean" feature to remove unwanted files
0720ae4 flymake: added "new" command to create folder structure and files
dee4fc6 flymake: version 0.1.2 first checkin. Can only do --version and --help
```

## 5 - A Discussion on C Dependies

Lets face it, C and C++ don't handle dependency versions well. For example, say you want to use
dependency `foo` with a public C function `foo()` prototyped in `foo.h`, implemented in `foo.c`.

In version 1.0, the prototype for foo() is `void foo(const char *sz, unsigned x);`.

In version 2.0, the prototype for foo() is `int foo(const char *sz);`.

Assume your project is depending on two libraries `dep1.a` and `dep2.a`, but these two libraries
depend on the two different versions of foo:

somedep1.c file (depends on version 1.0 of foo):

```c
#include "foo.h"

int func_dep1(void)
{
  foo("hello world", 99);
  return 0;
}
```

somedep2.c file (depends on version 2.0 of foo):

```c
#include "foo.h"

int func_dep2(void)
{
  return foo("hello world 99");
}
```

With me so far?

So, how does your main program, which calls on `func_dep1()` and `func_dep2()`, which depend on
different versions of `foo()`, actually work? (Hint: it doesn't!!)

```c
#include <stdio.h>
#include "somedep1.h"
#include "somedep2.h"

int foo(const char *sz)
{
  return printf("%s\n", sz);
}

int main(void)
{
  func_dep1();
  func_dep2();
  return 0;
}
```

You could:

1. Update everything to be compatible with one particular version of `foo()`
2. Update your program not to depend on both libraries
3. Change the name of `foo()` in version 2 to be `foo2()` and update any files needed

Symantic versioning helps as it (theoreticallty) ensures backwards compatibility with minor
versions, so a program or library compiled with version 1.0 could use library version 1.3.

See <https://semver.org>.

This problem isn't unique to C: Python has the same issue. See a discussion of Dependency Hell at
<https://medium.com/knerd/the-nine-circles-of-python-dependency-hell-481d53e3e025>

If you use verions in you git logs and/or flymake.toml files, then flymake can detect the version
conflict and inform you if you include incompatible versions. But flymake can't do anything to fix
the incompatibilty.

## 6 - Command Reference

The following sections describe the flymake commands and their options. Here they are in brief:

```
build  [--all] [-B] [-D] [--rX] [targets...]            Builds target(s)
clean  [--all] [targets...]                                  Clean all .o and other temporary files
new    [--all] [--cpp] [--lib] folder                        Create a new C or C++ project or library
run    [--all] [-B] [-D] [targets...] [-- arg1 -opt1]   Build and run the main program
test   [--all] [-B] [-D] [targets...] [-- arg1 -opt1]   Build and run the test suite
```

Single letter options start with a dash. Multi-letter options start with a double dash. Options can
go anywhere, that is `flymake build -B` is the same as `flymake -B build`.

### 6.1 - Build Command

The build command allows you to build debug or release versions of the project or package.

This can be as simple as building a single program (e.g my_prog), or building an entire project
with multiple libraries and programs from a source code heirrarchy of folders, along with building
dependency libraries cloned from various git repositories.

The basic syntax is below:

```bash
build  [--all] [-B] [-D[=#]] [--rN] [targets...]
```

Each option is described below:

- `--all` rebuilds all dependencies in addition to all files in the project or target
- `-B` rebuilds files in the project, but not the files in the dependencies
- `-D` adds the flags `-g` and `-DDEBUG=1` flags to the compiler and linker
- `--rl`, `--rs` and `--rt` build with library, source and tool rules respectively

For each target argument, flymake always builds one of:

1. Entire project
2. Folder with lib rules (creates a single lib.a file from all source in the folder)
3. Folder with src rules (creates a single program file from all the source in the folder)
4. Folder with tool rules (creates possibly multiple program files based on source names)
5. File with lib, src or tool rules, e.g. mylib.a, my_prog, my_tool

1. If no arguments
  - Build entire project
2. If file or folder target argument
  - If folder is project root, build entire project
  - If no rules specified
    1. If in `[folders]` section of flymake.toml, use the specified rule
    2. If path ends in src/ use src rules
    3. If path ends in lib/ use lib rules
    4. Otherwise use tool rules
  - If rule specified, use rule (lib, src, tool) to build folder

Some examples of the build command are below:

Build Command                 | Description
-------------                 | -----------
$ flymake                     | build entire project in current folder
$ flymake build               | build entire project in current folder
$ flymake build -B -D      | build debug version of entire project in current folder
$ flymake build ../project/   | build entire project
$ flymake build src/          | build src/ folder with source rules
$ flymake build lib/          | build lib/ folder with library rules
$ flymake build test/         | build test/ folder with tool rules
$ flymake build tools/        | build tools/ folder with tool rules
$ flymake build lib/my_lib.a  | build the library lib/my_lib.a with library rules
$ flymake build src/my_prog   | build the program src/my_prog with source rules
$ flymake build test/test_foo | build the program test/test_foo with tool rules
$ flymake build tools/my_tool | build the program tools/my_tool with tool rules
$ flymake build --rs .        | build current folder with source rules

The `[folders]` section in flymake.toml file specifies any special build rules per folder.
Otherwise, flymake builds `src/` and `source/` folders with the source rules, `lib/` and
`library/` folders with library rules and `test/` with tool rules.

For more details on the build rules, see [Section 6.1.1 - Build Rules]("#6.1.1-Build-Rules).

The output of a build with `-B` or `--all` can be used to make a shell script that will compile
your program. Just copy from the terminal window. All the lines that begin with `#` are comment
lines. Use `-v=2` as well in order to get the git cloning commands. Use `-n` to not actually build
but just show commands.

```bash
$ flymake --all

# flymake v1.0
cc lib/all_print.c -c -I. -Iinc/ -Wall -Werror -o lib/out/all_print.o
ar -crs lib/all.a lib/out/*.o
# created library lib/all.a
cc test/test_all.c -c -I. -Iinc/ -Wall -Werror -o test/out/test_all.o
cc test/out/test_all.o  lib/all.a -o test/test_all
# created program test_all
cc src/all.c -c -I. -Iinc/ -Wall -Werror -o src/out/all.o
cc src/out/*.o lib/all.a -o src/all
# created program src/all
```

Flymake can only build one project at a time. For example, this won't work:

```bash
$ flymake project1/ project2/
Error: project2/ is not in the same project
```

Instead build each project separately:

```bash
$ flymake project1/
$ flymake project2/
```

#### 6.1.1 - Build Rules

Flymake builds folders one of 3 ways:

1. Library Rules `--rl` creates a library from a folder tree of files
2. Source Rules `--rs` builds a single program from a folder tree of files
3. Test/Tool Rules `--rt` builds a set of programs in a folder based filenames

You cannot mix multiple rules on a single command-line: the rule applies to **all** targets.

```bash
$ flymake -rt folder1/ folder2/
```

The name rules for test/tool rules looks for a "base" name and any other files that have that base.
The following files, for exmaple, make 3 tools: `foo`, `bar` and `baz`.

```
foo.c
foo2.c
foo_print.c
bar.c
bar_none.c
baz.c
bazinga.c
```

### 6.2 - Clean Command

This command cleans up any .o files it created. If `--all` is specified, the library and
executables from `lib/`, `src/`, and the tools from other folders are deleted too.

flymake doesn't clean dependencies or remove the `deps/` folder. It's assumed that dependencies are
built once and stay built. To clean any dependencies, simply delete the `deps/` folder from the
root of your project.

```
$ rm -rf deps/
```

If using git, you can simply use `git clean` instead. Important! make sure you've checked in all
your source files before you do this or git will delete them!

### 6.3 - New Command

The `new`command creates a set of folders and files with which to start your project C or C++
project. The program or library created can be built with flymake.

Syntax: `flymake new [--all] [--cpp] [--lib] folder`

Below are brief descriptions of the default files and folders.

By default, the `inc/` and `src/` folders are created that build into the hello world program.

If using the `--cpp` flag, then the program or library will be .cpp instead of .c.

If using the `--lib` flag, then `lib/` and `test/` folders are created instead of the `src/`
folder. The test program uses the created C/C++ library.

If using the `--all` flag, then all the standard folders are created: `docs/`, `/inc`, `src/`,
`test/` and `lib/`. The `docs/` folder contains a user manual. The `inc/` folder contains the
project public API .h file(s). The `src/` folder contains the hello world program. The `lib/`
folder contains a print function and the `test/` folder verifies the library's print function.

Example 1, make a project (main program):

```
$ flymake new foo
# Creating folders...
mkdir foo
mkdir foo/inc/
mkdir foo/src/

# Creating files...
foo/LICENSE.txt
foo/README.md
foo/flymake.toml
foo/inc/foo.h
foo/src/foo.c
foo/src/foo_print.c

# Created project foo
```

Example 2, make a package (library):

```
$ flymake --lib new mylib
# Creating folders...
mkdir mylib
mkdir mylib/inc/
mkdir mylib/lib/
mkdir mylib/test/

# Creating files...
mylib/LICENSE.txt
mylib/README.md
mylib/flymake.toml
mylib/inc/mylib.h
mylib/lib/mylib_print.c
mylib/test/test_mylib.c

# Created package mylib
```

Example 3, make a C++ program with all the standard folders:

```
$ flymake --all --cpp new c_plus_plus
# Creating folders...
mkdir c_plus_plus
mkdir c_plus_plus/docs/
mkdir c_plus_plus/inc/
mkdir c_plus_plus/lib/
mkdir c_plus_plus/src/
mkdir c_plus_plus/test/

# Creating files...
c_plus_plus/LICENSE.txt
c_plus_plus/README.md
c_plus_plus/flymake.toml
c_plus_plus/docs/api_guide.md
c_plus_plus/inc/c_plus_plus.hpp
c_plus_plus/src/c_plus_plus.cpp
c_plus_plus/lib/c_plus_plus_print.cpp
c_plus_plus/test/test_c_plus_plus.cpp

# Created program c_plus_plus
```

### 6.4 - Run Command

Syntax: `flymake run [-D] [-B] [--all] [target(s)...] [-- target_arg1 -target_opt1]`

Builds then runs programsExamples:

```
$ flymake run                         # build and run src/myproj (main program)
$ flymake run -D -B --rs folder/      # build and run folder/ program using source rules
$ flymake run tools/foo -- --help     # run tools/foo --help
$ flymake run tools/ -- --help        # run all tools with --help option
```

Note that the special option `--` indicates all following options and arguments are for the
program(s) being run rather than to flymake itself.

### 6.5 - Test Command

Syntax: `flymake test [-D] [-B] [--all] [target(s)...] [-- target_arg1 -target_opt1]`

Similar to `flymake run or build`, but builds and runs the programs in the `test/` folder by default.

Example to run all tests:

```
$ flymake test

test/test_foo
test_foo passed
test/test_bar
test_bar passed
test/test_baz
test_baz passed
```

Example to run just `test_bar`:

```
flymake run test/test_bar

test/test_bar
test_bar passed
```
