#include <string.h>
#include <stdio.h>
#define _XOPEN_SOURCE 500
#define __USE_XOPEN_EXTENDED
#include <ftw.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include "logging.h"
#include "util.h"

size_t justin_util_path_join(const char *restrict a, size_t al, const char *restrict b, size_t bl, char* out) {
    if (al < 1) {
        strcpy(out, b);
        return bl;
    }
    if (bl < 1) {
        strcpy(out, a);
        return al;
    }

    memcpy(out, a, al); // NOLINT(bugprone-not-null-terminated-result)
    size_t off = al;

    char aLast = a[al - 1];
    if (!(aLast == '/' || aLast == '\\')) {
        out[off++] = '/';
    }

    char bFirst = b[0];
    if (bFirst == '/' || bFirst == '\\') {
        memcpy(&out[off], &b[1], bl - 1); // NOLINT(bugprone-not-null-terminated-result)
        off += bl - 1;
    } else {
        memcpy(&out[off], b, bl); // NOLINT(bugprone-not-null-terminated-result)
        off += bl;
    }

    out[off] = (char) 0;
    return off;
}

int justin_util_rimraf0(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
    switch (typeflag) {
        case FTW_F:
        case FTW_SL:
            // is a file
            if (unlink(fpath) == -1) {
                return JUSTIN_ERR_SYSTEM;
            }
            break;
        case FTW_D:
        case FTW_DP:
            // is a directory
            if (rmdir(fpath) == -1) {
                return JUSTIN_ERR_SYSTEM;
            }
            break;
        default:
            break;
    }
    return 0;
}

int justin_util_rimraf(const char *dir) {
    return nftw(dir, justin_util_rimraf0, 16, FTW_DEPTH | FTW_PHYS);
}
