
#ifndef JUSTIN_UTIL_H
#define JUSTIN_UTIL_H

size_t justin_util_path_join(const char *restrict a, size_t al, const char *restrict b, size_t bl, char* out);

int justin_util_rimraf(const char *dir);

#endif //JUSTIN_UTIL_H
