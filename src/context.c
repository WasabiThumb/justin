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
