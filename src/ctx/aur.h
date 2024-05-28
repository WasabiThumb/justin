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
