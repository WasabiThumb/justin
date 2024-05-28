#include "context.h"

bool justin_context_create(justin_context *out, justin_params params, alpm_db_t *alpm_db, CURL *curl, justin_storage storage) {
    justin_context ctx = (justin_context) malloc(sizeof(struct justin_context));
    if (ctx == NULL) return false;

    ctx->params = params;
    ctx->alpm_db = alpm_db;
    ctx->curl = curl;
    ctx->storage = storage;
    *out = ctx;
    return true;
}
