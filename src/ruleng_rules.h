#pragma once

#include <json-c/json.h>
#include <libubox/list.h>

enum ruleng_rules_rc {
	RULENG_RULES_OK = 0,
	RULENG_RULES_ERR_ALLOC,
	RULENG_RULES_ERR_NOT_FOUND,
	RULENG_RULES_ERR_NOT_VALID,
};

struct ruleng_rules_ctx {
	struct uci_context *uci_ctx;
};

struct ruleng_rule {
	struct list_head list;

	struct ruleng_rules_event {
		const char *name;
		struct json_object *args;
	} event;

	struct ruleng_rules_action {
		int timeout;
		const char *object;
		const char *name;
		struct json_object *args;
		struct json_object *envs;
	} action;
};

void ruleng_json_rules_free(struct list_head *rules);
void ruleng_rules_free(struct list_head *rules);
enum ruleng_rules_rc ruleng_rules_ctx_init(struct ruleng_rules_ctx **ctx);

enum ruleng_rules_rc ruleng_rules_get(
  struct ruleng_rules_ctx *ctx,
  struct list_head *rules, char *path
);

void ruleng_rules_ctx_free(struct ruleng_rules_ctx *ctx);
