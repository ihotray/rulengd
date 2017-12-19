#include <stdio.h>
#include <stdlib.h>

#include "ruleng.h"
#include "ruleng_bus.h"
#include "ruleng_rules.h"
#include "utils.h"

struct ruleng_ctx {
    struct ruleng_bus_ctx *bus_ctx;
    struct ruleng_rules_ctx *com_ctx;
};

enum ruleng_rc
ruleng_init(const char *sock,
            char *rules,
            struct ruleng_ctx **ctx)
{
    enum ruleng_rc rc = RULENG_OK;

    *ctx = malloc(sizeof(**ctx));
    if (NULL == *ctx) {
        RULENG_ERR("error allocating main context");
        rc = RULENG_ERR_ALLOC;
        goto exit;
    }
    struct ruleng_ctx *_ctx = *ctx;

    struct ruleng_rules_ctx *com_ctx = NULL;
    if (RULENG_RULES_OK != ruleng_rules_ctx_init(&com_ctx)) {
        rc = RULENG_ERR_RULES_INIT;
        goto cleanup_ctx;
    }

    struct ruleng_bus_ctx *bus_ctx = NULL;
    if (RULENG_BUS_OK != ruleng_bus_init(&bus_ctx, com_ctx, rules, sock)) {
        rc = RULENG_ERR_BUS_INIT;
        goto cleanup_com_ctx;
    }

    _ctx->bus_ctx = bus_ctx;
    _ctx->com_ctx = com_ctx;

    goto exit;

cleanup_com_ctx:
    ruleng_rules_ctx_free(com_ctx);
cleanup_ctx:
    free(_ctx);
exit:
    return rc;
}

void
ruleng_uloop_run(struct ruleng_ctx *ctx)
{
   ruleng_bus_uloop_run(ctx->bus_ctx);
}

void
ruleng_free(struct ruleng_ctx *ctx)
{
    ruleng_bus_free(ctx->bus_ctx);
    ruleng_rules_ctx_free(ctx->com_ctx);
    free(ctx);
}