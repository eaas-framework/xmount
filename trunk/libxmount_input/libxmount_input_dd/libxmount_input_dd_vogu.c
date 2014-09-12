/*******************************************************************************
* xmount Copyright (c) 2008-2013 by Gillen Daniel <gillen.dan@pinguin.lu>      *
*                                                                              *
* This module has been written by Guy Voncken. It contains the functions for   *
* accessing dd images. Split dd is supported as well.                          *
*                                                                              *
* This program is free software: you can redistribute it and/or modify it      *
* under the terms of the GNU General Public License as published by the Free   *
* Software Foundation, either version 3 of the License, or (at your option)    *
* any later version.                                                           *
*                                                                              *
* This program is distributed in the hope that it will be useful, but WITHOUT  *
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or        *
* FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for     *
* more details.                                                                *
*                                                                              *
* You should have received a copy of the GNU General Public License along with *
* this program. If not, see <http://www.gnu.org/licenses/>.                    *
*******************************************************************************/

// Please don't touch source code formatting!

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/types.h>
#include <locale.h>
#include <errno.h>

#include "../libxmount_input.h"

// --------------------------------
//  Definitions, types, structures
// --------------------------------

#define GETMAX(a,b) ((a)>(b)?(a):(b))
#define GETMIN(a,b) ((a)<(b)?(a):(b))
#ifndef TRUE
   #define TRUE 1
   #define FALSE 1
#endif

// DD Error codes are automatically mapped to errno codes by means of the groups
// below. DD uses these errno codes:
//   ENOMEM    memory allocation errors
//   EINVAL    wrong parameter(s) passed to a DD function
//   EIO       all others: DD function errors, image problems

enum // error codes
{
   DD_OK=0,

   DD_ERROR_ENOMEM_START=1000,
      DD_MEMALLOC_FAILED,
   DD_ERROR_ENOMEM_END,

   DD_ERROR_EINVAL_START=2000,
      DD_CANNOT_OPEN_LOGFILE,
   DD_ERROR_EINVAL_END,

   DD_ERROR_EIO_START=3000,
      DD_FILE_OPEN_FAILED,
      DD_CANNOT_READ_DATA,
      DD_CANNOT_CLOSE_FILE,
      DD_CANNOT_SEEK,
      DD_READ_BEYOND_END_OF_IMAGE,
      DD_READ_NOTHING,
   DD_ERROR_EIO_END
};



typedef struct
{
   char     *pFilename;
   uint64_t   FileSize;
   FILE     *pFile;
} t_Piece, *t_pPiece;

typedef struct
{
   t_pPiece  pPieceArr;
   uint64_t   Pieces;
   uint64_t   TotalSize;

   char     *pLogFilename;
   uint8_t    LogStdout;
} t_Dd, *t_pDd;

static int         DdClose           (void *pHandle);
static const char* DdGetErrorMessage (int ErrNum);


#define DD_OPTION_LOG "ddlog"

// ----------------------------
//  Logging and error handling
// ----------------------------

#define LOG_HEADER_LEN 80

int LogvEntry (const char *pLogFileName, uint8_t LogStdout, const char *pFileName, const char *pFunctionName, int LineNr, const char *pFormat, va_list pArguments)
{
   time_t       NowT;
   struct tm  *pNowTM;
   FILE       *pFile;
   int          wr;
   char       *pFullLogFileName = NULL;
   const char *pBase;
   char         LogLineHeader[1024];
   pid_t        OwnPID;

   if (!LogStdout && (pLogFileName==NULL))
      return DD_OK;

   time (&NowT);
   pNowTM = localtime (&NowT);
   OwnPID = getpid();  // pthread_self()
   wr  = strftime (&LogLineHeader[0] , sizeof(LogLineHeader)   , "%a %d.%b.%Y %H:%M:%S ", pNowTM);
   wr += snprintf (&LogLineHeader[wr], sizeof(LogLineHeader)-wr, "%5d ", OwnPID);

   if (pFileName && pFunctionName)
   {
      pBase = strrchr(pFileName, '/');
      if (pBase)
         pFileName = pBase+1;
      wr += snprintf (&LogLineHeader[wr], sizeof(LogLineHeader)-wr, "%s %s %d ", pFileName, pFunctionName, LineNr);
   }

//   while (wr < LOG_HEADER_LEN)
//      LogLineHeader[wr++] = ' ';

   if (pLogFileName)
   {
      int wr = asprintf (&pFullLogFileName, "%s_%d", pLogFileName, OwnPID);
      if ((wr <= 0) || (pFullLogFileName == NULL))
      {
         if (LogStdout)
            printf ("\nLog file error: Can't build filename");
          return DD_MEMALLOC_FAILED;
      }
      else
      {
         pFile = fopen64 (pFullLogFileName, "a");
         if (pFile == NULL)
         {
            if (LogStdout)
               printf ("\nLog file error: Can't be opened");
            return DD_CANNOT_OPEN_LOGFILE;
         }
         else
         {
            fprintf  (pFile, "%-*s", LOG_HEADER_LEN, &LogLineHeader[0]);
            vfprintf (pFile, pFormat, pArguments);
            fprintf  (pFile, "\n");
            fclose   (pFile);
         }
         free (pFullLogFileName);
      }
   }
   if (LogStdout)
   {
      printf  ("%s", &LogLineHeader[0]);
      vprintf (pFormat, pArguments);
      printf  ("\n");
   }
   return DD_OK;
}

int LogEntry (const char *pLogFileName, uint8_t LogStdout, const char *pFileName, const char *pFunctionName, int LineNr, const char *pFormat, ...)
{
   va_list VaList;
   int     rc;

   if (!LogStdout && (pLogFileName==NULL))
      return DD_OK;

   va_start(VaList, pFormat);
   rc = LogvEntry (pLogFileName, LogStdout, pFileName, pFunctionName, LineNr, pFormat, VaList);
   va_end(VaList);
   return rc;
}

// CHK requires existance of pDd handle

#ifdef DD_STANDALONE
   #define LOG_ERRORS_ON_STDOUT TRUE
#else
   #define LOG_ERRORS_ON_STDOUT pDd->LogStdout
#endif

#define CHK(ChkVal)                                                             \
{                                                                               \
   int ChkValRc;                                                                \
   if ((ChkValRc=(ChkVal)) != DD_OK)                                            \
   {                                                                            \
      const char *pErr = DdGetErrorMessage (ChkValRc);                          \
      LogEntry (pDd->pLogFilename, LOG_ERRORS_ON_STDOUT, __FILE__, __FUNCTION__, __LINE__, "Error %d (%s) occured", ChkValRc, pErr); \
      return ChkValRc;                                                          \
   }                                                                            \
}

#define LOG(...) \
   LogEntry (pDd->pLogFilename, pDd->LogStdout, __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__);

// DdCheckError is called before exiting DdRead. It should not
// be called elsewehere or else the statistics would become wrong.
static void DdCheckError (t_pDd pDd, int Ret, int *pErrno)
{
   *pErrno = 0;
   if (Ret != DD_OK)
   {
      if      ((Ret >= DD_ERROR_ENOMEM_START) && (Ret <= DD_ERROR_ENOMEM_END)) *pErrno = ENOMEM;
      else if ((Ret >= DD_ERROR_EINVAL_START) && (Ret <= DD_ERROR_EINVAL_END)) *pErrno = EINVAL;
      else                                                                     *pErrno = EIO;    // all other errors
   }
}

// ------------------------------------
//         Internal functions
// ------------------------------------

static inline int DdLogStdout (t_pDd pDd, uint8_t Flag)
{
   pDd->LogStdout = Flag;
   return DD_OK;
}

static inline uint64_t DdGetCurrentSeekPos (t_pPiece pPiece)
{
   return ftello (pPiece->pFile);
}

static inline int DdSetCurrentSeekPos (t_pPiece pPiece, uint64_t Val, int Whence)
{
   if (fseeko (pPiece->pFile, Val, Whence) != 0)
      return DD_CANNOT_SEEK;
   return DD_OK;
}

static int DdRead0 (t_pDd pDd, char *pBuffer, uint64_t Seek, uint32_t *pCount)
{
   t_pPiece pPiece;
   uint64_t  i;

   // Find correct piece to read from
   // -------------------------------

   for (i=0; i<pDd->Pieces; i++)
   {
      pPiece = &pDd->pPieceArr[i];
      if (Seek < pPiece->FileSize)
         break;
      Seek -= pPiece->FileSize;
   }
   if (i >= pDd->Pieces)
      return DD_READ_BEYOND_END_OF_IMAGE;

   // Read from this piece
   // --------------------
   CHK (DdSetCurrentSeekPos (pPiece, Seek, SEEK_SET))

   *pCount = GETMIN (*pCount, pPiece->FileSize - Seek);

   if (fread (pBuffer, *pCount, 1, pPiece->pFile) != 1)
      return DD_CANNOT_READ_DATA;

  return DD_OK;
}

// ------------------------------------
//             API functions
// ------------------------------------

static int DdCreateHandle (void **ppHandle, const char *pFormat, uint8_t Debug)
{
   (void)pFormat;
   t_pDd pDd=NULL;

   pDd = (t_pDd) malloc (sizeof (t_Dd));
   if (pDd == NULL)
      return DD_MEMALLOC_FAILED;

   memset (pDd, 0, sizeof(t_Dd));
   CHK (DdLogStdout (pDd, Debug))

   *ppHandle = pDd;

   return DD_OK;
}

static int DdDestroyHandle (void **ppHandle)
{
   t_pDd pDd = (t_pDd)*ppHandle;
   LOG ("Called");
   LOG ("Remark: 'Ret' won't be logged"); // Handle gets destroyed, 'ret' logging not possible

   free (*ppHandle);
   *ppHandle = NULL;

   return DD_OK;
}

static int DdOpen(void *pHandle, const char **ppFilenameArr, uint64_t FilenameArrLen)
{
   t_pDd    pDd = (t_pDd) pHandle;
   t_pPiece pPiece;
   int       rc = DD_OK;

   LOG ("Called - Files=%" PRIu64, FilenameArrLen);

   pDd->Pieces    = FilenameArrLen;
   pDd->pPieceArr = (t_pPiece) malloc (pDd->Pieces * sizeof (t_Piece));

   if (pDd->pPieceArr == NULL)
      return DD_MEMALLOC_FAILED;

   memset (pDd->pPieceArr, 0, pDd->Pieces * sizeof(t_Piece));

   pDd->TotalSize = 0;
   for (uint64_t i=0; i < pDd->Pieces; i++)
   {
      pPiece = &pDd->pPieceArr[i];
      pPiece->pFilename = strdup (ppFilenameArr[i]);
      if (pPiece->pFilename == NULL)
      {
         (void) DdClose (pHandle);
         rc = DD_MEMALLOC_FAILED;
         break;
      }
      pPiece->pFile = fopen (pPiece->pFilename, "r");
      if (pPiece->pFile == NULL)
      {
         (void) DdClose (pHandle);
         rc = DD_FILE_OPEN_FAILED;
         break;
      }
      CHK (DdSetCurrentSeekPos (pPiece, 0, SEEK_END))
      pPiece->FileSize = DdGetCurrentSeekPos (pPiece);
      pDd->TotalSize  += pPiece->FileSize;
   }

   LOG ("Ret - rc=%d", rc);
   return rc;
}

static int DdClose (void *pHandle)
{
   t_pDd    pDd = (t_pDd) pHandle;
   t_pPiece pPiece;
   int       CloseErrors = 0;

   LOG ("Called");
   if (pDd->pPieceArr)
   {
      for (uint64_t i=0; i < pDd->Pieces; i++)
      {
         pPiece = &pDd->pPieceArr[i];
         if (pPiece->pFile)
         {
            if (fclose (pPiece->pFile))
               CloseErrors=1;
         }
         if (pPiece->pFilename)
            free (pPiece->pFilename);
      }
      free (pDd->pPieceArr);
   }

   if (CloseErrors)
      return DD_CANNOT_CLOSE_FILE;

   LOG ("Ret");
   return DD_OK;
}

static int DdSize(void *pHandle, uint64_t *pSize)
{
   t_pDd pDd = (t_pDd) pHandle;

   LOG ("Called");
   *pSize = pDd->TotalSize;

   LOG ("Ret - Size=%" PRIu64, *pSize);
   return DD_OK;
}

static int DdRead (void *pHandle, char *pBuf, off_t Seek, size_t Count, size_t *pRead, int *pErrno)
{
   t_pDd    pDd       = (t_pDd) pHandle;
   uint32_t Remaining = Count;
   uint32_t Read;
   int      Ret = DD_OK;

   LOG ("Called - Seek=%'" PRIu64 ",Count=%'llu", Seek, Count);
   *pRead  = 0;
   *pErrno = 0;

   if ((Seek+Count) > pDd->TotalSize)
   {
      Ret = DD_READ_BEYOND_END_OF_IMAGE;
      goto Leave;
   }

   do
   {
      Read = Remaining;
      Ret = DdRead0 (pDd, pBuf, Seek, &Read);
      if (Ret)
         goto Leave;
      if (Read == 0)
      {
         Ret = DD_READ_NOTHING;
         goto Leave;
      }

      Remaining -= Read;
      pBuf      += Read;
      Seek      += Read;
      *pRead    += Read;
   } while (Remaining);

Leave:
   DdCheckError (pDd, Ret, pErrno);
   LOG ("Ret %d - Read=%" PRIu32, Ret, *pRead);
   return Ret;

}

static int DdOptionsHelp (const char **ppHelp)
{
   char *pHelp=NULL;
   int    wr;

   wr = asprintf (&pHelp, "    %-5s : set the log file name. The given log file name is extended by _<pid>. Specify the absolute path.\n"
                  , DD_OPTION_LOG);
   if ((pHelp == NULL) || (wr<=0))
      return DD_MEMALLOC_FAILED;

   *ppHelp = pHelp;

   return DD_OK;
}

static int DdOptionsParse (void *pHandle, uint32_t OptionCount, const pts_LibXmountOptions *ppOptions, const char **ppError)
{
   pts_LibXmountOptions pOption;
   t_pDd                pDd    = (t_pDd) pHandle;
   const char          *pError = NULL;
   int                   rc    = DD_OK;

   LOG ("Called - OptionCount=%" PRIu32, OptionCount);
   *ppError = NULL;
   for (uint32_t i=0; i<OptionCount; i++)
   {
      pOption = ppOptions[i];
      if (strcmp (pOption->p_key, DD_OPTION_LOG) == 0)
      {
         pDd->pLogFilename = pOption->p_value;
         rc = LOG ("Logging for libxmount_input_dd started")
         if (rc != DD_OK)
         {
            pError = "Write test to log file failed";
            break;
         }
         pOption->valid = TRUE;
         LOG ("Option %s set to %s", DD_OPTION_LOG, pDd->pLogFilename);
      }
   }

   if (pError)
      *ppError = strdup (pError);
   LOG ("Ret - rc=%d,Error=%s", rc, *ppError);
   return rc;
}

static int DdGetInfofileContent (void *pHandle, const char **ppInfoBuf)
{
   t_pDd pDd=(t_pDd)pHandle;
   int    wr;
   char *pInfo=NULL;

   LOG ("Called");
   // TODO: TotalSize seems to be incorrect here???
   wr = asprintf(&pInfo,
                 "DD image assembled of %" PRIu64 " piece(s)\n"
                 "%" PRIu64 " bytes in total (%0.3f GiB)\n",
                 pDd->Pieces,
                 pDd->TotalSize, pDd->TotalSize/(1024.0*1024.0*1024.0));
   if ((wr<0) || (*ppInfoBuf==NULL))
      return DD_MEMALLOC_FAILED;

   *ppInfoBuf = pInfo;

   LOG ("Ret - %d bytes of info", wr);
   return DD_OK;
}

static const char* DdGetErrorMessage (int ErrNum)
{
   const char *pMsg;
   #define ADD_ERR(DdErrCode)             \
      case DdErrCode: pMsg = #DdErrCode;  \
      break;

   switch (ErrNum)
   {
      ADD_ERR (DD_OK)
      ADD_ERR (DD_MEMALLOC_FAILED)
      ADD_ERR (DD_CANNOT_OPEN_LOGFILE)
      ADD_ERR (DD_FILE_OPEN_FAILED)
      ADD_ERR (DD_CANNOT_READ_DATA)
      ADD_ERR (DD_CANNOT_CLOSE_FILE)
      ADD_ERR (DD_CANNOT_SEEK)
      ADD_ERR (DD_READ_BEYOND_END_OF_IMAGE)
      ADD_ERR (DD_READ_NOTHING)
      default:
         return pMsg = "Unknown error";
   }
   #undef ARR_ERR
   return pMsg;
}

static int DdFreeBuffer (void *pBuf)
{
   free (pBuf);

   return DD_OK;
}

// ------------------------------------
//  LibXmount_Input API implementation
// ------------------------------------

uint8_t LibXmount_Input_GetApiVersion ()
{
   return LIBXMOUNT_INPUT_API_VERSION;
}

const char* LibXmount_Input_GetSupportedFormats ()
{
  return "dd\0\0";
}

void LibXmount_Input_GetFunctions (ts_LibXmountInputFunctions *pFunctions)
{
   pFunctions->CreateHandle       = &DdCreateHandle;
   pFunctions->DestroyHandle      = &DdDestroyHandle;
   pFunctions->Open               = &DdOpen;
   pFunctions->Close              = &DdClose;
   pFunctions->Size               = &DdSize;
   pFunctions->Read               = &DdRead;
   pFunctions->OptionsHelp        = &DdOptionsHelp;
   pFunctions->OptionsParse       = &DdOptionsParse;
   pFunctions->GetInfofileContent = &DdGetInfofileContent;
   pFunctions->GetErrorMessage    = &DdGetErrorMessage;
   pFunctions->FreeBuffer         = &DdFreeBuffer;
}


// -----------------------------------------------------
//              Small main routine for testing
//            It a split dd file to non-split dd
// -----------------------------------------------------


#ifdef DD_STANDALONE

int main(int argc, const char *argv[])
{
   t_pDd      pDd;
   uint64_t    TotalSize;
   uint64_t    Remaining;
   uint64_t    ToRead;
   uint64_t    Read;
   uint64_t    Pos;
   uint32_t    BuffSize = 1024;
   char        Buff[BuffSize];
   FILE      *pFile;
   int         Percent;
   int         PercentOld;
   const char *pHelp;
   const char *pInfo;

   printf ("Split DD to DD converter\n");
   if (argc < 3)
   {
      printf ("Usage: %s <dd part 1> <dd part 2> <...> <dd destination>\n", argv[0]);
      printf ("Options help:");
      CHK (DdOptionsHelp (&pHelp))
      printf ("%s\n", pHelp ? pHelp : "--");

      exit (1);
   }
   CHK (DdCreateHandle ((void**)&pDd, "dd"))
//   CHK (DdLogStdout (pDd, true))
   CHK (DdLogStdout (pDd, false))

   CHK (DdOpen ((void**)&pDd, &argv[1], argc-2))
   CHK (DdSize ((void* ) pDd, &TotalSize))
   printf ("Total size: %" PRIu64 " bytes\n", TotalSize);
   Remaining = TotalSize;

   pFile = fopen (argv[argc-1], "w");
   if (pFile == NULL)
   {
       printf ("Cannot open destination file\n");
       exit (1);
   }

   CHK (DdGetInfofileContent (pDd, &pInfo))
   printf ("-- Start of image info --\n");
   printf ("%s", pInfo);
   printf ("-- End of image info --\n");
   CHK (DdFreeBuffer ((void*) pInfo))

   Remaining  = TotalSize;
   Pos        = 0;
   PercentOld = -1;
   while (Remaining)
   {
      ToRead = GETMIN (Remaining, BuffSize);
      CHK (DdRead ((void*)pDd, &Buff[0], Pos, ToRead, &Read))
      if (ToRead != Read)
      {
         printf ("DdRead only returned part of the data (%" PRIu64 "/%" PRIu64 ")\n", Read, ToRead);
         exit (1);
      }

      if (fwrite (Buff, Read, 1, pFile) != 1)
      {
         printf ("Could not write to destinationfile\n");
         exit (2);
      }

      Remaining -= Read;
      Pos       += Read;
      Percent = (100*Pos) / TotalSize;
      if (Percent != PercentOld)
      {
         printf ("\r%d%% done...", Percent);
         PercentOld = Percent;
      }
   }
   if (fclose (pFile))
   {
      printf ("Error while closing destination file\n");
      exit (3);
   }

   CHK (DdClose         ((void**)&pDd))
   CHK (DdDestroyHandle ((void**)&pDd))
   printf ("\n");

   return 0;
}

#endif // DD_STANDALONE

