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

#include <stdint.h>
#include "logging.h"

#ifndef JUSTIN_STORAGE_H
#define JUSTIN_STORAGE_H

struct justin_storage_lockfile {
    uint8_t counter;
    uint64_t lock[4];
};
typedef struct justin_storage_lockfile* justin_storage_lockfile;

struct justin_storage {
    uid_t user;
    const char *path;
    int lockfile_handle;
    justin_storage_lockfile lockfile;
    uint8_t pid;
    uint8_t pid_page;
};
typedef struct justin_storage* justin_storage;

justin_storage justin_storage_init(uid_t user);

void justin_storage_destroy(justin_storage storage);

justin_err justin_storage_lock(justin_storage storage);

justin_err justin_storage_unlock(justin_storage storage);

const char* justin_storage_dir_create(justin_storage storage, justin_err *err);

#endif //JUSTIN_STORAGE_H
