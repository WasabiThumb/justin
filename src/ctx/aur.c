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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <json-c/json.h>
#include <curl/curl.h>
#include <git2.h>
#include "../util.h"
#include "aur.h"

void justin_aur_project_list_free0(justin_aur_project_list list, bool root) {
    if (list == NULL) return;
    justin_aur_project_list next = list->next;
    if (next != NULL) justin_aur_project_list_free0(next, false);
    if (list->size == 0) {
        if (root) free(list);
        return;
    }
    justin_aur_project_t *project = &list->value;
    free((void*) project->name);
    free((void*) project->version);
    free((void*) project->description);
    if (root) free(list);
}

void justin_aur_project_list_free(justin_aur_project_list list) {
    justin_aur_project_list_free0(list, true);
}

int justin_aur_project_list_sort_popularity0(const void *a, const void *b) {
    float pop_a = ((justin_aur_project_list) a)->value.popularity;
    float pop_b = ((justin_aur_project_list) b)->value.popularity;
    if (pop_a == pop_b) return 0;
    if (pop_a < pop_b) return -1;
    return 1;
}

void justin_aur_project_list_sort_popularity(justin_aur_project_list list) {
    size_t size = list->size;
    if (size < 2) return;

    qsort(list, size, sizeof(justin_aur_project_list_t), justin_aur_project_list_sort_popularity0);

    justin_aur_project_list node = &list[0];
    justin_aur_project_list next;
    size_t i = 1;
    while (i < size) {
        next = &list[i];
        node->next = next;
        node = next;
        node->next = NULL;
        i++;
    }
}

#define AUR_URL_SEARCH "https://aur.archlinux.org/rpc/v5/search/"
static const char *AUR_URL_SEARCH_S = AUR_URL_SEARCH;

char* build_url_search(const char *term) {
    size_t term_l = strlen(term);
    char *full = (char*) malloc((sizeof AUR_URL_SEARCH) + (term_l * 3));
    if (full == NULL) return NULL;

    memcpy(full, AUR_URL_SEARCH_S, sizeof AUR_URL_SEARCH);
    size_t head = (sizeof AUR_URL_SEARCH) - 1;

    char c;
    for (size_t i=0; i < term_l; i++) {
        c = term[i];
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
            full[head++] = c;
        } else if (c >= 'A' && c <= 'Z') {
            full[head++] = (char) (c + 32);
        } else if (c > 127) {
            full[head++] = '%';
            full[head++] = '2';
            full[head++] = '0';
        } else {
            full[head++] = '%';
            full[head++] = justin_util_n2hex((c >> 4) & 0xF);
            full[head++] = justin_util_n2hex(c & 0xF);
        }
    }
    full[head] = (char) 0;

    return full;
}

struct json_collector {
    struct json_tokener *tokener;
    struct json_object *obj;
};

size_t curl_collect(char *ptr, size_t size, size_t nmemb, void *userdata) {
    size_t real_size = size * nmemb;

    struct json_collector *collector = (struct json_collector*) userdata;
    struct json_object *obj = json_tokener_parse_ex(collector->tokener, ptr, (int) real_size);
    if (obj != NULL) {
        if (collector->obj != NULL) json_object_free_userdata(NULL, collector->obj);
        collector->obj = obj;
    }

    enum json_tokener_error err = json_tokener_get_error(collector->tokener);
    if (err != json_tokener_success && err != json_tokener_continue) {
        return 0;
    }
    return real_size;
}

justin_aur_project_list search_json2list(struct json_object *obj) {
    justin_aur_project_list node = (justin_aur_project_list) malloc(sizeof(justin_aur_project_list_t));
    if (node == NULL) return NULL;

    struct json_object *temp;
    temp = json_object_object_get(obj, "resultcount");
    if (temp == NULL) {
        node->size = 0;
        return node;
    }
    int64_t rc = json_object_get_int64(temp);

    temp = json_object_object_get(obj, "results");
    if (temp == NULL || json_object_get_type(temp) != json_type_array) {
        node->size = 0;
        return node;
    }

    justin_aur_project_list nodes = (justin_aur_project_list) realloc(node, sizeof(justin_aur_project_list_t) * rc);
    if (nodes == NULL) return NULL;

    struct json_object *element;
    struct json_object *prop;
    for (int64_t i=0; i < rc; i++) {
        node = &nodes[i];
        node->size = 0;
        node->next = NULL;
        if (i > 0) (&nodes[i - 1])->next = node;
        element = json_object_array_get_idx(temp, i);
        if (element == NULL) continue;

        justin_aur_project_t *project = &node->value;

        prop = json_object_object_get(element, "NumVotes");
        if (prop == NULL) continue;
        project->votes = json_object_get_int(prop);

        prop = json_object_object_get(element, "Popularity");
        if (prop == NULL) continue;
        project->popularity = (float) atof(json_object_get_string(prop));

        prop = json_object_object_get(element, "Name");
        if (prop == NULL) continue;
        char* name = strdup(json_object_get_string(prop));
        if (name == NULL) {
            justin_aur_project_list_free(nodes);
            return NULL;
        }
        project->name = name;

        prop = json_object_object_get(element, "Version");
        if (prop == NULL) continue;
        char* version = strdup(json_object_get_string(prop));
        if (version == NULL) {
            free(name);
            justin_aur_project_list_free(nodes);
            return NULL;
        }
        project->version = version;

        prop = json_object_object_get(element, "Description");
        if (prop == NULL) continue;
        char* description = strdup(json_object_get_string(prop));
        if (description == NULL) {
            free(name);
            free(description);
            justin_aur_project_list_free(nodes);
            return NULL;
        }
        project->description = description;

        node->size = rc;
    }

    return &nodes[0];
}

justin_aur_project_list justin_aur_search(justin_context ctx, const char *term, justin_err *err) {
    *err = JUSTIN_ERR_OK;

    CURL *curl = ctx->curl;
    CURLcode res;
    char *url = build_url_search(term);
    if (url == NULL) {
        *err = JUSTIN_ERR_NOMEM;
        return NULL;
    }

    struct json_tokener *tokener = json_tokener_new();
    struct json_collector col = { tokener, NULL };

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, ((void*) (&col)));
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_collect);
    res = curl_easy_perform(curl);

    json_tokener_free(tokener);
    if (res != CURLE_OK) {
        *err = JUSTIN_ERR_CURL(res);
        return NULL;
    }

    struct json_object *obj = col.obj;
    if (obj == NULL) return NULL;

    justin_aur_project_list ret = search_json2list(obj);
    if (ret == NULL) *err = JUSTIN_ERR_NOMEM;
    json_object_free_userdata(NULL, obj);
    return ret;
}

#define AUR_GIT_URL_A "https://aur.archlinux.org/"
static const char *AUR_GIT_URL_A_S = AUR_GIT_URL_A;
#define AUR_GIT_URL_A_L ((sizeof AUR_GIT_URL_A) - 1)

#define AUR_GIT_URL_B ".git"
static const char *AUR_GIT_URL_B_S = AUR_GIT_URL_B;
#define AUR_GIT_URL_B_L ((sizeof AUR_GIT_URL_B) - 1)

git_repository *justin_aur_project_clone_into(justin_context ctx, justin_aur_project_t *project, const char *path, justin_err *err) {
    const char *name = project->name;
    size_t name_len = strlen(name);

    char* url = (char*) malloc(AUR_GIT_URL_A_L + AUR_GIT_URL_B_L + name_len + 1);
    if (url == NULL) {
        *err = JUSTIN_ERR_NOMEM;
        return NULL;
    }
    memcpy(url, AUR_GIT_URL_A_S, AUR_GIT_URL_A_L);
    memcpy(&url[AUR_GIT_URL_A_L], name, name_len);
    memcpy(&url[AUR_GIT_URL_A_L + name_len], AUR_GIT_URL_B_S, AUR_GIT_URL_B_L);
    url[AUR_GIT_URL_A_L + AUR_GIT_URL_B_L + name_len] = (char) 0;

    git_repository *repo;
    if (git_clone(&repo, url, path, NULL) != 0) {
        *err = JUSTIN_ERR_GIT;
        free(url);
        return NULL;
    }
    free(url);

    if (justin_util_chown_r(path, ctx->storage->user) != 0) {
        *err = JUSTIN_ERR_SYSTEM;
        git_repository_free(repo);
        return NULL;
    }
    return repo;
}
