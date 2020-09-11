/*
 * Copyright (C) 2018 Inteno Broadband Technology AB. All rights reserved.
 *
 * Authors:	Hrvoje Varga	<hrvoje.varga@sartura.hr>
 * 			Matija Amidzic	<matija.amidzic@sartura.hr>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <libubox/list.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <libubox/blobmsg.h>
#include <libubox/blobmsg_json.h>
#include <libubox/uloop.h>
#include <libubus.h>
#include <json-c/json.h>
#include <json-editor.h>

#include "ruleng.h"
#include "ruleng_bus.h"
#include "ruleng_json.h"
#include "ruleng_rules.h"

struct test_env {
	struct ubus_context *ctx;
	struct ruleng_bus_ctx *r_ctx;
	struct json_object *obj;
	uint32_t template_id;
	int counter;
};

static struct ruleng_json_rule *rulengd_get_json_rule(struct ruleng_bus_ctx *ctx,
		char *event)
{
	struct ruleng_json_rule *r;

	LN_LIST_FOREACH(r, &ctx->json_rules, node) {
		if (!strncmp(event, r->event.name, strlen(event)))
			return r;
	}

	return NULL;
}

static void clear_rules_init(struct ruleng_bus_ctx *ctx)
{

	ruleng_json_rules_free(&ctx->json_rules);

	LN_LIST_HEAD_INITIALIZE(ctx->json_rules);
}

static void invoke_status_cb(struct ubus_request *req, int type, struct blob_attr *msg)
{
	struct test_env *e = (struct test_env *) req->priv;
	struct json_object *obj, *tmp;
	char *str;

	str = blobmsg_format_json(msg, true);
	assert_non_null(str);

	obj = json_tokener_parse(str);
	assert_non_null(obj);

	json_object_object_get_ex(obj, "counter", &tmp);
	assert_non_null(tmp);

	e->counter = json_object_get_int(tmp);

	free(str);
	json_object_put(obj);
}

/* invoke a proxied template object */
static void invoke_template(void **state, char *method, void *cb, void *priv)
{
	struct test_env *e = (struct test_env *) *state;
	struct blob_buf bb = {0};
	int rv;

	rv = blob_buf_init(&bb, 0);
	assert_int_equal(rv, 0);

	ubus_lookup_id(e->ctx, "template", &e->template_id); // why is this overwritten?

	rv = ubus_invoke(e->ctx, e->template_id, method, bb.head, cb, priv, 1500);
	assert_int_equal(rv, 0);

	blob_buf_free(&bb);
}

static int setup_bus_ctx(struct ruleng_bus_ctx **ctx)
{
	struct ruleng_rules_ctx *com_ctx;

	remove("/etc/test_recipe1.json");

	ruleng_rules_ctx_init(&com_ctx);

    *ctx = calloc(1, sizeof(struct ruleng_bus_ctx));
    if (NULL == *ctx) {
        fprintf(stderr, "error allocating main bus context");
        return -1;
    }
    struct ruleng_bus_ctx *_ctx = *ctx;

    struct ubus_context *ubus_ctx = ubus_connect(NULL);
    if (NULL == ubus_ctx) {
        fprintf(stderr, "error ubus connect: /var/run/ubus.sock\n");
        return -1;
    }
    _ctx->com_ctx = com_ctx;
    _ctx->ubus_ctx = ubus_ctx;

	LN_LIST_HEAD_INITIALIZE(_ctx->json_rules);
	return 0;
}

static void test_rulengd_register_listener(void **state)
{
	struct test_env *e = (struct test_env *) *state;
	int rv;
	enum ruleng_bus_rc rc;
	struct ruleng_bus_ctx *ctx = e->r_ctx;

	/* invalid conf - no listeners */
	json_object_set_by_string(&e->obj, "if", "1", json_type_int);
	json_object_set_by_string(&e->obj, "then", "1", json_type_int);
	json_object_to_file_ext("/etc/test_recipe1.json", e->obj, JSON_C_TO_STRING_PRETTY);
	rv = ruleng_bus_register_events(ctx, "ruleng-test-recipe", &rc);

	assert_int_equal(0, rc);
	assert_int_equal(0, rv);

	/* valid conf, no event - no listeners */
	json_object_set_by_string(&e->obj, "if", "[]", json_type_array);
	json_object_set_by_string(&e->obj, "then", "[]", json_type_array);
	json_object_to_file_ext("/etc/test_recipe1.json", e->obj, JSON_C_TO_STRING_PRETTY);
	clear_rules_init(e->r_ctx);
	rv = ruleng_bus_register_events(ctx, "ruleng-test-recipe", &rc);

	assert_int_equal(0, rc);
	assert_int_equal(0, rv);

	/* valid conf, one event - one listeners */
	json_object_set_by_string(&e->obj, "if[-1].event", "test.event", json_type_string);
	json_object_to_file_ext("/etc/test_recipe1.json", e->obj, JSON_C_TO_STRING_PRETTY);
	clear_rules_init(e->r_ctx);
	rv = ruleng_bus_register_events(ctx, "ruleng-test-recipe", &rc);
	assert_int_equal(0, rc);
	assert_int_equal(1, rv);

	/* valid conf, two events - two listeners */
	json_object_set_by_string(&e->obj, "if[-1].event", "test.event2", json_type_string);
	json_object_to_file_ext("/etc/test_recipe1.json", e->obj, JSON_C_TO_STRING_PRETTY);
	clear_rules_init(e->r_ctx);
	rv = ruleng_bus_register_events(ctx, "ruleng-test-recipe", &rc);

	assert_int_equal(0, rc);
	assert_int_equal(2, rv);
}

static void test_rulengd_trigger_event_fail(void **state)
{
	struct test_env *e = (struct test_env *) *state;
	struct ruleng_json_rule *r = NULL;
	int rv;
	enum ruleng_bus_rc rc;
	struct ruleng_bus_ctx *ctx = e->r_ctx;
	struct blob_buf bb = {0};

	/* valid cfg, no match - should not increment hits */
	json_object_set_by_string(&e->obj, "if[-1].event", "test.event", json_type_string);
	json_object_set_by_string(&e->obj, "then", "[]", json_type_array);
	json_object_to_file_ext("/etc/test_recipe1.json", e->obj, JSON_C_TO_STRING_PRETTY);
	rv = ruleng_bus_register_events(ctx, "ruleng-test-recipe", &rc);
	r = rulengd_get_json_rule(ctx, "test.event");

	assert_non_null(r);

	blob_buf_init(&bb, 0);
	ruleng_event_json_cb(ctx->ubus_ctx, &ctx->json_handler, "test.event", bb.head);

	assert_int_equal(0, r->hits);

	clear_rules_init(ctx);

	json_object_set_by_string(&e->obj, "if[0].match.placeholder", "1", json_type_string);
	json_object_to_file_ext("/etc/test_recipe1.json", e->obj, JSON_C_TO_STRING_PRETTY);
	rv = ruleng_bus_register_events(ctx, "ruleng-test-recipe", &rc);
	r = rulengd_get_json_rule(ctx, "test.event");
	assert_non_null(r);

	blob_buf_init(&bb, 0);
	blobmsg_add_u32(&bb, "placeholder", 1);
	ruleng_event_json_cb(ctx->ubus_ctx, &ctx->json_handler, "test.event", bb.head);

	assert_int_equal(0, r->hits);

	blob_buf_free(&bb);
}

static void test_rulengd_trigger_event(void **state)
{
	struct test_env *e = (struct test_env *) *state;
	struct ruleng_json_rule *r = NULL;
	int rv;
	enum ruleng_bus_rc rc;
	struct ruleng_bus_ctx *ctx = e->r_ctx;
	struct blob_buf bb = {0};

	/* valid cfg, but not valid 'then' - should increment hits */
	json_object_set_by_string(&e->obj, "if[0].event", "test.event", json_type_string);
	json_object_set_by_string(&e->obj, "if[0].match.placeholder", "1", json_type_int);
	json_object_set_by_string(&e->obj, "then", "[]", json_type_array);
	json_object_to_file_ext("/etc/test_recipe1.json", e->obj, JSON_C_TO_STRING_PRETTY);
	rv = ruleng_bus_register_events(ctx, "ruleng-test-recipe", &rc);
	r = rulengd_get_json_rule(ctx, "test.event");
	assert_non_null(r);

	blob_buf_init(&bb, 0);
	blobmsg_add_u32(&bb, "placeholder", 1);
	ruleng_event_json_cb(ctx->ubus_ctx, &ctx->json_handler, "test.event", bb.head);

	assert_int_equal(1, r->hits);
	blob_buf_free(&bb);
}

static void test_rulengd_trigger_invoke_fail(void **state)
{
	struct test_env *e = (struct test_env *) *state;
	struct ruleng_json_rule *r = NULL;
	int rv;
	enum ruleng_bus_rc rc;
	struct ruleng_bus_ctx *ctx = e->r_ctx;
	struct blob_buf bb = {0};

	blob_buf_init(&bb, 0);
	blobmsg_add_u32(&bb, "placeholder", 1);

	/* valid cfg, but no method - should increment hits, no segfault! */
	json_object_set_by_string(&e->obj, "if[0].event", "test.event", json_type_string);
	json_object_set_by_string(&e->obj, "if[0].match.placeholder", "1", json_type_int);
	json_object_set_by_string(&e->obj, "then[0]", "{\"object\": \"template\"}", json_type_object);
	json_object_to_file_ext("/etc/test_recipe1.json", e->obj, JSON_C_TO_STRING_PRETTY);
	rv = ruleng_bus_register_events(ctx, "ruleng-test-recipe", &rc);
	r = rulengd_get_json_rule(ctx, "test.event");
	assert_non_null(r);
	ruleng_event_json_cb(ctx->ubus_ctx, &ctx->json_handler, "test.event", bb.head);

	assert_int_equal(1, r->hits);

	/* valid cfg, but no object - should increment hits, no segfault! */
	json_object_set_by_string(&e->obj, "then[0]", "{\"method\": \"increment\"}", json_type_object);
	json_object_to_file_ext("/etc/test_recipe1.json", e->obj, JSON_C_TO_STRING_PRETTY);
	clear_rules_init(e->r_ctx);
	rv = ruleng_bus_register_events(ctx, "ruleng-test-recipe", &rc);
	r = rulengd_get_json_rule(ctx, "test.event");
	assert_non_null(r);
	ruleng_event_json_cb(ctx->ubus_ctx, &ctx->json_handler, "test.event", bb.head);


	assert_int_equal(1, r->hits);

	blob_buf_free(&bb);
}

static void test_rulengd_trigger_invoke(void **state)
{
	struct test_env *e = (struct test_env *) *state;
	struct ruleng_json_rule *r = NULL;
	int rv;
	enum ruleng_bus_rc rc;
	struct ruleng_bus_ctx *ctx = e->r_ctx;
	struct blob_buf bb = {0};

	blob_buf_init(&bb, 0);
	blobmsg_add_u32(&bb, "placeholder", 1);

	/* valid cfg, but no args (optional!) - should increment hits, and counter! */
	json_object_set_by_string(&e->obj, "if[0].event", "test.event", json_type_string);
	json_object_set_by_string(&e->obj, "if[0].match.placeholder", "1", json_type_int);
	json_object_set_by_string(&e->obj, "then[0]", "{\"object\": \"template\", \"method\": \"increment\"}", json_type_object);
	json_object_to_file_ext("/etc/test_recipe1.json", e->obj, JSON_C_TO_STRING_PRETTY);
	rv = ruleng_bus_register_events(ctx, "ruleng-test-recipe", &rc);
	r = rulengd_get_json_rule(ctx, "test.event");
	assert_non_null(r);
	ruleng_event_json_cb(ctx->ubus_ctx, &ctx->json_handler, "test.event", bb.head);


	assert_int_equal(1, r->hits);
	invoke_template(state, "status", invoke_status_cb, e);
	assert_int_equal(1, e->counter);

	/* valid cfg, with args, albeit empty - should increment hits, and counter! */
	json_object_set_by_string(&e->obj, "then[0]", "{\"object\": \"template\", \"method\": \"increment\", \"args\":{}}", json_type_object);
	json_object_to_file_ext("/etc/test_recipe1.json", e->obj, JSON_C_TO_STRING_PRETTY);
	clear_rules_init(e->r_ctx);
	rv = ruleng_bus_register_events(ctx, "ruleng-test-recipe", &rc);
	r = rulengd_get_json_rule(ctx, "test.event");
	assert_non_null(r);
	ruleng_event_json_cb(ctx->ubus_ctx, &ctx->json_handler, "test.event", bb.head);

	assert_int_equal(1, r->hits);
	invoke_template(state, "status", invoke_status_cb, e);
	assert_int_equal(2, e->counter);

	/* valid cfg, with args - should increment hits and write to file! */
	json_object_set_by_string(&e->obj, "then[0]", "{\"object\": \"file\", \"method\": \"write\", \"args\":{\"path\":\"/tmp/test_file.txt\", \"data\":\"test write\"}}", json_type_object);
	json_object_to_file_ext("/etc/test_recipe1.json", e->obj, JSON_C_TO_STRING_PRETTY);
	clear_rules_init(e->r_ctx);
	rv = ruleng_bus_register_events(ctx, "ruleng-test-recipe", &rc);
	r = rulengd_get_json_rule(ctx, "test.event");
	assert_non_null(r);
	ruleng_event_json_cb(ctx->ubus_ctx, &ctx->json_handler, "test.event", bb.head);


	assert_int_equal(1, r->hits);
	sleep(1); /* give ubus and rulengd some time to process the request and write to fs */
	assert_int_equal(0, access("/tmp/test_file.txt", F_OK));

	blob_buf_free(&bb);
}

static void test_rulengd_trigger_invoke_multi_condition(void **state)
{
	struct test_env *e = (struct test_env *) *state;
	struct ruleng_json_rule *r = NULL;
	int rv;
	enum ruleng_bus_rc rc;
	struct ruleng_bus_ctx *ctx = e->r_ctx;
	struct blob_buf bb = {0};

	blob_buf_init(&bb, 0);
	blobmsg_add_u32(&bb, "placeholder", 1);

	/* valid cfg with multiple if conditions - no time restrictions*/
	json_object_set_by_string(&e->obj, "if[0].event", "test.event", json_type_string);
	json_object_set_by_string(&e->obj, "if[0].match.placeholder", "1", json_type_int);
	json_object_set_by_string(&e->obj, "if[1].event", "test.event.two", json_type_string);
	json_object_set_by_string(&e->obj, "if[1].match.placeholder", "1", json_type_int);
	json_object_set_by_string(&e->obj, "then[0]", "{\"object\": \"template\", \"method\": \"increment\"}", json_type_object);
	json_object_to_file_ext("/etc/test_recipe1.json", e->obj, JSON_C_TO_STRING_PRETTY);
	rv = ruleng_bus_register_events(ctx, "ruleng-test-recipe", &rc);
	r = rulengd_get_json_rule(ctx, "test.event");
	assert_non_null(r);
	/* this one will (could fail, extremely unlikely? if time(NULL) ticks between calls?) work becuase wasted_time will be 0 and wait_time is unset (0) */
	ruleng_event_json_cb(ctx->ubus_ctx, &ctx->json_handler, "test.event", bb.head);


	assert_int_equal(1, r->hits);
	invoke_template(state, "status", invoke_status_cb, e);
	assert_int_equal(0, e->counter);

	ruleng_event_json_cb(ctx->ubus_ctx, &ctx->json_handler, "test.event.two", bb.head);


	assert_int_equal(2, r->hits);
	invoke_template(state, "status", invoke_status_cb, e);
	assert_int_equal(1, e->counter);

	/* this one wont work becuase wasted_time will be 3 and wait_time is unset (0) */
	ruleng_event_json_cb(ctx->ubus_ctx, &ctx->json_handler, "test.event", bb.head);

	assert_int_equal(3, r->hits);
	invoke_template(state, "status", invoke_status_cb, e);
	assert_int_equal(1, e->counter);
	sleep(1);

	ruleng_event_json_cb(ctx->ubus_ctx, &ctx->json_handler, "test.event.two", bb.head);

	assert_int_equal(4, r->hits);
	invoke_template(state, "status", invoke_status_cb, e);
	assert_int_equal(1, e->counter);

	/* valid cfg with multiple if conditions - let events come within 3seconds of eachother*/
	json_object_set_by_string(&e->obj, "time.event_period", "3", json_type_int);
	json_object_set_by_string(&e->obj, "time.execution_interval", "1", json_type_int);
	json_object_to_file_ext("/etc/test_recipe1.json", e->obj, JSON_C_TO_STRING_PRETTY);
	clear_rules_init(e->r_ctx);
	rv = ruleng_bus_register_events(ctx, "ruleng-test-recipe", &rc);
	r = rulengd_get_json_rule(ctx, "test.event");
	assert_non_null(r);

	ruleng_event_json_cb(ctx->ubus_ctx, &ctx->json_handler, "test.event", bb.head);


	assert_int_equal(1, r->hits);

	ruleng_event_json_cb(ctx->ubus_ctx, &ctx->json_handler, "test.event.two", bb.head);

	assert_int_equal(2, r->hits);
	invoke_template(state, "status", invoke_status_cb, e);
	assert_int_equal(2, e->counter);

	ruleng_event_json_cb(ctx->ubus_ctx, &ctx->json_handler, "test.event", bb.head);

	assert_int_equal(3, r->hits);

	sleep(2);
	ruleng_event_json_cb(ctx->ubus_ctx, &ctx->json_handler, "test.event.two", bb.head);

	assert_int_equal(4, r->hits);
	invoke_template(state, "status", invoke_status_cb, e);
	assert_int_equal(3, e->counter);

	ruleng_event_json_cb(ctx->ubus_ctx, &ctx->json_handler, "test.event", bb.head);

	assert_int_equal(5, r->hits);

	sleep(4);
	ruleng_event_json_cb(ctx->ubus_ctx, &ctx->json_handler, "test.event.two", bb.head);

	assert_int_equal(6, r->hits);
	invoke_template(state, "status", invoke_status_cb, e);
	assert_int_equal(3, e->counter);

	json_object_set_by_string(&e->obj, "if[2].event", "test.event.three", json_type_string);
	json_object_set_by_string(&e->obj, "if[2].match.placeholder", "1", json_type_int);
	json_object_to_file_ext("/etc/test_recipe1.json", e->obj, JSON_C_TO_STRING_PRETTY);
	clear_rules_init(e->r_ctx);
	rv = ruleng_bus_register_events(ctx, "ruleng-test-recipe", &rc);
	r = rulengd_get_json_rule(ctx, "test.event");
	assert_non_null(r);

	ruleng_event_json_cb(ctx->ubus_ctx, &ctx->json_handler, "test.event", bb.head);

	assert_int_equal(1, r->hits);

	sleep(1);
	ruleng_event_json_cb(ctx->ubus_ctx, &ctx->json_handler, "test.event.two", bb.head);
	assert_int_equal(2, r->hits);

	sleep(1);
	ruleng_event_json_cb(ctx->ubus_ctx, &ctx->json_handler, "test.event.three", bb.head);

	assert_int_equal(3, r->hits);
	invoke_template(state, "status", invoke_status_cb, e);
	assert_int_equal(4, e->counter);

	ruleng_event_json_cb(ctx->ubus_ctx, &ctx->json_handler, "test.event", bb.head);
	assert_int_equal(4, r->hits);

	sleep(2);
	ruleng_event_json_cb(ctx->ubus_ctx, &ctx->json_handler, "test.event.two", bb.head);
	assert_int_equal(5, r->hits);

	sleep(2);
	ruleng_event_json_cb(ctx->ubus_ctx, &ctx->json_handler, "test.event.three", bb.head);

	assert_int_equal(6, r->hits);
	invoke_template(state, "status", invoke_status_cb, e);
	assert_int_equal(4, e->counter);

	blob_buf_free(&bb);
}

static void test_rulengd_trigger_invoke_multi_then(void **state)
{
	struct test_env *e = (struct test_env *) *state;
	struct ruleng_json_rule *r = NULL;
	int rv;
	enum ruleng_bus_rc rc;
	struct ruleng_bus_ctx *ctx = e->r_ctx;
	struct blob_buf bb = {0};

	blob_buf_init(&bb, 0);
	blobmsg_add_u32(&bb, "placeholder", 1);

	/* valid cfg with multiple if conditions - no time restrictions*/
	json_object_set_by_string(&e->obj, "if[0].event", "test.event", json_type_string);
	json_object_set_by_string(&e->obj, "if[0].match.placeholder", "1", json_type_int);
	json_object_set_by_string(&e->obj, "if[1].event", "test.event.two", json_type_string);
	json_object_set_by_string(&e->obj, "if[1].match.placeholder", "1", json_type_int);
	json_object_set_by_string(&e->obj, "then[0]", "{\"object\": \"template\", \"method\": \"increment\"}", json_type_object);
	json_object_set_by_string(&e->obj, "then[-1]", "{\"object\": \"template\", \"method\": \"increment\", \"args\":{}}", json_type_object);
	json_object_set_by_string(&e->obj, "time.event_period", "3", json_type_int);
	json_object_set_by_string(&e->obj, "time.execution_interval", "1", json_type_int);
	json_object_to_file_ext("/etc/test_recipe1.json", e->obj, JSON_C_TO_STRING_PRETTY);
	rv = ruleng_bus_register_events(ctx, "ruleng-test-recipe", &rc);
	r = rulengd_get_json_rule(ctx, "test.event");
	assert_non_null(r);

	/* this one will (could fail, yet extremely unlikely? if time(NULL) ticks between calls?) work becuase wasted_time will be 0 and wait_time is unset (0) */
	ruleng_event_json_cb(ctx->ubus_ctx, &ctx->json_handler, "test.event", bb.head);

	assert_int_equal(1, r->hits);
	invoke_template(state, "status", invoke_status_cb, e);
	assert_int_equal(0, e->counter);

	ruleng_event_json_cb(ctx->ubus_ctx, &ctx->json_handler, "test.event.two", bb.head);

	assert_int_equal(2, r->hits);
	invoke_template(state, "status", invoke_status_cb, e);
	assert_int_equal(2, e->counter);


	json_object_set_by_string(&e->obj, "then[-1]", "{\"object\": \"file\", \"method\": \"write\", \"args\":{\"path\":\"/tmp/test_file.txt\", \"data\":\"test write\"}}", json_type_object);
	json_object_to_file_ext("/etc/test_recipe1.json", e->obj, JSON_C_TO_STRING_PRETTY);

	assert_int_not_equal(0, access("/tmp/test_file.txt", F_OK));

	clear_rules_init(e->r_ctx);
	rv = ruleng_bus_register_events(ctx, "ruleng-test-recipe", &rc);
	r = rulengd_get_json_rule(ctx, "test.event");
	assert_non_null(r);

	/* this one will (could fail, yet extremely unlikely? if time(NULL) ticks between calls?) work becuase wasted_time will be 0 and wait_time is unset (0) */

	ruleng_event_json_cb(ctx->ubus_ctx, &ctx->json_handler, "test.event", bb.head);

	assert_int_equal(1, r->hits);
	invoke_template(state, "status", invoke_status_cb, e);
	assert_int_equal(2, e->counter);

	ruleng_event_json_cb(ctx->ubus_ctx, &ctx->json_handler, "test.event.two", bb.head);

	assert_int_equal(2, r->hits);
	invoke_template(state, "status", invoke_status_cb, e);
	assert_int_equal(4, e->counter);
	assert_int_equal(0, access("/tmp/test_file.txt", F_OK));
	blob_buf_free(&bb);
}

static void test_rulengd_execution_interval(void **state)
{
	struct test_env *e = (struct test_env *) *state;
	struct ruleng_json_rule *r = NULL;
	int rv, before, after;
	enum ruleng_bus_rc rc;
	struct ruleng_bus_ctx *ctx = e->r_ctx;
	struct blob_buf bb = {0};

	blob_buf_init(&bb, 0);
	blobmsg_add_u32(&bb, "placeholder", 1);

	json_object_set_by_string(&e->obj, "if[0].event", "test.event", json_type_string);
	json_object_set_by_string(&e->obj, "if[0].match.placeholder", "1", json_type_int);
	json_object_set_by_string(&e->obj, "if[1].event", "test.event.two", json_type_string);
	json_object_set_by_string(&e->obj, "if[1].match.placeholder", "1", json_type_int);
	json_object_set_by_string(&e->obj, "then[0]", "{\"object\": \"template\", \"method\": \"increment\"}", json_type_object);
	json_object_set_by_string(&e->obj, "then[-1]", "{\"object\": \"template\", \"method\": \"increment\", \"args\":{}}", json_type_object);
	json_object_set_by_string(&e->obj, "time.event_period", "3", json_type_int);
	json_object_set_by_string(&e->obj, "time.execution_interval", "5", json_type_int);
	json_object_to_file_ext("/etc/test_recipe1.json", e->obj, JSON_C_TO_STRING_PRETTY);
	rv = ruleng_bus_register_events(ctx, "ruleng-test-recipe", &rc);
	r = rulengd_get_json_rule(ctx, "test.event");
	assert_non_null(r);

	before = time(NULL);

	ruleng_event_json_cb(ctx->ubus_ctx, &ctx->json_handler, "test.event", bb.head);
	ruleng_event_json_cb(ctx->ubus_ctx, &ctx->json_handler, "test.event.two", bb.head);
	after = time(NULL);

	assert_true(before + 5 <= after);
	blob_buf_free(&bb);
}

static void test_rulengd_multi_recipe(void **state)
{
	struct test_env *e = (struct test_env *) *state;
	struct ruleng_json_rule *r = NULL;
	int rv;
	enum ruleng_bus_rc rc;
	struct ruleng_bus_ctx *ctx = e->r_ctx;
	struct blob_buf bb = {0};

	blob_buf_init(&bb, 0);
	blobmsg_add_u32(&bb, "placeholder", 1);

	json_object_set_by_string(&e->obj, "if[0].event", "test.event", json_type_string);
	json_object_set_by_string(&e->obj, "if[0].match.placeholder", "1", json_type_int);
	json_object_set_by_string(&e->obj, "if[1].event", "test.event.two", json_type_string);
	json_object_set_by_string(&e->obj, "if[1].match.placeholder", "1", json_type_int);
	json_object_set_by_string(&e->obj, "then[0]", "{\"object\": \"template\", \"method\": \"increment\"}", json_type_object);
	json_object_set_by_string(&e->obj, "then[-1]", "{\"object\": \"template\", \"method\": \"increment\", \"args\":{}}", json_type_object);
	json_object_set_by_string(&e->obj, "time.event_period", "3", json_type_int);
	json_object_set_by_string(&e->obj, "time.execution_interval", "0", json_type_int);
	json_object_to_file_ext("/etc/test_recipe1.json", e->obj, JSON_C_TO_STRING_PRETTY);

	json_object_set_by_string(&e->obj, "if[0].event", "test.event", json_type_string);
	json_object_set_by_string(&e->obj, "if[0].match.placeholder", "1", json_type_int);
	json_object_set_by_string(&e->obj, "if[1].event", "test.event.three", json_type_string);
	json_object_set_by_string(&e->obj, "if[1].match.placeholder", "1", json_type_int);
	json_object_to_file_ext("/etc/test_recipe2.json", e->obj, JSON_C_TO_STRING_PRETTY);

	rv = ruleng_bus_register_events(ctx, "ruleng-test-recipe", &rc);
	r = rulengd_get_json_rule(ctx, "test.event");
	assert_non_null(r);
	ruleng_event_json_cb(ctx->ubus_ctx, &ctx->json_handler, "test.event", bb.head);
	ruleng_event_json_cb(ctx->ubus_ctx, &ctx->json_handler, "test.event.two", bb.head);

	invoke_template(state, "status", invoke_status_cb, e);
	assert_int_equal(2, e->counter);

	ruleng_event_json_cb(ctx->ubus_ctx, &ctx->json_handler, "test.event.three", bb.head);

	invoke_template(state, "status", invoke_status_cb, e);
	assert_int_equal(4, e->counter);

	blob_buf_free(&bb);
}

static void test_rulengd_regex(void **state)
{
	struct test_env *e = (struct test_env *) *state;
	struct ruleng_json_rule *r = NULL;
	int rv;
	enum ruleng_bus_rc rc;
	struct ruleng_bus_ctx *ctx = e->r_ctx;
	struct blob_buf bb = {0};

	blob_buf_init(&bb, 0);
	blobmsg_add_string(&bb, "placeholder", "test.asd");

	json_object_set_by_string(&e->obj, "if[0].event", "test.event", json_type_string);
	json_object_set_by_string(&e->obj, "if[0].match.placeholder", "^test\\..*", json_type_string);
	json_object_set_by_string(&e->obj, "if[0].regex", "1", json_type_int);
	json_object_set_by_string(&e->obj, "then[0]", "{\"object\": \"template\", \"method\": \"increment\"}", json_type_object);
	json_object_to_file_ext("/etc/test_recipe1.json", e->obj, JSON_C_TO_STRING_PRETTY);
	rv = ruleng_bus_register_events(ctx, "ruleng-test-recipe", &rc);
	r = rulengd_get_json_rule(ctx, "test.event");
	assert_non_null(r);

	ruleng_event_json_cb(ctx->ubus_ctx, &ctx->json_handler, "test.event", bb.head);

	invoke_template(state, "status", invoke_status_cb, e);
	assert_int_equal(1, e->counter);

	blob_buf_free(&bb);
	memset(&bb, 0, sizeof(struct blob_buf));
	blob_buf_init(&bb, 0);
	blobmsg_add_string(&bb, "placeholder", "test");

	ruleng_event_json_cb(ctx->ubus_ctx, &ctx->json_handler, "test.event", bb.head);

	invoke_template(state, "status", invoke_status_cb, e);
	assert_int_equal(1, e->counter);
	blob_buf_free(&bb);
}

static int setup(void** state) {
	struct test_env *e = (struct test_env *) *state;

	e->obj = json_object_new_object();

	remove("/tmp/test_file.txt");
	remove("/etc/test_recipe2.json");
	invoke_template(state, "reset", NULL, NULL);
	e->counter = 0;

	return 0;
}

static int teardown(void** state) {
	struct test_env *e = (struct test_env *) *state;

	clear_rules_init(e->r_ctx);

	json_object_put(e->obj);

	return 0;
}

static int group_setup(void** state) {
	int rv;
	struct test_env *e = calloc(1, sizeof(struct test_env));

	if (!e)
		return 1;

	rv = setup_bus_ctx(&e->r_ctx);
	if (rv < 0)
		return -1;

	e->ctx = e->r_ctx->ubus_ctx;

	ubus_add_uloop(e->ctx);
	ubus_lookup_id(e->ctx, "template", &e->template_id);

	*state = e;

	return 0;
}

static int group_teardown(void** state) {
	struct test_env *e = (struct test_env *) *state;

    ruleng_rules_ctx_free(e->r_ctx->com_ctx);
	ubus_free(e->ctx);

    free(e->r_ctx);
	free(e);
	return 0;
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test_setup_teardown(test_rulengd_register_listener, setup, teardown),
		cmocka_unit_test_setup_teardown(test_rulengd_trigger_event_fail, setup, teardown),
		cmocka_unit_test_setup_teardown(test_rulengd_trigger_event, setup, teardown),
		cmocka_unit_test_setup_teardown(test_rulengd_trigger_invoke_fail, setup, teardown),
		cmocka_unit_test_setup_teardown(test_rulengd_trigger_invoke, setup, teardown), // issue on ruleng_ubus_call if tested individually due to async
		cmocka_unit_test_setup_teardown(test_rulengd_trigger_invoke_multi_condition, setup, teardown),
		cmocka_unit_test_setup_teardown(test_rulengd_trigger_invoke_multi_then, setup, teardown), // issue on ruleng_ubus_call if tested individually due to async
		cmocka_unit_test_setup_teardown(test_rulengd_execution_interval, setup, teardown), // issue on ruleng_ubus_call if tested individually due to async
		cmocka_unit_test_setup_teardown(test_rulengd_multi_recipe, setup, teardown),
		cmocka_unit_test_setup_teardown(test_rulengd_regex, setup, teardown),
	};

	return cmocka_run_group_tests(tests, group_setup, group_teardown);
}
