/**************************************************************************************************
  flymakeprint.c - the view in model/view/controller. All output goes through here.
  Copyright 2024 Drew Gislason
  license: <https://mit-license.org>
**************************************************************************************************/
#include "flymake.h"
#include <stdarg.h>

/*-------------------------------------------------------------------------------------------------
  Print if level >= global verbose level

  @param    level     debug level (0=don't print, 1=normal, 2=more, 3=max
  @param    szFormat  printf() format string
  @param    ...       printf arguments
  @return   length of string as printed
*///-----------------------------------------------------------------------------------------------
int FlyMakePrintf(const char *szFormat, ...)
{
  va_list     arglist;
  int         len = 0;

  va_start(arglist, szFormat);
  len = vprintf(szFormat, arglist);
  va_end(arglist);

  return len;
}

/*-------------------------------------------------------------------------------------------------
  Print if level >= verbose level

  @param    level     none, some, more
  @param    szFormat  printf() format string
  @param    ...       printf arguments
  @return   length of string as printed
*///-----------------------------------------------------------------------------------------------
int FlyMakePrintfEx(fmkVerbose_t level, const char *szFormat, ...)
{
  va_list     arglist;
  int         len = 0;

  if(FlyMakeVerbose() >= level)
  {
    va_start(arglist, szFormat);
    len = vprintf(szFormat, arglist);
    va_end(arglist);
  }

  return len;
}

/*-------------------------------------------------------------------------------------------------
  Print if level >= verbose level

  @param    level     none, some, more
  @param    szFormat  printf() format string
  @param    ...       printf arguments
  @return   length of string as printed
*///-----------------------------------------------------------------------------------------------
int FlyMakeDbgPrintf(fmkDebug_t level, const char *szFormat, ...)
{
  va_list     arglist;
  int         len = 0;

  if(FlyMakeDebug() >= level)
  {
    va_start(arglist, szFormat);
    len = vprintf(szFormat, arglist);
    va_end(arglist);
  }

  return len;
}

/*-------------------------------------------------------------------------------------------------
  Print the error

  @param    err       ptr to state of project
  @param    szExtra   extended part of message, usually a path to a file,  may be NULL
  @return   none
*///-----------------------------------------------------------------------------------------------
void FlyMakePrintErr(fmkErr_t err, const char *szExtra)
{
  if(szExtra == NULL)
    szExtra = "";

  // most errors begin with error: 
  if((err != FMK_ERR_NONE) && (err != FMK_ERR_CUSTOM))
    FlyMakePrintf("flymake error: ");

  switch(err)
  {
    case FMK_ERR_NONE:
    case FMK_ERR_CUSTOM:
      // nothing to print
    break;
    case FMK_ERR_MEM:
      FlyMakePrintf("out of memory\n");
    break;
    case FMK_ERR_BAD_PATH:
      FlyMakePrintf("invalid path `%s`\n", FlyStrNullOk(szExtra));
    break;
    case FMK_ERR_BAD_PROG:
      FlyMakePrintf("'%s' is not a valid program\n", FlyStrNullOk(szExtra));
    break;
    case FMK_ERR_NO_FILES:
      FlyMakePrintf("no source files in folder %s\n", FlyStrNullOk(szExtra));
    break;
    case FMK_ERR_NOT_PROJECT:
      FlyMakePrintf("path `%s` does not appear to be in a project or is empty\n", FlyStrNullOk(szExtra));
    break;
    case FMK_ERR_NOT_SAME_ROOT:
      FlyMakePrintf("'%s' not in same root\n", FlyStrNullOk(szExtra));
    break;
    case FMK_ERR_NO_RULE:
      FlyMakePrintf("No rule to make target %s\n", FlyStrNullOk(szExtra));
    break;
    case FMK_ERR_CLONE:
      FlyMakePrintf("could not git clone %s\n", FlyStrNullOk(szExtra));
    break;
    case FMK_ERR_WRITE:
      FlyMakePrintf("cannot write to file/folder %s\n", FlyStrNullOk(szExtra));
    break;
    default:
      FlyMakePrintf("unknown (%u)\n", err);
    break;
  }
}

/*-------------------------------------------------------------------------------------------------
  Print "out of memory" and exit

  @return   none
*///-----------------------------------------------------------------------------------------------
fmkErr_t FlyMakeErrMem(void)
{
  FlyMakePrintErr(FMK_ERR_MEM, NULL);
  FlyMakeErrExit();
  return FMK_ERR_MEM;
}

/*-------------------------------------------------------------------------------------------------
  Print flymake.toml file error in standard error format.

      deps/mydep/flymake.toml:32:10: Expected inline table

  @param    pState    state of this project
  @param    szToml    ptr to character within pState->szTomlFile
  @param    szErr     error string
  @return   column of error (1-n)
*///-----------------------------------------------------------------------------------------------
fmkErr_t FlyMakeErrToml(const flyMakeState_t *pState, const char *szToml, const char *szErr)
{
  unsigned    line, col;
  const char *szLine;

  // print error line
  line = FlyStrLinePos(pState->szTomlFile, szToml, &col);
  FlyMakePrintf("%s%s:%u:%u: error: %s\n", pState->szRoot, g_szTomlFile, line, col, szErr);

  // print context
  szLine = FlyStrLineBeg(pState->szTomlFile, szToml);
  FlyMakePrintf("  %.*s\n", (unsigned)FlyStrLineLen(szLine), szLine);
  FlyMakePrintf("  %*s^\n", col - 1, "");

  return FMK_ERR_CUSTOM;
}

