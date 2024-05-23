#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include "ansi.h"
#include "version.h"
#include "logging.h"

// Errors
static char SYSTEM_ERR_BUF[256] = "System error: ";
static char UNKNOWN_ERR_BUF[35] = "Unknown error (0x0000000000000000)";

static const char* MSG_NOMEM = "Out of memory";
static const char* MSG_LOCKFILE_FULL = "Lockfile is at capacity";
static const char* MSG_LINK = "Linkage error";

const char* justin_err_str(justin_err err) {
    if (justin_err_is_system(err)) {
        const char* base = strerror((int) (err ^ JUSTIN_ERR_FLAG_SYSTEM));
        size_t base_len = strlen(base);
        bool overflow = (base_len > 241);
        if (overflow) {
            base_len = 241;
        }
        memcpy(&SYSTEM_ERR_BUF[14], base, base_len); // NOLINT(bugprone-not-null-terminated-result)
        SYSTEM_ERR_BUF[14 + base_len] = (char) 0;
        if (overflow) {
            for (int i=1; i < 4; i++) SYSTEM_ERR_BUF[14 + base_len - i] = '.';
        }
        return SYSTEM_ERR_BUF;
    }
    switch (err) {
        case JUSTIN_ERR_NOMEM:
            return MSG_NOMEM;
        case JUSTIN_ERR_LOCKFILE_FULL:
            return MSG_LOCKFILE_FULL;
        case JUSTIN_ERR_LINK:
            return MSG_LINK;
        default:
            sprintf(&UNKNOWN_ERR_BUF[17], "%lX)", err);
            return UNKNOWN_ERR_BUF;
    }
}

// Logging
static const char *LOG_FMT = "%s--[%s%s%s]%s %s%s%s\n";

#define APP_NAME "justin"
static const char *APP_NAME_S = APP_NAME;
#define APP_NAME_L ((sizeof APP_NAME) - 1)

#define LOG_ERR_S1 "Error at "
static const char *LOG_ERR_S1_S = LOG_ERR_S1;
#define LOG_ERR_S1_L ((sizeof LOG_ERR_S1) - 1)

#define LOG_ERR_S2 "Caused by: "
static const char *LOG_ERR_S2_S = LOG_ERR_S2;
#define LOG_ERR_S2_L ((sizeof LOG_ERR_S2) - 1)

#define GEN_INDENT int size = (indent << 1); char* indentation = alloca(size + 1); memset(indentation, (int) '-', size); indentation[size] = (char) 0

#ifndef NDEBUG
static const char *PREFIX_DBG = "DBG";
void justin_log_debug(const char* msg) {
    fprintf(stderr, LOG_FMT, CYN, BBLU, PREFIX_DBG, CYN, "", BLU, msg, CRESET);
}

void justin_log_debug_indent(const char* msg, uint8_t indent) {
    GEN_INDENT;
    fprintf(stderr, LOG_FMT, CYN, BBLU, PREFIX_DBG, CYN, indentation, BLU, msg, CRESET);
}
#endif

static const char *PREFIX_INFO = "NFO";
void justin_log_info(const char* msg) {
    fprintf(stderr, LOG_FMT, CYN, BWHT, PREFIX_INFO, CYN, "", WHT, msg, CRESET);
}

void justin_log_info_indent(const char* msg, uint8_t indent) {
    GEN_INDENT;
    fprintf(stderr, LOG_FMT, CYN, BWHT, PREFIX_INFO, CYN, indentation, WHT, msg, CRESET);
}

void justin_log_hello() {
    char* text = alloca(APP_NAME_L + 1 + justin_get_version_bytes());
    memcpy(text, APP_NAME_S, APP_NAME_L);
    memcpy(&text[APP_NAME_L + 1], justin_get_version(), justin_get_version_bytes());
    text[APP_NAME_L] = ' ';
    justin_log_info(text);
}

static const char *PREFIX_WARN = "WRN";
void justin_log_warn(const char* msg) {
    fprintf(stderr, LOG_FMT, CYN, BYEL, PREFIX_WARN, CYN, "", YEL, msg, CRESET);
}

static const char *PREFIX_ERR = "ERR";
void justin_log_err_msg_at(justin_err err, const char *restrict msg, const char *restrict file, int line) {
    if (justin_err_is_system(err) && ((err ^ JUSTIN_ERR_FLAG_SYSTEM) == 0L)) return;
    const char* err_str = justin_err_str(err);
    size_t err_str_len = strlen(err_str);

    size_t file_len = strlen(file);
    bool msg_nn = msg != NULL;
    size_t msg_len = msg_nn ? strlen(msg) : 0;

    size_t l1_len = LOG_ERR_S1_L + file_len + 13 + (msg_nn ? (2 + msg_len) : 0);
    size_t l2_len = LOG_ERR_S2_L + err_str_len + 1;
    char* buf = malloc(l1_len + l2_len);
    if (buf == NULL) {
        fprintf(stderr, "%s\n", MSG_NOMEM);
        return;
    }
    char* l2buf = &buf[l1_len];

    int w = sprintf(buf, "%s%s#%d", LOG_ERR_S1_S, file, line);
    if (msg_nn) sprintf(&buf[w], ": %s", msg);
    sprintf(l2buf, "%s%s", LOG_ERR_S2_S, err_str);

    fprintf(stderr, LOG_FMT, CYN, BRED, PREFIX_ERR, CYN, "", RED, buf, CRESET);
    fprintf(stderr, LOG_FMT, CYN, BRED, PREFIX_ERR, CYN, "--", RED, l2buf, CRESET);
    free(buf);
}
