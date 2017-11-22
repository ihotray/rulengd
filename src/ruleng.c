
#include "ruleng.h"
#include "utils.h"

struct ruleng_ctx_s {
    struct ubus_context *ubus_ctx;
};

ruleng_error_e
ruleng_init_ctx(struct ubus_context *ubus_ctx, ruleng_ctx_t **ctx)
{
    ruleng_error_e rc = RULENG_OK;

    *ctx = malloc(sizeof(ruleng_ctx_t));
    if (NULL == *ctx) {
        RULENG_ERR("error allocating main context");
        rc = RULENG_ERR_ALLOC;
        goto exit;
    }

    ruleng_ctx_t *_ctx = *ctx;

    _ctx->ubus_ctx = ubus_ctx;
exit:
    return rc;
}

void
ruleng_free_ctx(ruleng_ctx_t *ctx)
{
    free(ctx);
}
