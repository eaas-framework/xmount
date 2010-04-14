
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <argp.h>

// ---------------------
//   Argument parsing
// ---------------------

const char *argp_program_version     = "xmount 0.0.1";
const char *argp_program_bug_address = "<gida@gmail.com>";

static char ProgramInfo[] = "xmount is a very strange program written by a even stranger programmer."
                    "\vFor VMDK emulation, you have to uncomment ""user_allow_other"" in /etc/fuse.conf or run xmount as root."
                    "\n\nPlease excuse the mistakes in this software, the programmer comes from Vianden.";

static char ProgramArgs[] = "SourceFile Mountpoint";

static struct argp_option Options[] =
{
   {"in"            , 'i', "itype"   , 0, "Input image format. itype can be ""dd"", ""ewf"", ""aff"""},
   {"out"           , 'o', "otype"   , 0, "output image format. oitype can be ""dd"", ""vdi"", ""vmdk"""},
   {"cache"         , 'c', "file"    , 0, "Enable virtual write support and set cachefile to use"},
   {"owcache"       , 'k', "file"    , 0, "Same as option --cache, but overwrites existing cache"},
   {"singlethreaded", 's', 0         , 0, "Run single threaded"},
   {"fuseopts"      , 'f', "FuseOpts", 0, "Specify fuse mount options. Also disables the automatic addition of FUSE's allow_other option"},
   {"debug"         , 'd', 0         , 0, "Enable FUSE's and xmount's debug mode"},
   { 0 }
};


typedef enum
{
   DATAYTPE_NONE,
   DATAYTPE_DD,
   DATAYTPE_EWF,
   DATAYTPE_VDI,
   DATAYTPE_VMDK,
} t_DataTypeID;

typedef struct
{
   t_DataTypeID  ID;
   const char  *pName;
   bool          AllowedIn;
   bool          AllowedOut;
} t_DataType, *t_pDataType;

t_DataType DataTypeArr[] = {{ DATAYTPE_DD  , "dd"  , true , true },
                            { DATAYTPE_EWF , "ewf" , true , false},
                            { DATAYTPE_VDI , "vdi" , false, true },
                            { DATAYTPE_VMDK, "vmdk", false, true },
                            { DATAYTPE_NONE, 0     , false, false}};

typedef struct
{
   char               *pSourceFile;
   char               *pMountPoint;
   t_pDataType         pDataTypeIn;
   t_pDataType         pDataTypeOut;
   char               *pCacheFile;
   bool                 OverwriteCache;
   bool                 SingleThreaded;
   char               *pFuseOpts;
   bool                 Debug;
} t_Arguments, *t_pArguments;

bool SetDataType (t_pDataType *ppType, const char *pArg, bool In)
{
   t_pDataType pType;
   int          i;

   for (i=0;;i++)
   {
      pType = &DataTypeArr[i];
      if (!pType->pName)
         break;

      if (strcasecmp (pType->pName, pArg) == 0)
      {
         if (( In && pType->AllowedIn ) ||
             (!In && pType->AllowedOut))
         {
            *ppType = pType;
            return true;
         }
         break;
      }
   }
   *ppType = NULL;
   return false;
}


static error_t ParseOption (int Key, char *pArg, struct argp_state *pState)
{
   t_pArguments pArguments = (t_pArguments) pState->input;
   static int    ArgCount  = 0;
   switch (Key)
   {
      case 'i': if (!SetDataType (&pArguments->pDataTypeIn , pArg, true )) argp_error (pState, "Data type ""%s"" unavailable for input" , pArg); break;
      case 'o': if (!SetDataType (&pArguments->pDataTypeOut, pArg, false)) argp_error (pState, "Data type ""%s"" unavailable for output", pArg); break;
      case 'c': pArguments->OverwriteCache = false; pArguments->pCacheFile = pArg; break;
      case 'k': pArguments->OverwriteCache = true;  pArguments->pCacheFile = pArg; break;
      case 's': pArguments->SingleThreaded = true; break;
      case 'd': pArguments->Debug          = true; break;
      case 'f': pArguments->pFuseOpts      = pArg; break;

      case ARGP_KEY_NO_ARGS: argp_usage (pState);
                             break;
      case ARGP_KEY_ARG    : ArgCount++;
                             switch (ArgCount)
                             {
                                case 1 : pArguments->pSourceFile = pArg; break;
                                case 2 : pArguments->pMountPoint = pArg; break;
                                default: argp_usage (pState);            break;
                             }
                             break;
      default              : return ARGP_ERR_UNKNOWN;
  }
  return 0;
}

static struct argp argp = {Options, ParseOption, ProgramArgs, ProgramInfo};

void ParseArguments (int argc, char **argv, t_pArguments pArguments)
{
   // Default arguments
   // -----------------
   pArguments->pSourceFile    = NULL;
   pArguments->pMountPoint    = NULL;
   SetDataType (&pArguments->pDataTypeIn , "dd", true);
   SetDataType (&pArguments->pDataTypeOut, "dd", false);
   pArguments->pCacheFile     = NULL;
   pArguments->OverwriteCache = false;
   pArguments->SingleThreaded = false;
   pArguments->pFuseOpts      = false;
   pArguments->Debug          = false;

   argp_parse (&argp, argc, argv, 0, 0, pArguments);
}


int main (int argc, char *argv[])
{
   t_Arguments         Arguments;

   setbuf (stdout, NULL);
   setbuf (stderr, NULL);

   ParseArguments (argc, argv, &Arguments);

   #define BOOL_TO_STR(Flag) Flag?"yes":"no"

   printf("\nRunning with:");
   printf("\n   Source file     %s"     , Arguments.pSourceFile);
   printf("\n   Mount point     %s"     , Arguments.pMountPoint);
   printf("\n   In type         %s (%u)", Arguments.pDataTypeIn ->pName, Arguments.pDataTypeIn ->ID);
   printf("\n   Out type        %s (%u)", Arguments.pDataTypeOut->pName, Arguments.pDataTypeOut->ID);
   printf("\n   Cache file      %s"     , Arguments.pCacheFile);
   printf("\n   Overwrite cache %s"     , BOOL_TO_STR(Arguments.OverwriteCache));
   printf("\n   Single threaded %s"     , BOOL_TO_STR(Arguments.SingleThreaded));
   printf("\n   FUSE options    %s"     , Arguments.pFuseOpts);
   printf("\n   Debug           %s"     , BOOL_TO_STR(Arguments.Debug));
   printf("\n");

   return 0;
}

