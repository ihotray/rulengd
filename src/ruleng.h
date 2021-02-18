#pragma once

enum ruleng_rc {
    RULENG_OK = 0,
    RULENG_ERR_ALLOC,
    RULENG_ERR_BUS_INIT,
    RULENG_ERR_BUS_REGISTER,
    RULENG_ERR_RULES_INIT,
    RULENG_ERR_LOAD_RULES,
};

struct ruleng_ctx {
	struct ruleng_bus_ctx *bus_ctx;
	struct ruleng_rules_ctx *com_ctx;
};

enum ruleng_rc ruleng_init(
  const char *sock,
  char *rules,
  struct ruleng_ctx **ctx
);

void ruleng_uloop_run(struct ruleng_ctx *ctx);

void ruleng_free(struct ruleng_ctx *ctx);
