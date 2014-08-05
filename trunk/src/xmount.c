/*******************************************************************************
* xmount Copyright (c) 2008-2014 by Gillen Daniel <gillen.dan@pinguin.lu>      *
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

#include <config.h>

//#ifndef HAVE_LIBZ
//  #undef WITH_LIBAEWF
//#endif

//#define XMOUNT_LIBRARY_PATH "/usr/local/lib/xmount"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
//#include <fcntl.h>
#include <dlfcn.h> // For dlopen, dlclose, dlsym
#include <dirent.h> // For opendir, readdir, closedir
#include <unistd.h>
#include <sys/ioctl.h>
#ifndef __APPLE__
  #include <linux/fs.h>
#endif
#include <pthread.h>
#include <time.h>

#include "xmount.h"
#include "md5.h"

/*******************************************************************************
 * Global vars
 ******************************************************************************/
// Struct that contains various runtime configuration options
static ts_XmountConfData glob_xmount_cfg;

// Struct containing pointers to the libxmount_input functions
static pts_InputLib *glob_pp_input_libs=NULL;
static uint32_t glob_input_libs_count=0;
static pts_LibXmountInputFunctions glob_p_input_functions=NULL;

// Handle for input image
static void *glob_p_input_image=NULL;

// Pointer to virtual info file
static char *glob_p_info_file=NULL;

// Vars needed for VDI emulation
static pts_VdiFileHeader glob_p_vdi_header=NULL;
static uint32_t glob_vdi_header_size=0;
static char *glob_p_vdi_block_map=NULL;
static uint32_t glob_p_vdi_block_map_size=0;

// Vars needed for VHD emulation
static ts_VhdFileHeader *glob_p_vhd_header=NULL;

// Vars needed for VMDK emulation
static char *glob_p_vmdk_file=NULL;
static int glob_vmdk_file_size=0;
static char *glob_p_vmdk_lockdir1=NULL;
static char *glob_p_vmdk_lockdir2=NULL;
static char *glob_p_vmdk_lockfile_data=NULL;
static int glob_vmdk_lockfile_size=0;
static char *glob_p_vmdk_lockfile_name=NULL;

// Vars needed for virtual write access
static FILE *glob_p_cache_file=NULL;
static pts_CacheFileHeader glob_p_cache_header=NULL;
static pts_CacheFileBlockIndex glob_p_cache_blkidx=NULL;

// Mutexes to control concurrent read & write access
static pthread_mutex_t glob_mutex_image_rw;
static pthread_mutex_t glob_mutex_info_read;

/*
 * LogMessage:
 *   Print error and debug messages to stdout
 *
 * Params:
 *  p_msg_type: "ERROR" or "DEBUG"
 *  p_calling_fun: Name of calling function
 *  line: Line number of call
 *  p_msg: Message string
 *  ...: Variable params with values to include in message string
 *
 * Returns:
 *   n/a
 */
static void LogMessage(char *p_msg_type,
                       char *p_calling_fun,
                       int line,
                       char *p_msg,
                       ...)
{
  va_list var_list;

  // Print message "header"
  printf("%s: %s.%s@%u : ",p_msg_type,p_calling_fun,XMOUNT_VERSION,line);
  // Print message with variable parameters
  va_start(var_list,p_msg);
  vprintf(p_msg,var_list);
  va_end(var_list);
}

/*
 * LogWarnMessage:
 *   Print warning messages to stdout
 *
 * Params:
 *  p_msg: Message string
 *  ...: Variable params with values to include in message string
 *
 * Returns:
 *   n/a
 */
static void LogWarnMessage(char *p_msg,...) {
  va_list var_list;

  // Print message "header"
  printf("WARNING: ");
  // Print message with variable parameters
  va_start(var_list,p_msg);
  vprintf(p_msg,var_list);
  va_end(var_list);
}

/*
 * PrintUsage:
 *   Print usage instructions (cmdline options etc..)
 *
 * Params:
 *   p_prog_name: Program name (argv[0])
 *
 * Returns:
 *   n/a
 */
static void PrintUsage(char *p_prog_name) {
  char *p_buf;
  int first=1;

  printf("\nxmount v%s copyright (c) 2008-2014 by Gillen Daniel "
         "<gillen.dan@pinguin.lu>\n",XMOUNT_VERSION);
  printf("\nUsage:\n");
  printf("  %s [[fopts] [mopts]] <ifile> [<ifile> [...]] <mntp>\n\n",p_prog_name);
  printf("Options:\n");
  printf("  fopts:\n");
  printf("    -d : Enable FUSE's and xmount's debug mode.\n");
  printf("    -h : Display this help message.\n");
  printf("    -s : Run single threaded.\n");
  printf("    -o no_allow_other : Disable automatic addition of FUSE's allow_other option.\n");
  printf("    -o <fmopts> : Specify fuse mount options. Will also disable automatic\n");
  printf("                  addition of FUSE's allow_other option!\n");
  printf("    INFO: For VMDK emulation, you have to uncomment \"user_allow_other\" in\n");
  printf("          /etc/fuse.conf or run xmount as root.\n");
  printf("\n");
  printf("  mopts:\n");
  printf("    --cache <file> : Enable virtual write support and set cachefile to use.\n");
//  printf("    --debug : Enable xmount's debug mode.\n");
  printf("    --in <itype> : Input image format. <itype> can be ");

  // List supported input types
  for(uint32_t i=0;i<glob_input_libs_count;i++) {
    p_buf=glob_pp_input_libs[i]->p_supported_input_types;
    while(*p_buf!='\0') {
      if(first==1) {
        printf("\"%s\"",p_buf);
        first=0;
      } else printf(", \"%s\"",p_buf);
      p_buf+=(strlen(p_buf)+1);
    }
  }
  printf(".\n");

  printf("    --inopts <iopts> : Specify input library specific options.\n");
  printf("    --info : Print out some infos about used compiler and libraries.\n");
  printf("    --offset <off> : Move the output image data start <off> bytes into the input image.\n");
  printf("    --out <otype> : Output image format. <otype> can be \"dd\", \"dmg\", \"vdi\", \"vhd\", \"vmdk(s)\".\n");
  printf("    --owcache <file> : Same as --cache <file> but overwrites existing cache.\n");
  printf("    --rw <file> : Same as --cache <file>.\n");
  printf("    --version : Same as --info.\n");
#ifndef __APPLE__
  printf("    INFO: Input and output image type defaults to \"dd\" if not specified.\n");
#else
  printf("    INFO: Input image type defaults to \"dd\" and output image type defaults to \"dmg\" if not specified.\n");
#endif
  printf("    WARNING: Output image type \"vmdk(s)\" should be considered experimental!\n");
  printf("\n");
  printf("  ifile:\n");
  printf("    Input image file. If your input image is split into multiple files, you have to specify them all!\n");
  printf("\n");
  printf("  mntp:\n");
  printf("    Mount point where virtual files should be located.\n");
  printf("\n");
  printf("  iopts:\n");
  printf("    Some input libraries might support an own set of options to configure / tune their behaviour.\n");
  printf("    Input libraries supporting this feature (if any) and and their options are listed below.\n");
  printf("\n");

  // List input lib options
  first=1;
  for(uint32_t i=0;i<glob_input_libs_count;i++) {
    p_buf=(char*)glob_pp_input_libs[i]->lib_functions.OptionsHelp();
    if(p_buf==NULL) continue;
    first=0;
    printf("    - %s\n",glob_pp_input_libs[i]->p_name);
    printf("%s\n",p_buf);
    printf("\n");
  }

}

/*
 * CheckFuseAllowOther:
 *   Check if FUSE allows us to pass the -o allow_other parameter.
 *   This only works if we are root or user_allow_other is set in
 *   /etc/fuse.conf.
 *
 * Params:
 *   n/a
 *
 * Returns:
 *   TRUE on success, FALSE on error
 */
static int CheckFuseAllowOther() {
  if(geteuid()!=0) {
    // Not running xmount as root. Try to read FUSE's config file /etc/fuse.conf
    FILE *hFuseConf=(FILE*)FOPEN("/etc/fuse.conf","r");
    if(hFuseConf==NULL) {
      LogWarnMessage("FUSE will not allow other users nor root to access your "
                     "virtual harddisk image. To change this behavior, please "
                     "add \"user_allow_other\" to /etc/fuse.conf or execute "
                     "xmount as root.\n");
      return FALSE;
    }
    // Search conf file for set user_allow_others
    char line[256];
    int PermSet=FALSE;
    while(fgets(line,sizeof(line),hFuseConf)!=NULL && PermSet!=TRUE) {
      // TODO: This works as long as there is no other parameter beginning with
      // "user_allow_other" :)
      if(strncmp(line,"user_allow_other",strlen("user_allow_other"))==0) {
        PermSet=TRUE;
      }
    }
    fclose(hFuseConf);
    if(PermSet==FALSE) {
      LogWarnMessage("FUSE will not allow other users nor root to access your "
                     "virtual harddisk image. To change this behavior, please "
                     "add \"user_allow_other\" to /etc/fuse.conf or execute "
                     "xmount as root.\n");
      return FALSE;
    }
  }
  // Running xmount as root or user_allow_other is set in /etc/fuse.conf
  return TRUE;
}

/*
 * ParseCmdLine:
 *   Parse command line options
 *
 * Params:
 *   argc: Number of cmdline params
 *   argv: Array containing cmdline params
 *   pNargv: Number of FUSE options is written to this var
 *   pppNargv: FUSE options are written to this array
 *   pFilenameCount: Number of input image files is written to this var
 *   pppFilenames: Input image filenames are written to this array
 *   ppMountpoint: Mountpoint is written to this var
 *
 * Returns:
 *   "TRUE" on success, "FALSE" on error
 */
static int ParseCmdLine(const int argc,
                        char **argv,
                        int *pNargc,
                        char ***pppNargv,
                        int *pFilenameCount,
                        char ***pppFilenames,
                        char **ppMountpoint) {
  int i=1,files=0,opts=0,FuseMinusOControl=TRUE,FuseAllowOther=TRUE,first;
  char *p_buf;

  // add argv[0] to pppNargv
  opts++;
  XMOUNT_MALLOC(*pppNargv,char**,opts*sizeof(char*))
  XMOUNT_STRSET((*pppNargv)[opts-1],argv[0])

  // Parse options
  while(i<argc && *argv[i]=='-') {
    if(strlen(argv[i])>1 && *(argv[i]+1)!='-') {
      // Options beginning with - are mostly FUSE specific
      if(strcmp(argv[i],"-d")==0) {
        // Enable FUSE's and xmount's debug mode
        opts++;
        XMOUNT_REALLOC(*pppNargv,char**,opts*sizeof(char*))
        XMOUNT_STRSET((*pppNargv)[opts-1],argv[i])
        glob_xmount_cfg.Debug=TRUE;
      } else if(strcmp(argv[i],"-h")==0) {
        // Print help message
        PrintUsage(argv[0]);
        exit(1);
      } else if(strcmp(argv[i],"-o")==0) {
        // Next parameter specifies fuse / lib mount options
        if((argc+1)>i) {
          i++;
          // As the user specified the -o option, we assume he knows what he is
          // doing. We won't append allow_other automatically. And we allow him
          // to disable allow_other by passing a single "-o no_allow_other"
          // which won't be passed to FUSE as it is xmount specific.
          if(strcmp(argv[i],"no_allow_other")!=0) {
            opts+=2;
            XMOUNT_REALLOC(*pppNargv,char**,opts*sizeof(char*))
            XMOUNT_STRSET((*pppNargv)[opts-2],argv[i-1])
            XMOUNT_STRSET((*pppNargv)[opts-1],argv[i])
            FuseMinusOControl=FALSE;
          } else FuseAllowOther=FALSE;
        } else {
          LOG_ERROR("Couldn't parse mount options!\n")
          PrintUsage(argv[0]);
          exit(1);
        }
      } else if(strcmp(argv[i],"-s")==0) {
        // Enable FUSE's single threaded mode
        opts++;
        XMOUNT_REALLOC(*pppNargv,char**,opts*sizeof(char*))
        XMOUNT_STRSET((*pppNargv)[opts-1],argv[i])
      } else if(strcmp(argv[i],"-V")==0) {
        // Display FUSE version info
        opts++;
        XMOUNT_REALLOC(*pppNargv,char**,opts*sizeof(char*))
        XMOUNT_STRSET((*pppNargv)[opts-1],argv[i])
      } else {
        LOG_ERROR("Unknown command line option \"%s\"\n",argv[i]);
        PrintUsage(argv[0]);
        exit(1);
      }
    } else {
      // Options beginning with -- are xmount specific
      if(strcmp(argv[i],"--cache")==0 || strcmp(argv[i],"--rw")==0) {
        // Emulate writable access to mounted image
        // Next parameter must be cache file to read/write changes from/to
        if((argc+1)>i) {
          i++;
          XMOUNT_STRSET(glob_xmount_cfg.pCacheFile,argv[i])
          glob_xmount_cfg.Writable=TRUE;
        } else {
          LOG_ERROR("You must specify a cache file to read/write data from/to!\n")
          PrintUsage(argv[0]);
          exit(1);
        }
        LOG_DEBUG("Enabling virtual write support using cache file \"%s\"\n",
                  glob_xmount_cfg.pCacheFile)
      } else if(strcmp(argv[i],"--in")==0) {
        // Specify input image type
        // Next parameter must be image type
        if((argc+1)>i) {
          i++;
          if(glob_xmount_cfg.p_orig_image_type==NULL) {
            XMOUNT_STRSET(glob_xmount_cfg.p_orig_image_type,argv[i]);
            LOG_DEBUG("Setting input image type to '%s'\n",argv[i]);
          } else {
            LOG_ERROR("You can only specify --in once!")
            PrintUsage(argv[0]);
            exit(1);
          }
        } else {
          LOG_ERROR("You must specify an input image type!\n");
          PrintUsage(argv[0]);
          exit(1);
        }
      } else if(strcmp(argv[i],"--inopts")==0) {
        if((argc+1)>i) {
          i++;
          XMOUNT_STRSET(glob_xmount_cfg.p_lib_params,argv[i]);
        } else {
          LOG_ERROR("You must specify special options!\n");
          PrintUsage(argv[0]);
          exit(1);
        }
      } else if(strcmp(argv[i],"--out")==0) {
        // Specify output image type
        // Next parameter must be image type
        if((argc+1)>i) {
          i++;
          if(strcmp(argv[i],"dd")==0) {
            glob_xmount_cfg.VirtImageType=VirtImageType_DD;
            LOG_DEBUG("Setting virtual image type to DD\n")
          } else if(strcmp(argv[i],"dmg")==0) {
            glob_xmount_cfg.VirtImageType=VirtImageType_DMG;
            LOG_DEBUG("Setting virtual image type to DMG\n")
          } else if(strcmp(argv[i],"vdi")==0) {
            glob_xmount_cfg.VirtImageType=VirtImageType_VDI;
            LOG_DEBUG("Setting virtual image type to VDI\n")
          } else if(strcmp(argv[i],"vhd")==0) {
            glob_xmount_cfg.VirtImageType=VirtImageType_VHD;
            LOG_DEBUG("Setting virtual image type to VHD\n")
          } else if(strcmp(argv[i],"vmdk")==0) {
            glob_xmount_cfg.VirtImageType=VirtImageType_VMDK;
            LOG_DEBUG("Setting virtual image type to VMDK\n")
          } else if(strcmp(argv[i],"vmdks")==0) {
            glob_xmount_cfg.VirtImageType=VirtImageType_VMDKS;
            LOG_DEBUG("Setting virtual image type to VMDKS\n")
          } else {
            LOG_ERROR("Unknown output image type \"%s\"!\n",argv[i])
            PrintUsage(argv[0]);
            exit(1);
          }
        } else {
          LOG_ERROR("You must specify an output image type!\n");
          PrintUsage(argv[0]);
          exit(1);
        }
      } else if(strcmp(argv[i],"--owcache")==0) {
        // Enable writable access to mounted image and overwrite existing cache
        // Next parameter must be cache file to read/write changes from/to
        if((argc+1)>i) {
          i++;
          XMOUNT_STRSET(glob_xmount_cfg.pCacheFile,argv[i])
          glob_xmount_cfg.Writable=TRUE;
          glob_xmount_cfg.OverwriteCache=TRUE;
        } else {
          LOG_ERROR("You must specify a cache file to read/write data from/to!\n")
          PrintUsage(argv[0]);
          exit(1);
        }
        LOG_DEBUG("Enabling virtual write support overwriting cache file \"%s\"\n",
                  glob_xmount_cfg.pCacheFile)
      } else if(strcmp(argv[i],"--version")==0 || strcmp(argv[i],"--info")==0) {
        printf("xmount v%s copyright (c) 2008-2014 by Gillen Daniel "
               "<gillen.dan@pinguin.lu>\n\n",XMOUNT_VERSION);
#ifdef __GNUC__
        printf("  compile timestamp: %s %s\n",__DATE__,__TIME__);
        printf("  gcc version: %s\n",__VERSION__);
#endif
        printf("  loaded input libraries:\n");
        for(uint32_t ii=0;ii<glob_input_libs_count;ii++) {
          printf("    - %s supporting ",glob_pp_input_libs[ii]->p_name);
          p_buf=glob_pp_input_libs[ii]->p_supported_input_types;
          first=TRUE;
          while(*p_buf!='\0') {
            if(first) {
              printf("\"%s\"",p_buf);
              first=FALSE;
            } else printf(", \"%s\"",p_buf);
            p_buf+=(strlen(p_buf)+1);
          }
          printf("\n");
        }
        printf("\n");
        exit(0);
      } else if(strcmp(argv[i],"--offset")==0) {
        if((argc+1)>i) {
          i++;
          glob_xmount_cfg.orig_img_offset=strtoull(argv[i],NULL,10);
        } else {
          LOG_ERROR("You must specify an offset!\n")
          PrintUsage(argv[0]);
          exit(1);
        }
        LOG_DEBUG("Setting input image offset to \"%" PRIu64 "\"\n",
                  glob_xmount_cfg.orig_img_offset)
      } else {
        LOG_ERROR("Unknown command line option \"%s\"\n",argv[i]);
        PrintUsage(argv[0]);
        exit(1);
      }
    }
    i++;
  }
  
  // Parse input image filename(s)
  while(i<(argc-1)) {
    files++;
    XMOUNT_REALLOC(*pppFilenames,char**,files*sizeof(char*))
    XMOUNT_STRSET((*pppFilenames)[files-1],argv[i])
    i++;
  }
  if(files==0) {
    LOG_ERROR("No input files specified!\n")
    PrintUsage(argv[0]);
    exit(1);
  }
  *pFilenameCount=files;

  // Extract mountpoint
  if(i==(argc-1)) {
    XMOUNT_STRSET(*ppMountpoint,argv[argc-1])
    opts++;
    XMOUNT_REALLOC(*pppNargv,char**,opts*sizeof(char*))
    XMOUNT_STRSET((*pppNargv)[opts-1],*ppMountpoint)
  } else {
    LOG_ERROR("No mountpoint specified!\n")
    PrintUsage(argv[0]);
    exit(1);
  }

  if(FuseMinusOControl==TRUE) {
    // We control the -o flag, set subtype, fsname and allow_other options
    opts+=2;
    XMOUNT_REALLOC(*pppNargv,char**,opts*sizeof(char*))
    XMOUNT_STRSET((*pppNargv)[opts-2],"-o")
    XMOUNT_STRSET((*pppNargv)[opts-1],"subtype=xmount,fsname=")
    XMOUNT_STRAPP((*pppNargv)[opts-1],(*pppFilenames)[0])
    if(FuseAllowOther==TRUE) {
      // Try to add "allow_other" to FUSE's cmd-line params
      if(CheckFuseAllowOther()==TRUE) {
        XMOUNT_STRAPP((*pppNargv)[opts-1],",allow_other")
      }
    }
  }

  *pNargc=opts;

  return TRUE;
}

/*
 * ExtractVirtFileNames:
 *   Extract virtual file name from input image name
 *
 * Params:
 *   p_orig_name: Name of input image (Can include a path)
 *
 * Returns:
 *   "TRUE" on success, "FALSE" on error
 */
static int ExtractVirtFileNames(char *p_orig_name) {
  char *tmp;

  // Truncate any leading path
  tmp=strrchr(p_orig_name,'/');
  if(tmp!=NULL) p_orig_name=tmp+1;

  // Extract file extension
  tmp=strrchr(p_orig_name,'.');

  // Set leading '/'
  XMOUNT_STRSET(glob_xmount_cfg.pVirtualImagePath,"/")
  XMOUNT_STRSET(glob_xmount_cfg.pVirtualImageInfoPath,"/")
  if(glob_xmount_cfg.VirtImageType==VirtImageType_VMDK ||
     glob_xmount_cfg.VirtImageType==VirtImageType_VMDKS)
  {
    XMOUNT_STRSET(glob_xmount_cfg.pVirtualVmdkPath,"/")
  }

  // Copy filename
  if(tmp==NULL) {
    // Input image filename has no extension
    XMOUNT_STRAPP(glob_xmount_cfg.pVirtualImagePath,p_orig_name)
    XMOUNT_STRAPP(glob_xmount_cfg.pVirtualImageInfoPath,p_orig_name)
    if(glob_xmount_cfg.VirtImageType==VirtImageType_VMDK ||
       glob_xmount_cfg.VirtImageType==VirtImageType_VMDKS)
    {
      XMOUNT_STRAPP(glob_xmount_cfg.pVirtualVmdkPath,p_orig_name)
    }
    XMOUNT_STRAPP(glob_xmount_cfg.pVirtualImageInfoPath,".info")
  } else {
    XMOUNT_STRNAPP(glob_xmount_cfg.pVirtualImagePath,p_orig_name,
                   strlen(p_orig_name)-strlen(tmp))
    XMOUNT_STRNAPP(glob_xmount_cfg.pVirtualImageInfoPath,p_orig_name,
                   strlen(p_orig_name)-strlen(tmp))
    if(glob_xmount_cfg.VirtImageType==VirtImageType_VMDK ||
       glob_xmount_cfg.VirtImageType==VirtImageType_VMDKS)
    {
      XMOUNT_STRNAPP(glob_xmount_cfg.pVirtualVmdkPath,p_orig_name,
                     strlen(p_orig_name)-strlen(tmp))
    }
    XMOUNT_STRAPP(glob_xmount_cfg.pVirtualImageInfoPath,".info")
  }

  // Add virtual file extensions
  switch(glob_xmount_cfg.VirtImageType) {
    case VirtImageType_DD:
      XMOUNT_STRAPP(glob_xmount_cfg.pVirtualImagePath,".dd")
      break;
    case VirtImageType_DMG:
      XMOUNT_STRAPP(glob_xmount_cfg.pVirtualImagePath,".dmg")
      break;
    case VirtImageType_VDI:
      XMOUNT_STRAPP(glob_xmount_cfg.pVirtualImagePath,".vdi")
      break;
    case VirtImageType_VHD:
      XMOUNT_STRAPP(glob_xmount_cfg.pVirtualImagePath,".vhd")
      break;
    case VirtImageType_VMDK:
    case VirtImageType_VMDKS:
      XMOUNT_STRAPP(glob_xmount_cfg.pVirtualImagePath,".dd")
      XMOUNT_STRAPP(glob_xmount_cfg.pVirtualVmdkPath,".vmdk")
      break;
    default:
      LOG_ERROR("Unknown virtual image type!\n")
      return FALSE;
  }

  LOG_DEBUG("Set virtual image name to \"%s\"\n",
            glob_xmount_cfg.pVirtualImagePath)
  LOG_DEBUG("Set virtual image info name to \"%s\"\n",
            glob_xmount_cfg.pVirtualImageInfoPath)
  if(glob_xmount_cfg.VirtImageType==VirtImageType_VMDK ||
     glob_xmount_cfg.VirtImageType==VirtImageType_VMDKS)
  {
    LOG_DEBUG("Set virtual vmdk name to \"%s\"\n",
              glob_xmount_cfg.pVirtualVmdkPath)
  }
  return TRUE;
}

/*
 * GetOrigImageSize:
 *   Get size of original image
 *
 * Params:
 *   p_size: Pointer to an uint64_t to which the size will be written to
 *   without_offset: If set to TRUE, returns the real size without substracting
 *                   a given offset.
 *
 * Returns:
 *   "TRUE" on success, "FALSE" on error
 */
static int GetOrigImageSize(uint64_t *p_size, int without_offset) {
  int ret;

  // Make sure to return correct values when dealing with only 32bit file sizes
  *p_size=0;

  // When size was already queryed, use old value rather than regetting value
  // from disk
  if(glob_xmount_cfg.OrigImageSize!=0 && !without_offset) {
    *p_size=glob_xmount_cfg.OrigImageSize;
    return TRUE;
  }

  // Get size of original image
  ret=glob_p_input_functions->Size(glob_p_input_image,p_size);
  if(ret!=0) {
    LOG_ERROR("Unable to determine input image size: %s!\n",
              glob_p_input_functions->GetErrorMessage(ret));
    return FALSE;
  }

  if(!without_offset) {
    // Substract given offset
    (*p_size)-=glob_xmount_cfg.orig_img_offset;

    // Save size so we have not to reget it from disk next time
    glob_xmount_cfg.OrigImageSize=*p_size;
  }

  return TRUE;
}

/*
 * GetVirtImageSize:
 *   Get size of the emulated image
 *
 * Params:
 *   p_size: Pointer to an uint64_t to which the size will be written to
 *
 * Returns:
 *   "TRUE" on success, "FALSE" on error
 */
static int GetVirtImageSize(uint64_t *p_size) {
  if(glob_xmount_cfg.VirtImageSize!=0) {
    *p_size=glob_xmount_cfg.VirtImageSize;
    return TRUE;
  }

  switch(glob_xmount_cfg.VirtImageType) {
    case VirtImageType_DD:
    case VirtImageType_DMG:
    case VirtImageType_VMDK:
    case VirtImageType_VMDKS:
      // Virtual image is a DD, DMG or VMDK file. Just return the size of the
      // original image
      if(!GetOrigImageSize(p_size,FALSE)) {
        LOG_ERROR("Couldn't get size of input image!\n")
        return FALSE;
      }
      break;
    case VirtImageType_VDI:
      // Virtual image is a VDI file. Get size of original image and add size
      // of VDI header etc.
      if(!GetOrigImageSize(p_size,FALSE)) {
        LOG_ERROR("Couldn't get size of input image!\n")
        return FALSE;
      }
      (*p_size)+=(sizeof(ts_VdiFileHeader)+glob_p_vdi_block_map_size);
      break;
    case VirtImageType_VHD:
      // Virtual image is a VHD file. Get size of original image and add size
      // of VHD footer.
      if(!GetOrigImageSize(p_size,FALSE)) {
        LOG_ERROR("Couldn't get size of input image!\n")
        return FALSE;
      }
      (*p_size)+=sizeof(ts_VhdFileHeader);
      break;
    default:
      LOG_ERROR("Unsupported image type!\n")
      return FALSE;
  }

  glob_xmount_cfg.VirtImageSize=*p_size;
  return TRUE;
}

/*
 * GetOrigImageData:
 *   Read data from original image
 *
 * Params:
 *   p_buf: Pointer to buffer to write read data to (Must be preallocated!)
 *   offset: Offset at which data should be read
 *   size: Size of data which should be read (Size of buffer)
 *
 * Returns:
 *   Number of read bytes on success or "-1" on error
 */
static int GetOrigImageData(char *p_buf, off_t offset, size_t size) {
  int ret;
  size_t to_read=0;
  uint64_t image_size=0;

  // Make sure we aren't reading past EOF of image file
  if(!GetOrigImageSize(&image_size,FALSE)) {
    LOG_ERROR("Couldn't get image size!\n")
    return -1;
  }
  if(offset>=image_size) {
    // Offset is beyond image size
    LOG_DEBUG("Offset is beyond image size.\n")
    return 0;
  }
  if(offset+size>image_size) {
    // Attempt to read data past EOF of image file
    to_read=image_size-offset;
    LOG_DEBUG("Attempt to read data past EOF. Corrected size from %zd"
              " to %zd.\n",size,to_read)
  } else to_read=size;

  // Read data from image file (adding input image offset if one was specified)
  ret=glob_p_input_functions->Read(glob_p_input_image,
                                   offset+glob_xmount_cfg.orig_img_offset,
                                   p_buf,
                                   to_read);
  if(ret!=0) {
    LOG_ERROR("Couldn't read %zd bytes from offset %" PRIu64 "!\n",
              to_read,
              offset,
              glob_p_input_functions->GetErrorMessage(ret));
    return -1;
  }

  return to_read;
}

/*
 * GetVirtVmdkData:
 *   Read data from virtual VMDK file
 *
 * Params:
 *   buf: Pointer to buffer to write read data to (Must be preallocated!)
 *   offset: Offset at which data should be read
 *   size: Size of data which should be read (Size of buffer)
 *
 * Returns:
 *   Number of read bytes on success or "-1" on error
 */
 /*
static int GetVirtualVmdkData(char *buf, off_t offset, size_t size) {
  uint32_t len;

  len=strlen(glob_p_vmdk_file);
  if(offset<len) {
    if(offset+size>len) {
      size=len-offset;
      LOG_DEBUG("Attempt to read past EOF of virtual vmdk file\n")
    }
    if(glob_xmount_cfg.Writable==TRUE &&
       glob_p_cache_header->VmdkFileCached==TRUE)
    {
      // VMDK file is cached. Read data from cache file
      // TODO: Read data from cache file
    } else {
      // No write support or VMDK file not cached.
      memcpy(buf,glob_p_vmdk_file+offset,size);
      LOG_DEBUG("Read %" PRIu64 " bytes at offset %" PRIu64
                " from virtual vmdk file\n",size,offset)
    }
  } else {
    LOG_DEBUG("Attempt to read past EOF of virtual vmdk file\n");
    return -1;
  }
  return size;
}
*/

/*
 * GetVirtImageData:
 *   Read data from virtual image
 *
 * Params:
 *   p_buf: Pointer to buffer to write read data to (Must be preallocated!)
 *   offset: Offset at which data should be read
 *   size: Size of data which should be read (Size of buffer)
 *
 * Returns:
 *   Number of read bytes on success or "-1" on error
 */
static int GetVirtImageData(char *p_buf, off_t offset, size_t size) {
  uint32_t cur_block=0;
  uint64_t orig_image_size, virt_image_size;
  size_t to_read=0, cur_to_read=0;
  off_t file_off=offset, block_off=0;
  size_t to_read_later=0;

  // Get virtual image size
  if(!GetVirtImageSize(&virt_image_size)) {
    LOG_ERROR("Couldn't get virtual image size!\n")
    return -1;
  }

  if(offset>=virt_image_size) {
    LOG_ERROR("Attempt to read beyond virtual image EOF!\n")
    return -1;
  }

  if(offset+size>virt_image_size) {
    LOG_DEBUG("Attempt to read pas EOF of virtual image file\n")
    size=virt_image_size-offset;
  }

  to_read=size;

  if(!GetOrigImageSize(&orig_image_size,FALSE)) {
    LOG_ERROR("Couldn't get original image size!")
    return 0;
  }

  // Read virtual image type specific data preceeding original image data
  switch(glob_xmount_cfg.VirtImageType) {
    case VirtImageType_DD:
    case VirtImageType_DMG:
    case VirtImageType_VMDK:
    case VirtImageType_VMDKS:
      break;
    case VirtImageType_VDI:
      if(file_off<glob_vdi_header_size) {
        if(file_off+to_read>glob_vdi_header_size) {
          cur_to_read=glob_vdi_header_size-file_off;
        } else {
          cur_to_read=to_read;
        }
        if(glob_xmount_cfg.Writable==TRUE &&
           glob_p_cache_header->VdiFileHeaderCached==TRUE)
        {
          // VDI header was already cached
          if(fseeko(glob_p_cache_file,
                    glob_p_cache_header->pVdiFileHeader+file_off,
                    SEEK_SET)!=0)
          {
            LOG_ERROR("Couldn't seek to cached VDI header at offset %"
                      PRIu64 "\n",glob_p_cache_header->pVdiFileHeader+file_off)
            return 0;
          }
          if(fread(p_buf,cur_to_read,1,glob_p_cache_file)!=1) {
            LOG_ERROR("Couldn't read %zu bytes from cache file at offset %"
                      PRIu64 "\n",cur_to_read,
                      glob_p_cache_header->pVdiFileHeader+file_off)
            return 0;
          }
          LOG_DEBUG("Read %zd bytes from cached VDI header at offset %"
                    PRIu64 " at cache file offset %" PRIu64 "\n",
                    cur_to_read,file_off,
                    glob_p_cache_header->pVdiFileHeader+file_off)
        } else {
          // VDI header isn't cached
          memcpy(p_buf,((char*)glob_p_vdi_header)+file_off,cur_to_read);
          LOG_DEBUG("Read %zd bytes at offset %" PRIu64
                    " from virtual VDI header\n",cur_to_read,
                    file_off)
        }
        if(to_read==cur_to_read) return to_read;
        else {
          // Adjust values to read from original image
          to_read-=cur_to_read;
          p_buf+=cur_to_read;
          file_off=0;
        }
      } else file_off-=glob_vdi_header_size;
      break;
    case VirtImageType_VHD:
      // When emulating VHD, make sure the while loop below only reads data
      // available in the original image. Any VHD footer data must be read
      // afterwards.
      if(file_off>=orig_image_size) {
        to_read_later=to_read;
        to_read=0;
      } else if((file_off+to_read)>orig_image_size) {
        to_read_later=(file_off+to_read)-orig_image_size;
        to_read-=to_read_later;
      }
      break;
  }

  // Calculate block to read data from
  cur_block=file_off/CACHE_BLOCK_SIZE;
  block_off=file_off%CACHE_BLOCK_SIZE;
  
  // Read image data
  while(to_read!=0) {
    // Calculate how many bytes we have to read from this block
    if(block_off+to_read>CACHE_BLOCK_SIZE) {
      cur_to_read=CACHE_BLOCK_SIZE-block_off;
    } else cur_to_read=to_read;
    if(glob_xmount_cfg.Writable==TRUE &&
       glob_p_cache_blkidx[cur_block].Assigned==TRUE)
    {
      // Write support enabled and need to read altered data from cachefile
      if(fseeko(glob_p_cache_file,
                glob_p_cache_blkidx[cur_block].off_data+block_off,
                SEEK_SET)!=0)
      {
        LOG_ERROR("Couldn't seek to offset %" PRIu64
                  " in cache file\n")
        return -1;
      }
      if(fread(p_buf,cur_to_read,1,glob_p_cache_file)!=1) {
        LOG_ERROR("Couldn't read data from cache file!\n")
        return -1;
      }
      LOG_DEBUG("Read %zd bytes at offset %" PRIu64
                " from cache file\n",cur_to_read,file_off)
    } else {
      // No write support or data not cached
      if(GetOrigImageData(p_buf,
                          file_off,
                          cur_to_read)!=cur_to_read)
      {
        LOG_ERROR("Couldn't read data from input image!\n")
        return -1;
      }
      LOG_DEBUG("Read %zd bytes at offset %" PRIu64
                " from original image file\n",cur_to_read,
                file_off)
    }
    cur_block++;
    block_off=0;
    p_buf+=cur_to_read;
    to_read-=cur_to_read;
    file_off+=cur_to_read;
  }

  if(to_read_later!=0) {
    // Read virtual image type specific data following original image data
    switch(glob_xmount_cfg.VirtImageType) {
      case VirtImageType_DD:
      case VirtImageType_DMG:
      case VirtImageType_VMDK:
      case VirtImageType_VMDKS:
      case VirtImageType_VDI:
        break;
      case VirtImageType_VHD:
        // Micro$oft has choosen to use a footer rather then a header.
        if(glob_xmount_cfg.Writable==TRUE &&
           glob_p_cache_header->VhdFileHeaderCached==TRUE)
        {
          // VHD footer was already cached
          if(fseeko(glob_p_cache_file,
                    glob_p_cache_header->pVhdFileHeader+(file_off-orig_image_size),
                    SEEK_SET)!=0)
          {
            LOG_ERROR("Couldn't seek to cached VHD footer at offset %"
                      PRIu64 "\n",
                      glob_p_cache_header->pVhdFileHeader+
                        (file_off-orig_image_size))
            return 0;
          }
          if(fread(p_buf,to_read_later,1,glob_p_cache_file)!=1) {
            LOG_ERROR("Couldn't read %zu bytes from cache file at offset %"
                      PRIu64 "\n",to_read_later,
                      glob_p_cache_header->pVhdFileHeader+
                        (file_off-orig_image_size))
            return 0;
          }
          LOG_DEBUG("Read %zd bytes from cached VHD footer at offset %"
                    PRIu64 " at cache file offset %" PRIu64 "\n",
                    to_read_later,(file_off-orig_image_size),
                    glob_p_cache_header->pVhdFileHeader+(file_off-orig_image_size))
        } else {
          // VHD header isn't cached
          memcpy(p_buf,
                 ((char*)glob_p_vhd_header)+(file_off-orig_image_size),
                 to_read_later);
          LOG_DEBUG("Read %zd bytes at offset %" PRIu64
                    " from virtual VHD header\n",
                    to_read_later,
                    (file_off-orig_image_size))
        }
        break;
    }
  }

  return size;
}

/*
 * SetVdiFileHeaderData:
 *   Write data to virtual VDI file header
 *
 * Params:
 *   p_buf: Buffer containing data to write
 *   offset: Offset of changes
 *   size: Amount of bytes to write
 *
 * Returns:
 *   Number of written bytes on success or "-1" on error
 */
static int SetVdiFileHeaderData(char *p_buf,off_t offset,size_t size) {
  if(offset+size>glob_vdi_header_size) size=glob_vdi_header_size-offset;
  LOG_DEBUG("Need to cache %zu bytes at offset %" PRIu64
            " from VDI header\n",size,offset)
  if(glob_p_cache_header->VdiFileHeaderCached==1) {
    // Header was already cached
    if(fseeko(glob_p_cache_file,
              glob_p_cache_header->pVdiFileHeader+offset,
              SEEK_SET)!=0)
    {
      LOG_ERROR("Couldn't seek to cached VDI header at address %"
                PRIu64 "\n",glob_p_cache_header->pVdiFileHeader+offset)
      return -1;
    }
    if(fwrite(p_buf,size,1,glob_p_cache_file)!=1) {
      LOG_ERROR("Couldn't write %zu bytes to cache file at offset %"
                PRIu64 "\n",size,
                glob_p_cache_header->pVdiFileHeader+offset)
      return -1;
    }
    LOG_DEBUG("Wrote %zd bytes at offset %" PRIu64 " to cache file\n",
              size,glob_p_cache_header->pVdiFileHeader+offset)
  } else {
    // Header wasn't already cached.
    if(fseeko(glob_p_cache_file,
              0,
              SEEK_END)!=0)
    {
      LOG_ERROR("Couldn't seek to end of cache file!")
      return -1;
    }
    glob_p_cache_header->pVdiFileHeader=ftello(glob_p_cache_file);
    LOG_DEBUG("Caching whole VDI header\n")
    if(offset>0) {
      // Changes do not begin at offset 0, need to prepend with data from
      // VDI header
      if(fwrite((char*)glob_p_vdi_header,offset,1,glob_p_cache_file)!=1) {
        LOG_ERROR("Error while writing %" PRIu64 " bytes "
                  "to cache file at offset %" PRIu64 "!\n",
                  offset,
                  glob_p_cache_header->pVdiFileHeader);
        return -1;
      }
      LOG_DEBUG("Prepended changed data with %" PRIu64
                " bytes at cache file offset %" PRIu64 "\n",
                offset,glob_p_cache_header->pVdiFileHeader)
    }
    // Cache changed data
    if(fwrite(p_buf,size,1,glob_p_cache_file)!=1) {
      LOG_ERROR("Couldn't write %zu bytes to cache file at offset %"
                PRIu64 "\n",size,
                glob_p_cache_header->pVdiFileHeader+offset)
      return -1;
    }
    LOG_DEBUG("Wrote %zu bytes of changed data to cache file offset %"
              PRIu64 "\n",size,
              glob_p_cache_header->pVdiFileHeader+offset)
    if(offset+size!=glob_vdi_header_size) {
      // Need to append data from VDI header to cache whole data struct
      if(fwrite(((char*)glob_p_vdi_header)+offset+size,
                glob_vdi_header_size-(offset+size),
                1,
                glob_p_cache_file)!=1)
      {
        LOG_ERROR("Couldn't write %zu bytes to cache file at offset %"
                  PRIu64 "\n",glob_vdi_header_size-(offset+size),
                  (uint64_t)(glob_p_cache_header->pVdiFileHeader+offset+size))
        return -1;
      }
      LOG_DEBUG("Appended %" PRIu32
                " bytes to changed data at cache file offset %"
                PRIu64 "\n",glob_vdi_header_size-(offset+size),
                glob_p_cache_header->pVdiFileHeader+offset+size)
    }
    // Mark header as cached and update header in cache file
    glob_p_cache_header->VdiFileHeaderCached=1;
    if(fseeko(glob_p_cache_file,0,SEEK_SET)!=0) {
      LOG_ERROR("Couldn't seek to offset 0 of cache file!\n")
      return -1;
    }
    if(fwrite((char*)glob_p_cache_header,sizeof(ts_CacheFileHeader),1,glob_p_cache_file)!=1) {
      LOG_ERROR("Couldn't write changed cache file header!\n")
      return -1;
    }
  }
  // All important data has been written, now flush all buffers to make
  // sure data is written to cache file
  fflush(glob_p_cache_file);
#ifndef __APPLE__
  ioctl(fileno(glob_p_cache_file),BLKFLSBUF,0);
#endif
  return size;
}

/*
 * SetVhdFileHeaderData:
 *   Write data to virtual VHD file footer
 *
 * Params:
 *   p_buf: Buffer containing data to write
 *   offset: Offset of changes
 *   size: Amount of bytes to write
 *
 * Returns:
 *   Number of written bytes on success or "-1" on error
 */
static int SetVhdFileHeaderData(char *p_buf,off_t offset,size_t size) {
  LOG_DEBUG("Need to cache %zu bytes at offset %" PRIu64
            " from VHD footer\n",size,offset)
  if(glob_p_cache_header->VhdFileHeaderCached==1) {
    // Header has already been cached
    if(fseeko(glob_p_cache_file,
              glob_p_cache_header->pVhdFileHeader+offset,
              SEEK_SET)!=0)
    {
      LOG_ERROR("Couldn't seek to cached VHD header at address %"
                PRIu64 "\n",glob_p_cache_header->pVhdFileHeader+offset)
      return -1;
    }
    if(fwrite(p_buf,size,1,glob_p_cache_file)!=1) {
      LOG_ERROR("Couldn't write %zu bytes to cache file at offset %"
                PRIu64 "\n",size,
                glob_p_cache_header->pVhdFileHeader+offset)
      return -1;
    }
    LOG_DEBUG("Wrote %zd bytes at offset %" PRIu64 " to cache file\n",
              size,glob_p_cache_header->pVhdFileHeader+offset)
  } else {
    // Header hasn't been cached yet.
    if(fseeko(glob_p_cache_file,
              0,
              SEEK_END)!=0)
    {
      LOG_ERROR("Couldn't seek to end of cache file!")
      return -1;
    }
    glob_p_cache_header->pVhdFileHeader=ftello(glob_p_cache_file);
    LOG_DEBUG("Caching whole VHD header\n")
    if(offset>0) {
      // Changes do not begin at offset 0, need to prepend with data from
      // VHD header
      if(fwrite((char*)glob_p_vhd_header,offset,1,glob_p_cache_file)!=1) {
        LOG_ERROR("Error while writing %" PRIu64 " bytes "
                  "to cache file at offset %" PRIu64 "!\n",
                  offset,
                  glob_p_cache_header->pVhdFileHeader);
        return -1;
      }
      LOG_DEBUG("Prepended changed data with %" PRIu64
                " bytes at cache file offset %" PRIu64 "\n",
                offset,glob_p_cache_header->pVhdFileHeader)
    }
    // Cache changed data
    if(fwrite(p_buf,size,1,glob_p_cache_file)!=1) {
      LOG_ERROR("Couldn't write %zu bytes to cache file at offset %"
                PRIu64 "\n",size,
                glob_p_cache_header->pVhdFileHeader+offset)
      return -1;
    }
    LOG_DEBUG("Wrote %zu bytes of changed data to cache file offset %"
              PRIu64 "\n",size,
              glob_p_cache_header->pVhdFileHeader+offset)
    if(offset+size!=sizeof(ts_VhdFileHeader)) {
      // Need to append data from VHD header to cache whole data struct
      if(fwrite(((char*)glob_p_vhd_header)+offset+size,
                sizeof(ts_VhdFileHeader)-(offset+size),
                1,
                glob_p_cache_file)!=1)
      {
        LOG_ERROR("Couldn't write %zu bytes to cache file at offset %"
                  PRIu64 "\n",sizeof(ts_VhdFileHeader)-(offset+size),
                  (uint64_t)(glob_p_cache_header->pVhdFileHeader+offset+size))
        return -1;
      }
      LOG_DEBUG("Appended %" PRIu32
                " bytes to changed data at cache file offset %"
                PRIu64 "\n",sizeof(ts_VhdFileHeader)-(offset+size),
                glob_p_cache_header->pVhdFileHeader+offset+size)
    }
    // Mark header as cached and update header in cache file
    glob_p_cache_header->VhdFileHeaderCached=1;
    if(fseeko(glob_p_cache_file,0,SEEK_SET)!=0) {
      LOG_ERROR("Couldn't seek to offset 0 of cache file!\n")
      return -1;
    }
    if(fwrite((char*)glob_p_cache_header,sizeof(ts_CacheFileHeader),1,glob_p_cache_file)!=1) {
      LOG_ERROR("Couldn't write changed cache file header!\n")
      return -1;
    }
  }
  // All important data has been written, now flush all buffers to make
  // sure data is written to cache file
  fflush(glob_p_cache_file);
#ifndef __APPLE__
  ioctl(fileno(glob_p_cache_file),BLKFLSBUF,0);
#endif
  return size;
}

/*
 * SetVirtImageData:
 *   Write data to virtual image
 *
 * Params:
 *   p_buf: Buffer containing data to write
 *   offset: Offset to start writing at
 *   size: Size of data to be written
 *
 * Returns:
 *   Number of written bytes on success or "-1" on error
 */
static int SetVirtImageData(const char *p_buf, off_t offset, size_t size) {
  uint64_t CurBlock=0;
  uint64_t VirtImageSize;
  uint64_t OrigImageSize;
  size_t ToWrite=0;
  size_t to_write_later=0;
  size_t CurToWrite=0;
  off_t FileOff=offset;
  off_t BlockOff=0;
  char *WriteBuf=(char*)p_buf;
  char *p_buf2;
  ssize_t ret;

  // Get virtual image size
  if(!GetVirtImageSize(&VirtImageSize)) {
    LOG_ERROR("Couldn't get virtual image size!\n")
    return -1;
  }

  if(offset>=VirtImageSize) {
    LOG_ERROR("Attempt to write beyond EOF of virtual image file!\n")
    return -1;
  }

  if(offset+size>VirtImageSize) {
    LOG_DEBUG("Attempt to write past EOF of virtual image file\n")
    size=VirtImageSize-offset;
  }

  ToWrite=size;

  // Get original image size
  if(!GetOrigImageSize(&OrigImageSize,FALSE)) {
    LOG_ERROR("Couldn't get original image size!\n")
    return -1;
  }

  // Cache virtual image type specific data preceeding original image data
  switch(glob_xmount_cfg.VirtImageType) {
    case VirtImageType_DD:
    case VirtImageType_DMG:
    case VirtImageType_VMDK:
    case VirtImageType_VMDKS:
      break;
    case VirtImageType_VDI:
      if(FileOff<glob_vdi_header_size) {
        ret=SetVdiFileHeaderData(WriteBuf,FileOff,ToWrite);
        if(ret==-1) {
          LOG_ERROR("Couldn't write data to virtual VDI file header!\n")
          return -1;
        }
        if(ret==ToWrite) return ToWrite;
        else {
          ToWrite-=ret;
          WriteBuf+=ret;
          FileOff=0;
        }
      } else FileOff-=glob_vdi_header_size;
      break;
    case VirtImageType_VHD:
      // When emulating VHD, make sure the while loop below only writes data
      // available in the original image. Any VHD footer data must be written
      // afterwards.
      if(FileOff>=OrigImageSize) {
        to_write_later=ToWrite;
        ToWrite=0;
      } else if((FileOff+ToWrite)>OrigImageSize) {
        to_write_later=(FileOff+ToWrite)-OrigImageSize;
        ToWrite-=to_write_later;
      }
      break;
  }

  // Calculate block to write data to
  CurBlock=FileOff/CACHE_BLOCK_SIZE;
  BlockOff=FileOff%CACHE_BLOCK_SIZE;
  
  while(ToWrite!=0) {
    // Calculate how many bytes we have to write to this block
    if(BlockOff+ToWrite>CACHE_BLOCK_SIZE) {
      CurToWrite=CACHE_BLOCK_SIZE-BlockOff;
    } else CurToWrite=ToWrite;
    if(glob_p_cache_blkidx[CurBlock].Assigned==1) {
      // Block was already cached
      // Seek to data offset in cache file
      if(fseeko(glob_p_cache_file,
             glob_p_cache_blkidx[CurBlock].off_data+BlockOff,
             SEEK_SET)!=0)
      {
        LOG_ERROR("Couldn't seek to cached block at address %" PRIu64 "\n",
                  glob_p_cache_blkidx[CurBlock].off_data+BlockOff)
        return -1;
      }
      if(fwrite(WriteBuf,CurToWrite,1,glob_p_cache_file)!=1) {
        LOG_ERROR("Error while writing %zu bytes "
                  "to cache file at offset %" PRIu64 "!\n",
                  CurToWrite,
                  glob_p_cache_blkidx[CurBlock].off_data+BlockOff);
        return -1;
      }
      LOG_DEBUG("Wrote %zd bytes at offset %" PRIu64
                " to cache file\n",CurToWrite,
                glob_p_cache_blkidx[CurBlock].off_data+BlockOff)
    } else {
      // Uncached block. Need to cache entire new block
      // Seek to end of cache file to append new cache block
      fseeko(glob_p_cache_file,0,SEEK_END);
      glob_p_cache_blkidx[CurBlock].off_data=ftello(glob_p_cache_file);
      if(BlockOff!=0) {
        // Changed data does not begin at block boundry. Need to prepend
        // with data from virtual image file
        XMOUNT_MALLOC(p_buf2,char*,BlockOff*sizeof(char))
        if(GetOrigImageData(p_buf2,FileOff-BlockOff,BlockOff)!=BlockOff) {
          LOG_ERROR("Couldn't read data from original image file!\n")
          return -1;
        }
        if(fwrite(p_buf2,BlockOff,1,glob_p_cache_file)!=1) {
          LOG_ERROR("Couldn't writing %" PRIu64 " bytes "
                    "to cache file at offset %" PRIu64 "!\n",
                    BlockOff,
                    glob_p_cache_blkidx[CurBlock].off_data);
          return -1;
        }
        LOG_DEBUG("Prepended changed data with %" PRIu64
                  " bytes from virtual image file at offset %" PRIu64
                  "\n",BlockOff,FileOff-BlockOff)
        free(p_buf2);
      }
      if(fwrite(WriteBuf,CurToWrite,1,glob_p_cache_file)!=1) {
        LOG_ERROR("Error while writing %zd bytes "
                  "to cache file at offset %" PRIu64 "!\n",
                  CurToWrite,
                  glob_p_cache_blkidx[CurBlock].off_data+BlockOff);
        return -1;
      }
      if(BlockOff+CurToWrite!=CACHE_BLOCK_SIZE) {
        // Changed data does not end at block boundry. Need to append
        // with data from virtual image file
        XMOUNT_MALLOC(p_buf2,char*,(CACHE_BLOCK_SIZE-
                                 (BlockOff+CurToWrite))*sizeof(char))
        memset(p_buf2,0,CACHE_BLOCK_SIZE-(BlockOff+CurToWrite));
        if((FileOff-BlockOff)+CACHE_BLOCK_SIZE>OrigImageSize) {
          // Original image is smaller than full cache block
          if(GetOrigImageData(p_buf2,
               FileOff+CurToWrite,
               OrigImageSize-(FileOff+CurToWrite))!=
             OrigImageSize-(FileOff+CurToWrite))
          {
            LOG_ERROR("Couldn't read data from virtual image file!\n")
            return -1;
          }
        } else {
          if(GetOrigImageData(p_buf2,
               FileOff+CurToWrite,
               CACHE_BLOCK_SIZE-(BlockOff+CurToWrite))!=
             CACHE_BLOCK_SIZE-(BlockOff+CurToWrite))
          {
            LOG_ERROR("Couldn't read data from virtual image file!\n")
            return -1;
          }
        }
        if(fwrite(p_buf2,
                  CACHE_BLOCK_SIZE-(BlockOff+CurToWrite),
                  1,
                  glob_p_cache_file)!=1)
        {
          LOG_ERROR("Error while writing %zd bytes "
                    "to cache file at offset %" PRIu64 "!\n",
                    CACHE_BLOCK_SIZE-(BlockOff+CurToWrite),
                    glob_p_cache_blkidx[CurBlock].off_data+
                      BlockOff+CurToWrite);
          return -1;
        }
        free(p_buf2);
      }
      // All important data for this cache block has been written,
      // flush all buffers and mark cache block as assigned
      fflush(glob_p_cache_file);
#ifndef __APPLE__
      ioctl(fileno(glob_p_cache_file),BLKFLSBUF,0);
#endif
      glob_p_cache_blkidx[CurBlock].Assigned=1;
      // Update cache block index entry in cache file
      fseeko(glob_p_cache_file,
             sizeof(ts_CacheFileHeader)+(CurBlock*sizeof(ts_CacheFileBlockIndex)),
             SEEK_SET);
      if(fwrite(&(glob_p_cache_blkidx[CurBlock]),
                sizeof(ts_CacheFileBlockIndex),
                1,
                glob_p_cache_file)!=1)
      {
        LOG_ERROR("Couldn't update cache file block index!\n");
        return -1;
      }
      LOG_DEBUG("Updated cache file block index: Number=%" PRIu64
                ", Data offset=%" PRIu64 "\n",CurBlock,
                glob_p_cache_blkidx[CurBlock].off_data);
    }
    // Flush buffers
    fflush(glob_p_cache_file);
#ifndef __APPLE__
    ioctl(fileno(glob_p_cache_file),BLKFLSBUF,0);
#endif
    BlockOff=0;
    CurBlock++;
    WriteBuf+=CurToWrite;
    ToWrite-=CurToWrite;
    FileOff+=CurToWrite;
  }

  if(to_write_later!=0) {
    // Cache virtual image type specific data preceeding original image data
    switch(glob_xmount_cfg.VirtImageType) {
      case VirtImageType_DD:
      case VirtImageType_DMG:
      case VirtImageType_VMDK:
      case VirtImageType_VMDKS:
      case VirtImageType_VDI:
        break;
      case VirtImageType_VHD:
        // Micro$oft has choosen to use a footer rather then a header.
        ret=SetVhdFileHeaderData(WriteBuf,FileOff-OrigImageSize,to_write_later);
        if(ret==-1) {
          LOG_ERROR("Couldn't write data to virtual VHD file footer!\n")
          return -1;
        }
        break;
    }
  }

  return size;
}

/*
 * GetVirtFileAccess:
 *   FUSE access implementation
 *
 * Params:
 *   path: Path of file to get attributes from
 *   perm: Requested permissisons
 *
 * Returns:
 *   "0" on success, negated error code on error
 */
/*
static int GetVirtFileAccess(const char *path, int perm) {
  // TODO: Implement propper file permission handling
  // http://www.cs.cf.ac.uk/Dave/C/node20.html
  // Values for the second argument to access.
  // These may be OR'd together.
  //#define	R_OK	4		// Test for read permission.
  //#define	W_OK	2		// Test for write permission.
  //#define	X_OK	1		// Test for execute permission.
  //#define	F_OK	0		// Test for existence.
  return 0;
}
*/

/*
 * GetVirtFileAttr:
 *   FUSE getattr implementation
 *
 * Params:
 *   path: Path of file to get attributes from
 *   p_stat: Pointer to stat structure to save attributes to
 *
 * Returns:
 *   "0" on success, negated error code on error
 */
static int GetVirtFileAttr(const char *path, struct stat *p_stat) {
  memset(p_stat,0,sizeof(struct stat));
  if(strcmp(path,"/")==0) {
    // Attributes of mountpoint
    p_stat->st_mode=S_IFDIR | 0777;
    p_stat->st_nlink=2;
  } else if(strcmp(path,glob_xmount_cfg.pVirtualImagePath)==0) {
    // Attributes of virtual image
    if(!glob_xmount_cfg.Writable) p_stat->st_mode=S_IFREG | 0444;
    else p_stat->st_mode=S_IFREG | 0666;
    p_stat->st_nlink=1;
    // Get virtual image file size
    if(!GetVirtImageSize((uint64_t*)&(p_stat->st_size))) {
      LOG_ERROR("Couldn't get image size!\n");
      return -ENOENT;
    }
    if(glob_xmount_cfg.VirtImageType==VirtImageType_VHD) {
      // Make sure virtual image seems to be fully allocated (not sparse file).
      // Without this, Windows won't attach the vhd file!
      p_stat->st_blocks=p_stat->st_size/512;
      if(p_stat->st_size%512!=0) p_stat->st_blocks++;
    }
  } else if(strcmp(path,glob_xmount_cfg.pVirtualImageInfoPath)==0) {
    // Attributes of virtual image info file
    p_stat->st_mode=S_IFREG | 0444;
    p_stat->st_nlink=1;
    // Get virtual image info file size
    if(glob_p_info_file!=NULL) {
      p_stat->st_size=strlen(glob_p_info_file);
    } else p_stat->st_size=0;
  } else if(glob_xmount_cfg.VirtImageType==VirtImageType_VMDK ||
            glob_xmount_cfg.VirtImageType==VirtImageType_VMDKS)
  {
    // Some special files only present when emulating VMDK files
    if(strcmp(path,glob_xmount_cfg.pVirtualVmdkPath)==0) {
      // Attributes of virtual vmdk file
      if(!glob_xmount_cfg.Writable) p_stat->st_mode=S_IFREG | 0444;
      else p_stat->st_mode=S_IFREG | 0666;
      p_stat->st_nlink=1;
      // Get virtual image info file size
      if(glob_p_vmdk_file!=NULL) {
        p_stat->st_size=glob_vmdk_file_size;
      } else p_stat->st_size=0;
    } else if(glob_p_vmdk_lockdir1!=NULL &&
              strcmp(path,glob_p_vmdk_lockdir1)==0)
    {
      p_stat->st_mode=S_IFDIR | 0777;
      p_stat->st_nlink=2;
    } else if(glob_p_vmdk_lockdir2!=NULL &&
              strcmp(path,glob_p_vmdk_lockdir2)==0)
    {
      p_stat->st_mode=S_IFDIR | 0777;
      p_stat->st_nlink=2;
    } else if(glob_p_vmdk_lockfile_name!=NULL &&
              strcmp(path,glob_p_vmdk_lockfile_name)==0)
    {
      p_stat->st_mode=S_IFREG | 0666;
      if(glob_p_vmdk_lockfile_name!=NULL) {
        p_stat->st_size=strlen(glob_p_vmdk_lockfile_name);
      } else p_stat->st_size=0;
    } else return -ENOENT;
  } else return -ENOENT;
  // Set uid and gid of all files to uid and gid of current process
  p_stat->st_uid=getuid();
  p_stat->st_gid=getgid();
  return 0;
}

/*
 * CreateVirtDir:
 *   FUSE mkdir implementation
 *
 * Params:
 *   p_path: Directory path
 *   mode: Directory permissions
 *
 * Returns:
 *   "0" on success, negated error code on error
 */
static int CreateVirtDir(const char *p_path, mode_t mode) {
  // Only allow creation of VMWare's lock directories
  if(glob_xmount_cfg.VirtImageType==VirtImageType_VMDK ||
     glob_xmount_cfg.VirtImageType==VirtImageType_VMDKS)
  {
    if(glob_p_vmdk_lockdir1==NULL)  {
      char aVmdkLockDir[strlen(glob_xmount_cfg.pVirtualVmdkPath)+5];
      sprintf(aVmdkLockDir,"%s.lck",glob_xmount_cfg.pVirtualVmdkPath);
      if(strcmp(p_path,aVmdkLockDir)==0) {
        LOG_DEBUG("Creating virtual directory \"%s\"\n",aVmdkLockDir)
        XMOUNT_STRSET(glob_p_vmdk_lockdir1,aVmdkLockDir)
        return 0;
      } else {
        LOG_ERROR("Attempt to create illegal directory \"%s\"!\n",p_path)
        LOG_DEBUG("Supposed: %s\n",aVmdkLockDir)
        return -1;
      }
    } else if(glob_p_vmdk_lockdir2==NULL &&
              strncmp(p_path,glob_p_vmdk_lockdir1,strlen(glob_p_vmdk_lockdir1))==0)
    {
      LOG_DEBUG("Creating virtual directory \"%s\"\n",p_path)
      XMOUNT_STRSET(glob_p_vmdk_lockdir2,p_path)
      return 0;
    } else {
      LOG_ERROR("Attempt to create illegal directory \"%s\"!\n",p_path)
      LOG_DEBUG("Compared to first %u chars of \"%s\"\n",strlen(glob_p_vmdk_lockdir1),glob_p_vmdk_lockdir1)
      return -1;
    }
  }
  LOG_ERROR("Attempt to create directory \"%s\" "
            "on read-only filesystem!\n",p_path)
  return -1;
}

/*
 * CreateVirtFile:
 *   FUSE create implementation.
 *   Only allows to create VMWare's lock file!
 *
 * Params:
 *   p_path: File to create
 *   mode: File mode
 *   dev: ??? but not used
 *
 * Returns:
 *   "0" on success, negated error code on error
 */
static int CreateVirtFile(const char *p_path,
                          mode_t mode,
                          dev_t dev)
{
  if((glob_xmount_cfg.VirtImageType==VirtImageType_VMDK ||
      glob_xmount_cfg.VirtImageType==VirtImageType_VMDKS) &&
     glob_p_vmdk_lockdir1!=NULL && glob_p_vmdk_lockfile_name==NULL)
  {
    LOG_DEBUG("Creating virtual file \"%s\"\n",p_path)
    XMOUNT_STRSET(glob_p_vmdk_lockfile_name,p_path);
    return 0;
  } else {
    LOG_ERROR("Attempt to create illegal file \"%s\"\n",p_path)
    return -1;
  }
}

/*
 * GetVirtFiles:
 *   FUSE readdir implementation
 *
 * Params:
 *   p_path: Path from where files should be listed
 *   buf: Buffer to write file entrys to
 *   filler: Function to write file entrys to buffer
 *   offset: ??? but not used
 *   fi: ??? but not used
 *
 * Returns:
 *   "0" on success, negated error code on error
 */
static int GetVirtFiles(const char *p_path,
                        void *buf,
                        fuse_fill_dir_t filler,
                        off_t offset,
                        struct fuse_file_info *fi)
{
  (void)offset;
  (void)fi;

  if(strcmp(p_path,"/")==0) {
    // Add std . and .. entrys
    filler(buf,".",NULL,0);
    filler(buf,"..",NULL,0);
    // Add our virtual files (p+1 to ignore starting "/")
    filler(buf,glob_xmount_cfg.pVirtualImagePath+1,NULL,0);
    filler(buf,glob_xmount_cfg.pVirtualImageInfoPath+1,NULL,0);
    if(glob_xmount_cfg.VirtImageType==VirtImageType_VMDK ||
       glob_xmount_cfg.VirtImageType==VirtImageType_VMDKS)
    {
      // For VMDK's, we use an additional descriptor file
      filler(buf,glob_xmount_cfg.pVirtualVmdkPath+1,NULL,0);
      // And there could also be a lock directory
      if(glob_p_vmdk_lockdir1!=NULL) {
        filler(buf,glob_p_vmdk_lockdir1+1,NULL,0);
      }
    }
  } else if(glob_xmount_cfg.VirtImageType==VirtImageType_VMDK ||
            glob_xmount_cfg.VirtImageType==VirtImageType_VMDKS)
  {
    // For VMDK emulation, there could be a lock directory
    if(glob_p_vmdk_lockdir1!=NULL && strcmp(p_path,glob_p_vmdk_lockdir1)==0) {
      filler(buf,".",NULL,0);
      filler(buf,"..",NULL,0);
      if(glob_p_vmdk_lockfile_name!=NULL) {
        filler(buf,glob_p_vmdk_lockfile_name+strlen(glob_p_vmdk_lockdir1)+1,NULL,0);
      }
    } else if(glob_p_vmdk_lockdir2!=NULL &&
              strcmp(p_path,glob_p_vmdk_lockdir2)==0)
    {
      filler(buf,".",NULL,0);
      filler(buf,"..",NULL,0);
    } else return -ENOENT;
  } else return -ENOENT;
  return 0;
}

/*
 * OpenVirtFile:
 *   FUSE open implementation
 *
 * Params:
 *   p_path: Path to file to open
 *   p_fi: File info struct
 *
 * Returns:
 *   "0" on success, negated error code on error
 */
static int OpenVirtFile(const char *p_path, struct fuse_file_info *p_fi) {
  if(strcmp(p_path,glob_xmount_cfg.pVirtualImagePath)==0 ||
     strcmp(p_path,glob_xmount_cfg.pVirtualImageInfoPath)==0)
  {
    // Check open permissions
    if(!glob_xmount_cfg.Writable && (p_fi->flags & 3)!=O_RDONLY) {
      // Attempt to open a read-only file for writing
      LOG_DEBUG("Attempt to open the read-only file \"%s\" for writing.\n",p_path)
      return -EACCES;
    }
    return 0;
  } else if(glob_xmount_cfg.VirtImageType==VirtImageType_VMDK ||
            glob_xmount_cfg.VirtImageType==VirtImageType_VMDKS)
  {
    if(strcmp(p_path,glob_xmount_cfg.pVirtualVmdkPath)==0) {
      // Check open permissions
      if(!glob_xmount_cfg.Writable && (p_fi->flags & 3)!=O_RDONLY) {
        // Attempt to open a read-only file for writing
        LOG_DEBUG("Attempt to open the read-only file \"%s\" for writing.\n",p_path)
        return -EACCES;
      }
      return 0;
    } else if(glob_p_vmdk_lockfile_name!=NULL &&
              strcmp(p_path,glob_p_vmdk_lockfile_name)==0)
    {
      // Check open permissions
      if(!glob_xmount_cfg.Writable && (p_fi->flags & 3)!=O_RDONLY) {
        // Attempt to open a read-only file for writing
        LOG_DEBUG("Attempt to open the read-only file \"%s\" for writing.\n",p_path)
        return -EACCES;
      }
      return 0;
    } else {
      // Attempt to open a non existant file
      LOG_DEBUG("Attempt to open non existant file \"%s\".\n",p_path)
      return -ENOENT;
    }
  } else {
    // Attempt to open a non existant file
    LOG_DEBUG("Attempt to open non existant file \"%s\".\n",p_path)
    return -ENOENT;
  }
}

/*
 * ReadVirtFile:
 *   FUSE read implementation
 *
 * Params:
 *   p_path: Path (relative to mount folder) of file to read data from
 *   p_buf: Pre-allocated buffer where read data should be written to
 *   size: Number of bytes to read
 *   offset: Offset to start reading at
 *   fi: ?? but not used
 *
 * Returns:
 *   Read bytes on success, negated error code on error
 */
static int ReadVirtFile(const char *p_path,
                        char *p_buf,
                        size_t size,
                        off_t offset,
                        struct fuse_file_info *fi)
{
  uint64_t len;

  if(strcmp(p_path,glob_xmount_cfg.pVirtualImagePath)==0) {
    // Wait for other threads to end reading/writing data
    pthread_mutex_lock(&glob_mutex_image_rw);

    // Get virtual image file size
    if(!GetVirtImageSize(&len)) {
      LOG_ERROR("Couldn't get virtual image size!\n")
      pthread_mutex_unlock(&glob_mutex_image_rw);
      return 0;
    }
    if(offset<len) {
      if(offset+size>len) size=len-offset;
      if(GetVirtImageData(p_buf,offset,size)!=size) {
        LOG_ERROR("Couldn't read data from virtual image file!\n")
        pthread_mutex_unlock(&glob_mutex_image_rw);
        return 0;
      }
    } else {
      LOG_DEBUG("Attempt to read past EOF of virtual image file\n");
      pthread_mutex_unlock(&glob_mutex_image_rw);
      return 0;
    }

    // Allow other threads to read/write data again
    pthread_mutex_unlock(&glob_mutex_image_rw);

  } else if(strcmp(p_path,glob_xmount_cfg.pVirtualImageInfoPath)==0) {
    // Read data from virtual image info file
    len=strlen(glob_p_info_file);
    if(offset<len) {
      if(offset+size>len) {
        size=len-offset;
        LOG_DEBUG("Attempt to read past EOF of virtual image info file\n")
      }
      pthread_mutex_lock(&glob_mutex_info_read);
      memcpy(p_buf,glob_p_info_file+offset,size);
      pthread_mutex_unlock(&glob_mutex_info_read);
      LOG_DEBUG("Read %" PRIu64 " bytes at offset %" PRIu64
                " from virtual image info file\n",size,offset)
    } else {
      LOG_DEBUG("Attempt to read past EOF of virtual info file\n");
      return 0;
    }
  } else if(strcmp(p_path,glob_xmount_cfg.pVirtualVmdkPath)==0) {
    // Read data from virtual vmdk file
    len=glob_vmdk_file_size;
    if(offset<len) {
      if(offset+size>len) {
        LOG_DEBUG("Attempt to read past EOF of virtual vmdk file\n")
        LOG_DEBUG("Adjusting read size from %u to %u\n",size,len-offset)
        size=len-offset;
      }
      pthread_mutex_lock(&glob_mutex_image_rw);
      memcpy(p_buf,glob_p_vmdk_file+offset,size);
      pthread_mutex_unlock(&glob_mutex_image_rw);
      LOG_DEBUG("Read %" PRIu64 " bytes at offset %" PRIu64
                " from virtual vmdk file\n",size,offset)
    } else {
      LOG_DEBUG("Attempt to read behind EOF of virtual vmdk file\n");
      return 0;
    }
  } else if(glob_p_vmdk_lockfile_name!=NULL &&
            strcmp(p_path,glob_p_vmdk_lockfile_name)==0)
  {
    // Read data from virtual lock file
    len=glob_vmdk_lockfile_size;
    if(offset<len) {
      if(offset+size>len) {
        LOG_DEBUG("Attempt to read past EOF of virtual vmdk lock file\n")
        LOG_DEBUG("Adjusting read size from %u to %u\n",size,len-offset)
        size=len-offset;
      }
      pthread_mutex_lock(&glob_mutex_image_rw);
      memcpy(p_buf,glob_p_vmdk_lockfile_data+offset,size);
      pthread_mutex_unlock(&glob_mutex_image_rw);
      LOG_DEBUG("Read %" PRIu64 " bytes at offset %" PRIu64
                " from virtual vmdk lock file\n",size,offset)
    } else {
      LOG_DEBUG("Attempt to read past EOF of virtual vmdk lock file\n");
      return 0;
    }
  } else {
    // Attempt to read non existant file
    LOG_DEBUG("Attempt to read from non existant file \"%s\"\n",p_path)
    return -ENOENT;
  }

  return size;
}

/*
 * RenameVirtFile:
 *   FUSE rename implementation
 *
 * Params:
 *   p_path: File to rename
 *   p_npath: New filename
 *
 * Returns:
 *   "0" on error, negated error code on error
 */
static int RenameVirtFile(const char *p_path, const char *p_npath) {
  if(glob_xmount_cfg.VirtImageType==VirtImageType_VMDK ||
     glob_xmount_cfg.VirtImageType==VirtImageType_VMDKS)
  {
    if(glob_p_vmdk_lockfile_name!=NULL &&
       strcmp(p_path,glob_p_vmdk_lockfile_name)==0)
    {
      LOG_DEBUG("Renaming virtual lock file from \"%s\" to \"%s\"\n",
                glob_p_vmdk_lockfile_name,
                p_npath)
      XMOUNT_REALLOC(glob_p_vmdk_lockfile_name,char*,
                     (strlen(p_npath)+1)*sizeof(char));
      strcpy(glob_p_vmdk_lockfile_name,p_npath);
      return 0;
    }
  }
  return -ENOENT;
}

/*
 * DeleteVirtDir:
 *   FUSE rmdir implementation
 *
 * Params:
 *   p_path: Directory to delete
 *
 * Returns:
 *   "0" on success, negated error code on error
 */
static int DeleteVirtDir(const char *p_path) {
  // Only VMWare's lock directories can be deleted
  if(glob_xmount_cfg.VirtImageType==VirtImageType_VMDK ||
     glob_xmount_cfg.VirtImageType==VirtImageType_VMDKS)
  {
    if(glob_p_vmdk_lockdir1!=NULL && strcmp(p_path,glob_p_vmdk_lockdir1)==0) {
      LOG_DEBUG("Deleting virtual lock dir \"%s\"\n",glob_p_vmdk_lockdir1)
      free(glob_p_vmdk_lockdir1);
      glob_p_vmdk_lockdir1=NULL;
      return 0;
    } else if(glob_p_vmdk_lockdir2!=NULL &&
              strcmp(p_path,glob_p_vmdk_lockdir2)==0)
    {
      LOG_DEBUG("Deleting virtual lock dir \"%s\"\n",glob_p_vmdk_lockdir1)
      free(glob_p_vmdk_lockdir2);
      glob_p_vmdk_lockdir2=NULL;
      return 0;
    }
  }
  return -1;
}

/*
 * DeleteVirtFile:
 *   FUSE unlink implementation
 *
 * Params:
 *   p_path: File to delete
 *
 * Returns:
 *   "0" on success, negated error code on error
 */
static int DeleteVirtFile(const char *p_path) {
  // Only VMWare's lock file can be deleted
  if(glob_xmount_cfg.VirtImageType==VirtImageType_VMDK ||
     glob_xmount_cfg.VirtImageType==VirtImageType_VMDKS)
  {
    if(glob_p_vmdk_lockfile_name!=NULL &&
       strcmp(p_path,glob_p_vmdk_lockfile_name)==0)
    {
      LOG_DEBUG("Deleting virtual file \"%s\"\n",glob_p_vmdk_lockfile_name)
      free(glob_p_vmdk_lockfile_name);
      free(glob_p_vmdk_lockfile_data);
      glob_p_vmdk_lockfile_name=NULL;
      glob_p_vmdk_lockfile_data=NULL;
      glob_vmdk_lockfile_size=0;
      return 0;
    }
  }
  return -1;
}

/*
 * GetVirtFsStats:
 *   FUSE statfs implementation
 *
 * Params:
 *   p_path: Get stats for fs that the specified file resides in
 *   stats: Stats
 *
 * Returns:
 *   "0" on success, negated error code on error
 */
/*
static int GetVirtFsStats(const char *p_path, struct statvfs *stats) {
  struct statvfs CacheFileFsStats;
  int ret;

  if(glob_xmount_cfg.Writable==TRUE) {
    // If write support is enabled, return stats of fs upon which cache file
    // resides in
    if((ret=statvfs(glob_xmount_cfg.pCacheFile,&CacheFileFsStats))==0) {
      memcpy(stats,&CacheFileFsStats,sizeof(struct statvfs));
      return 0;
    } else {
      LOG_ERROR("Couldn't get stats for fs upon which resides \"%s\"\n",
                glob_xmount_cfg.pCacheFile)
      return ret;
    }
  } else {
    // TODO: Return read only
    return 0;
  }
}
*/

/*
 * WriteVirtFile:
 *   FUSE write implementation
 *
 * Params:
 *   p_buf: Buffer containing data to write
 *   size: Number of bytes to write
 *   offset: Offset to start writing at
 *   fi: ?? but not used
 *
 * Returns:
 *   Written bytes on success, negated error code on error
 */
static int WriteVirtFile(const char *p_path,
                         const char *p_buf,
                         size_t size,
                         off_t offset,
                         struct fuse_file_info *fi)
{
  uint64_t len;

  if(strcmp(p_path,glob_xmount_cfg.pVirtualImagePath)==0) {
    // Wait for other threads to end reading/writing data
    pthread_mutex_lock(&glob_mutex_image_rw);

    // Get virtual image file size
    if(!GetVirtImageSize(&len)) {
      LOG_ERROR("Couldn't get virtual image size!\n")
      pthread_mutex_unlock(&glob_mutex_image_rw);
      return 0;
    }
    if(offset<len) {
      if(offset+size>len) size=len-offset;
      if(SetVirtImageData(p_buf,offset,size)!=size) {
        LOG_ERROR("Couldn't write data to virtual image file!\n")
        pthread_mutex_unlock(&glob_mutex_image_rw);
        return 0;
      }
    } else {
      LOG_DEBUG("Attempt to write past EOF of virtual image file\n")
      pthread_mutex_unlock(&glob_mutex_image_rw);
      return 0;
    }

    // Allow other threads to read/write data again
    pthread_mutex_unlock(&glob_mutex_image_rw);
  } else if(strcmp(p_path,glob_xmount_cfg.pVirtualVmdkPath)==0) {
    pthread_mutex_lock(&glob_mutex_image_rw);
    len=glob_vmdk_file_size;
    if((offset+size)>len) {
      // Enlarge or create buffer if needed
      if(len==0) {
        len=offset+size;
        XMOUNT_MALLOC(glob_p_vmdk_file,char*,len*sizeof(char))
      } else {
        len=offset+size;
        XMOUNT_REALLOC(glob_p_vmdk_file,char*,len*sizeof(char))
      }
      glob_vmdk_file_size=offset+size;
    }
    // Copy data to buffer
    memcpy(glob_p_vmdk_file+offset,p_buf,size);
    pthread_mutex_unlock(&glob_mutex_image_rw);
  } else if(glob_p_vmdk_lockfile_name!=NULL &&
            strcmp(p_path,glob_p_vmdk_lockfile_name)==0)
  {
    pthread_mutex_lock(&glob_mutex_image_rw);
    if((offset+size)>glob_vmdk_lockfile_size) {
      // Enlarge or create buffer if needed
      if(glob_vmdk_lockfile_size==0) {
        glob_vmdk_lockfile_size=offset+size;
        XMOUNT_MALLOC(glob_p_vmdk_lockfile_data,char*,
                      glob_vmdk_lockfile_size*sizeof(char))
      } else {
        glob_vmdk_lockfile_size=offset+size;
        XMOUNT_REALLOC(glob_p_vmdk_lockfile_data,char*,
                       glob_vmdk_lockfile_size*sizeof(char))
      }
    }
    // Copy data to buffer
    memcpy(glob_p_vmdk_lockfile_data+offset,p_buf,size);
    pthread_mutex_unlock(&glob_mutex_image_rw);
  } else if(strcmp(p_path,glob_xmount_cfg.pVirtualImageInfoPath)==0) {
    // Attempt to write data to read only image info file
    LOG_DEBUG("Attempt to write data to virtual info file\n");
    return -ENOENT;
  } else {
    // Attempt to write to non existant file
    LOG_DEBUG("Attempt to write to the non existant file \"%s\"\n",p_path)
    return -ENOENT;
  }

  return size;
}

/*
 * CalculateInputImageHash:
 *   Calculates an MD5 hash of the first HASH_AMOUNT bytes of the input image.
 *
 * Params:
 *   pHashLow : Pointer to the lower 64 bit of the hash
 *   pHashHigh : Pointer to the higher 64 bit of the hash
 *
 * Returns:
 *   TRUE on success, FALSE on error
 */
static int CalculateInputImageHash(uint64_t *pHashLow, uint64_t *pHashHigh) {
  char hash[16];
  md5_state_t md5_state;
  char *p_buf;
  XMOUNT_MALLOC(p_buf,char*,HASH_AMOUNT*sizeof(char))
  size_t read_data=GetOrigImageData(p_buf,0,HASH_AMOUNT);
  if(read_data>0) {
    // Calculate MD5 hash
    md5_init(&md5_state);
    md5_append(&md5_state,(const md5_byte_t*)p_buf,HASH_AMOUNT);
    md5_finish(&md5_state,(md5_byte_t*)hash);
    // Convert MD5 hash into two 64bit integers
    *pHashLow=*((uint64_t*)hash);
    *pHashHigh=*((uint64_t*)(hash+8));
    free(p_buf);
    return TRUE;
  } else {
    LOG_ERROR("Couldn't read data from original image file!\n")
    free(p_buf);
    return FALSE;
  }
}

/*
 * InitVirtVdiHeader:
 *   Build and init virtual VDI file header
 *
 * Params:
 *   n/a
 *
 * Returns:
 *   "TRUE" on success, "FALSE" on error
 */
static int InitVirtVdiHeader() {
  // See http://forums.virtualbox.org/viewtopic.php?t=8046 for a
  // "description" of the various header fields

  uint64_t ImageSize;
  off_t offset;
  uint32_t i,BlockEntries;

  // Get input image size
  if(!GetOrigImageSize(&ImageSize,FALSE)) {
    LOG_ERROR("Couldn't get input image size!\n")
    return FALSE;
  }

  // Calculate how many VDI blocks we need
  BlockEntries=ImageSize/VDI_IMAGE_BLOCK_SIZE;
  if((ImageSize%VDI_IMAGE_BLOCK_SIZE)!=0) BlockEntries++;
  glob_p_vdi_block_map_size=BlockEntries*sizeof(uint32_t);
  LOG_DEBUG("BlockMap: %d (%08X) entries, %d (%08X) bytes!\n",
            BlockEntries,
            BlockEntries,
            glob_p_vdi_block_map_size,
            glob_p_vdi_block_map_size)

  // Allocate memory for vdi header and block map
  glob_vdi_header_size=sizeof(ts_VdiFileHeader)+glob_p_vdi_block_map_size;
  XMOUNT_MALLOC(glob_p_vdi_header,pts_VdiFileHeader,glob_vdi_header_size)
  memset(glob_p_vdi_header,0,glob_vdi_header_size);
  glob_p_vdi_block_map=((void*)glob_p_vdi_header)+sizeof(ts_VdiFileHeader);

  // Init header values
  strncpy(glob_p_vdi_header->szFileInfo,VDI_FILE_COMMENT,
          strlen(VDI_FILE_COMMENT)+1);
  glob_p_vdi_header->u32Signature=VDI_IMAGE_SIGNATURE;
  glob_p_vdi_header->u32Version=VDI_IMAGE_VERSION;
  glob_p_vdi_header->cbHeader=0x00000180;  // No idea what this is for! Testimage had same value
  glob_p_vdi_header->u32Type=VDI_IMAGE_TYPE_FIXED;
  glob_p_vdi_header->fFlags=VDI_IMAGE_FLAGS;
  strncpy(glob_p_vdi_header->szComment,VDI_HEADER_COMMENT,
          strlen(VDI_HEADER_COMMENT)+1);
  glob_p_vdi_header->offData=glob_vdi_header_size;
  glob_p_vdi_header->offBlocks=sizeof(ts_VdiFileHeader);
  glob_p_vdi_header->cCylinders=0; // Legacy info
  glob_p_vdi_header->cHeads=0; // Legacy info
  glob_p_vdi_header->cSectors=0; // Legacy info
  glob_p_vdi_header->cbSector=512; // Legacy info
  glob_p_vdi_header->u32Dummy=0;
  glob_p_vdi_header->cbDisk=ImageSize;
  // Seems as VBox is always using a 1MB blocksize
  glob_p_vdi_header->cbBlock=VDI_IMAGE_BLOCK_SIZE;
  glob_p_vdi_header->cbBlockExtra=0;
  glob_p_vdi_header->cBlocks=BlockEntries;
  glob_p_vdi_header->cBlocksAllocated=BlockEntries;
  // Use partial MD5 input file hash as creation UUID and generate a random
  // modification UUID. VBox won't accept immages where create and modify UUIDS
  // aren't set.
  glob_p_vdi_header->uuidCreate_l=glob_xmount_cfg.InputHashLo;
  glob_p_vdi_header->uuidCreate_h=glob_xmount_cfg.InputHashHi;
  //*((uint32_t*)(&(glob_p_vdi_header->uuidCreate_l)))=rand();
  //*((uint32_t*)(&(glob_p_vdi_header->uuidCreate_l))+4)=rand();
  //*((uint32_t*)(&(glob_p_vdi_header->uuidCreate_h)))=rand();
  //*((uint32_t*)(&(glob_p_vdi_header->uuidCreate_h))+4)=rand();

#define rand64(var) {              \
  *((uint32_t*)&(var))=rand();     \
  *(((uint32_t*)&(var))+1)=rand(); \
}

  rand64(glob_p_vdi_header->uuidModify_l);
  rand64(glob_p_vdi_header->uuidModify_h);

#undef rand64

  // Generate block map
  i=0;
  for(offset=0;offset<glob_p_vdi_block_map_size;offset+=4) {
    *((uint32_t*)(glob_p_vdi_block_map+offset))=i;
    i++;
  }

  LOG_DEBUG("VDI header size = %u\n",glob_vdi_header_size)

  return TRUE;
}

/*
 * InitVirtVhdHeader:
 *   Build and init virtual VHD file header
 *
 * Params:
 *   n/a
 *
 * Returns:
 *   "TRUE" on success, "FALSE" on error
 */
static int InitVirtVhdHeader() {
  uint64_t orig_image_size=0;
  uint16_t i=0;
  uint64_t geom_tot_s=0;
  uint64_t geom_c_x_h=0;
  uint16_t geom_c=0;
  uint8_t geom_h=0;
  uint8_t geom_s=0;
  uint32_t checksum=0;

  // Get input image size
  if(!GetOrigImageSize(&orig_image_size,FALSE)) {
    LOG_ERROR("Couldn't get input image size!\n")
    return FALSE;
  }

  // Allocate memory for vhd header
  XMOUNT_MALLOC(glob_p_vhd_header,pts_VhdFileHeader,sizeof(ts_VhdFileHeader))
  memset(glob_p_vhd_header,0,sizeof(ts_VhdFileHeader));

  // Init header values
  glob_p_vhd_header->cookie=VHD_IMAGE_HVAL_COOKIE;
  glob_p_vhd_header->features=VHD_IMAGE_HVAL_FEATURES;
  glob_p_vhd_header->file_format_version=VHD_IMAGE_HVAL_FILE_FORMAT_VERSION;
  glob_p_vhd_header->data_offset=VHD_IMAGE_HVAL_DATA_OFFSET;
  glob_p_vhd_header->creation_time=htobe32(time(NULL)-
                                          VHD_IMAGE_TIME_CONVERSION_OFFSET);
  glob_p_vhd_header->creator_app=VHD_IMAGE_HVAL_CREATOR_APPLICATION;
  glob_p_vhd_header->creator_ver=VHD_IMAGE_HVAL_CREATOR_VERSION;
  glob_p_vhd_header->creator_os=VHD_IMAGE_HVAL_CREATOR_HOST_OS;
  glob_p_vhd_header->size_original=htobe64(orig_image_size);
  glob_p_vhd_header->size_current=glob_p_vhd_header->size_original;

  // Convert size to sectors
  if(orig_image_size>136899993600) {
    // image is larger then CHS values can address.
    // Set sectors to max (C65535*H16*S255).
    geom_tot_s=267382800;
  } else {
    // Calculate actual sectors
    geom_tot_s=orig_image_size/512;
    if((orig_image_size%512)!=0) geom_tot_s++;
  }

  // Calculate CHS values. This is done according to the VHD specs
  if(geom_tot_s>=66059280) { // C65535 * H16 * S63
    geom_s=255;
    geom_h=16;
    geom_c_x_h=geom_tot_s/geom_s;
  } else {
    geom_s=17;
    geom_c_x_h=geom_tot_s/geom_s;
    geom_h=(geom_c_x_h+1023)/1024;
    if(geom_h<4) geom_h=4;
    if(geom_c_x_h>=(geom_h*1024) || geom_h>16) {
      geom_s=31;
      geom_h=16;
      geom_c_x_h=geom_tot_s/geom_s;
    }
    if(geom_c_x_h>=(geom_h*1024)) {
      geom_s=63;
      geom_h=16;
      geom_c_x_h=geom_tot_s/geom_s;
    }
  }
  geom_c=geom_c_x_h/geom_h;

  glob_p_vhd_header->disk_geometry_c=htobe16(geom_c);
  glob_p_vhd_header->disk_geometry_h=geom_h;
  glob_p_vhd_header->disk_geometry_s=geom_s;

  glob_p_vhd_header->disk_type=VHD_IMAGE_HVAL_DISK_TYPE;

  glob_p_vhd_header->uuid_l=glob_xmount_cfg.InputHashLo;
  glob_p_vhd_header->uuid_h=glob_xmount_cfg.InputHashHi;
  glob_p_vhd_header->saved_state=0x00;

  // Calculate footer checksum
  for(i=0;i<sizeof(ts_VhdFileHeader);i++) {
    checksum+=*((uint8_t*)(glob_p_vhd_header)+i);
  }
  glob_p_vhd_header->checksum=htobe32(~checksum);

  LOG_DEBUG("VHD header size = %u\n",sizeof(ts_VhdFileHeader));

  return TRUE;
}

/*
 * InitVirtualVmdkFile:
 *   Init the virtual VMDK file
 *
 * Params:
 *   n/a
 *
 * Returns:
 *   "TRUE" on success, "FALSE" on error
 */
static int InitVirtualVmdkFile() {
  uint64_t ImageSize=0;
  uint64_t ImageBlocks=0;
  char buf[500];

  // Get original image size
  if(!GetOrigImageSize(&ImageSize,FALSE)) {
    LOG_ERROR("Couldn't get original image size!\n")
    return FALSE;
  }

  ImageBlocks=ImageSize/512;
  if(ImageSize%512!=0) ImageBlocks++;

#define VMDK_DESC_FILE "# Disk DescriptorFile\n" \
                       "version=1\n" \
                       "CID=fffffffe\n" \
                       "parentCID=ffffffff\n" \
                       "createType=\"monolithicFlat\"\n\n" \
                       "# Extent description\n" \
                       "RW %" PRIu64 " FLAT \"%s\" 0\n\n" \
                       "# The Disk Data Base\n" \
                       "#DDB\n" \
                       "ddb.virtualHWVersion = \"3\"\n" \
                       "ddb.adapterType = \"%s\"\n" \
                       "ddb.geometry.cylinders = \"0\"\n" \
                       "ddb.geometry.heads = \"0\"\n" \
                       "ddb.geometry.sectors = \"0\"\n"

  if(glob_xmount_cfg.VirtImageType==VirtImageType_VMDK) {
    // VMDK with IDE bus
    sprintf(buf,
            VMDK_DESC_FILE,
            ImageBlocks,
            (glob_xmount_cfg.pVirtualImagePath)+1,
            "ide");
  } else if(glob_xmount_cfg.VirtImageType==VirtImageType_VMDKS){
    // VMDK with SCSI bus
    sprintf(buf,
            VMDK_DESC_FILE,
            ImageBlocks,
            (glob_xmount_cfg.pVirtualImagePath)+1,
            "scsi");
  } else {
    LOG_ERROR("Unknown virtual VMDK file format!\n")
    return FALSE;
  }

#undef VMDK_DESC_FILE

  // Do not use XMOUNT_STRSET here to avoid adding '\0' to the buffer!
  // TODO: Not adding \0 but using strlen afterwards??? Needs checking!!
  XMOUNT_MALLOC(glob_p_vmdk_file,char*,strlen(buf))
  strncpy(glob_p_vmdk_file,buf,strlen(buf));
  glob_vmdk_file_size=strlen(buf);

  return TRUE;
}

/*
 * InitVirtImageInfoFile:
 *   Create virtual image info file
 *
 * Params:
 *   n/a
 *
 * Returns:
 *   "TRUE" on success, "FALSE" on error
 */
static int InitVirtImageInfoFile() {
  int ret;
  char *p_buf;

  // Add static header
  XMOUNT_MALLOC(glob_p_info_file,char*,strlen(IMAGE_INFO_HEADER)+1)
  strncpy(glob_p_info_file,IMAGE_INFO_HEADER,strlen(IMAGE_INFO_HEADER)+1);
  
  // Get infos from input lib
  ret=glob_p_input_functions->GetInfofileContent(glob_p_input_image,&p_buf);
  if(ret!=0) {
    LOG_ERROR("Unable to get info file content: %s!\n",
              glob_p_input_functions->GetErrorMessage(ret));
    return FALSE;
  }

  // Add infos to main buffer and free p_buf
  XMOUNT_STRAPP(glob_p_info_file,p_buf);
  glob_p_input_functions->FreeBuffer(p_buf);

  return TRUE;
}

/*
 * InitCacheFile:
 *   Create / load cache file to enable virtual write support
 *
 * Params:
 *   n/a
 *
 * Returns:
 *   "TRUE" on success, "FALSE" on error
 */
static int InitCacheFile() {
  uint64_t ImageSize=0;
  uint64_t BlockIndexSize=0;
  uint64_t CacheFileHeaderSize=0;
  uint64_t CacheFileSize=0;
  uint32_t NeededBlocks=0;
  uint64_t buf;

  if(!glob_xmount_cfg.OverwriteCache) {
    // Try to open an existing cache file or create a new one
    glob_p_cache_file=(FILE*)FOPEN(glob_xmount_cfg.pCacheFile,"rb+");
    if(glob_p_cache_file==NULL) {
      // As the c lib seems to have no possibility to open a file rw wether it
      // exists or not (w+ does not work because it truncates an existing file),
      // when r+ returns NULL the file could simply not exist
      LOG_DEBUG("Cache file does not exist. Creating new one\n")
      glob_p_cache_file=(FILE*)FOPEN(glob_xmount_cfg.pCacheFile,"wb+");
      if(glob_p_cache_file==NULL) {
        // There is really a problem opening the file
        LOG_ERROR("Couldn't open cache file \"%s\"!\n",
                  glob_xmount_cfg.pCacheFile)
        return FALSE;
      }
    }
  } else {
    // Overwrite existing cache file or create a new one
    glob_p_cache_file=(FILE*)FOPEN(glob_xmount_cfg.pCacheFile,"wb+");
    if(glob_p_cache_file==NULL) {
      LOG_ERROR("Couldn't open cache file \"%s\"!\n",
                glob_xmount_cfg.pCacheFile)
      return FALSE;
    }
  }

  // Get input image size
  if(!GetOrigImageSize(&ImageSize,FALSE)) {
    LOG_ERROR("Couldn't get input image size!\n")
    return FALSE;
  }

  // Calculate how many blocks are needed and how big the buffers must be
  // for the actual cache file version
  NeededBlocks=ImageSize/CACHE_BLOCK_SIZE;
  if((ImageSize%CACHE_BLOCK_SIZE)!=0) NeededBlocks++;
  BlockIndexSize=NeededBlocks*sizeof(ts_CacheFileBlockIndex);
  CacheFileHeaderSize=sizeof(ts_CacheFileHeader)+BlockIndexSize;
  LOG_DEBUG("Cache blocks: %u (%04X) entries, %zd (%08zX) bytes\n",
            NeededBlocks,
            NeededBlocks,
            BlockIndexSize,
            BlockIndexSize)

  // Get cache file size
  // fseeko64 had massive problems!
  if(fseeko(glob_p_cache_file,0,SEEK_END)!=0) {
    LOG_ERROR("Couldn't seek to end of cache file!\n")
    return FALSE;
  }
  // Same here, ftello64 didn't work at all and returned 0 all the times
  CacheFileSize=ftello(glob_p_cache_file);
  LOG_DEBUG("Cache file has %zd bytes\n",CacheFileSize)

  if(CacheFileSize>0) {
    // Cache file isn't empty, parse block header
    LOG_DEBUG("Cache file not empty. Parsing block header\n")
    if(fseeko(glob_p_cache_file,0,SEEK_SET)!=0) {
      LOG_ERROR("Couldn't seek to beginning of cache file!\n")
      return FALSE;
    }
    // Read and check file signature
    if(fread(&buf,8,1,glob_p_cache_file)!=1 || buf!=CACHE_FILE_SIGNATURE) {
      free(glob_p_cache_header);
      LOG_ERROR("Not an xmount cache file or cache file corrupt!\n")
      return FALSE;
    }
    // Now get cache file version (Has only 32bit!)
    if(fread(&buf,4,1,glob_p_cache_file)!=1) {
      free(glob_p_cache_header);
      LOG_ERROR("Not an xmount cache file or cache file corrupt!\n")
      return FALSE;
    }
    switch((uint32_t)buf) {
      case 0x00000001:
        // Old v1 cache file.
        LOG_ERROR("Unsupported cache file version!\n")
        LOG_ERROR("Please use xmount-tool to upgrade your cache file.\n")
        return FALSE;
      case CUR_CACHE_FILE_VERSION:
        // Current version
        if(fseeko(glob_p_cache_file,0,SEEK_SET)!=0) {
          LOG_ERROR("Couldn't seek to beginning of cache file!\n")
          return FALSE;
        }
        // Alloc memory for header and block index
        XMOUNT_MALLOC(glob_p_cache_header,
                      pts_CacheFileHeader,
                      CacheFileHeaderSize);
        memset(glob_p_cache_header,0,CacheFileHeaderSize);
        // Read header and block index from file
        if(fread(glob_p_cache_header,
                 CacheFileHeaderSize,
                 1,
                 glob_p_cache_file)!=1)
        {
          // Cache file isn't big enough
          free(glob_p_cache_header);
          LOG_ERROR("Cache file corrupt!\n")
          return FALSE;
        }
        break;
      default:
        LOG_ERROR("Unknown cache file version!\n")
        return FALSE;
    }
    // Check if cache file has same block size as we do
    if(glob_p_cache_header->BlockSize!=CACHE_BLOCK_SIZE) {
      LOG_ERROR("Cache file does not use default cache block size!\n")
      return FALSE;
    }
    // Set pointer to block index
    glob_p_cache_blkidx=(pts_CacheFileBlockIndex)((void*)glob_p_cache_header+
                          glob_p_cache_header->pBlockIndex);
  } else {
    // New cache file, generate a new block header
    LOG_DEBUG("Cache file is empty. Generating new block header\n");
    // Alloc memory for header and block index
    XMOUNT_MALLOC(glob_p_cache_header,pts_CacheFileHeader,CacheFileHeaderSize)
    memset(glob_p_cache_header,0,CacheFileHeaderSize);
    glob_p_cache_header->FileSignature=CACHE_FILE_SIGNATURE;
    glob_p_cache_header->CacheFileVersion=CUR_CACHE_FILE_VERSION;
    glob_p_cache_header->BlockSize=CACHE_BLOCK_SIZE;
    glob_p_cache_header->BlockCount=NeededBlocks;
    //glob_p_cache_header->UsedBlocks=0;
    // The following pointer is only usuable when reading data from cache file
    glob_p_cache_header->pBlockIndex=sizeof(ts_CacheFileHeader);
    glob_p_cache_blkidx=(pts_CacheFileBlockIndex)((void*)glob_p_cache_header+
                         sizeof(ts_CacheFileHeader));
    glob_p_cache_header->VdiFileHeaderCached=FALSE;
    glob_p_cache_header->pVdiFileHeader=0;
    glob_p_cache_header->VmdkFileCached=FALSE;
    glob_p_cache_header->VmdkFileSize=0;
    glob_p_cache_header->pVmdkFile=0;
    glob_p_cache_header->VhdFileHeaderCached=FALSE;
    glob_p_cache_header->pVhdFileHeader=0;
    // Write header to file
    if(fwrite(glob_p_cache_header,CacheFileHeaderSize,1,glob_p_cache_file)!=1) {
      free(glob_p_cache_header);
      LOG_ERROR("Couldn't write cache file header to file!\n");
      return FALSE;
    }
  }
  return TRUE;
}

/*
 * LoadInputLibs
 */
static int LoadInputLibs() {
  DIR *p_dir=NULL;
  struct dirent *p_dirent=NULL;
  int base_library_path_len=0;
  char *p_library_path=NULL;
  void *p_libxmount_in=NULL;
  t_LibXmount_Input_GetApiVersion pfun_GetApiVersion;
  t_LibXmount_Input_GetSupportedFormats pfun_GetSupportedFormats;
  t_LibXmount_Input_GetFunctions pfun_GetFunctions;
  const char *p_supported_formats=NULL;
  const char *p_buf;
  uint32_t supported_formats_len=0;
  pts_InputLib p_input_lib=NULL;

  LOG_DEBUG("Searching for input libraries in '%s'.\n",
            XMOUNT_LIBRARY_PATH);

  // Open lib dir
  p_dir=opendir(XMOUNT_LIBRARY_PATH);
  if(p_dir==NULL) {
    LOG_ERROR("Unable to access xmount library directory '%s'!\n",
              XMOUNT_LIBRARY_PATH);
    return FALSE;
  }

  // Construct base library path
  base_library_path_len=strlen(XMOUNT_LIBRARY_PATH);
  XMOUNT_STRSET(p_library_path,XMOUNT_LIBRARY_PATH);
  if(XMOUNT_LIBRARY_PATH[base_library_path_len]!='/') {
    base_library_path_len++;
    XMOUNT_STRAPP(p_library_path,"/");
  }

  // Loop over lib dir
  while((p_dirent=readdir(p_dir))!=NULL) {
    if(strncmp(p_dirent->d_name,"libxmount_input_",16)!=0) {
      LOG_DEBUG("Ignoring '%s'.\n",p_dirent->d_name);
      continue;
    }

    LOG_DEBUG("Trying to load '%s'\n",p_dirent->d_name);

    // Found an input lib, construct full path to it and load it
    p_library_path=realloc(p_library_path,
                           base_library_path_len+strlen(p_dirent->d_name)+1);
    if(p_library_path==NULL) {
      LOG_ERROR("Couldn't allocate memmory!\n");
      exit(1);
    }
    strcpy(p_library_path+base_library_path_len,p_dirent->d_name);
    p_libxmount_in=dlopen(p_library_path,RTLD_NOW);
    if(p_libxmount_in==NULL) {
      LOG_ERROR("Unable to load input library '%s'!\n",p_library_path);
      LOG_DEBUG("DLOPEN returned '%s'.\n",dlerror());
      continue;
    }

    // Load library symbols
#define LIBXMOUNT_LOAD_SYMBOL(name,pfun) { \
  if((pfun=dlsym(p_libxmount_in,name))==NULL) { \
    LOG_ERROR("Unable to load symbol '%s' from library '%s'!\n", \
              name, \
              p_library_path); \
    dlclose(p_libxmount_in); \
    p_libxmount_in=NULL; \
    continue; \
  } \
}

    LIBXMOUNT_LOAD_SYMBOL("LibXmount_Input_GetApiVersion",pfun_GetApiVersion);

    // Check library's API version
    if(pfun_GetApiVersion()!=LIBXMOUNT_INPUT_API_VERSION) {
      LOG_DEBUG("Failed! Wrong API version.\n");
      LOG_ERROR("Unable to load input library '%s'. Wrong API version\n",
                p_library_path);
      dlclose(p_libxmount_in);
      continue;
    }

    LIBXMOUNT_LOAD_SYMBOL("LibXmount_Input_GetSupportedFormats",
                          pfun_GetSupportedFormats);
    LIBXMOUNT_LOAD_SYMBOL("LibXmount_Input_GetFunctions",pfun_GetFunctions);

#undef LIBXMOUNT_LOAD_SYMBOL

    // Construct new entry for our library list
    XMOUNT_MALLOC(p_input_lib,pts_InputLib,sizeof(ts_InputLib));
    XMOUNT_STRSET(p_input_lib->p_name,p_dirent->d_name);
    p_input_lib->p_lib=p_libxmount_in;
    p_supported_formats=pfun_GetSupportedFormats();
    supported_formats_len=0;
    p_buf=p_supported_formats;
    while(*p_buf!='\0') {
      supported_formats_len+=(strlen(p_buf)+1);
      p_buf+=(strlen(p_buf)+1);
    }
    supported_formats_len++;
    XMOUNT_MALLOC(p_input_lib->p_supported_input_types,
                  char*,
                  supported_formats_len);
    memcpy(p_input_lib->p_supported_input_types,
           p_supported_formats,
           supported_formats_len);
    // TODO: Maybe check if all functions are available
    pfun_GetFunctions(&(p_input_lib->lib_functions));

    // Add entry to our input library list
    XMOUNT_REALLOC(glob_pp_input_libs,
                   pts_InputLib*,
                   sizeof(pts_InputLib)*(glob_input_libs_count+1));
    glob_pp_input_libs[glob_input_libs_count++]=p_input_lib;

    LOG_DEBUG("%s loaded successfully\n",p_dirent->d_name);
  }

  LOG_DEBUG("A total of %u input libs were loaded.\n",glob_input_libs_count);

  free(p_library_path);
  closedir(p_dir);
  return (glob_input_libs_count>0 ? TRUE : FALSE);
}

/*
 * UnloadInputLibs
 */
static void UnloadInputLibs() {
  LOG_DEBUG("Unloading all input libs.\n");
  for(uint32_t i=0;i<glob_input_libs_count;i++) {
    free(glob_pp_input_libs[i]->p_name);
    dlclose(glob_pp_input_libs[i]->p_lib);
    free(glob_pp_input_libs[i]->p_supported_input_types);
    free(glob_pp_input_libs[i]);
  }
  free(glob_pp_input_libs);
  glob_pp_input_libs=NULL;
  glob_input_libs_count=0;
}

/*
 * FindInputLib
 */
static int FindInputLib() {
  char *p_buf;

  LOG_DEBUG("Trying to find suitable library for input type '%s'.\n",
            glob_xmount_cfg.p_orig_image_type);

  // Loop over all loaded libs
  for(uint32_t i=0;i<glob_input_libs_count;i++) {
    LOG_DEBUG("Checking input library %s\n",glob_pp_input_libs[i]->p_name);
    p_buf=glob_pp_input_libs[i]->p_supported_input_types;
    while(*p_buf!='\0') {
      if(strcmp(p_buf,glob_xmount_cfg.p_orig_image_type)==0) {
        // Library supports input type, set lib functions
        LOG_DEBUG("Input library '%s' pretends to handle that input type.\n",
                  glob_pp_input_libs[i]->p_name);
        glob_p_input_functions=&(glob_pp_input_libs[i]->lib_functions);
        return TRUE;
      }
      p_buf+=(strlen(p_buf)+1);
    }
  }

  LOG_DEBUG("Couldn't find any suitable library.\n");

  // No library supporting input type found
  return FALSE;
}

/*
 * Struct containing implemented FUSE functions
 */
static struct fuse_operations xmount_operations = {
//  .access=GetVirtFileAccess,
  .getattr=GetVirtFileAttr,
  .mkdir=CreateVirtDir,
  .mknod=CreateVirtFile,
  .open=OpenVirtFile,
  .readdir=GetVirtFiles,
  .read=ReadVirtFile,
  .rename=RenameVirtFile,
  .rmdir=DeleteVirtDir,
//  .statfs=GetVirtFsStats,
  .unlink=DeleteVirtFile,
  .write=WriteVirtFile
//  .release=mountewf_release,
};

/*
 * Main
 */
int main(int argc, char *argv[]) {
  char **pp_input_filenames=NULL;
  int input_filenames_count=0;
  int nargc=0;
  char **pp_nargv=NULL;
  char *p_mountpoint=NULL;
  int ret;
  int fuse_ret;
  char *p_err_msg;

  // Disable std output / std input buffering
  setbuf(stdout,NULL);
  setbuf(stderr,NULL);

  // Init glob_xmount_cfg
  glob_xmount_cfg.p_orig_image_type=NULL;
#ifndef __APPLE__
  glob_xmount_cfg.VirtImageType=VirtImageType_DD;
#else
  glob_xmount_cfg.VirtImageType=VirtImageType_DMG;
#endif
  glob_xmount_cfg.Debug=FALSE;
  glob_xmount_cfg.pVirtualImagePath=NULL;
  glob_xmount_cfg.pVirtualVmdkPath=NULL;
  glob_xmount_cfg.pVirtualImageInfoPath=NULL;
  glob_xmount_cfg.Writable=FALSE;
  glob_xmount_cfg.OverwriteCache=FALSE;
  glob_xmount_cfg.pCacheFile=NULL;
  glob_xmount_cfg.OrigImageSize=0;
  glob_xmount_cfg.VirtImageSize=0;
  glob_xmount_cfg.InputHashLo=0;
  glob_xmount_cfg.InputHashHi=0;
  glob_xmount_cfg.orig_img_offset=0;
  glob_xmount_cfg.p_lib_params=NULL;

  // Load input libs
  if(!LoadInputLibs()) {
    LOG_ERROR("Unable to load any input libraries!\n")
    return 1;
  }

  // Parse command line options
  if(!ParseCmdLine(argc,
                   argv,
                   &nargc,
                   &pp_nargv,
                   &input_filenames_count,
                   &pp_input_filenames,
                   &p_mountpoint))
  {
    LOG_ERROR("Error parsing command line options!\n")
    //PrintUsage(argv[0]);
    UnloadInputLibs();
    return 1;
  }

  // Check command line options
  if(nargc<2 /*|| input_filenames_count==0 || p_mountpoint==NULL*/) {
    LOG_ERROR("Couldn't parse command line options!\n")
    PrintUsage(argv[0]);
    UnloadInputLibs();
    return 1;
  }

  // TODO: Check if mountpoint is a valid dir

  // If no input type was specified, default to "dd"
  if(glob_xmount_cfg.p_orig_image_type==NULL) {
    XMOUNT_STRSET(glob_xmount_cfg.p_orig_image_type,"dd");
  }

  // Find an input lib for the specified input type
  if(!FindInputLib()) {
    LOG_ERROR("Unknown input image type \"%s\"!\n",
              glob_xmount_cfg.p_orig_image_type)
    PrintUsage(argv[0]);
    UnloadInputLibs();
    return 1;
  }

  // Init input image handle
  ret=glob_p_input_functions->CreateHandle(&glob_p_input_image);
  if(ret!=0) {
    LOG_ERROR("Unable to init input handle: %s!\n",
              glob_p_input_functions->GetErrorMessage(ret));
    return 1;
  }

  // Parse input lib specific options
  if(glob_xmount_cfg.p_lib_params!=NULL) {
    ret=glob_p_input_functions->OptionsParse(glob_p_input_image,
                                             glob_xmount_cfg.p_lib_params,
                                             &p_err_msg);
    if(ret!=0) {
      if(p_err_msg!=NULL) {
        LOG_ERROR("Unable to parse input library specific options: %s: %s!\n",
                  glob_p_input_functions->GetErrorMessage(ret),
                  p_err_msg);
        glob_p_input_functions->FreeBuffer(p_err_msg);
      } else {
        LOG_ERROR("Unable to parse input library specific options: %s!\n",
                  glob_p_input_functions->GetErrorMessage(ret));
      }
    }
  }

  if(glob_xmount_cfg.Debug==TRUE) {
    LOG_DEBUG("Options passed to FUSE: ")
    for(int i=0;i<nargc;i++) { printf("%s ",pp_nargv[i]); }
    printf("\n");
  }

  // Init mutexes
  pthread_mutex_init(&glob_mutex_image_rw,NULL);
  pthread_mutex_init(&glob_mutex_info_read,NULL);

  if(input_filenames_count==1) {
    LOG_DEBUG("Loading image file \"%s\"...\n",
              pp_input_filenames[0])
  } else {
    LOG_DEBUG("Loading image files \"%s .. %s\"...\n",
              pp_input_filenames[0],
              pp_input_filenames[input_filenames_count-1])
  }

  // Init random generator
  srand(time(NULL));

  // Open input image
  ret=glob_p_input_functions->Open(&glob_p_input_image,
                                   (const char**)pp_input_filenames,
                                   input_filenames_count);
  if(ret!=0) {
    LOG_ERROR("Unable to open input image file: %s!\n",
              glob_p_input_functions->GetErrorMessage(ret));
    UnloadInputLibs();
    return 1;
  }
  LOG_DEBUG("Input image file opened successfully\n")

  // If an offset was specified, make sure it is within limits
  if(glob_xmount_cfg.orig_img_offset!=0) {
    uint64_t size;
    if(!GetOrigImageSize(&size,TRUE)) {
      LOG_ERROR("Couldn't get original image's size!\n");
      return 1;
    }
    if(glob_xmount_cfg.orig_img_offset>size) {
      LOG_ERROR("The specified offset is larger then the size of the input "
                  "image! (%" PRIu64 " > %" PRIu64 ")\n",
                glob_xmount_cfg.orig_img_offset,
                size);
      return 1;
    }
  }

  // Calculate partial MD5 hash of input image file
  if(CalculateInputImageHash(&(glob_xmount_cfg.InputHashLo),
                             &(glob_xmount_cfg.InputHashHi))==FALSE)
  {
    LOG_ERROR("Couldn't calculate partial hash of input image file!\n")
    return 1;
  }

  if(glob_xmount_cfg.Debug==TRUE) {
    LOG_DEBUG("Partial MD5 hash of input image file: ")
    for(int i=0;i<8;i++) printf("%02hhx",
                            *(((char*)(&(glob_xmount_cfg.InputHashLo)))+i));
    for(int i=0;i<8;i++) printf("%02hhx",
                            *(((char*)(&(glob_xmount_cfg.InputHashHi)))+i));
    printf("\n");
  }

  if(!ExtractVirtFileNames(pp_input_filenames[0])) {
    LOG_ERROR("Couldn't extract virtual file names!\n");
    UnloadInputLibs();
    return 1;
  }
  LOG_DEBUG("Virtual file names extracted successfully\n")

  // Gather infos for info file
  if(!InitVirtImageInfoFile()) {
    LOG_ERROR("Couldn't gather infos for virtual image info file!\n")
    UnloadInputLibs();
    return 1;
  }
  LOG_DEBUG("Virtual image info file build successfully\n")

  // Do some virtual image type specific initialisations
  switch(glob_xmount_cfg.VirtImageType) {
    case VirtImageType_DD:
    case VirtImageType_DMG:
      break;
    case VirtImageType_VDI:
      // When mounting as VDI, we need to construct a vdi header
      if(!InitVirtVdiHeader()) {
        LOG_ERROR("Couldn't initialize virtual VDI file header!\n")
        UnloadInputLibs();
        return 1;
      }
      LOG_DEBUG("Virtual VDI file header build successfully\n")
      break;
    case VirtImageType_VHD:
      // When mounting as VHD, we need to construct a vhd footer
      if(!InitVirtVhdHeader()) {
        LOG_ERROR("Couldn't initialize virtual VHD file footer!\n")
        UnloadInputLibs();
        return 1;
      }
      LOG_DEBUG("Virtual VHD file footer build successfully\n")
      break;
    case VirtImageType_VMDK:
    case VirtImageType_VMDKS:
      // When mounting as VMDK, we need to construct the VMDK descriptor file
      if(!InitVirtualVmdkFile()) {
        LOG_ERROR("Couldn't initialize virtual VMDK file!\n")
        UnloadInputLibs();
        return 1;
      }
      break;
  }

  if(glob_xmount_cfg.Writable) {
    // Init cache file and cache file block index
    if(!InitCacheFile()) {
      LOG_ERROR("Couldn't initialize cache file!\n")
      UnloadInputLibs();
      return 1;
    }
    LOG_DEBUG("Cache file initialized successfully\n")
  }

  // Call fuse_main to do the fuse magic
  fuse_ret=fuse_main(nargc,pp_nargv,&xmount_operations,NULL);

  // Destroy mutexes
  pthread_mutex_destroy(&glob_mutex_image_rw);
  pthread_mutex_destroy(&glob_mutex_info_read);

  // Close input image and destroy handle
  ret=glob_p_input_functions->Close(&glob_p_input_image);
  if(ret!=0) {
    LOG_ERROR("Unable to close input image file: %s!",
              glob_p_input_functions->GetErrorMessage(ret));
  }
  ret=glob_p_input_functions->DestroyHandle(&glob_p_input_image);
  if(ret!=0) {
    LOG_ERROR("Unable to destroy input image handle: %s!",
              glob_p_input_functions->GetErrorMessage(ret));
  }

  // Close cache file if write support was enabled
  if(glob_xmount_cfg.Writable) {
    fclose(glob_p_cache_file);
    free(glob_p_cache_header);
  }

  // Free allocated memory
  // Free info file content
  if(glob_p_info_file!=NULL) free(glob_p_info_file);
  // Free output image specific data
  switch(glob_xmount_cfg.VirtImageType) {
    case VirtImageType_DD:
    case VirtImageType_DMG:
      break;
    case VirtImageType_VDI:
      free(glob_p_vdi_header);
      break;
    case VirtImageType_VHD:
      free(glob_p_vhd_header);
      break;
    case VirtImageType_VMDK:
    case VirtImageType_VMDKS: {
      free(glob_p_vmdk_file);
      free(glob_xmount_cfg.pVirtualVmdkPath);
      if(glob_p_vmdk_lockfile_name!=NULL) free(glob_p_vmdk_lockfile_name);
      if(glob_p_vmdk_lockfile_data!=NULL) free(glob_p_vmdk_lockfile_data);
      if(glob_p_vmdk_lockdir1!=NULL) free(glob_p_vmdk_lockdir1);
      if(glob_p_vmdk_lockdir2!=NULL) free(glob_p_vmdk_lockdir2);
      break;
    }
  }
  // Free input filenames
  if(pp_input_filenames!=NULL) {
    for(int i=0;i<input_filenames_count;i++) free(pp_input_filenames[i]);
    free(pp_input_filenames);
  }
  // Free constructed argv
  if(pp_nargv!=NULL) {
    for(int i=0;i<nargc;i++) free(pp_nargv[i]);
    free(pp_nargv);
  }
  // Free mountpoint
  if(p_mountpoint!=NULL) free(p_mountpoint);
  // Free virtual paths
  free(glob_xmount_cfg.pVirtualImagePath);
  free(glob_xmount_cfg.pVirtualImageInfoPath);
  // Free cachefile path
  free(glob_xmount_cfg.pCacheFile);

  // Unload input libs
  UnloadInputLibs();

  return fuse_ret;
}

/*
  ----- Change log -----
  20090131: v0.1.0 released
            * Some minor things have still to be done.
            * Mounting ewf as dd: Seems to work. Diff didn't complain about
              changes between original dd and emulated dd.
            * Mounting ewf as vdi: Seems to work too. VBox accepts the emulated
              vdi as valid vdi file and I was able to mount the containing fs
              under Debian. INFO: Debian freezed when not using mount -r !!
  20090203: v0.1.1 released
            * Multiple code improvements. For ex. cleaner vdi header allocation.
            * Fixed severe bug in image block calculation. Didn't check for odd
              input in conversion from bytes to megabytes.
            * Added more debug output
  20090210: v0.1.2 released
            * Fixed compilation problem (Typo in image_init_info() function).
            * Fixed some problems with the debian scripts to be able to build
              packages.
            * Added random generator initialisation (Makes it possible to use
              more than one image in VBox at a time).
  20090215: * Added function init_cache_blocks which creates / loads a cache
              file used to implement virtual write capability.
  20090217: * Implemented the fuse write function. Did already some basic tests
              with dd and it seems to work. But there are certainly still some
              bugs left as there are also still some TODO's left.
  20090226: * Changed program name from mountewf to xmount.
            * Began with massive code cleanups to ease full implementation of
              virtual write support and to be able to support multiple input
              image formats (DD, EWF and AFF are planned for now).
            * Added defines for supported input formats so it should be possible
              to compile xmount without supporting all input formats. (DD
              input images are always supported as these do not require any
              additional libs). Input formats should later be en/disabled
              by the configure script in function to which libs it detects.
            * GetOrigImageSize function added to get the size of the original
              image whatever type it is in.
            * GetOrigImageData function added to retrieve data from original
              image file whatever type it is in.
            * GetVirtImageSize function added to get the size of the virtual
              image file.
            * Cleaned function mountewf_getattr and renamed it to
              GetVirtFileAttr
            * Cleaned function mountewf_readdir and renamed it to GetVirtFiles
            * Cleaned function mountewf_open and renamed it to OpenVirtFile
  20090227: * Cleaned function init_info_file and renamed it to
              InitVirtImageInfoFile
  20090228: * Cleaned function init_cache_blocks and renamed it to
              InitCacheFile
            * Added LogMessage function to ease error and debug logging (See
              also LOG_ERROR and LOG_DEBUG macros in xmount.h)
            * Cleaned function init_vdi_header and renamed it to
              InitVirtVdiHeader
            * Added PrintUsage function to print out xmount usage informations
            * Cleaned function parse_cmdline and renamed it to ParseCmdLine
            * Cleaned function main
            * Added ExtractVirtFileNames function to extract virtual file names
              from input image name
            * Added function GetVirtImageData to retrieve data from the virtual
              image file. This includes reading data from cache file if virtual
              write support is enabled.
            * Added function ReadVirtFile to replace mountewf_read
  20090229: * Fixed a typo in virtual file name creation
            * Added function SetVirtImageData to write data to virtual image
              file. This includes writing data to cache file and caching entire
              new blocks
            * Added function WriteVirtFile to replace mountewf_write
  20090305: * Solved a problem that made it impossible to access offsets >32bit
  20090308: * Added SetVdiFileHeaderData function to handle virtual image type
              specific data to be cached. This makes cache files independent
              from virtual image type
  20090316: v0.2.0 released
  20090327: v0.2.1 released
            * Fixed a bug in virtual write support. Checking whether data is
              cached didn't use semaphores. This could corrupt cache files
              when running multi-threaded.
            * Added IsVdiFileHeaderCached function to check whether VDI file
              header was already cached
            * Added IsBlockCached function to check whether a block was already
              cached
  20090331: v0.2.2 released (Internal release)
            * Further changes to semaphores to fix write support bug.
  20090410: v0.2.3 released
            * Reverted most of the fixes from v0.2.1 and v0.2.2 as those did not
              solve the write support bug.
            * Removed all semaphores
            * Added two pthread mutexes to protect virtual image and virtual
              info file.
  20090508: * Configure script will now exit when needed libraries aren't found
            * Added support for newest libewf beta version 20090506 as it seems
              to reduce memory usage when working with EWF files by about 1/2.
            * Added LIBEWF_BETA define to adept source to new libewf API.
            * Added function InitVirtualVmdkFile to build a VmWare virtual disk
              descriptor file.
  20090519: * Added function CreateVirtDir implementing FUSE's mkdir to allow
              VMWare to create his <iname>.vmdk.lck lock folder. Function does
              not allow to create other folders!
            * Changed cache file handling as VMDK caching will need new cache
              file structure incompatible to the old one.
  20090522: v0.3.0 released
            * Added function DeleteVirtFile and DeleteVirtDir so VMWare can
              remove his lock directories and files.
            * Added function RenameVirtFile because VMWare needs to rename his
              lock files.
            * VMDK support should work now but descriptor file won't get cached
              as I didn't implement it yet.
  20090604: * Added --cache commandline parameter doing the same as --rw.
            * Added --owcache commandline parameter doing the same as --rw but
              overwrites any existing cache data. This can be handy for
              debugging and testing purposes.
            * Added "vmdks" output type. Same as "vmdk" but generates a disk
              connected to the SCSI bus rather than the IDE bus.
  20090710: v0.3.1 released
  20090721: * Added function CheckFuseAllowOther to check wether FUSE supports
              the "-o allow_other" option. It is supported when
              "user_allow_other" is set in /etc/fuse.conf or when running
              xmount as root.
            * Automatic addition of FUSE's "-o allow_other" option if it is
              supported.
            * Added special "-o no_allow_other" command line parameter to
              disable automatic addition of the above option.
            * Reorganisation of FUSE's and xmount's command line options
              processing.
            * Added LogWarnMessage function to output a warning message.
  20090722: * Added function CalculateInputImageHash to calculate an MD5 hash
              of the first input image's HASH_AMOUNT bytes of data. This hash is
              used as VDI creation UUID and will later be used to match cache
              files to input images.
  20090724: v0.3.2 released
  20090725: v0.4.0 released
            * Added AFF input image support.
            * Due to various problems with libewf and libaff packages (Mainly
              in Debian and Ubuntu), I decided to include them into xmount's
              source tree and link them in statically. This has the advantage
              that I can use whatever version I want.
  20090727: v0.4.1 released
            * Added again the ability to compile xmount with shared libs as the
              Debian folks don't like the static ones :)
  20090812: * Added TXMountConfData.OrigImageSize and
              TXMountConfData.VirtImageSize to save the size of the input and
              output image in order to avoid regetting it always from disk.
  20090814: * Replaced all malloc and realloc occurences with the two macros
              XMOUNT_MALLOC and XMOUNT_REALLOC.
  20090816: * Replaced where applicable all occurences of str(n)cpy or
              alike with their corresponding macros XMOUNT_STRSET, XMOUNT_STRCPY
              and XMOUNT_STRNCPY pendants.
  20090907: v0.4.2 released
            * Fixed a bug in VMDK lock file access. glob_vmdk_lockfile_size
              wasn't reset to 0 when the file was deleted.
            * Fixed a bug in VMDK descriptor file access. Had to add
              glob_vmdk_file_size to track the size of this file as strlen was
              a bad idea :).
  20100324: v0.4.3 released
            * Changed all header structs to prevent different sizes on i386 and
              amd64. See xmount.h for more details.
  20100810: v0.4.4 released
            * Found a bug in InitVirtVdiHeader(). The 64bit values were
              addressed incorrectly while filled with rand(). This leads to an
              error message when trying to add a VDI file to VirtualBox 3.2.8.
  20110210: * Adding subtype and fsname FUSE options in order to display mounted
              source in mount command output.
  20110211: v0.4.5 released
  20111011: * Changes to deal with libewf v2 API (thx to Joachim Metz)
  20111109: v0.4.6 released
            * Added support for DMG output type (actually a DD with .dmg file
              extension). This type is used as default output type when
              using xmount under Mac OS X.
  20120130: v0.4.7 released
            * Made InitVirtImageInfoFile less picky about missing EWF infos.
  20120507: * Added support for VHD output image as requested by various people.
            * Statically linked libs updated to 20120504 (libewf) and 3.7.0
              (afflib).
  20120510: v0.5.0 released
            * Added stbuf->st_blocks calculation for VHD images in function
              GetVirtFileAttr. This makes Windows not think the emulated
              file would be a sparse file. Sparse vhd files are not attachable
              in Windows.
  20130726: v0.6.0 released
            * Added libaaff to replace libaff (thx to Guy Voncken).
            * Added libdd to replace raw dd input file handling and finally
              support split dd files (thx to Guy Voncken).
  20140311: * Added libaewf (thx to Guy Voncken).
  20140726: * Added support for dynamically loading of input libs. This should
              ease adding support for new input image formats in the future.
            * Moved input image functions to their corresponding dynamically
              loadable libs.
            * Prepended "glob_" to all global vars for better identification.
  20140731: * Added --offset option as requested by HPM.
            * Began massive code cleanup.
  20140803: * Added correct return code handling when calling input lib
              functions including getting error messages using GetErrorMessage.
            * Added input lib specific option parsing.
            * Re-implemented InitVirtImageInfoFile using input lib's
              GetInfofileContent function.
            * Further code cleanups.
*/
