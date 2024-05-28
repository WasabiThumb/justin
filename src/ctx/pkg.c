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

#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <dirent.h>
#include <alpm.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include "../logging.h"
#include "../util.h"
#include "pkg.h"

#define PATH2_MAKEPKG "/usr/bin/makepkg"
static const char *PATH2_MAKEPKG_S = PATH2_MAKEPKG;

#define PKG_EXT ".pkg.tar.zst"
static const char *PKG_EXT_S = PKG_EXT;
#define PKG_EXT_L ((sizeof PKG_EXT) - 1)

void justin_pkg_make0(const char *path, justin_err *err) {
    char *old_cwd = get_current_dir_name();
    if (old_cwd == NULL) {
        *err = JUSTIN_ERR_NOMEM;
        return;
    }
    if (chdir(path) != 0) {
        *err = JUSTIN_ERR_SYSTEM;
        goto ex;
    }

    errno = 0;
    FILE *proc = popen(PATH2_MAKEPKG_S, "r");
    if (proc == NULL) {
        if (errno == 0) {
            *err = JUSTIN_ERR_NOMEM;
        } else {
            *err = JUSTIN_ERR_SYSTEM;
        }
        goto ex;
    }

    char buf[1024];
    while (fgets(buf, sizeof(buf), proc) != NULL) {
        printf("%s", buf);
    }
    if (errno != 0) *err = JUSTIN_ERR_SYSTEM;

    int close = pclose(proc);
    if (close == -1) {
        if ((*err) == JUSTIN_ERR_OK) *err = JUSTIN_ERR_SYSTEM;
    } else {
        int stat = WEXITSTATUS(close);
        if (stat != 0) {
            *err = JUSTIN_ERR_SUBPROC(stat);
        }
    }

    ex:
    chdir(old_cwd);
    free(old_cwd);
}

// Setup correct permissions and run justin_pkg_make0
void justin_pkg_make(uid_t user, const char *path, justin_err *err) {
    justin_err *shared_err = (justin_err*) mmap(NULL, sizeof(justin_err), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (shared_err == MAP_FAILED) {
        *err = JUSTIN_ERR_SYSTEM;
        return;
    }
    *shared_err = JUSTIN_ERR_OK;

    pid_t pid = fork();
    switch (pid) {
        case -1:
            *err = JUSTIN_ERR_SYSTEM;
            goto close_map;
        case 0: {
            setuid(user);
            justin_pkg_make0(path, shared_err);
            exit(0);
        }
        default: {
            int stat;
            if (waitpid(pid, &stat, 0) == -1) {
                *err = JUSTIN_ERR_SYSTEM;
                goto close_map;
            }
            if (WIFEXITED(stat)) {
                int exs = WEXITSTATUS(stat);
                if (exs != 0) *err = JUSTIN_ERR_SUBPROC(exs);
                goto close_map;
            }
            justin_log_warn("Child process did not terminate normally");
            *err = JUSTIN_ERR_ASSERTION;
            goto close_map;
        }
    }

    close_map:
    if (*err == JUSTIN_ERR_OK) memcpy(err, shared_err, sizeof(justin_err));
    int unmap_err = munmap(shared_err, sizeof(justin_err));
    if (unmap_err == -1 && *err == JUSTIN_ERR_OK) *err = JUSTIN_ERR_SYSTEM;
}

void justin_pkg_install(justin_context ctx, const char *file, justin_err *err) {
    alpm_db_t *db = ctx->alpm_db;
    alpm_handle_t *handle = alpm_db_get_handle(db);

    justin_log_debug("Initializing transaction");
    if (alpm_trans_init(handle, ALPM_TRANS_FLAG_ALLEXPLICIT) != 0) {
        *err = JUSTIN_ERR_ALPM;
        return;
    }

    alpm_siglevel_t level = alpm_option_get_local_file_siglevel(handle);
    justin_log_debug("Loading package");
    alpm_pkg_t *pkg;
    if (alpm_pkg_load(handle, file, true, level, &pkg) != 0) {
        *err = JUSTIN_ERR_ALPM;
        alpm_trans_release(handle);
        return;
    }

    justin_log_debug("Adding package to transaction");
    if (alpm_add_pkg(handle, pkg) != 0) {
        *err = JUSTIN_ERR_ALPM;
        alpm_pkg_free(pkg);
        alpm_trans_release(handle);
        return;
    }
    // The package is now bound to the transaction, releasing the transaction will free the package.

    justin_log_debug("Preparing transaction");
    alpm_list_t *list;
    if (alpm_trans_prepare(handle, &list) != 0) {
        *err = JUSTIN_ERR_ALPM;
        alpm_trans_release(handle);
        return;
    }

    justin_log_debug("Committing transaction");
    if (alpm_trans_commit(handle, &list) != 0) {
        *err = JUSTIN_ERR_ALPM;
        alpm_trans_release(handle);
        return;
    }

    alpm_trans_release(handle);
    *err = JUSTIN_ERR_OK;
}

struct justin_pkg_target_list_t {
    const char *dir;
    DIR *dirent;
    bool dirent_open;
    char **cache;
    size_t cache_len;
    size_t cache_capacity;
    uint8_t *selection;
    size_t selection_len;
};

justin_pkg_target_list justin_pkg_target_list_create(const char *path, justin_err *err) {
    *err = JUSTIN_ERR_OK;

    DIR *dirent = opendir(path);
    if (dirent == NULL) {
        *err = JUSTIN_ERR_SYSTEM;
        return NULL;
    }

    size_t path_len = strlen(path);
    justin_pkg_target_list ret = (justin_pkg_target_list) malloc(sizeof(struct justin_pkg_target_list_t) + path_len + 1);
    if (ret == NULL) {
        *err = JUSTIN_ERR_NOMEM;
        closedir(dirent);
        return NULL;
    }
    ret->dirent = dirent;
    ret->dirent_open = true;

    char *path_box = (char*) (ret + sizeof(struct justin_pkg_target_list_t));
    memcpy(path_box, path, path_len);
    path_box[path_len] = '\0';
    ret->dir = path_box;

    char **cache = (char**) calloc(4, sizeof(char*));
    if (cache == NULL) {
        *err = JUSTIN_ERR_NOMEM;
        free(ret);
        closedir(dirent);
        return NULL;
    }
    ret->cache = cache;
    ret->cache_capacity = 4;
    ret->cache_len = 0;
    ret->selection = NULL;
    ret->selection_len = 0;

    return ret;
}

void justin_pkg_target_list_destroy(justin_pkg_target_list tl) {
    free(tl->cache);
    if (tl->selection != NULL) free(tl->selection);
    if (tl->dirent_open) {
        closedir(tl->dirent);
    }
    free(tl);
}

void justin_pkg_target_list_insert(justin_pkg_target_list tl, char *name, justin_err *err) {
    size_t cap = tl->cache_capacity;
    size_t len = tl->cache_len;
    char **cache = tl->cache;
    if (len == cap) {
        cap <<= 1;
        cache = (char**) reallocarray(cache, cap, sizeof(char*));
        if (cache == NULL) {
            *err = JUSTIN_ERR_NOMEM;
            return;
        }
        tl->cache_capacity = cap;
    }
    cache[len] = name;
    tl->cache_len = len + 1;
}

const char* justin_pkg_target_list_next(justin_pkg_target_list tl, justin_err *err) {
    *err = JUSTIN_ERR_OK;
    if (tl->dirent_open) {
        errno = 0;
        struct dirent *ent;
        char *name;
        char *in;
        while ((ent = readdir(tl->dirent)) != NULL) {
            if (ent->d_type != DT_REG) continue;
            name = ent->d_name;
            in = strcasestr(name, PKG_EXT_S);
            if (in == NULL) continue;
            if (in[PKG_EXT_L] != '\0') continue; // ends with
            justin_pkg_target_list_insert(tl, name, err);
            return name;
        }
        if (errno != 0) {
            *err = JUSTIN_ERR_SYSTEM;
        }
        tl->dirent_open = false;
        if (closedir(tl->dirent) != 0) *err = JUSTIN_ERR_SYSTEM;
    }
    return NULL;
}

size_t justin_pkg_target_list_size(justin_pkg_target_list tl) {
    return tl->cache_len;
}

const char* justin_pkg_target_list_get(justin_pkg_target_list tl, size_t index) {
    return tl->cache[index];
}

bool justin_pkg_target_list_init_selection(justin_pkg_target_list tl, justin_err *err) {
    *err = JUSTIN_ERR_OK;
    size_t required = ((tl->cache_len - 1) >> 3) + 1;
    size_t cur = tl->selection_len;
    if (cur >= required) return false;
    uint8_t *selection;
    if (cur == 0) {
        selection = (uint8_t*) calloc(required, sizeof(uint8_t));
    } else {
        selection = (uint8_t*) reallocarray(tl->selection, required, sizeof(uint8_t));
    }
    if (selection == NULL) {
        *err = JUSTIN_ERR_NOMEM;
        return true;
    }
    if (cur != 0) memset(&selection[cur], 0, sizeof(uint8_t) * (required - cur));
    tl->selection = selection;
    tl->selection_len = required;
    return false;
}

void justin_pkg_target_list_select(justin_pkg_target_list tl, size_t index, justin_err *err) {
    if (justin_pkg_target_list_init_selection(tl, err)) return;
    if (index < 0 || index >= tl->cache_len) {
        *err = JUSTIN_ERR_ARGS;
        return;
    }
    uint8_t flag = (uint8_t) (1 << (index & 7));
    index >>= 3;
    tl->selection[index] |= flag;
}

void justin_pkg_target_list_select_all(justin_pkg_target_list tl, justin_err *err) {
    if (justin_pkg_target_list_init_selection(tl, err)) return;
    memset(tl->selection, 0xFF, sizeof(uint8_t) * tl->selection_len);
}

bool justin_pkg_target_list_is_selected(justin_pkg_target_list tl, size_t index) {
    if (tl->selection_len == 0) return false;
    if (index < 0 || index >= tl->cache_len) return false;
    uint8_t flag = (uint8_t) (1 << (index & 7));
    index >>= 3;
    return (tl->selection[index] & flag) != 0;
}

void justin_pkg_target_list_install(justin_context ctx, justin_pkg_target_list tl, justin_err *err) {
    *err = JUSTIN_ERR_OK;

    const char *dir = tl->dir;
    size_t dir_len = strlen(dir);

    size_t cat_buf_s = dir_len + 32L;
    char *cat_buf = (char*) malloc(cat_buf_s);
    if (cat_buf == NULL) {
        *err = JUSTIN_ERR_NOMEM;
        return;
    }
    size_t cat_buf_s_needed;

    size_t tls = justin_pkg_target_list_size(tl);
    const char *target;
    size_t target_len;
    for (size_t i=0; i < tls; i++) {
        if (!justin_pkg_target_list_is_selected(tl, i)) continue;
        target = justin_pkg_target_list_get(tl, i);
        target_len = strlen(target);

        cat_buf_s_needed = dir_len + target_len + 2;
        if (cat_buf_s < cat_buf_s_needed) {
            cat_buf = (char*) realloc(cat_buf, cat_buf_s_needed);
            if (cat_buf == NULL) {
                *err = JUSTIN_ERR_NOMEM;
                return;
            }
            cat_buf_s = cat_buf_s_needed;
        }
        justin_util_path_join(dir, dir_len, target, target_len, cat_buf);
        justin_log_debug_indent(cat_buf, 1);
        justin_pkg_install(ctx, cat_buf, err);
        if ((*err) != JUSTIN_ERR_OK) {
            free(cat_buf);
            return;
        }
    }
    free(cat_buf);
}
