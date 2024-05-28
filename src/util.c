/*
   Copyright 2024 Wasabi Codes

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
 */

#define _XOPEN_SOURCE 500
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <ftw.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>
#include <unistd.h>
#include <alloca.h>
#include "logging.h"
#include "util.h"

int justin_util_mini(size_t count, ...) {
    if (count < 1) return -1;
    va_list vl;
    va_start(vl, count);
    int ret = va_arg(vl, int);
    int n;
    for (int i=1; i < count; i++) {
        n = va_arg(vl, int);
        if (n < ret) ret = n;
    }
    va_end(vl);
    return ret;
}

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
                return errno;
            }
            break;
        case FTW_D:
        case FTW_DP:
            // is a directory
            if (rmdir(fpath) == -1) {
                return errno;
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

static uid_t CHOWN_RECURSIVE_ACTIVE_USER = 0;
int justin_util_chown_r0(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
    switch (typeflag) {
        case FTW_F: case FTW_SL: case FTW_D: case FTW_DP:
            if (chown(fpath, CHOWN_RECURSIVE_ACTIVE_USER, -1) == -1) {
                return errno;
            }
            return 0;
        default:
            return 0;
    }
}

int justin_util_chown_r(const char *dir, uid_t user) {
    pid_t pid = fork();
    switch (pid) {
        case -1:
            return 1;
        case 0: {
            CHOWN_RECURSIVE_ACTIVE_USER = user;
            exit(nftw(dir, justin_util_chown_r0, 16, FTW_DEPTH | FTW_PHYS));
        }
        default: {
            int stat;
            if (waitpid(pid, &stat, 0) == -1) {
                return 1;
            }
            if (WIFEXITED(stat)) {
                int exs = WEXITSTATUS(stat);
                if (exs != 0) {
                    errno = exs;
                    return 1;
                } else {
                    return 0;
                }
            }
            justin_log_warn("Child process did not terminate normally");
            return 1;
        }
    }
}

static const char *HEX_CHARS = "0123456789ABCDEF";
char justin_util_n2hex(int n) {
    return HEX_CHARS[n];
}

int justin_util_hex2n(char hex) {
    if (hex < 'A') {
        return (int) (hex - '0');
    } else if (hex < 'a') {
        return ((int) (hex - 'A')) + 10;
    } else {
        return ((int) (hex - 'a')) + 10;
    }
}

void justin_util_b2hex(void *buf, size_t len, char *out) {
    unsigned char *bytes = (unsigned char*) buf;
    int n = 0;
    unsigned char b;
    for (size_t i=0; i < len; i++) {
        b = bytes[i];
        out[n++] = justin_util_n2hex((int) (b >> 4));
        out[n++] = justin_util_n2hex((int) (b & 0xF));
    }
    out[n] = '\0';
}

void justin_util_hex2b(const char *hex, void *out) {
    unsigned char *bytes = (unsigned char*) out;
    char c;
    int n = 0;
    int i = -1;
    while ((c = hex[n]) != '\0') {
        if ((n & 1) == 0) {
            i++;
            bytes[i] = (unsigned char) (justin_util_hex2n(c) << 4);
        } else {
            bytes[i] |= (unsigned char) justin_util_hex2n(c);
        }
        n++;
    }
}

/*
 * Reference implementation:
 * https://en.wikipedia.org/wiki/Levenshtein_distance#Iterative_with_two_matrix_rows
 */
int justin_util_str_dist(const char *restrict s, int m, const char *restrict t, int n) {
    int *v0 = (int*) calloc(sizeof(int), n + n + 2);
    if (v0 == NULL) return -1;
    int *v1 = &v0[n + 1];
    bool zero_owns = true;

    for (int i=0; i <= n; i++) {
        v0[i] = i;
    }

    int dc, ic, sc;
    for (int i=0; i < m; i++) {
        v1[0] = i + 1;

        for (int j=0; j < n; j++) {
            dc = v0[j + 1] + 1;
            ic = v1[j] + 1;
            sc = v0[j];
            if (s[i] != t[j]) sc++;
            v1[j + 1] = mini(dc, ic, sc);
        }

        int *carry = v1;
        v1 = v0;
        v0 = carry;
        zero_owns = !zero_owns;
    }

    int ret = v0[n];
    free(zero_owns ? v0 : v1);
    return ret;
}

int justin_util_strlenol(const char *str) {
    int i = 0;
    char c;
    while (1) {
        c = str[i];
        if (c == '\0' || c == '\r' || c == '\n') return i;
        i++;
    }
}

// ISET

struct justin_util_iset_entry_range {
    bool is_range;
    union justin_util_iset_entry *next;
    int start;
    int end;
};

struct justin_util_iset_entry_single {
    bool is_range;
    union justin_util_iset_entry *next;
    int start;
};

union justin_util_iset_entry {
    struct justin_util_iset_entry_single single;
    struct justin_util_iset_entry_range range;
};

struct justin_util_iset_t {
    union justin_util_iset_entry *root;
};

bool justin_util_iset_insert(justin_util_iset set, union justin_util_iset_entry entry, size_t size, justin_err *err) {
    union justin_util_iset_entry *heap = (union justin_util_iset_entry*) malloc(size);
    if (heap == NULL) {
        *err = JUSTIN_ERR_NOMEM;
        return true;
    }
    memcpy(heap, &entry, size);

    union justin_util_iset_entry *child = set->root;
    if (child == NULL) {
        set->root = heap;
        return false;
    }

    union justin_util_iset_entry *head;
    do {
        head = child;
        child = (&head->single)->next;
    } while (child != NULL);

    (&head->single)->next = heap;
    return false;
}

bool justin_util_iset_insert_single(justin_util_iset set, int i, justin_err *err) {
    union justin_util_iset_entry entry;
    struct justin_util_iset_entry_single single = { false, NULL, i };
    entry.single = single;
    return justin_util_iset_insert(set, entry, sizeof(struct justin_util_iset_entry_single), err);
}

bool justin_util_iset_insert_range(justin_util_iset set, int a, int b, justin_err *err) {
    union justin_util_iset_entry entry;
    struct justin_util_iset_entry_range range = { true, NULL, a, b };
    entry.range = range;
    return justin_util_iset_insert(set, entry, sizeof(struct justin_util_iset_entry_range), err);
}

justin_util_iset justin_util_iset_parse(const char* str, justin_err *err) {
    justin_util_iset ret = (justin_util_iset) malloc(sizeof(struct justin_util_iset_t));
    if (ret == NULL) {
        *err = JUSTIN_ERR_NOMEM;
        return NULL;
    }
    ret->root = NULL;

    int target[2] = { 0, 0 };
    uint_fast8_t stage = 0;
    int_fast8_t type = 0;
    char c;
    size_t i = 0;
    while ((c = str[i++]) != '\0') {
        switch (c) {
            case '-': {
                if (type == 0) {
                    type = -1;
                } else {
                    stage = 1;
                    type = 0;
                }
                break;
            }
            case ',': {
                if (type != 0) {
                    bool out;
                    if (stage == 1) {
                        out = justin_util_iset_insert_range(ret, target[0], target[1], err);
                    } else {
                        out = justin_util_iset_insert_single(ret, target[0], err);
                    }
                    if (out) {
                        justin_util_iset_destroy(ret);
                        return NULL;
                    }
                    type = 0;
                }
                memset(target, 0, sizeof(int) * 2);
                stage = 0;
                break;
            }
            case '0': case '1': case '2':
            case '3': case '4': case '5':
            case '6': case '7': case '8':
            case '9': {
                if (type == 0) type = 1;
                int *next = &target[stage];
                *next *= 10;
                *next += (int) (((int_fast8_t) (c - '0')) * type);
                break;
            }
            case ' ':
                break;
            default:
                *err = JUSTIN_ERR_ARGS;
                justin_util_iset_destroy(ret);
                return NULL;
        }
    }
    if (type != 0) {
        bool out;
        if (stage == 1) {
            out = justin_util_iset_insert_range(ret, target[0], target[1], err);
        } else {
            out = justin_util_iset_insert_single(ret, target[0], err);
        }
        if (out) {
            justin_util_iset_destroy(ret);
            return NULL;
        }
    }

    return ret;
}

void justin_util_iset_destroy(justin_util_iset iset) {
    union justin_util_iset_entry *entry = iset->root;
    union justin_util_iset_entry *child;
    while (entry != NULL) {
        child = entry->single.next;
        free(entry);
        entry = child;
    }
    free(iset);
}

bool justin_util_iset_contains(justin_util_iset iset, int i) {
    union justin_util_iset_entry *entry = iset->root;
    while (entry != NULL) {
        struct justin_util_iset_entry_single *single = &entry->single;
        if (single->is_range) {
            struct justin_util_iset_entry_range *range = &entry->range;
            if ((i >= range->start) && (i <= range->end)) return true;
            entry = range->next;
        } else {
            if (i == single->start) return true;
            entry = single->next;
        }
    }
    return false;
}

#define SUDO_EXE "/usr/bin/sudo -S"
static const char *SUDO_EXE_S = SUDO_EXE;

#define PROC_SELF_EXE "/proc/self/exe"
static const char *PROC_SELF_EXE_S = PROC_SELF_EXE;

#define ARG_U " -u"
static const char *ARG_U_S = ARG_U;
#define ARG_U_L ((sizeof ARG_U) - 1)

int justin_util_sudo0(const char *cmd) {
    int ret = 0;

    errno = 0;
    FILE *proc = popen(cmd, "r");
    if (proc == NULL) {
        if (errno == 0) {
            justin_log_err(JUSTIN_ERR_NOMEM);
        } else {
            justin_log_err(JUSTIN_ERR_SYSTEM);
        }
        return 1;
    }

    char buf[1024];
    while (fgets(buf, sizeof(buf), proc) != NULL) {
        printf("%s", buf);
    }
    if (errno != 0) {
        justin_log_err(JUSTIN_ERR_SYSTEM);
        ret = 1;
    }

    int close = pclose(proc);
    if (close == -1) {
        if (ret == 0) {
            justin_log_err(JUSTIN_ERR_SYSTEM);
            ret = 1;
        }
    } else {
        ret = WEXITSTATUS(close);
    }

    return ret;
}

int justin_util_sudo(int argc, char *argv[], uid_t uid) {
    size_t bsize = 16;
    char *buf = (char*) malloc(bsize);
    if (buf == NULL) {
        justin_log_err(JUSTIN_ERR_NOMEM);
        return 1;
    }

    ssize_t read;
    while (1) {
        read = readlink(PROC_SELF_EXE_S, buf, bsize);
        if (read == -1) {
            free(buf);
            justin_log_err(JUSTIN_ERR_SYSTEM);
            return 1;
        }
        if (read == bsize) {
            bsize <<= 1;
            buf = (char*) realloc(buf, bsize);
            if (buf == NULL) {
                justin_log_err(JUSTIN_ERR_NOMEM);
                return 1;
            }
        } else {
            buf[read] = '\0';
            break;
        }
    }

    size_t tsize = (sizeof SUDO_EXE) + read + ARG_U_L + (sizeof(__uid_t) << 1) + 8;
    for (int i=1; i < argc; i++) {
        bsize += strlen(argv[i]) + 1;
    }

    char *full = (char*) malloc(tsize);
    if (full == NULL) {
        free(buf);
        justin_log_err(JUSTIN_ERR_NOMEM);
        return 1;
    }

    memcpy(full, SUDO_EXE_S, (sizeof SUDO_EXE) - 1);
    full[(sizeof SUDO_EXE) - 1] = ' ';
    memcpy(&full[sizeof SUDO_EXE], buf, read);
    free(buf);

    size_t args_off = sizeof SUDO_EXE + read;
    char *args_el;
    size_t args_el_len;
    for (int i=1; i < argc; i++) {
        full[args_off++] = ' ';
        args_el = argv[i];
        args_el_len = strlen(args_el);
        memcpy(&full[args_off], args_el, args_el_len);
        args_off += args_el_len;
    }
    memcpy(&full[args_off], ARG_U_S, ARG_U_L);
    args_off += ARG_U_L;

    justin_util_b2hex(&uid, sizeof(uid_t), &full[args_off]);

    int ret = justin_util_sudo0(full);
    free(full);
    return ret;
}
