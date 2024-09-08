/**************************************************************************************************
  flymake.c - a C/C++ project build, test and package manager, all in one.
  Copyright 2024 Drew Gislason
  license: MIT <https://mit-license.org>
**************************************************************************************************/
#include "flymake.h"

const char m_szFmkBanner[] =
  "\n"
  "-----------------------------------------------------------------------------\n"
  "                                 %s\n"
  "-----------------------------------------------------------------------------\n"
  "\n\n";

/*!
  @defgroup flymake - A C/C++ project build, test and package manager, all in one.

  version 1.0

  Inspired by Rust's Cargo, flymake is a C/C++ project build, test and package manager, all in one.

  Flymake does not try to replace tools like make or cmake, which build programs and tools in
  sophisticated ways, nor does it replace full featured package managers like Brew or conan.io.
  However, flymake will suffice for many C projects and works well with depencency libraries from
  GitHub or GitLab and in your site local folders and git repositories.

  Flymake is a command-line tool (that is, runs in bash or zsh) and can be built using any C99 or
  newer C compiler.

  flymake Features:

  * Quickly create new C or C++ projects with common folders and files
  * Easily Build projects (programs) and packages (libraries)
  * Build debug or release versions of projects and packages
  * Build and run test suite
  * Specify dependencies for projects, which in turn can have their own dependencies
  * Easily incorporate existing C or C++ projects from GitHub or GitLab into your projects
  * Configure project settings such as compiler, linker and dependencies with flymake.toml file
  * Create a shell script for compiling project without flymake, make or CMake

  A simple example (where $ is the command-line prompt):

  ```
  $ flymake new foo
  $ cd foo
  $ flymake run

  # flymake v1.0
  mkdir src/out/
  cc src/foo.c -c -I. -Iinc/  -Wall -Werror -o src/out/foo.o
  cc src/foo_print.c -c -I. -Iinc/  -Wall -Werror -o src/out/foo_print.o
  cc src/out/ *.o  -o src/foo
  # created program src/foo

  src/foo

  hello foo!
  ```
*/

// command prototypes
typedef fmkErr_t (*pfnCmd_t)(flyMakeState_t *pState);
static fmkErr_t FlyMakeCmdBuild(flyMakeState_t *pState);
static fmkErr_t FlyMakeCmdClean(flyMakeState_t *pState);
static fmkErr_t FlyMakeCmdNew  (flyMakeState_t *pState);
static fmkErr_t FlyMakeCmdNop  (flyMakeState_t *pState);
static fmkErr_t FlyMakeCmdRun  (flyMakeState_t *pState);
static fmkErr_t FlyMakeCmdTest (flyMakeState_t *pState);


typedef struct
{
  const char *szCmd;
  pfnCmd_t    pfnCmd;
  // const char *szHelp;
} flyMakeCmd_t;

// verbose and debug options are accessed globally, see FlyMakeVerbose(), FlyMakeDebug()
static int        m_verbose;
static fmkDebug_t m_debug;

static const char m_szVersion[] = "flymake v" FMK_SZ_VERSION;
static const char m_szHelp[]    =
  "Usage = flymake [options] command [args]\n"
  "\n"
  "Inspired by the Rust Lang tool Cargo, flymake can create new C/C++projects, build them, run them,\n"
  "test them and manage project dependencies.\n"
  "\n"
  "See <https://drewagislason.github.io/flymake-user-manual.html> for more information.\n"
  "\n"
  "Options:\n"
  "-B             Rebuild project (but not dependencies)\n"
  "-D[=#]         For build command: add -DDEBUG=1 flag when compiling. Use -D=2 to set -DDEBUG=2\n"
  "-n             Dry run (don't create any files)\n"
  "-v[=#]         Verbose level: -v- (error output only), -v (default: some), or -v=2 (more)\n"
  "--             For run/test commands: all following args/opts are sent to subprogram(s)\n"
  "--all          Rebuild project plus all dependencies\n"
  "--cpp          For new command: create a C++ project or package\n"
  "--help         This help screen\n"
  "--lib          For new command: create library/ and test/ folders\n"
  "--rN           Force build rules for all targets to one of: --rl (lib), --rs (src), --rt (tool)\n"
  "--user-guide   Print flyamke user guide to the screen\n"
  "--version      Display flymake version\n"
  "-w-            Turn off warning as errors on compile\n"
  "\n"
  "Commands:\n"
  "\n"
  "build  [--all] [-B] [-D] [--rN] [-w] [targets...]       Builds project or specific target(s)\n"
  "clean  [--all] [-B]                                     Clean all .o and other temporary files\n"
  "new    [--all] [--cpp] [--lib] folder                   Create a new C or C++ project or package\n"
  "run    [--all] [-B] [-D] [targets...] [-- arg1 -opt1]   Build and run target program(s)\n"
  "test   [--all] [-B] [-D] [targets...] [-- arg1 -opt1]   Build and run the program(s) in test/ folder\n";

static flyMakeCmd_t aCmds[] =
{
  { "build",  FlyMakeCmdBuild },
  { "clean",  FlyMakeCmdClean },
  { "new",    FlyMakeCmdNew },
  { "nop",    FlyMakeCmdNop },
  { "run",    FlyMakeCmdRun },
  { "test",   FlyMakeCmdTest },
};

/*-------------------------------------------------------------------------------------------------
  Find the command function based on name.

  @param    szCmdName   name of command
  @return   ptr to the command function, pfnCmd_t
*///-----------------------------------------------------------------------------------------------
static pfnCmd_t FlyMakeFindCmd(const char *szCmdName)
{
  unsigned  i;
  pfnCmd_t  pfnCmd = NULL;

  for(i = 0; i < NumElements(aCmds); ++i)
  {
    if(strcmp(szCmdName, aCmds[i].szCmd) == 0)
      pfnCmd = aCmds[i].pfnCmd;
  }

  return pfnCmd;
}

/*-------------------------------------------------------------------------------------------------
  Runs a single target program. Helper to FlyMakeCmdRun() and FlyMakeCmdTest()

  @param    szTarget    program to run, e.g. "src/foo"
  @param    pOpts       command-line options from state
  @param    pCmdline    working buffer to build command-line
  @param    pArgs       args to send to target program
  @return   TRUE if worked, FALSE if failed
*///-----------------------------------------------------------------------------------------------
static fmkErr_t FmkRun(const char *szTarget, flyMakeOpts_t *pOpts, flyStrSmart_t *pCmdline, const flyStrSmart_t *pArgs)
{
  fmkErr_t  err = FMK_ERR_NONE;
  int       ret;

  // create the cmdline
  FlyStrSmartCpy(pCmdline, "");
  if(!FlyStrNextSlash(szTarget))
    FlyStrSmartCat(pCmdline, "./");
  FlyStrSmartCat(pCmdline, szTarget);
  FlyStrSmartCat(pCmdline, pArgs->sz);

  // display and/or run the target cmdline
  if(pOpts->verbose)
    FlyMakePrintf("\n%s\n\n", pCmdline->sz);
  if(!pOpts->fNoBuild)
  {
    ret = system(pCmdline->sz);
    if(ret < 0)
      err = FMK_ERR_BAD_PROG;
  }

  return err;
}

/*-------------------------------------------------------------------------------------------------
  Runs all the tools in the folder. Assumes they have already been built

  @param    pState    cmdline options, etc...
  @param    szFolder  folder that contains 1 or more tools
  @param    pCmdline  buffer to build cmdline
  @param    pArgs     arguments and -opts to pass to program
  @return   TRUE if worked, FALSE if failed
*///-----------------------------------------------------------------------------------------------
static fmkErr_t FmkRunTools(flyMakeState_t *pState, const char *szFolder, flyStrSmart_t *pCmdline, flyStrSmart_t *pArgs)
{
  fmkToolList_t  *pToolList;
  flyStrSmart_t  *pToolPath;
  fmkErr_t        err = FMK_ERR_NONE;
  unsigned        i;

  pToolList = FlyMakeToolListNew(pState->pCompilerList, szFolder);
  if(pToolList)
  {
    // large enough for most tool names. Very long tool names will expand the smart buffer
    pToolPath = FlyStrSmartAlloc(strlen(szFolder) + 42);

    for(i = 0; !err && i <pToolList->nTools; ++i)
    {
      FlyStrSmartCpy(pToolPath, pToolList->apTools[i]->aszSrcFiles[0]);
      FlyStrPathOnly(pToolPath->sz);
      FlyStrSmartCat(pToolPath, pToolList->apTools[i]->szName);

      err = FmkRun(pToolPath->sz, &pState->opts, pCmdline, pArgs);
    }
  }
  FlyMakeToolListFree(pToolList);
  return err;
}

/*-------------------------------------------------------------------------------------------------
  Runs the target folder or file.

  @param    pState    cmdline options, etc...
  @param    pTarget   file or folder with rules
  @param    pCmdline  buffer to build cmdline
  @param    pArgs     arguments and -opts to pass to program
  @return   FMK_ERR_NONE if worked, otherwise error
*///-----------------------------------------------------------------------------------------------
static fmkErr_t FmkRunTarget(flyMakeState_t *pState, fmkTarget_t *pTarget, flyStrSmart_t *pCmdline, flyStrSmart_t *pArgs)
{
  char     *szTarget;
  fmkErr_t  err = FMK_ERR_NONE;

  // running a single program
  if(pTarget->rule == FMK_RULE_SRC)
  {
    if(pTarget->szFile)
      err = FmkRun(pTarget->szTarget, &pState->opts, pCmdline, pArgs);
    else
    {
      szTarget = FlyMakeFolderAllocSrcName(pState, pTarget->szFolder);
      if(!szTarget)
        err = FlyMakeErrMem();
      else
      {
        err = FmkRun(szTarget, &pState->opts, pCmdline, pArgs);
        FlyFree(szTarget);
      }
    }
  }

  // running test/ folder or examples/ folder type things, perhaps one test or all of them
  else if(pTarget->rule == FMK_RULE_TOOL)
  {
    if(pTarget->szFile)
      err = FmkRun(pTarget->szTarget, &pState->opts, pCmdline, pArgs);
    else
      err = FmkRunTools(pState, pTarget->szFolder, pCmdline, pArgs);
  }

  else
  {
    FlyMakePrintf("Error; Cannot run target %s\n", pTarget->szTarget);
    err = FMK_ERR_CUSTOM;
  }

  return err;
}

/*-------------------------------------------------------------------------------------------------
  Allocates a single smart string from a set of arguments in the command-line.
  
  Helper to FlyMakeCmdRun() and FlyMakeCmdTest().

  @param    pCli      command-line
  @return   flyStrSmart_t *, or NULL if not enough memory
*///-----------------------------------------------------------------------------------------------
static flyStrSmart_t * FmkArgs(const flyCli_t *pCli)
{
  flyStrSmart_t  *pArgs = NULL;
  int       start;
  int       i;

  pArgs = FlyStrSmartAlloc(100);
  start = FlyCliDoubleDash(pCli);
  if(start >= 1)
  {
    ++start;
    for(i = start; i < *pCli->pArgc; ++i)
    {
      if(!FlyStrSmartCat(pArgs, " "))
      {
        pArgs = FlyStrSmartFree(pArgs);
        break;
      }
      if(!FlyStrSmartCat(pArgs, pCli->argv[i]))
      {
        pArgs = FlyStrSmartFree(pArgs);
        break;
      }
    }
  }

  return pArgs;
}

/*-------------------------------------------------------------------------------------------------
  Clean files/folders that flymake would create

      Syntax: clean [--all] [-B]

  Option `-B` also deletes libraries and programs. Option `--all` also deletes dependencies.

  @param    pState    cmdline options, etc...
  @return   TRUE if worked, FALSE if failed
*///-----------------------------------------------------------------------------------------------
static fmkErr_t FlyMakeCmdClean(flyMakeState_t *pState)
{
  fmkErr_t  err = FMK_ERR_NONE;
  if(!FlyMakeCleanFiles(pState))
    err = FMK_ERR_BAD_PATH;
  return err;
}

/*-------------------------------------------------------------------------------------------------
  Create a new project

      Syntax: new [--all] [--cpp] [--lib] folder

  Jobs:

  1. Verify the folder can be created and is not inside another project (ask)
  2. Creates standard subfolders, e.g. inc/ src/ or lib/ test/ etc..
  3. Creates default files, e.g. README.md, flymake.toml
  4. Creates sample program so `flymake run` or `flymake test works`

  @param    pState    cmdline options, etc...
  @return   FMK_ERR_NONE if worked, error code if failed
*///-----------------------------------------------------------------------------------------------
static fmkErr_t FlyMakeCmdNew(flyMakeState_t *pState)
{
  fmkErr_t  err = FMK_ERR_NONE;
  if(!FlyMakeCreateStdFolders(pState, FlyCliArg(pState->pCli, 2)))
    err = FMK_ERR_BAD_PATH;
  return err;
}

/*-------------------------------------------------------------------------------------------------
  No operation. Used to print out debugging without doing anything.

  @param    pState    ignored, placeholder
  @return   FMK_ERR_NONE
*///-----------------------------------------------------------------------------------------------
static fmkErr_t FlyMakeCmdNop(flyMakeState_t *pState)
{
  return FMK_ERR_NONE;
}

/*-------------------------------------------------------------------------------------------------
  Build the project or a set of targets

  Syntax: build [--all] [-B] [-D] [--rN] [-w] [targets...]

  Build Command-line Examples:

      $ flymake build
      $ flymake build -B
      $ flymake build lib/ src/
      $ flymake build -rt mytools/ examples/
      $ flymake build -rs mysource/
      $ flymake build -rl mylib/
      $ flymake build ../myfolder/ -D --all
      $ flymake build tools/my_tool test/test_my_tool

  @param    pState    cmdline options, etc...
  @return   FMK_ERR_NONE or 
*///-----------------------------------------------------------------------------------------------
static fmkErr_t FlyMakeCmdBuild(flyMakeState_t *pState)
{
  fmkTarget_t    *pTarget     = NULL;
  char           *szErrExtra  = "";
  const char     *szTarget;
  unsigned        nArgs;
  fmkErr_t        err         = FMK_ERR_NONE;
  int             i;

  FlyAssert(pState && pState->szRoot);
  pState->nCompiled = pState->nSrcFiles = 0;

  // recursively discover and build dependencies
  // results in a list of dependencies for the root project and updated pState->incs and ->libs
  err = FlyMakeDepListBuild(pState);
  nArgs = FlyCliNumArgs(pState->pCli);

  if(!err)
  {
    // "flymake build" with no target builds entire project
    if(nArgs <= 2)
    {
      szTarget = pState->szRoot;
      pTarget = FlyMakeTargetAlloc(pState, szTarget, &err);
      if(!err)
        err = FlyMakeBuild(pState, pTarget, &szErrExtra);
    }
    else
    {
      for(i = 2; !err && i < nArgs; ++i)
      {
        szTarget = (char *)FlyCliArg(pState->pCli, i);
        pTarget = FlyMakeTargetAlloc(pState, szTarget, &err);
        if(!err)
          err = FlyMakeBuild(pState, pTarget, &szErrExtra);
      }
    }
  }

  if(err)
    FlyMakePrintErr(err, szErrExtra);
  else if(pState->nSrcFiles == 0)
    FlyMakePrintf("flymake warning: empty project\n");
  else if(pState->nCompiled == 0)
    FlyMakePrintf("# Everything is up to date\n");

  return err;
}

/*-------------------------------------------------------------------------------------------------
  Build entire project then run the given target file(s) and folder(s).

  Syntax: flymake run [-D] [--all] [target(s)...] [-- target_arg1 -target_opt1]

  If no targets are specified, then runs main program in `src/` folder. If `--` is found, then any
  of the following arguments or options go to the target program(s).

  @param    pState        cmdline options, etc...
  @param    szDefFolder   HULL means use cmdline args
  @return   FMK_ERR_NONE if worked
*///-----------------------------------------------------------------------------------------------
static fmkErr_t FmkRunCliTargets(flyMakeState_t *pState, const char *szDefTarget)
{
  flyStrSmart_t      *pCmdline      = NULL;
  flyStrSmart_t      *pArgs         = NULL;
  unsigned            nArgs;
  char               *szErrExtra    = NULL;
  fmkTarget_t        *pTarget;
  fmkErr_t            err           = FMK_ERR_NONE;
  int                 i;

  // make sure state has been initialized
  FlyAssert(pState && pState->szRoot && pState->pCli);

  nArgs = FlyCliNumArgs(pState->pCli);

  // build everything first, as test or run depends on target(s) being built first
  err = FlyMakeDepListBuild(pState);
  if(!err)
  {
    szErrExtra = pState->szRoot;
    pTarget = FlyMakeTargetAlloc(pState, szErrExtra, &err);
    err = FlyMakeBuild(pState, pTarget, &szErrExtra);
    pTarget = FlyMakeTargetFree(pTarget);
  }

  // all programs share the same command-line and args
  if(!err)
  {
    pArgs    = FmkArgs(pState->pCli);
    pCmdline = FlyStrSmartAlloc(PATH_MAX);
    if(!pArgs || !pCmdline)
      err = FlyMakeErrMem();
  }

  // if no targets specified, use default, e.g. "src/foo" or "test/"
  if(!err && nArgs <= 2)
  {
    FlyAssert(szDefTarget);
    pTarget = FlyMakeTargetAlloc(pState, szDefTarget, &err);
    if(!err)
      err = FmkRunTarget(pState, pTarget, pCmdline, pArgs);
  }

  if(!err && nArgs > 2)
  {
    for(i = 2; !err && i < nArgs; ++i)
    {
      pTarget = FlyMakeTargetAlloc(pState, FlyCliArg(pState->pCli, i), &err);
      if(!err)
        err = FmkRunTarget(pState, pTarget, pCmdline, pArgs);
    }
  }

  // cleanup
  FlyStrSmartFree(pCmdline);
  FlyStrSmartFree(pArgs);

  // print the error
  if(err)
    FlyMakePrintErr(err, szErrExtra);

 return err;
}

/*-------------------------------------------------------------------------------------------------
  Build and run one or more targets programs.

  Syntax: flymake run [-D] [--all] [target(s)...] [-- target_arg1 -target_opt1]

  If no targets are specified, then runs main program in `src/` folder.

  If `--` is found, then any of the following arguments or options go to the target program(s).

  @param    pState    cmdline options, etc...
  @return   FMK_ERR_NONE if worked
*///-----------------------------------------------------------------------------------------------
static fmkErr_t FlyMakeCmdRun(flyMakeState_t *pState)
{
  flyMakeFolder_t  *pFolder;
  char             *szDefTarget   = NULL;
  const char       *szName;
  fmkErr_t          err           = FMK_ERR_NONE;

  // find default target
  pFolder = pState->pFolderList;
  while(pFolder)
  {
    if(pFolder->rule == FMK_RULE_SRC)
    {
      if(!szDefTarget)
        szDefTarget = pFolder->szFolder;
      szName = pFolder->szFolder + strlen(pState->szRoot);
      if((strcmp(szName, "src/") == 0) || (strcmp(szName, "source/") == 0))
      {
        szDefTarget = pFolder->szFolder;
        break;
      }
    }
    pFolder = pFolder->pNext;
  }

  // if no targets specified, MUST have a default target
  if(!szDefTarget && FlyCliNumArgs(pState->pCli) <= 2)
  {
    FlyMakePrintf("flymake error: Project %s has no src/ folder or program to run\n", pState->szProjName);
    err = FMK_ERR_CUSTOM;
  }

  if(!err)
    err = FmkRunCliTargets(pState, szDefTarget);

  return err;
}

/*-------------------------------------------------------------------------------------------------
  Build and run the test suite or one or more tests.

      flymake test [--all] [-D] [-B] [target(s)...] [-- target_arg -target_opt]

  1. If no target specified, then runs all programs in the  `test/` folder.
  2. If a target is a folder, builds that target and runs all programs in it
  3. If a target is a file, the builds that target file and runs it

  If `--` is used, then any of the following arguments or options go to the target program(s).

  @param    pState    cmdline options, etc...
  @return   FMK_ERR_NONE if worked
*///-----------------------------------------------------------------------------------------------
static fmkErr_t FlyMakeCmdTest(flyMakeState_t *pState)
{
  flyMakeFolder_t  *pFolder;
  char             *szDefTarget   = NULL;
  const char       *szName;
  fmkErr_t          err           = FMK_ERR_NONE;

  // find default target
  pFolder = pState->pFolderList;
  while(pFolder)
  {
    szName = pFolder->szFolder + strlen(pState->szRoot);
    if(strcmp(szName, "test/") == 0)
    {
      szDefTarget = pFolder->szFolder;
      break;
    }
    pFolder = pFolder->pNext;
  }

  if(!szDefTarget && FlyCliNumArgs(pState->pCli) <= 2)
  {
    FlyMakePrintf("flymake error: Project %s has no test/ folder\n", pState->szProjName);
    err = FMK_ERR_CUSTOM;
  }

  if(!err)
    err = FmkRunCliTargets(pState, szDefTarget);
  return err;
}

/*-------------------------------------------------------------------------------------------------
  Fatal error, exit program
  @return   none
*///-----------------------------------------------------------------------------------------------
void FlyMakeErrExit(void)
{
  exit(1);
}

/*-------------------------------------------------------------------------------------------------
  Indicate that we're creating a shell script
  @return   none
*///-----------------------------------------------------------------------------------------------
static void FmkPrintScriptHeader(int argc, const char *argv[])
{
  int i;
  FlyMakePrintf("# shell script for flymake ");
  for(i = 1; i < argc; ++i)
    FlyMakePrintf("%s ", argv[i]);
  FlyMakePrintf("\n");
}

/*!------------------------------------------------------------------------------------------------
  Return debug level
  @return   debug level (0-n)
*///-----------------------------------------------------------------------------------------------
fmkDebug_t FlyMakeDebug(void)
{
  return m_debug;
}

/*!------------------------------------------------------------------------------------------------
  Return debug level
  @return   debug level (0-n)
*///-----------------------------------------------------------------------------------------------
fmkVerbose_t FlyMakeVerbose(void)
{
  return m_verbose;
}

/*!------------------------------------------------------------------------------------------------
  Main entry to program
  @return   0 if worked, 1 if failed
*///-----------------------------------------------------------------------------------------------
int main(int argc, const char *argv[])
{
  flyMakeState_t      state;  // define before cliOpts so options can be placed directly in state
  const flyCliOpt_t   cliOpts[] =
  {
    { "-B",      &state.opts.fRebuild,      FLYCLI_BOOL },
    { "-D",      &state.opts.dbg,           FLYCLI_INT  },
    { "-n",      &state.opts.fNoBuild,      FLYCLI_BOOL },
    { "-v",      &state.opts.verbose,       FLYCLI_INT  },
    { "-w",      &state.opts.fWarning,      FLYCLI_INT  },
    { "--all",   &state.opts.fAll,          FLYCLI_BOOL },
    { "--cpp",   &state.opts.fCpp,          FLYCLI_BOOL },
    { "--debug", &state.opts.debug,         FLYCLI_INT  },
    { "--lib",   &state.opts.fLib,          FLYCLI_BOOL },
    { "--rl",    &state.opts.fRulesLib,     FLYCLI_BOOL },
    { "--rs",    &state.opts.fRulesSrc,     FLYCLI_BOOL },
    { "--rt",    &state.opts.fRulesTools,   FLYCLI_BOOL },
    { "--user-guide", &state.opts.fUserGuide, FLYCLI_BOOL },
  };
  const flyCli_t cli =
  {
    .pArgc      = &argc,
    .argv       = argv,
    .nOpts      = NumElements(cliOpts),
    .pOpts      = cliOpts,
    .szVersion  = m_szVersion,
    .szHelp     = m_szHelp
  };
  pfnCmd_t            pfnCmd        = NULL;
  const char         *szCmd;
  const char         *szPath;
  char               *szRootFolder;
  int                 nArgs;
  bool_t              fWorked       = TRUE;
  fmkErr_t            err           = FMK_ERR_NONE;

  // initialize flymake state
  FlyMakeStateInit(&state);
  state.pCli = &cli;
  state.opts.verbose = FMK_VERBOSE_SOME;
  state.opts.fWarning = TRUE;

  // parse the cmdline line into state fields
  if(FlyCliParse(&cli) != FLYCLI_ERR_NONE)
    FlyMakeErrExit();
  if(state.opts.fAll)
    state.opts.fRebuild = TRUE;
  m_debug = state.opts.debug;

  // print the manual to the screen
  if(state.opts.fUserGuide)
  {
    puts(g_szFlyMakeUserGuide);
    exit(0);
  }

  if(state.opts.fNoBuild)
  {
    if(!state.opts.verbose)
      state.opts.verbose = FMK_VERBOSE_SOME;
    FmkPrintScriptHeader(argc, argv);
  }

  // verbose is a global state
  m_verbose = state.opts.verbose;
  if(FlyMakeDebug())
    FlyMakePrintf(m_szFmkBanner, m_szVersion);
  else if(state.opts.verbose)
    FlyMakePrintf("\n# %s\n", m_szVersion);

  // don't allow two or more build rules
  if((state.opts.fRulesLib && (state.opts.fRulesSrc || state.opts.fRulesTools)) || 
     (state.opts.fRulesSrc && state.opts.fRulesTools))
  {
    FlyMakePrintf("flymake error: select only one of --rl, --rs or --rt\n");
    FlyMakeErrExit();
  }

  // assume build command if no arguments to flymake
  nArgs = FlyCliNumArgs(&cli);
  if(nArgs < 2)
  {
    pfnCmd = FlyMakeFindCmd("build");
    FlyAssert(pfnCmd);
  }

  // determine command
  else
  {
    szCmd = FlyCliArg(&cli, 1);
    pfnCmd = FlyMakeFindCmd(szCmd);
    if(!pfnCmd)
    {
      FlyMakePrintf("flymake error: Command `%s` not found. See flymake --help\n", szCmd);
      FlyMakeErrExit();
    }
  }

  // making a new project
  if(pfnCmd == FlyMakeCmdNew)
  {
    if(nArgs != 3)
    {
      FlyMakePrintf("flymake error: Command `new` requires exactly 1 target folder. See flymake --help\n");
      FlyMakeErrExit();
    }
  }

  // all other commands use a project root and an optional flymake.toml file
  else
  {
    // find the project root folder for build/test/run from a file or folder
    if(nArgs >= 3)
      szPath = FlyCliArg(state.pCli, 2);
    else
      szPath = ".";

    // set up default rules for compiling C/C++ programs
    state.pCompilerList = FlyMakeCompilerListDefault(&state);

    // determine root folder from a target file/folder
    szRootFolder = FlyMakeTomlRootFind(szPath, state.pCompilerList, &err);
    if(!szRootFolder || err)
    {
      FlyMakePrintErr(err, szPath);
      FlyMakeErrExit();
    }
    else if(!FlyMakeTomlRootFill(&state, szRootFolder))
    {
      FlyMakeErrMem();
      FlyMakeErrExit();
    }
    else
    {
      fWorked = FlyMakeTomlAlloc(&state, NULL);
      if(!fWorked)
      {
        FlyMakeErrExit();
      }
    }
  }

  // debugging
  if(FlyMakeDebug() >= FMK_DEBUG_MAX)
    FlyMakeStatePrintEx(&state, TRUE);
  else if(FlyMakeDebug())
    FlyMakeStatePrint(&state);
  if(FlyMakeDebug() > FMK_DEBUG_MAX)
    FlyMakeErrExit();

  // execute the command
  err = (*pfnCmd)(&state);

  FlyMakePrintf("\n");
  return err ? 1 : 0;
}
