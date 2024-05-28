#include <stdlib.h>
#include <stdio.h>
#include "util.h"
#include "logging.h"
#include "params.h"

justin_params justin_params_create(int argc, char **argv) {
    justin_params ret = (justin_params) justin_malloc(sizeof(struct justin_params));
    ret->argc = argc;
    ret->argv = argv;
    ret->head = 1;
    ret->err = JUSTIN_PARAMS_ERR_NO_TARGET;
    //
    ret->v_target_start = 0;
    ret->v_target_end = 0;
    ret->v_target = NULL;
    ret->f_latest = false;
    ret->f_yes = false;
    ret->v_uid = 0;
    //
    return ret;
}

void justin_params_destroy(justin_params params) {
    if (params->v_target != NULL) free(params->v_target);
    free(params);
}

bool justin_params_read(justin_params params) {
    if (params->head >= params->argc) return false;
    int pos = params->head++;
    char* str = params->argv[pos];
    size_t str_len = strlen(str);
    if (str_len < 1) return true;

    if (str[0] == '-') {
        if (str_len < 2) return true;
        switch (str[1]) {
            case 'l':
                params->f_latest = true;
                break;
            case 'y':
                params->f_yes = true;
                break;
            case 'u': {
                size_t rem = str_len - 2;
                if (rem != (sizeof(__uid_t) << 1)) {
                    params->err = JUSTIN_PARAMS_ERR_FLAG_BAD_VALUE;
                    return false;
                }
                uid_t uid = 0;
                justin_util_hex2b(&str[2], &uid);
                params->v_uid = uid;
                break;
            }
            default:
                params->err = JUSTIN_PARAMS_ERR_FLAG_UNKNOWN;
                return false;
        }
        return true;
    }

    if (params->err == JUSTIN_PARAMS_ERR_NO_TARGET) {
        params->v_target_start = pos;
        params->v_target_end = pos;
        params->err = JUSTIN_PARAMS_ERR_OK;
        return true;
    }

    if (params->v_target_end == (pos - 1)) {
        params->v_target_end = pos;
        return true;
    }

    params->err = JUSTIN_PARAMS_ERR_MIX_MATCH;
    return false;
}

const char* justin_params_strerror(justin_params params) {
    switch (params->err) {
        case JUSTIN_PARAMS_ERR_OK:
            return NULL;
        case JUSTIN_PARAMS_ERR_NO_TARGET:
            return "No target specified";
        case JUSTIN_PARAMS_ERR_FLAG_UNKNOWN:
            return "Unknown flag";
        case JUSTIN_PARAMS_ERR_FLAG_NO_VALUE:
            return "Value expected for flag";
        case JUSTIN_PARAMS_ERR_FLAG_BAD_VALUE:
            return "Bad value for flag";
        case JUSTIN_PARAMS_ERR_MIX_MATCH:
            return "Cannot mix positional arguments with flags";
    }
    return NULL;
}

const char* justin_params_get_target(justin_params params) {
    if (params->v_target != NULL) return params->v_target;

    int start = params->v_target_start;
    int end = params->v_target_end;
    if (end == start) return params->argv[start];

    size_t *lengths = alloca(sizeof(size_t) * (end - start + 1));
    size_t len = 0;
    for (int i=start; i <= end; i++) {
        len += (lengths[i - start] = strlen(params->argv[i])) + 1;
    }

    char* dest = (char*) malloc(len);
    if (dest == NULL) {
        justin_log_err(JUSTIN_ERR_NOMEM);
        return "";
    }

    size_t el_len;
    size_t head = 0;
    for (int i=start; i <= end; i++) {
        if (i > start) dest[head++] = ' ';
        el_len = lengths[i - start];
        memcpy(&dest[head], params->argv[i], el_len);
        head += el_len;
    }
    dest[len - 1] = (char) 0;

    params->v_target = dest;
    return dest;
}
