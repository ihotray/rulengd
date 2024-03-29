#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>

#include <libubox/uloop.h>
#include <libubus.h>
#include <libubox/blobmsg_json.h>
#include <libubox/list.h>
#include <uci.h>

#include "ruleng_rules.h"
#include "ruleng_json.h"
#include "utils.h"

#define RULENG_EVENT_FIELD "event"
#define RULENG_EVENT_ARG_FIELD "event_data"
#define RULENG_METHOD_FIELD "method"
#define RULENG_METHOD_ARG_FIELD "method_data"
#define RULENG_METHOD_ENV_FIELD "method_envs"

void ruleng_json_rules_free(struct list_head *rules)
{
	struct ruleng_json_rule *rule = NULL, *tmp = NULL;

	list_for_each_entry_safe(rule, tmp, rules, list) {
		json_object_put(rule->action.args);
		json_object_put(rule->event.args);
		free(rule->event.name);
		free(rule);
	}
}

void ruleng_rules_free(struct list_head *rules)
{
	struct ruleng_rule *rule = NULL, *tmp = NULL;

	list_for_each_entry_safe(rule, tmp, rules, list) {
		json_object_put(rule->action.args);
		json_object_put(rule->action.envs);
		json_object_put(rule->event.args);
		free((void *) rule->event.name);
		free((void *) rule->action.name);
		free((void *) rule->action.object);
		free(rule);
	}
}

enum ruleng_rules_rc ruleng_rules_ctx_init(struct ruleng_rules_ctx **ctx)
{
	enum ruleng_rules_rc rc = RULENG_RULES_OK;
	*ctx = calloc(1, sizeof(**ctx));

	if (*ctx == NULL) {
		RULENG_ERR("failed to allocate model context");
		rc = RULENG_RULES_ERR_ALLOC;
		goto exit;
	}

	struct ruleng_rules_ctx *_ctx = *ctx;
	struct uci_context *uci_ctx = uci_alloc_context();

	if (uci_ctx == NULL) {
		RULENG_ERR("failed to allocate uci context");
		rc = RULENG_RULES_ERR_ALLOC;
		goto cleanup_ctx;
	}

	_ctx->uci_ctx = uci_ctx;

	goto exit;

cleanup_ctx:
	free(_ctx);
exit:
	return rc;
}

static enum ruleng_rules_rc ruleng_rules_rules_parse_object_method(
  struct uci_context *ctx,
  struct uci_section *s,
  char **method,
  char **object
) {
	enum ruleng_rules_rc rc = RULENG_RULES_OK;
	const char *om = uci_lookup_option_string(ctx, s, RULENG_METHOD_FIELD);

	if (om == NULL) {
		RULENG_ERR("%s: no method option", s->type);
		rc = RULENG_RULES_ERR_NOT_VALID;
		goto exit;
	}

	char *temp = strdup(om);
	char *orig_tmp = temp;

	if (temp == NULL) {
		RULENG_ERR("%s: failed to allocate object method", s->type);
		rc = RULENG_RULES_ERR_ALLOC;
		goto exit;
	}

	*object = strsep(&temp, "-");
	if (*object == NULL) {
		RULENG_ERR("%s: delimiter '-' not found", s->type);
		rc = RULENG_RULES_ERR_NOT_VALID;
		goto cleanup_temp;
	}

	if (strlen(temp) < 2 && temp[0] != '>') {
		RULENG_ERR("%s: method does not exist", s->type);
		rc = RULENG_RULES_ERR_NOT_VALID;
		goto cleanup_temp;
	}

	*object = strdup(*object);
	*method = strdup(temp + 1);

//	goto exit;

cleanup_temp:
	free(orig_tmp);
exit:
	return rc;
}

static enum ruleng_rules_rc ruleng_rules_rules_parse_event_name(
  struct uci_context *ctx,
  struct uci_section *s,
  char **ev
) {
	enum ruleng_rules_rc rc = RULENG_RULES_OK;
	const char *e = uci_lookup_option_string(ctx, s, RULENG_EVENT_FIELD);

	if (e == NULL) {
		rc = RULENG_RULES_ERR_NOT_VALID;
		goto exit;
	}

	*ev = strdup(e);

	if (*ev == NULL) {
		RULENG_ERR("%s: failed to allocate event name", s->type);
		rc = RULENG_RULES_ERR_ALLOC;
		goto exit;
	}

exit:
	return rc;
}

static void ruleng_rules_json_object_concat(
  struct json_object **src,
  struct json_object *dest
) {
	json_object_object_foreach(dest, key, val) {
		struct json_object *tmp = json_object_get(val);
		json_object_object_add(*src, key, tmp);
	}
	return;
}

static enum ruleng_rules_rc ruleng_rules_rules_parse_args(
  struct uci_context *ctx,
  const char *field,
  struct uci_section *s,
  const char *ev,
  struct json_object **args
) {
	enum ruleng_rules_rc rc = RULENG_RULES_OK;
	*args = json_object_new_object();

	if (*args == NULL) {
		RULENG_ERR("%s: failed to allocate json object", ev);
		rc = RULENG_RULES_ERR_ALLOC;
		goto exit;
	}

	struct uci_option *earg = uci_lookup_option(ctx, s, field);

	if (earg == NULL) {
		RULENG_INFO("%s: rule arguments not found", ev);
		goto exit;
	}

	struct uci_element *elem = NULL;

	uci_foreach_element(&earg->v.list, elem) {
		struct json_object *obj = json_tokener_parse(elem->name);

		if (obj == NULL) {
			RULENG_ERR("%s: rule contains invalid json object", ev);
			rc = RULENG_RULES_ERR_NOT_VALID;
			goto cleanup_json;
		}

		ruleng_rules_json_object_concat(args, obj);
		json_object_put(obj);
	}

	goto exit;

cleanup_json:
	json_object_put(*args);
exit:
	return rc;
}

static enum ruleng_rules_rc ruleng_rules_rules_parse_event(
  struct uci_context *ctx,
  struct uci_section *s,
  struct ruleng_rules_event *ev
) {
	enum ruleng_rules_rc rc = RULENG_RULES_OK;
	char *name = NULL;
	rc = ruleng_rules_rules_parse_event_name(ctx, s, &name);

	if (rc != RULENG_RULES_OK)
		goto exit;

	struct json_object *args = NULL;
	rc = ruleng_rules_rules_parse_args(ctx, RULENG_EVENT_ARG_FIELD, s, name, &args);

	if (rc != RULENG_RULES_OK)
		goto cleanup_name;

	RULENG_INFO("%s event data: %s", name, json_object_to_json_string(args));

	ev->name = name;
	ev->args = args;

	goto exit;

cleanup_name:
	free(name);
exit:
	return rc;
}

static enum ruleng_rules_rc ruleng_rules_rules_parse_action(
  struct uci_context *ctx,
  struct uci_section *s,
  struct ruleng_rules_action *ac
) {
	enum ruleng_rules_rc rc = RULENG_RULES_OK;
	char *name = NULL, *object = NULL;
	rc = ruleng_rules_rules_parse_object_method(ctx, s, &name, &object);

	if (rc != RULENG_RULES_OK)
		goto exit;

	struct json_object *args = NULL;
	rc = ruleng_rules_rules_parse_args(ctx, RULENG_METHOD_ARG_FIELD, s, name, &args);

	if (rc != RULENG_RULES_OK)
		goto cleanup_object;

	struct json_object *envs = NULL;
	rc = ruleng_rules_rules_parse_args(ctx, RULENG_METHOD_ENV_FIELD, s, name, &envs);

	if (rc != RULENG_RULES_OK)
		goto cleanup_args;

	ac->name = name;
	ac->object = object;
	ac->args = args;
	ac->envs = envs;

	goto exit;

cleanup_args:
	json_object_put(args);
cleanup_object:
	free(object);
exit:
	return rc;
}

enum ruleng_rules_rc ruleng_rules_get(
  struct ruleng_rules_ctx *ctx,
  struct list_head *rules, char *path
) {
	enum ruleng_rules_rc rc = RULENG_RULES_OK;
	struct uci_ptr ptr;

	if (uci_lookup_ptr(ctx->uci_ctx, &ptr, path, true) != UCI_OK) {
		RULENG_ERR("%s: uci lookup failed", path);
		rc = RULENG_RULES_ERR_NOT_FOUND;
		goto exit;
	}

	struct uci_element *e = NULL;

	uci_foreach_element(&ptr.p->sections, e) {
		struct uci_section *s = uci_to_section(e);
		struct ruleng_rule *rule = calloc(1, sizeof *rule);

		if (rule == NULL) {
			RULENG_ERR("%s: failed to allocate rule", path);
			rc = RULENG_RULES_ERR_ALLOC;
			goto cleanup_rules;
		}

		rc = ruleng_rules_rules_parse_event(ctx->uci_ctx, s, &rule->event);

		if (rc != RULENG_RULES_OK)
			goto cleanup_rule;

		rc = ruleng_rules_rules_parse_action(ctx->uci_ctx, s, &rule->action);

		if (rc != RULENG_RULES_OK)
			goto cleanup_event_args;

		list_add(&rule->list, rules);

		continue;

	cleanup_event_args:
		free((char *) rule->event.name);
		json_object_put(rule->event.args);
	cleanup_rule:
		if (rc == RULENG_RULES_ERR_NOT_VALID) {
			rc = RULENG_RULES_OK;
			free(rule);
		} else {
			goto cleanup_rules;
		}
	}
	uci_unload(ctx->uci_ctx, ptr.p);
	goto exit;

cleanup_rules:
	ruleng_rules_free(rules);
exit:
	return rc;
}

void ruleng_rules_ctx_free(struct ruleng_rules_ctx *ctx)
{
	uci_free_context(ctx->uci_ctx);
	free(ctx);
}
