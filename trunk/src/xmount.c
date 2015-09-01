/*******************************************************************************
* xmount Copyright (c) 2008-2015 by Gillen Daniel <gillen.dan@pinguin.lu>      *
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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h> // For PRI*
#include <errno.h>
#include <dlfcn.h> // For dlopen, dlclose, dlsym
#include <dirent.h> // For opendir, readdir, closedir
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/stat.h> // For fstat
#include <sys/types.h>
#ifdef HAVE_LINUX_FS_H
  #include <linux/fs.h> // For SEEK_* ??
#endif
#if !defined(__APPLE__) && defined(HAVE_GRP_H) && defined(HAVE_PWD_H)
  #include <grp.h> // For getgrnam, struct group
  #include <pwd.h> // For getpwuid, struct passwd
#endif
#include <pthread.h>
#include <time.h> // For time

#define FUSE_USE_VERSION 26
#include <fuse.h>

#include "xmount.h"
#include "md5.h"
#include "macros.h"
#include "../libxmount/libxmount.h"

#define XMOUNT_COPYRIGHT_NOTICE \
  "xmount v%s Copyright (c) 2008-2015 by Gillen Daniel <gillen.dan@pinguin.lu>"

#define LOG_WARNING(...) {            \
  LIBXMOUNT_LOG_WARNING(__VA_ARGS__); \
}
#define LOG_ERROR(...) {            \
  LIBXMOUNT_LOG_ERROR(__VA_ARGS__); \
}
#define LOG_DEBUG(...) {                              \
  LIBXMOUNT_LOG_DEBUG(glob_xmount.debug,__VA_ARGS__); \
}

/*******************************************************************************
 * Global vars
 ******************************************************************************/
//! Struct that contains various runtime configuration options
static ts_XmountData glob_xmount;

/*******************************************************************************
 * Forward declarations
 ******************************************************************************/
// Helper functions
static void PrintUsage(char*);
static void CheckFuseSettings();
static int ParseCmdLine(const int, char**);
static int ExtractVirtFileNames(char*);
static int GetMorphedImageSize(uint64_t*);
static int GetVirtImageSize(uint64_t*);
static int GetInputImageData(pts_InputImage, char*, off_t, size_t, size_t*);
static int GetMorphedImageData(char*, off_t, size_t, size_t*);
static int GetVirtImageData(char*, off_t, size_t);
static int SetVdiFileHeaderData(char*, off_t, size_t);
static int SetVhdFileHeaderData(char*, off_t, size_t);
static int SetVirtImageData(const char*, off_t, size_t);
static int CalculateInputImageHash(uint64_t*, uint64_t*);
static int InitVirtVdiHeader();
static int InitVirtVhdHeader();
static int InitVirtualVmdkFile();
static int InitVirtImageInfoFile();
static int InitCacheFile();
static int LoadLibs();
static int FindInputLib(pts_InputImage);
static int FindMorphingLib();
static void InitResources();
static void FreeResources();
static int SplitLibraryParameters(char*, uint32_t*, pts_LibXmountOptions**);
// Functions exported to LibXmount_Morphing
static int LibXmount_Morphing_ImageCount(uint64_t*);
static int LibXmount_Morphing_Size(uint64_t, uint64_t*);
static int LibXmount_Morphing_Read(uint64_t, char*, off_t, size_t, size_t*);
// Functions implementing FUSE functions
static int FuseGetAttr(const char*, struct stat*);
static int FuseMkDir(const char*, mode_t);
static int FuseMkNod(const char*, mode_t, dev_t);
static int FuseReadDir(const char*,
                       void*,
                       fuse_fill_dir_t,
                       off_t,
                       struct fuse_file_info*);
static int FuseOpen(const char*, struct fuse_file_info*);
static int FuseRead(const char*, char*, size_t, off_t, struct fuse_file_info*);
static int FuseRename(const char*, const char*);
static int FuseRmDir(const char*);
static int FuseUnlink(const char*);
//static int FuseStatFs(const char*, struct statvfs*);
static int FuseWrite(const char *p_path,
                     const char*,
                     size_t,
                     off_t,
                     struct fuse_file_info*);

/*******************************************************************************
 * Helper functions
 ******************************************************************************/
//! Print usage instructions (cmdline options etc..)
/*!
 * \param p_prog_name Program name (argv[0])
 */
static void PrintUsage(char *p_prog_name) {
  char *p_buf;
  int first;
  int ret;

  printf("\n" XMOUNT_COPYRIGHT_NOTICE "\n",XMOUNT_VERSION);
  printf("\nUsage:\n");
  printf("  %s [fopts] <xopts> <mntp>\n\n",p_prog_name);
  printf("Options:\n");
  printf("  fopts:\n");
  printf("    -d : Enable FUSE's and xmount's debug mode.\n");
  printf("    -h : Display this help message.\n");
  printf("    -s : Run single threaded.\n");
  printf("    -o no_allow_other : Disable automatic addition of FUSE's "
           "allow_other option.\n");
  printf("    -o <fopts> : Specify fuse mount options. Will also disable "
           "automatic addition of FUSE's allow_other option!\n");
  printf("\n");
  printf("  xopts:\n");
  printf("    --cache <cfile> : Enable virtual write support.\n");
  printf("      <cfile> specifies the cache file to use.\n");
  printf("    --in <itype> <ifile> : Input image format and source file(s). "
           "May be specified multiple times.\n");
  printf("      <itype> can be ");

  // List supported input formats
  first=1;
  for(uint32_t i=0;i<glob_xmount.input.libs_count;i++) {
    p_buf=glob_xmount.input.pp_libs[i]->p_supported_input_types;
    while(*p_buf!='\0') {
      if(first==1) {
        printf("\"%s\"",p_buf);
        first=0;
      } else printf(", \"%s\"",p_buf);
      p_buf+=(strlen(p_buf)+1);
    }
  }
  printf(".\n");

  printf("      <ifile> specifies the source file. If your image is split into "
           "multiple files, you have to specify them all!\n");
  printf("    --inopts <iopts> : Specify input library specific options.\n");
  printf("      <iopts> specifies a comma separated list of key=value options. "
           "See below for details.\n");
  printf("    --info : Print out infos about used compiler and libraries.\n");
  printf("    --morph <mtype> : Morphing function to apply to input image(s). "
           "If not specified, defaults to \"combine\".\n");
  printf("      <mtype> can be ");

  // List supported morphing functions
  first=1;
  for(uint32_t i=0;i<glob_xmount.morphing.libs_count;i++) {
    p_buf=glob_xmount.morphing.pp_libs[i]->p_supported_morphing_types;
    while(*p_buf!='\0') {
      if(first==1) {
        printf("\"%s\"",p_buf);
        first=0;
      } else printf(", \"%s\"",p_buf);
      p_buf+=(strlen(p_buf)+1);
    }
  }
  printf(".\n");

  printf("    --morphopts <mopts> : Specify morphing library specific "
           "options.\n");
  printf("      <mopts> specifies a comma separated list of key=value options. "
           "See below for details.\n");
  printf("    --offset <off> : Move the output image data start <off> bytes "
           "into the input image(s).\n");
  printf("    --out <otype> : Output image format. If not specified, "
           "defaults to ");
#ifdef __APPLE__
  printf("\"dmg\".\n");
#else
  printf("\"raw\".\n");
#endif
  printf("      <otype> can be ");

  // List supported output formats
  printf("\"raw\", \"dmg\", \"vdi\", \"vhd\", \"vmdk\", \"vmdks\".\n");

  printf("    --owcache <file> : Same as --cache <file> but overwrites "
           "existing cache file.\n");
  printf("    --sizelimit <size> : The data end of input image(s) is set to no "
           "more than <size> bytes after the data start.\n");
  printf("    --version : Same as --info.\n");
  printf("\n");
  printf("  mntp:\n");
  printf("    Mount point where output image should be located.\n");
  printf("\n");
  printf("Infos:\n");
  printf("  * One --in option and a mount point are mandatory!\n");
  printf("  * If you specify --in multiple times, data from all images is "
           "morphed into one output image using the specified morphing "
           "function.\n");
  printf("  * For VMDK emulation, you have to uncomment \"user_allow_other\" "
           "in /etc/fuse.conf or run xmount as root.\n");
  printf("\n");
  printf("Input / Morphing library specific options:\n");
  printf("  Input / Morphing libraries might support an own set of "
           "options to configure / tune their behaviour.\n");
  printf("  Libraries supporting this feature (if any) and their "
           "options are listed below.\n");
  printf("\n");

  // List input and morphing lib options
  for(uint32_t i=0;i<glob_xmount.input.libs_count;i++) {
    ret=glob_xmount.input.pp_libs[i]->
          lib_functions.OptionsHelp((const char**)&p_buf);
    if(ret!=0) {
      LOG_ERROR("Unable to get options help for library '%s': %s!\n",
                glob_xmount.input.pp_libs[i]->p_name,
                glob_xmount.input.pp_libs[i]->
                  lib_functions.GetErrorMessage(ret));
    }
    if(p_buf==NULL) continue;
    printf("  - %s\n",glob_xmount.input.pp_libs[i]->p_name);
    printf("%s",p_buf);
    printf("\n");
    ret=glob_xmount.input.pp_libs[i]->lib_functions.FreeBuffer(p_buf);
    if(ret!=0) {
      LOG_ERROR("Unable to free options help text from library '%s': %s!\n",
                glob_xmount.input.pp_libs[i]->p_name,
                glob_xmount.input.pp_libs[i]->
                  lib_functions.GetErrorMessage(ret));
    }
  }
  for(uint32_t i=0;i<glob_xmount.morphing.libs_count;i++) {
    ret=glob_xmount.morphing.pp_libs[i]->
          lib_functions.OptionsHelp((const char**)&p_buf);
    if(ret!=0) {
      LOG_ERROR("Unable to get options help for library '%s': %s!\n",
                glob_xmount.morphing.pp_libs[i]->p_name,
                glob_xmount.morphing.pp_libs[i]->
                  lib_functions.GetErrorMessage(ret));
    }
    if(p_buf==NULL) continue;
    printf("  - %s\n",glob_xmount.morphing.pp_libs[i]->p_name);
    printf("%s",p_buf);
    printf("\n");
  }
}

//! Check fuse settings
/*!
 * Check if FUSE allows us to pass the -o allow_other parameter. This only works
 * if we are root or user_allow_other is set in /etc/fuse.conf.
 *
 * In addition, this function also checks if the user is member of the fuse
 * group which is generally needed to use fuse at all.
 */
static void CheckFuseSettings() {
#if !defined(__APPLE__) && defined(HAVE_GRP_H) && defined(HAVE_PWD_H)
  struct group *p_group;
  struct passwd *p_passwd;
#endif
  int found;
  FILE *h_fuse_conf;
  char line[256];

  glob_xmount.may_set_fuse_allow_other=FALSE;

  if(geteuid()==0) {
    // Running as root, there should be no problems
    glob_xmount.may_set_fuse_allow_other=TRUE;
    return;
  }

#if !defined(__APPLE__) && defined(HAVE_GRP_H) && defined(HAVE_PWD_H)
  // Check if a fuse group exists and if so, make sure user is a member of it.
  // Makes only sense on Linux because as far as I know osxfuse has no own group
  p_group=getgrnam("fuse");
  if(p_group!=NULL) {
    // Get effective user name
    p_passwd=getpwuid(geteuid());
    if(p_passwd==NULL) {
      printf("\nWARNING: Unable to determine your effective user name. If "
             "mounting works, you can ignore this message.\n\n");
      return;
    }
    // Check if user is member of fuse group
    found=FALSE;
    while(*(p_group->gr_mem)!=NULL) {
      if(strcmp(*(p_group->gr_mem),p_passwd->pw_name)==0) {
        found=TRUE;
        break;
      }
      p_group->gr_mem++;
    }
    if(found==FALSE) {
      printf("\nWARNING: You are not a member of the \"fuse\" group. This will "
               "prevent you from mounting images using xmount. Please add "
               "yourself to the \"fuse\" group using the command "
               "\"sudo usermod -a -G fuse %s\" and reboot your system or "
               "execute xmount as root.\n\n",
             p_passwd->pw_name);
      return;
    }
  } else {
    printf("\nWARNING: Your system does not seem to have a \"fuse\" group. If "
             "mounting works, you can ignore this message.\n\n");
  }
#endif

  // Read FUSE's config file /etc/fuse.conf and check for set user_allow_other
  h_fuse_conf=(FILE*)FOPEN("/etc/fuse.conf","r");
  if(h_fuse_conf!=NULL) {
    // Search conf file for set user_allow_others
    found=FALSE;
    while(fgets(line,sizeof(line),h_fuse_conf)!=NULL) {
      // TODO: This works as long as there is no other parameter beginning with
      // "user_allow_other" :)
      if(strncmp(line,"user_allow_other",16)==0) {
        found=TRUE;
        break;
      }
    }
    fclose(h_fuse_conf);
    if(found==TRUE) {
      glob_xmount.may_set_fuse_allow_other=TRUE;
    } else {
      printf("\nWARNING: FUSE will not allow other users nor root to access "
               "your virtual harddisk image. To change this behavior, please "
               "add \"user_allow_other\" to /etc/fuse.conf or execute xmount "
               "as root.\n\n");
    }
  } else {
    printf("\nWARNING: Unable to open /etc/fuse.conf. If mounting works, you "
             "can ignore this message. If you encounter issues, please create "
             "the file and add a single line containing the string "
             "\"user_allow_other\" or execute xmount as root.\n\n");
    return;
  }
}

//! Parse command line options
/*!
 * \param argc Number of cmdline params
 * \param pp_argv Array containing cmdline params
 * \return TRUE on success, FALSE on error
 */
#define SUPPORT_DEPRECATED_IN 1
static int ParseCmdLine(const int argc, char **pp_argv) {
  int i=1;
  int FuseMinusOControl=TRUE;
  int FuseAllowOther=TRUE;
  int first;
  char *p_buf;
  pts_InputImage p_input_image=NULL;
  int ret;
#ifdef SUPPORT_DEPRECATED_IN
  int use_old_in_syntax=FALSE;
#endif

  // add pp_argv[0] to FUSE's argv
  XMOUNT_MALLOC(glob_xmount.pp_fuse_argv,char**,sizeof(char*));
  XMOUNT_STRSET(glob_xmount.pp_fuse_argv[0],pp_argv[0]);
  glob_xmount.fuse_argc=1;

  // Parse options
  while(i<argc && *pp_argv[i]=='-') {
    if(strlen(pp_argv[i])>1 && *(pp_argv[i]+1)!='-') {
      // Options beginning with one - are mostly FUSE specific
      if(strcmp(pp_argv[i],"-d")==0) {
        // Enable FUSE's and xmount's debug mode
        XMOUNT_REALLOC(glob_xmount.pp_fuse_argv,
                       char**,
                       (glob_xmount.fuse_argc+1)*sizeof(char*));
        XMOUNT_STRSET(glob_xmount.pp_fuse_argv[glob_xmount.fuse_argc],
                      pp_argv[i])
        glob_xmount.fuse_argc++;
        glob_xmount.debug=TRUE;
      } else if(strcmp(pp_argv[i],"-h")==0) {
        // Print help message
        PrintUsage(pp_argv[0]);
        exit(0);
      } else if(strcmp(pp_argv[i],"-o")==0) {
        // Next parameter specifies fuse mount options
        if((i+1)<argc) {
          i++;
          // As the user specified the -o option, we assume he knows what he is
          // doing. We won't append allow_other automatically. And we allow him
          // to disable allow_other by passing a single "-o no_allow_other"
          // which won't be passed to FUSE as it is xmount specific.
          if(strcmp(pp_argv[i],"no_allow_other")!=0) {
            glob_xmount.fuse_argc+=2;
            XMOUNT_REALLOC(glob_xmount.pp_fuse_argv,
                           char**,
                           glob_xmount.fuse_argc*sizeof(char*));
            XMOUNT_STRSET(glob_xmount.pp_fuse_argv[glob_xmount.fuse_argc-2],
                          pp_argv[i-1]);
            XMOUNT_STRSET(glob_xmount.pp_fuse_argv[glob_xmount.fuse_argc-1],
                          pp_argv[i]);
            FuseMinusOControl=FALSE;
          } else FuseAllowOther=FALSE;
        } else {
          LOG_ERROR("Couldn't parse fuse mount options!\n")
          return FALSE;
        }
      } else if(strcmp(pp_argv[i],"-s")==0) {
        // Enable FUSE's single threaded mode
        XMOUNT_REALLOC(glob_xmount.pp_fuse_argv,
                       char**,
                       (glob_xmount.fuse_argc+1)*sizeof(char*));
        XMOUNT_STRSET(glob_xmount.pp_fuse_argv[glob_xmount.fuse_argc],
                      pp_argv[i]);
        glob_xmount.fuse_argc++;
      } else if(strcmp(pp_argv[i],"-V")==0) {
        // Display FUSE version info
        XMOUNT_REALLOC(glob_xmount.pp_fuse_argv,
                       char**,
                       (glob_xmount.fuse_argc+1)*sizeof(char*));
        XMOUNT_STRSET(glob_xmount.pp_fuse_argv[glob_xmount.fuse_argc],
                      pp_argv[i]);
        glob_xmount.fuse_argc++;
      } else {
        LOG_ERROR("Unknown command line option \"%s\"\n",pp_argv[i]);
        return FALSE;
      }
    } else {
      // Options beginning with -- are xmount specific
      if(strcmp(pp_argv[i],"--cache")==0 /*|| strcmp(pp_argv[i],"--rw")==0*/) {
        // Emulate writable access to mounted image
        // Next parameter must be cache file to read/write changes from/to
        if((i+1)<argc) {
          i++;
          XMOUNT_STRSET(glob_xmount.cache.p_cache_file,pp_argv[i])
          glob_xmount.output.writable=TRUE;
        } else {
          LOG_ERROR("You must specify a cache file!\n")
          return FALSE;
        }
        LOG_DEBUG("Enabling virtual write support using cache file \"%s\"\n",
                  glob_xmount.cache.p_cache_file)
      } else if(strcmp(pp_argv[i],"--in")==0) {
        // Specify input image type and source files
#ifdef SUPPORT_DEPRECATED_IN
        if(use_old_in_syntax==TRUE) {
          LOG_ERROR("When using the deprecated --in option syntax, it is not "
                      "possible to specify --in multiple times!\n");
          free(p_input_image->p_type);
          free(p_input_image);
          return FALSE;
        }
#endif
        if((i+2)<argc) {
          i++;
          // Alloc and init new ts_InputImage struct
          XMOUNT_MALLOC(p_input_image,pts_InputImage,sizeof(ts_InputImage));
          XMOUNT_STRSET(p_input_image->p_type,pp_argv[i]);
          p_input_image->pp_files=NULL;
          p_input_image->p_functions=NULL;
          p_input_image->p_handle=NULL;
          // Parse input image filename(s) and add to p_input_image->pp_files
          i++;
          p_input_image->files_count=0;
          while(i<(argc-1) && strncmp(pp_argv[i],"--",2)!=0) {
            p_input_image->files_count++;
            XMOUNT_REALLOC(p_input_image->pp_files,
                           char**,
                           p_input_image->files_count*sizeof(char*));
            XMOUNT_STRSET(p_input_image->pp_files[p_input_image->files_count-1],
                          pp_argv[i]);
            i++;
          }
          i--;
          if(p_input_image->files_count==0) {
#ifndef SUPPORT_DEPRECATED_IN
            LOG_ERROR("No input files specified for \"--in %s\"!\n",
                      p_input_image->p_type)
            free(p_input_image->p_type);
            free(p_input_image);
            return FALSE;
#else
            use_old_in_syntax=TRUE;
            i++;
            continue;
#endif
          }
          // Add input image struct to input image array
          glob_xmount.input.images_count++;
          XMOUNT_REALLOC(glob_xmount.input.pp_images,
                         pts_InputImage*,
                         glob_xmount.input.images_count*
                           sizeof(pts_InputImage));
          glob_xmount.input.pp_images[glob_xmount.input.images_count-1]=
            p_input_image;
        } else {
          LOG_ERROR("You must specify an input image type and source file!\n");
          return FALSE;
        }
      } else if(strcmp(pp_argv[i],"--inopts")==0) {
        // Set input lib options
        if((i+1)<argc) {
          i++;
          if(glob_xmount.input.pp_lib_params==NULL) {
            if(SplitLibraryParameters(pp_argv[i],
                                      &(glob_xmount.input.lib_params_count),
                                      &(glob_xmount.input.pp_lib_params)
                                     )==FALSE)
            {
              LOG_ERROR("Unable to parse input library options '%s'!\n",
                        pp_argv[i]);
              return FALSE;
            }
          } else {
            LOG_ERROR("You can only specify --inopts once!")
            return FALSE;
          }
        } else {
          LOG_ERROR("You must specify special options!\n");
          return FALSE;
        }
      } else if(strcmp(pp_argv[i],"--morph")==0) {
        // Set morphing lib to use
        if((i+1)<argc) {
          i++;
          if(glob_xmount.morphing.p_morph_type==NULL) {
            XMOUNT_STRSET(glob_xmount.morphing.p_morph_type,pp_argv[i]);
          } else {
            LOG_ERROR("You can only specify --morph once!")
            return FALSE;
          }
        } else {
          LOG_ERROR("You must specify morphing type!\n");
          return FALSE;
        }
      } else if(strcmp(pp_argv[i],"--morphopts")==0) {
        // Set morphing lib options
        if((i+1)<argc) {
          i++;
          if(glob_xmount.morphing.pp_lib_params==NULL) {
            if(SplitLibraryParameters(pp_argv[i],
                                      &(glob_xmount.morphing.lib_params_count),
                                      &(glob_xmount.morphing.pp_lib_params)
                                     )==FALSE)
            {
              LOG_ERROR("Unable to parse morphing library options '%s'!\n",
                        pp_argv[i]);
              return FALSE;
            }
          } else {
            LOG_ERROR("You can only specify --morphopts once!")
            return FALSE;
          }
        } else {
          LOG_ERROR("You must specify special morphing lib params!\n");
          return FALSE;
        }
      } else if(strcmp(pp_argv[i],"--offset")==0) {
        // Set input image offset
        if((i+1)<argc) {
          i++;
          glob_xmount.input.image_offset=StrToUint64(pp_argv[i],&ret);
          if(ret==0) {
            LOG_ERROR("Unable to convert '%s' to a number!\n",pp_argv[i])
            return FALSE;
          }
        } else {
          LOG_ERROR("You must specify an offset!\n")
          return FALSE;
        }
        LOG_DEBUG("Setting input image offset to \"%" PRIu64 "\"\n",
                  glob_xmount.input.image_offset)
      } else if(strcmp(pp_argv[i],"--out")==0) {
        // Specify output image type
        // Next parameter must be image type
        if((i+1)<argc) {
          i++;
          if(strcmp(pp_argv[i],"raw")==0) {
            glob_xmount.output.VirtImageType=VirtImageType_DD;
            LOG_DEBUG("Setting virtual image type to RAW\n")
          } else if(strcmp(pp_argv[i],"dd")==0) {
            glob_xmount.output.VirtImageType=VirtImageType_DD;
            LOG_WARNING("Using '--out dd' is deprecated and will be removed in "
                          "the next release. "
                          "Please use '--out raw' instead.\n");
            LOG_DEBUG("Setting virtual image type to RAW\n")
          } else if(strcmp(pp_argv[i],"dmg")==0) {
            glob_xmount.output.VirtImageType=VirtImageType_DMG;
            LOG_DEBUG("Setting virtual image type to DMG\n")
          } else if(strcmp(pp_argv[i],"vdi")==0) {
            glob_xmount.output.VirtImageType=VirtImageType_VDI;
            LOG_DEBUG("Setting virtual image type to VDI\n")
          } else if(strcmp(pp_argv[i],"vhd")==0) {
            glob_xmount.output.VirtImageType=VirtImageType_VHD;
            LOG_DEBUG("Setting virtual image type to VHD\n")
          } else if(strcmp(pp_argv[i],"vmdk")==0) {
            glob_xmount.output.VirtImageType=VirtImageType_VMDK;
            LOG_DEBUG("Setting virtual image type to VMDK\n")
          } else if(strcmp(pp_argv[i],"vmdks")==0) {
            glob_xmount.output.VirtImageType=VirtImageType_VMDKS;
            LOG_DEBUG("Setting virtual image type to VMDKS\n")
          } else {
            LOG_ERROR("Unknown output image type \"%s\"!\n",pp_argv[i])
            return FALSE;
          }
        } else {
          LOG_ERROR("You must specify an output image type!\n");
          return FALSE;
        }
      } else if(strcmp(pp_argv[i],"--owcache")==0) {
        // Enable writable access to mounted image and overwrite existing cache
        // Next parameter must be cache file to read/write changes from/to
        if((i+1)<argc) {
          i++;
          XMOUNT_STRSET(glob_xmount.cache.p_cache_file,pp_argv[i])
          glob_xmount.output.writable=TRUE;
          glob_xmount.cache.overwrite_cache=TRUE;
        } else {
          LOG_ERROR("You must specify a cache file!\n")
          return FALSE;
        }
        LOG_DEBUG("Enabling virtual write support overwriting cache file %s\n",
                  glob_xmount.cache.p_cache_file)
      } else if(strcmp(pp_argv[i],"--sizelimit")==0) {
        // Set input image size limit
        if((i+1)<argc) {
          i++;
          glob_xmount.input.image_size_limit=StrToUint64(pp_argv[i],&ret);
          if(ret==0) {
            LOG_ERROR("Unable to convert '%s' to a number!\n",pp_argv[i])
            return FALSE;
          }
        } else {
          LOG_ERROR("You must specify a size limit!\n")
          return FALSE;
        }
        LOG_DEBUG("Setting input image size limit to \"%" PRIu64 "\"\n",
                  glob_xmount.input.image_size_limit)
      } else if(strcmp(pp_argv[i],"--version")==0 ||
                strcmp(pp_argv[i],"--info")==0)
      {
        // Print xmount info
        printf(XMOUNT_COPYRIGHT_NOTICE "\n\n",XMOUNT_VERSION);
#ifdef __GNUC__
        printf("  compile timestamp: %s %s\n",__DATE__,__TIME__);
        printf("  gcc version: %s\n",__VERSION__);
#endif
        printf("  loaded input libraries:\n");
        for(uint32_t ii=0;ii<glob_xmount.input.libs_count;ii++) {
          printf("    - %s supporting ",glob_xmount.input.pp_libs[ii]->p_name);
          p_buf=glob_xmount.input.pp_libs[ii]->p_supported_input_types;
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
        printf("  loaded morphing libraries:\n");
        for(uint32_t ii=0;ii<glob_xmount.morphing.libs_count;ii++) {
          printf("    - %s supporting ",
                 glob_xmount.morphing.pp_libs[ii]->p_name);
          p_buf=glob_xmount.morphing.pp_libs[ii]->p_supported_morphing_types;
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
      } else {
        LOG_ERROR("Unknown command line option \"%s\"\n",pp_argv[i]);
        return FALSE;
      }
    }
    i++;
  }

#ifdef SUPPORT_DEPRECATED_IN
  if(use_old_in_syntax==TRUE && i<(argc-1)) {
    LOG_WARNING("You are using a deprecated --in option syntax which will be "
                  "removed in the next release. Please see the man page "
                  "on how to use the new syntax.\n");
    while(i<(argc-1)) {
      p_input_image->files_count++;
      XMOUNT_REALLOC(p_input_image->pp_files,
                     char**,
                     p_input_image->files_count*sizeof(char*));
      XMOUNT_STRSET(p_input_image->pp_files[p_input_image->files_count-1],
                    pp_argv[i]);
      i++;
    }
    // Add input image struct to input image array
    glob_xmount.input.images_count++;
    XMOUNT_REALLOC(glob_xmount.input.pp_images,
                   pts_InputImage*,
                   glob_xmount.input.images_count*sizeof(pts_InputImage));
    glob_xmount.input.pp_images[glob_xmount.input.images_count-1]=p_input_image;
  } else if(use_old_in_syntax==TRUE) {
    free(p_input_image->p_type);
    free(p_input_image);
    LOG_ERROR("You must specify an input image type, source file(s) and a "
                "mount point!\n");
    return FALSE;
  }
#endif

  // Extract mountpoint
  if(i==(argc-1)) {
    XMOUNT_STRSET(glob_xmount.p_mountpoint,pp_argv[argc-1])
    XMOUNT_REALLOC(glob_xmount.pp_fuse_argv,
                   char**,
                   (glob_xmount.fuse_argc+1)*sizeof(char*));
    XMOUNT_STRSET(glob_xmount.pp_fuse_argv[glob_xmount.fuse_argc],
                  glob_xmount.p_mountpoint);
    glob_xmount.fuse_argc++;
  } else {
    LOG_ERROR("No mountpoint specified!\n")
    return FALSE;
  }

  if(FuseMinusOControl==TRUE) {
    // We control the -o flag, set subtype, fsname and allow_other options
    glob_xmount.fuse_argc+=2;
    XMOUNT_REALLOC(glob_xmount.pp_fuse_argv,
                   char**,
                   glob_xmount.fuse_argc*sizeof(char*));
    XMOUNT_STRSET(glob_xmount.pp_fuse_argv[glob_xmount.fuse_argc-2],"-o");
    XMOUNT_STRSET(glob_xmount.pp_fuse_argv[glob_xmount.fuse_argc-1],
                  "subtype=xmount");
    if(glob_xmount.input.images_count!=0) {
      // Set name of first source file as fsname
      XMOUNT_STRAPP(glob_xmount.pp_fuse_argv[glob_xmount.fuse_argc-1],
                    ",fsname='");
      // If possible, use full path
      p_buf=realpath(glob_xmount.input.pp_images[0]->pp_files[0],NULL);
      if(p_buf==NULL) {
        XMOUNT_STRSET(p_buf,glob_xmount.input.pp_images[0]->pp_files[0]);
      }
      // Make sure fsname does not include some forbidden chars
      for(uint32_t i=0;i<strlen(p_buf);i++) {
        if(p_buf[i]=='\'') p_buf[i]='_';
      }
      // Set fsname
      XMOUNT_STRAPP(glob_xmount.pp_fuse_argv[glob_xmount.fuse_argc-1],
                    p_buf);
      XMOUNT_STRAPP(glob_xmount.pp_fuse_argv[glob_xmount.fuse_argc-1],
                    "'");
      free(p_buf);
    }
    if(FuseAllowOther==TRUE) {
      // Add "allow_other" option if allowed
      if(glob_xmount.may_set_fuse_allow_other) {
        XMOUNT_STRAPP(glob_xmount.pp_fuse_argv[glob_xmount.fuse_argc-1],
                      ",allow_other");
      }
    }
  }

  return TRUE;
}

//! Extract virtual file name from input image name
/*!
 * \param p_orig_name Name of input image (may include a path)
 * \return TRUE on success, FALSE on error
 */
static int ExtractVirtFileNames(char *p_orig_name) {
  char *tmp;

  // Truncate any leading path
  tmp=strrchr(p_orig_name,'/');
  if(tmp!=NULL) p_orig_name=tmp+1;

  // Extract file extension
  tmp=strrchr(p_orig_name,'.');

  // Set leading '/'
  XMOUNT_STRSET(glob_xmount.output.p_virtual_image_path,"/");
  XMOUNT_STRSET(glob_xmount.output.p_info_path,"/");
  if(glob_xmount.output.VirtImageType==VirtImageType_VMDK ||
     glob_xmount.output.VirtImageType==VirtImageType_VMDKS)
  {
    XMOUNT_STRSET(glob_xmount.output.vmdk.p_virtual_vmdk_path,"/");
  }

  // Copy filename
  if(tmp==NULL) {
    // Input image filename has no extension
    XMOUNT_STRAPP(glob_xmount.output.p_virtual_image_path,p_orig_name);
    XMOUNT_STRAPP(glob_xmount.output.p_info_path,p_orig_name);
    if(glob_xmount.output.VirtImageType==VirtImageType_VMDK ||
       glob_xmount.output.VirtImageType==VirtImageType_VMDKS)
    {
      XMOUNT_STRAPP(glob_xmount.output.vmdk.p_virtual_vmdk_path,p_orig_name);
    }
    XMOUNT_STRAPP(glob_xmount.output.p_info_path,".info");
  } else {
    XMOUNT_STRNAPP(glob_xmount.output.p_virtual_image_path,p_orig_name,
                   strlen(p_orig_name)-strlen(tmp));
    XMOUNT_STRNAPP(glob_xmount.output.p_info_path,p_orig_name,
                   strlen(p_orig_name)-strlen(tmp));
    if(glob_xmount.output.VirtImageType==VirtImageType_VMDK ||
       glob_xmount.output.VirtImageType==VirtImageType_VMDKS)
    {
      XMOUNT_STRNAPP(glob_xmount.output.vmdk.p_virtual_vmdk_path,p_orig_name,
                     strlen(p_orig_name)-strlen(tmp));
    }
    XMOUNT_STRAPP(glob_xmount.output.p_info_path,".info");
  }

  // Add virtual file extensions
  switch(glob_xmount.output.VirtImageType) {
    case VirtImageType_DD:
      XMOUNT_STRAPP(glob_xmount.output.p_virtual_image_path,".dd");
      break;
    case VirtImageType_DMG:
      XMOUNT_STRAPP(glob_xmount.output.p_virtual_image_path,".dmg");
      break;
    case VirtImageType_VDI:
      XMOUNT_STRAPP(glob_xmount.output.p_virtual_image_path,".vdi");
      break;
    case VirtImageType_VHD:
      XMOUNT_STRAPP(glob_xmount.output.p_virtual_image_path,".vhd");
      break;
    case VirtImageType_VMDK:
    case VirtImageType_VMDKS:
      XMOUNT_STRAPP(glob_xmount.output.p_virtual_image_path,".dd");
      XMOUNT_STRAPP(glob_xmount.output.vmdk.p_virtual_vmdk_path,".vmdk");
      break;
    default:
      LOG_ERROR("Unknown virtual image type!\n")
      return FALSE;
  }

  LOG_DEBUG("Set virtual image name to \"%s\"\n",
            glob_xmount.output.p_virtual_image_path);
  LOG_DEBUG("Set virtual image info name to \"%s\"\n",
            glob_xmount.output.p_info_path);
  if(glob_xmount.output.VirtImageType==VirtImageType_VMDK ||
     glob_xmount.output.VirtImageType==VirtImageType_VMDKS)
  {
    LOG_DEBUG("Set virtual vmdk name to \"%s\"\n",
              glob_xmount.output.vmdk.p_virtual_vmdk_path);
  }
  return TRUE;
}

//! Get size of morphed image
/*!
 * \param p_size Buf to save size to
 * \return TRUE on success, FALSE on error
 */
static int GetMorphedImageSize(uint64_t *p_size) {
  int ret;

  ret=glob_xmount.morphing.p_functions->Size(glob_xmount.morphing.p_handle,
                                             p_size);
  if(ret!=0) {
    LOG_ERROR("Unable to get morphed image size: %s!\n",
              glob_xmount.morphing.p_functions->GetErrorMessage(ret));
    return FALSE;
  }

  return TRUE;
}

//! Get size of virtual image
/*!
 * \param p_size Pointer to an uint64_t to which the size will be written to
 * \return TRUE on success, FALSE on error
 */
static int GetVirtImageSize(uint64_t *p_size) {
  if(glob_xmount.output.image_size!=0) {
    *p_size=glob_xmount.output.image_size;
    return TRUE;
  }

  switch(glob_xmount.output.VirtImageType) {
    case VirtImageType_DD:
    case VirtImageType_DMG:
    case VirtImageType_VMDK:
    case VirtImageType_VMDKS:
      // Virtual image is a DD, DMG or VMDK file. Just return the size of the
      // original image
      if(!GetMorphedImageSize(p_size)) {
        LOG_ERROR("Couldn't get size of input image!\n")
        return FALSE;
      }
      break;
    case VirtImageType_VDI:
      // Virtual image is a VDI file. Get size of original image and add size
      // of VDI header etc.
      if(!GetMorphedImageSize(p_size)) {
        LOG_ERROR("Couldn't get size of input image!\n")
        return FALSE;
      }
      (*p_size)+=(sizeof(ts_VdiFileHeader)+
                   glob_xmount.output.vdi.vdi_block_map_size);
      break;
    case VirtImageType_VHD:
      // Virtual image is a VHD file. Get size of original image and add size
      // of VHD footer.
      if(!GetMorphedImageSize(p_size)) {
        LOG_ERROR("Couldn't get size of input image!\n")
        return FALSE;
      }
      (*p_size)+=sizeof(ts_VhdFileHeader);
      break;
    default:
      LOG_ERROR("Unsupported image type!\n")
      return FALSE;
  }

  glob_xmount.output.image_size=*p_size;
  return TRUE;
}

//! Read data from input image
/*!
 * \param p_image Image from which to read data
 * \param p_buf Pointer to buffer to write read data to (must be preallocated!)
 * \param offset Offset at which data should be read
 * \param size Size of data which should be read (size of buffer)
 * \param p_read Number of read bytes on success
 * \return 0 on success, negated error code on error
 */
static int GetInputImageData(pts_InputImage p_image,
                             char *p_buf,
                             off_t offset,
                             size_t size,
                             size_t *p_read)
{
  int ret;
  size_t to_read=0;
  int read_errno=0;

  LOG_DEBUG("Reading %zu bytes at offset %zu from input image '%s'\n",
            size,
            offset,
            p_image->pp_files[0]);

  // Make sure we aren't reading past EOF of image file
  if(offset>=p_image->size) {
    // Offset is beyond image size
    LOG_DEBUG("Offset %zu is at / beyond size of input image '%s'\n",
              offset,
              p_image->pp_files[0]);
    *p_read=0;
    return 0;
  }
  if(offset+size>p_image->size) {
    // Attempt to read data past EOF of image file
    to_read=p_image->size-offset;
    LOG_DEBUG("Attempt to read data past EOF of input image '%s'. "
                "Correcting size from %zu to %zu\n",
              p_image->pp_files[0],
              size,
              to_read);
  } else to_read=size;

  // Read data from image file (adding input image offset if one was specified)
  ret=p_image->p_functions->Read(p_image->p_handle,
                                 p_buf,
                                 offset+glob_xmount.input.image_offset,
                                 to_read,
                                 p_read,
                                 &read_errno);
  if(ret!=0) {
    LOG_ERROR("Couldn't read %zu bytes at offset %zu from input image "
                "'%s': %s!\n",
              to_read,
              offset,
              p_image->pp_files[0],
              p_image->p_functions->GetErrorMessage(ret));
    if(read_errno==0) return -EIO;
    else return (read_errno*(-1));
  }

  return 0;
}

//! Read data from morphed image
/*!
 * \param p_buf Pointer to buffer to write read data to (must be preallocated!)
 * \param offset Offset at which data should be read
 * \param size Size of data which should be read (size of buffer)
 * \param p_read Number of read bytes on success
 * \return TRUE on success, negated error code on error
 */
static int GetMorphedImageData(char *p_buf,
                               off_t offset,
                               size_t size,
                               size_t *p_read)
{
  int ret;
  size_t to_read=0;
  size_t read;
  uint64_t image_size=0;

  // Make sure we aren't reading past EOF of image file
  if(GetMorphedImageSize(&image_size)!=TRUE) {
    LOG_ERROR("Couldn't get size of morphed image!\n");
    return -EIO;
  }
  if(offset>=image_size) {
    // Offset is beyond image size
    LOG_DEBUG("Offset %zu is at / beyond size of morphed image.\n",offset);
    *p_read=0;
    return 0;
  }
  if(offset+size>image_size) {
    // Attempt to read data past EOF of morphed image file
    to_read=image_size-offset;
    LOG_DEBUG("Attempt to read data past EOF of morphed image. Corrected size "
                "from %zu to %zu.\n",
              size,
              to_read);
  } else to_read=size;

  // Read data from morphed image
  ret=glob_xmount.morphing.p_functions->Read(glob_xmount.morphing.p_handle,
                                             p_buf,
                                             offset,
                                             to_read,
                                             &read);
  if(ret!=0) {
    LOG_ERROR("Couldn't read %zu bytes at offset %zu from morphed image: %s!\n",
              to_read,
              offset,
              glob_xmount.morphing.p_functions->GetErrorMessage(ret));
    return -EIO;
  }

  *p_read=to_read;
  return TRUE;
}

//! Read data from virtual image
/*!
 * \param p_buf Pointer to buffer to write read data to
 * \param offset Offset at which data should be read
 * \param size Size of data which should be read
 * \return Number of read bytes on success or negated error code on error
 */
static int GetVirtImageData(char *p_buf, off_t offset, size_t size) {
  uint32_t cur_block=0;
  uint64_t morphed_image_size, virt_image_size;
  size_t read, to_read=0, cur_to_read=0;
  off_t file_off=offset, block_off=0;
  size_t to_read_later=0;
  int ret;

  // Get virtual image size
  if(GetVirtImageSize(&virt_image_size)!=TRUE) {
    LOG_ERROR("Couldn't get size of virtual image!\n")
    return -EIO;
  }
  if(offset>=virt_image_size) {
    LOG_DEBUG("Offset %zu is at / beyond size of virtual image.\n",offset);
    return 0;
  }
  if(offset+size>virt_image_size) {
    LOG_DEBUG("Attempt to read data past EOF of virtual image. Corrected size "
                "from %zu to %zu.\n",
              size,
              virt_image_size-offset);
    size=virt_image_size-offset;
  }
  to_read=size;

  // Get morphed image size
  if(GetMorphedImageSize(&morphed_image_size)!=TRUE) {
    LOG_ERROR("Couldn't get morphed image size!")
    return -EIO;
  }

  // Read virtual image type specific data preceeding morphed image data
  switch(glob_xmount.output.VirtImageType) {
    case VirtImageType_DD:
    case VirtImageType_DMG:
    case VirtImageType_VMDK:
    case VirtImageType_VMDKS:
      break;
    case VirtImageType_VDI:
      if(file_off<glob_xmount.output.vdi.vdi_header_size) {
        if(file_off+to_read>glob_xmount.output.vdi.vdi_header_size) {
          cur_to_read=glob_xmount.output.vdi.vdi_header_size-file_off;
        } else {
          cur_to_read=to_read;
        }
        if(glob_xmount.output.writable==TRUE &&
           glob_xmount.cache.p_cache_header->VdiFileHeaderCached==TRUE)
        {
          // VDI header was already cached
          if(fseeko(glob_xmount.cache.h_cache_file,
                    glob_xmount.cache.p_cache_header->pVdiFileHeader+file_off,
                    SEEK_SET)!=0)
          {
            LOG_ERROR("Couldn't seek to cached VDI header at offset %"
                        PRIu64 "\n",
                      glob_xmount.cache.p_cache_header->pVdiFileHeader+file_off)
            return -EIO;
          }
          if(fread(p_buf,cur_to_read,1,glob_xmount.cache.h_cache_file)!=1) {
            LOG_ERROR("Couldn't read %zu bytes from cache file at offset %"
                        PRIu64 "\n",
                      cur_to_read,
                      glob_xmount.cache.p_cache_header->pVdiFileHeader+file_off)
            return -EIO;
          }
          LOG_DEBUG("Read %zd bytes from cached VDI header at offset %"
                      PRIu64 " at cache file offset %" PRIu64 "\n",
                    cur_to_read,
                    file_off,
                    glob_xmount.cache.p_cache_header->pVdiFileHeader+file_off)
        } else {
          // VDI header isn't cached
          memcpy(p_buf,
                 ((char*)glob_xmount.output.vdi.p_vdi_header)+file_off,
                 cur_to_read);
          LOG_DEBUG("Read %zd bytes at offset %" PRIu64
                    " from virtual VDI header\n",cur_to_read,
                    file_off)
        }
        if(to_read==cur_to_read) return to_read;
        else {
          // Adjust values to read from morphed image
          to_read-=cur_to_read;
          p_buf+=cur_to_read;
          file_off=0;
        }
      } else file_off-=glob_xmount.output.vdi.vdi_header_size;
      break;
    case VirtImageType_VHD:
      // When emulating VHD, make sure the while loop below only reads data
      // available in the morphed image. Any VHD footer data must be read
      // afterwards.
      if(file_off>=morphed_image_size) {
        to_read_later=to_read;
        to_read=0;
      } else if((file_off+to_read)>morphed_image_size) {
        to_read_later=(file_off+to_read)-morphed_image_size;
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
    if(glob_xmount.output.writable==TRUE &&
       glob_xmount.cache.p_cache_blkidx[cur_block].Assigned==TRUE)
    {
      // Write support enabled and need to read altered data from cachefile
      if(fseeko(glob_xmount.cache.h_cache_file,
                glob_xmount.cache.p_cache_blkidx[cur_block].off_data+block_off,
                SEEK_SET)!=0)
      {
        LOG_ERROR("Couldn't seek to offset %" PRIu64
                  " in cache file\n")
        return -EIO;
      }
      if(fread(p_buf,cur_to_read,1,glob_xmount.cache.h_cache_file)!=1) {
        LOG_ERROR("Couldn't read data from cache file!\n")
        return -EIO;
      }
      LOG_DEBUG("Read %zd bytes at offset %" PRIu64
                " from cache file\n",cur_to_read,file_off)
    } else {
      // No write support or data not cached
      ret=GetMorphedImageData(p_buf,file_off,cur_to_read,&read);
      if(ret!=TRUE || read!=cur_to_read) {
        LOG_ERROR("Couldn't read data from virtual image!\n")
        return -EIO;
      }
      LOG_DEBUG("Read %zu bytes at offset %zu from virtual image file\n",
                cur_to_read,
                file_off);
    }
    cur_block++;
    block_off=0;
    p_buf+=cur_to_read;
    to_read-=cur_to_read;
    file_off+=cur_to_read;
  }

  if(to_read_later!=0) {
    // Read virtual image type specific data following morphed image data
    switch(glob_xmount.output.VirtImageType) {
      case VirtImageType_DD:
      case VirtImageType_DMG:
      case VirtImageType_VMDK:
      case VirtImageType_VMDKS:
      case VirtImageType_VDI:
        break;
      case VirtImageType_VHD:
        // Micro$oft has choosen to use a footer rather then a header.
        if(glob_xmount.output.writable==TRUE &&
           glob_xmount.cache.p_cache_header->VhdFileHeaderCached==TRUE)
        {
          // VHD footer was already cached
          if(fseeko(glob_xmount.cache.h_cache_file,
                    glob_xmount.cache.p_cache_header->pVhdFileHeader+
                      (file_off-morphed_image_size),
                    SEEK_SET)!=0)
          {
            LOG_ERROR("Couldn't seek to cached VHD footer at offset %"
                        PRIu64 "\n",
                      glob_xmount.cache.p_cache_header->pVhdFileHeader+
                        (file_off-morphed_image_size))
            return -EIO;
          }
          if(fread(p_buf,to_read_later,1,glob_xmount.cache.h_cache_file)!=1) {
            LOG_ERROR("Couldn't read %zu bytes from cache file at offset %"
                        PRIu64 "\n",
                      to_read_later,
                      glob_xmount.cache.p_cache_header->pVhdFileHeader+
                        (file_off-morphed_image_size))
            return -EIO;
          }
          LOG_DEBUG("Read %zd bytes from cached VHD footer at offset %"
                      PRIu64 " at cache file offset %" PRIu64 "\n",
                    to_read_later,
                    (file_off-morphed_image_size),
                    glob_xmount.cache.p_cache_header->pVhdFileHeader+
                      (file_off-morphed_image_size))
        } else {
          // VHD header isn't cached
          memcpy(p_buf,
                 ((char*)glob_xmount.output.vhd.p_vhd_header)+
                   (file_off-morphed_image_size),
                 to_read_later);
          LOG_DEBUG("Read %zd bytes at offset %" PRIu64
                      " from virtual VHD header\n",
                    to_read_later,
                    (file_off-morphed_image_size))
        }
        break;
    }
  }

  return size;
}

//! Write data to virtual VDI file header
/*!
 * \param p_buf Buffer containing data to write
 * \param offset Offset of changes
 * \param size Amount of bytes to write
 * \return Number of written bytes on success or "-1" on error
 */
static int SetVdiFileHeaderData(char *p_buf,off_t offset,size_t size) {
  if(offset+size>glob_xmount.output.vdi.vdi_header_size) {
    size=glob_xmount.output.vdi.vdi_header_size-offset;
  }

  LOG_DEBUG("Need to cache %zu bytes at offset %" PRIu64
              " from VDI header\n",
            size,
            offset);

  if(glob_xmount.cache.p_cache_header->VdiFileHeaderCached==1) {
    // Header was already cached
    if(fseeko(glob_xmount.cache.h_cache_file,
              glob_xmount.cache.p_cache_header->pVdiFileHeader+offset,
              SEEK_SET)!=0)
    {
      LOG_ERROR("Couldn't seek to cached VDI header at address %" PRIu64 "\n",
                glob_xmount.cache.p_cache_header->pVdiFileHeader+offset)
      return -1;
    }
    if(fwrite(p_buf,size,1,glob_xmount.cache.h_cache_file)!=1) {
      LOG_ERROR("Couldn't write %zu bytes to cache file at offset %"
                  PRIu64 "\n",
                size,
                glob_xmount.cache.p_cache_header->pVdiFileHeader+offset)
      return -1;
    }

    LOG_DEBUG("Wrote %zd bytes at offset %" PRIu64 " to cache file\n",
              size,
              glob_xmount.cache.p_cache_header->pVdiFileHeader+offset)
  } else {
    // Header wasn't already cached.
    if(fseeko(glob_xmount.cache.h_cache_file,
              0,
              SEEK_END)!=0)
    {
      LOG_ERROR("Couldn't seek to end of cache file!")
      return -1;
    }
    glob_xmount.cache.p_cache_header->pVdiFileHeader=
      ftello(glob_xmount.cache.h_cache_file);
    LOG_DEBUG("Caching whole VDI header\n")
    if(offset>0) {
      // Changes do not begin at offset 0, need to prepend with data from
      // VDI header
      if(fwrite((char*)glob_xmount.output.vdi.p_vdi_header,
                offset,
                1,
                glob_xmount.cache.h_cache_file)!=1)
      {
        LOG_ERROR("Error while writing %" PRIu64 " bytes "
                    "to cache file at offset %" PRIu64 "!\n",
                  offset,
                  glob_xmount.cache.p_cache_header->pVdiFileHeader);
        return -1;
      }
      LOG_DEBUG("Prepended changed data with %" PRIu64
                  " bytes at cache file offset %" PRIu64 "\n",
                offset,
                glob_xmount.cache.p_cache_header->pVdiFileHeader)
    }
    // Cache changed data
    if(fwrite(p_buf,size,1,glob_xmount.cache.h_cache_file)!=1) {
      LOG_ERROR("Couldn't write %zu bytes to cache file at offset %"
                  PRIu64 "\n",size,
                glob_xmount.cache.p_cache_header->pVdiFileHeader+offset)
      return -1;
    }
    LOG_DEBUG("Wrote %zu bytes of changed data to cache file offset %"
                PRIu64 "\n",
              size,
              glob_xmount.cache.p_cache_header->pVdiFileHeader+offset)
    if(offset+size!=glob_xmount.output.vdi.vdi_header_size) {
      // Need to append data from VDI header to cache whole data struct
      if(fwrite(((char*)glob_xmount.output.vdi.p_vdi_header)+offset+size,
                glob_xmount.output.vdi.vdi_header_size-(offset+size),
                1,
                glob_xmount.cache.h_cache_file)!=1)
      {
        LOG_ERROR("Couldn't write %zu bytes to cache file at offset %"
                    PRIu64 "\n",
                  glob_xmount.output.vdi.vdi_header_size-(offset+size),
                  (uint64_t)(glob_xmount.cache.p_cache_header->pVdiFileHeader+
                    offset+size));
        return -1;
      }
      LOG_DEBUG("Appended %" PRIu32
                  " bytes to changed data at cache file offset %" PRIu64 "\n",
                glob_xmount.output.vdi.vdi_header_size-(offset+size),
                glob_xmount.cache.p_cache_header->pVdiFileHeader+offset+size)
    }
    // Mark header as cached and update header in cache file
    glob_xmount.cache.p_cache_header->VdiFileHeaderCached=1;
    if(fseeko(glob_xmount.cache.h_cache_file,0,SEEK_SET)!=0) {
      LOG_ERROR("Couldn't seek to offset 0 of cache file!\n")
      return -1;
    }
    if(fwrite((char*)glob_xmount.cache.p_cache_header,
              sizeof(ts_CacheFileHeader),
              1,
              glob_xmount.cache.h_cache_file)!=1)
    {
      LOG_ERROR("Couldn't write changed cache file header!\n")
      return -1;
    }
  }
  // All important data has been written, now flush all buffers to make
  // sure data is written to cache file
  fflush(glob_xmount.cache.h_cache_file);
#ifndef __APPLE__
  ioctl(fileno(glob_xmount.cache.h_cache_file),BLKFLSBUF,0);
#endif
  return size;
}

//! Write data to virtual VHD file footer
/*!
 * \param p_buf Buffer containing data to write
 * \param offset Offset of changes
 * \param size Amount of bytes to write
 * \return Number of written bytes on success or "-1" on error
 */
static int SetVhdFileHeaderData(char *p_buf,off_t offset,size_t size) {
  LOG_DEBUG("Need to cache %zu bytes at offset %" PRIu64
            " from VHD footer\n",size,offset)
  if(glob_xmount.cache.p_cache_header->VhdFileHeaderCached==1) {
    // Header has already been cached
    if(fseeko(glob_xmount.cache.h_cache_file,
              glob_xmount.cache.p_cache_header->pVhdFileHeader+offset,
              SEEK_SET)!=0)
    {
      LOG_ERROR("Couldn't seek to cached VHD header at address %" PRIu64 "\n",
                glob_xmount.cache.p_cache_header->pVhdFileHeader+offset);
      return -1;
    }
    if(fwrite(p_buf,size,1,glob_xmount.cache.h_cache_file)!=1) {
      LOG_ERROR("Couldn't write %zu bytes to cache file at offset %"
                  PRIu64 "\n",
                size,
                glob_xmount.cache.p_cache_header->pVhdFileHeader+offset);
      return -1;
    }
    LOG_DEBUG("Wrote %zd bytes at offset %" PRIu64 " to cache file\n",
              size,
              glob_xmount.cache.p_cache_header->pVhdFileHeader+offset);
  } else {
    // Header hasn't been cached yet.
    if(fseeko(glob_xmount.cache.h_cache_file,
              0,
              SEEK_END)!=0)
    {
      LOG_ERROR("Couldn't seek to end of cache file!")
      return -1;
    }
    glob_xmount.cache.p_cache_header->pVhdFileHeader=
      ftello(glob_xmount.cache.h_cache_file);
    LOG_DEBUG("Caching whole VHD header\n")
    if(offset>0) {
      // Changes do not begin at offset 0, need to prepend with data from
      // VHD header
      if(fwrite((char*)glob_xmount.output.vhd.p_vhd_header,
                offset,
                1,
                glob_xmount.cache.h_cache_file)!=1)
      {
        LOG_ERROR("Error while writing %" PRIu64 " bytes "
                  "to cache file at offset %" PRIu64 "!\n",
                  offset,
                  glob_xmount.cache.p_cache_header->pVhdFileHeader);
        return -1;
      }
      LOG_DEBUG("Prepended changed data with %" PRIu64
                  " bytes at cache file offset %" PRIu64 "\n",
                offset,
                glob_xmount.cache.p_cache_header->pVhdFileHeader);
    }
    // Cache changed data
    if(fwrite(p_buf,size,1,glob_xmount.cache.h_cache_file)!=1) {
      LOG_ERROR("Couldn't write %zu bytes to cache file at offset %"
                  PRIu64 "\n",
                size,
                glob_xmount.cache.p_cache_header->pVhdFileHeader+offset);
      return -1;
    }
    LOG_DEBUG("Wrote %zu bytes of changed data to cache file offset %"
                PRIu64 "\n",
              size,
              glob_xmount.cache.p_cache_header->pVhdFileHeader+offset);
    if(offset+size!=sizeof(ts_VhdFileHeader)) {
      // Need to append data from VHD header to cache whole data struct
      if(fwrite(((char*)glob_xmount.output.vhd.p_vhd_header)+offset+size,
                sizeof(ts_VhdFileHeader)-(offset+size),
                1,
                glob_xmount.cache.h_cache_file)!=1)
      {
        LOG_ERROR("Couldn't write %zu bytes to cache file at offset %"
                    PRIu64 "\n",
                  sizeof(ts_VhdFileHeader)-(offset+size),
                  (uint64_t)(glob_xmount.cache.p_cache_header->pVhdFileHeader+
                    offset+size))
        return -1;
      }
      LOG_DEBUG("Appended %" PRIu32
                  " bytes to changed data at cache file offset %" PRIu64 "\n",
                sizeof(ts_VhdFileHeader)-(offset+size),
                glob_xmount.cache.p_cache_header->pVhdFileHeader+offset+size);
    }
    // Mark header as cached and update header in cache file
    glob_xmount.cache.p_cache_header->VhdFileHeaderCached=1;
    if(fseeko(glob_xmount.cache.h_cache_file,0,SEEK_SET)!=0) {
      LOG_ERROR("Couldn't seek to offset 0 of cache file!\n")
      return -1;
    }
    if(fwrite((char*)glob_xmount.cache.p_cache_header,
              sizeof(ts_CacheFileHeader),
              1,
              glob_xmount.cache.h_cache_file)!=1)
    {
      LOG_ERROR("Couldn't write changed cache file header!\n")
      return -1;
    }
  }
  // All important data has been written, now flush all buffers to make
  // sure data is written to cache file
  fflush(glob_xmount.cache.h_cache_file);
#ifndef __APPLE__
  ioctl(fileno(glob_xmount.cache.h_cache_file),BLKFLSBUF,0);
#endif
  return size;
}

//! Write data to virtual image
/*!
 * \param p_buf Buffer containing data to write
 * \param offset Offset to start writing at
 * \param size Size of data to be written
 * \return Number of written bytes on success or "-1" on error
 */
static int SetVirtImageData(const char *p_buf, off_t offset, size_t size) {
  uint64_t cur_block=0;
  uint64_t virt_image_size;
  uint64_t orig_image_size;
  size_t to_write=0;
  size_t to_write_later=0;
  size_t to_write_now=0;
  off_t file_offset=offset;
  off_t block_offset=0;
  char *p_write_buf=(char*)p_buf;
  char *p_buf2;
  int ret;
  size_t read;

  // Get virtual image size
  if(!GetVirtImageSize(&virt_image_size)) {
    LOG_ERROR("Couldn't get virtual image size!\n")
    return -1;
  }

  if(offset>=virt_image_size) {
    LOG_ERROR("Attempt to write beyond EOF of virtual image file!\n")
    return -1;
  }

  if(offset+size>virt_image_size) {
    LOG_DEBUG("Attempt to write past EOF of virtual image file\n")
    size=virt_image_size-offset;
  }

  to_write=size;

  // Get original image size
  if(!GetMorphedImageSize(&orig_image_size)) {
    LOG_ERROR("Couldn't get morphed image size!\n")
    return -1;
  }

  // Cache virtual image type specific data preceeding original image data
  switch(glob_xmount.output.VirtImageType) {
    case VirtImageType_DD:
    case VirtImageType_DMG:
    case VirtImageType_VMDK:
    case VirtImageType_VMDKS:
      break;
    case VirtImageType_VDI:
      if(file_offset<glob_xmount.output.vdi.vdi_header_size) {
        ret=SetVdiFileHeaderData(p_write_buf,file_offset,to_write);
        if(ret==-1) {
          LOG_ERROR("Couldn't write data to virtual VDI file header!\n")
          return -1;
        }
        if(ret==to_write) return to_write;
        else {
          to_write-=ret;
          p_write_buf+=ret;
          file_offset=0;
        }
      } else file_offset-=glob_xmount.output.vdi.vdi_header_size;
      break;
    case VirtImageType_VHD:
      // When emulating VHD, make sure the while loop below only writes data
      // available in the original image. Any VHD footer data must be written
      // afterwards.
      if(file_offset>=orig_image_size) {
        to_write_later=to_write;
        to_write=0;
      } else if((file_offset+to_write)>orig_image_size) {
        to_write_later=(file_offset+to_write)-orig_image_size;
        to_write-=to_write_later;
      }
      break;
  }

  // Calculate block to write data to
  cur_block=file_offset/CACHE_BLOCK_SIZE;
  block_offset=file_offset%CACHE_BLOCK_SIZE;
  
  while(to_write!=0) {
    // Calculate how many bytes we have to write to this block
    if(block_offset+to_write>CACHE_BLOCK_SIZE) {
      to_write_now=CACHE_BLOCK_SIZE-block_offset;
    } else to_write_now=to_write;
    if(glob_xmount.cache.p_cache_blkidx[cur_block].Assigned==1) {
      // Block was already cached
      // Seek to data offset in cache file
      if(fseeko(glob_xmount.cache.h_cache_file,
             glob_xmount.cache.p_cache_blkidx[cur_block].off_data+block_offset,
             SEEK_SET)!=0)
      {
        LOG_ERROR("Couldn't seek to cached block at address %" PRIu64 "\n",
                  glob_xmount.cache.p_cache_blkidx[cur_block].off_data+
                    block_offset);
        return -1;
      }
      if(fwrite(p_write_buf,to_write_now,1,glob_xmount.cache.h_cache_file)!=1) {
        LOG_ERROR("Error while writing %zu bytes "
                  "to cache file at offset %" PRIu64 "!\n",
                  to_write_now,
                  glob_xmount.cache.p_cache_blkidx[cur_block].off_data+
                    block_offset);
        return -1;
      }
      LOG_DEBUG("Wrote %zd bytes at offset %" PRIu64
                  " to cache file\n",to_write_now,
                glob_xmount.cache.p_cache_blkidx[cur_block].off_data+
                  block_offset);
    } else {
      // Uncached block. Need to cache entire new block
      // Seek to end of cache file to append new cache block
      fseeko(glob_xmount.cache.h_cache_file,0,SEEK_END);
      glob_xmount.cache.p_cache_blkidx[cur_block].off_data=
        ftello(glob_xmount.cache.h_cache_file);
      if(block_offset!=0) {
        // Changed data does not begin at block boundry. Need to prepend
        // with data from virtual image file
        XMOUNT_MALLOC(p_buf2,char*,block_offset*sizeof(char));
        ret=GetMorphedImageData(p_buf2,
                                file_offset-block_offset,
                                block_offset,
                                &read);
        if(ret!=TRUE || read!=block_offset) {
          LOG_ERROR("Couldn't read data from morphed image!\n")
          return -1;
        }
        if(fwrite(p_buf2,block_offset,1,glob_xmount.cache.h_cache_file)!=1) {
          LOG_ERROR("Couldn't writing %" PRIu64 " bytes "
                    "to cache file at offset %" PRIu64 "!\n",
                    block_offset,
                    glob_xmount.cache.p_cache_blkidx[cur_block].off_data);
          return -1;
        }
        LOG_DEBUG("Prepended changed data with %" PRIu64
                  " bytes from virtual image file at offset %" PRIu64
                  "\n",block_offset,file_offset-block_offset)
        free(p_buf2);
      }
      if(fwrite(p_write_buf,to_write_now,1,glob_xmount.cache.h_cache_file)!=1) {
        LOG_ERROR("Error while writing %zd bytes "
                    "to cache file at offset %" PRIu64 "!\n",
                  to_write_now,
                  glob_xmount.cache.p_cache_blkidx[cur_block].off_data+
                    block_offset);
        return -1;
      }
      if(block_offset+to_write_now!=CACHE_BLOCK_SIZE) {
        // Changed data does not end at block boundry. Need to append
        // with data from virtual image file
        XMOUNT_MALLOC(p_buf2,char*,(CACHE_BLOCK_SIZE-
                                 (block_offset+to_write_now))*sizeof(char))
        memset(p_buf2,0,CACHE_BLOCK_SIZE-(block_offset+to_write_now));
        if((file_offset-block_offset)+CACHE_BLOCK_SIZE>orig_image_size) {
          // Original image is smaller than full cache block
          ret=GetMorphedImageData(p_buf2,
                                  file_offset+to_write_now,
                                  orig_image_size-(file_offset+to_write_now),
                                  &read);
          if(ret!=TRUE || read!=orig_image_size-(file_offset+to_write_now)) {
            LOG_ERROR("Couldn't read data from virtual image file!\n")
            return -1;
          }
        } else {
          ret=GetMorphedImageData(p_buf2,
                                  file_offset+to_write_now,
                                  CACHE_BLOCK_SIZE-(block_offset+to_write_now),
                                  &read);
          if(ret!=TRUE || read!=CACHE_BLOCK_SIZE-(block_offset+to_write_now)) {
            LOG_ERROR("Couldn't read data from virtual image file!\n")
            return -1;
          }
        }
        if(fwrite(p_buf2,
                  CACHE_BLOCK_SIZE-(block_offset+to_write_now),
                  1,
                  glob_xmount.cache.h_cache_file)!=1)
        {
          LOG_ERROR("Error while writing %zd bytes "
                      "to cache file at offset %" PRIu64 "!\n",
                    CACHE_BLOCK_SIZE-(block_offset+to_write_now),
                    glob_xmount.cache.p_cache_blkidx[cur_block].off_data+
                      block_offset+to_write_now);
          return -1;
        }
        free(p_buf2);
      }
      // All important data for this cache block has been written,
      // flush all buffers and mark cache block as assigned
      fflush(glob_xmount.cache.h_cache_file);
#ifndef __APPLE__
      ioctl(fileno(glob_xmount.cache.h_cache_file),BLKFLSBUF,0);
#endif
      glob_xmount.cache.p_cache_blkidx[cur_block].Assigned=1;
      // Update cache block index entry in cache file
      fseeko(glob_xmount.cache.h_cache_file,
             sizeof(ts_CacheFileHeader)+
               (cur_block*sizeof(ts_CacheFileBlockIndex)),
             SEEK_SET);
      if(fwrite(&(glob_xmount.cache.p_cache_blkidx[cur_block]),
                sizeof(ts_CacheFileBlockIndex),
                1,
                glob_xmount.cache.h_cache_file)!=1)
      {
        LOG_ERROR("Couldn't update cache file block index!\n");
        return -1;
      }
      LOG_DEBUG("Updated cache file block index: Number=%" PRIu64
                  ", Data offset=%" PRIu64 "\n",
                cur_block,
                glob_xmount.cache.p_cache_blkidx[cur_block].off_data);
    }
    // Flush buffers
    fflush(glob_xmount.cache.h_cache_file);
#ifndef __APPLE__
    ioctl(fileno(glob_xmount.cache.h_cache_file),BLKFLSBUF,0);
#endif
    block_offset=0;
    cur_block++;
    p_write_buf+=to_write_now;
    to_write-=to_write_now;
    file_offset+=to_write_now;
  }

  if(to_write_later!=0) {
    // Cache virtual image type specific data preceeding original image data
    switch(glob_xmount.output.VirtImageType) {
      case VirtImageType_DD:
      case VirtImageType_DMG:
      case VirtImageType_VMDK:
      case VirtImageType_VMDKS:
      case VirtImageType_VDI:
        break;
      case VirtImageType_VHD:
        // Micro$oft has choosen to use a footer rather then a header.
        ret=SetVhdFileHeaderData(p_write_buf,
                                 file_offset-orig_image_size,
                                 to_write_later);
        if(ret==-1) {
          LOG_ERROR("Couldn't write data to virtual VHD file footer!\n")
          return -1;
        }
        break;
    }
  }

  return size;
}

//! Calculates an MD5 hash of the first HASH_AMOUNT bytes of the input image
/*!
 * \param p_hash_low Pointer to the lower 64 bit of the hash
 * \param p_hash_high Pointer to the higher 64 bit of the hash
 * \return TRUE on success, FALSE on error
 */
static int CalculateInputImageHash(uint64_t *p_hash_low,
                                   uint64_t *p_hash_high)
{
  char hash[16];
  md5_state_t md5_state;
  char *p_buf;
  int ret;
  size_t read_data;

  XMOUNT_MALLOC(p_buf,char*,HASH_AMOUNT*sizeof(char));
  ret=GetMorphedImageData(p_buf,0,HASH_AMOUNT,&read_data);
  if(ret!=TRUE || read_data==0) {
    LOG_ERROR("Couldn't read data from morphed image file!\n")
    free(p_buf);
    return FALSE;
  }

  // Calculate MD5 hash
  md5_init(&md5_state);
  md5_append(&md5_state,(const md5_byte_t*)p_buf,read_data);
  md5_finish(&md5_state,(md5_byte_t*)hash);
  // Convert MD5 hash into two 64bit integers
  *p_hash_low=*((uint64_t*)hash);
  *p_hash_high=*((uint64_t*)(hash+8));
  free(p_buf);

  return TRUE;
}

//! Build and init virtual VDI file header
/*!
 * \return TRUE on success, FALSE on error
 */
static int InitVirtVdiHeader() {
  // See http://forums.virtualbox.org/viewtopic.php?t=8046 for a
  // "description" of the various header fields

  uint64_t image_size;
  off_t offset;
  uint32_t i,block_entries;

  // Get input image size
  if(!GetMorphedImageSize(&image_size)) {
    LOG_ERROR("Couldn't get morphed image size!\n")
    return FALSE;
  }

  // Calculate how many VDI blocks we need
  block_entries=image_size/VDI_IMAGE_BLOCK_SIZE;
  if((image_size%VDI_IMAGE_BLOCK_SIZE)!=0) block_entries++;
  glob_xmount.output.vdi.vdi_block_map_size=block_entries*sizeof(uint32_t);
  LOG_DEBUG("BlockMap: %d (%08X) entries, %d (%08X) bytes!\n",
            block_entries,
            block_entries,
            glob_xmount.output.vdi.vdi_block_map_size,
            glob_xmount.output.vdi.vdi_block_map_size)

  // Allocate memory for vdi header and block map
  glob_xmount.output.vdi.vdi_header_size=
    sizeof(ts_VdiFileHeader)+glob_xmount.output.vdi.vdi_block_map_size;
  XMOUNT_MALLOC(glob_xmount.output.vdi.p_vdi_header,
                pts_VdiFileHeader,
                glob_xmount.output.vdi.vdi_header_size);
  memset(glob_xmount.output.vdi.p_vdi_header,
         0,
         glob_xmount.output.vdi.vdi_header_size);
  glob_xmount.output.vdi.p_vdi_block_map=
    ((void*)glob_xmount.output.vdi.p_vdi_header)+sizeof(ts_VdiFileHeader);

  // Init header values
  strncpy(glob_xmount.output.vdi.p_vdi_header->szFileInfo,
          VDI_FILE_COMMENT,
          strlen(VDI_FILE_COMMENT)+1);
  glob_xmount.output.vdi.p_vdi_header->u32Signature=VDI_IMAGE_SIGNATURE;
  glob_xmount.output.vdi.p_vdi_header->u32Version=VDI_IMAGE_VERSION;
  // No idea what the following value is for! Testimage had same value
  glob_xmount.output.vdi.p_vdi_header->cbHeader=0x00000180;
  glob_xmount.output.vdi.p_vdi_header->u32Type=VDI_IMAGE_TYPE_FIXED;
  glob_xmount.output.vdi.p_vdi_header->fFlags=VDI_IMAGE_FLAGS;
  strncpy(glob_xmount.output.vdi.p_vdi_header->szComment,
          VDI_HEADER_COMMENT,
          strlen(VDI_HEADER_COMMENT)+1);
  glob_xmount.output.vdi.p_vdi_header->offData=
    glob_xmount.output.vdi.vdi_header_size;
  glob_xmount.output.vdi.p_vdi_header->offBlocks=sizeof(ts_VdiFileHeader);
  glob_xmount.output.vdi.p_vdi_header->cCylinders=0; // Legacy info
  glob_xmount.output.vdi.p_vdi_header->cHeads=0; // Legacy info
  glob_xmount.output.vdi.p_vdi_header->cSectors=0; // Legacy info
  glob_xmount.output.vdi.p_vdi_header->cbSector=512; // Legacy info
  glob_xmount.output.vdi.p_vdi_header->u32Dummy=0;
  glob_xmount.output.vdi.p_vdi_header->cbDisk=image_size;
  // Seems as VBox is always using a 1MB blocksize
  glob_xmount.output.vdi.p_vdi_header->cbBlock=VDI_IMAGE_BLOCK_SIZE;
  glob_xmount.output.vdi.p_vdi_header->cbBlockExtra=0;
  glob_xmount.output.vdi.p_vdi_header->cBlocks=block_entries;
  glob_xmount.output.vdi.p_vdi_header->cBlocksAllocated=block_entries;
  // Use partial MD5 input file hash as creation UUID and generate a random
  // modification UUID. VBox won't accept immages where create and modify UUIDS
  // aren't set.
  glob_xmount.output.vdi.p_vdi_header->uuidCreate_l=
    glob_xmount.input.image_hash_lo;
  glob_xmount.output.vdi.p_vdi_header->uuidCreate_h=
    glob_xmount.input.image_hash_hi;

#define rand64(var) {              \
  *((uint32_t*)&(var))=rand();     \
  *(((uint32_t*)&(var))+1)=rand(); \
}

  rand64(glob_xmount.output.vdi.p_vdi_header->uuidModify_l);
  rand64(glob_xmount.output.vdi.p_vdi_header->uuidModify_h);

#undef rand64

  // Generate block map
  i=0;
  for(offset=0;offset<glob_xmount.output.vdi.vdi_block_map_size;offset+=4) {
    *((uint32_t*)(glob_xmount.output.vdi.p_vdi_block_map+offset))=i;
    i++;
  }

  LOG_DEBUG("VDI header size = %u\n",glob_xmount.output.vdi.vdi_header_size);

  return TRUE;
}

//! Build and init virtual VHD file header
/*!
 * \return TRUE on success, FALSE on error
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
  if(!GetMorphedImageSize(&orig_image_size)) {
    LOG_ERROR("Couldn't get morphed image size!\n")
    return FALSE;
  }

  // Allocate memory for vhd header
  XMOUNT_MALLOC(glob_xmount.output.vhd.p_vhd_header,
                pts_VhdFileHeader,
                sizeof(ts_VhdFileHeader));
  memset(glob_xmount.output.vhd.p_vhd_header,0,sizeof(ts_VhdFileHeader));

  // Init header values
  glob_xmount.output.vhd.p_vhd_header->cookie=VHD_IMAGE_HVAL_COOKIE;
  glob_xmount.output.vhd.p_vhd_header->features=VHD_IMAGE_HVAL_FEATURES;
  glob_xmount.output.vhd.p_vhd_header->file_format_version=
    VHD_IMAGE_HVAL_FILE_FORMAT_VERSION;
  glob_xmount.output.vhd.p_vhd_header->data_offset=VHD_IMAGE_HVAL_DATA_OFFSET;
  glob_xmount.output.vhd.p_vhd_header->creation_time=
    htobe32(time(NULL)-VHD_IMAGE_TIME_CONVERSION_OFFSET);
  glob_xmount.output.vhd.p_vhd_header->creator_app=
    VHD_IMAGE_HVAL_CREATOR_APPLICATION;
  glob_xmount.output.vhd.p_vhd_header->creator_ver=
    VHD_IMAGE_HVAL_CREATOR_VERSION;
  glob_xmount.output.vhd.p_vhd_header->creator_os=
    VHD_IMAGE_HVAL_CREATOR_HOST_OS;
  glob_xmount.output.vhd.p_vhd_header->size_original=htobe64(orig_image_size);
  glob_xmount.output.vhd.p_vhd_header->size_current=
    glob_xmount.output.vhd.p_vhd_header->size_original;

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

  glob_xmount.output.vhd.p_vhd_header->disk_geometry_c=htobe16(geom_c);
  glob_xmount.output.vhd.p_vhd_header->disk_geometry_h=geom_h;
  glob_xmount.output.vhd.p_vhd_header->disk_geometry_s=geom_s;

  glob_xmount.output.vhd.p_vhd_header->disk_type=VHD_IMAGE_HVAL_DISK_TYPE;

  glob_xmount.output.vhd.p_vhd_header->uuid_l=glob_xmount.input.image_hash_lo;
  glob_xmount.output.vhd.p_vhd_header->uuid_h=glob_xmount.input.image_hash_hi;
  glob_xmount.output.vhd.p_vhd_header->saved_state=0x00;

  // Calculate footer checksum
  for(i=0;i<sizeof(ts_VhdFileHeader);i++) {
    checksum+=*((uint8_t*)(glob_xmount.output.vhd.p_vhd_header)+i);
  }
  glob_xmount.output.vhd.p_vhd_header->checksum=htobe32(~checksum);

  LOG_DEBUG("VHD header size = %u\n",sizeof(ts_VhdFileHeader));

  return TRUE;
}

//! Init the virtual VMDK file
/*!
 * \return TRUE on success, FALSE on error
 */
static int InitVirtualVmdkFile() {
  uint64_t image_size=0;
  uint64_t image_blocks=0;
  char buf[500];

  // Get original image size
  if(!GetMorphedImageSize(&image_size)) {
    LOG_ERROR("Couldn't get morphed image size!\n")
    return FALSE;
  }

  image_blocks=image_size/512;
  if(image_size%512!=0) image_blocks++;

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

  if(glob_xmount.output.VirtImageType==VirtImageType_VMDK) {
    // VMDK with IDE bus
    sprintf(buf,
            VMDK_DESC_FILE,
            image_blocks,
            (glob_xmount.output.p_virtual_image_path)+1,
            "ide");
  } else if(glob_xmount.output.VirtImageType==VirtImageType_VMDKS){
    // VMDK with SCSI bus
    sprintf(buf,
            VMDK_DESC_FILE,
            image_blocks,
            (glob_xmount.output.p_virtual_image_path)+1,
            "scsi");
  } else {
    LOG_ERROR("Unknown virtual VMDK file format!\n")
    return FALSE;
  }

#undef VMDK_DESC_FILE

  // Do not use XMOUNT_STRSET here to avoid adding '\0' to the buffer!
  XMOUNT_MALLOC(glob_xmount.output.vmdk.p_vmdk_file,char*,strlen(buf))
  strncpy(glob_xmount.output.vmdk.p_vmdk_file,buf,strlen(buf));
  glob_xmount.output.vmdk.vmdk_file_size=strlen(buf);

  return TRUE;
}

//! Create virtual image info file
/*!
 * \return TRUE on success, FALSE on error
 */
static int InitVirtImageInfoFile() {
  int ret;
  char *p_buf;

  // Start with static input header
  XMOUNT_MALLOC(glob_xmount.output.p_info_file,
                char*,
                strlen(IMAGE_INFO_INPUT_HEADER)+1);
  strncpy(glob_xmount.output.p_info_file,
          IMAGE_INFO_INPUT_HEADER,
          strlen(IMAGE_INFO_INPUT_HEADER)+1);

  // Get and add infos from input lib(s)
  for(uint64_t i=0;i<glob_xmount.input.images_count;i++) {
    ret=glob_xmount.input.pp_images[i]->p_functions->
          GetInfofileContent(glob_xmount.input.pp_images[i]->p_handle,(const char**)&p_buf);
    if(ret!=0) {
      LOG_ERROR("Unable to get info file content for image '%s': %s!\n",
                glob_xmount.input.pp_images[i]->pp_files[0],
                glob_xmount.input.pp_images[i]->p_functions->
                  GetErrorMessage(ret));
      return FALSE;
    }
    // Add infos to main buffer and free p_buf
    XMOUNT_STRAPP(glob_xmount.output.p_info_file,"\n--> ");
    XMOUNT_STRAPP(glob_xmount.output.p_info_file,
                  glob_xmount.input.pp_images[i]->pp_files[0]);
    XMOUNT_STRAPP(glob_xmount.output.p_info_file," <--\n");
    if(p_buf!=NULL) {
      XMOUNT_STRAPP(glob_xmount.output.p_info_file,p_buf);
      glob_xmount.input.pp_images[i]->p_functions->FreeBuffer(p_buf);
    } else {
      XMOUNT_STRAPP(glob_xmount.output.p_info_file,"None\n");
    }
  }

  // Add static morphing header
  XMOUNT_STRAPP(glob_xmount.output.p_info_file,IMAGE_INFO_MORPHING_HEADER);

  // Get and add infos from morphing lib
  ret=glob_xmount.morphing.p_functions->
        GetInfofileContent(glob_xmount.morphing.p_handle,(const char**)&p_buf);
  if(ret!=0) {
    LOG_ERROR("Unable to get info file content from morphing lib: %s!\n",
              glob_xmount.morphing.p_functions->GetErrorMessage(ret));
    return FALSE;
  }
  if(p_buf!=NULL) {
    XMOUNT_STRAPP(glob_xmount.output.p_info_file,p_buf);
    glob_xmount.morphing.p_functions->FreeBuffer(p_buf);
  } else {
    XMOUNT_STRAPP(glob_xmount.output.p_info_file,"None\n");
  }

  return TRUE;
}

//! Create / load cache file to enable virtual write support
/*!
 * \return TRUE on success, FALSE on error
 */
static int InitCacheFile() {
  uint64_t image_size=0;
  uint64_t blockindex_size=0;
  uint64_t cachefile_header_size=0;
  uint64_t cachefile_size=0;
  uint32_t needed_blocks=0;
  uint64_t buf;

  if(!glob_xmount.cache.overwrite_cache) {
    // Try to open an existing cache file or create a new one
    glob_xmount.cache.h_cache_file=(FILE*)FOPEN(glob_xmount.cache.p_cache_file,
                                                "rb+");
    if(glob_xmount.cache.h_cache_file==NULL) {
      // As the c lib seems to have no possibility to open a file rw wether it
      // exists or not (w+ does not work because it truncates an existing file),
      // when r+ returns NULL the file could simply not exist
      LOG_DEBUG("Cache file does not exist. Creating new one\n")
      glob_xmount.cache.h_cache_file=
        (FILE*)FOPEN(glob_xmount.cache.p_cache_file,"wb+");
      if(glob_xmount.cache.h_cache_file==NULL) {
        // There is really a problem opening the file
        LOG_ERROR("Couldn't open cache file \"%s\"!\n",
                  glob_xmount.cache.p_cache_file)
        return FALSE;
      }
    }
  } else {
    // Overwrite existing cache file or create a new one
    glob_xmount.cache.h_cache_file=(FILE*)FOPEN(glob_xmount.cache.p_cache_file,
                                                "wb+");
    if(glob_xmount.cache.h_cache_file==NULL) {
      LOG_ERROR("Couldn't open cache file \"%s\"!\n",
                glob_xmount.cache.p_cache_file)
      return FALSE;
    }
  }

  // Get input image size
  if(!GetMorphedImageSize(&image_size)) {
    LOG_ERROR("Couldn't get morphed image size!\n")
    return FALSE;
  }

  // Calculate how many blocks are needed and how big the buffers must be
  // for the actual cache file version
  needed_blocks=image_size/CACHE_BLOCK_SIZE;
  if((image_size%CACHE_BLOCK_SIZE)!=0) needed_blocks++;
  blockindex_size=needed_blocks*sizeof(ts_CacheFileBlockIndex);
  cachefile_header_size=sizeof(ts_CacheFileHeader)+blockindex_size;
  LOG_DEBUG("Cache blocks: %u (%04X) entries, %zd (%08zX) bytes\n",
            needed_blocks,
            needed_blocks,
            blockindex_size,
            blockindex_size)

  // Get cache file size
  // fseeko64 had massive problems!
  if(fseeko(glob_xmount.cache.h_cache_file,0,SEEK_END)!=0) {
    LOG_ERROR("Couldn't seek to end of cache file!\n")
    return FALSE;
  }
  // Same here, ftello64 didn't work at all and returned 0 all the times
  cachefile_size=ftello(glob_xmount.cache.h_cache_file);
  LOG_DEBUG("Cache file has %zd bytes\n",cachefile_size)

  if(cachefile_size>0) {
    // Cache file isn't empty, parse block header
    LOG_DEBUG("Cache file not empty. Parsing block header\n")
    if(fseeko(glob_xmount.cache.h_cache_file,0,SEEK_SET)!=0) {
      LOG_ERROR("Couldn't seek to beginning of cache file!\n")
      return FALSE;
    }
    // Read and check file signature
    if(fread(&buf,8,1,glob_xmount.cache.h_cache_file)!=1 ||
       buf!=CACHE_FILE_SIGNATURE)
    {
      LOG_ERROR("Not an xmount cache file or cache file corrupt!\n")
      return FALSE;
    }
    // Now get cache file version (Has only 32bit!)
    if(fread(&buf,4,1,glob_xmount.cache.h_cache_file)!=1) {
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
        if(fseeko(glob_xmount.cache.h_cache_file,0,SEEK_SET)!=0) {
          LOG_ERROR("Couldn't seek to beginning of cache file!\n")
          return FALSE;
        }
        // Alloc memory for header and block index
        XMOUNT_MALLOC(glob_xmount.cache.p_cache_header,
                      pts_CacheFileHeader,
                      cachefile_header_size);
        memset(glob_xmount.cache.p_cache_header,0,cachefile_header_size);
        // Read header and block index from file
        if(fread(glob_xmount.cache.p_cache_header,
                 cachefile_header_size,
                 1,
                 glob_xmount.cache.h_cache_file)!=1)
        {
          // Cache file isn't big enough
          LOG_ERROR("Cache file corrupt!\n")
          return FALSE;
        }
        break;
      default:
        LOG_ERROR("Unknown cache file version!\n")
        return FALSE;
    }
    // Check if cache file has same block size as we do
    if(glob_xmount.cache.p_cache_header->BlockSize!=CACHE_BLOCK_SIZE) {
      LOG_ERROR("Cache file does not use default cache block size!\n")
      return FALSE;
    }
    // Set pointer to block index
    glob_xmount.cache.p_cache_blkidx=
      (pts_CacheFileBlockIndex)((void*)glob_xmount.cache.p_cache_header+
        glob_xmount.cache.p_cache_header->pBlockIndex);
  } else {
    // New cache file, generate a new block header
    LOG_DEBUG("Cache file is empty. Generating new block header\n");
    // Alloc memory for header and block index
    XMOUNT_MALLOC(glob_xmount.cache.p_cache_header,
                  pts_CacheFileHeader,
                  cachefile_header_size)
    memset(glob_xmount.cache.p_cache_header,0,cachefile_header_size);
    glob_xmount.cache.p_cache_header->FileSignature=CACHE_FILE_SIGNATURE;
    glob_xmount.cache.p_cache_header->CacheFileVersion=CUR_CACHE_FILE_VERSION;
    glob_xmount.cache.p_cache_header->BlockSize=CACHE_BLOCK_SIZE;
    glob_xmount.cache.p_cache_header->BlockCount=needed_blocks;
    //glob_xmount.cache.p_cache_header->UsedBlocks=0;
    // The following pointer is only usuable when reading data from cache file
    glob_xmount.cache.p_cache_header->pBlockIndex=sizeof(ts_CacheFileHeader);
    glob_xmount.cache.p_cache_blkidx=
      (pts_CacheFileBlockIndex)((void*)glob_xmount.cache.p_cache_header+
        sizeof(ts_CacheFileHeader));
    glob_xmount.cache.p_cache_header->VdiFileHeaderCached=FALSE;
    glob_xmount.cache.p_cache_header->pVdiFileHeader=0;
    glob_xmount.cache.p_cache_header->VmdkFileCached=FALSE;
    glob_xmount.cache.p_cache_header->VmdkFileSize=0;
    glob_xmount.cache.p_cache_header->pVmdkFile=0;
    glob_xmount.cache.p_cache_header->VhdFileHeaderCached=FALSE;
    glob_xmount.cache.p_cache_header->pVhdFileHeader=0;
    // Write header to file
    if(fwrite(glob_xmount.cache.p_cache_header,
              cachefile_header_size,
              1,
              glob_xmount.cache.h_cache_file)!=1)
    {
      LOG_ERROR("Couldn't write cache file header to file!\n");
      return FALSE;
    }
  }
  return TRUE;
}

//! Load input / morphing libs
/*!
 * \return TRUE on success, FALSE on error
 */
static int LoadLibs() {
  DIR *p_dir=NULL;
  struct dirent *p_dirent=NULL;
  int base_library_path_len=0;
  char *p_library_path=NULL;
  void *p_libxmount=NULL;
  t_LibXmount_Input_GetApiVersion pfun_input_GetApiVersion;
  t_LibXmount_Input_GetSupportedFormats pfun_input_GetSupportedFormats;
  t_LibXmount_Input_GetFunctions pfun_input_GetFunctions;
  t_LibXmount_Morphing_GetApiVersion pfun_morphing_GetApiVersion;
  t_LibXmount_Morphing_GetSupportedTypes pfun_morphing_GetSupportedTypes;
  t_LibXmount_Morphing_GetFunctions pfun_morphing_GetFunctions;
  const char *p_supported_formats=NULL;
  const char *p_buf;
  uint32_t supported_formats_len=0;
  pts_InputLib p_input_lib=NULL;
  pts_MorphingLib p_morphing_lib=NULL;

  LOG_DEBUG("Searching for xmount libraries in '%s'.\n",
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

#define LIBXMOUNT_LOAD(path) {                            \
  p_libxmount=dlopen(path,RTLD_NOW);                      \
  if(p_libxmount==NULL) {                                 \
    LOG_ERROR("Unable to load input library '%s': %s!\n", \
              path,                                       \
              dlerror());                                 \
    continue;                                             \
  }                                                       \
}
#define LIBXMOUNT_LOAD_SYMBOL(name,pfun) {                       \
  if((pfun=dlsym(p_libxmount,name))==NULL) {                     \
    LOG_ERROR("Unable to load symbol '%s' from library '%s'!\n", \
              name,                                              \
              p_library_path);                                   \
    dlclose(p_libxmount);                                        \
    p_libxmount=NULL;                                            \
    continue;                                                    \
  }                                                              \
}

  // Loop over lib dir
  while((p_dirent=readdir(p_dir))!=NULL) {
    LOG_DEBUG("Trying to load '%s'\n",p_dirent->d_name);

    // Construct full path to found object
    p_library_path=realloc(p_library_path,
                           base_library_path_len+strlen(p_dirent->d_name)+1);
    if(p_library_path==NULL) {
      LOG_ERROR("Couldn't allocate memory!\n");
      exit(1);
    }
    strcpy(p_library_path+base_library_path_len,p_dirent->d_name);

    if(strncmp(p_dirent->d_name,"libxmount_input_",16)==0) {
      // Found possible input lib. Try to load it
      LIBXMOUNT_LOAD(p_library_path);

      // Load library symbols
      LIBXMOUNT_LOAD_SYMBOL("LibXmount_Input_GetApiVersion",
                            pfun_input_GetApiVersion);

      // Check library's API version
      if(pfun_input_GetApiVersion()!=LIBXMOUNT_INPUT_API_VERSION) {
        LOG_DEBUG("Failed! Wrong API version.\n");
        LOG_ERROR("Unable to load input library '%s'. Wrong API version\n",
                  p_library_path);
        dlclose(p_libxmount);
        continue;
      }

      LIBXMOUNT_LOAD_SYMBOL("LibXmount_Input_GetSupportedFormats",
                            pfun_input_GetSupportedFormats);
      LIBXMOUNT_LOAD_SYMBOL("LibXmount_Input_GetFunctions",
                            pfun_input_GetFunctions);

      // Construct new entry for our library list
      XMOUNT_MALLOC(p_input_lib,pts_InputLib,sizeof(ts_InputLib));
      // Initialize lib_functions structure to NULL
      memset(&(p_input_lib->lib_functions),
             0,
             sizeof(ts_LibXmountInputFunctions));

      // Set name and handle
      XMOUNT_STRSET(p_input_lib->p_name,p_dirent->d_name);
      p_input_lib->p_lib=p_libxmount;

      // Get and set supported formats
      p_supported_formats=pfun_input_GetSupportedFormats();
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

      // Get, set and check lib_functions
      pfun_input_GetFunctions(&(p_input_lib->lib_functions));
      if(p_input_lib->lib_functions.CreateHandle==NULL ||
         p_input_lib->lib_functions.DestroyHandle==NULL ||
         p_input_lib->lib_functions.Open==NULL ||
         p_input_lib->lib_functions.Close==NULL ||
         p_input_lib->lib_functions.Size==NULL ||
         p_input_lib->lib_functions.Read==NULL ||
         p_input_lib->lib_functions.OptionsHelp==NULL ||
         p_input_lib->lib_functions.OptionsParse==NULL ||
         p_input_lib->lib_functions.GetInfofileContent==NULL ||
         p_input_lib->lib_functions.GetErrorMessage==NULL ||
         p_input_lib->lib_functions.FreeBuffer==NULL)
      {
        LOG_DEBUG("Missing implemention of one or more functions in lib %s!\n",
                  p_dirent->d_name);
        free(p_input_lib->p_supported_input_types);
        free(p_input_lib->p_name);
        free(p_input_lib);
        dlclose(p_libxmount);
        continue;
      }

      // Add entry to the input library list
      XMOUNT_REALLOC(glob_xmount.input.pp_libs,
                     pts_InputLib*,
                     sizeof(pts_InputLib)*(glob_xmount.input.libs_count+1));
      glob_xmount.input.pp_libs[glob_xmount.input.libs_count++]=p_input_lib;

      LOG_DEBUG("Input library '%s' loaded successfully\n",p_dirent->d_name);
    } if(strncmp(p_dirent->d_name,"libxmount_morphing_",19)==0) {
      // Found possible morphing lib. Try to load it
      LIBXMOUNT_LOAD(p_library_path);

      // Load library symbols
      LIBXMOUNT_LOAD_SYMBOL("LibXmount_Morphing_GetApiVersion",
                            pfun_morphing_GetApiVersion);

      // Check library's API version
      if(pfun_morphing_GetApiVersion()!=LIBXMOUNT_MORPHING_API_VERSION) {
        LOG_DEBUG("Failed! Wrong API version.\n");
        LOG_ERROR("Unable to load morphing library '%s'. Wrong API version\n",
                  p_library_path);
        dlclose(p_libxmount);
        continue;
      }

      LIBXMOUNT_LOAD_SYMBOL("LibXmount_Morphing_GetSupportedTypes",
                            pfun_morphing_GetSupportedTypes);
      LIBXMOUNT_LOAD_SYMBOL("LibXmount_Morphing_GetFunctions",
                            pfun_morphing_GetFunctions);

      // Construct new entry for our library list
      XMOUNT_MALLOC(p_morphing_lib,pts_MorphingLib,sizeof(ts_MorphingLib));
      // Initialize lib_functions structure to NULL
      memset(&(p_morphing_lib->lib_functions),
             0,
             sizeof(ts_LibXmountMorphingFunctions));

      // Set name and handle
      XMOUNT_STRSET(p_morphing_lib->p_name,p_dirent->d_name);
      p_morphing_lib->p_lib=p_libxmount;

      // Get and set supported types
      p_supported_formats=pfun_morphing_GetSupportedTypes();
      supported_formats_len=0;
      p_buf=p_supported_formats;
      while(*p_buf!='\0') {
        supported_formats_len+=(strlen(p_buf)+1);
        p_buf+=(strlen(p_buf)+1);
      }
      supported_formats_len++;
      XMOUNT_MALLOC(p_morphing_lib->p_supported_morphing_types,
                    char*,
                    supported_formats_len);
      memcpy(p_morphing_lib->p_supported_morphing_types,
             p_supported_formats,
             supported_formats_len);

      // Get, set and check lib_functions
      pfun_morphing_GetFunctions(&(p_morphing_lib->lib_functions));
      if(p_morphing_lib->lib_functions.CreateHandle==NULL ||
         p_morphing_lib->lib_functions.DestroyHandle==NULL ||
         p_morphing_lib->lib_functions.Morph==NULL ||
         p_morphing_lib->lib_functions.Size==NULL ||
         p_morphing_lib->lib_functions.Read==NULL ||
         p_morphing_lib->lib_functions.OptionsHelp==NULL ||
         p_morphing_lib->lib_functions.OptionsParse==NULL ||
         p_morphing_lib->lib_functions.GetInfofileContent==NULL ||
         p_morphing_lib->lib_functions.GetErrorMessage==NULL ||
         p_morphing_lib->lib_functions.FreeBuffer==NULL)
      {
        LOG_DEBUG("Missing implemention of one or more functions in lib %s!\n",
                  p_dirent->d_name);
        free(p_morphing_lib->p_supported_morphing_types);
        free(p_morphing_lib->p_name);
        free(p_morphing_lib);
        dlclose(p_libxmount);
        continue;
      }

      // Add entry to the input library list
      XMOUNT_REALLOC(glob_xmount.morphing.pp_libs,
                     pts_MorphingLib*,
                     sizeof(pts_MorphingLib)*
                       (glob_xmount.morphing.libs_count+1));
      glob_xmount.morphing.pp_libs[glob_xmount.morphing.libs_count++]=
        p_morphing_lib;

      LOG_DEBUG("Morphing library '%s' loaded successfully\n",p_dirent->d_name);
    } else {
      LOG_DEBUG("Ignoring '%s'.\n",p_dirent->d_name);
      continue;
    }
  }

#undef LIBXMOUNT_LOAD_SYMBOL
#undef LIBXMOUNT_LOAD

  LOG_DEBUG("A total of %u input libs and %u morphing libs were loaded.\n",
            glob_xmount.input.libs_count,
            glob_xmount.morphing.libs_count);

  free(p_library_path);
  closedir(p_dir);
  return ((glob_xmount.input.libs_count>0 &&
           glob_xmount.morphing.libs_count>0) ? TRUE : FALSE);
}

//! Search an appropriate input lib for specified input type
/*!
 * \param p_input_image Input image to search input lib for
 * \return TRUE on success, FALSE on error
 */
static int FindInputLib(pts_InputImage p_input_image) {
  char *p_buf;

  LOG_DEBUG("Trying to find suitable library for input type '%s'.\n",
            p_input_image->p_type);

  // Loop over all loaded libs
  for(uint32_t i=0;i<glob_xmount.input.libs_count;i++) {
    LOG_DEBUG("Checking input library %s\n",
              glob_xmount.input.pp_libs[i]->p_name);
    p_buf=glob_xmount.input.pp_libs[i]->p_supported_input_types;
    while(*p_buf!='\0') {
      if(strcmp(p_buf,p_input_image->p_type)==0) {
        // Library supports input type, set lib functions
        LOG_DEBUG("Input library '%s' pretends to handle that input type.\n",
                  glob_xmount.input.pp_libs[i]->p_name);
        p_input_image->p_functions=
          &(glob_xmount.input.pp_libs[i]->lib_functions);
        return TRUE;
      }
      p_buf+=(strlen(p_buf)+1);
    }
  }

  LOG_DEBUG("Couldn't find any suitable library.\n");

  // No library supporting input type found
  return FALSE;
}

//! Search an appropriate morphing lib for the specified morph type
/*!
 * \return TRUE on success, FALSE on error
 */
static int FindMorphingLib() {
  char *p_buf;

  LOG_DEBUG("Trying to find suitable library for morph type '%s'.\n",
            glob_xmount.morphing.p_morph_type);

  // Loop over all loaded libs
  for(uint32_t i=0;i<glob_xmount.morphing.libs_count;i++) {
    LOG_DEBUG("Checking morphing library %s\n",
              glob_xmount.morphing.pp_libs[i]->p_name);
    p_buf=glob_xmount.morphing.pp_libs[i]->p_supported_morphing_types;
    while(*p_buf!='\0') {
      if(strcmp(p_buf,glob_xmount.morphing.p_morph_type)==0) {
        // Library supports morph type, set lib functions
        LOG_DEBUG("Morphing library '%s' pretends to handle that morph type.\n",
                  glob_xmount.morphing.pp_libs[i]->p_name);
        glob_xmount.morphing.p_functions=
          &(glob_xmount.morphing.pp_libs[i]->lib_functions);
        return TRUE;
      }
      p_buf+=(strlen(p_buf)+1);
    }
  }

  LOG_DEBUG("Couldn't find any suitable library.\n");

  // No library supporting input type found
  return FALSE;
}

static void InitResources() {
  // Input
  glob_xmount.input.libs_count=0;
  glob_xmount.input.pp_libs=NULL;
  glob_xmount.input.lib_params_count=0;
  glob_xmount.input.pp_lib_params=NULL;
  glob_xmount.input.images_count=0;
  glob_xmount.input.pp_images=NULL;
  glob_xmount.input.image_offset=0;
  glob_xmount.input.image_size_limit=0;
  glob_xmount.input.image_hash_lo=0;
  glob_xmount.input.image_hash_hi=0;

  // Morphing
  glob_xmount.morphing.libs_count=0;
  glob_xmount.morphing.pp_libs=NULL;
  glob_xmount.morphing.p_morph_type=NULL;
  glob_xmount.morphing.lib_params_count=0;
  glob_xmount.morphing.pp_lib_params=NULL;
  glob_xmount.morphing.p_handle=NULL;
  glob_xmount.morphing.p_functions=NULL;
  glob_xmount.morphing.input_image_functions.ImageCount=
    &LibXmount_Morphing_ImageCount;
  glob_xmount.morphing.input_image_functions.Size=&LibXmount_Morphing_Size;
  glob_xmount.morphing.input_image_functions.Read=&LibXmount_Morphing_Read;

  // Cache
  glob_xmount.cache.p_cache_file=NULL;
  glob_xmount.cache.h_cache_file=NULL;
  glob_xmount.cache.p_cache_header=NULL;
  glob_xmount.cache.p_cache_blkidx=NULL;
  glob_xmount.cache.overwrite_cache=FALSE;

  // Output
#ifndef __APPLE__
  glob_xmount.output.VirtImageType=VirtImageType_DD;
#else
  glob_xmount.output.VirtImageType=VirtImageType_DMG;
#endif
  glob_xmount.output.image_size=0;
  glob_xmount.output.writable=FALSE;
  glob_xmount.output.p_virtual_image_path=NULL;
  glob_xmount.output.p_info_path=NULL;
  glob_xmount.output.p_info_file=NULL;
  glob_xmount.output.vdi.vdi_header_size=0;
  glob_xmount.output.vdi.p_vdi_header=NULL;
  glob_xmount.output.vdi.vdi_block_map_size=0;
  glob_xmount.output.vdi.p_vdi_block_map=NULL;
  glob_xmount.output.vhd.p_vhd_header=NULL;
  glob_xmount.output.vmdk.p_virtual_vmdk_path=NULL;
  glob_xmount.output.vmdk.p_vmdk_file=NULL;
  glob_xmount.output.vmdk.vmdk_file_size=0;
  glob_xmount.output.vmdk.p_vmdk_lockdir1=NULL;
  glob_xmount.output.vmdk.p_vmdk_lockdir2=NULL;
  glob_xmount.output.vmdk.p_vmdk_lockfile_data=NULL;
  glob_xmount.output.vmdk.vmdk_lockfile_size=0;
  glob_xmount.output.vmdk.p_vmdk_lockfile_name=NULL;

  // Misc data
  glob_xmount.debug=FALSE;
  glob_xmount.may_set_fuse_allow_other=FALSE;
  glob_xmount.fuse_argc=0;
  glob_xmount.pp_fuse_argv=NULL;
  glob_xmount.p_mountpoint=NULL;
}

/*
 * FreeResources
 */
static void FreeResources() {
  int ret;

  LOG_DEBUG("Freeing all resources\n");

  // Misc
  if(glob_xmount.pp_fuse_argv!=NULL) {
    for(int i=0;i<glob_xmount.fuse_argc;i++) free(glob_xmount.pp_fuse_argv[i]);
    free(glob_xmount.pp_fuse_argv);
  }
  if(glob_xmount.p_mountpoint!=NULL) free(glob_xmount.p_mountpoint);

  // Output
  if(glob_xmount.output.vmdk.p_vmdk_lockfile_name!=NULL)
    free(glob_xmount.output.vmdk.p_vmdk_lockfile_name);
  if(glob_xmount.output.vmdk.p_vmdk_lockfile_data!=NULL)
    free(glob_xmount.output.vmdk.p_vmdk_lockfile_data);
  if(glob_xmount.output.vmdk.p_vmdk_lockdir2!=NULL)
    free(glob_xmount.output.vmdk.p_vmdk_lockdir2);
  if(glob_xmount.output.vmdk.p_vmdk_lockdir1!=NULL)
    free(glob_xmount.output.vmdk.p_vmdk_lockdir1);
  if(glob_xmount.output.vmdk.p_vmdk_file!=NULL)
    free(glob_xmount.output.vmdk.p_vmdk_file);
  if(glob_xmount.output.vmdk.p_virtual_vmdk_path!=NULL)
    free(glob_xmount.output.vmdk.p_virtual_vmdk_path);
  if(glob_xmount.output.p_info_path!=NULL)
    free(glob_xmount.output.p_info_path);
  if(glob_xmount.output.p_info_file!=NULL)
    free(glob_xmount.output.p_info_file);
  if(glob_xmount.output.vhd.p_vhd_header!=NULL)
    free(glob_xmount.output.vhd.p_vhd_header);
  if(glob_xmount.output.vdi.p_vdi_header!=NULL)
    free(glob_xmount.output.vdi.p_vdi_header);
  if(glob_xmount.output.p_virtual_image_path!=NULL)
    free(glob_xmount.output.p_virtual_image_path);

  // Cache
  if(glob_xmount.cache.h_cache_file!=NULL)
    fclose(glob_xmount.cache.h_cache_file);
  if(glob_xmount.cache.p_cache_header!=NULL)
    free(glob_xmount.cache.p_cache_header);
  // glob_xmount.cache.p_cache_blkidx is freed by the above call
  if(glob_xmount.cache.p_cache_file!=NULL)
    free(glob_xmount.cache.p_cache_file);

  // Morphing
  if(glob_xmount.morphing.p_functions!=NULL) {
    if(glob_xmount.morphing.p_handle!=NULL) {
      // Destroy morphing handle
      ret=glob_xmount.morphing.p_functions->
            DestroyHandle(&(glob_xmount.morphing.p_handle));
      if(ret!=0) {
        LOG_ERROR("Unable to destroy morphing handle: %s!\n",
                  glob_xmount.morphing.p_functions->GetErrorMessage(ret));
      }
    }
  }
  if(glob_xmount.morphing.pp_lib_params!=NULL) {
    for(uint32_t i=0;i<glob_xmount.morphing.lib_params_count;i++)
      free(glob_xmount.morphing.pp_lib_params[i]);
    free(glob_xmount.morphing.pp_lib_params);
  }
  if(glob_xmount.morphing.p_morph_type!=NULL)
    free(glob_xmount.morphing.p_morph_type);
  if(glob_xmount.morphing.pp_libs!=NULL) {
    // Unload morphing libs
    for(uint32_t i=0;i<glob_xmount.morphing.libs_count;i++) {
      if(glob_xmount.morphing.pp_libs[i]==NULL) continue;
      if(glob_xmount.morphing.pp_libs[i]->p_supported_morphing_types!=NULL)
        free(glob_xmount.morphing.pp_libs[i]->p_supported_morphing_types);
      if(glob_xmount.morphing.pp_libs[i]->p_lib!=NULL)
        dlclose(glob_xmount.morphing.pp_libs[i]->p_lib);
      if(glob_xmount.morphing.pp_libs[i]->p_name!=NULL)
        free(glob_xmount.morphing.pp_libs[i]->p_name);
      free(glob_xmount.morphing.pp_libs[i]);
    }
    free(glob_xmount.morphing.pp_libs);
  }

  // Input
  if(glob_xmount.input.pp_images!=NULL) {
    // Close all input images
    for(uint64_t i=0;i<glob_xmount.input.images_count;i++) {
      if(glob_xmount.input.pp_images[i]==NULL) continue;
      if(glob_xmount.input.pp_images[i]->p_functions!=NULL) {
        if(glob_xmount.input.pp_images[i]->p_handle!=NULL) {
          ret=glob_xmount.input.pp_images[i]->p_functions->
                Close(glob_xmount.input.pp_images[i]->p_handle);
          if(ret!=0) {
            LOG_ERROR("Unable to close input image: %s\n",
                      glob_xmount.input.pp_images[i]->p_functions->
                        GetErrorMessage(ret));
          }
          ret=glob_xmount.input.pp_images[i]->p_functions->
                DestroyHandle(&(glob_xmount.input.pp_images[i]->p_handle));
          if(ret!=0) {
            LOG_ERROR("Unable to destroy input image handle: %s\n",
                      glob_xmount.input.pp_images[i]->p_functions->
                        GetErrorMessage(ret));
          }
        }
      }
      if(glob_xmount.input.pp_images[i]->pp_files!=NULL) {
        for(uint64_t ii=0;ii<glob_xmount.input.pp_images[i]->files_count;ii++) {
          if(glob_xmount.input.pp_images[i]->pp_files[ii]!=NULL)
            free(glob_xmount.input.pp_images[i]->pp_files[ii]);
        }
        free(glob_xmount.input.pp_images[i]->pp_files);
      }
      if(glob_xmount.input.pp_images[i]->p_type!=NULL)
        free(glob_xmount.input.pp_images[i]->p_type);
      free(glob_xmount.input.pp_images[i]);
    }
    free(glob_xmount.input.pp_images);
  }
  if(glob_xmount.input.pp_lib_params!=NULL) {
    for(uint32_t i=0;i<glob_xmount.input.lib_params_count;i++)
      free(glob_xmount.input.pp_lib_params[i]);
    free(glob_xmount.input.pp_lib_params);
  }
  if(glob_xmount.input.pp_libs!=NULL) {
    // Unload all input libs
    for(uint32_t i=0;i<glob_xmount.input.libs_count;i++) {
      if(glob_xmount.input.pp_libs[i]->p_supported_input_types!=NULL)
        free(glob_xmount.input.pp_libs[i]->p_supported_input_types);
      if(glob_xmount.input.pp_libs[i]->p_lib!=NULL)
        dlclose(glob_xmount.input.pp_libs[i]->p_lib);
      if(glob_xmount.input.pp_libs[i]->p_name!=NULL)
        free(glob_xmount.input.pp_libs[i]->p_name);
      free(glob_xmount.input.pp_libs[i]);
    }
    free(glob_xmount.input.pp_libs);
  }

  // Before we return, initialize everything in case ReleaseResources would be
  // called again.
  InitResources();
}

//! Function to split given library options
static int SplitLibraryParameters(char *p_params,
                                  uint32_t *p_ret_opts_count,
                                  pts_LibXmountOptions **ppp_ret_opt)
{
  pts_LibXmountOptions p_opts=NULL;
  pts_LibXmountOptions *pp_opts=NULL;
  uint32_t params_len;
  uint32_t opts_count=0;
  uint32_t sep_pos=0;
  char *p_buf=p_params;

  if(p_params==NULL) return FALSE;

  // Get params length
  params_len=strlen(p_params);

  // Return if no params specified
  if(params_len==0) {
    *ppp_ret_opt=NULL;
    p_ret_opts_count=0;
    return TRUE;
  }

  // Split params
  while(*p_buf!='\0') {
    XMOUNT_MALLOC(p_opts,pts_LibXmountOptions,sizeof(ts_LibXmountOptions));
    p_opts->valid=0;

#define FREE_PP_OPTS() {                                 \
  if(pp_opts!=NULL) {                                    \
    for(uint32_t i=0;i<opts_count;i++) free(pp_opts[i]); \
    free(pp_opts);                                       \
  }                                                      \
}

    // Search next assignment operator
    sep_pos=0;
    while(p_buf[sep_pos]!='\0' &&  p_buf[sep_pos]!='=') sep_pos++;
    if(sep_pos==0 || p_buf[sep_pos]=='\0') {
      LOG_ERROR("Library parameter '%s' is missing an assignment operator!\n",
                p_buf);
      free(p_opts);
      FREE_PP_OPTS();
      return FALSE;
    }

    // Save option key
    XMOUNT_STRNSET(p_opts->p_key,p_buf,sep_pos);
    p_buf+=(sep_pos+1);

    // Search next separator
    sep_pos=0;
    while(p_buf[sep_pos]!='\0' &&  p_buf[sep_pos]!=',') sep_pos++;
    if(sep_pos==0) {
      LOG_ERROR("Library parameter '%s' is not of format key=value!\n",
                p_opts->p_key);
      free(p_opts->p_key);
      free(p_opts);
      FREE_PP_OPTS();
      return FALSE;
    }

    // Save option value
    XMOUNT_STRNSET(p_opts->p_value,p_buf,sep_pos);
    p_buf+=sep_pos;

    LOG_DEBUG("Extracted library option: '%s' = '%s'\n",
              p_opts->p_key,
              p_opts->p_value);

#undef FREE_PP_OPTS

    // Add current option to return array
    XMOUNT_REALLOC(pp_opts,
                   pts_LibXmountOptions*,
                   sizeof(pts_LibXmountOptions)*(opts_count+1));
    pp_opts[opts_count++]=p_opts;

    // If we're not at the end of p_params, skip over separator for next run
    if(*p_buf!='\0') p_buf++;
  }

  LOG_DEBUG("Extracted a total of %" PRIu32 " library options\n",opts_count);

  *p_ret_opts_count=opts_count;
  *ppp_ret_opt=pp_opts;
  return TRUE;
}

/*******************************************************************************
 * LibXmount_Morphing function implementation
 ******************************************************************************/
//! Function to get the amount of input images
/*!
 * \param p_count Count of input images
 * \return 0 on success
 */
static int LibXmount_Morphing_ImageCount(uint64_t *p_count) {
  *p_count=glob_xmount.input.images_count;
  return 0;
}

//! Function to get the size of the morphed data
/*!
 * \param image Image number
 * \param p_size Pointer to store input image's size to
 * \return 0 on success
 */
static int LibXmount_Morphing_Size(uint64_t image, uint64_t *p_size) {
  if(image>=glob_xmount.input.images_count) return -1;
  *p_size=glob_xmount.input.pp_images[image]->size;
  return 0;
}

//! Function to read data from input image
/*!
 * \param image Image number
 * \param p_buf Buffer to store read data to
 * \param offset Position at which to start reading
 * \param count Amount of bytes to read
 * \param p_read Number of read bytes on success
 * \return 0 on success or negated error code on error
 */
static int LibXmount_Morphing_Read(uint64_t image,
                            char *p_buf,
                            off_t offset,
                            size_t count,
                            size_t *p_read)
{
  if(image>=glob_xmount.input.images_count) return -EIO;
  return GetInputImageData(glob_xmount.input.pp_images[image],
                           p_buf,
                           offset,
                           count,
                           p_read);
}

/*******************************************************************************
 * FUSE function implementation
 ******************************************************************************/
//! FUSE access implementation
/*!
 * \param p_path Path of file to get attributes from
 * \param perm Requested permissisons
 * \return 0 on success, negated error code on error
 */
/*
static int FuseAccess(const char *path, int perm) {
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

//! FUSE getattr implementation
/*!
 * \param p_path Path of file to get attributes from
 * \param p_stat Pointer to stat structure to save attributes to
 * \return 0 on success, negated error code on error
 */
static int FuseGetAttr(const char *p_path, struct stat *p_stat) {
  memset(p_stat,0,sizeof(struct stat));
  if(strcmp(p_path,"/")==0) {
    // Attributes of mountpoint
    p_stat->st_mode=S_IFDIR | 0777;
    p_stat->st_nlink=2;
  } else if(strcmp(p_path,glob_xmount.output.p_virtual_image_path)==0) {
    // Attributes of virtual image
    if(!glob_xmount.output.writable) p_stat->st_mode=S_IFREG | 0444;
    else p_stat->st_mode=S_IFREG | 0666;
    p_stat->st_nlink=1;
    // Get virtual image file size
    if(!GetVirtImageSize((uint64_t*)&(p_stat->st_size))) {
      LOG_ERROR("Couldn't get image size!\n");
      return -ENOENT;
    }
    if(glob_xmount.output.VirtImageType==VirtImageType_VHD) {
      // Make sure virtual image seems to be fully allocated (not sparse file).
      // Without this, Windows won't attach the vhd file!
      p_stat->st_blocks=p_stat->st_size/512;
      if(p_stat->st_size%512!=0) p_stat->st_blocks++;
    }
  } else if(strcmp(p_path,glob_xmount.output.p_info_path)==0) {
    // Attributes of virtual image info file
    p_stat->st_mode=S_IFREG | 0444;
    p_stat->st_nlink=1;
    // Get virtual image info file size
    if(glob_xmount.output.p_info_file!=NULL) {
      p_stat->st_size=strlen(glob_xmount.output.p_info_file);
    } else p_stat->st_size=0;
  } else if(glob_xmount.output.VirtImageType==VirtImageType_VMDK ||
            glob_xmount.output.VirtImageType==VirtImageType_VMDKS)
  {
    // Some special files only present when emulating VMDK files
    if(strcmp(p_path,glob_xmount.output.vmdk.p_virtual_vmdk_path)==0) {
      // Attributes of virtual vmdk file
      if(!glob_xmount.output.writable) p_stat->st_mode=S_IFREG | 0444;
      else p_stat->st_mode=S_IFREG | 0666;
      p_stat->st_nlink=1;
      // Get virtual image info file size
      if(glob_xmount.output.vmdk.p_vmdk_file!=NULL) {
        p_stat->st_size=glob_xmount.output.vmdk.vmdk_file_size;
      } else p_stat->st_size=0;
    } else if(glob_xmount.output.vmdk.p_vmdk_lockdir1!=NULL &&
              strcmp(p_path,glob_xmount.output.vmdk.p_vmdk_lockdir1)==0)
    {
      p_stat->st_mode=S_IFDIR | 0777;
      p_stat->st_nlink=2;
    } else if(glob_xmount.output.vmdk.p_vmdk_lockdir2!=NULL &&
              strcmp(p_path,glob_xmount.output.vmdk.p_vmdk_lockdir2)==0)
    {
      p_stat->st_mode=S_IFDIR | 0777;
      p_stat->st_nlink=2;
    } else if(glob_xmount.output.vmdk.p_vmdk_lockfile_name!=NULL &&
              strcmp(p_path,glob_xmount.output.vmdk.p_vmdk_lockfile_name)==0)
    {
      p_stat->st_mode=S_IFREG | 0666;
      if(glob_xmount.output.vmdk.p_vmdk_lockfile_name!=NULL) {
        p_stat->st_size=strlen(glob_xmount.output.vmdk.p_vmdk_lockfile_name);
      } else p_stat->st_size=0;
    } else return -ENOENT;
  } else return -ENOENT;
  // Set uid and gid of all files to uid and gid of current process
  p_stat->st_uid=getuid();
  p_stat->st_gid=getgid();
  return 0;
}

//! FUSE mkdir implementation
/*!
 * \param p_path Directory path
 * \param mode Directory permissions
 * \return 0 on success, negated error code on error
 */
static int FuseMkDir(const char *p_path, mode_t mode) {
  // Only allow creation of VMWare's lock directories
  if(glob_xmount.output.VirtImageType==VirtImageType_VMDK ||
     glob_xmount.output.VirtImageType==VirtImageType_VMDKS)
  {
    if(glob_xmount.output.vmdk.p_vmdk_lockdir1==NULL)  {
      char aVmdkLockDir[strlen(glob_xmount.output.vmdk.p_virtual_vmdk_path)+5];
      sprintf(aVmdkLockDir,
              "%s.lck",
              glob_xmount.output.vmdk.p_virtual_vmdk_path);
      if(strcmp(p_path,aVmdkLockDir)==0) {
        LOG_DEBUG("Creating virtual directory \"%s\"\n",aVmdkLockDir)
        XMOUNT_STRSET(glob_xmount.output.vmdk.p_vmdk_lockdir1,aVmdkLockDir)
        return 0;
      } else {
        LOG_ERROR("Attempt to create illegal directory \"%s\"!\n",p_path)
        LOG_DEBUG("Supposed: %s\n",aVmdkLockDir)
        return -1;
      }
    } else if(glob_xmount.output.vmdk.p_vmdk_lockdir2==NULL &&
              strncmp(p_path,
                      glob_xmount.output.vmdk.p_vmdk_lockdir1,
                      strlen(glob_xmount.output.vmdk.p_vmdk_lockdir1))==0)
    {
      LOG_DEBUG("Creating virtual directory \"%s\"\n",p_path)
      XMOUNT_STRSET(glob_xmount.output.vmdk.p_vmdk_lockdir2,p_path)
      return 0;
    } else {
      LOG_ERROR("Attempt to create illegal directory \"%s\"!\n",p_path)
      LOG_DEBUG("Compared to first %u chars of \"%s\"\n",
                strlen(glob_xmount.output.vmdk.p_vmdk_lockdir1),
                glob_xmount.output.vmdk.p_vmdk_lockdir1)
      return -1;
    }
  }
  LOG_ERROR("Attempt to create directory \"%s\" "
            "on read-only filesystem!\n",p_path)
  return -1;
}

//! FUSE create implementation.
/*!
 * Currently only allows to create VMWare's lock file
 *
 * \param p_path File to create
 * \param mode File mode
 * \param dev ??? but not used
 * \return 0 on success, negated error code on error
 */
static int FuseMkNod(const char *p_path, mode_t mode, dev_t dev) {
  if((glob_xmount.output.VirtImageType==VirtImageType_VMDK ||
      glob_xmount.output.VirtImageType==VirtImageType_VMDKS) &&
     glob_xmount.output.vmdk.p_vmdk_lockdir1!=NULL &&
     glob_xmount.output.vmdk.p_vmdk_lockfile_name==NULL)
  {
    LOG_DEBUG("Creating virtual file \"%s\"\n",p_path)
    XMOUNT_STRSET(glob_xmount.output.vmdk.p_vmdk_lockfile_name,p_path);
    return 0;
  } else {
    LOG_ERROR("Attempt to create illegal file \"%s\"\n",p_path)
    return -1;
  }
}

//! FUSE readdir implementation
/*!
 * \param p_path Path from where files should be listed
 * \param p_buf Buffer to write file entrys to
 * \param filler Function to write dir entrys to buffer
 * \param offset ??? but not used
 * \param p_fi File info struct
 * \return 0 on success, negated error code on error
 */
static int FuseReadDir(const char *p_path,
                       void *p_buf,
                       fuse_fill_dir_t filler,
                       off_t offset,
                       struct fuse_file_info *p_fi)
{
  // Ignore some params
  (void)offset;
  (void)p_fi;

  if(strcmp(p_path,"/")==0) {
    // Add std . and .. entrys
    filler(p_buf,".",NULL,0);
    filler(p_buf,"..",NULL,0);
    // Add our virtual files (p+1 to ignore starting "/")
    filler(p_buf,glob_xmount.output.p_virtual_image_path+1,NULL,0);
    filler(p_buf,glob_xmount.output.p_info_path+1,NULL,0);
    if(glob_xmount.output.VirtImageType==VirtImageType_VMDK ||
       glob_xmount.output.VirtImageType==VirtImageType_VMDKS)
    {
      // For VMDK's, we use an additional descriptor file
      filler(p_buf,glob_xmount.output.vmdk.p_virtual_vmdk_path+1,NULL,0);
      // And there could also be a lock directory
      if(glob_xmount.output.vmdk.p_vmdk_lockdir1!=NULL) {
        filler(p_buf,glob_xmount.output.vmdk.p_vmdk_lockdir1+1,NULL,0);
      }
    }
  } else if(glob_xmount.output.VirtImageType==VirtImageType_VMDK ||
            glob_xmount.output.VirtImageType==VirtImageType_VMDKS)
  {
    // For VMDK emulation, there could be a lock directory
    if(glob_xmount.output.vmdk.p_vmdk_lockdir1!=NULL &&
       strcmp(p_path,glob_xmount.output.vmdk.p_vmdk_lockdir1)==0)
    {
      filler(p_buf,".",NULL,0);
      filler(p_buf,"..",NULL,0);
      if(glob_xmount.output.vmdk.p_vmdk_lockfile_name!=NULL) {
        filler(p_buf,
               glob_xmount.output.vmdk.p_vmdk_lockfile_name+
                 strlen(glob_xmount.output.vmdk.p_vmdk_lockdir1)+1,
               NULL,
               0);
      }
    } else if(glob_xmount.output.vmdk.p_vmdk_lockdir2!=NULL &&
              strcmp(p_path,glob_xmount.output.vmdk.p_vmdk_lockdir2)==0)
    {
      filler(p_buf,".",NULL,0);
      filler(p_buf,"..",NULL,0);
    } else return -ENOENT;
  } else return -ENOENT;

  return 0;
}

//! FUSE open implementation
/*!
 * \param p_path Path to file to open
 * \param p_fi File info struct
 * \return 0 on success, negated error code on error
 */
static int FuseOpen(const char *p_path, struct fuse_file_info *p_fi) {

#define CHECK_OPEN_PERMS() {                                              \
  if(!glob_xmount.output.writable && (p_fi->flags & 3)!=O_RDONLY) {       \
    LOG_DEBUG("Attempt to open the read-only file \"%s\" for writing.\n", \
              p_path)                                                     \
    return -EACCES;                                                       \
  }                                                                       \
  return 0;                                                               \
}

  if(strcmp(p_path,glob_xmount.output.p_virtual_image_path)==0 ||
     strcmp(p_path,glob_xmount.output.p_info_path)==0)
  {
    CHECK_OPEN_PERMS();
  } else if(glob_xmount.output.VirtImageType==VirtImageType_VMDK ||
            glob_xmount.output.VirtImageType==VirtImageType_VMDKS)
  {
    if(strcmp(p_path,glob_xmount.output.vmdk.p_virtual_vmdk_path)==0 ||
         (glob_xmount.output.vmdk.p_vmdk_lockfile_name!=NULL &&
            strcmp(p_path,glob_xmount.output.vmdk.p_vmdk_lockfile_name)==0))
    {
      CHECK_OPEN_PERMS();
    }
  }

#undef CHECK_OPEN_PERMS

  LOG_DEBUG("Attempt to open inexistant file \"%s\".\n",p_path);
  return -ENOENT;
}

//! FUSE read implementation
/*!
 * \param p_path Path (relative to mount folder) of file to read data from
 * \param p_buf Pre-allocated buffer where read data should be written to
 * \param size Number of bytes to read
 * \param offset Offset to start reading at
 * \param p_fi: File info struct
 * \return Read bytes on success, negated error code on error
 */
static int FuseRead(const char *p_path,
                    char *p_buf,
                    size_t size,
                    off_t offset,
                    struct fuse_file_info *p_fi)
{
  (void)p_fi;

  int ret;
  uint64_t len;

#define READ_MEM_FILE(filebuf,filesize,filetypestr,mutex) {                    \
  len=filesize;                                                                \
  if(offset<len) {                                                             \
    if(offset+size>len) {                                                      \
      LOG_DEBUG("Attempt to read past EOF of virtual " filetypestr " file\n"); \
      LOG_DEBUG("Adjusting read size from %u to %u\n",size,len-offset);        \
      size=len-offset;                                                         \
    }                                                                          \
    pthread_mutex_lock(&mutex);                                                \
    memcpy(p_buf,filebuf+offset,size);                                         \
    pthread_mutex_unlock(&mutex);                                              \
    LOG_DEBUG("Read %" PRIu64 " bytes at offset %" PRIu64                      \
              " from virtual " filetypestr " file\n",size,offset);             \
    ret=size;                                                                  \
  } else {                                                                     \
    LOG_DEBUG("Attempt to read behind EOF of virtual " filetypestr " file\n"); \
    ret=0;                                                                     \
  }                                                                            \
}

  if(strcmp(p_path,glob_xmount.output.p_virtual_image_path)==0) {
    // Read data from virtual output file
    // Wait for other threads to end reading/writing data
    pthread_mutex_lock(&(glob_xmount.mutex_image_rw));
    // Get requested data
    if((ret=GetVirtImageData(p_buf,offset,size))<0) {
      LOG_ERROR("Couldn't read data from virtual image file!\n")
    }
    // Allow other threads to read/write data again
    pthread_mutex_unlock(&(glob_xmount.mutex_image_rw));
  } else if(strcmp(p_path,glob_xmount.output.p_info_path)==0) {
    // Read data from virtual info file
    READ_MEM_FILE(glob_xmount.output.p_info_file,
                  strlen(glob_xmount.output.p_info_file),
                  "info",
                  glob_xmount.mutex_info_read);
  } else if(strcmp(p_path,glob_xmount.output.vmdk.p_virtual_vmdk_path)==0) {
    // Read data from virtual vmdk file
    READ_MEM_FILE(glob_xmount.output.vmdk.p_vmdk_file,
                  glob_xmount.output.vmdk.vmdk_file_size,
                  "vmdk",
                  glob_xmount.mutex_image_rw);
  } else if(glob_xmount.output.vmdk.p_vmdk_lockfile_name!=NULL &&
            strcmp(p_path,glob_xmount.output.vmdk.p_vmdk_lockfile_name)==0)
  {
    // Read data from virtual lock file
    READ_MEM_FILE(glob_xmount.output.vmdk.p_vmdk_lockfile_data,
                  glob_xmount.output.vmdk.vmdk_lockfile_size,
                  "vmdk lock",
                  glob_xmount.mutex_image_rw);
  } else {
    // Attempt to read non existant file
    LOG_DEBUG("Attempt to read from non existant file \"%s\"\n",p_path)
    ret=-ENOENT;
  }

#undef READ_MEM_FILE

  // TODO: Return size of read data!!!!!
  return ret;
}

//! FUSE rename implementation
/*!
 * \param p_path File to rename
 * \param p_npath New filename
 * \return 0 on error, negated error code on error
 */
static int FuseRename(const char *p_path, const char *p_npath) {
  if(glob_xmount.output.VirtImageType==VirtImageType_VMDK ||
     glob_xmount.output.VirtImageType==VirtImageType_VMDKS)
  {
    if(glob_xmount.output.vmdk.p_vmdk_lockfile_name!=NULL &&
       strcmp(p_path,glob_xmount.output.vmdk.p_vmdk_lockfile_name)==0)
    {
      LOG_DEBUG("Renaming virtual lock file from \"%s\" to \"%s\"\n",
                glob_xmount.output.vmdk.p_vmdk_lockfile_name,
                p_npath)
      XMOUNT_REALLOC(glob_xmount.output.vmdk.p_vmdk_lockfile_name,char*,
                     (strlen(p_npath)+1)*sizeof(char));
      strcpy(glob_xmount.output.vmdk.p_vmdk_lockfile_name,p_npath);
      return 0;
    }
  }
  return -ENOENT;
}

//! FUSE rmdir implementation
/*!
 * \param p_path Directory to delete
 * \return 0 on success, negated error code on error
 */
static int FuseRmDir(const char *p_path) {
  // Only VMWare's lock directories can be deleted
  if(glob_xmount.output.VirtImageType==VirtImageType_VMDK ||
     glob_xmount.output.VirtImageType==VirtImageType_VMDKS)
  {
    if(glob_xmount.output.vmdk.p_vmdk_lockdir1!=NULL &&
       strcmp(p_path,glob_xmount.output.vmdk.p_vmdk_lockdir1)==0)
    {
      LOG_DEBUG("Deleting virtual lock dir \"%s\"\n",
                glob_xmount.output.vmdk.p_vmdk_lockdir1)
      free(glob_xmount.output.vmdk.p_vmdk_lockdir1);
      glob_xmount.output.vmdk.p_vmdk_lockdir1=NULL;
      return 0;
    } else if(glob_xmount.output.vmdk.p_vmdk_lockdir2!=NULL &&
              strcmp(p_path,glob_xmount.output.vmdk.p_vmdk_lockdir2)==0)
    {
      LOG_DEBUG("Deleting virtual lock dir \"%s\"\n",
                glob_xmount.output.vmdk.p_vmdk_lockdir1)
      free(glob_xmount.output.vmdk.p_vmdk_lockdir2);
      glob_xmount.output.vmdk.p_vmdk_lockdir2=NULL;
      return 0;
    }
  }
  return -1;
}

//! FUSE unlink implementation
/*!
 * \param p_path File to delete
 * \return 0 on success, negated error code on error
 */
static int FuseUnlink(const char *p_path) {
  // Only VMWare's lock file can be deleted
  if(glob_xmount.output.VirtImageType==VirtImageType_VMDK ||
     glob_xmount.output.VirtImageType==VirtImageType_VMDKS)
  {
    if(glob_xmount.output.vmdk.p_vmdk_lockfile_name!=NULL &&
       strcmp(p_path,glob_xmount.output.vmdk.p_vmdk_lockfile_name)==0)
    {
      LOG_DEBUG("Deleting virtual file \"%s\"\n",
                glob_xmount.output.vmdk.p_vmdk_lockfile_name)
      free(glob_xmount.output.vmdk.p_vmdk_lockfile_name);
      free(glob_xmount.output.vmdk.p_vmdk_lockfile_data);
      glob_xmount.output.vmdk.p_vmdk_lockfile_name=NULL;
      glob_xmount.output.vmdk.p_vmdk_lockfile_data=NULL;
      glob_xmount.output.vmdk.vmdk_lockfile_size=0;
      return 0;
    }
  }
  return -1;
}

//! FUSE statfs implementation
/*!
 * \param p_path Get stats for fs that the specified file resides in
 * \param stats Stats
 * \return 0 on success, negated error code on error
 */
/*
static int FuseStatFs(const char *p_path, struct statvfs *stats) {
  struct statvfs CacheFileFsStats;
  int ret;

  if(glob_xmount.writable==TRUE) {
    // If write support is enabled, return stats of fs upon which cache file
    // resides in
    if((ret=statvfs(glob_xmount.p_cache_file,&CacheFileFsStats))==0) {
      memcpy(stats,&CacheFileFsStats,sizeof(struct statvfs));
      return 0;
    } else {
      LOG_ERROR("Couldn't get stats for fs upon which resides \"%s\"\n",
                glob_xmount.p_cache_file)
      return ret;
    }
  } else {
    // TODO: Return read only
    return 0;
  }
}
*/

// FUSE write implementation
/*!
 * \param p_buf Buffer containing data to write
 * \param size Number of bytes to write
 * \param offset Offset to start writing at
 * \param p_fi: File info struct
 *
 * Returns:
 *   Written bytes on success, negated error code on error
 */
static int FuseWrite(const char *p_path,
                     const char *p_buf,
                     size_t size,
                     off_t offset,
                     struct fuse_file_info *p_fi)
{
  (void)p_fi;

  uint64_t len;

  if(strcmp(p_path,glob_xmount.output.p_virtual_image_path)==0) {
    // Wait for other threads to end reading/writing data
    pthread_mutex_lock(&(glob_xmount.mutex_image_rw));

    // Get virtual image file size
    if(!GetVirtImageSize(&len)) {
      LOG_ERROR("Couldn't get virtual image size!\n")
      pthread_mutex_unlock(&(glob_xmount.mutex_image_rw));
      return 0;
    }
    if(offset<len) {
      if(offset+size>len) size=len-offset;
      if(SetVirtImageData(p_buf,offset,size)!=size) {
        LOG_ERROR("Couldn't write data to virtual image file!\n")
        pthread_mutex_unlock(&(glob_xmount.mutex_image_rw));
        return 0;
      }
    } else {
      LOG_DEBUG("Attempt to write past EOF of virtual image file\n")
      pthread_mutex_unlock(&(glob_xmount.mutex_image_rw));
      return 0;
    }

    // Allow other threads to read/write data again
    pthread_mutex_unlock(&(glob_xmount.mutex_image_rw));
  } else if(strcmp(p_path,glob_xmount.output.vmdk.p_virtual_vmdk_path)==0) {
    pthread_mutex_lock(&(glob_xmount.mutex_image_rw));
    len=glob_xmount.output.vmdk.vmdk_file_size;
    if((offset+size)>len) {
      // Enlarge or create buffer if needed
      if(len==0) {
        len=offset+size;
        XMOUNT_MALLOC(glob_xmount.output.vmdk.p_vmdk_file,
                      char*,
                      len*sizeof(char));
      } else {
        len=offset+size;
        XMOUNT_REALLOC(glob_xmount.output.vmdk.p_vmdk_file,
                       char*,
                       len*sizeof(char));
      }
      glob_xmount.output.vmdk.vmdk_file_size=offset+size;
    }
    // Copy data to buffer
    memcpy(glob_xmount.output.vmdk.p_vmdk_file+offset,p_buf,size);
    pthread_mutex_unlock(&(glob_xmount.mutex_image_rw));
  } else if(glob_xmount.output.vmdk.p_vmdk_lockfile_name!=NULL &&
            strcmp(p_path,glob_xmount.output.vmdk.p_vmdk_lockfile_name)==0)
  {
    pthread_mutex_lock(&(glob_xmount.mutex_image_rw));
    if((offset+size)>glob_xmount.output.vmdk.vmdk_lockfile_size) {
      // Enlarge or create buffer if needed
      if(glob_xmount.output.vmdk.vmdk_lockfile_size==0) {
        glob_xmount.output.vmdk.vmdk_lockfile_size=offset+size;
        XMOUNT_MALLOC(glob_xmount.output.vmdk.p_vmdk_lockfile_data,char*,
                      glob_xmount.output.vmdk.vmdk_lockfile_size*sizeof(char))
      } else {
        glob_xmount.output.vmdk.vmdk_lockfile_size=offset+size;
        XMOUNT_REALLOC(glob_xmount.output.vmdk.p_vmdk_lockfile_data,char*,
                       glob_xmount.output.vmdk.vmdk_lockfile_size*sizeof(char))
      }
    }
    // Copy data to buffer
    memcpy(glob_xmount.output.vmdk.p_vmdk_lockfile_data+offset,p_buf,size);
    pthread_mutex_unlock(&(glob_xmount.mutex_image_rw));
  } else if(strcmp(p_path,glob_xmount.output.p_info_path)==0) {
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

/*******************************************************************************
 * Main
 ******************************************************************************/
int main(int argc, char *argv[]) {
  struct stat file_stat;
  int ret;
  int fuse_ret;
  char *p_err_msg;

  // Set implemented FUSE functions
  struct fuse_operations xmount_operations = {
    //.access=FuseAccess,
    .getattr=FuseGetAttr,
    .mkdir=FuseMkDir,
    .mknod=FuseMkNod,
    .open=FuseOpen,
    .readdir=FuseReadDir,
    .read=FuseRead,
    .rename=FuseRename,
    .rmdir=FuseRmDir,
    //.statfs=FuseStatFs,
    .unlink=FuseUnlink,
    .write=FuseWrite
  };

  // Disable std output / input buffering
  setbuf(stdout,NULL);
  setbuf(stderr,NULL);

  // Init glob_xmount
  InitResources();

  // Load input and morphing libs
  if(!LoadLibs()) {
    LOG_ERROR("Unable to load any libraries!\n")
    return 1;
  }

  // Check FUSE settings
  CheckFuseSettings();

  // Parse command line options
  if(ParseCmdLine(argc,argv)!=TRUE) {
    PrintUsage(argv[0]);
    FreeResources();
    return 1;
  }

  // Check command line options
  if(glob_xmount.input.images_count==0) {
    LOG_ERROR("No --in command line option specified!\n")
    PrintUsage(argv[0]);
    FreeResources();
    return 1;
  }
  if(glob_xmount.fuse_argc<2) {
    LOG_ERROR("Couldn't parse command line options!\n")
    PrintUsage(argv[0]);
    FreeResources();
    return 1;
  }
  if(glob_xmount.morphing.p_morph_type==NULL) {
    XMOUNT_STRSET(glob_xmount.morphing.p_morph_type,"combine");
  }

  // Check if mountpoint is a valid dir
  if(stat(glob_xmount.p_mountpoint,&file_stat)!=0) {
    LOG_ERROR("Unable to stat mount point '%s'!\n",glob_xmount.p_mountpoint);
    PrintUsage(argv[0]);
    FreeResources();
    return 1;
  }
  if(!S_ISDIR(file_stat.st_mode)) {
    LOG_ERROR("Mount point '%s' is not a directory!\n",
              glob_xmount.p_mountpoint);
    PrintUsage(argv[0]);
    FreeResources();
    return 1;
  }

  if(glob_xmount.debug==TRUE) {
    LOG_DEBUG("Options passed to FUSE: ")
    for(int i=0;i<glob_xmount.fuse_argc;i++) {
      printf("%s ",glob_xmount.pp_fuse_argv[i]);
    }
    printf("\n");
  }

  // Init mutexes
  pthread_mutex_init(&(glob_xmount.mutex_image_rw),NULL);
  pthread_mutex_init(&(glob_xmount.mutex_info_read),NULL);

  // Load input images
  for(uint64_t i=0;i<glob_xmount.input.images_count;i++) {
    if(glob_xmount.debug==TRUE) {
      if(glob_xmount.input.pp_images[i]->files_count==1) {
        LOG_DEBUG("Loading image file \"%s\"...\n",
                  glob_xmount.input.pp_images[i]->pp_files[0])
      } else {
        LOG_DEBUG("Loading image files \"%s .. %s\"...\n",
                  glob_xmount.input.pp_images[i]->pp_files[0],
                  glob_xmount.input.pp_images[i]->
                    pp_files[glob_xmount.input.pp_images[i]->files_count-1])
      }
    }

    // Find input lib
    if(!FindInputLib(glob_xmount.input.pp_images[i])) {
      LOG_ERROR("Unknown input image type '%s' for input image '%s'!\n",
                glob_xmount.input.pp_images[i]->p_type,
                glob_xmount.input.pp_images[i]->pp_files[0])
      PrintUsage(argv[0]);
      FreeResources();
      return 1;
    }

    // Init input image handle
    ret=glob_xmount.input.pp_images[i]->p_functions->
          CreateHandle(&(glob_xmount.input.pp_images[i]->p_handle),
                       glob_xmount.input.pp_images[i]->p_type,
                       glob_xmount.debug);
    if(ret!=0) {
      LOG_ERROR("Unable to init input handle for input image '%s': %s!\n",
                glob_xmount.input.pp_images[i]->pp_files[0],
                glob_xmount.input.pp_images[i]->p_functions->
                  GetErrorMessage(ret));
      FreeResources();
      return 1;
    }

    // Parse input lib specific options
    if(glob_xmount.input.pp_lib_params!=NULL) {
      ret=glob_xmount.input.pp_images[i]->p_functions->
            OptionsParse(glob_xmount.input.pp_images[i]->p_handle,
                         glob_xmount.input.lib_params_count,
                         glob_xmount.input.pp_lib_params,
                         (const char**)&p_err_msg);
      if(ret!=0) {
        if(p_err_msg!=NULL) {
          LOG_ERROR("Unable to parse input library specific options for image "
                      "'%s': %s: %s!\n",
                    glob_xmount.input.pp_images[i]->pp_files[0],
                    glob_xmount.input.pp_images[i]->p_functions->
                      GetErrorMessage(ret),
                    p_err_msg);
          glob_xmount.input.pp_images[i]->p_functions->FreeBuffer(p_err_msg);
          FreeResources();
          return 1;
        } else {
          LOG_ERROR("Unable to parse input library specific options for image "
                      "'%s': %s!\n",
                    glob_xmount.input.pp_images[i]->pp_files[0],
                    glob_xmount.input.pp_images[i]->p_functions->
                      GetErrorMessage(ret));
          FreeResources();
          return 1;
        }
      }
    }

    // Open input image
    ret=
      glob_xmount.input.pp_images[i]->
        p_functions->
          Open(glob_xmount.input.pp_images[i]->p_handle,
               (const char**)(glob_xmount.input.pp_images[i]->pp_files),
               glob_xmount.input.pp_images[i]->files_count);
    if(ret!=0) {
      LOG_ERROR("Unable to open input image file '%s': %s!\n",
                glob_xmount.input.pp_images[i]->pp_files[0],
                glob_xmount.input.pp_images[i]->p_functions->
                  GetErrorMessage(ret));
      FreeResources();
      return 1;
    }

    // Determine input image size
    ret=glob_xmount.input.pp_images[i]->
      p_functions->
        Size(glob_xmount.input.pp_images[i]->p_handle,
             &(glob_xmount.input.pp_images[i]->size));
    if(ret!=0) {
      LOG_ERROR("Unable to determine size of input image '%s': %s!\n",
                glob_xmount.input.pp_images[i]->pp_files[0],
                glob_xmount.input.pp_images[i]->
                  p_functions->GetErrorMessage(ret));
      FreeResources();
      return 1;
    }

    // If an offset was specified, check it against offset and change size
    if(glob_xmount.input.image_offset!=0) {
      if(glob_xmount.input.image_offset>glob_xmount.input.pp_images[i]->size) {
        LOG_ERROR("The specified offset is larger than the size of the input "
                    "image '%s'! (%" PRIu64 " > %" PRIu64 ")\n",
                  glob_xmount.input.pp_images[i]->pp_files[0],
                  glob_xmount.input.image_offset,
                  glob_xmount.input.pp_images[i]->size);
        FreeResources();
        return 1;
      }
      glob_xmount.input.pp_images[i]->size-=glob_xmount.input.image_offset;
    }

    // If a size limit was specified, check it and change size
    if(glob_xmount.input.image_size_limit!=0) {
      if(glob_xmount.input.pp_images[i]->size<
           glob_xmount.input.image_size_limit)
      {
        LOG_ERROR("The specified size limit is larger than the size of the "
                    "input image '%s'! (%" PRIu64 " > %" PRIu64 ")\n",
                  glob_xmount.input.pp_images[i]->pp_files[0],
                  glob_xmount.input.image_size_limit,
                  glob_xmount.input.pp_images[i]->size);
        FreeResources();
        return 1;
      }
      glob_xmount.input.pp_images[i]->size=glob_xmount.input.image_size_limit;
    }

    LOG_DEBUG("Input image loaded successfully\n")
  }

  // Find morphing lib
  if(FindMorphingLib()!=TRUE) {
    LOG_ERROR("Unable to find a library supporting the morphing type '%s'!\n",
              glob_xmount.morphing.p_morph_type);
    FreeResources();
    return 1;
  }

  // Init morphing
  ret=glob_xmount.morphing.p_functions->
        CreateHandle(&glob_xmount.morphing.p_handle,
                     glob_xmount.morphing.p_morph_type,
                     glob_xmount.debug);
  if(ret!=0) {
    LOG_ERROR("Unable to create morphing handle: %s!\n",
              glob_xmount.morphing.p_functions->GetErrorMessage(ret));
    FreeResources();
    return 1;
  }

  // Parse morphing lib specific options
  if(glob_xmount.morphing.pp_lib_params!=NULL) {
    p_err_msg=NULL;
    ret=glob_xmount.morphing.p_functions->
          OptionsParse(glob_xmount.morphing.p_handle,
                       glob_xmount.morphing.lib_params_count,
                       glob_xmount.morphing.pp_lib_params,
                       (const char**)&p_err_msg);
    if(ret!=0) {
      if(p_err_msg!=NULL) {
        LOG_ERROR("Unable to parse morphing library specific options: %s: %s!\n",
                  glob_xmount.morphing.p_functions->GetErrorMessage(ret),
                  p_err_msg);
        glob_xmount.morphing.p_functions->FreeBuffer(p_err_msg);
        FreeResources();
        return 1;
      } else {
        LOG_ERROR("Unable to parse morphing library specific options: %s!\n",
                  glob_xmount.morphing.p_functions->GetErrorMessage(ret));
        FreeResources();
        return 1;
      }
    }
  }

  // Morph image
  ret=glob_xmount.morphing.p_functions->
        Morph(glob_xmount.morphing.p_handle,
              &(glob_xmount.morphing.input_image_functions));
  if(ret!=0) {
    LOG_ERROR("Unable to start morphing: %s!\n",
              glob_xmount.morphing.p_functions->GetErrorMessage(ret));
    FreeResources();
    return 1;
  }

  // Init random generator
  srand(time(NULL));

  // Calculate partial MD5 hash of input image file
  if(CalculateInputImageHash(&(glob_xmount.input.image_hash_lo),
                             &(glob_xmount.input.image_hash_hi))==FALSE)
  {
    LOG_ERROR("Couldn't calculate partial hash of morphed image!\n")
    return 1;
  }

  if(glob_xmount.debug==TRUE) {
    LOG_DEBUG("Partial MD5 hash of morphed image: ")
    for(int i=0;i<8;i++)
      printf("%02hhx",*(((char*)(&(glob_xmount.input.image_hash_lo)))+i));
    for(int i=0;i<8;i++)
      printf("%02hhx",*(((char*)(&(glob_xmount.input.image_hash_hi)))+i));
    printf("\n");
  }

  if(!ExtractVirtFileNames(glob_xmount.input.pp_images[0]->pp_files[0])) {
    LOG_ERROR("Couldn't extract virtual file names!\n");
    FreeResources();
    return 1;
  }
  LOG_DEBUG("Virtual file names extracted successfully\n")

  // Gather infos for info file
  if(!InitVirtImageInfoFile()) {
    LOG_ERROR("Couldn't gather infos for virtual image info file!\n")
    FreeResources();
    return 1;
  }
  LOG_DEBUG("Virtual image info file build successfully\n")

  // Do some virtual image type specific initialisations
  switch(glob_xmount.output.VirtImageType) {
    case VirtImageType_DD:
    case VirtImageType_DMG:
      break;
    case VirtImageType_VDI:
      // When mounting as VDI, we need to construct a vdi header
      if(!InitVirtVdiHeader()) {
        LOG_ERROR("Couldn't initialize virtual VDI file header!\n")
        FreeResources();
        return 1;
      }
      LOG_DEBUG("Virtual VDI file header build successfully\n")
      break;
    case VirtImageType_VHD:
      // When mounting as VHD, we need to construct a vhd footer
      if(!InitVirtVhdHeader()) {
        LOG_ERROR("Couldn't initialize virtual VHD file footer!\n")
        FreeResources();
        return 1;
      }
      LOG_DEBUG("Virtual VHD file footer build successfully\n")
      break;
    case VirtImageType_VMDK:
    case VirtImageType_VMDKS:
      // When mounting as VMDK, we need to construct the VMDK descriptor file
      if(!InitVirtualVmdkFile()) {
        LOG_ERROR("Couldn't initialize virtual VMDK file!\n")
        FreeResources();
        return 1;
      }
      break;
  }

  if(glob_xmount.output.writable) {
    // Init cache file and cache file block index
    if(!InitCacheFile()) {
      LOG_ERROR("Couldn't initialize cache file!\n")
      FreeResources();
      return 1;
    }
    LOG_DEBUG("Cache file initialized successfully\n")
  }

  // Call fuse_main to do the fuse magic
  fuse_ret=fuse_main(glob_xmount.fuse_argc,
                     glob_xmount.pp_fuse_argv,
                     &xmount_operations,
                     NULL);

  // Destroy mutexes
  pthread_mutex_destroy(&(glob_xmount.mutex_image_rw));
  pthread_mutex_destroy(&(glob_xmount.mutex_info_read));

  // Free allocated memory
  FreeResources();
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
            * Re-implemented InitVirtImageInfoFile() using input lib's
              GetInfofileContent() function.
            * Further code cleanups.
  20140807: * Further code cleanups.
            * Renamed GetVirtFileAttr() to FuseGetAttr()
            * Renamed CreateVirtDir() to FuseMkDir()
            * Renamed CreateVirtDir() to FuseMkNod()
            * Renamed OpenVirtFile() to FuseOpen()
            * Renamed GetVirtFiles() to FuseReadDir()
            * Renamed ReadVirtFile() to FuseRead()
            * Renamed RenameVirtFile() to FuseRename()
            * Renamed DeleteVirtDir() to FuseRmDir()
            * Renamed DeleteVirtFile() to FuseUnlink()
            * Renamed WriteVirtFile() to FuseWrite()
            * Fixed bug in CalculateInputImageHash() where always HASH_AMOUNT
              bytes were hased even if input image is smaller.
            * Fixed a newly introduced bug in FuseRead() and GetVirtImageData()
              returning -EIO when trying to read behind EOF. The correct return
              value is 0.
  20140811: * Renamed CheckFuseAllowOther() to CheckFuseSettings() and added a
              check to see if user is part of the fuse group.
  20140814: * Replaced cuserid() with getpwuid(geteuid()) in CheckFuseSettings()
              as it is deprecated on Linux and not available on OSx.
            * Only build fuse group checks from CheckFuseSettings() on Linux.
  20140820: * Added libxmount_morphing handling.
            * Renamed LoadInputLibs() to LoadLibs() and added code to find and
              load morphing libs.
            * Added FindMorphingLib() function.
  20140821: * Changed command line syntax to support specification of multiple
              input images of different types.
            * Added code in main() to initialize and open multiple input images.
            * Added GetMorphedImageSize() and GetMorphedImageData().
  20140822: * Removed GetInputImageSize() as it isn't needed anymore.
            * Quick test showed morphing using libxmount_morphing_combine seems
              to work.
  20140825: * Added InitResources() and FreeResources() functions.
  20140926: v0.7.0 released
  20140930: v0.7.1 released
            * Fixed bug with --sizelimit command line option.
  20141001: v0.7.2 released
            * Fixed bug in FreeResources(). Do not free vdi.p_vdi_block_map as
              it is part of vdi.p_vdi_header.
  20141113: v0.7.3 released
            * Fixed minor bug in ParseCmdLine() which was not checking upper
              argc bounds correctly.
  20150820: v0.7.4 released
  20150901: * Improved the way fsname is built
*/

