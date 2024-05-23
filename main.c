#include <curl/curl.h>
#include <git2.h>
#include "src/storage.h"
#include "src/logging.h"

int main(int argc, char *argv[]) {
    int app_err = 0;
    justin_log_hello();

    justin_log_debug("Initializing libgit");
    git_libgit2_init();

    justin_log_debug("Initializing cURL");
    CURL *curl = curl_easy_init();
    if (curl == NULL) {
        justin_log_err(JUSTIN_ERR_LINK);
        app_err = 1;
        goto exit_c;
    }

    justin_log_debug("Initializing storage");
    justin_storage storage = justin_storage_init();
    if (storage == NULL) {
        justin_log_err(JUSTIN_ERR_SYSTEM);
        app_err = 1;
        goto exit_b;
    }

    justin_log_debug("Locking storage");
    app_err = justin_storage_lock(storage);
    if (app_err != 0) {
        justin_log_err_msg(app_err, "Failed to lock storage");
        goto exit;
    }

    //

    justin_log_debug("Unlocking storage");
    app_err = justin_storage_unlock(storage);
    if (app_err != 0) {
        justin_log_err_msg(app_err, "Failed to lock storage");
        goto exit;
    }

    exit:
    justin_log_debug("Cleaning up storage");
    justin_storage_destroy(storage);
    exit_b:
    justin_log_debug("Cleaning up cURL");
    curl_easy_cleanup(curl);
    exit_c:
    justin_log_debug("Cleaning up libgit");
    git_libgit2_shutdown();
    return app_err;
}
