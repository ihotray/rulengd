
#pragma once

#include <libubox/uloop.h>
#include <libubus.h>
#include <libubox/blobmsg_json.h>
#include "ruleng_rules.h"
#include "ruleng_json.h"

enum ruleng_bus_rc {
    RULENG_BUS_OK = 0,
    RULENG_BUS_ERR_ALLOC,
    RULENG_BUS_ERR_CONNECT,
    RULENG_BUS_ERR_RULES_NOT_FOUND,
    RULENG_BUS_ERR_RULES_GET,
    RULENG_BUS_ERR_REGISTER_EVENT,
};

struct ruleng_bus_ctx {
    struct ubus_context *ubus_ctx;
    struct ruleng_rules_ctx *com_ctx;
    struct ubus_event_handler handler;
    struct ubus_event_handler json_handler;
    struct ruleng_rules rules;
    struct ruleng_json_rules json_rules;
};


enum ruleng_bus_rc
ruleng_bus_init(struct ruleng_bus_ctx **, struct ruleng_rules_ctx *, char *,
                const char *);
void
ruleng_bus_uloop_run(struct ruleng_bus_ctx *);

void
ruleng_bus_free(struct ruleng_bus_ctx *);

bool
ruleng_bus_take_action(struct blob_attr *a, struct blob_attr *b, bool regex);

void
ruleng_ubus_call(struct ubus_context *ubus_ctx, struct ruleng_rule *r);

void
ruleng_event_cb(struct ubus_context *ubus_ctx,
                struct ubus_event_handler *handler,
                const char *type,
                struct blob_attr *msg);

int ruleng_bus_register_events(struct ruleng_bus_ctx *ctx, char *rules,
                enum ruleng_bus_rc *rc);