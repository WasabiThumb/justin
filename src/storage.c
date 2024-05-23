#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <stdbool.h>
#include <pwd.h>
#include <errno.h>
#include <dirent.h>
#include "util.h"
#include "logging.h"
#include "storage.h"

#define DATA_DIR_STR ".cache/justin"
static const char* DATA_DIR = DATA_DIR_STR;
#define DATA_DIR_LEN ((sizeof DATA_DIR_STR) - 1)

#define LOCK_FILE_STR ".lockfile"
static const char* LOCK_FILE = LOCK_FILE_STR;
#define LOCK_FILE_LEN ((sizeof LOCK_FILE_STR) - 1)

justin_storage justin_storage_init() {
    struct passwd *user = getpwuid(getuid());
    char *home = user->pw_dir;
    size_t home_len = strlen(home);

    justin_storage ret = (justin_storage) justin_malloc_msg(sizeof(struct justin_storage) + home_len + DATA_DIR_LEN + 2, "Failed to allocate storage struct");
    char* dir = (char*) (ret + sizeof(struct justin_storage));
    size_t dir_len = justin_util_path_join(home, strlen(home), DATA_DIR, DATA_DIR_LEN, dir);
    ret->path = dir;

    struct stat st = { 0 };
    if (stat(dir, &st) == -1) {
        justin_log_warn("Cache directory does not exist, creating...");
        if (mkdir(dir, 0775) == -1) {
            justin_log_err_soft(JUSTIN_ERR_SYSTEM);
        }
    }

    char* lockfile_path = (char*) alloca(home_len + DATA_DIR_LEN + LOCK_FILE_LEN + 3);
    justin_util_path_join(dir, dir_len, LOCK_FILE, LOCK_FILE_LEN, lockfile_path);
    int lockfile_fd = open(lockfile_path, O_CREAT | O_RDWR, 0775);
    if (lockfile_fd == -1) {
        free(ret);
        return NULL;
    }
    if (ftruncate(lockfile_fd, sizeof(struct justin_storage_lockfile)) == -1) {
        free(ret);
        return NULL;
    }
    ret->lockfile_handle = lockfile_fd;

    if (flock(lockfile_fd, LOCK_EX) == -1) {
        free(ret);
        return NULL;
    }

    long page_size = sysconf(_SC_PAGE_SIZE);
    justin_storage_lockfile lockfile = (justin_storage_lockfile) mmap(NULL, page_size, PROT_READ | PROT_WRITE, MAP_SHARED, lockfile_fd, 0);
    if (lockfile == MAP_FAILED) {
        flock(lockfile_fd, LOCK_UN);
        free(ret);
        return NULL;
    }

    ret->lockfile = lockfile;
    uint8_t pid = lockfile->counter++;
    ret->pid = pid & 63;
    ret->pid_page = pid >> 6;
    bool full = (lockfile->lock[ret->pid_page] & (1 << ((uint64_t) ret->pid))) != 0;
    if (full) lockfile->counter--;

    int sync_err = msync(lockfile, page_size, MS_SYNC | MS_INVALIDATE);
    flock(lockfile_fd, LOCK_UN);
    if (sync_err == -1) {
        munmap(lockfile, page_size);
        free(ret);
        return NULL;
    }

    if (full) {
        justin_log_err(JUSTIN_ERR_LOCKFILE_FULL);
        munmap(lockfile, page_size);
        free(ret);
        return NULL;
    }

    return ret;
}

void justin_storage_destroy(justin_storage storage) {
    if (munmap(storage->lockfile, sysconf(_SC_PAGE_SIZE)) == -1) {
        justin_log_err_soft(JUSTIN_ERR_SYSTEM);
    }
    close(storage->lockfile_handle);
    free(storage);
}

// INTERNAL METHOD! ASSUMES THAT LOCKFILE IS CURRENTLY WRITE-LOCKED
void justin_storage_clean(justin_storage storage) {
    DIR *d;
    struct dirent *ent;
    d = opendir(storage->path);
    if (d == NULL) return;

    size_t base_len = strlen(storage->path);
    char* path_buffer = malloc(base_len + FILENAME_MAX + 2);
    if (path_buffer == NULL) {
        justin_log_err(JUSTIN_ERR_NOMEM);
        closedir(d);
        return;
    }

    size_t ent_name_len;
    int err;
    while ((ent = readdir(d)) != NULL) {
        if (ent->d_type != DT_DIR) continue;
        ent_name_len = strlen(ent->d_name);
        if (ent_name_len < 1) continue;
        if (ent->d_name[0] == '.') continue;
        justin_util_path_join(storage->path, base_len, ent->d_name, ent_name_len, path_buffer);
        err = justin_util_rimraf(path_buffer);
        if (err != 0) {
            justin_log_err_msg(err, "Failed to clean cache");
            break;
        }
    }
    free(path_buffer);
    closedir(d);
}

int justin_storage_set_locked(justin_storage storage, bool locked) {
    if (flock(storage->lockfile_handle, LOCK_SH) == -1) return JUSTIN_ERR_SYSTEM;

    uint64_t flag = storage->lockfile->lock[storage->pid_page];
    uint64_t entry = ((uint64_t) 1) << ((uint64_t) storage->pid);
    flock(storage->lockfile_handle, LOCK_UN);
    if ((flag & entry) == 0) {
        if (!locked) return 0;
    } else {
        if (locked) return 0;
    }

    if (flock(storage->lockfile_handle, LOCK_EX) == -1) return JUSTIN_ERR_SYSTEM;
    if (locked) {
        flag |= entry;
    } else {
        flag &= (~entry);
    }
    storage->lockfile->lock[storage->pid_page] = flag;

    if (!locked) {
        bool all = true;
        for (int i=0; i < 4; i++) {
            if (storage->lockfile->lock[i] != 0) {
                all = false;
                break;
            }
        }
        if (all) {
            justin_storage_clean(storage);
        }
    }

    msync(storage->lockfile, sysconf(_SC_PAGE_SIZE), MS_SYNC | MS_INVALIDATE);
    if (flock(storage->lockfile_handle, LOCK_UN) == -1) return JUSTIN_ERR_SYSTEM;
    return 0;
}

int justin_storage_lock(justin_storage storage) {
    return justin_storage_set_locked(storage, true);
}

int justin_storage_unlock(justin_storage storage) {
    return justin_storage_set_locked(storage, false);
}
