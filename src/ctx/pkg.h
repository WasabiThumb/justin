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

#include <stdbool.h>
#include "../logging.h"
#include "../context.h"

#ifndef JUSTIN_PKG_H
#define JUSTIN_PKG_H

void justin_pkg_make(uid_t user, const char *path, justin_err *err);

void justin_pkg_install(justin_context ctx, const char *file, justin_err *err);

struct justin_pkg_target_list_t;
typedef struct justin_pkg_target_list_t *justin_pkg_target_list;

justin_pkg_target_list justin_pkg_target_list_create(const char *path, justin_err *err);

void justin_pkg_target_list_destroy(justin_pkg_target_list tl);

const char* justin_pkg_target_list_next(justin_pkg_target_list tl, justin_err *err);

size_t justin_pkg_target_list_size(justin_pkg_target_list tl);

const char* justin_pkg_target_list_get(justin_pkg_target_list tl, size_t index);

void justin_pkg_target_list_select(justin_pkg_target_list tl, size_t index, justin_err *err);

void justin_pkg_target_list_select_all(justin_pkg_target_list tl, justin_err *err);

bool justin_pkg_target_list_is_selected(justin_pkg_target_list tl, size_t index);

void justin_pkg_target_list_install(justin_context ctx, justin_pkg_target_list tl, justin_err *err);

#endif //JUSTIN_PKG_H
