#ifndef _GETOPT_H_
#define _GETOPT_H_


#ifdef  __cplusplus
extern "C" {
#endif

#ifdef __NEVER_DEFINED__
} // close extern "C" for emacs
#endif


int	getopt(int argc, char * const *argv, const char *opts);
extern  int optind;
extern	int optopt;
extern	char *optarg;

#ifdef __NEVER_DEFINED__
{
#endif
#ifdef  __cplusplus
}
#endif

#endif
