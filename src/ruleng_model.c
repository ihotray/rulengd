
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>

#include <uci.h>

#include "ruleng_model.h"
#include "ruleng_list.h"
#include "utils.h"

#define RULENG_EVENT_FIELD "event"
#define RULENG_EVENT_ARG_FIELD "event_arg"
#define RULENG_METHOD_FIELD "method"
#define RULENG_METHOD_ARG_FIELD "method_arg"

struct ruleng_com_ctx {
    struct uci_context *uci_ctx;
    struct json_object *com;
};

void
ruleng_com_free_rules(struct ruleng_com_rules *rules)
{
    struct ruleng_com_rule *rule = NULL, *tmp = NULL;
    LN_LIST_FOREACH_SAFE(rule, rules, node, tmp) {
        json_object_put(rule->action.args);
        free(rule->event.name);
        json_object_put(rule->event.args);
        free(rule->action.object);
        free(rule);
    }
}

enum ruleng_com_rc
ruleng_com_init(struct ruleng_com_ctx **ctx, const char *path)
{
    enum ruleng_com_rc rc = RULENG_COM_OK;

    *ctx = calloc(1, sizeof(**ctx));
    if (NULL == *ctx) {
        RULENG_ERR("failed to allocate model context");
        rc = RULENG_COM_ERR_ALLOC;
        goto exit;
    }

    struct ruleng_com_ctx *_ctx = *ctx;

    struct uci_context *uci_ctx = uci_alloc_context();;
    if (NULL == uci_ctx) {
        RULENG_ERR("failed to allocate uci context");
        rc = RULENG_COM_ERR_ALLOC;
        goto cleanup_ctx;
    }

    struct json_object *com = json_object_from_file(path);
    if (NULL == com) {
        RULENG_ERR("failed to parse common object model: %s", path);
        rc = RULENG_COM_ERR_MODEL_NOT_VALID;
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

static enum ruleng_com_rc
ruleng_com_rule_parse(const char *r, char **k, char **v)
{
    enum ruleng_com_rc rc = RULENG_COM_ERR_RULES_NOT_VALID;

    if (NULL == r)
        goto exit;

    *v = strdup(r);
    if (NULL == *v)
        goto exit;

    *k = strsep(v, ":");
    if (NULL == *k)
        goto exit;
    if (NULL == *v || '\0' == **v)
        goto cleanup;

    size_t i = 0, l = strlen(*v);
    while(i < l && isspace((*v)[i]))
        ++i;
    if (i == l)
        goto cleanup;

    memmove(*v, *v + i, l - i);
    (*v)[l - i] = '\0';

    rc = RULENG_COM_OK;
    goto exit;

cleanup:
    free(*k);
exit:
    return rc;
}

static struct json_object *
ruleng_com_json_array_find_object(struct json_object *arr,
                                  const char *k,
                                  const char *v)
{
    for (int i = 0, len = json_object_array_length(arr); i < len; i++) {
        struct json_object *o = json_object_array_get_idx(arr, i);
        struct json_object *f = NULL;
        if (true == json_object_object_get_ex(o, k, &f)
            && 0 == strcmp(v, json_object_get_string(f)))
            return o;
    }
    return NULL;
}

static enum ruleng_com_rc
ruleng_com_get_method_arg_type(struct json_object *com,
                               const char *mname,
                               const char *aname,
                               const char **type)
{
    enum ruleng_com_rc rc = RULENG_COM_ERR_MODEL_NOT_VALID;

    struct json_object *methods = NULL;
    if (false == json_object_object_get_ex(com, "method", &methods)) {
        RULENG_ERR("model: 'method' not found");
        goto exit;
    }

    struct json_object *o =
        ruleng_com_json_array_find_object(methods, "name", mname);
    if (NULL == o) {
        RULENG_ERR("model: method 'name' not found");
        goto exit;
    }

    struct json_object *input = NULL;
    if (false == json_object_object_get_ex(o, "input", &input)) {
        RULENG_ERR("model: method missing 'input'");
        goto exit;
    }

    struct json_object *t =
        ruleng_com_json_array_find_object(input, "name", aname);
    if (NULL == t) {
        RULENG_ERR("model: '%s' method input 'name' not found", mname);
        goto exit;
    }

    struct json_object *js = NULL;
    if (false == json_object_object_get_ex(t, "type", &js)) {
        RULENG_ERR("model: '%s' method input missing type", mname);
        goto exit;
    }

    *type = json_object_get_string(js);
    rc = RULENG_COM_OK;

exit:
    return rc;
}

static enum ruleng_com_rc
ruleng_com_get_event_arg_type(struct json_object *com, const char *ename,
                              const char *name, const char **type)
{
    enum ruleng_com_rc rc = RULENG_COM_ERR_MODEL_NOT_VALID;

    struct json_object *events = NULL;
    if (false == json_object_object_get_ex(com, "event", &events)) {
        RULENG_ERR("model: 'event' not found");
        goto exit;
    }

    struct json_object *o =
        ruleng_com_json_array_find_object(events, "name", ename);
    if (NULL == o) {
        RULENG_ERR("model: event 'name' not found");
        goto exit;
    }

    struct json_object *data = NULL;
    if (false == json_object_object_get_ex(o, "data", &data)) {
        RULENG_ERR("model: event 'data' not found");
        goto exit;
    }

    struct json_object *t =
        ruleng_com_json_array_find_object(data, "name", name);
    if (NULL == t) {
        RULENG_ERR("model: event data 'name' not found");
        goto exit;
    }

    struct json_object *js = NULL;
    if (false == json_object_object_get_ex(t, "type", &js)) {
        RULENG_ERR("model: event data 'type' not found");
        goto exit;
    }

    *type = json_object_get_string(js);
    rc = RULENG_COM_OK;

exit:
    return rc;
}

static bool
strtoi(const char *str, int *val)
{
    char *endptr;

    errno = 0;
    long res = strtol(str, &endptr, 10);

    if (errno != 0
        || endptr == str
        || *endptr != '\0'
        || res < INT_MIN
        || res > INT_MAX)
        return false;

    *val = (int)res;
    return true;
}

static enum ruleng_com_rc
ruleng_com_rules_parse_object_method(struct uci_context *ctx,
                                     struct uci_section *s,
                                     char **method,
                                     char **object)
{
    enum ruleng_com_rc rc = RULENG_COM_OK;

    const char *om = uci_lookup_option_string(ctx, s, RULENG_METHOD_FIELD);
    if (NULL == om) {
        RULENG_ERR("%s: no method option", s->type);
        rc = RULENG_COM_ERR_RULES_NOT_VALID;
        goto exit;
    }

    char *temp = strdup(om);
    if (NULL == temp) {
        RULENG_ERR("failed to allocate object method");
        rc = RULENG_COM_ERR_ALLOC;
        goto exit;
    }

    *object = strsep(&temp, "-");
    if (NULL == temp) {
        RULENG_ERR("%s: delimiter '-' not found", s->type);
        rc = RULENG_COM_ERR_RULES_NOT_VALID;
        goto cleanup_temp;
    }

    if (2 > strlen(temp) && temp[0] != '>') {
        RULENG_ERR("method does not exist");
        rc = RULENG_COM_ERR_RULES_NOT_VALID;
        goto cleanup_temp;
    }

    *method = temp + 1;

    goto exit;

cleanup_temp:
    free(temp);
exit:
    return rc;
}

static enum ruleng_com_rc
ruleng_com_rules_parse_event_name(struct uci_context *ctx,
                                  struct uci_section *s,
                                  char **ev)
{
    enum ruleng_com_rc rc = RULENG_COM_OK;

    const char *e = uci_lookup_option_string(ctx, s, RULENG_EVENT_FIELD);
    if (NULL == e) {
        RULENG_ERR("%s: failed to find event name", s->type);
        rc = RULENG_COM_ERR_RULES_NOT_VALID;
        goto exit;
    }

    *ev = strdup(e);
    if (NULL == *ev) {
        RULENG_ERR("%s: failed to allocate event name", s->type);
        rc = RULENG_COM_ERR_ALLOC;
        goto exit;
    }

exit:
    return rc;
}

static struct json_object *
ruleng_com_rules_conv_to_json(const char *v, const char *t)
{
    struct json_object *obj = NULL;

    if (0 == strcmp("s", t)) {
        obj = json_object_new_string(v);
    } else if (0 == strcmp("i", t)) {
        int tmp = 0;
        if (false == strtoi(v, &tmp)) {
            RULENG_ERR("failed casting %s to integer", v);
        } else {
            obj = json_object_new_int(tmp);
        }
    } else if (0 == strcmp("b", t)) {
        bool b = 0 == strcmp("t", v);
        obj = json_object_new_boolean(b);
    } else {
        RULENG_ERR("invalid type: %s", t);
    }

    return obj;
}

static enum ruleng_com_rc
ruleng_com_rules_parse_event_type(struct uci_option *o,
                                  const char *ev,
                                  char **type)
{
    enum ruleng_com_rc rc = RULENG_COM_OK;

    struct uci_element *le = list_to_element(o->v.list.next);
    if (NULL == le) {
        RULENG_ERR("%s: rules not found", ev);
        rc = RULENG_COM_ERR_RULES_NOT_VALID;
        goto exit;
    }
    char *k = NULL, *v = NULL;
    rc = ruleng_com_rule_parse(le->name, &k, &v);
    if (RULENG_COM_OK != rc) {
        RULENG_ERR("%s: failed to parse rule", ev);
        goto exit;
    }
    if (0 != strcmp(k, "type")) {
        RULENG_ERR("%s: event type not found", ev);
        rc = RULENG_COM_ERR_RULES_NOT_VALID;
        goto cleanup_key;
    }
    *type = strdup(v);
    if (NULL == *type) {
        RULENG_ERR("%s: failed to allocate event type", ev);
        rc = RULENG_COM_ERR_ALLOC;
        goto cleanup_key;
    }

cleanup_key:
    free(k);
exit:
    return rc;
}

static enum ruleng_com_rc
ruleng_com_rules_parse_event_args(struct uci_context *ctx,
                                  struct json_object *com,
                                  struct uci_section *s,
                                  const char *ev,
                                  struct json_object **args)
{
    enum ruleng_com_rc rc = RULENG_COM_OK;

    *args = json_object_new_object();
    if (NULL == *args) {
        RULENG_ERR("failed to allocate json object");
        rc = RULENG_COM_ERR_ALLOC;
        goto exit;
    }

    struct uci_option *earg = uci_lookup_option(ctx, s, RULENG_EVENT_ARG_FIELD);

    char *et = NULL;
    rc = ruleng_com_rules_parse_event_type(earg, ev, &et);
    if (RULENG_COM_OK != rc)
        goto exit;

    json_object_object_add(*args, "type", json_object_new_string(et));

    struct uci_list *list = &earg->v.list;
    for (struct uci_element *le = list_to_element(list->next->next);
         &le->list != list;
         le = list_to_element(le->list.next)) {

        char *k = NULL, *v = NULL;
        rc = ruleng_com_rule_parse(le->name, &k, &v);
        if (RULENG_COM_OK != rc) {
            RULENG_ERR("failed to parse event rule");
            goto cleanup_json;
        }
        const char *t = NULL;
        rc = ruleng_com_get_event_arg_type(com, et, k, &t);
        if (RULENG_COM_OK != rc)
            goto cleanup_key;

        struct json_object *rule = ruleng_com_rules_conv_to_json(v, t);
        if (NULL == rule)
            goto cleanup_key;

        json_object_object_add(*args, k, rule);

    cleanup_key:
        free(k);
        if (RULENG_COM_OK != rc)
            goto cleanup_json;
    }

    goto exit;

cleanup_json:
    json_object_put(*args);
exit:
    free(et);
    return rc;
}

static enum ruleng_com_rc
ruleng_com_rules_parse_method_args(struct uci_context *ctx,
                                   struct json_object *com,
                                   struct uci_section *s,
                                   const char *method,
                                   struct json_object **args)
{
    enum ruleng_com_rc rc = RULENG_COM_OK;

    *args = json_object_new_object();
    if (NULL == *args) {
        RULENG_ERR("failed to allocate json object");
        rc = RULENG_COM_ERR_ALLOC;
        goto exit;
    }

    struct uci_option *earg = uci_lookup_option(ctx, s, RULENG_METHOD_ARG_FIELD);
    struct uci_element *le = NULL;

    uci_foreach_element(&earg->v.list, le) {
        char *k = NULL, *v = NULL;
        rc = ruleng_com_rule_parse(le->name, &k, &v);
        if (RULENG_COM_OK != rc) {
            RULENG_ERR("failed to parse method argument rule");
            goto cleanup_json;
        }

        const char *t = NULL;
        rc = ruleng_com_get_method_arg_type(com, method, k, &t);
        if (RULENG_COM_OK != rc)
            goto cleanup_key;

        struct json_object *rule = ruleng_com_rules_conv_to_json(v, t);
        if (NULL == rule)
            goto cleanup_key;

        json_object_object_add(*args, k, rule);

    cleanup_key:
        free(k);
        if (RULENG_COM_OK != rc)
            goto cleanup_json;
    }

    goto exit;

cleanup_json:
    json_object_put(*args);
exit:
    return rc;
}

static enum ruleng_com_rc
ruleng_com_rules_parse_event(struct uci_context *ctx,
                             struct json_object *com,
                             struct uci_section *s,
                             struct ruleng_com_event *ev)
{
    enum ruleng_com_rc rc = RULENG_COM_OK;

    char *name = NULL;
    rc = ruleng_com_rules_parse_event_name(ctx, s, &name);
    if (RULENG_COM_OK != rc)
        goto exit;

    struct json_object *args = NULL;
    rc = ruleng_com_rules_parse_event_args(ctx, com, s, name, &args);
    if (RULENG_COM_OK != rc)
        goto cleanup_name;

    ev->name = name;
    ev->args = args;

    goto exit;

cleanup_name:
    free(name);
exit:
    return rc;
}

static enum ruleng_com_rc
ruleng_com_rules_parse_action(struct uci_context *ctx,
                              struct json_object *com,
                              struct uci_section *s,
                              struct ruleng_com_action *ac)
{
    enum ruleng_com_rc rc = RULENG_COM_OK;

    char *name = NULL, *object = NULL;
    rc = ruleng_com_rules_parse_object_method(ctx, s, &name, &object);
    if (RULENG_COM_OK != rc)
        goto exit;

    struct json_object *args = NULL;
    rc = ruleng_com_rules_parse_method_args(ctx, com, s, name, &args);
    if (RULENG_COM_OK != rc)
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

enum ruleng_com_rc
ruleng_com_get_rules(struct ruleng_com_ctx *ctx, struct ruleng_com_rules *rules,
                     char *path)
{
    enum ruleng_com_rc rc = RULENG_COM_OK;

    struct uci_ptr ptr;
    if (UCI_OK != uci_lookup_ptr(ctx->uci_ctx, &ptr, path, true)) {
        RULENG_ERR("uci lookup failed");
        rc = RULENG_COM_ERR_RULES_NOT_FOUND;
        goto exit;
    }

    struct uci_element *e = NULL;
    uci_foreach_element(&ptr.p->sections, e) {
        struct uci_section *s = uci_to_section(e);

        struct ruleng_com_rule *rule = calloc(1, sizeof *rule);
        if (NULL == rule) {
            RULENG_ERR("failed to allocate rule");
            rc = RULENG_COM_ERR_ALLOC;
            goto cleanup_rules;
        }

        rc = ruleng_com_rules_parse_event(ctx->uci_ctx, ctx->com, s, &rule->event);
        if (RULENG_COM_OK != rc)
            goto cleanup_rule;

        rc = ruleng_com_rules_parse_action(ctx->uci_ctx, ctx->com, s, &rule->action);
        if (RULENG_COM_OK != rc)
            goto cleanup_event_args;

        LN_LIST_INSERT(rules, rule, node);

        continue;

    cleanup_event_args:
        json_object_put(rule->event.args);
    cleanup_rule:
        free(rule);
        goto cleanup_rules;
    }

    goto exit;

cleanup_rules:
    ruleng_com_free_rules(rules);
exit:
    return rc;
}

void
ruleng_com_free(struct ruleng_com_ctx *ctx)
{
    uci_free_context(ctx->uci_ctx);
    json_object_put(ctx->com);
    free(ctx);
}
