/**************************************************************************************************
  flymakestate.c - state of flymake as it is processing
  Copyright 2024 Drew Gislason  
  license: MIT <https://mit-license.org>
**************************************************************************************************/
#include "flymake.h"

/*-------------------------------------------------------------------------------------------------
  Initialize state

  @param    pState    ptr to state of project
  @return   none
*///-----------------------------------------------------------------------------------------------
void FlyMakeStateInit(flyMakeState_t *pState)
{
  memset(pState, 0, sizeof(*pState));
  pState->sanchk = FLYMAKESTATE_SANCHK;
}

/*-------------------------------------------------------------------------------------------------
  Clone options into a new state. Now ready for FlyMakeTomlRootFill() and FlyMakeTomlAlloc().

  @return   newly allocated cloned state
*///-----------------------------------------------------------------------------------------------
flyMakeState_t * FlyMakeStateClone(flyMakeState_t *pState)
{
  flyMakeState_t *pNewState;

  pNewState = FlyAlloc(sizeof(*pState));
  if(pNewState)
  {
    FlyMakeStateInit(pNewState);
    pNewState->opts = pState->opts;
    pNewState->pCompilerList = pState->pCompilerList;
  }

  return pNewState;
}

/*-------------------------------------------------------------------------------------------------
  Free a state and all of it's pointers. Knows about each subsystem that's part of the state.

  @return   NULL
*///-----------------------------------------------------------------------------------------------
void *FlyMakeStateFree(flyMakeState_t *pState)
{
  (void)pState;
  return NULL;
}

/*-------------------------------------------------------------------------------------------------
  Is this a state variable?

  @param    pState            state of a project
  @return   none
*///-----------------------------------------------------------------------------------------------
bool_t FlyMakeIsState(const flyMakeState_t *pState)
{
  return (pState && pState->sanchk == FLYMAKESTATE_SANCHK) ? TRUE : FALSE;
}

/*-------------------------------------------------------------------------------------------------
  Depth for building tool source code tree

  @param    pState            state of a project
  @return   none
*///-----------------------------------------------------------------------------------------------
unsigned FlyMakeStateDepth(const flyMakeState_t *pState)
{
  return pState->fIsSimple ? 1 : FMK_SRC_DEPTH;
}

/*-------------------------------------------------------------------------------------------------
  Print state including dependencies and folders

  @param    pState      state of a project
  @param    fVerbose    brief (outline), or verbose (all variables)
  @return   none
*///-----------------------------------------------------------------------------------------------
void FlyMakeStatePrintEx(const flyMakeState_t *pState, bool_t fVerbose)
{
  flyMakeDep_t    *pDep;
  flyStrSmart_t    brief;
  flyMakeFolder_t *pFolder;

  FlyMakePrintf(m_szFmkBanner, "FlyMakeStatePrintEx");

  if(fVerbose)
    FlyMakeStatePrint(pState);
  else
  {
    FlyStrSmartInit(&brief);
    pFolder = pState->pFolderList;
    while(pFolder)
    {
      if((pFolder->rule == FMK_RULE_SRC) || (pFolder->rule == FMK_RULE_LIB))
      {
        FlyStrSmartCat(&brief, pFolder->szFolder);
        FlyStrSmartCat(&brief, " ");
      }
      pFolder = pFolder->pNext;
    }
    FlyMakePrintf("State %p %s\n", pState, brief.sz);
  }

  // print dependencies
  pDep = pState->pDepList;
  while(pDep)
  {
    if(fVerbose)
      FlyMakeDepPrint(pDep);
    else
      FlyMakePrintf("  Dep %p %s: %s", pDep, pDep->szName, pDep->libs.sz);

    if(pDep->pState)
    {
      if(fVerbose)
        FlyMakeStatePrint(pDep->pState);
      else
        FlyMakePrintf(", state %p", pDep->pState);
    }
    FlyMakePrintf(" %s\n", pDep->libs.sz);

    pDep = pDep->pNext;
  }
}

/*-------------------------------------------------------------------------------------------------
  Print state

  @param    pState            state of a project
  @return   none
*///-----------------------------------------------------------------------------------------------
void FlyMakeStatePrint(const flyMakeState_t *pState)
{
  int                       i;
  const char               *pszCliArg;

  FlyMakePrintf("\n---- state %p ----\n", pState);

  if(!FlyMakeIsState(pState))
  {
    FlyMakePrintf("invalid state!\n");
    return;
  }

  // from FlyCliParse()
  FlyMakePrintf("opts: fAll %u, fCpp %u, dbg %u, debug %u, fLib %u, fRebuild %u, fNoBuild %u\n"
                "      fRulesLib %u, fRulesTools %u, fRulesSrc %u, verbose %u\n",
    pState->opts.fAll, pState->opts.fCpp, pState->opts.dbg, pState->opts.debug, pState->opts.fLib,
    pState->opts.fRebuild, pState->opts.fNoBuild, pState->opts.fRulesLib, pState->opts.fRulesTools,
    pState->opts.fRulesSrc, pState->opts.verbose);

  // from FlyMakeTomlRootFind()
  FlyMakePrintf("szFullPath  %s\n", FlyStrNullOk(pState->szFullPath));
  FlyMakePrintf("szRoot      %s\n", FlyStrNullOk(pState->szRoot));
  FlyMakePrintf("szInc       %s\n", FlyStrNullOk(pState->szInc));
  FlyMakePrintf("szDepDir    %s\n", FlyStrNullOk(pState->szDepDir));

  // from [package] in flymake.toml
  FlyMakePrintf("szProjName  %s\n", FlyStrNullOk(pState->szProjName));
  FlyMakePrintf("szProjVer   %s\n", FlyStrNullOk(pState->szProjVer));
  FlyMakePrintf("incs:       %s\n", pState->incs.sz);
  FlyMakePrintf("libs:       %s\n", pState->libs.sz);

  // cmdline
  if(pState->pCli)
  {
    FlyMakePrintf("cmdline:    ");
    i = 0;
    while(1)
    {
      pszCliArg = FlyCliArg(pState->pCli, i);
      if(pszCliArg == NULL)
        break;
      FlyMakePrintf("%s ", pszCliArg);
      ++i;
    }
    FlyMakePrintf("\n");
  }

  // from [dependencies] in flymake.toml
  if(!pState->pDepList)
    FlyMakePrintf("deps:       none\n");
  else
  {
    FlyMakePrintf("[dependencies] =\n");
    FlyMakeDepListPrint(pState->pDepList);
  }

  // from [folders] in flymake.toml
  if(!pState->pFolderList)
    FlyMakePrintf("folders:    none\n");
  else
  {
    FlyMakePrintf("[folders] =\n");
    FlyMakeFolderListPrint(pState->pFolderList);
  }

  // from [compiler] in flymake.toml
  if(!pState->pCompilerList)
    FlyMakePrintf("compilers:  none\n");
  else
  {
    FlyMakePrintf("[compilers] =\n");
    FlyMakeCompilerListPrint(pState->pCompilerList);
  }

  FlyMakePrintf("---- end state %p ----\n\n", pState);
}
