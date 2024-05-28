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
#include <git2.h>
#include "../context.h"
#include "../logging.h"

#ifndef JUSTIN_AUR_H
#define JUSTIN_AUR_H

typedef struct justin_aur_project_t {
    const char *name;
    const char *version;
    const char *description;
    int votes;
    float popularity;
} justin_aur_project_t;

typedef struct justin_aur_project_list_t {
    int64_t size;
    justin_aur_project_t value;
    struct justin_aur_project_list_t *next;
} justin_aur_project_list_t;

typedef justin_aur_project_list_t *justin_aur_project_list;

//

void justin_aur_project_list_free(justin_aur_project_list list);

void justin_aur_project_list_sort_popularity(justin_aur_project_list list);

justin_aur_project_list justin_aur_search(justin_context ctx, const char *term, justin_err *err);

git_repository *justin_aur_project_clone_into(justin_context ctx, justin_aur_project_t *project, const char *path, justin_err *err);

#endif //JUSTIN_AUR_H
