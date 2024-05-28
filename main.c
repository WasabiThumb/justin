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

#include <curl/curl.h>
#include <git2.h>
#include <alpm.h>
#include "src/ansi.h"
#include "src/params.h"
#include "src/storage.h"
#include "src/logging.h"
#include "src/context.h"
#include "src/util.h"
#include "src/ctx/aur.h"
#include "src/ctx/repo.h"
#include "src/ctx/pkg.h"

void display_help() {
    fprintf(stderr, "\n%sUsage%s: justin %s<target> %s[flags]%s\n", BWHT, WHT, CYN, MAG, CRESET);
    fprintf(stderr, "%starget %s:: %sPackage to install%s\n", CYN, BWHT, WHT, CRESET);
    fprintf(stderr, "%s-l     %s:: %sAlways install the latest version%s\n", MAG, BWHT, WHT, CRESET);
    fprintf(stderr, "%s-y     %s:: %sAccept prompts by default%s\n", MAG, BWHT, WHT, CRESET);
    fprintf(stderr, "\n");
}

bool pkg_list_print(justin_context ctx, justin_aur_project_t *project, int64_t index, char **lb, size_t *ls) {
    char* buf = *lb;
    size_t required = strlen(project->name) +
            25 + // num votes
            12 + // (INSTALLED)
            29; // escape sequences & null terminator

    if ((*ls) < required) {
        buf = realloc(buf, required);
        if (buf == NULL) return false;
        *ls = required;
        *lb = buf;
    }

    sprintf(buf, "%s[%s%ld%s] %s%s %s+%d %s", CYN, BYEL, index, CYN, BWHT, project->name, BGRN, project->votes, alpm_db_get_pkg(ctx->alpm_db, project->name) != NULL ? "(INSTALLED)" : "");
    justin_log_info_indent(buf, 1);
    justin_log_info_indent(project->description, 2);

    return true;
}

justin_err install_package(justin_context ctx, justin_aur_project_t *project) {
    justin_err err = JUSTIN_ERR_OK;
    if ((!ctx->params->f_yes) && alpm_db_get_pkg(ctx->alpm_db, project->name) != NULL) {
        justin_log_warn("This package is already installed. Reinstall? (y/N)");
        char sel;
        scanf(" %c", &sel);
        if (!(sel == 'y' || sel == 'Y')) {
            justin_log_info("Installation cancelled");
            return err;
        }
    }
    justin_log_debug("Locking storage");
    err = justin_storage_lock(ctx->storage);
    if (err != JUSTIN_ERR_OK) return err;

    justin_log_debug("Creating temp dir");
    const char *dir = justin_storage_dir_create(ctx->storage, &err);
    if (err != JUSTIN_ERR_OK) goto ex;

    justin_log_info("Cloning...");
    git_repository *repo = justin_aur_project_clone_into(ctx, project, dir, &err);
    if (err != JUSTIN_ERR_OK) goto ex_b;

    if (!ctx->params->f_latest) {
        justin_log_debug("Creating commit list");
        justin_repo_commit_list commits = justin_repo_commit_list_create(repo, &err);
        if (err != JUSTIN_ERR_OK) goto ex_c;

        justin_repo_commit_list_goto(commits, 0);
        justin_repo_commit_list_entry entry = JUSTIN_REPO_COMMIT_LIST_ENTRY_INITIALIZER;
        if (!justin_repo_commit_list_next(commits, &entry, &err)) {
            justin_log_err_msg(JUSTIN_ERR_ASSERTION, "Commit list has no first element(?) Try running again with -l");
            if (err == JUSTIN_ERR_OK) err = JUSTIN_ERR_ASSERTION;
            justin_repo_commit_list_free(commits);
            goto ex_c;
        }

        justin_log_debug("Opening commit list prompt");
        entry = justin_repo_commit_list_prompt(commits, 0, &err);
        if (err != JUSTIN_ERR_OK) {
            justin_repo_commit_list_free(commits);
            goto ex_c;
        }

        justin_log_info("Rolling back");
        int reset_stat = git_reset(repo, (const git_object *) entry.commit, GIT_RESET_HARD, NULL);
        justin_repo_commit_list_free(commits);
        if (reset_stat != 0) {
            err = JUSTIN_ERR_GIT;
            goto ex_c;
        }
    }

    justin_log_info("Running makepkg");
    justin_pkg_make(ctx->storage->user, dir, &err);
    if (err != JUSTIN_ERR_OK) goto ex_c;

    justin_log_debug("Building target list");
    justin_pkg_target_list targets = justin_pkg_target_list_create(dir, &err);
    if (err != JUSTIN_ERR_OK) goto ex_c;

    justin_log_info("Identified targets:");
    const char *target;
    size_t counter = 0;
    char cbuf[256];
    while ((target = justin_pkg_target_list_next(targets, &err)) != NULL) {
        sprintf(cbuf, "%s[%s%ld%s]%s %.200s", CYN, BYEL, ++counter, CYN, BWHT, target);
        justin_log_info_indent(cbuf, 1);
    }
    if (err != JUSTIN_ERR_OK) goto ex_d;

    if (counter == 0) {
        justin_log_warn("No targets found!");
        err = JUSTIN_ERR_ASSERTION;
        goto ex_d;
    }

    bool install_all = counter == 1 || ctx->params->f_yes;
    if (!install_all) {
        justin_log_info("Install all targets (Y/n)? ");
        char sel;
        scanf(" %c", &sel);
        install_all = !(sel == 'n' || sel == 'N');
    }
    if (install_all) {
        justin_pkg_target_list_select_all(targets, &err);
    } else {
        justin_log_info("Enter the targets to install (e.g. 1, 2, 5-7):");
        scanf("%255s", cbuf);
        justin_util_iset iset = justin_util_iset_parse(cbuf, &err);
        if (err != JUSTIN_ERR_OK) goto ex_d;

        for (size_t i=0; i < counter; i++) {
            if (justin_util_iset_contains(iset, (int) (i + 1))) {
                justin_pkg_target_list_select(targets, i, &err);
            }
        }
        justin_util_iset_destroy(iset);
    }
    if (err != JUSTIN_ERR_OK) goto ex_d;

    justin_log_info("Installing targets");
    justin_pkg_target_list_install(ctx, targets, &err);
    if (err != JUSTIN_ERR_OK) goto ex_d;
    justin_log_info("Success!");

    ex_d:
    justin_pkg_target_list_destroy(targets);
    ex_c:
    git_repository_free(repo);
    ex_b:
    free((void*) dir);
    ex:
    justin_log_debug("Unlocking storage");
    justin_err unlock_err = justin_storage_unlock(ctx->storage);
    if (err == 0) err = unlock_err;
    return err;
}

// Search for package, then install it
int search_package(justin_context ctx) {
    justin_err err = JUSTIN_ERR_OK;

    justin_log_info_indent("Searching for packages", 1);
    justin_aur_project_list pl = justin_aur_search(ctx, justin_params_get_target(ctx->params), &err);
    if (err != JUSTIN_ERR_OK) {
        justin_log_err_msg(err, "Failed to execute AUR search");
        return 1;
    }
    if (pl->size == 0) {
        justin_log_warn("No results found");
        return 0;
    }
    justin_log_debug_indent("Sorting package list by popularity", 1);
    justin_aur_project_list_sort_popularity(pl);

    size_t ls = 64;
    char* lb = (char*) malloc(ls);
    if (lb == NULL) {
        justin_log_err_msg(JUSTIN_ERR_NOMEM, "Cannot allocate line buffer");
        return 1;
    }
    justin_aur_project_list head = pl;
    int64_t index = pl->size;
    do {
        pkg_list_print(ctx, &head->value, index, &lb, &ls);
        head = head->next;
        index--;
    } while (head != NULL);

    justin_log_info("Enter a selection: ");
    int sel;
    scanf("%d", &sel); // NOLINT(cert-err34-c)

    if (sel < 1 || sel > pl->size) {
        justin_log_warn("Invalid entry");
        justin_aur_project_list_free(pl);
        free(lb);
        return 1;
    }

    justin_aur_project_t *project = NULL;
    head = pl;
    index = pl->size;
    do {
        if (index == sel) {
            project = &head->value;
            break;
        }
        head = head->next;
        index--;
    } while (head != NULL);
    if (project == NULL) {
        justin_log_err_msg(JUSTIN_ERR_ASSERTION, "Entry no longer valid");
        justin_aur_project_list_free(pl);
        free(lb);
        return 1;
    }

    sprintf(lb, "Installing %s%s", BYEL, project->name);
    justin_log_info(lb);
    free(lb);

    err = install_package(ctx, project);
    justin_aur_project_list_free(pl);
    if (err != JUSTIN_ERR_OK) {
        justin_log_err_msg(err, "Failed to install package");
        return 1;
    }
    return 0;
}

// Build the context and pass it to search_package(ctx), then destroy the context
int main(int argc, char *argv[]) {
    int app_err = 0;

    justin_log_debug("Reading launch parameters");
    justin_params params = justin_params_create(argc, argv);
    bool done;
    do {
        done = justin_params_read(params);
    } while (done);

    const char* params_err = justin_params_strerror(params);
    if (params_err != NULL) {
        justin_log_err_msg(JUSTIN_ERR_ARGS, params_err);
        justin_params_destroy(params);
        justin_log_warn("Displaying help");
        display_help();
        return 1;
    }

    __uid_t uid = getuid();
    if (uid == 0) {
        __uid_t user = params->v_uid;
        if (user == 0) {
            justin_log_err_msg(JUSTIN_ERR_ASSERTION, "Running as root, but no UID is set\n");
            justin_params_destroy(params);
            return 1;
        }
    } else {
        justin_params_destroy(params);
        justin_log_debug("Restarting with sudo");
        return justin_util_sudo(argc, argv, uid);
    }

    justin_log_hello();

    justin_log_debug("Initializing libalpm");
    alpm_errno_t alpm_err;
    struct _alpm_handle_t *alpm = alpm_initialize("/", "/var/lib/pacman", &alpm_err);
    if (alpm == NULL) {
        justin_log_err_msg(JUSTIN_ERR_LINK, alpm_strerror(alpm_err));
        app_err = 1;
        goto exit_d;
    }
    alpm_db_t *db = alpm_get_localdb(alpm);

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
    justin_storage storage = justin_storage_init(params->v_uid);
    if (storage == NULL) {
        justin_log_err(JUSTIN_ERR_SYSTEM);
        app_err = 1;
        goto exit_b;
    }

    justin_context ctx;
    if (justin_context_create(&ctx, params, db, curl, storage)) {
        justin_log_debug("Created context");
        app_err = search_package(ctx);
        justin_context_destroy(ctx);
    } else {
        justin_log_err(JUSTIN_ERR_NOMEM);
    }

    justin_log_debug("Cleaning up storage");
    justin_storage_destroy(storage);
    exit_b:
    justin_log_debug("Cleaning up cURL");
    curl_easy_cleanup(curl);
    exit_c:
    justin_log_debug("Cleaning up libgit");
    git_libgit2_shutdown();
    justin_log_debug("Cleaning up libalpm");
    alpm_release(alpm);
    exit_d:

    justin_params_destroy(params);
    printf("\n");
    return app_err;
}
