
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>

#include <uci.h>

#include "ruleng_rules.h"
#include "ruleng_list.h"
#include "utils.h"

#define RULENG_EVENT_FIELD "event"
#define RULENG_EVENT_ARG_FIELD "event_data"
#define RULENG_METHOD_FIELD "method"
#define RULENG_METHOD_ARG_FIELD "method_data"

struct ruleng_rules_ctx {
    struct uci_context *uci_ctx;
    struct json_object *com;
};

void
ruleng_rules_free(struct ruleng_rules *rules)
{
    struct ruleng_rule *rule = NULL, *tmp = NULL;
    LN_LIST_FOREACH_SAFE(rule, rules, node, tmp) {
        json_object_put(rule->action.args);
        free(rule->event.name);
        json_object_put(rule->event.args);
        free(rule->action.object);
        free(rule);
    }
}

enum ruleng_rules_rc
ruleng_rules_ctx_init(struct ruleng_rules_ctx **ctx, const char *path)
{
    enum ruleng_rules_rc rc = RULENG_RULES_OK;

    *ctx = calloc(1, sizeof(**ctx));
    if (NULL == *ctx) {
        RULENG_ERR("failed to allocate model context");
        rc = RULENG_RULES_ERR_ALLOC;
        goto exit;
    }

    struct ruleng_rules_ctx *_ctx = *ctx;

    struct uci_context *uci_ctx = uci_alloc_context();;
    if (NULL == uci_ctx) {
        RULENG_ERR("failed to allocate uci context");
        rc = RULENG_RULES_ERR_ALLOC;
        goto cleanup_ctx;
    }

    struct json_object *com = json_object_from_file(path);
    if (NULL == com) {
        RULENG_ERR("failed to parse common object model: %s", path);
        rc = RULENG_RULES_ERR_NOT_VALID;
        goto cleanup_uci;
    }

    _ctx->uci_ctx = uci_ctx;
    _ctx->com = com;

    goto exit;

cleanup_uci:
    uci_free_context(uci_ctx);
cleanup_ctx:
    free(_ctx);
exit:
    return rc;
}

static enum ruleng_rules_rc
ruleng_rules_rules_parse_object_method(struct uci_context *ctx,
                                     struct uci_section *s,
                                     char **method,
                                     char **object)
{
    enum ruleng_rules_rc rc = RULENG_RULES_OK;

    const char *om = uci_lookup_option_string(ctx, s, RULENG_METHOD_FIELD);
    if (NULL == om) {
        RULENG_ERR("%s: no method option", s->type);
        rc = RULENG_RULES_ERR_NOT_VALID;
        goto exit;
    }

    char *temp = strdup(om);
    if (NULL == temp) {
        RULENG_ERR("%s: failed to allocate object method", s->type);
        rc = RULENG_RULES_ERR_ALLOC;
        goto exit;
    }

    *object = strsep(&temp, "-");
    if (NULL == temp) {
        RULENG_ERR("%s: delimiter '-' not found", s->type);
        rc = RULENG_RULES_ERR_NOT_VALID;
        goto cleanup_temp;
    }

    if (2 > strlen(temp) && temp[0] != '>') {
        RULENG_ERR("%s: method does not exist", s->type);
        rc = RULENG_RULES_ERR_NOT_VALID;
        goto cleanup_temp;
    }

    *method = temp + 1;

    goto exit;

cleanup_temp:
    free(temp);
exit:
    return rc;
}

static enum ruleng_rules_rc
ruleng_rules_rules_parse_event_name(struct uci_context *ctx,
                                  struct uci_section *s,
                                  char **ev)
{
    enum ruleng_rules_rc rc = RULENG_RULES_OK;

    const char *e = uci_lookup_option_string(ctx, s, RULENG_EVENT_FIELD);
    if (NULL == e) {
        RULENG_ERR("%s: failed to find event name", s->type);
        rc = RULENG_RULES_ERR_NOT_VALID;
        goto exit;
    }

    *ev = strdup(e);
    if (NULL == *ev) {
        RULENG_ERR("%s: failed to allocate event name", s->type);
        rc = RULENG_RULES_ERR_ALLOC;
        goto exit;
    }

exit:
    return rc;
}

static void
ruleng_rules_json_object_concat(struct json_object **src, struct json_object *dest)
{
    json_object_object_foreach(dest, key, val) {
        struct json_object *tmp = json_object_get(val);
        json_object_object_add(*src, key, tmp);
    }
    return;
}

static enum ruleng_rules_rc
ruleng_rules_rules_parse_args(struct uci_context *ctx,
                            const char *field,
                            struct uci_section *s,
                            const char *ev,
                            struct json_object **args)
{
    enum ruleng_rules_rc rc = RULENG_RULES_OK;

    *args = json_object_new_object();
    if (NULL == *args) {
        RULENG_ERR("%s: failed to allocate json object", ev);
        rc = RULENG_RULES_ERR_ALLOC;
        goto exit;
    }

    struct uci_option *earg = uci_lookup_option(ctx, s, field);
    if (NULL == earg) {
        RULENG_INFO("%s: rule arguments not found", ev);
        goto exit;
    }

    struct uci_element *elem = NULL;
    uci_foreach_element(&earg->v.list, elem) {
        struct json_object *obj = json_tokener_parse(elem->name);
        if (NULL == obj) {
            RULENG_ERR("%s: rule contains invalid json object", ev);
            rc = RULENG_RULES_ERR_NOT_VALID;
            goto cleanup_json;
        }
        ruleng_rules_json_object_concat(args, obj);
        json_object_put(obj);
    }

    goto exit;

cleanup_json:
    json_object_put(*args);
exit:
    return rc;
}

static enum ruleng_rules_rc
ruleng_rules_rules_parse_event(struct uci_context *ctx,
                             struct uci_section *s,
                             struct ruleng_rules_event *ev)
{
    enum ruleng_rules_rc rc = RULENG_RULES_OK;

    char *name = NULL;
    rc = ruleng_rules_rules_parse_event_name(ctx, s, &name);
    if (RULENG_RULES_OK != rc)
        goto exit;

    struct json_object *args = NULL;
    rc = ruleng_rules_rules_parse_args(ctx, RULENG_EVENT_ARG_FIELD, s, name, &args);
    if (RULENG_RULES_OK != rc)
        goto cleanup_name;

    RULENG_INFO("%s event data: %s", name, json_object_to_json_string(args));

    ev->name = name;
    ev->args = args;

    goto exit;

cleanup_name:
    free(name);
exit:
    return rc;
}

static enum ruleng_rules_rc
ruleng_rules_rules_parse_action(struct uci_context *ctx,
                              struct uci_section *s,
                              struct ruleng_rules_action *ac)
{
    enum ruleng_rules_rc rc = RULENG_RULES_OK;

    char *name = NULL, *object = NULL;
    rc = ruleng_rules_rules_parse_object_method(ctx, s, &name, &object);
    if (RULENG_RULES_OK != rc)
        goto exit;

    struct json_object *args = NULL;
    rc = ruleng_rules_rules_parse_args(ctx, RULENG_METHOD_ARG_FIELD, s, name, &args);
    if (RULENG_RULES_OK != rc)
        goto cleanup_object;

    ac->name = name;
    ac->object = object;
    ac->args = args;

    goto exit;

cleanup_object:
    free(object);
exit:
    return rc;
}

enum ruleng_rules_rc
ruleng_rules_get(struct ruleng_rules_ctx *ctx, struct ruleng_rules *rules,
                 char *path)
{
    enum ruleng_rules_rc rc = RULENG_RULES_OK;

    struct uci_ptr ptr;
    if (UCI_OK != uci_lookup_ptr(ctx->uci_ctx, &ptr, path, true)) {
        RULENG_ERR("%s: uci lookup failed", path);
        rc = RULENG_RULES_ERR_NOT_FOUND;
        goto exit;
    }

    struct uci_element *e = NULL;
    uci_foreach_element(&ptr.p->sections, e) {
        struct uci_section *s = uci_to_section(e);

        struct ruleng_rule *rule = calloc(1, sizeof *rule);
        if (NULL == rule) {
            RULENG_ERR("%s: failed to allocate rule", path);
            rc = RULENG_RULES_ERR_ALLOC;
            goto cleanup_rules;
        }

        rc = ruleng_rules_rules_parse_event(ctx->uci_ctx, s, &rule->event);
        if (RULENG_RULES_OK != rc)
            goto cleanup_rule;

        rc = ruleng_rules_rules_parse_action(ctx->uci_ctx, s, &rule->action);
        if (RULENG_RULES_OK != rc)
            goto cleanup_event_args;

        LN_LIST_INSERT(rules, rule, node);

        continue;

    cleanup_event_args:
        free(rule->event.name);
        json_object_put(rule->event.args);
    cleanup_rule:
        free(rule);
        goto cleanup_rules;
    }

    goto exit;

cleanup_rules:
    ruleng_rules_free(rules);
exit:
    return rc;
}

void
ruleng_rules_ctx_free(struct ruleng_rules_ctx *ctx)
{
    uci_free_context(ctx->uci_ctx);
    json_object_put(ctx->com);
    free(ctx);
}
