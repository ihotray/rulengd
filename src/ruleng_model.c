
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

struct ruleng_com_ctx {
    struct uci_context *uci_ctx;
    struct json_object *com;
};

void
ruleng_com_free_recipes(struct ruleng_com_recipes *rs)
{
    struct ruleng_com_recipe *r = NULL, *t = NULL;
    LN_LIST_FOREACH_SAFE(r, rs, node, t) {
        json_object_put(r->action.args);
        free(r->event);
        json_object_put(r->args);
        free(r->action.object);
        free(r);
    }
}

enum ruleng_com_rc
ruleng_com_init(struct ruleng_com_ctx **ctx, const char *path)
{
    enum ruleng_com_rc rc = RULENG_COM_OK;

    *ctx = calloc(1, sizeof(**ctx));
    if (NULL == *ctx) {
        RULENG_ERR("error allocating model context");
        rc = RULENG_COM_ERR_ALLOC;
        goto exit;
    }

    struct ruleng_com_ctx *_ctx = *ctx;

    struct uci_context *uci_ctx = uci_alloc_context();;
    if (NULL == uci_ctx) {
        RULENG_ERR("error allocating uci context");
        rc = RULENG_COM_ERR_ALLOC;
        goto cleanup_ctx;
    }

    struct json_object *com = json_object_from_file(path);
    if (NULL == com) {
        RULENG_ERR("error parsing model");
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
ruleng_com_parse_key_value(const char *r, char **k, char **v)
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
ruleng_com_json_array_find_object(struct json_object *arr, const char *k,
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
ruleng_com_get_method_arg_type(json_object *com, const char *mname,
                               const char *aname, const char **type)
{
    enum ruleng_com_rc rc = RULENG_COM_ERR_MODEL_NOT_VALID;

    struct json_object *methods = NULL;
    if (false == json_object_object_get_ex(com, "method", &methods)) {
        RULENG_ERR("invalid model, 'method' not found");
        goto exit;
    }

    struct json_object *o =
        ruleng_com_json_array_find_object(methods, "name", mname);
    if (NULL == o) {
        RULENG_ERR("invalid model, method 'name' not found");
        goto exit;
    }

    struct json_object *input = NULL;
    if (false == json_object_object_get_ex(o, "input", &input)) {
        RULENG_ERR("invalid model: method missing 'input'");
        goto exit;
    }

    struct json_object *t =
        ruleng_com_json_array_find_object(input, "name", aname);
    if (NULL == t) {
        RULENG_ERR("invalid model, '%s' method input 'name' not found", mname);
        goto exit;
    }

    struct json_object *js = NULL;
    if (false == json_object_object_get_ex(t, "type", &js)) {
        RULENG_ERR("invalid model: '%s' method input missing type", mname);
        goto exit;
    }

    *type = json_object_get_string(js);
    rc = RULENG_COM_OK;

exit:
    return rc;
}

static enum ruleng_com_rc
ruleng_com_get_event_arg_type(json_object *com, const char *ename,
                              const char *name, const char **type)
{
    enum ruleng_com_rc rc = RULENG_COM_ERR_MODEL_NOT_VALID;

    struct json_object *events = NULL;
    if (false == json_object_object_get_ex(com, "event", &events)) {
        RULENG_ERR("invalid model, 'event' not found");
        goto exit;
    }

    struct json_object *o =
        ruleng_com_json_array_find_object(events, "name", ename);
    if (NULL == o) {
        RULENG_ERR("invalid model, event 'name' not found");
        goto exit;
    }

    struct json_object *data = NULL;
    if (false == json_object_object_get_ex(o, "data", &data)) {
        RULENG_ERR("invalid model, event 'data' not found");
        goto exit;
    }

    struct json_object *t =
        ruleng_com_json_array_find_object(data, "name", name);
    if (NULL == t) {
        RULENG_ERR("invalid model, event data 'name' not found");
        goto exit;
    }

    struct json_object *js = NULL;
    if (false == json_object_object_get_ex(t, "type", &js)) {
        RULENG_ERR("invalid model, event data 'type' not found");
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
ruleng_com_parse_object_method(struct uci_context *ctx, struct uci_section *s,
                               char **method, char **object)
{
    enum ruleng_com_rc rc = RULENG_COM_OK;

    const char *om = uci_lookup_option_string(ctx, s, "method");
    if (NULL == om) {
        RULENG_ERR("error no method option for %s", s->type);
        rc = RULENG_COM_ERR_RULES_NOT_VALID;
        goto exit;
    }
    RULENG_DEBUG("parsing object method: %s", om);

    char *temp = strdup(om);
    if (NULL == temp) {
        RULENG_ERR("error allocating object method");
        rc = RULENG_COM_ERR_ALLOC;
        goto exit;
    }

    *object = strsep(&temp, "-");
    if (NULL == temp) {
        RULENG_ERR("delimiter '-' not found");
        rc = RULENG_COM_ERR_RULES_NOT_VALID;
        goto cleanup_temp;
    }
    RULENG_DEBUG("parsing object: %s", *object);

    if (2 > strlen(temp) && temp[0] != '>') {
        RULENG_ERR("method does not exist");
        rc = RULENG_COM_ERR_RULES_NOT_VALID;
        goto cleanup_temp;
    }

    *method = temp + 1;
    RULENG_DEBUG("parsing method: %s", *method);

    goto exit;

cleanup_temp:
    free(temp);
exit:
    return rc;
}

static enum ruleng_com_rc
ruleng_com_parse_event(struct uci_context *ctx, struct uci_section *s, char **ev)
{
    enum ruleng_com_rc rc = RULENG_COM_OK;

    const char *e = uci_lookup_option_string(ctx, s, "event");
    if (NULL == e) {
        RULENG_ERR("%s: no event option", s->type);
        rc = RULENG_COM_ERR_RULES_NOT_VALID;
        goto exit;
    }
    RULENG_DEBUG("parsing event: %s", e);

    *ev = strdup(e);
    if (NULL == *ev) {
        RULENG_ERR("error allocating event");
        rc = RULENG_COM_ERR_ALLOC;
        goto exit;
    }

exit:
    return rc;
}

static enum ruleng_com_rc
ruleng_com_conv_to_json(struct json_object **o, const char *k,
                        const char *v, const char *t)
{
    enum ruleng_com_rc rc = RULENG_COM_OK;

    if (0 == strcmp("s", t)) {
        json_object_object_add(*o, k, json_object_new_string(v));
    } else if (0 == strcmp("i", t)) {
        int tmp = 0;
        if (false == strtoi(v, &tmp)) {
            RULENG_ERR("error casting %s to integer", v);
            rc = RULENG_COM_ERR_RULES_NOT_VALID;
            goto exit;
        }
        json_object_object_add(*o, k, json_object_new_int(tmp));
    } else if (0 == strcmp("b", t)) {
        bool b = 0 == strcmp("t", v);
        json_object_object_add(*o, k, json_object_new_boolean(b));
    } else {
        RULENG_ERR("error parsing event: invalid type: %s", t);
        rc = RULENG_COM_ERR_RULES_NOT_VALID;
        goto exit;
    }

exit:
    return rc;
}

static enum ruleng_com_rc
ruleng_com_parse_event_type(struct uci_option *o, const char *ev, char **type)
{
    enum ruleng_com_rc rc = RULENG_COM_OK;

    struct uci_element *le = list_to_element(o->v.list.next);
    if (NULL == le) {
        RULENG_ERR("error parsing event '%s' type", ev);
        rc = RULENG_COM_ERR_RULES_NOT_VALID;
        goto exit;
    }
    char *k = NULL, *v = NULL;
    rc = ruleng_com_parse_key_value(le->name, &k, &v);
    if (RULENG_COM_OK != rc) {
        RULENG_ERR("invalid rule for event type");
        goto exit;
    }
    RULENG_DEBUG("parsing rules: (key, value) = (%s, %s)", k, v);

    if (0 != strcmp(k, "type")) {
        RULENG_ERR("event '%s' type not found", ev);
        rc = RULENG_COM_ERR_RULES_NOT_VALID;
        goto cleanup_key;
    }

    *type = strdup(v);
    if (NULL == *type) {
        RULENG_ERR("error allocating event type");
        rc = RULENG_COM_ERR_ALLOC;
        goto cleanup_key;
    }

cleanup_key:
    free(k);
exit:
    return rc;
}

static enum ruleng_com_rc
ruleng_com_parse_event_arg(struct uci_context *ctx, struct json_object *com,
                           struct uci_section *s, const char *ev,
                           struct json_object **args)
{
    enum ruleng_com_rc rc = RULENG_COM_OK;

    *args = json_object_new_object();
    if (*args == NULL) {
        RULENG_ERR("error allocating json object");
        rc = RULENG_COM_ERR_ALLOC;
        goto exit;
    }

    struct uci_option *earg = uci_lookup_option(ctx, s, "event_arg");
    struct uci_element *le = NULL;

    char *et = NULL;
    rc = ruleng_com_parse_event_type(earg, ev, &et);
    if (RULENG_COM_OK != rc)
        goto exit;

    json_object_object_add(*args, "type", json_object_new_string(et));

    bool skip = true;
    uci_foreach_element(&earg->v.list, le) {
        // TODO: Fix this...
        if (skip) {
            skip = false;
            continue;
        }
        char *k = NULL, *v = NULL;
        rc = ruleng_com_parse_key_value(le->name, &k, &v);
        if (RULENG_COM_OK != rc) {
            RULENG_ERR("invalid rule for event argument");
            goto cleanup_json;
        }

        RULENG_DEBUG("parsing rules: (key, value) = (%s, %s)", k, v);

        const char *t = NULL;
        rc = ruleng_com_get_event_arg_type(com, et, k, &t);
        if (RULENG_COM_OK != rc)
            goto cleanup_key;

        rc = ruleng_com_conv_to_json(args, k, v, t);
        if (RULENG_COM_OK != rc)
            goto cleanup_key;

    cleanup_key:
        free(k);
        if (RULENG_COM_OK != rc)
            goto cleanup_json;
    }

    RULENG_DEBUG("event arguments: %s", json_object_to_json_string(*args));
    goto exit;

cleanup_json:
    json_object_put(*args);
exit:
    free(et);
    return rc;
}

static enum ruleng_com_rc
ruleng_com_parse_method_arg(struct uci_context *ctx, struct json_object *com,
                            struct uci_section *s, const char *method,
                            struct json_object **args)
{
    enum ruleng_com_rc rc = RULENG_COM_OK;

    *args = json_object_new_object();
    if (NULL == *args) {
        RULENG_ERR("error allocating json object");
        rc = RULENG_COM_ERR_ALLOC;
        goto exit;
    }

    struct uci_option *earg = uci_lookup_option(ctx, s, "method_arg");
    struct uci_element *le = NULL;

    uci_foreach_element(&earg->v.list, le) {
        char *k = NULL, *v = NULL;
        rc = ruleng_com_parse_key_value(le->name, &k, &v);
        if (RULENG_COM_OK != rc) {
            RULENG_ERR("invalid rules");
            goto cleanup_json;
        }
        RULENG_DEBUG("(key, value) = (%s, %s)", k, v);

        const char *t = NULL;
        rc = ruleng_com_get_method_arg_type(com, method, k, &t);
        if (RULENG_COM_OK != rc)
            goto cleanup_key;

        rc = ruleng_com_conv_to_json(args, k, v, t);
        if (RULENG_COM_OK != rc)
            goto cleanup_key;

    cleanup_key:
        free(k);
        if (RULENG_COM_OK != rc)
            goto cleanup_json;
    }

    RULENG_DEBUG("method arguments: %s", json_object_to_json_string(*args));
    goto exit;

cleanup_json:
    json_object_put(*args);
exit:
    return rc;
}


enum ruleng_com_rc
ruleng_com_parse_recipes(struct ruleng_com_ctx *ctx,
                         struct ruleng_com_recipes *recipes,
                         char *path)
{
    enum ruleng_com_rc rc = RULENG_COM_OK;

    struct uci_ptr ptr;
    if (uci_lookup_ptr(ctx->uci_ctx, &ptr, path, true) != UCI_OK) {
        RULENG_ERR("error uci lookup");
        rc = RULENG_COM_ERR_RULES_NOT_FOUND;
        goto exit;
    }

    struct uci_element *e = NULL;
    uci_foreach_element(&ptr.p->sections, e) {
        struct uci_section *s = uci_to_section(e);

        char *event = NULL;
        rc = ruleng_com_parse_event(ctx->uci_ctx, s, &event);
        if (RULENG_COM_OK != rc)
            goto cleanup_recipes;

        struct json_object *args = NULL;
        rc = ruleng_com_parse_event_arg(ctx->uci_ctx, ctx->com, s, event, &args);
        if (RULENG_COM_OK != rc)
            goto cleanup_event;

        char *method = NULL, *object = NULL;
        rc = ruleng_com_parse_object_method(ctx->uci_ctx, s, &method, &object);
        if (RULENG_COM_OK != rc)
            goto cleanup_event_arg;

        struct json_object *margs = NULL;
        rc = ruleng_com_parse_method_arg(ctx->uci_ctx, ctx->com, s, method,
                                         &margs);
        if (RULENG_COM_OK != rc)
            goto cleanup_object;

        struct ruleng_com_recipe *recipe = calloc(1, sizeof *recipe);
        if (NULL == recipe) {
            RULENG_ERR("error allocating recipe");
            rc = RULENG_COM_ERR_ALLOC;
            goto cleanup_method_args;
        }

        recipe->event = event;
        recipe->args = args;
        recipe->action.object = object;
        recipe->action.method = method;
        recipe->action.args = margs;

        LN_LIST_INSERT(recipes, recipe, node);

        continue;

    cleanup_method_args:
        json_object_put(margs);
    cleanup_object:
        free(object);
    cleanup_event_arg:
        json_object_put(args);
    cleanup_event:
        free(event);
        goto cleanup_recipes;
    }

    goto exit;

cleanup_recipes:
    ruleng_com_free_recipes(recipes);
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
