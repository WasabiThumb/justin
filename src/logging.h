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
#include <stdbool.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#ifndef JUSTIN_LOGGING_H
#define JUSTIN_LOGGING_H

// Errors
typedef int64_t justin_err;
#define JUSTIN_ERR_OK 0L
#define JUSTIN_ERR_NOMEM 1L
#define JUSTIN_ERR_LOCKFILE_FULL 2L
#define JUSTIN_ERR_LINK 3L
#define JUSTIN_ERR_ARGS 4L
#define JUSTIN_ERR_ASSERTION 5L
#define JUSTIN_ERR_GIT 6L
#define JUSTIN_ERR_FLAG_SYSTEM (0b1L << (sizeof(int) * 8))
#define JUSTIN_ERR_SYSTEM (errno | JUSTIN_ERR_FLAG_SYSTEM)
#define JUSTIN_ERR_FLAG_CURL (0b10L << (sizeof(int) * 8))
#define JUSTIN_ERR_CURL(e) (((int64_t) (e)) | JUSTIN_ERR_FLAG_CURL)
#define JUSTIN_ERR_FLAG_SUBPROC (0b100L << (sizeof(int) * 8))
#define JUSTIN_ERR_SUBPROC(e) (((int64_t) (e)) | JUSTIN_ERR_FLAG_SUBPROC)
#define JUSTIN_ERR_FLAG_ALPM (0b1000L << (sizeof(int) * 8))
#define JUSTIN_ERR_ALPM_X(handle) (((int64_t) alpm_errno(handle)) | JUSTIN_ERR_FLAG_ALPM)
#define JUSTIN_ERR_ALPM JUSTIN_ERR_ALPM_X(handle)

static inline bool justin_err_is_system(justin_err err) {
    return (err & JUSTIN_ERR_FLAG_SYSTEM) != 0;
}

static inline bool justin_err_is_curl(justin_err err) {
    return (err & JUSTIN_ERR_FLAG_CURL) != 0;
}

static inline bool justin_err_is_subproc(justin_err err) {
    return (err & JUSTIN_ERR_FLAG_SUBPROC) != 0;
}

static inline bool justin_err_is_alpm(justin_err err) {
    return (err & JUSTIN_ERR_FLAG_ALPM) != 0;
}

const char* justin_err_str(justin_err err);

// Logging

#ifndef NDEBUG
void justin_log_debug(const char* msg);
void justin_log_debug_indent(const char* msg, uint8_t indent);
#else
#define justin_log_debug(msg)
#define justin_log_debug_indent(msg, indent)
#endif

void justin_log_info(const char* msg);
void justin_log_info_indent(const char* msg, uint8_t indent);

void justin_log_hello();

void justin_log_warn(const char* msg);
#define justin_log_err_soft(err) justin_log_warn(justin_err_str(err))

void justin_log_err_msg_at(justin_err err, const char *restrict msg, const char *restrict file, int line);
#define justin_log_err_at(err, file, line) justin_log_err_msg_at(err, NULL, file, line)
#define justin_log_err(err) justin_log_err_at(err, __FILE__, __LINE__)
#define justin_log_err_msg(err, msg) justin_log_err_msg_at(err, msg, __FILE__, __LINE__)

static inline void* justin_malloc_msg(size_t size, const char* msg) {
    void *ret = malloc(size);
    if (ret == NULL) {
        justin_log_err_msg(JUSTIN_ERR_NOMEM, msg);
        exit(1);
    }
    return ret;
}
#define justin_malloc(size) justin_malloc_msg(size, NULL)

#endif //JUSTIN_LOGGING_H
