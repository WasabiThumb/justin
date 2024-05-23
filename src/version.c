#include "version.h"

static const char *VERSION = JUSTIN_VERSION;
inline const char* justin_get_version() {
    return VERSION;
}

inline size_t justin_get_version_bytes() {
    return (sizeof JUSTIN_VERSION);
}
