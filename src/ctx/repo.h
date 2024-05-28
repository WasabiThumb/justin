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

#include <git2.h>
#include <stdbool.h>
#include "../logging.h"

#ifndef JUSTIN_REPO_H
#define JUSTIN_REPO_H

typedef struct justin_repo_commit_list_entry {
    git_commit *commit;
    const char *message;
    size_t index;
} justin_repo_commit_list_entry;
#define JUSTIN_REPO_COMMIT_LIST_ENTRY_INITIALIZER { NULL, NULL, -1 }

typedef struct justin_repo_commit_list_t {
    git_repository *repo;
    git_reference *head;
    git_oid oid;
    git_revwalk *walker;
    git_commit **cache;
    size_t cache_capacity;
    size_t cache_len;
    size_t traversal_head;
    bool iter_over;
} justin_repo_commit_list_t;

typedef justin_repo_commit_list_t *justin_repo_commit_list;

//

justin_repo_commit_list justin_repo_commit_list_create(git_repository *repo, justin_err *err);

bool justin_repo_commit_list_next(justin_repo_commit_list list, justin_repo_commit_list_entry *entry, justin_err *err);

void justin_repo_commit_list_goto(justin_repo_commit_list list, size_t skip);

void justin_repo_commit_list_free(justin_repo_commit_list list);

justin_repo_commit_list_entry justin_repo_commit_list_prompt(justin_repo_commit_list list, size_t start, justin_err *err);

#endif //JUSTIN_REPO_H
