
#pragma once

#include <json-c/json.h>
#include "ruleng_list.h"

enum ruleng_com_rc {
    RULENG_COM_OK = 0,
    RULENG_COM_ERR_ALLOC,
    RULENG_COM_ERR_MODEL_NOT_VALID,
    RULENG_COM_ERR_RULES_NOT_FOUND,
    RULENG_COM_ERR_RULES_NOT_VALID,
};

struct ruleng_com_ctx;

struct ruleng_com_recipe {
    LN_LIST_NODE(ruleng_com_recipe) node;
    char *event;
    struct json_object *args;
    struct action {
        char *object;
        char *method;
        struct json_object *args;
    } action;
};

LN_LIST_HEAD(ruleng_com_recipes, ruleng_com_recipe);

enum ruleng_com_rc
ruleng_com_init(struct ruleng_com_ctx **, const char *);

enum ruleng_com_rc
ruleng_com_parse_recipes(struct ruleng_com_ctx *, struct ruleng_com_recipes *,
                         char *);
void
ruleng_com_free_recipes(struct ruleng_com_recipes *);

void
ruleng_com_free(struct ruleng_com_ctx *);

