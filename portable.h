#ifndef PORTABLE_H
#define PORTABLE_H

#if !defined(WIN32)

#define __STDC_FORMAT_MACROS

#include <inttypes.h>

typedef int64_t __int64;

#define _fileno fileno

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

static int64_t _filelengthi64( int fd )
{
    struct stat st;
    if (fstat(fd, &st) == -1) {
        return -1L;
    }
    return st.st_size;
}

#endif

#endif /* PORTABLE_H */
