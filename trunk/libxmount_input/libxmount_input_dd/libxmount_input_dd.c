/*******************************************************************************
* xmount Copyright (c) 2008-2013 by Gillen Daniel <gillen.dan@pinguin.lu>      *
*                                                                              *
* This module has been written by Guy Voncken. It contains the functions for   *
* accessing dd images. Split dd is supported as well.                          *
*                                                                              *
* xmount is a small tool to "fuse mount" various harddisk image formats as dd, *
* vdi, vhd or vmdk files and enable virtual write access to them.              *
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

#include <string.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <limits.h>

#include "dd.h"

// ----------------------
//  Constant definitions
// ----------------------

#define GETMAX(a,b) ((a)>(b)?(a):(b))
#define GETMIN(a,b) ((a)<(b)?(a):(b))


// ---------------------
//  Types and strutures
// ---------------------

typedef struct 
{
   char              *pFilename;
   unsigned long long  FileSize;
   FILE               *pFile;
} t_Piece, *t_pPiece;

typedef struct _t_dd
{
   t_pPiece           pPieceArr;
   unsigned int        Pieces;
   unsigned long long  TotalSize;
   char              *pInfo;
} t_dd;


// ----------------
//  Error handling
// ----------------

#ifdef DD_DEBUG
   #define CHK(ChkVal)    \
   {                                                                  \
      int ChkValRc;                                                   \
      if ((ChkValRc=(ChkVal)) != DD_OK)                               \
      {                                                               \
         printf ("Err %d in %s, %d\n", ChkValRc, __FILE__, __LINE__); \
         return ChkValRc;                                             \
      }                                                               \
   }
   #define DEBUG_PRINTF(pFormat, ...) \
      printf (pFormat, ##__VA_ARGS__);
#else
   #define CHK(ChkVal)                      \
   {                                        \
      int ChkValRc;                         \
      if ((ChkValRc=(ChkVal)) != DD_OK)     \
         return ChkValRc;                   \
   }
   #define DEBUG_PRINTF(...)
#endif

// ---------------------------
//  Internal static functions
// ---------------------------

static inline unsigned long long ddGetCurrentSeekPos (t_pPiece pPiece)
{
   return ftello (pPiece->pFile);
}

static inline int ddSetCurrentSeekPos (t_pPiece pPiece, unsigned long long Val, int Whence)
{
   if (fseeko (pPiece->pFile, Val, Whence) != 0)
      return DD_CANNOT_SEEK;
   return DD_OK;
}

int ddDestroyHandle (t_pdd *ppdd)
{
    t_pdd    pdd = *ppdd;
    t_pPiece pPiece;
    int       CloseErrors = 0;

    if (pdd->pPieceArr)
    {
        for (int i=0; i < pdd->Pieces; i++)
        {
            pPiece = &pdd->pPieceArr[i];
            if (pPiece->pFile)
               if (fclose (pPiece->pFile))
                  CloseErrors++;
            if (pPiece->pFilename)
               free (pPiece->pFilename);
        }
        free (pdd->pPieceArr);
    }
    if (pdd->pInfo)
        free (pdd->pInfo);

    free (pdd);
    *ppdd = NULL;

    if (CloseErrors)
        return DD_CANNOT_CLOSE_FILE;

    return DD_OK;
}

static int ddCreateHandle (t_pdd *ppdd, unsigned FilenameArrLen, const char **ppFilenameArr)
{
   t_pdd    pdd;
   t_pPiece pPiece;

   *ppdd = NULL;
   pdd = (t_pdd) malloc (sizeof(t_dd));
   if (pdd == NULL)
      return DD_MEMALLOC_FAILED;

   memset (pdd, 0, sizeof(t_dd));
   pdd->Pieces    = FilenameArrLen;
   pdd->pPieceArr = (t_pPiece) malloc (pdd->Pieces * sizeof(t_Piece));
   if (pdd->pPieceArr == NULL)
   {
      (void) ddDestroyHandle (&pdd);
      return DD_MEMALLOC_FAILED;
   }

   pdd->TotalSize = 0;
   for (int i=0; i < pdd->Pieces; i++)
   {
       pPiece = &pdd->pPieceArr[i];
       pPiece->pFilename = strdup (ppFilenameArr[i]);
       if (pPiece->pFilename == NULL)
       {
          (void) ddDestroyHandle (&pdd);
          return DD_MEMALLOC_FAILED;
       }
       pPiece->pFile = fopen (pPiece->pFilename, "r");
       if (pPiece->pFile == NULL)
       {
          (void) ddDestroyHandle (&pdd);
          return DD_FILE_OPEN_FAILED;
       }
       CHK(ddSetCurrentSeekPos(pPiece, 0, SEEK_END))
       pPiece->FileSize = ddGetCurrentSeekPos (pPiece);
       pdd->TotalSize  += pPiece->FileSize;
   }

   asprintf (&pdd->pInfo, "dd image made of %u pieces, %llu bytes in total (%0.3f GiB)", pdd->Pieces, pdd->TotalSize, pdd->TotalSize / (1024.0*1024.0*1024.0));

   *ppdd = pdd;

   return DD_OK;
}


// ---------------
//  API functions
// ---------------

int ddOpen  (t_pdd *ppdd, unsigned FilenameArrLen, const char **pFilenameArr)
{
    CHK (ddCreateHandle (ppdd, FilenameArrLen, pFilenameArr))

    return DD_OK;
}

int ddSize (t_pdd pdd, unsigned long long *pSize)
{
    *pSize = pdd->TotalSize;

    return DD_OK;
}

int ddRead0  (t_pdd pdd, unsigned long long Seek, unsigned char *pBuffer, unsigned int *pCount)
{
    t_pPiece pPiece;
    int       i;

    // Find correct piece to read from
    // -------------------------------

    for (i=0; i<pdd->Pieces; i++)
    {
        pPiece = &pdd->pPieceArr[i];
        if (Seek < pPiece->FileSize)
            break;
        Seek -= pPiece->FileSize;
    }
    if (i >= pdd->Pieces)
        return DD_READ_BEYOND_END_OF_IMAGE;

    // Read from this piece
    // --------------------
    CHK (ddSetCurrentSeekPos (pPiece, Seek, SEEK_SET))

    *pCount = GETMIN (*pCount, pPiece->FileSize - Seek);

    if (fread (pBuffer, *pCount, 1, pPiece->pFile) != 1)
       return DD_CANNOT_READ_DATA;

    return DD_OK;
}


int ddRead (t_pdd pdd, unsigned long long Seek, unsigned char *pBuffer, unsigned int Count)
{
    unsigned Remaining = Count;
    unsigned Read;

    if ((Seek + Count) > pdd->TotalSize)
       return DD_READ_BEYOND_END_OF_IMAGE;

    do
    {
        Read = Remaining;
        CHK (ddRead0 (pdd, Seek, pBuffer, &Read))
        Remaining -= Read;
        pBuffer   += Read;
        Seek      += Read;
    } while (Remaining);

    return DD_OK;
}

int ddInfo (t_pdd pdd, char **ppInfoBuff)
{
    *ppInfoBuff = pdd->pInfo;
    return DD_OK;
}

int ddClose (t_pdd *ppdd)
{
    CHK (ddDestroyHandle(ppdd))
    return DD_OK;
}

// -----------------------------------------------------
//              Small main routine for testing
//            It a split dd file to non-split dd
// -----------------------------------------------------


#ifdef DD_MAIN_FOR_TESTING

int main(int argc, const char *argv[])
{
   t_pdd              pdd;
   unsigned long long  TotalSize;
   unsigned long long  Remaining;
   unsigned long long  Read;
   unsigned long long  Pos;
   unsigned int        BuffSize = 1024;
   unsigned char       Buff[BuffSize];
   FILE              *pFile;
   int                 Percent;
   int                 PercentOld;
   int                 rc;

   printf ("Split DD to DD converter\n");
   if (argc < 3)
   {
      printf ("Usage: %s <dd part 1> <dd part 2> <...> <dd destination>\n", argv[0]);
      exit (1);
   }

   if (ddOpen (&pdd, argc-2, &argv[1]) != DD_OK)
   {
       printf ("Cannot open split dd file\n");
       exit (1);
   }
   CHK (ddSize (pdd, &TotalSize))
   printf ("Total size: %llu bytes\n", TotalSize);
   Remaining = TotalSize;

   pFile = fopen (argv[argc-1], "w");
   if (pFile == NULL)
   {
       printf ("Cannot open destination file\n");
       exit (1);
   }

   Remaining  = TotalSize;
   Pos        = 0;
   PercentOld = -1;
   while (Remaining)
   {
       Read = GETMIN (Remaining, BuffSize);
       rc = ddRead (pdd, Pos, &Buff[0], Read);
       if (rc != DD_OK)
       {
           printf ("Error %d while calling ddRead\n", rc);
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
      printf ("Error while closing destinationfile\n");
      exit (3);
   }

   printf ("\n");
   return 0;
}

#endif
