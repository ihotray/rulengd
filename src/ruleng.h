
#pragma once

#include <libubus.h>

typedef enum {
    RULENG_OK = 0,
    RULENG_ERR_ALLOC,
    RULENG_ERR_UBUS_REGISTER,
} ruleng_error_e;

typedef struct ruleng_ctx_s ruleng_ctx_t;

ruleng_error_e
ruleng_init_ctx(struct ubus_context *ubus_ctx, ruleng_ctx_t **ctx);

void
ruleng_free_ctx(ruleng_ctx_t *ctx);
