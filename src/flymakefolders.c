/**************************************************************************************************
  flymakefolders.c - basically the flymake "new" command
  Copyright 2024 Drew Gislason
  license: <https://mit-license.org>
**************************************************************************************************/
#include "flymake.h"

static const char m_szFmtFileReadMe[] =
  "# README for project %s\n"
  "\n"
  "Written in markdown. See <https://www.markdownguide.org/basic-syntax/>\n"
  "\n"
  "## Project Folder Tree\n"
  "\n"
  "```\n";

static const char m_szFmtFileReadMeDocs[] = 
  "docs    Documents such as user manual found here\n";

static const char m_szFmtFileReadMeInc[] = 
  "inc     Public API include files\n";

static const char m_szFmtFileReadMeLib[] = 
  "lib     Package (library) source code\n";

static const char m_szFmtFileReadMeSrc[] = 
  "src     Project source code\n";

static const char m_szFmtFileReadMeTest[] =
  "test    Test suite source code\n";

static const char m_szFmtFileReadMeEnd[] =
  "```\n";

static const char m_szFmtFileLicense[] =
  "MIT License <https://mit-license.org>\n"
  "\n"
  "Permission is hereby granted, free of charge, to any person obtaining a copy of this software and\n"
  "associated documentation files (the \"Software\"), to deal in the Software without restriction,\n"
  "including without limitation the rights to use, copy, modify, merge, publish, distribute,\n"
  "sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is\n"
  "furnished to do so, subject to the following conditions:\n"
  "\n"
  "The above copyright notice and this permission notice shall be included in all copies or\n"
  "substantial portions of the Software.\n"
  "\n"
  "THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT\n"
  "NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND\n"
  "NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,\n"
  "DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,\n"
  "OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.\n";

static const char m_szFmtApiGuide[] =
  "# API Guide for %s\n"
  "\n"
  "## print_hello\n"
  "\n"
  "Prints a \"hello foo!\", where foo is replaced by the given string. Also allocates and returns\n"
  "the given string possibly prepended by \"(debug) \".\n"
  "\n"
  "### Prototype\n"
  "\n"
  "```\n"
  "char * print_hello(const char *sz)\n"
  "\n"
  "@param  sz   a string to allocate and print\n"
  "@return allocated string or NULL if failed.\n"
  "```\n"
  "\n";

// inc/projname.h
static const char m_szFmtFileH[] = 
  "/*\n"
  "  Project wide types and defines go here.\n"
  "*/\n"
  "#include <stdio.h>\n"
  "#include <stdlib.h>\n"
  "#include <string.h>\n"
  "\n"
  "#ifndef %s_H\n"
  "#define %s_H\n"
  "\n"
  "// allows source to be compiled with C or C++ compilers\n"
  "#ifdef __cplusplus\n"
  "  extern \"C\" {\n"
  "#endif\n"
  "\n"
  "#define SZ_PROJ_NAME \"%s\"\n"
  "\n"
  "#ifndef DEBUG\n"
  "  #define DEBUG    0\n"
  "#endif\n"
  "\n"
  "#if DEBUG\n"
  "  #define SZ_DEBUG \"(debug) \"\n"
  "#else\n"
  "  #define SZ_DEBUG \"\"\n"
  "#endif\n"
  "\n"
  "char * print_hello(const char *sz);\n"
  "\n"
  "#ifdef __cplusplus\n"
  "  }\n"
  "#endif\n"
  "\n"
  "#endif // %s_H\n";

// either in lib/projname_print.c or src/projname_print.c
static const char m_szFmtFileLib[] =
  "/*\n"
  "  hello world example\n"
  "*/\n"
  "#include \"%s.h\"\n"
  "\n"
  "char * print_hello(const char *sz)\n"
  "{\n"
  "  const char szDebug[] = SZ_DEBUG;\n"
  "  char * psz           = NULL;\n"
  "\n"
  "  // create new string with debug string + user given string\n"
  "  psz = malloc(sizeof(szDebug) + strlen(sz));\n"
  "  if(psz)\n"
  "  {\n"
  "    strcpy(psz, szDebug);\n"
  "    strcat(psz, sz);\n"
  "    printf(\"hello %s!\\n\", psz);\n"
  "  }\n"
  "\n"
  "  return psz;\n"
  "}\n";

// src/projname.c
static const char m_szFmtFileMain[] =
  "/*\n"
  "  main program\n"
  "*/\n"
  "#include \"%s.h\"\n"
  "\n"
  "\n"
  "int main(int argc, const char *argv[])\n"
  "{\n"
  "  const char *szProjName = SZ_PROJ_NAME;\n"
  "  print_hello(szProjName);\n"
  "  return 0;\n"
  "}\n";

// test/test_projname.c
static const char m_szFmtFileTest[] =
  "/*\n"
  "  test cases go here.\n"
  "*/\n"
  "#include \"%s.h\"\n"
  "\n"
  "int main(int argc, const char *argv[])\n"
  "{\n"
  "  char       *szResult;\n"
  "  const char szExpectedResult[] = SZ_DEBUG SZ_PROJ_NAME;\n"
  "  int        retCode = 0;\n"
  "\n"
  "  szResult = print_hello(SZ_PROJ_NAME);\n"
  "  if(szResult == NULL || strcmp(szResult, szExpectedResult) != 0)\n"
  "  {\n"
  "    printf(\"test failed\\n\");\n"
  "    retCode = 1;\n"
  "  }\n"
  "  else\n"
  "    printf(\"test passed\\n\");\n"
  "\n"
  "  return retCode;\n"
  "}\n";

// inc/projname.hpp
static const char m_szFmtFileHpp[] = 
  "/*\n"
  "  Project wide types and defines go here.\n"
  "*/\n"
  "\n"
  "#ifndef %s_HPP\n"
  "#define %s_HPP\n"
  "\n"
  "#define SZ_PROJ_NAME \"%s\"\n"
  "\n"
  "#ifndef DEBUG\n"
  "  #define DEBUG    0\n"
  "#endif\n"
  "\n"
  "#if DEBUG\n"
  "  #define SZ_DEBUG \"(debug) \"\n"
  "#else\n"
  "  #define SZ_DEBUG \"\"\n"
  "#endif\n"
  "\n"
  "/*!\n"
  "  @class Car A class for greeting\n"
  "\n"
  "  Greet the user with special greeting.\n"
  "*/\n"
  "class MyClass {\n"
  "  public:\n"
  "    string greeting;\n"
  "\n"
  "    /*!\n"
  "      Constructor for MyClass\n"
  "      @param    _greeting    greeting to use\n"
  "    */\n"
  "    MyClass(string _greeting) {\n"
  "      greeting = _greeting;\n"
  "    }\n"
  "\n"
  "    void greet(string who);\n"
  "};\n"
  "\n"
  "#endif // %s_HPP\n";


// either in lib/projname_greet.cpp or src/projname_greet.cpp
static const char m_szFmtFileCppLib[] =
  "#include <iostream>\n"
  "using namespace std;\n"
  "#include \"%s.hpp\"\n"
  "\n"
  "/*!\n"
  "  Greet someone with our standard greeting\n"
  "*/\n"
  "void MyClass::greet(string who)\n"
  "{\n"
  "  // for project %s\n"
  "  cout << SZ_DEBUG << this->greeting << \" \" << who << \"!\\n\";\n"
  "}\n";

// src/projname.cpp
static const char m_szFmtFileCppMain[] =
  "#include <iostream>\n"
  "using namespace std;\n"
  "#include \"%s.hpp\"\n"
  "\n"
  "int main() {\n"
  "  MyClass hello(\"Hello\");\n"
  "\n"
  "  cout << \"c++: \";\n"
  "  hello.greet(SZ_PROJ_NAME);\n"
  "\n"
  "  return 0;\n"
  "}\n";

// test/test_projname.cpp
static const char m_szFmtFileCppTest[] =
  "#include <iostream>\n"
  "using namespace std;\n"
  "#include \"%s.hpp\"\n"
  "\n"
  "int main() {\n"
  "  string  answer;\n"
  "  MyClass hello(\"Hello\");\n"
  "  int     ret;\n"
  "\n"
  "  cout << \"c++: \";\n"
  "  hello.greet(SZ_PROJ_NAME);\n"
  "  cout << \"\\nDid the greeting appear? \";\n"
  "  cin >> answer;\n"
  "  if(answer[0] == 'Y' || answer[0] == 'y')\n"
  "  {\n"
  "    ret = 0;\n"
  "    cout << \"Passed\\n\";\n"
  "  }\n"
  "  else\n"
  "  {\n"
  "    cout << \"Failed\\n\";\n"
  "    ret = 1;\n"
  "  }\n"
  "\n"
  "  return ret;\n"
  "}\n";

/*-------------------------------------------------------------------------------------------------
  Execute the system command.

  @param    szCmdline      command to execute
  @param    pOpts          options like verbose, fNoBuild
  @return   0 if worked, -1 if failed
*///-----------------------------------------------------------------------------------------------
int FlyMakeSystem(fmkVerbose_t verbose, flyMakeOpts_t *pOpts, const char *szCmdline)
{
  int ret = 0;

  if(pOpts->verbose >= verbose)
    FlyMakePrintf("%s\n", szCmdline);
  if(!pOpts->fNoBuild)
  {
    ret = system(szCmdline);
    if(ret != 0)
      ret = -1;
  }

  return ret;
}

/*-------------------------------------------------------------------------------------------------
  Create the folder if not already created. Also displays shell script line for creating folder.

  @param    fNoBuild          Don't actually craete the folder
  @param    szFolder          folder e.g. "tools/" or "test/"
  @return   TRUE if worked, FALSE if bad path
*///-----------------------------------------------------------------------------------------------
bool_t FlyMakeFolderCreate(flyMakeOpts_t *pOpts, const char *szFolder)
{
  char       *szExpandedFolder = NULL;
  unsigned    size;
  bool_t      fWorked = TRUE;

  if(pOpts->fNoBuild || pOpts->verbose >= FMK_VERBOSE_MORE)
    FlyMakePrintfEx(FMK_VERBOSE_SOME, "if test ! -d %s; then mkdir %s; fi\n", szFolder, szFolder);
  if(!pOpts->fNoBuild)
  {
    // expand home folder if needed
    if(*szFolder == '~' && (isslash(szFolder[1]) || (szFolder[1] == '\0')))
    {
      size = strlen(szFolder) + FlyFileHomeGetLen() + 10;
      szExpandedFolder = FlyAlloc(size);
      if(szExpandedFolder)
      {
        strcpy(szExpandedFolder, szFolder);
        FlyFileHomeExpand(szExpandedFolder, size);
        FlyMakeDbgPrintf(FMK_DEBUG_MORE, "szExpandedFolder %s\n", szExpandedFolder);
      }
    }

    // no need to make it if it already exists
    if(!FlyFileExistsFolder(szExpandedFolder ? szExpandedFolder : szFolder))
    {
      if(FlyFileMakeDir(szExpandedFolder ? szExpandedFolder : szFolder) < 0)
      {
        FlyMakePrintf("error: failed to mkdir %s\n", szFolder);
        fWorked = FALSE;
      }
    }
    FlyStrFreeIf(szExpandedFolder);
  }

  return fWorked;
}

/*-------------------------------------------------------------------------------------------------
  Force remove an entire folder tree.

  @param    pOpts       ptr to print options
  @param    szFolder    folder name, e.g. "deps/"
  @return   TRUE if worked, FALSE if no memory
*///-----------------------------------------------------------------------------------------------
bool_t FlyMakeFolderRemove(fmkVerbose_t verbose, flyMakeOpts_t *pOpts, const char *szFolder)
{
  static const char   szRmDir[] = "rm -rf ";
  flyStrSmart_t      *pCmdline;
  bool_t              fWorked = TRUE;

  pCmdline = FlyStrSmartAlloc(sizeof(szRmDir) + strlen(szFolder));
  if(!pCmdline)
    fWorked = FALSE;
  else
  {
    // remove folder, just in case garbage is in folder we want
    FlyStrSmartCpy(pCmdline, szRmDir);
    FlyStrSmartCat(pCmdline, szFolder);
    FlyMakeSystem(verbose, pOpts, pCmdline->sz);
    FlyStrSmartFree(pCmdline);
  }

  return fWorked;
}

/*-------------------------------------------------------------------------------------------------
  Create a file with a formatted string and set of 0 or more string args (up to 4).

  @param    szFname     filename (potentially with path)
  @param    szFmt       contents of file, with possible string options
  @param    nStrings    # of strings (0-4)
  @param    aszStrings  strings needed by szFmt
  @return   TRUE if created file, FALSE if not
*///-----------------------------------------------------------------------------------------------
static bool_t FmkCreateFmtFile(const char *szFName, const char szFmt[], unsigned nStrings, const char *aszStrings[])
{
  char     *szContents;
  size_t    len;
  unsigned  i;
  bool_t    fWorked = FALSE;

  FlyAssert(nStrings <= 4);

  len = strlen(szFmt);
  for(i = 0; i < nStrings; ++i)
    len += strlen(aszStrings[i]);
  szContents = FlyAlloc(len + 1);
  if(szContents)
  {
    if(nStrings == 0)
      strcpy(szContents, szFmt);
    else if(nStrings == 1)
      len = snprintf(szContents, len, szFmt, aszStrings[0]);
    else if(nStrings == 2)
      len = snprintf(szContents, len, szFmt, aszStrings[0], aszStrings[1]);
    else if(nStrings == 3)
      len = snprintf(szContents, len, szFmt, aszStrings[0], aszStrings[1], aszStrings[2]);
    else if(nStrings >= 4)
      len = snprintf(szContents, len, szFmt, aszStrings[0], aszStrings[1], aszStrings[2], aszStrings[3]);
    szContents[len] = '\0';

    fWorked = FlyFileWrite(szFName, szContents);

    FlyFree(szContents);
  }

  return fWorked;
}

/*-------------------------------------------------------------------------------------------------
  Allocate projname.h file with all the proper trimmings

  @param    szProjName
  @return   allocated H file or NULL if problem
*///-----------------------------------------------------------------------------------------------
static bool_t FmkCreateHFile(const char *szFilename, const char *szProjName, const char *szFmtFile)
{
  char         *szCapsProj;
  const char   *aszStrings[4];
  unsigned      size;
  bool_t        fWorked = TRUE;


  // for #ifdef in .h file, UPPERCASE the #ifndef PROJNAME_H
  size = strlen(szProjName) + 1;
  szCapsProj = FlyAlloc(size);
  if(!szCapsProj)
    fWorked = FALSE;
  else
  {
    strcpy(szCapsProj, szProjName);
    FlyStrToCase(szCapsProj, szProjName, size, IS_UPPER_CASE);
  }

  // create the file
  if(fWorked)
  {
    // make sure we account for each %s, that is m_szFmtFileH must match code
    FlyAssert(FlyStrCount(szFmtFile, "%s") == 4);
    aszStrings[0] = szCapsProj;
    aszStrings[1] = szCapsProj;
    aszStrings[2] = szProjName;
    aszStrings[3] = szCapsProj;
    if(!FmkCreateFmtFile(szFilename, szFmtFile, 4, aszStrings))
      fWorked = FALSE;
  }

  return fWorked;
}

/*-------------------------------------------------------------------------------------------------
  Create a subfolder

  @param    pOpts       flymake options
  @param    szPath      working path to create sub folder in, PATH_MAX in size
  @param    szFolder    ptr to folder name
  @return   TRUE if worked, FALSE if failed
*///-----------------------------------------------------------------------------------------------
static bool_t FmkCreateSubFolder(flyMakeOpts_t *pOpts, char *szPath, const char *szSubFolder)
{
  bool_t        fWorked = TRUE;

  FlyStrPathAppend(szPath, szSubFolder, PATH_MAX);
  if(FlyMakeFolderCreate(pOpts, szPath) < 0)
    fWorked = FALSE;
  FlyStrPathParent(szPath, PATH_MAX);

  return fWorked;
}

/*-------------------------------------------------------------------------------------------------
  Command: flymake new projname [--lib]

  Jobs:

  1. Verify the folder can be created and is not inside another project (ask)
  2. Creates standard subfolders, e.g. inc/ src/ or lib/ test/ etc..
  3. Creates default files, e.g. README.md, flymake.toml
  4. Creates sample program so `flymake run` or `flymake test works`

  @param    pState    cmdline options, etc...
  @return   TRUE if worked, FALSE if failed
*///-----------------------------------------------------------------------------------------------
bool_t FlyMakeCreateStdFolders(flyMakeState_t *pState, const char *szFolder)
{
  // make sure these arrays and constants match
  static const char *aszFolders[]    = { "docs/", "inc/", "lib/", "src/", "test/"};
  static bool_t      aAddFolder[]    = { FALSE,    TRUE,  FALSE,   TRUE,  FALSE };
  static const char *aAddReadMe[]    = { m_szFmtFileReadMeDocs, m_szFmtFileReadMeInc,
                                         m_szFmtFileReadMeLib, m_szFmtFileReadMeSrc,
                                         m_szFmtFileReadMeTest };
  #define INDEX_DOCS 0
  #define INDEX_INC  1
  #define INDEX_LIB  2
  #define INDEX_SRC  3
  #define INDEX_TEST 4

  const char   *szFmtToml       = FlyMakeTomlFmtFileDefault();
  char         *szProjName      = NULL;
  char         *szFullPath      = NULL;
  char         *psz;
  char         *szRootFolder;
  char         *szReadMe;
  const char   *aszStrings[4];
  unsigned      len             = 0;
  unsigned      size;
  unsigned      i;
  bool_t        fFolder;
  fmkErr_t      err             = FMK_ERR_NONE;
  char          szAsk[8];

  // make sure the related arrays match
  FlyAssert(NumElements(aszFolders) == NumElements(aAddFolder));

  // don't create a folder if already exists
  if(FlyFileExists(szFolder, &fFolder))
  {
    FlyMakePrintf("error: %s %s already exists\n", fFolder ? "folder" : "file", szFolder);
    err = FMK_ERR_CUSTOM;
  }

  // create project name from folder path
  if(!err)
  {
    len = 0;
    psz = FlyStrPathNameLast(szFolder, &len);
    if(psz && len)
    {
      szProjName = FlyAlloc(len + 1);
      if(szProjName)
      {
        strncpy(szProjName, psz, len);
        szProjName[len] = '\0';
      }
    }
    if(szProjName == NULL)
    {
      FlyMakePrintf("error: invalid project name %s\n", szFolder);
      err = FMK_ERR_CUSTOM;
    }
  }

  // don't create a project within a project without approval
  if(!err)
  {
    // if parent is in a project, warn user
    szRootFolder = FlyMakeTomlRootFind(szFolder, pState->pCompilerList, NULL);
    if(szRootFolder)
    {
      FlyFree(szRootFolder);
      FlyMakePrintf("warning: folder %s appears to be in a project.\n", szFolder);
      FlyStrAsk(szAsk, "Are you sure you want to create a project within a project?", sizeof(szAsk));
      if(toupper(*szAsk) != 'Y')
      {
        FlyMakePrintf("Aborting...\n");
        err = FMK_ERR_CUSTOM;
      }
    }
  }

  // create project root folder
  if(!err)
  {
    FlyMakePrintfEx(FMK_VERBOSE_SOME, "# Creating folders...\n");
    if(!FlyMakeFolderCreate(&pState->opts, szFolder))
      err = FMK_ERR_BAD_PATH;
  }

  // allocate a working path string
  if(!err)
  {
    szFullPath = FlyAlloc(PATH_MAX);
    if(szFullPath == NULL)
      err = FlyMakeErrMem();
  }

  // create subfolders
  if(!err)
  {
    // determine which folders will be created
    if(pState->opts.fAll)
    {
      for(i = 0; i < NumElements(aszFolders); ++i)
        aAddFolder[i] = TRUE;
    }
    else if(pState->opts.fLib)
    {
      aAddFolder[INDEX_LIB]  = TRUE;   // lib/
      aAddFolder[INDEX_SRC]  = FALSE;  // src/
      aAddFolder[INDEX_TEST] = TRUE;   // test/
    }

    for(i = 0; !err && i < NumElements(aszFolders); ++i)
    {
      FlyStrZCpy(szFullPath, szFolder, PATH_MAX);
      if(aAddFolder[i])
      {
        if(!FmkCreateSubFolder(&pState->opts, szFullPath, aszFolders[i]))
          err = FMK_ERR_WRITE;
      }
    }
  }

  // create LICENSE.txt in root
  if(!err)
  {
    if(pState->opts.verbose)
      FlyMakePrintf("\n# Creating files...\n");
    FlyStrZCpy(szFullPath, szFolder, PATH_MAX);
    FlyStrPathAppend(szFullPath, "LICENSE.txt", PATH_MAX);
    FlyMakePrintfEx(FMK_VERBOSE_SOME, "%s\n", szFullPath);
    if(!FlyFileWrite(szFullPath, m_szFmtFileLicense))
      err = FMK_ERR_WRITE;
  }

  // create README.md in root
  if(!err)
  {
    FlyStrZCpy(szFullPath, szFolder, PATH_MAX);
    FlyStrPathAppend(szFullPath, "README.md", PATH_MAX);
    FlyMakePrintfEx(FMK_VERBOSE_SOME, "%s\n", szFullPath);

    size = sizeof(m_szFmtFileReadMe) + sizeof(m_szFmtFileReadMeDocs) +
           sizeof(m_szFmtFileReadMeInc) + sizeof(m_szFmtFileReadMeLib) +
           sizeof(m_szFmtFileReadMeSrc) + sizeof(m_szFmtFileReadMeTest) +
           sizeof(m_szFmtFileReadMeEnd) + strlen(szProjName);
    szReadMe = FlyAlloc(size);
    if(!szReadMe)
      err = FlyMakeErrMem();
    else
    {
    // expected 3 formatted strings in README.md
      FlyAssert(FlyStrCount(m_szFmtFileReadMe, "%s") == 1);
      aszStrings[0] = szProjName;

      FlyStrZCpy(szReadMe, m_szFmtFileReadMe, size);
      for(i = 0; i < NumElements(aAddReadMe); ++i)
      {
        if(aAddFolder[i])
          FlyStrZCat(szReadMe, aAddReadMe[i], size);
      }
      FlyStrZCat(szReadMe, m_szFmtFileReadMeEnd, size);

      if(!FmkCreateFmtFile(szFullPath, szReadMe, 1, aszStrings))
        err = FMK_ERR_WRITE;
      FlyFree(szReadMe);
    }
  }

  // create flymake.toml in root
  if(!err)
  {
    FlyStrZCpy(szFullPath, szFolder, PATH_MAX);
    FlyStrPathAppend(szFullPath, g_szTomlFile, PATH_MAX);
    FlyMakePrintfEx(FMK_VERBOSE_SOME, "%s\n", szFullPath);

    // expected 1 formatted string in flymake.toml
    FlyAssert(FlyStrCount(szFmtToml, "%s") == 1);
    aszStrings[0] = szProjName;
    if(!FmkCreateFmtFile(szFullPath, szFmtToml, 1, aszStrings))
      err = FMK_ERR_WRITE;
  }

  // create docs/api_guide.
  if(!err && aAddFolder[INDEX_DOCS])
  {
    FlyStrZCpy(szFullPath, szFolder, PATH_MAX);
    FlyStrPathAppend(szFullPath, aszFolders[INDEX_DOCS], PATH_MAX);
    FlyStrPathAppend(szFullPath, "api_guide.md", PATH_MAX);
    FlyMakePrintfEx(FMK_VERBOSE_SOME, "%s\n", szFullPath);

    // expected 1 formatted string in flymake.toml
    FlyAssert(FlyStrCount(m_szFmtApiGuide, "%s") == 1);
    aszStrings[0] = szProjName;
    if(!FmkCreateFmtFile(szFullPath, m_szFmtApiGuide, 1, aszStrings))
      err = FMK_ERR_WRITE;
  }

  // create inc/projname.h file
  if(!err)
  {
    // get path for .h file
    FlyStrZCpy(szFullPath, szFolder, PATH_MAX);
    FlyStrPathAppend(szFullPath, aszFolders[INDEX_INC], PATH_MAX);
    FlyStrZCat(szFullPath, szProjName, PATH_MAX);
    FlyStrZCat(szFullPath, pState->opts.fCpp ? ".hpp" : ".h", PATH_MAX);
    FlyMakePrintfEx(FMK_VERBOSE_SOME, "%s\n", szFullPath);
    FlyAssert(FlyStrCount(m_szFmtFileH, "%s") == 4);
    FlyAssert(FlyStrCount(m_szFmtFileHpp, "%s") == 4);
    if(!FmkCreateHFile(szFullPath, szProjName, pState->opts.fCpp ? m_szFmtFileHpp : m_szFmtFileH))
      err = FMK_ERR_WRITE;
  }

  // create main src/projname.c or src/projname.cpp
  if(!err && aAddFolder[INDEX_SRC])
  {
    FlyStrZCpy(szFullPath, szFolder, PATH_MAX);
    FlyStrPathAppend(szFullPath, aszFolders[INDEX_SRC], PATH_MAX);
    FlyStrZCat(szFullPath, szProjName, PATH_MAX);
    FlyStrZCat(szFullPath, pState->opts.fCpp ? ".cpp" : ".c", PATH_MAX);
    FlyMakePrintfEx(FMK_VERBOSE_SOME, "%s\n", szFullPath);

    FlyAssert(FlyStrCount(m_szFmtFileMain, "%s") == 1);
    FlyAssert(FlyStrCount(m_szFmtFileCppMain, "%s") == 1);
    aszStrings[0] = szProjName;
    if(!FmkCreateFmtFile(szFullPath, pState->opts.fCpp ? m_szFmtFileCppMain : m_szFmtFileMain, 1, aszStrings))
      err = FMK_ERR_WRITE;
  }

  // create src/projname_print.c or lib/projname_print.c
  if(!err)
  {
    FlyStrZCpy(szFullPath, szFolder, PATH_MAX);
    if(aAddFolder[INDEX_LIB])
      FlyStrPathAppend(szFullPath, aszFolders[INDEX_LIB], PATH_MAX);
    else
      FlyStrPathAppend(szFullPath, aszFolders[INDEX_SRC], PATH_MAX);
    FlyStrZCat(szFullPath, szProjName, PATH_MAX);
    FlyStrZCat(szFullPath, pState->opts.fCpp ? "_print.cpp" : "_print.c", PATH_MAX);
    FlyMakePrintfEx(FMK_VERBOSE_SOME, "%s\n", szFullPath);

    FlyAssert(FlyStrCount(m_szFmtFileLib, "%s") == 2);
    FlyAssert(FlyStrCount(m_szFmtFileCppLib, "%s") == 2);
    aszStrings[0] = szProjName;
    aszStrings[1] = pState->opts.fCpp ? szProjName : "%s";
    if(!FmkCreateFmtFile(szFullPath, pState->opts.fCpp ? m_szFmtFileCppLib : m_szFmtFileLib, 2, aszStrings))
      err = FMK_ERR_WRITE;
  }

  // create test/test_projname if --lib or --all flag
  if(!err && aAddFolder[INDEX_TEST])
  {
    FlyStrZCpy(szFullPath, szFolder, PATH_MAX);
    FlyStrPathAppend(szFullPath, "test/test_", PATH_MAX);
    FlyStrZCat(szFullPath, szProjName, PATH_MAX);
    FlyStrZCat(szFullPath, pState->opts.fCpp ? ".cpp" : ".c", PATH_MAX);
    FlyMakePrintfEx(FMK_VERBOSE_SOME, "%s\n", szFullPath);

    FlyAssert(FlyStrCount(m_szFmtFileTest, "%s") == 1);
    FlyAssert(FlyStrCount(m_szFmtFileCppTest, "%s") == 1);
    aszStrings[0] = szProjName;
    if(!FmkCreateFmtFile(szFullPath, pState->opts.fCpp ? m_szFmtFileCppTest : m_szFmtFileTest, 1, aszStrings))
      err = FMK_ERR_WRITE;
  }

  if(err)
    FlyMakePrintErr(err, szFullPath);

  if(szFullPath)
    FlyFree(szFullPath);

  return err ? FALSE : TRUE;
}
