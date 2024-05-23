#include <stdint.h>

#ifndef JUSTIN_STORAGE_H
#define JUSTIN_STORAGE_H

struct justin_storage_lockfile {
    uint8_t counter;
    uint64_t lock[4];
};
typedef struct justin_storage_lockfile* justin_storage_lockfile;

struct justin_storage {
    const char *path;
    int lockfile_handle;
    justin_storage_lockfile lockfile;
    uint8_t pid;
    uint8_t pid_page;
};
typedef struct justin_storage* justin_storage;

justin_storage justin_storage_init();

void justin_storage_destroy(justin_storage storage);

int justin_storage_lock(justin_storage storage);

int justin_storage_unlock(justin_storage storage);

const char* justin_storage_dir_create(justin_storage storage);

#endif //JUSTIN_STORAGE_H
