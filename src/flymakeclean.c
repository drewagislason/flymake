/**************************************************************************************************
  flymakeclean.c - the clean command. Deletes object files and optionally libraries and programs.
  Copyright 2024 Drew Gislason
  license: <https://mit-license.org>
**************************************************************************************************/
#include "flymake.h"

static const char m_szOutFolder[]  = FMK_SZ_OUT;    // e.g. "out/"

/*-------------------------------------------------------------------------------------------------
  Delete each tool program in this folder

  @param  pState
  @param  szFolder    folder in which to delete tool programs
  @return none
*///-----------------------------------------------------------------------------------------------
void FmkDelToolsProg(flyMakeState_t *pState, const char *szFolder)
{
  fmkToolList_t  *pToolList;
  flyStrSmart_t  *pCmdline;
  unsigned        i;

  pCmdline = FlyStrSmartAlloc(strlen(szFolder) + 42);
  pToolList = FlyMakeToolListNew(pState->pCompilerList, szFolder);
  if(pToolList && pCmdline)
  {
    for(i = 0; i < pToolList->nTools; ++i)
    {
      // remove the executable
      FlyStrSmartSprintf(pCmdline, "rm -f %s%s", szFolder, pToolList->apTools[i]->szName);
      FlyMakeSystem(FMK_VERBOSE_SOME, &pState->opts, pCmdline->sz);
    }
  }
  if(pCmdline)
    FlyStrSmartFree(pCmdline);
  if(pToolList)
    FlyMakeToolListFree(pToolList);
}

/*-------------------------------------------------------------------------------------------------
  Delete each tool program in this folder

  @param  pState
*///-----------------------------------------------------------------------------------------------
void FmkDelProgOrLib(flyMakeState_t *pState, flyMakeFolder_t *pFolder)
{
  const char      szFmtDel[]  = "rm -f %s";
  flyStrSmart_t  *pCmdline    = NULL;
  char           *szName      = NULL;

  if(pFolder->rule == FMK_RULE_LIB)
    szName = FlyMakeFolderAllocLibName(pState, pFolder->szFolder);
  else
    szName = FlyMakeFolderAllocSrcName(pState, pFolder->szFolder);

  if(szName)
    pCmdline = FlyStrSmartAlloc(sizeof(szFmtDel) + strlen(szName));
  if(pCmdline)
  {
    FlyStrSmartSprintf(pCmdline, szFmtDel, szName);
    FlyMakeSystem(FMK_VERBOSE_SOME, &pState->opts, pCmdline->sz);
  }

  FlyFreeIf(szName);
  FlyStrSmartFree(pCmdline);  
}

/*-------------------------------------------------------------------------------------------------
  Usage: flymake clean [--all] [-B]

  1. No options removes just .o (object) files
  2. Option `-B` removes programs/libs as well as o
  3. Option `--all` removes  dependency objs

  Deletes .o (objs). --all cleans programs/libs as well as .objs

  @param    pState    cmdline options, etc...
  @return   TRUE if worked, FALSE if failed
*///-----------------------------------------------------------------------------------------------
bool_t FlyMakeCleanFiles(flyMakeState_t *pState)
{
  const char        szFmtDelOut[]     = "rm -rf %s%s";
  flyMakeFolder_t  *pFolder;
  flyStrSmart_t    *pCmdline          = FlyStrSmartAlloc(128);

  // count the number of folders
  pFolder = pState->pFolderList;
  while(pFolder)
  {
    // delete the .o (object) files for each folder
    FlyStrSmartSprintf(pCmdline, szFmtDelOut, pFolder->szFolder, m_szOutFolder);
    FlyMakeSystem(FMK_VERBOSE_SOME, &pState->opts, pCmdline->sz);

    //
    if(pState->opts.fRebuild)
    {
      // delete program/library
      if((pFolder->rule == FMK_RULE_LIB) || (pFolder->rule == FMK_RULE_SRC))
        FmkDelProgOrLib(pState, pFolder);

      // delete tools
      else if(pFolder->rule == FMK_RULE_TOOL)
        FmkDelToolsProg(pState, pFolder->szFolder);
    }

    pFolder = pFolder->pNext;
  }

  // flag --all will force re-checking out of the dependencies by deleteing the whole folder tree
  if(pState->opts.fAll)
    FlyMakeFolderRemove(FMK_VERBOSE_SOME, &pState->opts, pState->szDepDir);

  FlyStrSmartFree(pCmdline);

  return TRUE;
}

