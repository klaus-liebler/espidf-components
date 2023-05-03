#include <net/if.h>
#include <sys/socket.h>
#include <pwd.h>
#include <glob.h>
#include <errno.h>
#include <libssh/libssh.h>

#if !defined(GLOB_NOMATCH)
#define GLOB_NOMATCH -3
#endif

#define gai_strerror(ecode) "unknown error"
uid_t getuid(){
    return (uid_t)0;
}


int getpwuid_r(uid_t uid, struct passwd *pwd, char *buf, size_t buflen, struct passwd **result){
    errno = ENOENT;
    *result = NULL;
    return 1;
} 

struct passwd *getpwnam(const char *name){
    errno = ENOENT;
    return NULL;
}


int gethostname(char *name, size_t namelen){
    strlcpy(name, "esp32", namelen);
    return 0;
}

pid_t waitpid(pid_t pid, int *status, int options){
    errno = ENOSYS;
    return (pid_t)-1;
}


int glob(const char *pattern, int flags, int (*errfunc) (const char *epath, int eerrno), glob_t *pglob){
    errno = ENOENT;
    return GLOB_NOMATCH;
}

void globfree(glob_t *pglob){
    do { } while(0);
}



int socketpair(int domain, int type, int protocol,int socket_vector[2]){
    errno = ENOSYS; 
    return -1;
}