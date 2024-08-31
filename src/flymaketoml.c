/**************************************************************************************************
  flymaketoml.c - Processes flymake.toml configuration files
  Copyright 2024 Drew Gislason  
  license: MIT <https://mit-license.org>
**************************************************************************************************/
#include "flymake.h"

typedef struct
{
  const char  *szMarker;  // marker string in format string
  unsigned     found;     // # of times it was found
} fmkMarker_t;

// for new projects, default flymake.toml file
static const char m_szTomlFmtDefault[] =
{
  "[package]\n"
  "name = \"%s\"\n"
  "version = \"0.1.0\"\n"
  "std = \"*\"\n"
  "\n"
  "[dependencies]\n"
  "# foo = { path=\"../foo/lib/foo.a\", inc=\"../foo/inc\" }\n"
  "# bar = { path=\"../bar\" }\n"
  "# flylib = { git=\"git@github.com:drewagislason/flylibc.git\" }\n"
  "\n"
  "[compiler]\n"
  "# \".c\" = { cc=\"cc {in} -c {incs}{warn}{debug}-o {out}\", ll=\"cc {in} {libs}{debug}-o {out}\" }\n"
  "# \".c++.cpp.cxx.cc.C\" = { cc=\"c++ {in} -c {incs}{warn}{debug} -o {out}\", ll=\"c++ {in} {libs}{debug}-o {out}\" }\n"
  "\n"
  "[folders]\n"
  "# \"lib/\" = \"--rl\"\n"
  "# \"src/\" = \"--rt\"\n"
  "# \"src/\" = \"--rs\"\n"
};

// archiving .o (objs) into a library is the same for all languages
const char g_szFmtArchive[]           = "ar -crs %s %s";

// default compile/link/archive format strings
// .c = { cc="cc {in} -c {inc} {warn} -o {out}", ll="cc {in} {libs} -o {out} ", cc_dbg="-g -DDEBUG=1", ll_dbg="-g" }

static const char m_szExtsC[]         = ".c";
static const char m_szDefCc[]         = "cc {in} -c {incs}{warn}{debug}-o {out}";
static const char m_szDefLl[]         = "cc {in} {libs}{debug}-o {out}";
static const char m_szDefCcDbg[]      = "-g -DDEBUG=1 ";
static const char m_szDefLlDbg[]      = "-g ";
static const char m_szDefInc[]        = "-I";
static const char m_szDefWarn[]       = "-Wall -Werror ";

static const char m_szCppExts[]       = ".c++.cpp.cxx.cc.C";
static const char m_szCppDefCc[]      = "c++ {in} -c {incs}{warn}{debug}-o {out}";
static const char m_szCppDefLl[]      = "c++ {in} {libs}{debug}-o {out}";


static const char m_szKeyCc[]         = "cc";
static const char m_szKeyCcDbg[]      = "cc_dbg";
static const char m_szKeyLl[]         = "ll";
static const char m_szKeyLlDbg[]      = "ll_dbg";
static const char m_szKeyInc[]        = "inc";
static const char m_szKeyWarn[]       = "warn";

static const char m_szDepDir[]        = FMK_SZ_DEP_DIR;
const char        g_szTomlFile[]      = FMK_SZ_FLYMAKE_TOML;  // used as a public API

// alternative names for these folders...
static const char  *m_aszInc[]        = { "inc/", "include/" };
static const char  *m_aszRoot[]       = { "flymake.toml", "src/", "source/", "lib/", "library/" };
static const char  *m_aszLib[]        = { "lib/", "library/" };
static const char  *m_aszSrc[]        = { "src/", "source/" };
flyMakeFolder_t     m_aDefFolders[]   =
{
  {.szFolder = "src/",      .rule=FMK_RULE_SRC },
  {.szFolder = "source/",   .rule=FMK_RULE_SRC },
  {.szFolder = "lib/",      .rule=FMK_RULE_LIB },
  {.szFolder = "library/",  .rule=FMK_RULE_LIB },
  {.szFolder = "test/",     .rule=FMK_RULE_TOOL },
};

static const char  *m_aszRules[]      = { "--rl", "--rs", "--rt", NULL };
static const char  m_szRuleInvalid[]  = "build rule must be one of \"--rl\", \"--rs\" or \"--rt\"";
static const char  m_szFolderNotStr[] = "Folder must be in string form, e.g. \"folder\"";

/*-------------------------------------------------------------------------------------------------
  Allocate all found folders into the state

  For example if szPath is "~/Work/folder/

  @param    szPath        destination path string
  @param    size          sizeof(path)
  @param    szPath1       start of path
  @param    szPath2       middle of path
  @param    szPath3       end of path
  @return   none
*///-----------------------------------------------------------------------------------------------
static void FmkMakePathFrom3(char *szPath, unsigned size, const char *szPath1, const char *szPath2, const char *szPath3)
{
  FlyStrZCpy(szPath, szPath1, size);
  FlyStrPathAppend(szPath, szPath2, size);
  FlyStrPathAppend(szPath, szPath3, size);
}

/*-------------------------------------------------------------------------------------------------
  Allocate a UTF-8 string from a TOML string.

  @param    szTomlStr   NULL or ptr to a TOML string, e.g. "1.2.3" or "af54e24"
  @return   ptr to allocated string, or NULL if szTomlStr was NULL
*///-----------------------------------------------------------------------------------------------
char * FlyMakeTomlStrAlloc(const char *szTomlStr)
{
  char      *sz = NULL;
  unsigned   size;

  // allocate pDep->szName
  if(szTomlStr)
  {
    size = FlyTomlStrLen(szTomlStr) + 1;
    sz = FlyAlloc(size);
    if(sz != NULL)
      FlyTomlStrCpy(sz, szTomlStr, size);
  }

  return sz;
}

/*-------------------------------------------------------------------------------------------------
  Allocate a UTF-8 string from a TOML key.

  @param    szTomlKey      ptr to a key name, e.g. foo = { path="../foo/" }
  @return   none
*///-----------------------------------------------------------------------------------------------
char * FlyMakeTomlKeyAlloc(const char *szTomlKey)
{
  char      *szName;
  unsigned   size;

  // allocate pDep->szName
  size = FlyTomlKeyLen(szTomlKey) + 1;
  szName = FlyAlloc(size);
  if(szName != NULL)
    FlyTomlKeyCpy(szName, szTomlKey, size);

  return szName;
}

/*--------------------------------------------------------------------------------------------------
  Get default flymake.toml with exactly 1 %s for the project name

  @return ptr to default flymake.toml with exactly 1 %s for the project name
*///-----------------------------------------------------------------------------------------------
const char *FlyMakeTomlFmtFileDefault(void)
{
  return m_szTomlFmtDefault;
}

/*--------------------------------------------------------------------------------------------------
  Find the compiler for this file extension.

  @param    pCompilerList   linked list of flyMakeCompiler_t structures
  @param    szExt           file extension, e.g. ".c" or ".c++"
  @return   ptr to compiler or NULL if ext not found in compiler list
*///-----------------------------------------------------------------------------------------------
flyMakeCompiler_t * FlyMakeCompilerFind(const flyMakeCompiler_t *pCompilerList, const char *szExt)
{
  const char               *psz;
  const flyMakeCompiler_t  *pCompiler;
  unsigned                  len = strlen(szExt);
 
  // look for a compiler that can handle this file extsion, e.g. ".c "or ".c++"
  pCompiler = pCompilerList;
  while(szExt && *szExt && pCompiler)
  {
    psz = strstr(pCompiler->szExts, szExt);
    if(psz && (psz[len] == '.' || psz[len] == '\0'))
      break;
    pCompiler = pCompiler->pNext;
  }

  return (flyMakeCompiler_t *)pCompiler;
}

/*--------------------------------------------------------------------------------------------------
  Given list of compilers, return a string with all file extensions

  @param    pCompilerList   linked list of flyMakeCompiler_t structures
  @param    szTomlKey       key for this compiler, e.g. ".c" or ".c++.cc.cpp.cxx"
  @return   ptr to compiler or NULL if ext not found in compiler list
*///-----------------------------------------------------------------------------------------------
char * FlyMakeCompilerAllExts(const flyMakeCompiler_t *pCompilerList)
{
  char                     *pAllExts  = NULL;
  const flyMakeCompiler_t  *pCompiler;
  unsigned                  len       = 0;

  // determine length
  pCompiler = pCompilerList;
  while(pCompiler)
  {
    len += strlen(pCompiler->szExts);
    pCompiler = pCompiler->pNext;
  }

  // create one big string
  pAllExts = FlyAllocZ(len + 1);
  pCompiler = pCompilerList;
  if(pAllExts)
  {
    while(pCompiler)
    {
      strcat(pAllExts, pCompiler->szExts);
      pCompiler = pCompiler->pNext;
    }
  }

  return pAllExts;
}

/*--------------------------------------------------------------------------------------------------
  Find the compiler by key

  @param    pCompilerList   linked list of flyMakeCompiler_t structures
  @param    szTomlKey       key for this compiler, e.g. ".c" or ".c++.cc.cpp.cxx"
  @return   ptr to compiler or NULL if ext not found in compiler list
*///-----------------------------------------------------------------------------------------------
flyMakeCompiler_t * FlyMakeCompilerFindByKey(const flyMakeCompiler_t *pCompilerList, const char *szTomlKey)
{
  const flyMakeCompiler_t  *pCompiler;
 
  pCompiler = pCompilerList;
  while(pCompiler)
  {
    if(strcmp(pCompiler->szExts, szTomlKey) == 0)
      break;
    pCompiler = pCompiler->pNext;
  }

  return (flyMakeCompiler_t *)pCompiler;
}

/*--------------------------------------------------------------------------------------------------
  Allocate a new compiler structure

  @return   ptr to allocated flymake compiler list for default supported languages and options
*///-----------------------------------------------------------------------------------------------
static flyMakeCompiler_t * FmkCompilerNew(const char *szExts)
{
  flyMakeCompiler_t *pCompiler;

  pCompiler = malloc(sizeof(*pCompiler));
  if(pCompiler)
  {
    memset(pCompiler, 0, sizeof(*pCompiler));
    pCompiler->szExts = FlyStrClone(szExts);
  }

  return pCompiler;
}

/*--------------------------------------------------------------------------------------------------
  Free compiler structure

  @return   ptr to allocated flymake compiler list for default supported languages and options
*///-----------------------------------------------------------------------------------------------
static flyMakeCompiler_t * FmkCompilerFree(flyMakeCompiler_t *pCompiler)
{
  FlyStrFreeIf(pCompiler->szExts);
  FlyStrFreeIf(pCompiler->szCc);
  FlyStrFreeIf(pCompiler->szCcDbg);
  FlyStrFreeIf(pCompiler->szInc);
  FlyStrFreeIf(pCompiler->szWarn);
  FlyStrFreeIf(pCompiler->szLl);
  FlyStrFreeIf(pCompiler->szLlDbg);
  memset(pCompiler, 0, sizeof(*pCompiler));
  return NULL;
}

/*--------------------------------------------------------------------------------------------------
  Free compiler list

  @return   none
*///-----------------------------------------------------------------------------------------------
void FmkCompilerListFree(flyMakeCompiler_t *pHead)
{
  flyMakeCompiler_t *pThis;

  while(pHead)
  {
    pThis = pHead;
    pHead = FlyListRemove(pHead, pThis);
    FmkCompilerFree(pThis);
  }
}

/*--------------------------------------------------------------------------------------------------
  Display the a single compiler structure

  @return  ptr to allocated compiler list for default supported languages and options (C and C++)
*///-----------------------------------------------------------------------------------------------
void FlyMakeCompilerPrint(const flyMakeCompiler_t *pCompiler)
{
  FlyMakePrintf("%s={cc=%s, ll=%s,\n    cc_dbg=%s, ll_dbg=%s, inc=%s, warn=%s}\n",
      pCompiler->szExts, pCompiler->szCc, pCompiler->szLl, pCompiler->szCcDbg,
      pCompiler->szLlDbg, pCompiler->szInc, pCompiler->szWarn);
}

/*--------------------------------------------------------------------------------------------------
  Display the compiler list

  @return  ptr to allocated compiler list for default supported languages and options (C and C++)
*///-----------------------------------------------------------------------------------------------
void FlyMakeCompilerListPrint(const flyMakeCompiler_t *pCompilerList)
{
  const flyMakeCompiler_t *pCompiler = pCompilerList;
  while(pCompiler)
  {
    FlyMakeCompilerPrint(pCompiler);
    pCompiler = pCompiler->pNext;
  }
}

/*--------------------------------------------------------------------------------------------------
  Creates default list for C and C++. flymake.toml may override some or all the fields.

  The compiler list defines how to compile and link source code into programs and libraries.

  See also: FlyMakeTomlAlloc()

  Each field must be allocated so it can be freed() and overridden by flymake.toml.

  @return  ptr to allocated compiler list for default supported languages and options (C and C++)
*///-----------------------------------------------------------------------------------------------
flyMakeCompiler_t * FlyMakeCompilerListDefault(void)
{
  flyMakeCompiler_t *pCompilerList = NULL;
  flyMakeCompiler_t *pCompiler;

  // create default C compiler structure
  pCompiler = FmkCompilerNew(m_szExtsC);
  if(pCompiler)
  {
    pCompilerList = pCompiler;
    pCompiler->szCc     = FlyStrClone(m_szDefCc);     // "cc {in} -c {incs}{warn}{debug}-o {out}"
    pCompiler->szLl     = FlyStrClone(m_szDefLl);     // "cc {in} {libs}{debug}-o {out}"
    pCompiler->szInc    = FlyStrClone(m_szDefInc);    // "-I";
    pCompiler->szWarn   = FlyStrClone(m_szDefWarn);   // "-Wall -Werror";
    pCompiler->szCcDbg  = FlyStrClone(m_szDefCcDbg);  // "-g -DDEBUG=1";
    pCompiler->szLlDbg  = FlyStrClone(m_szDefLlDbg);  // "-g";

    // create default C++ compiler structure
    pCompiler = FmkCompilerNew(m_szCppExts);
    if(pCompiler)
    {
      pCompilerList->pNext = pCompiler;
      pCompiler->szCc     = FlyStrClone(m_szCppDefCc);    // "c++ {in} -c {incs}{warn}{debug}-o {out}"
      pCompiler->szCcDbg  = FlyStrClone(m_szDefCcDbg);    // "-g -DDEBUG=1";
      pCompiler->szInc    = FlyStrClone(m_szDefInc);      // "-I";
      pCompiler->szWarn   = FlyStrClone(m_szDefWarn);     // "-Wall -Werror";
      pCompiler->szLl     = FlyStrClone(m_szCppDefLl);    // "c++ {in} {libs}{debug}-o {out}"
      pCompiler->szLlDbg  = FlyStrClone(m_szDefLlDbg);    // "-g";
    }
  }

  return pCompilerList;
}

#if 0
/*-------------------------------------------------------------------------------------------------
  Convert from {markers} to %s

  This will either ammend an existing flyMakeCompiler_t, or prepend a new one to the list.

  @param  sz          string to check for markers
  @param  aMarkers    an array of markers
  @param  n  
  @return the modified string
*///-----------------------------------------------------------------------------------------------
static char * FmkConvertMarkers(char *szStr, fmkMarker_t *aMarkers, unsigned n)
{
  char     *psz;
  unsigned  len;
  unsigned  i;

   // mark of all markers found
  for(i = 0; i < n; ++i)
  {
    psz = strstr(szStr, aMarkers[i].szMarker);
    if(psz)
    {
      len = strlen(aMarkers[i].szMarker);
      memmove(&psz[2], &psz[len], strlen(&psz[len]) + 1);
      memcpy(psz, "%s", 2);
    }
  }

  return szStr;
}
#endif

/*-------------------------------------------------------------------------------------------------
  Converts a space separate list of folders to an allocated list of include options.

  For example, converts ". inc/ deps/dep1/inc/" to "-I. -Iinc/ -Ideps/dep1/inc/"
  If szIncs is "", then returns allocated ""

  @param  szIncs list of include folders, e.g. ". inc/ deps/dep1/inc/"
  @param  szIncOpt, e.g. "-I"
  @return allocated string or NULL if failed to allocate memory.
*///-----------------------------------------------------------------------------------------------
static char * FmkAddIncOpts(const char *szIncs, const char *szIncOpt)
{
  const char *psz;
  char       *pszNewIncs = NULL;
  unsigned    len;
  unsigned    size = 0;

  // get length of list
  psz = szIncs;
  do
  {
    len = FlyStrArgLen(psz);
    if(len)
      size += strlen(szIncOpt) + len + 1;
    psz = FlyStrArgNext(psz);
  } while(len);

  // no include folders, return allocated empty string
  if(size == 0)
    pszNewIncs = FlyStrClone("");

  // create list of include options, e.g. "-I. -Iinc/ -Ideps/dep1/inc/"
  else
  {
    ++size;
    pszNewIncs = FlyAlloc(size);
    if(pszNewIncs)
    {
      memset(pszNewIncs, 0, size);
      psz = szIncs;
      do
      {
        len = FlyStrArgLen(psz);
        if(len)
        {
          strcat(pszNewIncs, szIncOpt);
          strncat(pszNewIncs, psz, len);
          strcat(pszNewIncs, " ");
        }
        psz = FlyStrArgNext(psz);
      } while(len); 
    }
  }

  return pszNewIncs;
}

/*-------------------------------------------------------------------------------------------------
  Does the substiturions, converting from {makers} to the given strings

  This will either ammend an existing flyMakeCompiler_t, or prepend a new one to the list.

  @param  pStr        return value, smart string to be filled in
  @param  pComplier   compiler to use with format strings
  @param  szIn        input file(s)
  @param  szInc       include libraries, e.g. "mylib.a deplib.a "
  @param  szWarn      warning options, e.g. "-Wall -Werror"
  @param  szDebug     debug options, e.g. "-g -DEBUG=1"
  @param  szOut       output file(s)
  @return TRUE if worked, FALSE if no memory
*///-----------------------------------------------------------------------------------------------
bool_t FlyMakeCompilerFmtCompile(
  flyStrSmart_t *pStr,
  const flyMakeCompiler_t *pCompiler,
  const char *szIn,
  const char *szIncs,
  const char *szWarn,
  const char *szDebug,
  const char *szOut)
{
  const char *aszMarkers[] = { "{in}", "{incs}", "{warn}", "{debug}", "{out}" };
  char       *psz;
  const char *szSub;
  unsigned    len;
  unsigned    lenSub;
  unsigned    i;
  bool_t      fWorked = TRUE;

  // make sure input smart string is large enough
  len = strlen(pCompiler->szCc) + strlen(szIn) + strlen(szIncs) + strlen(szWarn) + strlen(szDebug) + strlen(szOut);
  if(!FlyStrSmartResize(pStr, len))
    fWorked = FALSE;
  else
  {
    FlyStrSmartCpy(pStr, pCompiler->szCc);
    for(i = 0; fWorked && i < NumElements(aszMarkers); ++i)
    {
      // markers must have already been validated
      psz = strstr(pStr->sz, aszMarkers[i]);
      FlyAssert(psz);

      // substitute each marker
      if(i == 0)
        szSub = szIn;
      else if(i == 1)
      {
        // converts ". inc/ dep/foo/inc/" to "-I. -Iinc/ -Idep/foo/inc/ "
        szSub = FmkAddIncOpts(szIncs, pCompiler->szInc);
        if(!szSub)
          fWorked = FALSE;
      }
      else if(i == 2)
        szSub = szWarn;
      else if(i == 3)
        szSub = szDebug;
      else
        szSub = szOut;

      // do the substitution
      if(fWorked)
      {
        len = strlen(aszMarkers[i]);
        lenSub = strlen(szSub);
        memmove(&psz[lenSub], &psz[len], strlen(&psz[len]) + 1);
        memcpy(psz, szSub, lenSub);
        if(i == 1)
          FlyFree((void *)szSub);
      }
    }
  }

  return fWorked;
}

/*-------------------------------------------------------------------------------------------------
  Return the formatted string with values filled in in the 

  This will either ammend an existing flyMakeCompiler_t, or prepend a new one to the list.

  @param  pStr        return value, smart string to be filled in
  @param  pComplier   compiler to use with format strings
  @param  szIn        input file(s)
  @param  szLibs      libraries, e.g "mylib.a dep.a"
  @param  szDebug     debug options, e.g. "-g"
  @param  szOut       output file(s)
  @return TRUE if worked, FALSE if no memory
*///-----------------------------------------------------------------------------------------------
bool_t FlyMakeCompilerFmtLink(
  flyStrSmart_t *pStr,
  const flyMakeCompiler_t *pCompiler,
  const char *szIn,
  const char *szLibs,
  const char *szDebug,
  const char *szOut)
{
  const char *aszMarkers[] = { "{in}", "{libs}", "{debug}", "{out}" };
  char       *psz;
  const char *szSub;
  unsigned    len;
  unsigned    lenSub;
  unsigned    i;
  bool_t      fWorked = TRUE;

  // make sure input smart string is large enough
  len = strlen(pCompiler->szLl) + strlen(szIn) + strlen(szLibs) + strlen(szDebug) + strlen(szOut);
  if(!FlyStrSmartResize(pStr, len))
    fWorked = FALSE;
  else
  {
    FlyStrSmartCpy(pStr, pCompiler->szLl);
    for(i = 0; i < NumElements(aszMarkers); ++i)
    {
      // markers must have already been validated
      psz = strstr(pStr->sz, aszMarkers[i]);
      FlyAssert(psz);

      // substitute each marker
      if(i == 0)
        szSub = szIn;
      else if(i == 1)
        szSub = szLibs;
      else if(i == 2)
        szSub = szDebug;
      else
        szSub = szOut;

      // do the substitution
      len = strlen(aszMarkers[i]);
      lenSub = strlen(szSub);
      memmove(&psz[lenSub], &psz[len], strlen(&psz[len]) + 1);
      memcpy(psz, szSub, lenSub);
    }
  }
  return fWorked;
}

/*-------------------------------------------------------------------------------------------------
  Found a key in the [compiler] table, process it.

  This will either ammend an existing flyMakeCompiler_t, or prepend a new one to the list.

  @param  sz          string to check for markers
  @param  aMarkers    an array of markers
  @param  n  
  @return TRUE if all markers are there, FALSE if not
*///-----------------------------------------------------------------------------------------------
static bool_t  FmkTomlCheckMarkers(const char *sz, fmkMarker_t *aMarkers, unsigned n)
{
  unsigned    i;
  bool_t      fWorked = TRUE;

  // start with no markers found
  for(i = 0; i < n; ++i)
    aMarkers[i].found = 0;

  // mark of all markers found
  for(i = 0; i < n; ++i)
  {
    if(strstr(sz, aMarkers[i].szMarker) != NULL)
      ++aMarkers[i].found;
  }

  // all markers must be there, once and only once
  for(i = 0; i < n; ++i)
  {
    if(aMarkers[i].found != 1)
    {
      fWorked = FALSE;
      break;
    }
  }

  return fWorked;
}

/*-------------------------------------------------------------------------------------------------
  Add a space at end if needed to the string.

  May free the sz and replace it with another string.

  @param    sz    allocated string from heap
  @return   appended string with space, or unchanged if already has space
*///-----------------------------------------------------------------------------------------------
char * FmkAddSpace(char *sz)
{
  char *szNew = sz;

  if(FlyStrCharLast(sz) != ' ')
  {
    szNew = FlyAlloc(strlen(sz) + 2);
    if(szNew == NULL)
      szNew = sz;
    if(szNew)
    {
      strcpy(szNew, sz);
      strcat(szNew, " ");
      free(sz);
    }
  }

  return szNew;
}

/*-------------------------------------------------------------------------------------------------
  Found a key in the [compiler] table, process it.

  This will either ammend an existing flyMakeCompiler_t, or prepend a new one to the list.

  @return   archive format string
*///-----------------------------------------------------------------------------------------------
fmkErr_t FmkTomlProcessCompilerKey(flyMakeState_t *pState, tomlKey_t *pKey)
{
  fmkMarker_t aMarkersCompile[]   = { {"{in}"}, {"{incs}"}, {"{warn}"}, {"{debug}"}, {"{out}"} };
  fmkMarker_t aMarkersLink[]      = { {"{in}"}, {"{libs}"}, {"{debug}"}, {"{out}"} };
  static const char szTomlCompileErr[]  = "cc= must contain: {in} {incs} {warn} {debug} {out}";
  static const char szTomlLinkErr[]     = "ll= must contain: {in} {libs} {debug} {out}";

  tomlKey_t           key;
  flyMakeCompiler_t  *pCompiler;
  const char         *szIter;
  char               *szValue;
  char               *szKey;
  fmkErr_t            err = FMK_ERR_NONE;

  // if not an inline table, then there can't be keys
  if(FlyTomlType(pKey->szValue) != TOML_INLINE_TABLE)
  {
    FlyMakeErrToml(pState, key.szValue, "Expected TOML inline table");
    err = FMK_ERR_CUSTOM;
  }

  // get the key in string form
  if(!err)
  {
    szKey = FlyMakeTomlKeyAlloc(pKey->szKey);
    if(!szKey)
      err = FlyMakeErrMem();
  }

  // if found by key, we are updating, otherwising, creating a new compiler
  if(!err)
  {
    pCompiler = FlyMakeCompilerFindByKey(pState->pCompilerList, szKey);
    if(pCompiler == NULL)
      pCompiler = FmkCompilerNew(szKey);
    if(!pCompiler)
      err = FlyMakeErrMem();
    FlyFree(szKey);
  }

  // iterate through keys and look for a match
  if(!err)
  {
    szIter = FlyTomlKeyIter(pKey->szValue, &key);
    while(szIter && !err)
    {
      if(key.type != TOML_STRING)
      {
        FlyMakeErrToml(pState, key.szValue, "Expected string");
        err = FMK_ERR_CUSTOM;
      }

      // process each key
      szValue = FlyMakeTomlStrAlloc(key.szValue);
      szKey   = FlyMakeTomlKeyAlloc(key.szKey);
      if(!szValue || !szKey)
        err = FlyMakeErrMem();

      // cc= "cc %s -c %s%s%s-o %s""
      else if(strcmp(szKey, m_szKeyCc) == 0)
      {
        if(!FmkTomlCheckMarkers(szValue, aMarkersCompile, NumElements(aMarkersCompile)))
        {
          FlyMakeErrToml(pState, key.szValue, szTomlCompileErr);
          err = FMK_ERR_CUSTOM;
        }
        else
        {
          FlyStrFreeIf(pCompiler->szCc);
          pCompiler->szCc = szValue;
        }
      }

      // ll= "cc %s %s%s-o %s"
      else if(strcmp(szKey, m_szKeyLl) == 0)
      {
        if(!FmkTomlCheckMarkers(szValue, aMarkersLink, NumElements(aMarkersLink)))
        {
          FlyMakeErrToml(pState, key.szValue, szTomlLinkErr);
          err = FMK_ERR_CUSTOM;
        }
        else
        {
          FlyStrFreeIf(pCompiler->szLl);
          pCompiler->szLl = szValue;
        }
      }

      // cc_dbg= "-g -DDEBUG=1 "
      else if(strcmp(szKey, m_szKeyCcDbg) == 0)
      {
        FlyStrFreeIf(pCompiler->szCcDbg);
        pCompiler->szCcDbg = FmkAddSpace(szValue);
      }

      // ll_dbg= "-g "
      else if(strcmp(szKey, m_szKeyLlDbg) == 0)
      {
        FlyStrFreeIf(pCompiler->szLlDbg);
        pCompiler->szLlDbg = FmkAddSpace(szValue);
      }

      // inc= "-I"
      else if(strcmp(szKey, m_szKeyInc) == 0)
      {
        FlyStrFreeIf(pCompiler->szInc);
        pCompiler->szInc = szValue;
      }

      // warn= "-Wall -Werror "
      else if(strcmp(szKey, m_szKeyWarn) == 0)
      {
        FlyStrFreeIf(pCompiler->szWarn);
        pCompiler->szWarn = FmkAddSpace(szValue);
      }

      FlyStrFreeIf(szKey);
      szIter = FlyTomlKeyIter(szIter, &key);
    }
  }

  // at a minimum, need at least 
  if(!pCompiler->szCc || !pCompiler->szLl)
  {
    FlyMakeErrToml(pState, pKey->szValue, "Keys cc=, ll= are required");
    err = FMK_ERR_CUSTOM;
  }

  // optional fields will be filled in with C defaults if not set
  if(!err)
  {
    if(!pCompiler->szInc)
      pCompiler->szInc = FlyStrClone(m_szDefInc);
    if(!pCompiler->szCcDbg)
      pCompiler->szCcDbg = FlyStrClone(m_szDefCcDbg);
    if(!pCompiler->szLlDbg)
      pCompiler->szLlDbg = FlyStrClone(m_szDefLlDbg);
    if(!pCompiler->szWarn)
      pCompiler->szWarn = FlyStrClone(m_szDefWarn);
  }

  return err;
}

/*-------------------------------------------------------------------------------------------------
  Process the [package] section of flymake.toml.

  If there is no flymake.toml file, then the 

  Fills in the following pState fields: szProjName, szProjVer, szLibName, szSrcName

  @return   TRUE if worked. FALSE (and prints TOML ERR) if failed
*///-----------------------------------------------------------------------------------------------
bool_t FmkTomlProcessPackage(flyMakeState_t *pState, const char *szName)
{
  const char     *szProjName  = NULL;
  unsigned        projNameLen = 0;
  unsigned        size;
  tomlKey_t       key;
  bool_t          fWorked     = TRUE;

  // determine project name, which may be provided in flymake.toml file
  if(szName)
    pState->szProjName = FlyStrClone(szName);
  else if(pState->szTomlFile)
  {
    if(FlyTomlKeyPathFind(pState->szTomlFile, "package:name", &key) && key.type == TOML_STRING)
    {
      projNameLen = FlyTomlStrLen(key.szValue);
      pState->szProjName = FlyAlloc(projNameLen + 1);
      if(pState->szProjName == NULL)
      {
        FlyMakeErrMem();
        fWorked = FALSE;
      }
      else
        FlyTomlStrCpy(pState->szProjName, key.szValue, projNameLen + 1);
    }
  }

  // no flymake.toml file or package:name, so use folder name as project name
  if(fWorked && pState->szProjName == NULL)
  {
    szProjName = FlyStrPathNameLast(pState->szFullPath, &projNameLen);
    pState->szProjName = FlyAlloc(projNameLen + 1);
    if(pState->szProjName == NULL)
    {
      FlyMakeErrMem();
      fWorked = FALSE;
    }
    else
    {
      strncpy(pState->szProjName, szProjName, projNameLen);
      pState->szProjName[projNameLen] = '\0';
    }
  }

#if 0
  // create library name if there is one, e.g. "../lib/foo.a"
  if(fWorked && pState->szLib)
  {
    // e.g. ../lib/projname.a
    pState->szLibName  = FlyAlloc(strlen(pState->szLib) + strlen(pState->szProjName) + 8);
    if(!pState->szLibName)
    {
      FlyMakeErrMem();
      fWorked = FALSE;
    }
    else
    {
      strcpy(pState->szLibName, pState->szLib);
      strcat(pState->szLibName, pState->szProjName);
      strcat(pState->szLibName, ".a");
    }
  }

  // create source name if there is one, e.g. "src/foo"
  if(fWorked && pState->szSrc)
  {
    // e.g. ../lib/projname.a
    pState->szSrcName  = FlyAlloc(strlen(pState->szSrc) + projNameLen + 1);
    if(!pState->szSrcName)
    {
      FlyMakeErrMem();
      fWorked = FALSE;
    }
    else
    {
      strcpy(pState->szSrcName, pState->szSrc);
      strcat(pState->szSrcName, pState->szProjName);
    }
  }
#endif

  // find package:version, defaults to "*"
  if(fWorked)
  {
    if(pState->szTomlFile &&
      FlyTomlKeyPathFind(pState->szTomlFile, "package:version", &key) && key.type == TOML_STRING)
    {
      size = FlyTomlStrLen(key.szValue) + 1;
      pState->szProjVer = FlyAlloc(size);
      if(pState->szProjVer)
        FlyTomlStrCpy(pState->szProjVer, key.szValue, size);
    }
    if(pState->szProjVer == NULL)
      pState->szProjVer = FlyStrClone("*");
    if(pState->szProjVer == NULL)
    {
      FlyMakeErrMem();
      fWorked = FALSE;
    }
  }

  return fWorked;
}

/*-------------------------------------------------------------------------------------------------
  Is this folder in the list? If so, what is the build rule?

  @param    pState    state for this project
  @param    szFolder  folder to check
  @return   FMK_RULE_NONE (not found) or FMK_RULE_LIB, FMK_RULE_SRC, FMK_RULE_TOOL
*///-----------------------------------------------------------------------------------------------
fmkRule_t FlyMakeTomlFindRule(flyMakeState_t *pState, const char *szFolder)
{
  fmkRule_t         rule      = FMK_RULE_NONE;
  flyMakeFolder_t  *pFolder;

  pFolder = pState->pFolderList;
  while(pFolder)
  {
    if(FlyMakeIsSameFolder(pFolder->szFolder, szFolder))
    {
      rule = pFolder->rule;
      break;
    }
    pFolder = pFolder->pNext;
  }

  return rule;
}

/*-------------------------------------------------------------------------------------------------
  Given a folder list, find the first folder with the given rule

  @param    pFolderList   A list of folders
  @param    rule          The rule to look for (e.g. FMK_RULE_LIB)    
  @return   ptr to folder if found or NULL
*///-----------------------------------------------------------------------------------------------
flyMakeFolder_t * FlyMakeFolderFindByRule(const flyMakeFolder_t *pFolderList, fmkRule_t rule)
{
  const flyMakeFolder_t *pFolder;

  pFolder = pFolderList;
  while(pFolder)
  {
    if(pFolder->rule == rule)
      break;
    pFolder = pFolder->pNext;
  }

  return (flyMakeFolder_t *)pFolder;
}

/*-------------------------------------------------------------------------------------------------
  Process the `[compiler]` section of flymake.toml.

  Fills in the following pState variables: pCompilerList, incs, libs

  @param    pState    state for this project
  @return   TRUE if worked, FALSE if failed
*///-----------------------------------------------------------------------------------------------
bool_t FmkTomlProcessCompiler(flyMakeState_t *pState)
{
  const char *pszTable;
  const char *szIter    = NULL;
  tomlKey_t   key;
  bool_t      fWorked   = TRUE;
  fmkErr_t    err = FMK_ERR_NONE;

  // look compilers to set up, e.g. ".c" = { cc="...", ll="..." }
  if(pState->szTomlFile)
  {
    pszTable = FlyTomlTableFind(pState->szTomlFile, "compiler");
    if(pszTable)
      szIter = FlyTomlKeyIter(pszTable, &key);
    while(szIter)
    {
      err = FmkTomlProcessCompilerKey(pState, &key);
      if(err)
        break;
      szIter = FlyTomlKeyIter(szIter, &key);
    }
  }

  // default compiler list MUST have already been filled in
  // needed early on to determine which folders contain source code to find root
  FlyAssert(pState->pCompilerList);

  if(err)
    fWorked = FALSE;
  return fWorked;
}

/*-------------------------------------------------------------------------------------------------
  Print a single folder structure

  @param    pFolder   folder
  @param    szTomlKey   ptr to a TOML key that is a folder path
  @return   none
*///-----------------------------------------------------------------------------------------------
void FlyMakeFolderPrint(const flyMakeFolder_t *pFolder)
{
  char szRule[8];

  if(pFolder->rule >= FMK_RULE_LIB && pFolder->rule <= FMK_RULE_TOOL)
    FlyStrZCpy(szRule, m_aszRules[pFolder->rule - FMK_RULE_LIB], sizeof(szRule));
  else
    FlyStrZCpy(szRule, "???", sizeof(szRule));
  FlyAssert(pFolder->szFolder && pFolder->rule >= FMK_RULE_LIB);
  FlyMakePrintf("{ szFolder=%s, rule = %s }\n", pFolder->szFolder, szRule);
}

/*-------------------------------------------------------------------------------------------------
  Free a folder structure

  @param    pFolder   folder
  @param    szTomlKey   ptr to a TOML key that is a folder path
  @return   none
*///-----------------------------------------------------------------------------------------------
void FlyMakeFolderListPrint(const flyMakeFolder_t *pFolderList)
{
  const flyMakeFolder_t *pFolder = pFolderList;
  while(pFolder)
  {
    FlyMakeFolderPrint(pFolder);
    pFolder = pFolder->pNext;
  }
}

/*-------------------------------------------------------------------------------------------------
  Free a folder structure

  @param    pFolder   folder
  @param    szTomlKey   ptr to a TOML key that is a folder path
  @return   none
*///-----------------------------------------------------------------------------------------------
static void FmkFolderFree(flyMakeFolder_t *pFolder)
{
  if(pFolder)
  {
    if(pFolder->szFolder)
      FlyFree(pFolder->szFolder);
    FlyFree(pFolder);
  }
}

/*-------------------------------------------------------------------------------------------------
  Allocate a new folder structure on the heap. Fill in pFolder->szFolder field from parameters.

  @param    szRoot      ptr to the root folder, e.g. "", or "~/Git/folder/"
  @param    szTomlKey   ptr to a TOML key that is a folder path
  @return   ptr to flyMakeFolder_t or NULL if failed
*///-----------------------------------------------------------------------------------------------
static flyMakeFolder_t * FmkFolderNew(const char *szRoot, const char *szTomlKey)
{
  flyMakeFolder_t  *pFolder;
  unsigned          len;
  unsigned          size;

  pFolder = FlyAllocZ(sizeof(*pFolder));
  if(pFolder)
  {
    size = strlen(szRoot) + FlyTomlKeyLen(szTomlKey) + 2;   // allow for slash and NUL
    pFolder->szFolder = FlyAllocZ(size);
    if(pFolder->szFolder)
    {
      strcpy(pFolder->szFolder, szRoot);
      len = strlen(pFolder->szFolder);
      FlyTomlKeyCpy(&pFolder->szFolder[len], szTomlKey, size - len);
      if(!FlyStrPathIsRelative(&pFolder->szFolder[len]))
        FlyTomlKeyCpy(pFolder->szFolder, szTomlKey, size);
      if(!isslash(FlyStrCharLast(pFolder->szFolder)))
        strcat(pFolder->szFolder, "/");
    }
    else
      FmkFolderFree(pFolder);
  }

  return pFolder;
}

/*-------------------------------------------------------------------------------------------------
  Returns allocated library name, e.g. "../project/lib/project.a" or "folder/folder.a"

  Folder must already contain path to root, e.g. "../project/lib/" or "folder/"

  Any folder named "lib" or "library" uses project name for library name.

  @return   ptr to allocated library "path/name.a" or NULL if allocation failed
*///-----------------------------------------------------------------------------------------------
char * FlyMakeFolderAllocLibName(flyMakeState_t *pState, const char *szFolder)
{
  char        *szLibName  = NULL;
  const char  *psz;
  unsigned    i;
  unsigned    len        = 0;
  unsigned    size;

  psz = FlyStrPathNameLast(szFolder, &len);
  for(i = 0; i < NumElements(m_aszLib); ++i)
  {
    if(strcmp(psz, m_aszLib[i]) == 0)
    {
      psz = pState->szProjName;
      len = strlen(psz);
      break;
    }
  }

  size = strlen(szFolder) + len + 3;
  szLibName = FlyAlloc(size);
  if(!szLibName)
    FlyMakeErrMem();
  else
  {
    FlyStrZCpy(szLibName, szFolder, size);
    FlyStrZNCat(szLibName, psz, size, len);
    FlyStrZCat(szLibName, ".a", size);
  }

  return szLibName;
}

/*-------------------------------------------------------------------------------------------------
  Returns allocated program name from a folder

  Any folder named "src" or "source" uses project name for program name, otherwise folder name is
  used.

  Folder must already contain path to root, e.g. "../project/src/" or "prog_name/"

  Examples:

  @return   ptr to allocated program name "src/proj_name" or NULL if allocation failure
*///-----------------------------------------------------------------------------------------------
char * FlyMakeFolderAllocSrcName(flyMakeState_t *pState,const char *szFolder)
{
  char        *szSrcName  = NULL;
  const char  *psz;
  unsigned    i;
  unsigned    len        = 0;
  unsigned    size;

  psz = FlyStrPathNameLast(szFolder, &len);
  for(i = 0; i < NumElements(m_aszSrc); ++i)
  {
    if(strcmp(psz, m_aszSrc[i]) == 0)
    {
      psz = pState->szProjName;
      len = strlen(psz);
      break;
    }
  }

  size = strlen(szFolder) + len + 1;
  szSrcName = FlyAlloc(size);
  if(!szSrcName)
    FlyMakeErrMem();
  else
  {
    FlyStrZCpy(szSrcName, szFolder, size);
    FlyStrZNCat(szSrcName, psz, size, len);
  }

  return szSrcName;
}

/*-------------------------------------------------------------------------------------------------
  Process the `[foldeer]` section of flymake.toml.

  1. Fills in pState->pFolderList
  2. Fills in initial pState->libs
  3. Fills in initial pState->incs

  @return   TRUE if worked, FALSE if failed
*///-----------------------------------------------------------------------------------------------
bool_t FmkTomlProcessFolders(flyMakeState_t *pState)
{
  tomlKey_t         key;
  char              szRule[8];
  flyMakeFolder_t  *pFolder   = NULL;
  const char       *szIter    = NULL;
  const char       *pszTable;
  const char       *szName;
  char             *szPath    = NULL;
  char             *psz;
  void             *hList     = NULL;
  int               pos;
  unsigned          i, j;
  bool_t            fWorked   = TRUE;

  FlyAssert(pState && pState->szRoot);

  if(pState->szTomlFile)
  {
    // look for [folders] section in flymake.toml file
    pszTable = FlyTomlTableFind(pState->szTomlFile, "folders");
    if(pszTable)
      szIter = FlyTomlKeyIter(pszTable, &key);

    // processs [folders] section in flymake.toml file
    while(szIter)
    {
      // if value is not a string, invalid
      if(key.type != TOML_STRING)
      {
        FlyMakeErrToml(pState, key.szValue, m_szFolderNotStr);
        fWorked = FALSE;
        break;
      }

      // get the rule
      FlyTomlStrCpy(szRule, key.szValue, sizeof(szRule));
      pos = FlyStrArrayFind(m_aszRules, szRule);
      if(pos < 0)
      {
        FlyMakeErrToml(pState, key.szValue, m_szRuleInvalid);
        fWorked = FALSE;
        break;
      }

      // create a folder structure based on TOML key, e.g. "folder/"
      pFolder = FmkFolderNew(pState->szRoot, key.szKey);
      if(!pFolder)
      {
        FlyMakeErrMem();
        fWorked = FALSE;
      }

      // if folder does not exist, do NOT include in list
      if(!FlyFileExistsFolder(pFolder->szFolder))
        FmkFolderFree(pFolder);

      // otherwise, link into folder list
      else
      {
        pFolder->rule = FMK_RULE_LIB + pos;
        pState->pFolderList = FlyListAppend(pState->pFolderList, pFolder);
      }

      // next key
      szIter = FlyTomlKeyIter(szIter, &key);
    }
  }

  // look for default folders lib, library, src, source, test
  szPath = FlyAlloc(strlen(pState->szRoot) + 3);
  if(szPath)
  {
    FmkMakePathFrom3(szPath, PATH_MAX, pState->szRoot, "", "*");
    hList = FlyFileListNew(szPath);
  }
  if(hList)
  {
    for(i = 0; fWorked && i < FlyFileListLen(hList); ++i)
    {
      szName = FlyFileListGetName(hList, i);
      if(FlyStrPathIsFolder(szName))
      {
        // look for inc/ or include/ folder
        psz = FlyStrPathNameLast(szName, NULL);
        for(j = 0; j < NumElements(m_aDefFolders); ++j)
        {
          if(strcmp(psz, m_aDefFolders[j].szFolder) == 0)
          {
            pFolder = FlyAllocZ(sizeof(*pFolder));
            if(!pFolder)
            {
              FlyMakeErrMem();
              fWorked = FALSE;
              break;
            }
            pFolder->szFolder = FlyStrClone(szName);
            pFolder->rule = m_aDefFolders[j].rule;
            pState->pFolderList = FlyListAppend(pState->pFolderList, pFolder);
          }
        }
      }
    }
    FlyFileListFree(hList);
    FlyFree(szPath);
  }

  // if no folders and there are source files in the root, it's a simple project
  if(pState->pFolderList == NULL)
  {
    hList = FlyMakeSrcListNew(pState->pCompilerList, pState->szRoot, 0);
    if(hList)
    {
      if(FlyMakeSrcListLen(hList) >= 0)
      {
        pState->fIsSimple = TRUE;
        pFolder = FlyAllocZ(sizeof(*pFolder));
        if(!pFolder)
        {
          FlyMakeErrMem();
          fWorked = FALSE;
        }
        else
        {
          pFolder->szFolder = FlyStrClone(pState->szRoot);
          pFolder->rule = FMK_RULE_LIB;
          pState->pFolderList = FlyListAppend(pState->pFolderList, pFolder);
        }
      }
      FlyFileListFree(hList);
    }
  }

  // initialize libraries, e.g. "lib/myproj.a" or "folder/folder.a"
  if(fWorked)
  {
    FlyStrSmartCpy(&pState->libs, "");
    pFolder = pState->pFolderList;
    while(pFolder)
    {
      if(pFolder->rule == FMK_RULE_LIB)
      {
        szPath = FlyMakeFolderAllocLibName(pState, pFolder->szFolder);
        if(szPath)
        {
          FlyStrSmartCat(&pState->libs, szPath);
          FlyStrSmartCat(&pState->libs, " ");
          FlyFree(szPath);
        }
      }
      pFolder = pFolder->pNext;
    }
  }

  // initialize include folders, e.g. ". " or ". inc/ "
  // always ends in a space. note: the "-I" is added later, as given in the flymake.toml file
  if(fWorked)
  {
    FlyStrSmartCat(&pState->incs, ". ");
    if(pState->szInc && (strcmp(pState->szInc, ".") != 0))
    {
      FlyStrSmartCat(&pState->incs, pState->szInc);
      FlyStrSmartCat(&pState->incs, " ");
    }
  }

  return fWorked;
}

/*-------------------------------------------------------------------------------------------------
  Process `[package]`, `[compiler]` and `[folders]` sections in optional flymake.toml

  Sets up the following fields in pState: szTomlFile, szProjName, szProjVer, szLibName, szSrcName,
  szArchiveFmt, pCompilerList, pFolderList.

  @param    pState    state, including dependencies
  @param    szName    NULL or preferred project name
  @return   TRUE if worked, FALSE if problem with flymake.toml file or allocating variables
*///-----------------------------------------------------------------------------------------------
bool_t FlyMakeTomlAlloc(flyMakeState_t *pState, const char *szName)
{
  bool_t    fWorked = TRUE;

  // FlyMakeTomlRootFill() must have been called prior to this
  FlyAssert(pState && pState->szRoot != NULL);

  FlyMakeDbgPrintf(FMK_DEBUG_MORE, "FlyMakeTomlAlloc(pState->szRoot=%s, szName=%s)\n", pState->szRoot, FlyStrNullOk(szName));

  pState->szTomlFilePath = FlyAlloc(strlen(pState->szRoot) + strlen(g_szTomlFile) + 1);
  if(pState->szTomlFilePath == NULL)
  {
    FlyMakeErrMem();
    fWorked = FALSE;
  }
  else
  {
    // if flymake.toml exists, reade it into memory, otherwise it's NULL
    strcpy(pState->szTomlFilePath, pState->szRoot);
    strcat(pState->szTomlFilePath, g_szTomlFile);
    pState->szTomlFile = FlyFileRead(pState->szTomlFilePath);
  }

  // called even if no flymake.toml for defaults, has custom error messages
  if(fWorked && !FmkTomlProcessPackage(pState, szName))
    fWorked = FALSE;

  // called even if no flymake.toml for defaults, has custom error messages
  if(fWorked && !FmkTomlProcessCompiler(pState))
    fWorked = FALSE;

  // called even if no flymake.toml for defaults, has custom error messages
  if(fWorked && !FmkTomlProcessFolders(pState))
    fWorked = FALSE;

  FlyMakeDbgPrintf(FMK_DEBUG_MORE, "  fWorked %u\n", fWorked);

  return fWorked;
}

/*-------------------------------------------------------------------------------------------------
  Free any root related strings (e.g. pState->szRoot, pState->szInc, etc...)
  @param    pState    current state for this project
  @return   none
*///-----------------------------------------------------------------------------------------------
void FmkRootFree(flyMakeState_t *pState)
{
  pState->szRoot      = FlyStrFreeIf(pState->szRoot);
  pState->szFullPath  = FlyStrFreeIf(pState->szFullPath);
  pState->szInc       = FlyStrFreeIf(pState->szInc);
  pState->szDepDir    = FlyStrFreeIf(pState->szDepDir);
}

/*-------------------------------------------------------------------------------------------------
  Allocate the full path for this folder.

  For example if szPath is "~/Work/folder", allocates "/Users/me/Work/folder"

  @param    szWorkingPath   a string buffer that can hold PATH_MAX bytes
  @param    szRootFolder    e.g. "~/some_folder", "./", ""
  @return   allocated full path, e.g. "/Users/me/some_folder/" or NULL if didn't work
*///-----------------------------------------------------------------------------------------------
static bool_t FmkGetFullRootPath(char *szPath, const char *szRootFolder)
{
  if(*szRootFolder == '\0')
    szRootFolder = ".";
  return FlyFileFullPath(szPath, szRootFolder) == 0 ? FALSE : TRUE;
}

/*-------------------------------------------------------------------------------------------------
  Fills in pState->szRoot, ->szFullPath, ->szInc, ->szDepDir.

  Assumes FlyMakeTomlRootFind() has been called successfully with this szRootFolder.

  Works with simple projects with source in the root, like:

      mylib.h
      mylib.c

  Works with flymake projects with include and source and/or library folders, like:

      flymake.toml
      inc/myproj.h
      lib/myproj_print.c
      src/myproj.c

  To be a root folder, must be one of the above 2 types. flymake.toml file is optional.

  @param    pState        current state for this project, to be filled in with "root strings"
  @param    szRootFolder  User path to file or folder or "."
  @return   TRUE if worked, FALSE if out of memory
*///-----------------------------------------------------------------------------------------------
bool_t FlyMakeTomlRootFill(flyMakeState_t *pState, const char *szRootFolder)
{
  char           *szPath;
  unsigned        size;
  unsigned        i;
  bool_t          fWorked = TRUE;

  // debugging
  FlyAssert(szRootFolder && (isslash(FlyStrCharLast(szRootFolder)) || *szRootFolder == '\0'));
  FlyMakeDbgPrintf(FMK_DEBUG_SOME, "FlyMakeTomlRootFill(pState %p, %s)\n", pState, szRootFolder);

  // special case: no need for ".", that is, search for "*" not "./*"
  if(strcmp(szRootFolder, ".") == 0 || strcmp(szRootFolder, "./") == 0)
    szRootFolder = "";
  pState->szRoot = FlyStrClone(szRootFolder);

  // make string for the path
  size = PATH_MAX;
  szPath = FlyAlloc(size);
  if(!szPath)
  {
    FlyMakeErrMem();
    fWorked = FALSE;
  }

  // get full path
  if(fWorked)
  {
    if(FmkGetFullRootPath(szPath, pState->szRoot))
      pState->szFullPath = FlyStrClone(szPath);
    if(!pState->szFullPath)
    {
      FlyMakeErrMem();
      fWorked = FALSE;
    }
  }

  // get main include folder
  if(fWorked)
  {
    for(i = 0; i < NumElements(m_aszInc); ++i)
    {
      // look to determine if there is a src/ or lib/ folder
      FmkMakePathFrom3(szPath, size, pState->szRoot, "", m_aszInc[i]);
      if(FlyFileExistsFolder(szPath))
          pState->szInc = FlyStrClone(szPath);
    }

    // no include folder, so assume root is main include folder
    if(!pState->szInc)
      pState->szInc = FlyStrClone(szRootFolder);
  }

  // determine main dependency folder, e.g. "deps/" or "../project/deps"
  if(fWorked)
  {
    size = strlen(pState->szRoot) + sizeof(m_szDepDir) + 3;
    pState->szDepDir = FlyAlloc(size);
    if(!pState->szDepDir)
    {
      FlyMakeErrMem();
      fWorked = FALSE;
    }
    else
    {
      strcpy(pState->szDepDir, pState->szRoot);
      strcat(pState->szDepDir, m_szDepDir);
    }
  }

  FlyMakeDbgPrintf(FMK_DEBUG_SOME, "  fWorked %u, szRoot \"%s\", szInc \"%s\", szDepDir \"%s\"\n",
    fWorked, FlyStrNullOk(pState->szRoot), FlyStrNullOk(pState->szInc), FlyStrNullOk(pState->szDepDir));

  // free the path variable
  FlyStrFreeIf(szPath);

  return fWorked;
}

/*-------------------------------------------------------------------------------------------------
  Given a path to a file or folder, find the project root folder.

  A valid project root is defined by (in order):

  1. flymake.toml file
  2. src/ or lib/ folder
  3. source files (e.g. .c or .c++) for simple projects (no folders, but perhaps inc/)
  4. Parent or grandparent from szPath is also checked for above, e.g. ".." and "../..".
  5. Empty szPath assumes current folder ".".

  Some valid input paths:

      ""
      "file.c"
      "folder/"
      "folder"
      "."
      "../"
      "~/myfolder/myfile.txt"
      "~/myfolder"
      "/Users/Documents/me/git/my_project/"

  @param    szPath          Path to file or folder
  @param    pCompilerList   List of source file extensions
  @return   Allocated root path or NULL if not found
*///-----------------------------------------------------------------------------------------------
char * FlyMakeTomlRootFind(const char *szPath, const flyMakeCompiler_t *pCompilerList, fmkErr_t *pErr)
{
  void           *hList;
  char           *szRoot      = NULL;   // returned value
  char           *szFolder    = NULL;   // foler to check in user form, e.g. "~/folder/"
  char           *szWildPath  = NULL;
  const char     *szName;
  const char     *szExt;
  unsigned        size;
  unsigned        len;
  unsigned        i, j, k;
  fmkErr_t        err         = FMK_ERR_NONE;
  bool_t          fWorked     = TRUE;

  FlyMakeDbgPrintf(FMK_DEBUG_SOME, "FlyMakeTomlRootFind(%s)\n", szPath);

  // allocate strings
  szFolder = FlyMakeFolderAlloc(szPath, &err);
  if(err)
    fWorked = FALSE;
  else
  {
    size   = PATH_MAX;
    szWildPath = FlyAlloc(PATH_MAX);
    if(!szWildPath)
    {
      FlyMakeErrMem();
      fWorked = FALSE;
    }
  }

  // check for flymake.toml and src/ or lib/ in current folder, parent and grandparent
  // e.g. "", "../" and "../../", or "~/folder/subfolder/", "~/folder/", "~/"
  if(fWorked)
  {
    FlyStrZCpy(szWildPath, szFolder, size);
    for(i = 0; szRoot == NULL && i < 3; ++i)
    {
      if(i > 0)
        FlyStrPathParent(szWildPath, size); // parent or grandparent
      len = strlen(szWildPath);
      FlyMakeDbgPrintf(FMK_DEBUG_MORE, "  checking folder: %s for root\n", szWildPath);

      // look for root indicators, e.g. "src/" or "flymake.toml"
      FlyStrPathAppend(szWildPath, "*", size);
      hList = FlyFileListNew(szWildPath);
      if(hList)
      {
        // check through filenames to find any matches that make it a "root", e.g.g "flymake.toml" or "src/"
        for(j = 0; !szRoot && j < FlyFileListLen(hList); ++j)
        {
          for(k = 0; k < NumElements(m_aszRoot); ++k)
          {
            szName = FlyStrPathNameLast(FlyFileListGetName(hList, j), NULL);
            if(strcmp(m_aszRoot[k], szName) == 0)
            {
              szWildPath[len] = '\0';
              szRoot = FlyStrClone(szWildPath);
              break;
            }
          }
        }
        FlyFileListFree(hList);
      }
      szWildPath[len] = '\0';
    }
  }

  // check for source files, but only in the given folder, don't look in parent/grandparent folders
  if(fWorked && !szRoot && pCompilerList)
  {
    FlyMakeDbgPrintf(FMK_DEBUG_MORE, "  checking for simple project: %s\n", szFolder);
    FlyStrZCpy(szWildPath, szFolder, size);
    FlyStrPathAppend(szWildPath, "*", size);
    hList = FlyFileListNew(szWildPath);
    if(hList)
    {
      // printf("  hList[0].name `%s`, hList.len %u\n", FlyFileListGetName(hList, 0), FlyFileListLen(hList));
      for(i = 0; i < FlyFileListLen(hList); ++i)
      {
        szExt = FlyStrPathExt(FlyFileListGetName(hList, i));
        if(szExt && FlyMakeCompilerFind(pCompilerList, szExt))
        {
          szRoot = FlyStrClone(szFolder);
          break;
        }
      }
    }
  }

  if(!err && !szRoot)
    err = FMK_ERR_NOT_PROJECT;
  if(pErr)
    *pErr = err;

  FlyMakeDbgPrintf(FMK_DEBUG_MORE, "  szRoot '%s' err %u\n", FlyStrNullOk(szRoot), err);

  // cleanup
  FlyStrFreeIf(szWildPath);
  FlyStrFreeIf(szFolder);

  return szRoot;
}
