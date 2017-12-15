#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include <libubus.h>

#include <libubox/uloop.h>
#include <libubus.h>
#include <libubox/blobmsg_json.h>
#include <uci.h>
#include <json-c/json.h>

#include "ruleng_bus.h"
#include "utils.h"

struct ruleng_bus_ctx {
    struct ubus_context *ubus_ctx;
    struct ruleng_rules_ctx *com_ctx;
    struct ubus_event_handler handler;
    struct ruleng_rules rules;
};

static void
ruleng_ubus_complete_cb(struct ubus_request *req, int ret)
{
    RULENG_INFO("ubus call completed, ret = %d", ret);
    free(req);
}

static void
ruleng_ubus_data_cb(struct ubus_request *req, int type, struct blob_attr *msg)
{
    (void) req;
    (void) type;

    char *json = blobmsg_format_json(msg, true);
    RULENG_INFO("ubus call response: %s", json);
    free(json);
}

static struct ruleng_rule *
ruleng_bus_find_rule(struct ruleng_rules *rules, const char *ev)
{
    struct ruleng_rule *r = NULL;
    LN_LIST_FOREACH(r, rules, node) {
        if (0 == strcmp(r->event.name, ev))
            return r;
    }
    return NULL;
}

static struct blob_attr *
ruleng_bus_blob_find_key(struct blob_attr *b, const char *k)
{
    struct blob_attr *e = NULL;
    int r = 0;
    blob_for_each_attr(e, b, r) {
        if (0 == strcmp(k, blobmsg_name(e)))
            return e;
    }
    return NULL;
}

static bool
ruleng_bus_blob_check_subset(struct blob_attr *a, struct blob_attr *b)
{
    bool rc = false;

    struct blob_attr *e = NULL;
    int r = 0;
    blob_for_each_attr(e, a, r) {
        struct blob_attr *k = ruleng_bus_blob_find_key(b, blobmsg_name(e));
        if (NULL == k || blobmsg_type(e) != blobmsg_type(k))
            goto exit;

        switch(blobmsg_type(k)) {
        case BLOBMSG_TYPE_STRING:
            if (0 != strcmp(blobmsg_get_string(e), blobmsg_get_string(k)))
                goto exit;
            break;
        case BLOBMSG_TYPE_INT64:
            if (blobmsg_get_u64(e) != blobmsg_get_u64(k))
                goto exit;
            break;
        case BLOBMSG_TYPE_INT32:
            if (blobmsg_get_u32(e) != blobmsg_get_u32(k))
                goto exit;
            break;
        case BLOBMSG_TYPE_INT16:
            if (blobmsg_get_u16(e) != blobmsg_get_u16(k))
                goto exit;
            break;
        case BLOBMSG_TYPE_BOOL:
            if (blobmsg_get_bool(e) != blobmsg_get_bool(k))
                goto exit;
            break;
        default:
            goto exit;
        }
    }

    rc = true;
exit:
    return rc;
}

static bool
ruleng_bus_take_action(struct blob_attr *a, struct blob_attr *b)
{
    return ruleng_bus_blob_check_subset(a, b);
}

static void
ruleng_event_cb(struct ubus_context *ubus_ctx,
                struct ubus_event_handler *handler,
                const char *type,
                struct blob_attr *msg)
{
    char *data = blobmsg_format_json(msg, true);
    RULENG_INFO("{ \"%s\": %s }\n", type, data);
    free(data);

    struct ruleng_bus_ctx *ctx =
        container_of(handler, struct ruleng_bus_ctx, handler);

    struct ruleng_rule *r = ruleng_bus_find_rule(&ctx->rules, type);
    if (NULL == r) {
        RULENG_ERR("%s: rule not found", type);
        goto exit;
    }
    RULENG_INFO("%s: rule found", type);

    struct blob_buf eargs = {0};
    blob_buf_init(&eargs, 0);
    blobmsg_add_object(&eargs, r->event.args);

    if (false == ruleng_bus_take_action(eargs.head, msg))
        goto cleanup_args;

    RULENG_INFO("%s: rule exists, doing ubus call", type);

    uint32_t id;
    if (ubus_lookup_id(ubus_ctx, r->action.object, &id)) {
        RULENG_ERR("%s: failed to find ubus object", type);
        goto cleanup_args;
    }

    struct blob_buf buff = {0};
    blob_buf_init(&buff, 0);
    blobmsg_add_object(&buff, r->action.args);

    struct ubus_request *req = calloc(1, sizeof(*req));
    if (NULL == req) {
        RULENG_ERR("error allocating ubus request");
        goto cleanup_buff;
    }
    ubus_invoke_async(ubus_ctx, id, r->action.name, buff.head, req);

    req->complete_cb = ruleng_ubus_complete_cb;
    req->data_cb = ruleng_ubus_data_cb;

    ubus_complete_request_async(ubus_ctx, req);

cleanup_buff:
    blob_buf_free(&buff);
cleanup_args:
    blob_buf_free(&eargs);
exit:
    return;
}

static enum ruleng_bus_rc
ruleng_bus_register_events(struct ruleng_bus_ctx *ctx, char *rules)
{
    enum ruleng_bus_rc rc = RULENG_BUS_OK;

    LN_LIST_HEAD_INITIALIZE(ctx->rules);
    if (RULENG_RULES_OK != ruleng_rules_get(ctx->com_ctx, &ctx->rules, rules)) {
        rc = RULENG_BUS_ERR_RULES_GET;
        goto exit;
    }

    ctx->handler.cb = ruleng_event_cb;

    struct ruleng_rule *r = NULL;
    LN_LIST_FOREACH(r, &ctx->rules, node) {
        if (ubus_register_event_handler(ctx->ubus_ctx, &ctx->handler, r->event.name)) {
            RULENG_ERR("failed to register event handler");
            rc = RULENG_BUS_ERR_REGISTER_EVENT;
            goto exit;
        }
    }

exit:
    return rc;
}

enum ruleng_bus_rc
ruleng_bus_init(struct ruleng_bus_ctx **ctx, struct ruleng_rules_ctx *com_ctx,
                char *rules, const char *sock)
{
    enum ruleng_bus_rc rc = RULENG_BUS_OK;

    *ctx = calloc(1, sizeof(struct ruleng_bus_ctx));
    if (NULL == *ctx) {
        RULENG_ERR("error allocating main bus context");
        rc = RULENG_BUS_ERR_ALLOC;
        goto exit;
    }
    struct ruleng_bus_ctx *_ctx = *ctx;

    struct ubus_context *ubus_ctx = ubus_connect(sock);
    if (NULL == ubus_ctx) {
        RULENG_ERR("error ubus connect: %s", sock);
        rc = RULENG_BUS_ERR_CONNECT;
        goto cleanup_ctx;
    }
    _ctx->com_ctx = com_ctx;
    _ctx->ubus_ctx = ubus_ctx;

    rc = ruleng_bus_register_events(_ctx, rules);
    if (RULENG_BUS_OK != rc) {
        goto cleanup_bus_ctx;
    }

    goto exit;

cleanup_bus_ctx:
    ubus_free(ubus_ctx);
cleanup_ctx:
    free(_ctx);
exit:
    return rc;
}

void
ruleng_bus_uloop_run(struct ruleng_bus_ctx *ctx)
{
    uloop_init();
    ubus_add_uloop(ctx->ubus_ctx);
    RULENG_INFO("running uloop...");
    uloop_run();
}

void
ruleng_bus_free(struct ruleng_bus_ctx *ctx)
{
    ruleng_rules_free(&ctx->rules);
    ubus_free(ctx->ubus_ctx);
    free(ctx);
}
