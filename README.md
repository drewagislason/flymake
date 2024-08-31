# Flymake

Version 1.0 released 07/14/2024  
Copyright (c) 2024, Drew Gislason  
License: [MIT](https://mit-license.org)  

## Getting started

Inspired by Rust's Cargo, flymake is a C/C++ project build, test and package manager, all in one.

Flymake does not try to replace tools like make or cmake, which build programs and tools in
sophisticated ways, nor does it replace full featured package managers like Brew or conan.io.
However, flymake will suffice for many C projects and works well with depencency libraries from
GitHub or GitLab or your site local git repositories or folders.

Flymake is a command-line tool (that is, runs in bash or zsh) and can be built using any C99 or
greater C compiler. It runs on:

* macOs
* Linux
* Windows (with WSL 2.0, see <https://allthings.how/how-to-use-linux-terminal-in-windows-11/>)

## Overview

Flymake Features:

* Quickly create new C or C++ projects with common folders and files
* Build projects (programs) and packages (libraries) with configurable compiler and linker options
* Build debug or release versions of projects and packages
* Build and run test suite for projects and packages
* Easily incorporate existing C/C++ projects from GitHub or GitLab into your projects
* Automatically checks for dependancy version conflicts
* Configure compiler, linker and other project settings with flymake.toml file
* Create a shell script for compiling project without flymake, make or CMake

See flymake.md (the user manual) for more information on flymake.

## Compiling flymake

Make sure you have a C99 compliant C compiler installed with the command `cc`. Xcode works on
MacOS. Gcc works on other platforms. There is lots of help on this on the internet. Simply
Google "install C on Linux" or Windows WSL 2.0 or macOS.

Once installed, verify the C compiler truly is installed. At the command-prompt type:

```bash
$ cc --version
```

Assuming the C compiler reported a version, create flymake by typing the following:

```bash
$ git clone git@github.com:drewagislason/flymake.git
$ cd flymake/src/
$ ./makeall.sh
$ ./flymake --help
```

## Quick Start

The following commands create, build and run a simple "hello world" program, assuming you just built
flymake as described above.

```bash
$ cd ../..
$ flymake/src/flymake new world
$ flymake/src/flymake run world
```

If you want flymake accessible from any where, add it to your path. I put the flymake executable in
the folder `~/bin`.

```bash
$ mkdir ~/bin
$ sudo cp flymake/src/flymake ~/bin
```

In `~/.bashrc` (bash) or `~/.zshrc` (zsh), add `export PATH=$PATH:~/bin`.
