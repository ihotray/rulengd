#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include <libubus.h>
#include <regex.h>

#include <libubox/uloop.h>
#include <libubus.h>
#include <libubox/blobmsg_json.h>
#include <libubox/list.h>
#include <uci.h>
#include <json-c/json.h>

#include "ruleng_bus.h"
#include "ruleng_json.h"
#include "utils.h"

static void ruleng_ubus_complete_cb(struct ubus_request *req, int ret)
{
	RULENG_INFO("ubus call completed, ret = %d", ret);
	free(req);
}

static void ruleng_ubus_data_cb(
  struct ubus_request *req, int type,
  struct blob_attr *msg
) {
	(void) req;
	(void) type;

	char *json = blobmsg_format_json(msg, true);
	RULENG_INFO("ubus call response: %s", json);
	free(json);
}

static struct blob_attr *ruleng_bus_blob_find_key(
  struct blob_attr *b,
  const char *k
) {
	struct blob_attr *e = NULL;
	int r = 0;

	blob_for_each_attr(e, b, r) {
		if (strcmp(k, blobmsg_name(e)) == 0)
			return e;
	}
	return NULL;
}

static bool ruleng_bus_blob_compare_primitive(
  struct blob_attr *a,
  struct blob_attr *b,
  bool regex
) {
	bool rc = false;
	regex_t regex_exp;
	int reti;
	char msgbuf[100];

	switch(blobmsg_type(a)) {
		case BLOBMSG_TYPE_STRING:
			if (!regex) {
				if (strcmp(blobmsg_get_string(a), blobmsg_get_string(b)) != 0)
					goto exit;
			} else {
				reti = regcomp(&regex_exp, blobmsg_get_string(a), 0);

				if (reti) {
					RULENG_ERR("Could not compile regex\n");
					goto exit;
				}

				reti = regexec(&regex_exp, blobmsg_get_string(b), 0, NULL, 0);

				if (!reti) {
					RULENG_INFO("Match");
				} else if (reti == REG_NOMATCH) {
					regfree(&regex_exp);
					goto exit;
				} else {
					regerror(reti, &regex_exp, msgbuf, sizeof(msgbuf));
					RULENG_ERR("Regex match failed: %s\n", msgbuf);
					regfree(&regex_exp);
					goto exit;
				}

				regfree(&regex_exp);
			}
			break;
		case BLOBMSG_TYPE_INT64:
			if (blobmsg_get_u64(a) != blobmsg_get_u64(b))
				goto exit;
			break;
		case BLOBMSG_TYPE_INT32:
			if (blobmsg_get_u32(a) != blobmsg_get_u32(b))
				goto exit;
			break;
		case BLOBMSG_TYPE_INT16:
			if (blobmsg_get_u16(a) != blobmsg_get_u16(b))
				goto exit;
			break;
		case BLOBMSG_TYPE_BOOL:
			if (blobmsg_get_bool(a) != blobmsg_get_bool(b))
				goto exit;
			break;
		default:
			goto exit;
	}

	rc = true;
exit:
	return rc;
}

static bool ruleng_bus_blob_compare_array(
  struct blob_attr *a,
  struct blob_attr *b
) {
	char *as = blobmsg_format_json(a, true);
	char *bs = blobmsg_format_json(b, true);

	bool rc = !strcmp(as, bs);

	free(as);
	free(bs);

	return rc;
}

static bool ruleng_bus_blob_check_subset(
  struct blob_attr *a,
  struct blob_attr *b,
  bool regex
) {
	bool rc = false;
	struct blob_attr *e = NULL;
	int r = 0;

	blob_for_each_attr(e, a, r) {
		struct blob_attr *k = ruleng_bus_blob_find_key(b, blobmsg_name(e));

		if (k == NULL || blobmsg_type(e) != blobmsg_type(k))
			goto exit;

		switch(blobmsg_type(e)) {
			case BLOBMSG_TYPE_ARRAY:
				if (ruleng_bus_blob_compare_array(e, k) == false)
					goto exit;
				break;
			case BLOBMSG_TYPE_TABLE:
				goto exit;
			default:
				if (ruleng_bus_blob_compare_primitive(e, k, regex) == false)
					goto exit;
		}
	}

	rc = true;
exit:
	return rc;
}

void ruleng_ubus_call(
  struct ubus_context *ubus_ctx,
  struct ruleng_rule *r
) {
	uint32_t id;

	if (ubus_lookup_id(ubus_ctx, r->action.object, &id)) {
		RULENG_ERR("%s: failed to find ubus object", r->action.object);
		goto exit;
	}

	struct blob_buf buff = {0};
	blob_buf_init(&buff, 0);

	if (r->action.args)
		blobmsg_add_object(&buff, r->action.args);

	struct ubus_request *req = calloc(1, sizeof(*req));

	if (req == NULL) {
		RULENG_ERR("error allocating ubus request");
		goto cleanup_buff;
	}

	ubus_invoke_async(ubus_ctx, id, r->action.name, buff.head, req);

	req->complete_cb = ruleng_ubus_complete_cb;
	req->data_cb = ruleng_ubus_data_cb;

	ubus_complete_request_async(ubus_ctx, req);

cleanup_buff:
	blob_buf_free(&buff);
exit:
	return;
}

void ruleng_cli_call(struct ruleng_rule *r)
{
	FILE *fp;
	char buff[256] = {0};
	fp = popen(r->action.object, "r");

	RULENG_INFO("Command executed, result:");
	
	while (fgets(buff, sizeof(buff), fp) != NULL) {
		RULENG_INFO("%s", buff);
	}

	int ret = WEXITSTATUS(pclose(fp));
	RULENG_INFO("Exit code = %d", ret);
}

bool ruleng_bus_take_action(
  struct blob_attr *a,
  struct blob_attr *b,
  bool regex
) {
	return ruleng_bus_blob_check_subset(a, b, regex);
}

void ruleng_event_cb(
  struct ubus_context *ubus_ctx,
  struct ubus_event_handler *handler,
  const char *type,
  struct blob_attr *msg
) {
	char *data = blobmsg_format_json(msg, true);
	RULENG_INFO("{ \"%s\": %s }\n", type, data);
	free(data);

	struct ruleng_bus_ctx *ctx =
		container_of(handler, struct ruleng_bus_ctx, handler);

	struct ruleng_rule *r = NULL;

	list_for_each_entry(r, &ctx->rules, list) {
		if (strcmp(r->event.name, type) != 0)
			continue;

		struct blob_buf eargs = {0};
		blob_buf_init(&eargs, 0);
		blobmsg_add_object(&eargs, r->event.args);

		bool match = false;
		match = ruleng_bus_take_action(eargs.head, msg, false);

		if (match == false) {
			blob_buf_free(&eargs);
			continue;
		}

		RULENG_INFO("%s: found matching event name and data, doing ubus call", type);
		blob_buf_free(&eargs);

		ruleng_ubus_call(ubus_ctx, r);
	}

	return;
}

int ruleng_bus_register_events(
  struct ruleng_bus_ctx *ctx,
  char *rules,
  enum ruleng_bus_rc *rc
) {
	int listeners = 0;
	*rc = RULENG_BUS_OK;
	INIT_LIST_HEAD(&ctx->rules);

	if (ruleng_rules_get(ctx->com_ctx, &ctx->rules, rules)
		!= RULENG_RULES_OK) {
		*rc = RULENG_BUS_ERR_RULES_GET;
		goto exit;
	}

	ctx->handler.cb = ruleng_event_cb;
	struct ruleng_rule *r = NULL;

	list_for_each_entry(r, &ctx->rules, list) {
		if (ubus_register_event_handler(ctx->ubus_ctx,
			&ctx->handler, r->event.name)) {
			RULENG_ERR("failed to register event handler");
			*rc = RULENG_BUS_ERR_REGISTER_EVENT;
			goto exit;
		}

		++listeners;
	}

	INIT_LIST_HEAD(&ctx->json_rules);
	*rc = ruleng_process_json(ctx->com_ctx, &ctx->json_rules, rules);
	ctx->json_handler.cb = ruleng_event_json_cb;

	//Register for ubus events here
	struct ruleng_json_rule *jr = NULL;

	list_for_each_entry(jr, &ctx->json_rules, list) {
		for(int i=0; i<B_COUNT(jr->rules_bitmask); ++i) {
			const char *event_name = get_json_string_object(
									 	json_object_array_get_idx(
											 jr->event.args, i
										),
									 	JSON_EVENT_FIELD);

			if (!event_name)
				continue;

			RULENG_INFO("Register ubus event[%s]", event_name);

			if (ubus_register_event_handler(ctx->ubus_ctx,
				&ctx->json_handler, event_name)) {
				RULENG_ERR("failed to register event handler");
				*rc = RULENG_BUS_ERR_REGISTER_EVENT;
				goto exit;
			}

			++listeners;
		}
	}

exit:
	return listeners;
}

enum ruleng_bus_rc ruleng_bus_init(
  struct ruleng_bus_ctx **ctx,
  struct ruleng_rules_ctx *com_ctx,
  char *rules, const char *sock
) {
	enum ruleng_bus_rc rc = RULENG_BUS_OK;
	*ctx = calloc(1, sizeof(struct ruleng_bus_ctx));

	if (*ctx == NULL) {
		RULENG_ERR("error allocating main bus context");
		rc = RULENG_BUS_ERR_ALLOC;
		goto exit;
	}

	struct ruleng_bus_ctx *_ctx = *ctx;
	struct ubus_context *ubus_ctx = ubus_connect(sock);

	if (ubus_ctx == NULL) {
		RULENG_ERR("error ubus connect: %s", sock);
		rc = RULENG_BUS_ERR_CONNECT;
		goto cleanup_ctx;
	}

	_ctx->com_ctx = com_ctx;
	_ctx->ubus_ctx = ubus_ctx;
	ruleng_bus_register_events(_ctx, rules, &rc);

	if (rc != RULENG_BUS_OK)
		goto cleanup_bus_ctx;

	goto exit;

cleanup_bus_ctx:
	ubus_free(ubus_ctx);
cleanup_ctx:
	free(_ctx);
exit:
	return rc;
}

void ruleng_bus_uloop_run(struct ruleng_bus_ctx *ctx)
{
	uloop_init();
	ubus_add_uloop(ctx->ubus_ctx);
	RULENG_INFO("running uloop...");
	uloop_run();
}

void ruleng_bus_free(struct ruleng_bus_ctx *ctx)
{
	ruleng_rules_free(&ctx->rules);
	ruleng_json_rules_free(&ctx->json_rules);
	ubus_free(ctx->ubus_ctx);
	free(ctx);
}
