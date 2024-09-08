/**************************************************************************************************
  flymake.h
  Copyright 2024 Drew Gislason
  license: <https://mit-license.org>

  Inspired by Rust's Cargo, flymake is a C/C++ project build, test and package manager, all in one.
**************************************************************************************************/
#ifndef FLYMAKE_H
#define FLYMAKE_H
#include "Fly.h"
#include "FlyCli.h"
#include "FlyList.h"
#include "FlyMem.h"
#include "FlyStr.h"
#include "FlyFile.h"
#include "FlyToml.h"

// allows source to be compiled with gcc or g++ compilers
#ifdef __cplusplus
  extern "C" {
#endif

#define FLYMAKESTATE_SANCHK   57073
#define FMK_SZ_EXT_MAX        16
#define FMK_SZ_OUT            "out/"
#define FMK_SZ_DEP_DIR        "deps/"
#define FMK_SZ_FLYMAKE_TOML   "flymake.toml"
#define FMK_SZ_VERSION        "1.0.1"
#define FMK_SRC_DEPTH         3

typedef struct
{
  bool_t  fAll;         // --all, build all files, clean all files, create all folders
  bool_t  fRebuild;     // -B, build main project files even if already built
  bool_t  fCpp;         // --cpp, used by cmd `new`, make a C++ program instead of C
  int     dbg;          // -D, enables --DEBUG=1 and -g flags
  int     debug;        // hidden option --debug
  bool_t  fLib;         // --lib, create lib/, not src/
  bool_t  fNoBuild;     // -n, don't build anything, but show all commands that would build sometbing
  bool_t  fRulesLib;    // -rl, use lib/ rules to build target folders
  bool_t  fRulesSrc;    // -rs, use src/ rules to build target folders
  bool_t  fRulesTools;  // -rt, use tools/ rules to build target files/folders
  int     verbose;      // -v, default verbose
  bool_t  fWarning;     // -w- turns of warnings as errors (no -Werror)
  bool_t  fUserGuide;   // --user-guide, prints users guide
} flyMakeOpts_t;

typedef enum
{
  FMK_ERR_NONE = 0,       // no error, doesn't print anything
  FMK_ERR_CUSTOM,         // Custom error message: FlyMakeErrPrint() doesn't print this
  FMK_ERR_MEM,            // couldn't allocate memory
  FMK_ERR_BAD_PATH,       // bad path or target, ppszErrExtra must be the path
  FMK_ERR_BAD_PROG,       // not a valid program
  FMK_ERR_BAD_TOML,       // invalid TOML file
  FMK_ERR_NOT_PROJECT,    // not a valid project
  FMK_ERR_NO_FILES,       // no files in folder
  FMK_ERR_NOT_SAME_ROOT,  // no files in folder
  FMK_ERR_NO_RULE,        // no build rule
  FMK_ERR_CLONE,          // can't git clone
  FMK_ERR_WRITE           // propblem writing to a file
} fmkErr_t;               // See also FlyMakeErrPrint()

typedef enum
{
  FMK_VERBOSE_NONE = 0,   // only show errors
  FMK_VERBOSE_SOME,       // 1 = normal level, show progress (default)
  FMK_VERBOSE_MORE,       // 2 = more info
} fmkVerbose_t;

typedef enum
{
  FMK_DEBUG_NONE = 0,   // 0 = no debugging info
  FMK_DEBUG_SOME,       // 1 = normal level, show progress
  FMK_DEBUG_MORE,       // 2 = more info 
  FMK_DEBUG_MUCH,       // 3 = lots of info 
  FMK_DEBUG_MAX         // 4+ = all debug info
} fmkDebug_t;

#define FMK_TOOLLIST_SANCHK     7001
#define FMK_TOOLLIST_MAX_TOOLS    16   // allocate blocks of tools

typedef struct fmkTool
{
  char           *szName;
  unsigned        nSrcFiles;
  const char    **aszSrcFiles;  // array of string pointers of source file names
} fmkTool_t;

typedef struct
{
  void           *hSrcList;     // list of source files
  fmkTool_t     **apTools;      // array of tool pointers
  unsigned        sanchk;
  unsigned        nTools;
  unsigned        nMaxTools;
} fmkToolList_t;

// [compiler]
// ".c" = {cc="cc {in} -c {incs}{warn}{debug}-o {out}", ll="cc {in} {libs}{debug}-o {out}"}
typedef struct
{
  void           *pNext;
  char           *szExts;     // e.g. ".c" or ".cc.cpp.cxx.c++"
  char           *szCc;       // e.g. "cc {in} -c {incs}{warn}{debug}-o {out}"
  char           *szCcDbg;    // e.g. "-g -DDEBUG=1";
  char           *szInc;      // e.g. "-I";
  char           *szWarn;     // e.g. "-Wall -Werror";
  char           *szLl;       // e.g. "cc {in} {libs}{debug}-o {out}"
  char           *szLlDbg;    // e.g. "-g";
} flyMakeCompiler_t;

// how to build a folder, see also FlyMakeFolderPrint() if changing order
typedef enum
{
  FMK_RULE_NONE,
  FMK_RULE_LIB,
  FMK_RULE_SRC,
  FMK_RULE_TOOL,
  FMK_RULE_PROJ,
} fmkRule_t;

typedef struct
{
  char         *szTarget;   // original target provided by user, e.g. "../src/foo"
  char         *szFolder;   // folder, e.g. "../lib/" or "src/"
  char         *szFile;     // file (no path), e.g. test_foo, project.a, or NULL if just folder
  fmkRule_t     rule;
} fmkTarget_t;

// [folders]
typedef struct
{
  void           *pNext;
  char           *szFolder; // relative to flymake.toml (root), e.g. "tools/"
  fmkRule_t       rule;     // FMK_RULE_LIB, FMK_RULE_SRC or FMK_RULE_TOOL
} flyMakeFolder_t;

struct flyMakeState;  // so each dep can include a state

// [dependencies]
// dep1 = { path="../dep1/lib/dep1.a", inc="../dep1/inc/" }               # inc dependency
// dep2 = { path="../dep2/" }                                             # path dependency
// dep3 = { git="https://github.com/drewagislason/flylib", version="*" }  # git dependency
typedef struct
{
  void                 *pNext;
  char                 *szName;       // dependency name, e.g. foo
  char                 *szVer;        // actual package version, e.g. "*", "1.2", "2.0.32"
  char                 *szRange;      // desired version range, e.g. "*", "1.2" is >= 1.2 and < 2.0
  flyStrSmart_t         libs;         // library name(s), e.g. ../some_path/foo/lib/foo.a
  char                 *szIncFolder;  // include folder, e.g. ../some_path/foo/inc/
  bool_t                fBuilt;       // TRUE if already built successfully
  struct flyMakeState  *pState;       // state for this dependency
} flyMakeDep_t;

typedef struct flyMakeState
{
  unsigned            sanchk;

  // filled in by higher layer
  flyMakeOpts_t       opts;           // options set from command-line
  const flyCli_t     *pCli;           // input command-line

  // see FlyMakeTomlRootFill()
  char                *szFullPath;    // full root path, e.g. "/Users/me/Documents/Work/my_project/"
  char                *szRoot;        // e.g. "" or "../../" or "../path/to/project/"
  char                *szInc;         // e.g. "" or "inc/" or "../../include/"
  char                *szDepDir;      // e.g. "deps/" or "../deps/"

  // see FlyMakeTomlAlloc()
  bool_t               fIsSimple;
  char                *szTomlFilePath;  // relative path to flymake.toml file
  char                *szTomlFile;      // entire TOML file loaded into memory, or NULL if no TOML file
  char                *szProjName;      // base project name, e.g. "myproj"
  char                *szProjVer;       // project version from flymake.toml file, e.g. "1.1.15"
  flyMakeCompiler_t   *pCompilerList;   // ptr to 1 or more compiler cmdlines for compiling, linking, etc..
  flyMakeFolder_t     *pFolderList;     // ptr to list of folders

  // see FlyMakeDepAlloc()
  flyMakeDep_t       *pDepList;       // ptr to list of dependencies (may be NULL)
  flyStrSmart_t       libs;           // e.g. "lib/myproj.a ../dep1/lib/dep1.a deps/bar/lib/bar.a"
  flyStrSmart_t       incs;           // e.g. "-I. -Iinc/ -I../dep1/inc/ -Ideps/bar/inc/"
  bool_t              fLibCompiled;   // TRUE if any library source file was compiled, as we need to relink

  // statistics
  unsigned            nCompiled;
  unsigned            nSrcFiles;
} flyMakeState_t;

// flymake.c
void                FlyMakeErrExit              (void);
fmkDebug_t          FlyMakeDebug                (void);
fmkVerbose_t        FlyMakeVerbose              (void);

// flymakestate.c
void                FlyMakeStateInit            (flyMakeState_t *pState);
flyMakeState_t     *FlyMakeStateClone           (flyMakeState_t *pState);
void               *FlyMakeStateFree            (flyMakeState_t *pState);
bool_t              FlyMakeIsState              (const flyMakeState_t *pState);
void                FlyMakeStatePrint           (const flyMakeState_t *pState);
void                FlyMakeStatePrintEx         (const flyMakeState_t *pState, bool_t fVerbose);
unsigned            FlyMakeStateDepth           (const flyMakeState_t *pState);

// flymakeclean.c
bool_t              FlyMakeCleanFiles           (flyMakeState_t *pState);

// flymakedep.c
bool_t              FlyMakeIsSameRoot           (flyMakeState_t *pState, const char *szTarget);
bool_t              FlyMakeIsSameFolder         (const char *szFolder1, const char *szFolder2);
fmkErr_t            FlyMakeDepDiscover          (flyMakeState_t *pState);
fmkErr_t            FlyMakeDepListBuild         (flyMakeState_t *pRootState);
void                FlyMakeDepListFree          (flyMakeDep_t *pDepList);
void                FlyMakeDepPrint             (const flyMakeDep_t *pDep);
void                FlyMakeDepListPrint         (const flyMakeDep_t *pDepList);
char               *FlyMakeFolderAlloc          (const char *szTarget, fmkErr_t *pErr);
void               *FlyMakeTargetFree           (fmkTarget_t *pTarget);
fmkTarget_t        *FlyMakeTargetAlloc          (flyMakeState_t *pState, const char *szTarget, fmkErr_t *pErr);
fmkErr_t            FlyMakeBuildLibs            (flyMakeState_t *pState);
fmkErr_t            FlyMakeBuild                (flyMakeState_t *pState, fmkTarget_t *pTarget, char **ppszErrExtra);

// flymakefolders.c
bool_t              FlyMakeCreateStdFolders     (flyMakeState_t *pState, const char *szFolder);
bool_t              FlyMakeFolderCreate         (flyMakeOpts_t *pOpts, const char *szFolder);
bool_t              FlyMakeFolderRemove         (fmkVerbose_t verbose, flyMakeOpts_t *pOpts, const char *szFolder);
int                 FlyMakeSystem               (fmkVerbose_t verbose, flyMakeOpts_t *pOpts, const char *szCmdline);

// flymakelist.c
void               *FlyMakeSrcListNew           (flyMakeCompiler_t *pCompilerList, const char *szFolder, unsigned depth);
void                FlyMakeSrcListPrint         (void *hSrcList);
void               *FlyMakeSrcListFree          (void *hSrcList);
bool_t              FlyMakeSrcListIs            (void *hSrcList);
unsigned            FlyMakeSrcListLen           (void *hSrcList);
const char         *FlyMakeSrcListGetName       (void *hSrcList, unsigned i);
fmkToolList_t      *FlyMakeToolListNew          (flyMakeCompiler_t *pCompilerList, const char *szFolder);
void               *FlyMakeToolListFree         (fmkToolList_t *pToolList);
fmkTool_t          *FlyMakeToolListFind         (const fmkToolList_t *pToolList, const char *szToolName);
bool_t              FlyMakeToolListIs           (const fmkToolList_t *pToolList);
void                FlyMakeToolListPrint        (const fmkToolList_t *pToolList);
void                FlyMakeToolPrint            (const fmkTool_t *pTool);

// flymakeprint.c
int                 FlyMakePrintf               (const char *szFormat, ...);
int                 FlyMakePrintfEx             (fmkVerbose_t level, const char *szFormat, ...);
int                 FlyMakeDbgPrintf            (fmkDebug_t level, const char *szFormat, ...);
void                FlyMakePrintErr             (fmkErr_t err, const char *szExtra);
fmkErr_t            FlyMakeErrMem               (void);
fmkErr_t            FlyMakeErrToml              (const flyMakeState_t *pState, const char *szToml, const char *szErr);

// flymaketoml.c
char               *FlyMakeTomlKeyAlloc         (const char *szTomlKey);
char               *FlyMakeTomlStrAlloc         (const char *szTomlStr);
fmkErr_t            FlyMakeTomlCheckString      (flyMakeState_t *pState, tomlKey_t *pKey);
char               *FlyMakeTomlRootFind         (const char *szPath, const flyMakeCompiler_t *pCompilerList, fmkErr_t *pErr);
bool_t              FlyMakeTomlRootFill         (flyMakeState_t *pState, const char *szRootFolder);
bool_t              FlyMakeTomlAlloc            (flyMakeState_t *pState, const char *szName);
const char         *FlyMakeTomlFmtCompile       (flyMakeState_t *pState, const char *szExt);
const char         *FlyMakeTomlFmtLink          (flyMakeState_t *pState, const char *szExt);
const char         *FlyMakeTomlFmtArchive       (flyMakeState_t *pState);
const char         *FlyMakeTomlFmtFileDefault   (void);
fmkRule_t           FlyMakeTomlFindRule         (flyMakeState_t *pState, const char *szFolder);
char               *FlyMakeFolderAllocLibName   (flyMakeState_t *pState, const char *szFolder);
char               *FlyMakeFolderAllocSrcName   (flyMakeState_t *pState, const char *szFolder);
void                FlyMakeFolderPrint          (const flyMakeFolder_t *pFolder);
void                FlyMakeFolderListPrint      (const flyMakeFolder_t *pFolderList);
flyMakeFolder_t *   FlyMakeFolderFindByRule     (const flyMakeFolder_t *pFolderList, fmkRule_t rule);
flyMakeFolder_t *   FlyMakeFolderFindByName     (const flyMakeFolder_t *pFolderList, const char *szRoot, const char *szName);
flyMakeCompiler_t  *FlyMakeCompilerListDefault  (flyMakeState_t *pState);
void                FlyMakeCompilerPrint        (const flyMakeCompiler_t *pCompiler);
void                FlyMakeCompilerListPrint    (const flyMakeCompiler_t *pCompilerList);
char               *FlyMakeCompilerAllExts      (const flyMakeCompiler_t *pCompilerList);
flyMakeCompiler_t  *FlyMakeCompilerFind         (const flyMakeCompiler_t *pCompilerList, const char *szExt);
flyMakeCompiler_t  *FlyMakeCompilerFindByKey    (const flyMakeCompiler_t *pCompilerList, const char *szTomlKey);
bool_t              FlyMakeCompilerFmtCompile   (flyStrSmart_t *pStr,
                                                 const flyMakeCompiler_t *pCompiler,
                                                 const char *szIn,
                                                 const char *szIncs,
                                                 const char *szWarn,
                                                 const char *szDebug,
                                                 const char *szOut);
bool_t              FlyMakeCompilerFmtLink      (flyStrSmart_t *pStr,
                                                 const flyMakeCompiler_t *pCompiler,
                                                 const char *szIn,
                                                 const char *szLibs,
                                                 const char *szDebug,
                                                 const char *szOut);

// some cross module read-only data
extern const char   g_szTomlFile[];
extern const char   g_szFmtArchive[];
extern const char   m_szFmkBanner[];
extern const char   g_szFlyMakeUserGuide[];

// allows source to be compiled with gcc or g++ compilers
#ifdef __cplusplus
  }
#endif

#endif // FLYMAKE_H
