#include <net/if.h>
#include <pwd.h>
#include <glob.h>
#include <errno.h>

#if !defined(GLOB_NOMATCH)
#define GLOB_NOMATCH -3
#endif

#define gai_strerror(ecode) "unknown error"
#define        PF_LOCAL        1        /* Local to host (pipes and file-domain).  */
#define        PF_UNIX                PF_LOCAL /* POSIX name for PF_LOCAL.  */
#define        AF_UNIX                PF_UNIX
int socketpair(int domain, int type, int protocol,int socket_vector[2]);