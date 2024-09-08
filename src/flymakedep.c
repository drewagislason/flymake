/**************************************************************************************************
  flymakedep.c - handles checking out and building all dependencies
  Copyright 2024 Drew Gislason
  license: <https://mit-license.org>
**************************************************************************************************/
#include "flymake.h"
#include "FlySemVer.h"
#include "FlyStr.h"

static const char m_szOutFolder[]  = FMK_SZ_OUT;        // e.g. "out/"
static const char m_szOutFiles[]   = FMK_SZ_OUT "*.o";  // e.g. "out/*.o"
static const char m_szDepTable[]   = "dependencies";    // in flymake.toml, [dependencies]

// states and keys for proecessing dependencies
typedef struct
{
  flyMakeState_t *pRootState;   // root state
  flyMakeState_t *pState;       // state containing TOML file [dependencies]
  tomlKey_t      keyDep;        // key for dependency, e.g. foo = { path = "../foo/" }
  tomlKey_t      keyGit;        // key if git= "url" is pressent
  tomlKey_t      keyInc;        // key if inc= "folder/" is present
  tomlKey_t      keyPath;       // key if path= "some/path/" is present
  tomlKey_t      keyVer;        // key if version= "1.2" is present (version range)
  tomlKey_t      keySha;        // key if sha= "cba1855" is present
  tomlKey_t      keyBranch;     // key if branch= "main" is present
} fmkDepKeys_t;

typedef struct
{
  const char  *szKey;
  tomlKey_t   *pKey;
} fmkDepKeyVal_t;


/*-------------------------------------------------------------------------------------------------
  print the dependency

  @param    pDep    ptr to a dependency
  @return   none
*///-----------------------------------------------------------------------------------------------
void FlyMakeDepPrint(const flyMakeDep_t *pDep)
{
  FlyMakePrintf("\n---- pDep %p ----\n", pDep);
  FlyAssert(pDep);
  FlyMakePrintf("  pNext       %p\n", pDep->pNext);
  FlyMakePrintf("  szName      %s\n", FlyStrNullOk(pDep->szName));
  FlyMakePrintf("  szVer       %s\n", FlyStrNullOk(pDep->szVer));
  FlyMakePrintf("  szIncFolder %s\n", FlyStrNullOk(pDep->szIncFolder));
  FlyMakePrintf("  libs        %s\n", FlyStrNullOk(pDep->libs.sz));
  FlyMakePrintf("  fBuilt      %s\n", FlyStrTrueFalse(pDep->fBuilt));
  FlyMakePrintf("  pState      %p {", pDep->pState);
  if(pDep->pState && pDep->pState->szTomlFilePath)
    FlyMakePrintf("  szTomlFile=%s", pDep->pState->szTomlFilePath);
  FlyMakePrintf("---- end pDep %p ----\n\n", pDep);
}

/*-------------------------------------------------------------------------------------------------
  print the list of dependencies

  @param    pDep    ptr to a dependency
  @return   none
*///-----------------------------------------------------------------------------------------------
void FlyMakeDepListPrint(const flyMakeDep_t *pDepList)
{
  const flyMakeDep_t *pDep = pDepList;
  while(pDep)
  {
    FlyMakeDepPrint(pDep);
    pDep = pDep->pNext;
  }
}

/*-------------------------------------------------------------------------------------------------
  Prints the key/value

  @param    szKey         ptr to key
  @param    szTomlValue   ptr to TOML file string value
  @param    verbose       verbose mode to print
  @return   none
*///-----------------------------------------------------------------------------------------------
void FmkTomlKeyPrint(const char *szKey, const char *szTomlValue)
{
  char       *szValue;
  unsigned    sizeValue;

  // allocate strings
  sizeValue = FlyTomlStrLen(szTomlValue) + 1;
  szValue = FlyAlloc(sizeValue);

  // copy/print strings
  if(szValue)
  {
    FlyTomlStrCpy(szValue, szTomlValue, sizeValue);
    FlyMakePrintf("%s=\"%s\"", szKey, szValue);
  }

  // cleanup
  FlyStrFreeIf(szValue);
}

/*-------------------------------------------------------------------------------------------------
  print banner

      # ---- lib/foo.a (Lib Rules) ----

  @param    verbose     e.g. FMK_VERBOSE_SOME
  @param    szTarget    target
  @param    szParen     (2nd parameter), or NULL
  @return   none
*///-----------------------------------------------------------------------------------------------
static void FmkBanner(int verbose, const char *szTarget, const char *szParen)
{
  if(szParen)
    FlyMakePrintfEx(verbose, "\n# ---- %s (%s) ----\n", szTarget, szParen);
  else
    FlyMakePrintfEx(verbose, "\n# ------ %s ------\n", szTarget);
}

/*-------------------------------------------------------------------------------------------------
  Are the two folders the same? 

  @param    szFolder1    a folder, e.g. "/Users/me/folder" or ".." or even ""
  @param    szFolder2    a folder or file, e.g. "/Users/me/folder/myfile" or ".."
  @return   TRUE if they are the same folder
*///-----------------------------------------------------------------------------------------------
bool_t FlyMakeIsSameFolder(const char *szFolder1, const char *szFolder2)
{
  bool_t     fIsSame  = FALSE;

  // if folder is "", then treat it as "."
  if(*szFolder1 == '\0')
    szFolder1 = ".";
  if(*szFolder2 == '\0')
    szFolder2 = ".";

  fIsSame = FlyFileIsSamePath(szFolder1, szFolder2);

  if(FlyMakeDebug() >= FMK_DEBUG_MORE)
    FlyMakePrintf("FlyMakeIsSameFolder(%s,%s) = %u\n", szFolder1, szFolder2, fIsSame);

  return fIsSame;
}

/*-------------------------------------------------------------------------------------------------
  Add a path to a smart string folder. Example: "../folder" + "file.c" = "../folder/file.c"

  @param    pFolder       Smart string with folder in it
  @param    szFile        a file or subfolders to be 
  @return   0 if worked, -1 if program doesn't exist, 1-n = program error
*///-----------------------------------------------------------------------------------------------
static void FmkSmartPathCat(flyStrSmart_t *pFolder, const char *szFile)
{
  if(strpbrk(pFolder->sz, "/\\"))
  {
    if(!FlyStrIsSlash(FlyStrCharLast(pFolder->sz)))
      FlyStrSmartCat(pFolder, "/");
  }
  FlyStrSmartCat(pFolder, szFile);
}

/*-------------------------------------------------------------------------------------------------
  Allocate an outfile name from the input file and output folder

  @param    szInFileName    input filename (e.g. src/file.c)
  @return   smart string containing output filername (e.g. src/out/file.o)
*///-----------------------------------------------------------------------------------------------
static char * FmkGetOutName(const char *szOutFolder, const char *szInFileName)
{
  static const char   szObjExt[] = ".o";
  char               *szOutName;
  const char         *szBase;
  unsigned            len;
  size_t              size;

  size = strlen(szOutFolder) + strlen(szInFileName) + strlen(szObjExt) + 3;
  szOutName = FlyAlloc(size);
  if(szOutName)
  {
    FlyStrZCpy(szOutName, szOutFolder, size);
    szBase = FlyStrPathNameBase(szInFileName, &len);
    FlyStrZNCat(szOutName, szBase, size, len);
    FlyStrZCat(szOutName, szObjExt, size);
  }

  return szOutName;
}

/*-------------------------------------------------------------------------------------------------
  Compile a single file to a single obj in the out folder. Assumes folder/out is already made.

  1. If out/file.o is newer than file.c, then doesn't need to compile
  2. If pState->opts.fRebuild is set, always compiles

  @param    pState            flymake state
  @param    szOutFolder       e.g. "src/out/"
  @param    szFileName        e.g. "src/myufile.c"
  @return   -1 if failed, 0 if worked, 1 if didn't need to compile
*///-----------------------------------------------------------------------------------------------
static int FmkCompileFile(flyMakeState_t *pState, const char *szOutFolder, const char *szFileName)
{
  const flyMakeCompiler_t  *pCompiler;
  char               *szOutFile     = NULL;
  flyStrSmart_t      *pCmdline      = NULL;
  char               *szWarn;
  char               *szDebug;
  time_t              srcFileModTime;
  bool_t              fBuild        = TRUE;
  int                 ret           = 0;
  sFlyFileInfo_t      info;

  ++pState->nSrcFiles;
  if(FlyMakeDebug() >= FMK_DEBUG_MORE)
    FlyMakePrintf("FmkCompileFile(out=%s, file=%s), nSrcFiles %u\n", szOutFolder, szFileName, pState->nSrcFiles);

  // e.g. "cc %s -c %s%s%s-o %s" where %s is: {in} {incs} {warn} {cc_dbg} {out}
  // the file list should only contain known file extenstions, so this should always succeed
  pCompiler  = FlyMakeCompilerFind(pState->pCompilerList, FlyStrPathExt(szFileName));
  FlyAssert(pCompiler);

  // verify source file exists
  FlyFileInfoInit(&info);
  if(!FlyFileInfoGetEx(&info, szFileName) || !info.fExists)
  {
    if(FlyMakeDebug())
      FlyMakePrintf("dbg: Internal Error: file %s does not exist!\n", szFileName);
    ret = -1;
  }
  if(ret >= 0 && info.fIsDir)
  {
    if(FlyMakeDebug())
      FlyMakePrintf("dbg: Internal Error: %s is not a file!\n", szFileName);
    ret = -1;
  }
  srcFileModTime = info.modTime;

  // verify we can make outfile
  if(ret >= 0)
  {
    szOutFile = FmkGetOutName(szOutFolder, szFileName);
    if(szOutFile == NULL)
    {
      FlyMakeErrMem();
      ret = -1;
    }
  }

  // check date of folder/out/file.o vs folder/file.c to see if it needs to be compiled
  if(ret >= 0)
  {
    FlyFileInfoInit(&info);
    if(!pState->opts.fRebuild && FlyFileInfoGetEx(&info, szOutFile))
    {
      if(difftime(srcFileModTime, info.modTime) <= 0)
        fBuild = FALSE;
    }
  }

  // create cmdline, e.g. cc src/file.c -c -I. -Iinc/ -Wall -Werror -o src/out/file.o
  // "cc %s -c %s%s%s-o %s" where %s is: {in} {incs} {warn} {cc_dbg} {out}
  if(ret >= 0 && fBuild)
  {
    pCmdline = FlyStrSmartAlloc(128);
    if(pCmdline)
    {
      szWarn = pState->opts.fWarning ? pCompiler->szWarn : "";
      szDebug = pState->opts.dbg ? pCompiler->szCcDbg : "";
      if(!FlyMakeCompilerFmtCompile(pCmdline, pCompiler, szFileName, pState->incs.sz,
            szWarn, szDebug, szOutFile))
      {
        FlyMakeErrMem();
        ret = -1;
      }
      else
      {
        // any return not zero is an error
        ret = FlyMakeSystem(FMK_VERBOSE_SOME, &pState->opts, pCmdline->sz);
        if(ret != 0)
          ret = -1;

        // update statistics
        else
          ++pState->nCompiled;
      }
      FlyStrSmartFree(pCmdline);
    }
    else
      ret = -1;
  }

  FlyFreeIf(szOutFile);

  if(ret >= 0 && !fBuild)
    ret = 1;

  return ret;
}

/*-------------------------------------------------------------------------------------------------
  Compile a folder full of files. Does not link, just creates {folder}/out/file(s).o

  Used for both library and source rules (FMK_RULE_LIB, FMK_RULE_SRC), but not tools. See FmkTool

  The # of files compiled is returned in pFilesCompiled (0-n). If all files are up to date, and no
  option forces compile, then nothing is compiled.

  Also returns 1st file extension, so caller can know which "compiler" to use to link this project.

  Duties:

  1. Makes a list of all source files,  file.c, file2.cpp, etc...
  2. Only returns FALSE if a compile failed.
  3. Returns TRUE even if there are no files to compile.
  4. Only compiles if source file .c is newer than .o. unless option --all or -B was used

  @param    pState            state of flymake (flags, etc...)
  @param    szFolder          e.g. "", "src/" or "lib/"
  @param    pFilesCompiled    return value, # of files compiled (0-n)
  @param    szExt             optional return value if not NULL, the 1st file extension
  @return   TRUE if worked, FALSE if failed to compile a file
*///-----------------------------------------------------------------------------------------------
static bool_t FmkCompileFolder(flyMakeState_t *pState, const char *szFolder, unsigned *pFilesCompiled, char *szExt)
{
  void           *hSrcList        = NULL;
  char           *szOutFolder     = NULL;
  const char     *szFileName;
  unsigned        nFilesCompiled  = 0;
  unsigned        i;
  unsigned        size;
  int             ret;
  bool_t          fWorked         = TRUE;

  // default to no file extension returned
  if(szExt)
    *szExt = '\0';

  if(FlyMakeDebug())
    FlyMakePrintf("FmkCompileFolder(%s)\n", szFolder);

  hSrcList = FlyMakeSrcListNew(pState->pCompilerList, szFolder, FlyMakeStateDepth(pState));
  if(hSrcList && FlyMakeSrcListLen(hSrcList) > 0)
  {
    // allocate the output folder
    FlyAssert(*szFolder == '\0' || FlyStrPathIsFolder(szFolder));
    size = strlen(szFolder) + strlen(m_szOutFolder) + 2;
    szOutFolder = FlyAllocZ(size);
    if(!szOutFolder)
      fWorked = FALSE;

    // make out/ folder, e.g. "src/out" (OK if already exists)
    else
    {
      FlyStrZCpy(szOutFolder, szFolder, size);
      FlyStrPathAppend(szOutFolder, m_szOutFolder, size);
      if(!FlyMakeFolderCreate(&pState->opts, szOutFolder))
        fWorked = FALSE;
    }
  }

  if(fWorked && hSrcList && FlyMakeSrcListLen(hSrcList) > 0)
  {
    // return first file extension (so link can use proper link options)
    szFileName = FlyMakeSrcListGetName(hSrcList, 0);
    if(szExt)
      FlyStrZCpy(szExt, FlyStrPathExt(szFileName), FMK_SZ_EXT_MAX);

    nFilesCompiled = 0;
    for(i = 0; i < FlyMakeSrcListLen(hSrcList); ++i)
    {
      szFileName = FlyMakeSrcListGetName(hSrcList, i);
      ret = FmkCompileFile(pState, szOutFolder, szFileName);
      if(ret < 0)
        fWorked = FALSE;
      if(ret == 0)
        ++nFilesCompiled;
    }
    if(fWorked && !nFilesCompiled)
      FlyMakePrintfEx(FMK_VERBOSE_MORE, "# %s folder up to date\n", szFolder);
  }

  // done with source files
  FlyMakeSrcListFree(hSrcList);
  FlyFreeIf(szOutFolder);

  *pFilesCompiled = nFilesCompiled;

  return fWorked;
}

/*-------------------------------------------------------------------------------------------------
  Compile a single tool from a set of one or more source files

  @param    pState        state of flymake
  @param    pTool         list of .c files, and target link name
  @return   -1 if failed, 0 if worked, 1 if no need to compile or link
*///-----------------------------------------------------------------------------------------------
static int FmkToolCompile(flyMakeState_t *pState, const char *szOutFolder, const fmkTool_t *pTool)
{
  const flyMakeCompiler_t  *pCompiler;
  char               *szObj         = NULL; // single obj
  flyStrSmart_t      *pInObjs       = NULL; // list of input objs for linking
  flyStrSmart_t      *pToolOut      = NULL;
  flyStrSmart_t      *pCmdline      = NULL;
  char               *szDebug;
  unsigned            i;
  unsigned            nCompiled     = 0;
  int                 ret           = 0;
  bool_t              fWorked       = TRUE;

  // compile each source file in this tool
  for(i = 0; i < pTool->nSrcFiles; ++i)
  {
    ret = FmkCompileFile(pState, szOutFolder, pTool->aszSrcFiles[i]);

    // didn't work, e.g. source file didn't compile due to source code errors
    if(ret < 0)
    {
      fWorked = FALSE;
      break;
    }

    // ret of 1 means it didn't compile because source file is not newer than obj file
    // so only ret == 0 (worked and compiled) means this source file compiled
    if(ret == 0)
      ++nCompiled;
  }

  // assume link will use linker of the 1st source file
  if(fWorked)
  {
    pCompiler  = FlyMakeCompilerFind(pState->pCompilerList, FlyStrPathExt(pTool->aszSrcFiles[0]));
    FlyAssert(pCompiler);
  }

  // create a place to hold list of objs for this tool
  if(fWorked)
  {
    pInObjs = FlyStrSmartAlloc(PATH_MAX);
    if(!pInObjs)
    {
      FlyMakeErrMem();
      fWorked = FALSE;
    }
  }

  // create list of input objs for linking, e.g. "out/tool.o out/tool2.o "
  if(fWorked)
  {
    for(i = 0; i < pTool->nSrcFiles; ++i)
    {
      szObj = FmkGetOutName(szOutFolder, pTool->aszSrcFiles[i]);
      if(!szObj)
      {
        FlyMakeErrMem();
        fWorked = FALSE;
        break;
      }
      FlyStrSmartCat(pInObjs, szObj);
      FlyStrSmartCat(pInObjs, " ");
      FlyFree(szObj);
    }
  }

  // create output name for tool, e.g. "test/test_foo"
  if(fWorked)
  {
    pToolOut = FlyStrSmartAlloc(strlen(pTool->aszSrcFiles[0]) + strlen(pTool->szName) + 1);
    if(!pToolOut)
    {
      FlyMakeErrMem();
      fWorked = FALSE;
    }
    else
    {
      FlyStrSmartCpy(pToolOut, pTool->aszSrcFiles[0]);
      FlyStrPathOnly(pToolOut->sz);
      FlyStrSmartCat(pToolOut, pTool->szName);
      if(!FlyFileExistsFile(pToolOut->sz))
      {
        ++nCompiled;
        ++pState->nCompiled;
      }
    }
  }

  // if we need to link the tool, do it
  if(fWorked && (nCompiled || pState->opts.fRebuild))
  {
    // make sure we can get the memory
    pCmdline = FlyStrSmartAlloc(PATH_MAX);
    if(!pCmdline)
    {
      FlyMakeErrMem();
      fWorked = FALSE;
    }
    else
    {
      // create output name for tool, e.g. "test/test_foo"
      FlyStrSmartCpy(pToolOut, pTool->aszSrcFiles[0]);
      FlyStrPathOnly(pToolOut->sz);
      FlyStrSmartCat(pToolOut, pTool->szName);

      szDebug = pState->opts.dbg ? pCompiler->szLlDbg : "";

      // convert from {markers} into the command-line for link
      if(!FlyMakeCompilerFmtLink(pCmdline, pCompiler, pInObjs->sz, pState->libs.sz,
                            szDebug, pToolOut->sz))
      {
        FlyMakeErrMem();
        fWorked = FALSE;
      }

      if(fWorked)
      {
        ret = FlyMakeSystem(FMK_VERBOSE_SOME, &pState->opts, pCmdline->sz);
        if(ret != 0)
          FlyMakePrintf("# failed to create %s\n\n", pTool->szName);
        else
          FlyMakePrintf("# created program %s\n\n", pTool->szName);
      }
      FlyStrSmartFree(pCmdline);
    }
  }

  // everything was already up to date (nothing to compile or link)
  else if(fWorked && nCompiled == 0 && !pState->opts.fRebuild)
    ret = 1;

  // cleanup
  FlyStrSmartFree(pToolOut);
  FlyStrSmartFree(pInObjs);

  // some kind of problem (e.g. system didn't compile or memory issue)
  if(!fWorked)
    ret = -1;

  return ret;
}

/*-------------------------------------------------------------------------------------------------
  Build lib/ or any folder under lib rules. Folder must exist and have at least 1 source file.

  1. Compile each file with `-I. -I../inc -Wall -Werror lib/file.c -o lib/out`
  2. Create library using `ar -crs libname.a lib/out/ *.o`

  @param  pState    state of flymake
  @param  szFolder  folder to build under lib/ rules, e.g. lib/ or ../myfolder/
  @return TRUE if worked, FALSE if failed
*///-----------------------------------------------------------------------------------------------
bool_t FlyMakeBuildLib(flyMakeState_t *pState, const char *szFolder)
{
  static const char   szObjs[]        = "*.o";
  char               *pszLibName      = NULL;
  flyStrSmart_t      *pCmdline        = NULL;
  flyStrSmart_t      *pObjs           = NULL;
  unsigned            nFilesCompiled  = 0;
  bool_t              fWorked;

  // compile any files in the folder than need compiling
  if(FlyMakeDebug() >= FMK_DEBUG_MORE)
    FmkBanner(FMK_VERBOSE_NONE, szFolder, "Lib Rules");
  if(FlyMakeDebug())
    FlyMakePrintf("FlyMakeBuildLib(fAll %u, fRebuild %u, %s)\n", pState->opts.fAll, pState->opts.fRebuild, szFolder);

  // must have 2 replacement strings for libname and objs
  FlyAssert(FlyStrCount(g_szFmtArchive, "%s") == 2);

  // compile the files in the lib folder
  fWorked = FmkCompileFolder(pState, szFolder, &nFilesCompiled, NULL);

  if(fWorked)
  {
    pszLibName = FlyMakeFolderAllocLibName(pState, szFolder);
    if(pszLibName == NULL)
    {
      FlyMakeErrMem();
      fWorked = FALSE;
    }
    else if(!FlyFileExistsFile(pszLibName))
      ++nFilesCompiled;
  }

  // archive the file into a static library, e.g. "lib/myproj.a"
  if(fWorked && nFilesCompiled)
  {
    pState->fLibCompiled = TRUE;
    if(fWorked)
    {
      pCmdline = FlyStrSmartNewEx("", strlen(g_szFmtArchive) + strlen(pszLibName) + sizeof(m_szOutFolder) + sizeof(szObjs));
      if(pCmdline == NULL)
        fWorked = FALSE;
    }
    if(fWorked)
    {
      pObjs = FlyStrSmartNewEx(szFolder, strlen(szFolder) + sizeof(m_szOutFiles) + 16);
      if(pObjs == NULL)
        fWorked = FALSE;
      else
        FmkSmartPathCat(pObjs, m_szOutFiles); // e.g. "lib/out/*.o"
    }
    if(fWorked)
    {
      // e.g. "ar -crs projname.a lib/out/*.o:"
      // e.g. "ar -crs ../somefolder.a ../somefolder/out/*.o"
      FlyStrSmartSprintf(pCmdline, g_szFmtArchive, pszLibName, pObjs->sz);
      fWorked = FlyMakeSystem(FMK_VERBOSE_SOME, &pState->opts, pCmdline->sz) == 0 ? TRUE : FALSE;
      if(!fWorked)
        FlyMakePrintfEx(FMK_VERBOSE_SOME, "# failed to create %s\n\n", pszLibName);
      else
        FlyMakePrintfEx(FMK_VERBOSE_SOME, "# created library %s\n\n", pszLibName);

      if(pszLibName)
        FlyFree(pszLibName);
    }
  }

  return fWorked;
}

/*-------------------------------------------------------------------------------------------------
  Build src/ folder or any folder under src rules

  1. Compile each file with `-I. -Iinc/ -Wall -Werror` or user set cmdline
  2. Optional `-DDEBUG=1`
  3. link with static library, e.g. lib/projname.a and any dependency libraries

  @param  pState    state of flymake
  @param  szFolder  folder to build under src/ rules
  @return TRUE if worked, FALSE if failed
*///-----------------------------------------------------------------------------------------------
static bool_t FlyMakeBuildSrc(flyMakeState_t *pState, const char *szFolder)
{
  const flyMakeCompiler_t *pCompiler;
  char           *szTarget        = NULL;
  flyStrSmart_t  *pCmdline        = NULL;
  flyStrSmart_t  *pInFiles        = NULL;
  char           *szDebug;
  char            szExt[FMK_SZ_EXT_MAX];
  unsigned        nFilesCompiled  = 0;
  bool_t          fWorked;
  size_t          sizeIn;

  if(FlyMakeDebug() >= FMK_DEBUG_MORE)
    FmkBanner(FMK_VERBOSE_NONE, szFolder, "Src Rules");
  if(FlyMakeDebug())
    FlyMakePrintf("FlyMakeBuildSrc(fAll %u, fRebuild %u, %s)\n", pState->opts.fAll, pState->opts.fRebuild, szFolder);

  // compile the folder
  fWorked = FmkCompileFolder(pState, szFolder, &nFilesCompiled, szExt);
  if(pState->fLibCompiled)
    ++nFilesCompiled;

  // get target name, e.g. "src/foo"
  // note: szExt is empty if no source code in folder
  if(fWorked && *szExt)
  {
    szTarget = FlyMakeFolderAllocSrcName(pState, szFolder);
    if(!szTarget)
    {
      FlyMakeErrMem();
      fWorked = FALSE;
    }
    if(!FlyFileExistsFile(szTarget))
    {
      ++nFilesCompiled;
      ++pState->nCompiled;
    }
  }

  // no need to link if no new obj files
  if(fWorked && *szExt && (nFilesCompiled || pState->opts.fRebuild))
  {
    // get the compiler cmdline for this source file
    pCompiler = FlyMakeCompilerFind(pState->pCompilerList, szExt);
    FlyAssert(pCompiler && pCompiler->szLl);

    sizeIn    = strlen(szFolder) + sizeof(m_szOutFiles) + 1;
    pInFiles  = FlyStrSmartAlloc(sizeIn);
    pCmdline  = FlyStrSmartAlloc(strlen(pCompiler->szLl) + sizeIn + strlen(pState->libs.sz) +
                                 strlen(pCompiler->szLlDbg) + strlen(szTarget) + 1);
    if(!pCmdline || !pInFiles)
    {
      FlyMakeErrMem();
      fWorked = FALSE;
    }
    else
    {
      // e.g. "src/out/*.o
      FlyStrSmartCpy(pInFiles, szFolder);
      FmkSmartPathCat(pInFiles, m_szOutFiles);

      szDebug = pState->opts.dbg ? pCompiler->szLlDbg : "";

      // create link command-line from {markers}
      // e.g. cc src/out/*.o lib/projname.a -DDEBUG=1 -o src/projname
      if(!FlyMakeCompilerFmtLink(pCmdline, pCompiler, pInFiles->sz, pState->libs.sz, szDebug, szTarget))
      {
        FlyMakeErrMem();
        fWorked = FALSE;
      }
      else
      {
        // link the files/lib and create target
        if((FlyMakeSystem(FMK_VERBOSE_SOME, &pState->opts, pCmdline->sz) != 0))
          fWorked = FALSE;
        if(!fWorked)
          FlyMakePrintfEx(FMK_VERBOSE_SOME, "# failed to create %s\n\n", szTarget);
        else
          FlyMakePrintfEx(FMK_VERBOSE_SOME, "# created program %s\n\n", szTarget);
      }
    }

    FlyStrSmartFree(pCmdline);
  }

  FlyStrFreeIf(szTarget);

  return fWorked;
}

/*-------------------------------------------------------------------------------------------------
  Build the target using "tools" rules.

  1. Look for basepairs in the tools folder to build
    1a. If target is non NULL, then just build that one target (not all targets in folder)
  2. Basepairs are file.c file.h. Will also build/link file2.c, fileme.c, etc...
  3. Standalone targets are file.c with no file.h other files
  4. Links with any libraries and/or dependency libraries (pState->szDepLibs)

  @param  pState    state of flymake
  @param  szFolder  folder containing target, e.g. src/ or ../myfolder/
  @param  szTarget  a specific tool name or NULL if building all tools in folder
  @return TRUE if worked, FALSE if failed
*///-----------------------------------------------------------------------------------------------
static bool_t FlyMakeBuildTools(flyMakeState_t *pState, const char *szFolder, const char *szTarget)
{
  fmkToolList_t   *pToolList;
  char           *szOutFolder     = NULL;
  unsigned        size;
  unsigned        i;
  unsigned        nToolsCompiled  = 0;
  int             ret             = 0;
  bool_t          fFound;

  // debugging
  FlyMakeDbgPrintf(FMK_DEBUG_SOME, "FlyMakeBuildTools(szFolder %s, szTarget %s) fAll %u, fRebuild %u\n",
                  FlyStrNullOk(szFolder), FlyStrNullOk(szTarget), pState->opts.fAll, pState->opts.fRebuild);

  // get the list of tools and src files for those tools
  // if the folder is invalid, then no tool list is created
  pToolList = FlyMakeToolListNew(pState->pCompilerList, szFolder);
  if(!pToolList)
    ret = -1;
  else
  {
    if(pToolList->nTools == 0)
      FlyMakePrintfEx(FMK_VERBOSE_SOME, "# folder '%s' contains no source files\n", szFolder);
    else
      FmkBanner(FMK_VERBOSE_MORE, szTarget ? szTarget : szFolder, "Tool Rules");

    if(FlyMakeDebug() >= FMK_DEBUG_MORE)
      FlyMakeToolListPrint(pToolList);
  }

  // get a string for creating paths, can create both "folder/prog_file" or "folder/out/"
  if(ret >= 0)
  {
    size = strlen(m_szOutFolder);
    if(szTarget && strlen(szTarget) > size)
      size = strlen(szTarget);
    size += strlen(szFolder) + 3;
    szOutFolder = FlyAlloc(size);
    if(!szOutFolder)
      ret = -1;
  }

  // indicate bad target program if not found in tool list
  if(ret >= 0 && szTarget)
  {
    fFound = FALSE;
    for(i = 0; i < pToolList->nTools; ++i)
    {
      if(strcmp(szTarget, pToolList->apTools[i]->szName) == 0)
      {
        fFound = TRUE;
        break;
      }
    }
    if(!fFound)
    {
      FlyAssert(size && szOutFolder && szTarget);
      FlyStrZCpy(szOutFolder, szFolder, size);
      FlyStrPathAppend(szOutFolder, szTarget, size);
      FlyMakePrintErr(FMK_ERR_BAD_PROG, szOutFolder);
      ret = -1;
    }
  }

  // make out folder, e.g. "tools/out/", if needed
  if(ret >= 0 && pToolList->nTools)
  {
    FlyAssert(szFolder);
    FlyStrZCpy(szOutFolder, szFolder, size);
    FlyStrPathAppend(szOutFolder, m_szOutFolder, size);
    if(!FlyMakeFolderCreate(&pState->opts, szOutFolder))
      ret = -1;
  }

  if(ret >= 0 && pToolList->nTools)
  {
    for(i = 0; i < pToolList->nTools; ++i)
    {
      if(szTarget == NULL || strcmp(szTarget, pToolList->apTools[i]->szName) == 0)
      {
        ret = FmkToolCompile(pState, szOutFolder, pToolList->apTools[i]);
        if(ret < 0)
          break;
        if(ret == 0)
          ++nToolsCompiled;
      }
    }

    // if no tools needed compiling, then folder was up to date
    if(ret >= 0 && pToolList->nTools && !nToolsCompiled)
      FlyMakePrintfEx(FMK_VERBOSE_MORE, "# %s folder up to date\n", szFolder);
  }

  // cleanup
  if(szOutFolder)
    FlyFree(szOutFolder);
  if(pToolList)
    FlyMakeToolListFree(pToolList);

  return ret >= 0 ? TRUE : FALSE;
}

/*-------------------------------------------------------------------------------------------------
  Free a single dependency. Does not remove from any list.

  @param    pDep
  @return   NULL
*///-----------------------------------------------------------------------------------------------
static void * FmkDepFree(flyMakeDep_t *pDep)
{
  FlyStrFreeIf(pDep->szName);
  FlyStrFreeIf(pDep->szVer);
  FlyStrFreeIf(pDep->szRange);
  FlyStrSmartUnInit(&pDep->libs);
  FlyStrFreeIf(pDep->szIncFolder);
  if(pDep->pState)
    FlyMakeStateFree(pDep->pState);

  memset(pDep, 0, sizeof(*pDep));
  FlyFree(pDep);
  return NULL;
}

/*-------------------------------------------------------------------------------------------------
  Free the entire dependency state chain. Does not delete any files, just frees memory.

  @param    pDepList    dependency list
  @return   none
*///-----------------------------------------------------------------------------------------------
void FlyMakeDepListFree(flyMakeDep_t *pDepList)
{
  flyMakeDep_t  *pDep;
  flyMakeDep_t  *pDepNext;

  pDep = pDepList;
  while(pDep)
  {
    pDepNext = pDep->pNext;
    if(pDep->pState)
      FlyMakeStateFree(pDep->pState);
    FmkDepFree(pDep);
    pDep = pDepNext;
  }
}

/*-------------------------------------------------------------------------------------------------
  Allocate a version. If the TOML string is NULL, then use "*" for version.

  @param    szTomlStr   ptr to a TOML string, e.g. "1.2.3", or NULL
  @return   none
*///-----------------------------------------------------------------------------------------------
static char * FmkTomlVerAlloc(const char *szTomlStr)
{
  char     *szVersion;

  szVersion = szTomlStr ? FlyMakeTomlStrAlloc(szTomlStr) : FlyStrClone("*");

  return szVersion;
}

/*-------------------------------------------------------------------------------------------------
  Peek at a character in the TOML string.

  @param  szTomlStr   a TOML string
  @param  i           index into character
  @return 1st char of TOML string or '\0' if not a TOML string
*///-----------------------------------------------------------------------------------------------
static char FmkTomlPeek(const char *szTomlStr)
{
  char  c = 0;
  if(*szTomlStr == '"' || *szTomlStr == '\'')
    c = szTomlStr[1];
  return c;
}

/*-------------------------------------------------------------------------------------------------
  Create a TOML path from the root path and the TOML path string

  @param  szRoot        root folder, e.g. "", "../" or "/Users/me/work/git/my_project/"
  @param  szTomlPath    TOML string for path or file, e.g. "../my_package"
  @return ptr to a string containing the combined path
*///-----------------------------------------------------------------------------------------------
static char * FmkTomlPath(const char *szRoot, const char *szTomlPath)
{
  char       *pszPath;
  unsigned    size = 0;
  unsigned    len;
  bool_t      fRelativePath = FALSE;

  if(!(isslash(FmkTomlPeek(szTomlPath)) || (FmkTomlPeek(szTomlPath) == '~')))
  {
    fRelativePath = TRUE;
    size = strlen(szRoot);
  }
  size += FlyTomlStrLen(szTomlPath) + 1;

  pszPath = FlyAlloc(size);
  if(pszPath)
  {
    len = 0;
    if(fRelativePath)
    {
      strcpy(pszPath, szRoot);
      len = strlen(szRoot);
    }
    FlyTomlStrCpy(&pszPath[len], szTomlPath, size - len);
  }

  return pszPath;
}

/*-------------------------------------------------------------------------------------------------
  Allocate a new dependency. Does NOT add the dependency to any list.

  Essentially initializes to 0 and fills in name and version range (pDep->szRange). No other
  fields are filled in.

  If szRange is NULL, use default version range "*" (any).

  @param    szName    ptr to name of dependency package
  @param    szRange   NULL or ptr to TOML version string, e.g. "1.2.3"
  @return   ptr to flyMakeDep_t or NULL
*///-----------------------------------------------------------------------------------------------
static flyMakeDep_t * FmkDepNew(const char *szName, const char *szRange)
{
  flyMakeDep_t *pDep;

  pDep = FlyAlloc(sizeof(*pDep));
  if(pDep)
  {
    // initialize dependecy
    memset(pDep, 0, sizeof(*pDep));

    // allocate version range string
    if(szRange == NULL)
      pDep->szRange = FlyStrClone("*");
    else
      pDep->szRange = FlyMakeTomlStrAlloc(szRange);

    // allocate name string 
    pDep->szName = FlyStrClone(szName);

    if(!pDep->szName || !pDep->szRange)
      pDep = FmkDepFree(pDep);
  }

  return pDep;
}

/*-------------------------------------------------------------------------------------------------
  Find the dependency by name in a list, case sensitive.

  @param  pDepList    a list of dependencies
  @param  szName      the name to find
  @return ptr to dependency if found, NULL if not found
*///-----------------------------------------------------------------------------------------------
static flyMakeDep_t * FmkDepFind(const flyMakeDep_t *pDepList, const char *szName)
{
  const flyMakeDep_t   *pDep;

  pDep = pDepList;
  while(pDep)
  {
    if(strcmp(szName, pDep->szName) == 0)
      break;
    pDep = pDep->pNext;
  }

  return (flyMakeDep_t *)pDep;
}

/*-------------------------------------------------------------------------------------------------
  Find the dependency by TOML key in the dependency list

  @param  pDepList    a list of dependencies
  @param  szTomlKey   ptr to a TOML key that may be a dependency
  @return ptr to dependency if found, NULL if not found
*///-----------------------------------------------------------------------------------------------
static flyMakeDep_t * FmkDepTomlFind(const flyMakeDep_t *pDepList, const char *szTomlKey)
{
  const flyMakeDep_t   *pDep  = NULL;
  char                 *szName;
  unsigned              size;

  size = FlyTomlKeyLen(szTomlKey) + 1;
  szName = FlyAllocZ(size);
  if(szName)
  {
    FlyTomlKeyCpy(szName, szTomlKey, size);
    pDep = FmkDepFind(pDepList, szName);
    FlyFree(szName);
  }

  return (flyMakeDep_t *)pDep;
}

/*-------------------------------------------------------------------------------------------------
  Adds include folder and library file to appropriate states.

  1. If not NULL, adds the inc folder to the state who's flymake.toml file is being processed.
  2. If not NULL, adds the library file to the root folder.

  @param  pDepKeys      contains both root and state which is processing flymake.toml
  @param  szIncFolder   include folder string, e.g. "../packages/foo/inc/" or NULL
  @param  szLibFile     library file string, e.g. "../packages/foo/lib/foo.a" or NULL
  @return none
*///-----------------------------------------------------------------------------------------------
static void FmkDepAddIncLibs(fmkDepKeys_t *pDepKeys, const char *szIncFolder, flyStrSmart_t *pLibs)
{
  // add dependency inc folder (.e.g. dep/foo/inc/) to root state so it can compile properly
  // no need to add current folder -I. as that's already added to every project
  if(szIncFolder)
  {
    FlyStrSmartCat(&pDepKeys->pState->incs, szIncFolder);
    FlyStrSmartCat(&pDepKeys->pState->incs, " ");
  }

  // add library to root state
  if(pLibs)
  {
    FlyStrSmartCat(&pDepKeys->pRootState->libs, pLibs->sz);
    FlyStrSmartCat(&pDepKeys->pRootState->libs, " ");
  }
}

/*-------------------------------------------------------------------------------------------------
  Returns # of dependencies in this TOML file

  @param  szTomlFile    flymake.toml, loaded into memory
  @return 0-n, number of dependencies
*///-----------------------------------------------------------------------------------------------
static unsigned FmkDepNumDependencies(const char *szTomlFile)
{
  const char   *psz;
  tomlKey_t     key;
  unsigned      nDeps = 0;

  if(szTomlFile)
  {
    psz = FlyTomlTableFind(szTomlFile, m_szDepTable);
    while(psz)
    {
      psz = FlyTomlKeyIter(psz, &key);
      if(psz)
        ++nDeps;
    }
  }

  return nDeps;
}

/*-------------------------------------------------------------------------------------------------
  Checks version of depenency vs desired version range. If dep not found, always passes

  @param  pDepKeys    Information needed to process dependency
  @param  szDepName   name of dependency, e.g. "foo"
  @param  szRange     Version range, e.g. "1.2", which means >= 1.2.0 and <= 2.0.0
  @param  ppDep       Returned dependency or NULL if not found
  @return FMK_ERR_NONE or FMK_ERR_CUSTOM
*///-----------------------------------------------------------------------------------------------
static fmkErr_t FmkDepVersionValidate(
  fmkDepKeys_t *pDepKeys,
  const char *szDepName,
  const char *szRange,
  flyMakeDep_t **ppDep)
{
  flyMakeDep_t   *pDep = NULL;
  fmkErr_t        err = FMK_ERR_NONE;

  // check that version of package doesn't conflict with specified version range
  pDep = FmkDepFind(pDepKeys->pRootState->pDepList, szDepName);
  if(pDep && !FlySemVerMatch(szRange, pDep->szVer))
  {
    err = FlyMakeErrToml(pDepKeys->pState, pDepKeys->keyInc.szValue, "version conflict");
    FlyMakePrintf("  Previous version %s\n", pDep->szVer);
  }
  *ppDep = pDep;

  return err;
}

/*-------------------------------------------------------------------------------------------------
  Creates a valid flyMakeState_t upon success. Fails if folder does not point to a valid package.

  Also returns the newly created state so that package can be built with flymake.

  Tasks:

  1. Determines root of project, inc/ and lib/ folders
  2. Reads flymake.toml and determines version based on that
  3. Sets build option to --rl (library rules)

  Possible errors:

  1. folder not a project (that is, contains no source files)
  2. invalid flymake.toml file
  3. project cannot be built as object file library (package)

  @param  pDepKeys    All the information needed to proce
  @param  szFolder    file folder, e.g. "../foo/" or "deps/bar/"
  @param  szDepName   
  @param  ppState     Returned flyMakeState_t
  @return FMK_ERR_NONE, FMK_ERR_MEM, FMK_ERR_CUSTOM
*///-----------------------------------------------------------------------------------------------
static fmkErr_t FmkDepPackageValidate(
  fmkDepKeys_t *pDepKeys,
  const char *szFolder,
  const char *szDepName,
  const char *szVer,
  flyMakeState_t **ppState)
{
  flyMakeState_t *pState        = NULL;
  fmkErr_t        err           = FMK_ERR_NONE;
  const char     *szValue       = NULL;


  FlyAssertDbg(pDepKeys && pDepKeys->pRootState);
  FlyAssertDbg(szFolder);
  FlyAssertDbg(ppState);

  // create a new empty state, cloning options
  if(!err)
  {
    pState = FlyMakeStateClone(pDepKeys->pRootState);
    if(!pState)
      err = FlyMakeErrMem();
    else
    {
      // always compile with lib rules, and don't rebuild with -B, only --all
      pState->opts.fRulesLib = TRUE;
      pState->opts.fRulesSrc = pState->opts.fRulesTools = FALSE;
      pState->opts.fRebuild = (pState->opts.fAll) ? TRUE : FALSE;
    }
  }

  // verify it's a valid root folder of a project
  if(!err && !FlyMakeTomlRootFill(pState, szFolder))
  {
    szValue = pDepKeys->keyGit.szValue ? pDepKeys->keyGit.szValue : pDepKeys->keyPath.szValue;
    err = FlyMakeErrToml(pDepKeys->pState, szValue, "folder not a project");
  }

  // validate flymake.toml and allocate things like the name
  if(!err && !FlyMakeTomlAlloc(pState, szDepName))
  {
    FlyAssert(szValue);
    err = FlyMakeErrToml(pDepKeys->pState, szValue, "invalid flymake.toml file");
  }

  // fixup project version
  if(!err && !pState->szProjVer)
  {
    if(szVer)
      pState->szProjVer = FlyStrClone(szVer);
    else
      pState->szProjVer = FlyStrClone("*");
  }

  if(!err && !FlyMakeFolderFindByRule(pState->pFolderList, FMK_RULE_LIB))
  {
    FlyMakeStatePrint(pState);
    err = FlyMakeErrToml(pDepKeys->pState, szValue, "project cannot be built as library");
  }

  // failed, free state
  if(err)
    FlyMakeStateFree(pState);
  else
    *ppState = pState;

  return err;
}

/*-------------------------------------------------------------------------------------------------
  Vallidates and adds the git or package dependency to the deplist.

  1. Checks that the folder can be built into one or more a libraries.
  3. Validates actual version of project vs range specified by dependency
  2. Adds lib to root project, adds inc/ folder to project referencing dependency

  The actual version found depends on a variety of factors:

  szRange  | szVer  | flymake.toml | resulting szProjVer
  -------- | ------ | ------------ | -------------------
  1.0      | NULL   | NULL         | *
  1.0      | 1.3    | NULL         | 1.3
  1.0      | 1.3    | 1.2          | 1.2

  @param  pDepKeys    Information needed to process dependency
  @param  szFolder    Folder to add

  @return FMK_ERR_NONE or FMK_ERR_CUSTOM
*///-----------------------------------------------------------------------------------------------
static fmkErr_t FmkDepPackageAdd(
  fmkDepKeys_t *pDepKeys,
  const char *szFolder,
  const char *szDepName,
  const char *szRange,    // version range specified by use, defaults to *
  const char *szVer,      // may be NULL, what was found in git log
  flyMakeDep_t **ppDep)
{
  flyMakeDep_t     *pDep;
  flyMakeState_t   *pState  = NULL;
  char             *szLibs;
  fmkErr_t          err     = FMK_ERR_NONE;

  // allocate a new state if this folder is a valid package
  err = FmkDepPackageValidate(pDepKeys, szFolder, szDepName, szVer, &pState);
  if(!err)
  {
    FlyAssert(pState);
  }

  // create new dependency
  if(!err)
  {
    pDep = FmkDepNew(szDepName, szRange);
    if(!pDep)
      err = FlyMakeErrMem();
    else
    {
      pDep->szVer       = FlyStrClone(pState->szProjVer);
      szLibs            = FlyStrSmartCpy(&pDep->libs, pState->libs.sz);
      pDep->szIncFolder = FlyStrClone(pState->szInc);
      pDep->pState      = pState;
      if(!pDep->szVer || !szLibs || !pDep->szIncFolder)
        err = FlyMakeErrMem();
    }
  }

  // add dependency to root state
  if(!err)
    pDepKeys->pRootState->pDepList = FlyListAppend(pDepKeys->pRootState->pDepList, pDep);

  // validate version of package vs specified version range (also fills in ppDep)
  if(!err)
    err = FmkDepVersionValidate(pDepKeys, szDepName, szRange, ppDep);

  // add include/ folder to current state and and library/file.a to root state
  if(!err)
    FmkDepAddIncLibs(pDepKeys, pState->szInc, &pState->libs);

  if(err)
    *ppDep = NULL;

  return err;
}

/*-------------------------------------------------------------------------------------------------
  Look in this line for semantic version, e.g. v1.2.3. or version 1 or ver 2.0.

  @param  szLine    a line of text
  @param  lineLen   length of line
  @return ptr to allocated string if semver found, otherwise NULL
*///-----------------------------------------------------------------------------------------------
char * FmkDepVerFindInLine(const char *szLine, unsigned lineLen)
{
  static const char  *aVerStrs[] = { "version", "ver", "v"};
  char               *szSemVer = NULL;
  unsigned            i;
  unsigned            n;
  bool_t              fFound = FALSE;

  while(lineLen && !fFound)
  {
    if(toupper(*szLine) == 'V')
    {
      for(i = 0; i < NumElements(aVerStrs); ++i)
      {
        n = strlen(aVerStrs[i]);
        if(strncasecmp(szLine, aVerStrs[i], n) == 0)
        {
          szLine = FlyStrSkipWhite(szLine + n);
          n = FlySemVerCpy(NULL, szLine, lineLen);
          if(n != 0)
          {
            szSemVer = FlyAlloc(n + 1);
            FlySemVerCpy(szSemVer, szLine, n + 1);
            fFound = TRUE;
            break;
          }
        }
      }
    }
    ++szLine;
    --lineLen;
  }

  return szSemVer;
}

/*-------------------------------------------------------------------------------------------------
  Given a version range, look in the git log for find a SHA that matches.

  For example, if version range is "1", then it will look for versions >= 1.0.0 and < 2.0.0.

  @param  pOpts     So system calls are printed properly
  @param  szRange   The version range to find
  @return allocated SHA or NULL if version found
*///-----------------------------------------------------------------------------------------------
static char * FmkDepVersionFind(flyMakeOpts_t *pOpts, const char *szRange, char **ppszSha)
{
  static const char   szMakeGitLog[]    = "git log --oneline >log.tmp";
  static const char   szRemoveGitLog[]  = "rm -f log.tmp";
  static const char   szLogFileName[]   = "log.tmp";
  const char         *szLine;
  char               *szTmpFile;
  char               *szSemVer          = NULL;
  char               *szSha             = NULL;
  unsigned            size;

  FlyMakeSystem(FMK_VERBOSE_MORE, pOpts, szMakeGitLog);
  szTmpFile = FlyFileRead(szLogFileName);
  if(szTmpFile)
  {
    szLine = szTmpFile;
    while(*szLine)
    {
      szSemVer = FmkDepVerFindInLine(szLine, (unsigned)FlyStrLineLen(szLine));
      if(szSemVer)
      {
        // debugging
        if(FlyMakeDebug() >= FMK_DEBUG_MORE)
          FlyMakePrintf("dbg: found szSemVer '%s' in line '%.*s'\n", szSemVer, (unsigned)FlyStrLineLen(szLine), szLine);

        // e.g. cba1855 fixes #271 v1.2.1 Added SemVer
        if(!FlySemVerMatch(szRange, szSemVer) || !isxdigit(*szLine))
          FlyFree(szSemVer);
        else
        {
          size = FlyStrArgLen(szLine) + 1;
          szSha = FlyAlloc(size);
          if(szSha)
          {
            strncpy(szSha, szLine, size - 1);
            szSha[size - 1] = '\0';
            // debugging
            if(FlyMakeDebug() >= FMK_DEBUG_MORE)
              FlyMakePrintf("dbg: found sha '%s'\n", szSha);
          }
          break;
        }
      }
      szLine = FlyStrLineNext(szLine);
    }
  }
  FlyStrFreeIf(szTmpFile);
  FlyMakeSystem(FMK_VERBOSE_MORE, pOpts, szRemoveGitLog);

  // return both SHA and found version
  *ppszSha = szSha;
  return szSemVer;
}

/*-------------------------------------------------------------------------------------------------
  Checkout the given sha

  @param  pOpts   Options for displaying system commands/errors.
  @param  szSha   short SHA for checkout, e.g. "4cb16b7"
  @return allocated SHA or NULL if version found
*///-----------------------------------------------------------------------------------------------
bool_t FmkDepCheckoutSha(flyMakeOpts_t *pOpts, const char *szSha)
{
  static const char szGitCheckout[] = "git checkout -q ";
  char     *szCmdline;
  unsigned  size;
  bool_t    fWorked = FALSE;

  size = sizeof(szGitCheckout) + strlen(szSha);
  szCmdline = FlyAlloc(size);
  if(szCmdline)
  {
    FlyStrZCpy(szCmdline, szGitCheckout, size);
    FlyStrZCat(szCmdline, szSha, size);
    if(FlyMakeSystem(FMK_VERBOSE_MORE, pOpts, szCmdline) == 0)
      fWorked = TRUE;
  }

  return fWorked;
}

/*-------------------------------------------------------------------------------------------------
  Has this szDepName already been cloned? Checks for "deps/<depname>/.git/" folder.

  @param  szDepDir        dep folder, e.g. "deps/" or "../deps/"
  @param  szDepName       dependency name, e.g. "foo"
  @return TRUE if already cloned, FALSE if not
*///-----------------------------------------------------------------------------------------------
static bool_t FmkDepPackageAlreadyCloned(const char *szDepDir, const char *szDepName)
{
  const char szGitFolder[] = ".git/";
  unsigned    size;
  char       *szPath;
  bool_t      fExists = FALSE;

  // check if .git folder exists already, e.g. "deps/foo/.git/"
  size = strlen(szDepDir) + strlen(szDepName) + sizeof(szGitFolder) + 4;
  szPath = FlyAlloc(size);
  if(szPath)
  {
    FlyStrZCpy(szPath, szDepDir, size);
    FlyStrZCat(szPath, szDepName, size);
    FlyStrZCat(szPath, "/", size);
    FlyStrZCat(szPath, szGitFolder, size);
    fExists = FlyFileExistsFolder(szPath);
  }

  return fExists;
}

/*-------------------------------------------------------------------------------------------------
  Clone a project given a URL into the deps/<depname>/ folder.

  Uses the optional `version=`, `brannch=` and `sha=` flags. 

  @param  pDepKeys        Information needed clone
  @param  szDepName       dependency name, e.g. "foo"
  @param  szRange         version range, e.g. "1.2" means >=1.2 and <= 2.0
  @param  szGitUrl        URL to git, e.g. "git@gitlab.com:drew.gislason/foo.git"
  @param  szFolder        folder where to clone the git repo, e.g. "deps/foo/"
  @param  ppszVer         return value, version found
  @return FMK_ERR_NONE or FMK_ERR_CUSTOM
*///-----------------------------------------------------------------------------------------------
static fmkErr_t FmkDepPackageClone(
  fmkDepKeys_t *pDepKeys, 
  const char   *szDepName, 
  const char   *szGitUrl,
  char         *szFolder,
  char        **ppszVer)
{
  flyStrSmart_t   cmdline;
  char           *szBranch    = NULL;   // e.g. branch="main"
  char           *szRange     = NULL;   // e.g. range="1.2", means >= 1.2.0 and < 2.0.0
  char           *szSha       = NULL;   // e.g. sha="5e925d2" or "615619802b2c0b4105eabf516f05f3ad199ef8c9"
  char           *szOrgDir    = NULL;
  char           *szClonePath = NULL;
  char           *szVer       = NULL;   // found version in git log
  fmkErr_t        err         = FMK_ERR_NONE;

  // git clone url [-b branch] folder/
  // git log --oneline >tmp.log
  // git checkout sha
  // git checkout branch

  FlyAssertDbg(pDepKeys && pDepKeys->pRootState);

  // determine path to clone into, e.g. deps/foo
  FlyStrSmartInit(&cmdline);
  szClonePath = FlyStrClone(szFolder);
  if(!szClonePath)
    err = FlyMakeErrMem();

  // get a buffer for cloning system commands
  if(!err && !FlyStrSmartInitEx(&cmdline, PATH_MAX))
    err = FlyMakeErrMem();

  // allocat strings
  szBranch = FlyMakeTomlStrAlloc(pDepKeys->keyBranch.szValue);
  szSha = FlyMakeTomlStrAlloc(pDepKeys->keySha.szValue);
  szRange = FlyMakeTomlStrAlloc(pDepKeys->keyVer.szValue);

  // nothing to do if already checked out (hidden .git folder present)
  if(!err)
  {
    // clone into the dep folder, e.g. "deps/foo/"
    FlyMakeFolderRemove(FMK_VERBOSE_MORE, &pDepKeys->pRootState->opts, szClonePath);
    FlyMakePrintfEx(FMK_VERBOSE_SOME, "# Cloning %s into %s\n", szGitUrl, szClonePath);

    // clone the project
    FlyStrSmartCpy(&cmdline, "git clone -q ");
    FlyStrSmartCat(&cmdline, szGitUrl);
    FlyStrSmartCat(&cmdline, szBranch ? " -b " : " ");
    if(szBranch)
      FlyStrSmartCat(&cmdline, szBranch);
    FlyStrSmartCat(&cmdline, szClonePath);
    if(FlyMakeSystem(FMK_VERBOSE_MORE, &pDepKeys->pRootState->opts, cmdline.sz) != 0)
    {
      FlyMakePrintf("error: cannot clone '%s'. Check URL or git permissions.\n", szGitUrl);
      err = FMK_ERR_CUSTOM;
    }

    // don't specify both version and sha
    if(szRange && szSha)
    {
      err = FlyMakeErrToml(pDepKeys->pState, pDepKeys->keyVer.szValue, "cannot specify both version and sha");
    }

    // user has specified version range or SHA. Find them.
    if(!err && (szRange || szSha))
    {
      FlyFileGetCwd(cmdline.sz, cmdline.size);
      szOrgDir = FlyStrClone(cmdline.sz);
      if(!szOrgDir)
        err = FlyMakeErrMem();
      else
      {
        // change to deps/depname/ folder
        FlyFileChangeDir(szClonePath);

        // find the Git SHA of the specific version
        if(szRange && !szSha)
        {
          szVer = FmkDepVersionFind(&pDepKeys->pRootState->opts, szRange, &szSha);
          if(!szSha)
          {
            err = FlyMakeErrToml(pDepKeys->pState, pDepKeys->keyVer.szValue, "version not found");
          }
        }

        // have a SHA, use it
        if(szSha && !FmkDepCheckoutSha(&pDepKeys->pRootState->opts, szSha))
        {
          err = FlyMakeErrToml(pDepKeys->pState, pDepKeys->keySha.szValue, "SHA not found");
        }

        // back to our original folder
        FlyFileChangeDir(szOrgDir);
      }
    }
  }

  // return found version and clone path
  if(!err)
    *ppszVer = szVer;

  // cleaup, but do not delete szVer or szClonePath as they are return values
  FlyStrFreeIf(szOrgDir);
  FlyStrFreeIf(szSha);
  FlyStrFreeIf(szBranch);
  FlyStrSmartUnInit(&cmdline);

  return err;
}

/*-------------------------------------------------------------------------------------------------
  Process prebuilt dependency.

  This type of dependency is not built by flymake, but is simply included as-is.

  Requires `path=` key to point to a valid `lib.a` file and `inc=` to an include `folder/`.

  @param  pDepKeys      Information needed to process dependency
  @return FMK_ERR_NONE or FMK_ERR_CUSTOM
*///-----------------------------------------------------------------------------------------------
static fmkErr_t FmkDepProcessPrebuilt(fmkDepKeys_t *pDepKeys)
{
  flyMakeDep_t   *pDep        = NULL;
  char           *szLibFile   = NULL;
  char           *szIncFolder = NULL;
  char           *szDepName   = NULL;
  flyStrSmart_t  *pLibs       = NULL;
  fmkErr_t        err         = FMK_ERR_NONE;

  // should never get here without dep = { path="../some/folder/lib.a", inc="../some/folder/inc/" }
  FlyAssert(pDepKeys && pDepKeys->keyDep.szKey);
  FlyAssert(pDepKeys->keyInc.szValue);
  FlyAssert(pDepKeys->keyPath.szValue);

  // allocate things we'll need
  szDepName = FlyMakeTomlKeyAlloc(pDepKeys->keyDep.szKey);
  szIncFolder = FmkTomlPath(pDepKeys->pState->szRoot, pDepKeys->keyInc.szValue);
  szLibFile = FmkTomlPath(pDepKeys->pState->szRoot, pDepKeys->keyPath.szValue);
  if(!szDepName || !szIncFolder || !szLibFile)
    err = FlyMakeErrMem();

  // print the header
  if(!err)
    FlyMakePrintfEx(FMK_VERBOSE_SOME, "# Dependency prebuilt: %s: ", szDepName);

  // make sure inc/ folder exists
  if(!err && !FlyFileExistsFolder(szIncFolder))
  {
    FlyMakePrintfEx(FMK_VERBOSE_SOME, "\n");
    err = FlyMakeErrToml(pDepKeys->pState, pDepKeys->keyInc.szValue, "include folder not found");
  }

  // path must point to valid prebuilt library file, e.g. "../project/lib/project.a"
  if(!err && !FlyFileExistsFile(szLibFile))
  {
    FlyMakePrintfEx(FMK_VERBOSE_SOME, "\n");
    err = FlyMakeErrToml(pDepKeys->pState, pDepKeys->keyPath.szValue, "library not found");
  } 

  // don't allow same dep name with different folders
  if(!err)
  {
    pDep = FmkDepFind(pDepKeys->pRootState->pDepList, szDepName);
    if(pDep)
    {
      if(!FlyFileIsSamePath(szIncFolder, pDep->szIncFolder))
      {
        FlyMakePrintfEx(FMK_VERBOSE_SOME, "\n");
        err = FlyMakeErrToml(pDepKeys->pState, pDepKeys->keyInc.szValue, "duplicate dependency, different includer folder");
        FlyMakePrintf("  previous include folder: %s\n", pDep->szIncFolder);
      }
    }
  }

  // only add dependency if it hasn't already been added (pDep will be non-NULL if previously added)
  if(!err && !pDep)
  {
    pDep = FmkDepNew(szDepName, NULL);
    if(!pDep)
      err =  FlyMakeErrMem();
    else
    {
      pDepKeys->pRootState->pDepList = FlyListAppend(pDepKeys->pRootState->pDepList, pDep);

      // add dependency library and inc folder to appropriate places
      FlyMakePrintfEx(FMK_VERBOSE_SOME, "%s\n", szLibFile);
      pLibs = FlyStrSmartNew(szLibFile);
      FmkDepAddIncLibs(pDepKeys, szIncFolder, pLibs);
    }
  }

  // cleanup
  FlyStrSmartFree(pLibs);
  FlyStrFreeIf(szLibFile);
  FlyStrFreeIf(szIncFolder);
  FlyStrFreeIf(szDepName);

  return err;
}

/*-------------------------------------------------------------------------------------------------
  Process a project style dependency.

  Requires `path=` key to point to a project folder.

  This will set up a new state so that the project can be built, and will add it to the
  flyMakeDep_t dependency root state's list.

  @param  pDepKeys      All the information needed to proce
  @return FMK_ERR_NONE or FMK_ERR_CUSTOM
*///-----------------------------------------------------------------------------------------------
static fmkErr_t FmkDepProcessPackage(fmkDepKeys_t *pDepKeys)
{
  flyMakeDep_t   *pDep       = NULL;
  char           *szDepName  = NULL;
  char           *szRange    = NULL;
  char           *szFolder   = NULL;
  fmkErr_t        err        = FMK_ERR_NONE;

  FlyAssert(pDepKeys && pDepKeys->keyDep.szKey);
  FlyAssert(pDepKeys->keyPath.szValue);

  // specified folder in path= key must exist
  szDepName = FlyMakeTomlKeyAlloc(pDepKeys->keyDep.szKey);
  szRange   = FmkTomlVerAlloc(pDepKeys->keyVer.szValue);
  szFolder  = FmkTomlPath(pDepKeys->pState->szRoot, pDepKeys->keyPath.szValue);
  if(!szRange || !szDepName || !szFolder)
    err = FlyMakeErrMem();

  // print the header
  if(!err)
    FlyMakePrintfEx(FMK_VERBOSE_SOME, "# Dependency project : %s %s: %s\n", szDepName, szRange, szFolder);

  // check for version conflict with existing dep
  if(!err)
    err = FmkDepVersionValidate(pDepKeys, szDepName, szRange, &pDep);

  // if not already in dependency list, add the new dependency to the list
  if(!err)
  {
    if(pDep)
      FmkDepAddIncLibs(pDepKeys, pDep->szIncFolder, NULL);
    else
      err = FmkDepPackageAdd(pDepKeys, szFolder, szDepName, szRange, NULL, &pDep);
  }

  // display actual version found
  if(!err)
  {
    FlyAssert(pDep);
    FlyMakePrintfEx(FMK_VERBOSE_SOME, "#     found version => %s\n", pDep->szVer);
  }

  // cleanup
  FlyStrFreeIf(szFolder);
  FlyStrFreeIf(szRange);
  FlyStrFreeIf(szDepName);

  return err;
}

/*-------------------------------------------------------------------------------------------------
  Process "git" style dependency. Requires `git=` key to specify a URL.

  Optionally, specify `version=` key to pick version.

  See 

  Requires the following (otherwise, error):

  1. URL specifies a git repository
  2. User has permissions to check out said repository
  3. Repository is a valid package (contains source to build a library and an include folder)

  @param  pDepKeys      All the information needed to proce
  @return FMK_ERR_NONE or FMK_ERR_CUSTOM
*///-----------------------------------------------------------------------------------------------
static fmkErr_t FmkDepProcessGit(fmkDepKeys_t *pDepKeys)
{
  flyMakeDep_t   *pDep        = NULL;
  char           *szFolder    = NULL;
  char           *szDepName   = NULL;
  char           *szRange     = NULL;
  char           *szGitUrl    = NULL;
  char           *szVer       = NULL;
  unsigned        size;
  fmkErr_t        err               = FMK_ERR_NONE;

  // validate some parameters
  FlyAssert(pDepKeys && pDepKeys->keyDep.szKey);
  FlyAssert(pDepKeys->keyGit.szValue);

  // specified folder in path= key must exist
  szDepName = FlyMakeTomlKeyAlloc(pDepKeys->keyDep.szKey);
  szRange   = FmkTomlVerAlloc(pDepKeys->keyVer.szValue);
  szGitUrl  = FlyMakeTomlStrAlloc(pDepKeys->keyGit.szValue);
  if(!szRange || !szDepName || !szGitUrl)
    err = FlyMakeErrMem(); 
  else
    FlyMakePrintfEx(FMK_VERBOSE_SOME, "# Dependency git     : %s %s: %s\n", szDepName, szRange, szGitUrl);

  // check if package already exists, and if so, is it in version range?
  if(!err)
    err = FmkDepVersionValidate(pDepKeys, szDepName, szRange, &pDep);

  // package not checked out, so clone it here
  if(!err)
  {
    // dependency already exists, just add to dep inc/ folder to state including that dependency
    if(pDep)
      FmkDepAddIncLibs(pDepKeys, pDep->szIncFolder, NULL);

    // add new dependency
    else
    {
      size = strlen(pDepKeys->pRootState->szDepDir) + strlen(szDepName) + 3;
      szFolder = FlyAlloc(size);
      if(!szFolder)
        err = FlyMakeErrMem();
      else
      {
        strcpy(szFolder, pDepKeys->pRootState->szDepDir);
        strcat(szFolder, szDepName);
        strcat(szFolder, "/");
      }

      // only clone if not already cloned
      if(!err && !FmkDepPackageAlreadyCloned(pDepKeys->pRootState->szDepDir, szDepName))
        err = FmkDepPackageClone(pDepKeys, szDepName, szGitUrl, szFolder, &szVer);

      // add the dependency to list
      if(!err)
        err = FmkDepPackageAdd(pDepKeys, szFolder, szDepName, szRange, szVer, &pDep);
    }
  }

  // display actual version found
  if(!err)
  {
    FlyAssert(pDep);
    FlyMakePrintfEx(FMK_VERBOSE_SOME, "#     found version => %s\n", pDep->szVer);
  }

  // cleanup
  FlyStrFreeIf(szVer);
  FlyStrFreeIf(szGitUrl);
  FlyStrFreeIf(szRange);
  FlyStrFreeIf(szDepName);

  return err;
}

/*-------------------------------------------------------------------------------------------------
  Recursively process flymake.toml `[dependencies]`. Results in pRootState->pDepList filled in.

  Also updates pRootState->libs and pState->incs.

  Broad first, then deep. That is, process all `[dependencies]` in this TOML file, then check each
  dependency for sub-dependencies and so on.

  Prints error and returns FMK_ERR_CUSTOM if there is a problem.

  @param  pRootState    root project state
  @param  pState        state of project containing flymake.toml `[dependencies]`
  @return FMK_ERR_NONE or FMK_ERR_CUSTOM
*///-----------------------------------------------------------------------------------------------
static fmkErr_t FmkDepProcessToml(flyMakeState_t *pRootState, flyMakeState_t *pState)
{
  const char     *pszDepTable     = NULL; // table of [dependencies]
  const char     *pszIter         = NULL; // iter through dependencies
  const char     *pszInlineTable;
  flyMakeDep_t   *pDep            = NULL;
  fmkDepKeys_t    depKeys;
  fmkDepKeyVal_t  aKeyVal[] =
  {
    { "git",     &depKeys.keyGit },
    { "inc",     &depKeys.keyInc },
    { "path",    &depKeys.keyPath },
    { "version", &depKeys.keyVer },
    { "sha",     &depKeys.keySha },
    { "branch",  &depKeys.keyBranch },
  };
  unsigned        i;
  fmkErr_t        err = FMK_ERR_NONE;

  // verify inputs
  FlyAssert(FlyMakeIsState(pRootState));
  FlyAssert(FlyMakeIsState(pState));
  FlyAssert(pState->szRoot && pState->szFullPath);

  FlyMakeDbgPrintf(FMK_DEBUG_SOME, "FmkDepProcessToml(%s,%s)\n", pRootState->szRoot, pState->szRoot);

  // nothing to do if no flymake.toml or no [dependencies]
  if(FmkDepNumDependencies(pState->szTomlFile) == 0)
  {
    FlyMakePrintfEx(FMK_VERBOSE_MORE, "# no dependencies in project `%s`\n", pState->szFullPath);
    return FMK_ERR_NONE;
  }

  // initialize structure used to process dependencies
  memset(&depKeys, 0, sizeof(depKeys));
  depKeys.pRootState = pRootState;
  depKeys.pState = pState;

  // process each dependency (TOML inline table)
  pszDepTable = FlyTomlTableFind(pState->szTomlFile, m_szDepTable);
  if(pszDepTable)
    pszIter = FlyTomlKeyIter(pszDepTable, &depKeys.keyDep);
  while(!err && pszIter)
  {
    // every dependency must be a TOML inline table, e.g. foo = { "path" = "../foo/" }
    if(depKeys.keyDep.type != TOML_INLINE_TABLE)
    {
      FlyMakeErrToml(pState, depKeys.keyDep.szValue, "expected inline table");
      err = FMK_ERR_CUSTOM;
      break;
    }

    // look for inline table keys we recognize in this dependency
    pszInlineTable = depKeys.keyDep.szValue;
    for(i = 0; !err && i < NumElements(aKeyVal); ++i)
    {
      if(FlyTomlKeyFind(pszInlineTable, aKeyVal[i].szKey, aKeyVal[i].pKey))
        err = FlyMakeTomlCheckString(pState, aKeyVal[i].pKey);
    }

    // print out keys found, all on one line
    if(!err && FlyMakeDebug() >= FMK_DEBUG_MORE)
    {
      for(i = 0; i < NumElements(aKeyVal); ++i)
      {
        if(i == 0)
        {
          FlyMakePrintf("%.*s = { ",
            FlyTomlKeyLen(depKeys.keyDep.szKey), depKeys.keyDep.szKey);
        }
        if(aKeyVal[i].pKey->szValue)
        {
          if(i != 0)
            FlyMakePrintf(", ");
          FmkTomlKeyPrint(aKeyVal[i].szKey, aKeyVal[i].pKey->szValue);
        }
      }
      FlyMakePrintf(" }\n");
    }

    // must have either a path= or git= key
    if(!depKeys.keyGit.szValue && !depKeys.keyPath.szValue)
      err = FlyMakeErrToml(pState, pszInlineTable, "expected \"path=\" or \"git=\" key in inline table");

    if(!err)
    {
      pDep = FmkDepTomlFind(pRootState->pDepList, depKeys.keyDep.szKey);
      if(pDep)
        FmkDepAddIncLibs(&depKeys, pDep->szIncFolder, NULL);
      else if(depKeys.keyGit.szValue)
        err = FmkDepProcessGit(&depKeys);
      else if(depKeys.keyInc.szValue && depKeys.keyPath.szValue)
        err = FmkDepProcessPrebuilt(&depKeys);
      else
        err = FmkDepProcessPackage(&depKeys);
    }

    // look for next dependency
    pszIter = FlyTomlKeyIter(pszIter, &depKeys.keyDep);
  }

  // iter through TOML file again and see if we need to recurse into any of the projects
  if(pszDepTable)
    pszIter = FlyTomlKeyIter(pszDepTable, &depKeys.keyDep);
  while(!err && pszIter)
  {
    // process only those dependencies with flymake.toml files
    pDep = FmkDepTomlFind(pRootState->pDepList, depKeys.keyDep.szKey);
    if(pDep && pDep->pState && pDep->pState->szTomlFile && FmkDepNumDependencies(pDep->pState->szTomlFile))
      err = FmkDepProcessToml(pRootState, pDep->pState);

    // look for next dependency
    pszIter = FlyTomlKeyIter(pszIter, &depKeys.keyDep);
  }

  FlyMakeDbgPrintf(FMK_DEBUG_SOME, "  err %u, root: incs \"%s\", libs \"%s\"\n", err, pRootState->incs.sz, pRootState->libs.sz);

  return err;
}

/*-------------------------------------------------------------------------------------------------
  Builds all "standard" folders in the project

  @param  pState          filled out state, e.g. pState->szRoot is filled out
  @return FMK_ERR_NONE if worked, some other FMK_ERR if failed
*///-----------------------------------------------------------------------------------------------
static fmkErr_t FmkDepBuildProject(flyMakeState_t *pState)
{
  flyMakeFolder_t  *pFolder;
  fmkErr_t          err = FMK_ERR_NONE;

  // verify dependency state has been initialized
  FlyAssert(FlyMakeIsState(pState) && pState->szRoot);
  FlyMakeDbgPrintf(FMK_DEBUG_SOME, "FmkDepBuildProject(%s)\n", pState->szRoot);

  // ignore cmdline rules for dependencies, use only specified folder rules
  pState->opts.fRulesLib = pState->opts.fRulesSrc = pState->opts.fRulesTools = FALSE;

  // build libraries first
  err = FlyMakeBuildLibs(pState);

  // build all known and existing folders in dependency project
  pFolder = pState->pFolderList;
  while(!err && pFolder)
  {
    // build each existing folder
    if(pFolder->rule == FMK_RULE_SRC && !FlyMakeBuildSrc(pState, pFolder->szFolder))
      err = FMK_ERR_CUSTOM;
    else if(pFolder->rule == FMK_RULE_TOOL && !FlyMakeBuildTools(pState, pFolder->szFolder, NULL))
      err = FMK_ERR_CUSTOM;

    pFolder = pFolder->pNext;
  }

  return err;
}

/*-------------------------------------------------------------------------------------------------
  Is this target root folder the same as the target file/folder?

  @param    pState        filled in flymake state (must have pState->szRoot)
  @param    szTargetRoot  target file/folder
  @return   TRUE if same root folder
*///-----------------------------------------------------------------------------------------------
bool_t FlyMakeIsSameRoot(flyMakeState_t *pState, const char *szTarget)
{
  char   *szTargetRoot;
  bool_t  fIsSameRoot;

  szTargetRoot = FlyMakeTomlRootFind(szTarget, pState->pCompilerList, NULL);
  fIsSameRoot = FlyMakeIsSameFolder(pState->szRoot, szTargetRoot);
  FlyFree(szTargetRoot);

  return fIsSameRoot;
}

/*-------------------------------------------------------------------------------------------------
  Discover all dependencies.

    @param  pState    root project state
  @return FMK_ERR_NONE or FMK_ERR_CUSTOM
*///-----------------------------------------------------------------------------------------------
fmkErr_t FlyMakeDepDiscover(flyMakeState_t *pRootState)
{
  fmkErr_t  err = FMK_ERR_NONE;

  // if no [dependencies], then  nothing to do
  if(FmkDepNumDependencies(pRootState->szTomlFile))
  {
    FlyMakeFolderCreate(&pRootState->opts, pRootState->szDepDir);
    err = FmkDepProcessToml(pRootState, pRootState);
  }

  return err;
}

/*-------------------------------------------------------------------------------------------------
  Discover and Build all the dependencies

  Root state must already be valid. See FlyMakeTomlRootFill() and FlyMakeTomlAlloc()

  Only happens once per run of flymake.

  Recursive (that is, discovers, validates and checks out any sub dependencies). Goes wide first,
  then deep. that is, processes all dependencies before recurssing into those dependenies with
  dependencies of their own.

  This accomplishes the following:

  1. Checks flymake.toml file for [dependencies] section. If none or empty, nothing to do
  2. Dependencies are one of three types: prebuilt, package and git
  2. Finds or checks out from Git each dependency as specified in flymake.toml
  3. Verifies version of dependency does not conflict
  4. Creates a pState for each dependency that must be built
  5. Recursively does all the above

  @param  pState    root project state
  @return FMK_ERR_NONE or FMK_ERR_CUSTOM
*///-----------------------------------------------------------------------------------------------
fmkErr_t FlyMakeDepListBuild(flyMakeState_t *pRootState)
{
  flyMakeDep_t   *pDep;
  fmkErr_t        err = FMK_ERR_NONE;

  FlyAssert(pRootState && pRootState->szDepDir);

  // if no [dependencies], then  nothing to do
  if(FmkDepNumDependencies(pRootState->szTomlFile))
  {
    // discover all dependencies, includes cloning them if needed
    FlyMakePrintfEx(FMK_VERBOSE_SOME, "\n# ---- Discovering dependencies... ----\n");
    err = FlyMakeDepDiscover(pRootState);

    // build dependencies with state
    if(!err && pRootState->pDepList)
    {
      if(FlyMakeDebug() >= FMK_DEBUG_SOME)
        FlyMakeDepListPrint(pRootState->pDepList);
      FlyMakePrintfEx(FMK_VERBOSE_SOME, "\n# ---- Building dependencies... ----\n");

      pDep = pRootState->pDepList;
      while(!err && pDep)
      {
        if(pDep->pState)
        {
          err = FlyMakeBuildLibs(pDep->pState);
          if(pDep->pState->fLibCompiled)
          {
            pRootState->fLibCompiled = TRUE;
            ++pRootState->nCompiled;
          }
        }
        pDep = pDep->pNext;
      }
      FlyMakePrintfEx(FMK_VERBOSE_SOME, "\n# ---- Building project... ----\n");
    }
  }

  return err;
}

/*-------------------------------------------------------------------------------------------------
  Return an allocated folder/ based on target path. Parent folder must exist.

  szTarget                    | Returns
  --------------------------- | ------------
  "" (empty)                  | ./
  .                           | ./
  ..                          | ../
  file.c                      | ./
  folder/                     | folder/
  foo                         | foo/ (if folder)
  foo/src                     | foo/src/
  foo/src/foo                 | foo/src/
  foo/lib/foo.a               | foo/lib/
  ~/git/folder                | ~/git/folder/
  /Users/me/git/folder/file.c | /Users/me/git/folder/
  /                           | /

  @param  szTarget    file or folder, e.g. "file" , "folder/", "../folder/file"
  @return FMK_ERR_NONE if worked, otherwise FMK_ERR_MEM or FML_ERR_PATH
*///-----------------------------------------------------------------------------------------------
char * FlyMakeFolderAlloc(const char *szTarget, fmkErr_t *pErr)
{
  sFlyFileInfo_t  info;
  char           *szFolder  = NULL;
  bool_t          fIsDir    = FALSE;
  fmkErr_t        err       = FMK_ERR_NONE;

  // if a folder is specified, verify it exists
  FlyFileInfoInit(&info);
  if(FlyStrNextSlash(szTarget))
  {
    if(!FlyFileInfoGetEx(&info, szTarget))
      err = FMK_ERR_BAD_PATH;
    else
    {
      if(info.fIsDir)
        fIsDir = TRUE;
      else
      {
        szFolder = strdup(szTarget);
        if(szFolder)
          FlyStrPathOnly(szFolder);
      }
    }
  }

  // no path part, just name, which might be a folder name, e.g. "foo" instead of "foo/"
  // could also be non-existing file such as "mylib.a"
  else
  {
    if(*szTarget && FlyFileInfoGet(&info, szTarget) && info.fIsDir)
      fIsDir = TRUE;

    // file but no path specified, assume path is current folder
    else
      szFolder = FlyStrClone("./");
  }

  // determined that this is a folder, make sure it ends in a slash
  if(fIsDir)
  {
    szFolder = FlyAlloc(strlen(szTarget) + 2);
    if(szFolder == NULL)
      err = FlyMakeErrMem();
    else
    {
      strcpy(szFolder, szTarget);
      if(!isslash(FlyStrCharLast(szFolder)))
        strcat(szFolder, "/");
    }
  }

  *pErr = err;
  return szFolder;
}

/*-------------------------------------------------------------------------------------------------
  Free a target structure allocated by FlyMakeTargetAlloc()

  @param  pTarget    target structure
  @return NULL
*///-----------------------------------------------------------------------------------------------
void *FlyMakeTargetFree(fmkTarget_t *pTarget)
{
  if(pTarget)
  {
    FlyFreeIf(pTarget->szTarget);
    FlyFreeIf(pTarget->szFolder);
    FlyFreeIf(pTarget->szFile);
    memset(pTarget, 0, sizeof(*pTarget));
    FlyFree(pTarget);
  }
  return NULL;
}

/*!------------------------------------------------------------------------------------------------
  Allocates a target: Converts a user typed target into an allocated folder/, target and rule.

  Use FlyMakeTargetFree() to free the allocated fields of the fmkTarget_t structure.

  - szTarget: input target string
  - szFolder: folder must exist
  - szFile: NULL if folder only, 
  - rule: one of FMK_RULE_NONE (no rule), FMK_RULE_LIB, FMK_RULE_SRC, FMK_RULE_TOOL, FMK_RULE_PROJ

  Duties:

  1. Verify folder exists, otherwise FMK_ERR_BAD_PATH
  2. Verify folder is same project, otherwise FMK_ERR_NOT_SAME_ROOT
  3. Look for rule for folder if rule not already set from command-line, otherwise FMK_ERR_NO_RULE
  4. Determine target name

  @param  pState     Valid root state for main project
  @param  szTarget   A user typed path
  @param  pTarget    Return value with allocated fields: folder, target, rule
  @return FMK_ERR_NONE if worked or FMK_ERR_BAD_PATH, FMK_ERR_NOT_SAME_ROOT, FMK_ERR_NO_RULE
*///-----------------------------------------------------------------------------------------------
fmkTarget_t * FlyMakeTargetAlloc(flyMakeState_t *pState, const char *szTarget, fmkErr_t *pErr)
{
  fmkTarget_t     *pTarget;
  flyMakeFolder_t *pFolder  = NULL;
  const char     *psz;
  fmkRule_t       rule      = FMK_RULE_NONE;
  fmkErr_t        err       = FMK_ERR_NONE;

  FlyMakeDbgPrintf(FMK_DEBUG_MORE, "FlyMakeTargetAlloc(%s)\n", szTarget);

  // initalize return value
  pTarget = FlyAllocZ(sizeof(*pTarget));
  if(!pTarget)
    err = FlyMakeErrMem();

  if(!err)
  {
    pTarget->szTarget = FlyStrClone(szTarget);
    if(!pTarget->szTarget)
      err = FlyMakeErrMem();
  }

  // 1. Verify folder exists, otherwise FMK_ERR_BAD_PATH
  if(!err)
    pTarget->szFolder = FlyMakeFolderAlloc(szTarget, &err);

  // 2. Verify folder is same project, otherwise FMK_ERR_NOT_SAME_ROOT
  if(!err && !FlyMakeIsSameRoot(pState, pTarget->szFolder))
    err = FMK_ERR_NOT_SAME_ROOT;

  // 3. Look for rule for folder if rule not already set from command-line, otherwise FMK_ERR_NO_RULE
  if(!err)
  {
    // rule is always FMK_RULE_PROJ for project root
    if(FlyMakeIsSameFolder(szTarget, pState->szRoot))
      pTarget->rule = FMK_RULE_PROJ;
  
    // if user specified rule command-line, use that rule
    else
    {
      // look for default rule if not specified by user
      if(pState->opts.fRulesLib)
        rule = FMK_RULE_LIB;
      else if(pState->opts.fRulesSrc)
        rule = FMK_RULE_SRC;
      else if(pState->opts.fRulesTools)
        rule = FMK_RULE_TOOL;
      else
      {
        pFolder = pState->pFolderList;
        while(pFolder)
        {
          if(FlyMakeIsSameFolder(pFolder->szFolder, pTarget->szFolder))
          {
            FlyMakeDbgPrintf(FMK_DEBUG_MORE, "  found folder %s, rule %u\n", pFolder->szFolder, pFolder->rule);
            rule = pFolder->rule;
            break;
          }
          pFolder = pFolder->pNext;
        }
      }
      if(rule == FMK_RULE_NONE)
        err = FMK_ERR_NO_RULE;
      else
        pTarget->rule = rule;
    }
  }

  // 4. Determine file name part
  if(!err && pFolder)
  {
    pTarget->szFile = NULL;
    if(!FlyFileExistsFolder(szTarget))
    {
      psz = FlyStrLastSlash(szTarget);
      if(!psz)
        pTarget->szFile = FlyStrClone(szTarget);
      else if(psz[1] != '\0')
        pTarget->szFile = FlyStrClone(psz + 1);
    }
  }

  // if any errors, return NULL (no target)
  *pErr = err;
  if(err)
    pTarget = FlyMakeTargetFree(pTarget);

  if(pTarget)
  {
    FlyMakeDbgPrintf(FMK_DEBUG_MORE, "  err %u, szTarget %s, szFolder %s, szFile %s, rule %u\n",
      err, pTarget->szTarget, pTarget->szFolder, pTarget->szFile, pTarget->rule);
  }
  else
    FlyMakeDbgPrintf(FMK_DEBUG_MORE, "  err %u, pTarget (NULL)\n", err);

  return pTarget;
}

/*!------------------------------------------------------------------------------------------------
  Build libraries only in this project

  @param  pState          Valid root state for main project
  @return FMK_ERR_NONE if worked, otherwise FMK_ERR_nnn
*///-----------------------------------------------------------------------------------------------
fmkErr_t FlyMakeBuildLibs(flyMakeState_t *pState)
{
  fmkErr_t          err = FMK_ERR_NONE;
  flyMakeFolder_t *pFolder;

  pFolder = pState->pFolderList;
  while(pFolder)
  {
    if(pFolder->rule == FMK_RULE_LIB)
    {
      if(!FlyMakeBuildLib(pState, pFolder->szFolder))
      {
        err = FMK_ERR_CUSTOM;
        break;
      }
    }
    pFolder = pFolder->pNext;
  }

  return err;
}

/*!-------------------------------------------------------------------------------------------------
  Build all deependencies, then the target file/folder

  1. Dependencies will be built only once if this is called repeatedly
  2. Any error will stop the build projecess
  3. All targets must be on the same root project, or it is an error

  @param  pState          Valid root state for main project
  @param  pTarget         A parsed target originally provided by user
  @param  ppszErrExtra    must be initialized with original target that created pTarget
  @return FMK_ERR_NONE if worked, otherwise FMK_ERR_nnn
*///-----------------------------------------------------------------------------------------------
fmkErr_t FlyMakeBuild(flyMakeState_t *pState, fmkTarget_t *pTarget, char **ppszErrExtra)
{
  fmkErr_t        err     = FMK_ERR_NONE;

  // must have allocated root
  FlyAssert(FlyMakeIsState(pState) && pState->szRoot);
  FlyMakeDbgPrintf(FMK_DEBUG_SOME, "FlyMakeBuild(pTarget %p, szFolder %s, szFile %s, rule %u)\n", pTarget,
                   pTarget->szFolder, FlyStrNullOk(pTarget->szFile), pTarget->rule);
  FlyAssert(pTarget && pTarget->rule);

  // set error extra info to target path
  *ppszErrExtra = pTarget->szTarget;

  // build based on rule
  if(pTarget->rule == FMK_RULE_PROJ)
  {
    err = FmkDepBuildProject(pState);
  }
  else if(pTarget->rule == FMK_RULE_LIB)
  {
    if(!FlyMakeBuildLib(pState, pTarget->szFolder))
      err = FMK_ERR_CUSTOM;
  }
  else if(pTarget->rule == FMK_RULE_SRC)
  {
    if(!FlyMakeBuildSrc(pState, pTarget->szFolder))
      err = FMK_ERR_CUSTOM;
  }
  else if(pTarget->rule == FMK_RULE_TOOL)
  {
    if(!FlyMakeBuildTools(pState, pTarget->szFolder, pTarget->szFile))
      err = FMK_ERR_CUSTOM;
  }

  return err;
}
