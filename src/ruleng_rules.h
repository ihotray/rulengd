
#pragma once

#include <json-c/json.h>
#include "ruleng_list.h"

enum ruleng_rules_rc {
    RULENG_RULES_OK = 0,
    RULENG_RULES_ERR_ALLOC,
    RULENG_RULES_ERR_NOT_FOUND,
    RULENG_RULES_ERR_NOT_VALID,
};

struct ruleng_rules_ctx {
    struct uci_context *uci_ctx;
};

struct ruleng_rule {
    LN_LIST_NODE(ruleng_rule) node;
    struct ruleng_rules_event {
        char *name;
        struct json_object *args;
    } event;
    struct ruleng_rules_action {
        char *object;
        char *name;
        struct json_object *args;
    } action;
};

LN_LIST_HEAD(ruleng_rules, ruleng_rule);

enum ruleng_rules_rc
ruleng_rules_ctx_init(struct ruleng_rules_ctx **);

enum ruleng_rules_rc
ruleng_rules_get(struct ruleng_rules_ctx *, struct ruleng_rules *, char *);

void
ruleng_rules_free(struct ruleng_rules *);

void
ruleng_rules_ctx_free(struct ruleng_rules_ctx *);

