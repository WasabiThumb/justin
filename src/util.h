#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include "logging.h"

#ifndef JUSTIN_UTIL_H
#define JUSTIN_UTIL_H

#define NUMARGS(...)  (sizeof((int[]){__VA_ARGS__})/sizeof(int))

int justin_util_mini(size_t count, ...);
#define mini(...) justin_util_mini(NUMARGS(__VA_ARGS__), __VA_ARGS__)

size_t justin_util_path_join(const char *restrict a, size_t al, const char *restrict b, size_t bl, char* out);

int justin_util_rimraf(const char *dir);

int justin_util_chown_r(const char *dir, uid_t user);

char justin_util_n2hex(int n);

int justin_util_hex2n(char hex);

/**
 * Writes a null-terminated hex string to "out" from the given byte buffer
 * @param buf Bytes to read
 * @param len Size of buffer
 * @param out Hex string, should be sized ((len shl 1) or 1)
 */
void justin_util_b2hex(void *buf, size_t len, char *out);

/**
 * Writes bytes to "out" from a null-terminated hex string. The number of bytes written to "out" will be
 * @code ((strlen(hex) + 1) >> 1)
 * @endcode
 * In other words, the number of bytes that the hex string codes for, unless the hex string is odd length in which case
 * the last nibble is included in "out" shl 4.
 */
void justin_util_hex2b(const char *hex, void *out);

int justin_util_str_dist(const char *restrict a, int al, const char *restrict b, int bl);
#define strdist(s, t) justin_util_str_dist(s, (int) strlen(s), t, (int) strlen(t));

int justin_util_strlenol(const char *str);
/**
 * One-line string length (gets position of first NULL, CR or LF char)
 */
#define strlenol(str) justin_util_strlenol(str)

struct justin_util_iset_t;
typedef struct justin_util_iset_t *justin_util_iset;

justin_util_iset justin_util_iset_parse(const char* str, justin_err *err);

void justin_util_iset_destroy(justin_util_iset iset);

bool justin_util_iset_contains(justin_util_iset iset, int i);

int justin_util_sudo(int argc, char *argv[], uid_t uid);

#endif //JUSTIN_UTIL_H
