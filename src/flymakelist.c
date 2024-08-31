/**************************************************************************************************
  flymakelist.c - lists of source files
  Copyright 2024 Drew Gislason
  license: <https://mit-license.org>
**************************************************************************************************/
#include "flymake.h"
#include "FlySort.h"

#define SRCLIST_SANCHK 9979

typedef struct
{
  void         *hList;    // list of source files
  bool_t       *pfUsed;   // for marking off when making tool lists
  unsigned      sanchk;
  unsigned      len;
} fmkSrcList_t;

/*-------------------------------------------------------------------------------------------------
  Is this a src list?

  @param  _pSrcList    ptr to srclist allocated with FlyMakeSrcListNew()
  @return TRUE if this is a src list
*///-----------------------------------------------------------------------------------------------
bool_t FlyMakeSrcListIs(void *pSrcList_)
{
  fmkSrcList_t  *pSrcList = pSrcList_;
  return (pSrcList && pSrcList->sanchk == SRCLIST_SANCHK) ? TRUE : FALSE;
}

/*-------------------------------------------------------------------------------------------------
  Create a new list of 0 or more source files based on a folder tree and file extensions.

  Only files with extensions found in the pCompilerList are included. All other files and all
  folders are NOT in list.

  The results of the list are in sorted order.

  Returns a handle even if there are no source files so that the caller can differentiate between a
  bad path and no source files in the folder tree.

  Example use:

      void *hList = FlyMakeSrcListNew(pCompilerList, "folder/");
      
      if(!hList)
        printf("bad path\n");
      else if(FlyMakeSrcListLen(hList) == 0)
        printf("no files\n");
      else
        FlyMakeSrcListPrint(hList);

  @param    pCompilerList   contains source file extensions, e.g. ".c" or ".cpp.c++"
  @param    szFolder        folder to check for source files, e.g. "", "src/", "../lib/"
  @param    depth           0-n, how many subfolders deep to add to source list
  @return   handle to source list or NULL if bad folder path
*///-----------------------------------------------------------------------------------------------
void * FlyMakeSrcListNew(flyMakeCompiler_t *pCompilerList, const char *szFolder, unsigned depth)
{
  void         *hList;
  char         *szExtList;
  fmkSrcList_t *pSrcList    = NULL;
  bool_t        fWorked     = TRUE;

  FlyMakeDbgPrintf(FMK_DEBUG_SOME, "FlyMakeSrcListNew(%s,%u)\n", szFolder, depth);

  // Get the extension list from the compilers
  szExtList = FlyMakeCompilerAllExts(pCompilerList);
  if(!szExtList)
    fWorked = FALSE;
  FlyMakeDbgPrintf(FMK_DEBUG_MORE, "  szExtList %s\n", FlyStrNullOk(szExtList));

  // look for only source files
  if(fWorked)
  {
    hList = FlyFileListNewExts(szFolder, szExtList, depth);
    FlyMakeDbgPrintf(FMK_DEBUG_MORE, "  hList %p, len %u\n", hList, FlyFileListLen(hList));
    if(!hList)
      fWorked = FALSE;
    else
      FlyFileListSort(hList, NULL);
  }

  // allocate srcList and source file array
  if(fWorked)
  {
    pSrcList = FlyAllocZ(sizeof(*pSrcList));
    if(!pSrcList)
      fWorked = FALSE;
    else
    {
      pSrcList->sanchk  = SRCLIST_SANCHK;
      pSrcList->hList   = hList;
      pSrcList->len     = FlyFileListLen(hList);
      pSrcList->pfUsed  = FlyAllocZ(sizeof(bool_t) * pSrcList->len);
      if(!pSrcList->pfUsed)
        fWorked = FALSE;
    }
  }


  FlyFreeIf(szExtList);
  if(!fWorked)
  {
    if(pSrcList)
      FlyMakeSrcListFree(pSrcList);
    else if(hList)
      FlyFileListFree(hList);
  }

  FlyMakeDbgPrintf(FMK_DEBUG_SOME, "  fWorked %u\n", fWorked);
  if(FlyMakeDebug() >= FMK_DEBUG_MAX)
    FlyMakeSrcListPrint(pSrcList);

  return (void *)pSrcList;
}

/*-------------------------------------------------------------------------------------------------
  Get the source list entry 0-(n-1), where n is 

  @param    hSrcList   returned value from FlyMakeSrcListNew()
  @
  @return   source file name or NULL if no source files
*///-----------------------------------------------------------------------------------------------
const char * FlyMakeSrcListGetName(void *hSrcList, unsigned i)
{
  fmkSrcList_t   *pSrcList    = hSrcList;
  const char     *szFileName  = NULL;
  if(FlyMakeSrcListIs(hSrcList))
    szFileName = FlyFileListGetName(pSrcList->hList, i);
  return szFileName;
}

/*-------------------------------------------------------------------------------------------------
  Get the next source file name in list. See also FlyMakeSrcListGetFirst().

  @param    hSrcList   returned value from FlyMakeSrcListNew()
  @return   source file name or NULL if no more source files
*///-----------------------------------------------------------------------------------------------
unsigned FlyMakeSrcListLen(void *hSrcList)
{
  fmkSrcList_t   *pSrcList  = hSrcList;
  unsigned        len       = 0;

  if(FlyMakeSrcListIs(hSrcList))
    len = FlyFileListLen(pSrcList->hList);

  return len;
}

/*-------------------------------------------------------------------------------------------------
  Free the source list

  @hList    hSrcList   returned value from FlyMakeSrcListNew()
  @return   NULL
*///-----------------------------------------------------------------------------------------------
void * FlyMakeSrcListFree(void *hSrcList)
{
  fmkSrcList_t  *pSrcList = hSrcList;

  if(FlyMakeSrcListIs(pSrcList))
  {
    if(pSrcList->hList)
      FlyFileListFree(pSrcList->hList);
    if(pSrcList->pfUsed)
      FlyFree(pSrcList->pfUsed);
    memset(pSrcList, 0, sizeof(*pSrcList));
  }

  return NULL;
}

/*-------------------------------------------------------------------------------------------------
  Print the sorted file list

  @hList    hSrcList   returned value from FlyMakeSrcListNew()
  @return   none
*///-----------------------------------------------------------------------------------------------
void FlyMakeSrcListPrint(void *hSrcList)
{
  const fmkSrcList_t *pSrcList = hSrcList;
  unsigned            i;
  
  FlyMakePrintf("Source File List %p: ", pSrcList);
  if(!FlyMakeSrcListIs(hSrcList))
    FlyMakePrintf("(invalid)\n");
  else
  {
    FlyMakePrintf("%u file(s)\n", pSrcList->len);
    for(i = 0; i < pSrcList->len; ++i)
      printf("  %u: Used %u %s\n", i, pSrcList->pfUsed[i], FlyMakeSrcListGetName(hSrcList, i));
  }
}

/*-------------------------------------------------------------------------------------------------
  Given a source file, allocate a tool and add all files to that match.

  Marks of any matching source files as "used".

  @param    hSrcList    handle to source file list
  @param    index       index 1st source file in tool
  @return   ptr to allocated tool list for this file
*///-----------------------------------------------------------------------------------------------
fmkTool_t * FmkToolAlloc(void *hSrcList, unsigned index)
{
  fmkSrcList_t   *pSrcList    = hSrcList;
  fmkTool_t      *pTool;
  char           *szToolname;   // e.g. "my_tool"
  const char     *szFilename;   // e.g. "../tools/my_tool.c"
  const char     *psz;
  unsigned        len;          // e.g. length of "../tools/my_tool"
  unsigned        i;
  unsigned        n;
  unsigned        nSrcFiles   = 0;
  bool_t          fWorked     = TRUE;

  // should never try to allocate tool based on file that's already used or out of bounds
  FlyAssert(FlyMakeSrcListIs(hSrcList) && index < FlyMakeSrcListLen(hSrcList));
  FlyAssert(!pSrcList->pfUsed[index]);

  // allocate toolname for index
  szFilename = FlyMakeSrcListGetName(hSrcList, index);  // e.g. "../tools/my_tool.c"
  psz = FlyStrPathNameBase(szFilename, &len);
  szToolname = FlyStrAllocN(psz, len);                  // e.g. "my_tool"
  if(!szToolname)
    fWorked = FALSE;
  len += (unsigned)(psz - szFilename);                  // e.g. length of "../tools/my_tool"

  // count # of files that match this tool
  if(fWorked)
  {
    nSrcFiles = 0;
    for(i = 0; i < pSrcList->len; ++i)
    {
      if(!pSrcList->pfUsed[i] && (strncmp(szFilename, FlyMakeSrcListGetName(hSrcList, i), len) == 0))
        ++nSrcFiles;
    }
  }

  // allocate the tool
  if(fWorked)
  {
    pTool = FlyAllocZ(sizeof(*pTool) + (sizeof(char *) * nSrcFiles));
    if(!pTool)
      fWorked = FALSE;
    else
    {
      pTool->szName = szToolname;
      pTool->aszSrcFiles = (void *)(pTool + 1);
      pTool->nSrcFiles = nSrcFiles;

      n = 0;
      for(i = 0; i < pSrcList->len; ++i)
      {
        psz = FlyMakeSrcListGetName(hSrcList, i);
        if(!pSrcList->pfUsed[i] && (strncmp(szFilename, psz, len) == 0))
        {
          pTool->aszSrcFiles[n] = psz;
          pSrcList->pfUsed[i] = TRUE;
          ++n;
        }
      }
    }
  }

  if(!fWorked)
  {
    if(szToolname)
      FlyFree(szToolname);
    if(pTool)
      FlyFree(pTool);
    pTool = NULL;
  }

  return pTool;
}

/*-------------------------------------------------------------------------------------------------
  Verify this pointer is a tool list

  @param  pToolList   list of tools
  @return TRUE if the tool list is valid
*///-----------------------------------------------------------------------------------------------
bool_t FlyMakeToolListIs(const fmkToolList_t *pToolList)
{
  return (pToolList && pToolList->sanchk == FMK_TOOLLIST_SANCHK) ? TRUE : FALSE;
}

/*-------------------------------------------------------------------------------------------------
  Create a tool list from a folder of source files, case sensitive.

  The following example has 3 tools: `MyTool`, `tool` and `my_cpp_tool`:

      MyTool
        MyTool.c
        MyToolFoo.c
        MyToolBar.c
      tool
        tool.c
        tool_other.c
      my_cpp_tool
        my_cpp_tool.c++

  @param    pCompilerList   list of source file extensions and rules to compile them
  @param    szFolder        folder with 0 or more source files, e.g. "", "test/", "../tools/"
  @param    
  @return   ptr to tool list or NULL if no source files (tools)
*///-----------------------------------------------------------------------------------------------
fmkToolList_t * FlyMakeToolListNew(flyMakeCompiler_t *pCompilerList, const char *szFolder)
{
  fmkSrcList_t     *pSrcList    = NULL;
  fmkToolList_t    *pToolList   = NULL;
  fmkToolList_t    *pToolListNew;
  fmkTool_t        *pTool;
  unsigned          i;
  unsigned          size;
  bool_t            fWorked     = TRUE;

  FlyMakeDbgPrintf(FMK_DEBUG_MORE, "FlyMakeToolListNew(%s)\n", szFolder);

  // get a list of source files, all type (.c, .c++, etc...)
  pSrcList = FlyMakeSrcListNew(pCompilerList, szFolder, 0);
  if(!pSrcList)
    fWorked = FALSE;

  // allocate a tool list
  if(fWorked)
  {
    pToolList = FlyAllocZ(sizeof(*pToolList) + (sizeof(fmkTool_t *) * FMK_TOOLLIST_MAX_TOOLS));
    if(!pToolList)
      fWorked = FALSE;
    else
    {
      pToolList->sanchk    = FMK_TOOLLIST_SANCHK;
      pToolList->hSrcList  = pSrcList;
      pToolList->nMaxTools = FMK_TOOLLIST_MAX_TOOLS;
      pToolList->apTools   = (void *)(pToolList + 1);
    }
  }

  // add each tool until no unused files
  if(fWorked && FlyMakeSrcListLen(pSrcList))
  {
    // find 1st unused file
    while(TRUE)
    {
      for(i = 0; i < FlyMakeSrcListLen(pSrcList); ++i)
      {
        if(!pSrcList->pfUsed[i])
          break;
      }
      if(i == FlyMakeSrcListLen(pSrcList))
        break;

      // found another tool
      if(i < FlyMakeSrcListLen(pSrcList))
      {
        // if we need room for more tools, make it now (doubles nMaxTools)
        if(pToolList->nTools >= pToolList->nMaxTools)
        {
          size = sizeof(*pToolList) + (sizeof(fmkTool_t *) * (pToolList->nMaxTools * 2));
          FlyMakeDbgPrintf(FMK_DEBUG_MORE, "  realloc new size %u", size);
          pToolListNew = FlyRealloc(pToolList, size);
          if(!pToolListNew)
          {
            FlyMakeDbgPrintf(FMK_DEBUG_MORE, " (failed)\n");
            fWorked = FALSE;
          }
          else
          {
            pToolList = pToolListNew;
            pToolList->apTools   = (void *)(pToolList + 1);
            memset(&pToolList->apTools[pToolList->nMaxTools], 0, (sizeof(fmkTool_t *) * pToolList->nMaxTools));
            pToolList->nMaxTools = pToolList->nMaxTools * 2;
            FlyMakeDbgPrintf(FMK_DEBUG_MORE, ", new max %u, pToolList %p, new apTools %p\n",
                          pToolList->nMaxTools, pToolList, pToolList->apTools);
          }
        }

        // allocate the tool
        pTool = FmkToolAlloc(pSrcList, i);
        if(!pTool)
          fWorked = FALSE;
        else
        {
          pToolList->apTools[pToolList->nTools] = pTool;
          ++pToolList->nTools;
        }
      }
    }
  }

  // free the tool list if failed
  if(!fWorked && pToolList)
    pToolList = FlyMakeToolListFree(pToolList);

  FlyMakeDbgPrintf(FMK_DEBUG_MORE, "  fWorked %u, pToolList %p\n", fWorked, pToolList);
  if(pToolList && FlyMakeDebug() >= FMK_DEBUG_MUCH)
    FlyMakeToolListPrint(pToolList);

  return pToolList;
}

/*-------------------------------------------------------------------------------------------------
  Find the tool by name in the tool list

  @param    pToolList   ptr to allocated tool list
  @param    szName      name of tool, e.g. "my_tool"
  @return   ptr to tool, or NULL if not found
*///-----------------------------------------------------------------------------------------------
fmkTool_t * FlyMakeToolListFind(const fmkToolList_t *pToolList, const char *szName)
{
  fmkTool_t  *pTool = NULL;
  unsigned    i;

  if(FlyMakeToolListIs(pToolList))
  {
    for(i = 0; i < pToolList->nTools; ++i)
    {
      if(strcmp(pToolList->apTools[i]->szName, szName) == 0)
      {
        pTool = pToolList->apTools[i];
        break;
      }
    }
  }

  return pTool;
}

/*-------------------------------------------------------------------------------------------------
  Free the tool list allocated by FmkToolListAlloc()

  @param    pToolList   ptr to allocated tool list
  @return   NULL
*///-----------------------------------------------------------------------------------------------
void * FlyMakeToolListFree(fmkToolList_t *pToolList)
{
  unsigned    i;

  // free tool list
  if(FlyMakeToolListIs(pToolList))
  {
    // delete each tool
    for(i = 0; i < pToolList->nTools; ++i)
    {
      if(pToolList->apTools[i])
        FlyFree(pToolList->apTools[i]);
    }

    // free the src list structure
    FlyMakeSrcListFree(pToolList->hSrcList);

    // free the tool list
    memset(pToolList, 0, sizeof(*pToolList));
    FlyFree(pToolList);
  }

  return NULL;
}

/*-------------------------------------------------------------------------------------------------
  Returns number of tools allocated with FlyMakeToolListNew()

  @param    pToolList   ptr to tool list
  @return   Number of tools found on FlyMakeTool
*///-----------------------------------------------------------------------------------------------
void FlyMakeToolPrint(const fmkTool_t *pTool)
{
  unsigned    i;

  // print this tool
  FlyMakePrintf("pTool %p: %s [", pTool, pTool->szName);
  for(i = 0; i < pTool->nSrcFiles; ++i)
  {
    if(i)
      FlyMakePrintf(", ");
    FlyMakePrintf("%s", pTool->aszSrcFiles[i]);
  }
  FlyMakePrintf("]\n");  
}

/*-------------------------------------------------------------------------------------------------
  Returns number of tools allocated with FlyMakeToolListNew()

  @param    pToolList   ptr to tool list
  @return   Number of tools found on FlyMakeTool
*///-----------------------------------------------------------------------------------------------
void FlyMakeToolListPrint(const fmkToolList_t *pToolList)
{
  unsigned    i;

  if(!FlyMakeToolListIs(pToolList))
    FlyMakePrintf("pToolList %p (invalid)\n", pToolList);
  else
  {
    FlyMakePrintf("pToolList %p: nTools=%u, nMaxTools=%u:\n", pToolList, pToolList->nTools, pToolList->nMaxTools);
    for(i = 0; i < pToolList->nTools; ++i)
    {
      FlyMakePrintf("%u: ", i);
      FlyMakeToolPrint(pToolList->apTools[i]);
    }
  }
}
