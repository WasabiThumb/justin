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
#include <stdint.h>

#ifndef JUSTIN_PARAMS_H
#define JUSTIN_PARAMS_H

typedef enum justin_params_err: uint_fast8_t {
    JUSTIN_PARAMS_ERR_OK,
    JUSTIN_PARAMS_ERR_NO_TARGET,
    JUSTIN_PARAMS_ERR_FLAG_UNKNOWN,
    JUSTIN_PARAMS_ERR_FLAG_NO_VALUE,
    JUSTIN_PARAMS_ERR_FLAG_BAD_VALUE,
    JUSTIN_PARAMS_ERR_MIX_MATCH
} justin_params_err;

struct justin_params {
    int argc;
    char **argv;
    int head;
    justin_params_err err;
    //
    int v_target_start;
    int v_target_end;
    char *v_target;
    bool f_latest;
    bool f_yes;
    __uid_t v_uid;
};
typedef struct justin_params* justin_params;

//

justin_params justin_params_create(int argc, char **argv);

void justin_params_destroy(justin_params params);

bool justin_params_read(justin_params params);

const char* justin_params_strerror(justin_params params);

const char* justin_params_get_target(justin_params params);

#endif //JUSTIN_PARAMS_H
