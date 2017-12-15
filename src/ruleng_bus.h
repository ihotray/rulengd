
#pragma once

#include "ruleng_rules.h"

enum ruleng_bus_rc {
    RULENG_BUS_OK = 0,
    RULENG_BUS_ERR_ALLOC,
    RULENG_BUS_ERR_CONNECT,
    RULENG_BUS_ERR_PARSE_MODEL,
    RULENG_BUS_ERR_RULES_NOT_FOUND,
    RULENG_BUS_ERR_REGISTER_EVENT,
};

struct ruleng_bus_ctx;

enum ruleng_bus_rc
ruleng_bus_init(struct ruleng_bus_ctx **, struct ruleng_rules_ctx *, char *,
                const char *);
void
ruleng_bus_uloop_run(struct ruleng_bus_ctx *);

void
ruleng_bus_free(struct ruleng_bus_ctx *);
