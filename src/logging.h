#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#ifndef JUSTIN_LOGGING_H
#define JUSTIN_LOGGING_H

// Errors
typedef int64_t justin_err;
#define JUSTIN_ERR_NOMEM 1L
#define JUSTIN_ERR_LOCKFILE_FULL 2L
#define JUSTIN_ERR_LINK 3L
#define JUSTIN_ERR_FLAG_SYSTEM (0b01L << (sizeof(int) * 8))
#define JUSTIN_ERR_SYSTEM (errno | JUSTIN_ERR_FLAG_SYSTEM)

static inline bool justin_err_is_system(justin_err err) {
    return (err & JUSTIN_ERR_FLAG_SYSTEM) != 0;
}

const char* justin_err_str(justin_err err);

// Logging
#ifndef __FILENAME__
#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#endif

#ifndef NDEBUG
void justin_log_debug(const char* msg);
void justin_log_debug_indent(const char* msg, uint8_t indent);
#else
#define justin_log_debug(msg)
#define justin_log_debug_indent(msg)
#endif

void justin_log_info(const char* msg);
void justin_log_info_indent(const char* msg, uint8_t indent);

void justin_log_hello();

void justin_log_warn(const char* msg);
#define justin_log_err_soft(err) justin_log_warn(justin_err_str(err))

void justin_log_err_msg_at(justin_err err, const char *restrict msg, const char *restrict file, int line);
#define justin_log_err_at(err, file, line) justin_log_err_msg_at(err, NULL, file, line)
#define justin_log_err(err) justin_log_err_at(err, __FILENAME__, __LINE__)
#define justin_log_err_msg(err, msg) justin_log_err_msg_at(err, msg, __FILENAME__, __LINE__)

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
