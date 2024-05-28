#include <alpm.h>
#include <curl/curl.h>
#include <stdbool.h>
#include <stdlib.h>
#include "params.h"
#include "storage.h"

#ifndef JUSTIN_CONTEXT_H
#define JUSTIN_CONTEXT_H

struct justin_context {
    justin_params params;
    alpm_db_t *alpm_db;
    CURL *curl;
    justin_storage storage;
};
typedef struct justin_context *justin_context;

bool justin_context_create(justin_context *out, justin_params params, alpm_db_t *alpm_db, CURL *curl, justin_storage storage);

#define justin_context_destroy(ctx) free(ctx)

#endif //JUSTIN_CONTEXT_H
