/* io.h -- DOS/Win32 low-level I/O names mapped onto their POSIX equivalents. */
#ifndef WWPORT_COMPAT_IO_H
#define WWPORT_COMPAT_IO_H
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdio.h>
#define _open   open
#define _close  close
#define _read   read
#define _write  write
#define _lseek  lseek
#define _unlink unlink
#define _access access
#define O_BINARY 0     /* macOS draws no text/binary distinction. */
#endif
