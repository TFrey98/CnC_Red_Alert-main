/* direct.h -- directory calls, mapped to POSIX. */
#ifndef WWPORT_COMPAT_DIRECT_H
#define WWPORT_COMPAT_DIRECT_H
#include <unistd.h>
#include <sys/stat.h>
#define _getcwd getcwd
#define _chdir  chdir
#define _mkdir(p) mkdir((p), 0755)
#define _rmdir  rmdir
#endif
