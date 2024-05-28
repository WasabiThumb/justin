#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include "../ansi.h"
#include "../util.h"
#include "repo.h"

//

justin_repo_commit_list justin_repo_commit_list_create(git_repository *repo, justin_err *err) {
    *err = JUSTIN_ERR_OK;

    git_reference *head;
    if (git_repository_head(&head, repo) != 0) {
        *err = JUSTIN_ERR_GIT;
        return NULL;
    }

    git_annotated_commit *commit;
    if (git_annotated_commit_from_ref(&commit, repo, head) != 0) {
        git_reference_free(head);
        *err = JUSTIN_ERR_GIT;
        return NULL;
    }

    const git_oid *oid = git_annotated_commit_id(commit);
    git_commit *un_commit;
    if (git_commit_lookup(&un_commit, repo, oid) != 0) {
        git_annotated_commit_free(commit);
        git_reference_free(head);
        *err = JUSTIN_ERR_GIT;
        return NULL;
    }
    git_annotated_commit_free(commit);
    oid = git_commit_id(un_commit);

    git_revwalk *walker;
    if (git_revwalk_new(&walker, repo) != 0) {
        git_commit_free(un_commit);
        git_reference_free(head);
        *err = JUSTIN_ERR_GIT;
        return NULL;
    }
    git_revwalk_sorting(walker, GIT_SORT_TOPOLOGICAL | GIT_SORT_TIME);
    if (git_revwalk_push(walker, oid) != 0) {
        git_revwalk_free(walker);
        git_commit_free(un_commit);
        git_reference_free(head);
        *err = JUSTIN_ERR_GIT;
        return NULL;
    }

    justin_repo_commit_list ret = (justin_repo_commit_list) malloc(sizeof(justin_repo_commit_list_t));
    if (ret == NULL) {
        git_revwalk_free(walker);
        git_commit_free(un_commit);
        git_reference_free(head);
        *err = JUSTIN_ERR_NOMEM;
        return NULL;
    }

    ret->repo = repo;
    ret->head = head;
    git_oid_cpy(&ret->oid, oid);
    git_revwalk_next(&ret->oid, walker);
    ret->walker = walker;
    ret->cache_capacity = 1;
    ret->cache_len = 1;
    ret->traversal_head = 0;
    ret->iter_over = false;

    git_commit **cache = (git_commit**) calloc(1, sizeof(git_commit*));
    if (cache == NULL) {
        git_revwalk_free(walker);
        git_commit_free(un_commit);
        git_reference_free(head);
        free(ret);
        *err = JUSTIN_ERR_NOMEM;
        return NULL;
    }

    cache[0] = un_commit;
    ret->cache = cache;
    return ret;
}

bool justin_repo_commit_list_next(justin_repo_commit_list list, justin_repo_commit_list_entry *entry, justin_err *err) {
    *err = JUSTIN_ERR_OK;
    git_commit *commit;
    if (list->traversal_head < list->cache_len) {
        commit = list->cache[list->traversal_head++];
    } else {
        if (list->iter_over) {
            return false;
        }
        int no = git_revwalk_next(&list->oid, list->walker);
        if (no == GIT_ITEROVER) {
            list->iter_over = true;
            return false;
        } else if (no != 0) {
            *err = JUSTIN_ERR_GIT;
            return false;
        }
        if (git_commit_lookup(&commit, list->repo, &list->oid) != 0) {
            *err = JUSTIN_ERR_GIT;
            return false;
        }
        // Insert commit into cache
        if (list->cache_len >= list->cache_capacity) {
            size_t cap = list->cache_capacity << 1;
            git_commit **n_cache = (git_commit**) reallocarray(list->cache, cap, sizeof(git_commit*));
            if (n_cache == NULL) {
                *err = JUSTIN_ERR_NOMEM;
                return false;
            }
            list->cache = n_cache;
            list->cache_capacity = cap;
        }
        list->cache[list->traversal_head++] = commit;
        list->cache_len = list->traversal_head;
    }
    entry->commit = commit;
    entry->message = git_commit_message(commit);
    entry->index = list->traversal_head;
    return true;
}

void justin_repo_commit_list_goto(justin_repo_commit_list list, size_t dest) {
    size_t max = list->cache_len;

    if (dest > max) {
        list->traversal_head = max;
        dest -= max;
        justin_repo_commit_list_entry entry = JUSTIN_REPO_COMMIT_LIST_ENTRY_INITIALIZER;
        justin_err err = JUSTIN_ERR_OK;
        while (dest > 0 && justin_repo_commit_list_next(list, &entry, &err)) dest--;
        if (err != JUSTIN_ERR_OK) justin_log_err_soft(err);
    } else {
        list->traversal_head = dest;
    }
}

void justin_repo_commit_list_free(justin_repo_commit_list list) {
    git_revwalk_free(list->walker);
    for (size_t i=0; i < list->cache_len; i++) git_commit_free(list->cache[i]);
    free(list->cache);
    git_reference_free(list->head);
    free(list);
}

#define LIST_PROMPT_SIZE 10
justin_repo_commit_list_entry justin_repo_commit_list_prompt(justin_repo_commit_list list, size_t start, justin_err *err) {
    // CHECK IF EMPTY BEFORE CALLING
    justin_log_info("Select a version to install");

    justin_repo_commit_list_entry entries[LIST_PROMPT_SIZE];
    size_t entry_count = 0;

    justin_repo_commit_list_goto(list, start);

    while (justin_repo_commit_list_next(list, &entries[entry_count], err)) {
        if ((++entry_count) == LIST_PROMPT_SIZE) break;
    }
    if (*err != JUSTIN_ERR_OK) return entries[0];

    char lbuf[256];
    for (int i=(LIST_PROMPT_SIZE - 1); i >= 0; i--) {
        if (i >= entry_count) continue;
        int head = sprintf(lbuf, "%s[%s%ld%s] %s", CYN, BYEL, entries[i].index, CYN, BWHT);
        if (head < 0) {
            *err = JUSTIN_ERR_ASSERTION;
            return entries[0];
        }
        const char* msg = entries[i].message;
        size_t msg_len = strlen(msg);
        char c;
        for (size_t q=0; q < msg_len; q++) {
            c = msg[q];
            if (c == '\n' || c == '\r') break;
            lbuf[head++] = c;
            if (head == 255) break;
        }
        lbuf[head] = (char) 0;
        justin_log_info_indent(lbuf, 1);
    }

    justin_log_info("Number, (S)earch, (N)ext or (P)revious: ");
    scanf("%255s", lbuf);

    switch (lbuf[0]) {
        case '0': case '1': case '2':
        case '3': case '4': case '5':
        case '6': case '7': case '8':
        case '9': {
            errno = 0;
            long dest = strtol(lbuf, NULL, 10);
            if (errno == EINVAL) {
                *err = JUSTIN_ERR_ARGS;
                return entries[0];
            }
            justin_repo_commit_list_goto(list, (size_t) ((dest - 1) & LONG_MAX));
            justin_repo_commit_list_entry entry = JUSTIN_REPO_COMMIT_LIST_ENTRY_INITIALIZER;
            if (justin_repo_commit_list_next(list, &entry, err)) {
                return entry;
            }
            if ((*err) == JUSTIN_ERR_OK) *err = JUSTIN_ERR_ARGS;
            return entries[0];
        }
        case 's': case 'S': {
            justin_log_info("Enter search term:");
            scanf("%255s", lbuf);
            int inp_len = (int) strlen(lbuf);

            justin_repo_commit_list_goto(list, 0);
            justin_repo_commit_list_entry entry = JUSTIN_REPO_COMMIT_LIST_ENTRY_INITIALIZER;
            int dist;
            justin_repo_commit_list_entry min = JUSTIN_REPO_COMMIT_LIST_ENTRY_INITIALIZER;
            int min_dist = INT_MAX;
            while (justin_repo_commit_list_next(list, &entry, err)) {
                dist = justin_util_str_dist(entry.message, strlenol(entry.message), lbuf, inp_len);
                if (dist == -1) {
                    *err = JUSTIN_ERR_NOMEM;
                    return min;
                }
                if (dist < min_dist) {
                    min = entry;
                    min_dist = dist;
                    if (dist == 0) break;
                }
            }
            if ((*err) == JUSTIN_ERR_OK) {
                sprintf(lbuf, "Selected %s%.238s", BYEL, min.message);
                justin_log_info(lbuf);
            }
            return min;
        }
        case 'n': case 'N': {
            return justin_repo_commit_list_prompt(list, entry_count == LIST_PROMPT_SIZE ? start + LIST_PROMPT_SIZE : start, err);
        }
        case 'p': case 'P': {
            return justin_repo_commit_list_prompt(list, start >= LIST_PROMPT_SIZE ? start - LIST_PROMPT_SIZE : 0, err);
        }
        default: {
            *err = JUSTIN_ERR_ARGS;
            return entries[0];
        }
    }
}
