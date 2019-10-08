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
	uint32_t template_id;
	int counter;
};

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

static void test_rulengd_barebones_recipe(void **state)
{
	struct test_env *e = (struct test_env *) *state;
	struct ruleng_bus_ctx *ctx;
	struct ruleng_rules_ctx *com_ctx = calloc(1, sizeof(*com_ctx));
	struct blob_buf bb = {0};
	void *array;

	ruleng_rules_ctx_init(&com_ctx);

	int error = ruleng_bus_init(&ctx, com_ctx, "ruleng-test-recipe", "/var/run/ubus.sock");

	printf("ctx ptr = %p\n", ctx);

	blob_buf_init(&bb, 0);
	//blobmsg_add_u32(&bb, "placeholder", 1);

	//ubus_send_event(ctx->ubus_ctx, "test.client", bb.head);

	ruleng_event_json_cb(ctx->ubus_ctx, &ctx->json_handler, "test.event", bb.head);

	assert_int_equal(error, 0);

	invoke_template(state, "status", invoke_status_cb, e);

	printf("e->counter=%d\n", e->counter);

	assert_int_equal(1, e->counter);
}

static int setup_bus_ctx(struct ruleng_bus_ctx **ctx)
{
	struct ruleng_rules_ctx *com_ctx = calloc(1, sizeof(struct ruleng_rules_ctx));

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

static void test_rulengd_invalid_recipes(void **state)
{
	struct test_env *e = (struct test_env *) *state;
	struct json_object *obj = json_object_new_object();
	struct ruleng_json_rule *r = NULL;
	int rc;
	struct ruleng_bus_ctx *ctx;

	rc = setup_bus_ctx(&ctx);
	if (rc < 0)
		return -1;

	ruleng_process_json(ctx->com_ctx, &ctx->json_rules, "ruleng-test-recipe");

	/* no rule should be added, always fail if enter foreach */
	LN_LIST_FOREACH(r, &ctx->json_rules, node) {
		assert_int_equal(0, 1);
    }

	json_object_set_by_string(&obj, "if", "1", json_type_int);
	json_object_set_by_string(&obj, "then", "1", json_type_int);
	json_object_to_file_ext("/etc/test_recipe1.json", obj, JSON_C_TO_STRING_PRETTY);


	ruleng_process_json(ctx->com_ctx, &ctx->json_rules, "ruleng-test-recipe");
	LN_LIST_FOREACH(r, &ctx->json_rules, node) {
		assert_int_equal(0, 1);
    }

	json_object_set_by_string(&obj, "if", "[1, 2, 3]", json_type_array);
	json_object_to_file_ext("/etc/test_recipe1.json", obj, JSON_C_TO_STRING_PRETTY);

	ruleng_process_json(ctx->com_ctx, &ctx->json_rules, "ruleng-test-recipe");
	LN_LIST_FOREACH(r, &ctx->json_rules, node) {
		assert_int_equal(0, 1);
    }

	json_object_del_by_string(obj, "if");
	json_object_set_by_string(&obj, "if[0].event", "test.sta", json_type_string);
	json_object_to_file_ext("/etc/test_recipe1.json", obj, JSON_C_TO_STRING_PRETTY);

	ruleng_process_json(ctx->com_ctx, &ctx->json_rules, "ruleng-test-recipe");
	LN_LIST_FOREACH(r, &ctx->json_rules, node) {
		assert_int_equal(0, 1);
	}

	json_object_set_by_string(&obj, "if[0].match.placeholder", "1", json_type_int);
	json_object_to_file_ext("/etc/test_recipe1.json", obj, JSON_C_TO_STRING_PRETTY);

	ruleng_process_json(ctx->com_ctx, &ctx->json_rules, "ruleng-test-recipe");
	LN_LIST_FOREACH(r, &ctx->json_rules, node) {
		assert_int_equal(0, 1);
	}

	json_object_set_by_string(&obj, "then.object", "1", json_type_int);
	json_object_to_file_ext("/etc/test_recipe1.json", obj, JSON_C_TO_STRING_PRETTY);

	ruleng_process_json(ctx->com_ctx, &ctx->json_rules, "ruleng-test-recipe");
	LN_LIST_FOREACH(r, &ctx->json_rules, node) {
		assert_int_equal(0, 1);
	}

	json_object_del_by_string(obj, "if");
	json_object_set_by_string(&obj, "then[0].object", "template", json_type_string);
	json_object_set_by_string(&obj, "then[0].method", "increment", json_type_string);
	json_object_to_file_ext("/etc/test_recipe1.json", obj, JSON_C_TO_STRING_PRETTY);
	ruleng_process_json(ctx->com_ctx, &ctx->json_rules, "ruleng-test-recipe");
	LN_LIST_FOREACH(r, &ctx->json_rules, node) {
		assert_int_equal(0, 1);
	}
}

static void test_rulengd_valid_recipe(void **state)
{
	struct test_env *e = (struct test_env *) *state;
	struct json_object *obj = json_object_new_object();
	struct ruleng_json_rule *r = NULL;
	int rc;
	struct ruleng_bus_ctx *ctx;

	rc = setup_bus_ctx(&ctx);
	if (rc < 0)
		return -1;
	ruleng_process_json(ctx->com_ctx, &ctx->json_rules, "ruleng-test-recipe");

	/* no rule should be added, always fail if enter foreach */
	LN_LIST_FOREACH(r, &ctx->json_rules, node) {
		assert_int_equal(0, 1);
    }

	json_object_set_by_string(&obj, "if", "[]", json_type_array);
	json_object_set_by_string(&obj, "then", "[]", json_type_array);
	printf("%s %d obj=%s\n", __func__, __LINE__, json_object_get_string(obj));
	json_object_to_file_ext("/etc/test_recipe1.json", obj, JSON_C_TO_STRING_PRETTY);

	/* allow invalid cfg's as long as if and then keys are of correct type */
	ruleng_process_json(ctx->com_ctx, &ctx->json_rules, "ruleng-test-recipe");
	LN_LIST_FOREACH(r, &ctx->json_rules, node) {
		assert_int_equal(1, 1);
	}
}

static void test_rulengd_register_listener(void **state)
{
	struct test_env *e = (struct test_env *) *state;
	struct json_object *obj = json_object_new_object();
	struct ruleng_json_rule *r = NULL;
	int rv;
	enum ruleng_bus_rc rc;
	struct ruleng_bus_ctx *ctx;

	rv = setup_bus_ctx(&ctx);
	if (rv < 0)
		return -1;

	/* invalid conf - no listeners */
	json_object_set_by_string(&obj, "if", "1", json_type_int);
	json_object_set_by_string(&obj, "then", "1", json_type_int);
	json_object_to_file_ext("/etc/test_recipe1.json", obj, JSON_C_TO_STRING_PRETTY);

	rv = ruleng_bus_register_events(ctx, "ruleng-test-recipe", &rc);

	assert_int_equal(0, rc);
	assert_int_equal(0, rv);

	/* valid conf, no event - no listeners */
	json_object_set_by_string(&obj, "if", "[]", json_type_array);
	json_object_set_by_string(&obj, "then", "[]", json_type_array);
	printf("%s %d obj=%s\n", __func__, __LINE__, json_object_get_string(obj));
	json_object_to_file_ext("/etc/test_recipe1.json", obj, JSON_C_TO_STRING_PRETTY);

	rv = ruleng_bus_register_events(ctx, "ruleng-test-recipe", &rc);

	assert_int_equal(0, rc);
	assert_int_equal(0, rv);

	/* valid conf, one event - one listeners */
	json_object_set_by_string(&obj, "if[-1].event", "test.event", json_type_string);
	json_object_to_file_ext("/etc/test_recipe1.json", obj, JSON_C_TO_STRING_PRETTY);

	rv = ruleng_bus_register_events(ctx, "ruleng-test-recipe", &rc);

	assert_int_equal(0, rc);
	assert_int_equal(1, rv);

	/* valid conf, two events - two listeners */
	json_object_set_by_string(&obj, "if[-1].event", "test.event2", json_type_string);
	json_object_to_file_ext("/etc/test_recipe1.json", obj, JSON_C_TO_STRING_PRETTY);

	rv = ruleng_bus_register_events(ctx, "ruleng-test-recipe", &rc);

	assert_int_equal(0, rc);
	assert_int_equal(2, rv);
}

static void test_rulengd_trigger_event_fail(void **state)
{
	struct test_env *e = (struct test_env *) *state;
	struct json_object *obj = json_object_new_object();
	struct ruleng_json_rule *r = NULL;
	int rv;
	enum ruleng_bus_rc rc;
	struct ruleng_bus_ctx *ctx;
	struct blob_buf bb = {0};

	rv = setup_bus_ctx(&ctx);
	if (rv < 0)
		return -1;

	/* valid cfg, no match - should not increment hits */
	json_object_set_by_string(&obj, "if[-1].event", "test.event", json_type_string);
	json_object_set_by_string(&obj, "then", "[]", json_type_array);
	json_object_to_file_ext("/etc/test_recipe1.json", obj, JSON_C_TO_STRING_PRETTY);

	rv = ruleng_bus_register_events(ctx, "ruleng-test-recipe", &rc);
	LN_LIST_FOREACH(r, &ctx->json_rules, node) {
		if (!strncmp("test.event", r->event.name, strlen("test.event"))) {
			break;
		}
	}
	assert_non_null(r);

	blob_buf_init(&bb, 0);
	ruleng_event_json_cb(ctx->ubus_ctx, &ctx->json_handler, "test.event", bb.head);
	assert_int_equal(0, r->hits);
}

static void test_rulengd_trigger_event(void **state)
{
	struct test_env *e = (struct test_env *) *state;
	struct json_object *obj = json_object_new_object();
	struct ruleng_json_rule *r = NULL;
	int rv;
	enum ruleng_bus_rc rc;
	struct ruleng_bus_ctx *ctx;
	struct blob_buf bb = {0};

	rv = setup_bus_ctx(&ctx);
	if (rv < 0)
		return -1;

	/* valid cfg, but not valid 'then' - should increment hits */
	json_object_set_by_string(&obj, "if[0].event", "test.event", json_type_string);
	json_object_set_by_string(&obj, "if[0].match.placeholder", "1", json_type_int);
	json_object_set_by_string(&obj, "then", "[]", json_type_array);
	json_object_to_file_ext("/etc/test_recipe1.json", obj, JSON_C_TO_STRING_PRETTY);

	rv = ruleng_bus_register_events(ctx, "ruleng-test-recipe", &rc);
	LN_LIST_FOREACH(r, &ctx->json_rules, node) {
		printf("%s %d: event=%s\n", __func__, __LINE__, r->event.name);
		if (!strncmp("test.event", r->event.name, strlen("test.event"))) {
			break;
		}
	}

	blob_buf_init(&bb, 0);
	blobmsg_add_u32(&bb, "placeholder", 1);
	ruleng_event_json_cb(ctx->ubus_ctx, &ctx->json_handler, "test.event", bb.head);
	assert_int_equal(1, r->hits);
}

static void test_rulengd_trigger_invoke_fail(void **state)
{
	struct test_env *e = (struct test_env *) *state;
	struct json_object *obj = json_object_new_object();
	struct ruleng_json_rule *r = NULL;
	int rv;
	enum ruleng_bus_rc rc;
	struct ruleng_bus_ctx *ctx;
	struct blob_buf bb = {0};

	rv = setup_bus_ctx(&ctx);
	if (rv < 0)
		return -1;

	/* valid cfg, but no method - should increment hits, no segfault! */
	json_object_set_by_string(&obj, "if[0].event", "test.event", json_type_string);
	json_object_set_by_string(&obj, "if[0].match.placeholder", "1", json_type_int);
	json_object_set_by_string(&obj, "then[0]", "{\"object\": \"template\"}", json_type_object);
	json_object_to_file_ext("/etc/test_recipe1.json", obj, JSON_C_TO_STRING_PRETTY);

	blob_buf_init(&bb, 0);
	blobmsg_add_u32(&bb, "placeholder", 1);
	rv = ruleng_bus_register_events(ctx, "ruleng-test-recipe", &rc);
	LN_LIST_FOREACH(r, &ctx->json_rules, node) {
		if (!strncmp("test.event", r->event.name, strlen("test.event"))) {
			break;
		}
	}
	ruleng_event_json_cb(ctx->ubus_ctx, &ctx->json_handler, "test.event", bb.head);
	assert_int_equal(1, r->hits);

	/* valid cfg, but no object - should increment hits, no segfault! */
	json_object_set_by_string(&obj, "then[0]", "{\"method\": \"increment\"}", json_type_object);
	json_object_to_file_ext("/etc/test_recipe1.json", obj, JSON_C_TO_STRING_PRETTY);

	rv = ruleng_bus_register_events(ctx, "ruleng-test-recipe", &rc);
	LN_LIST_FOREACH(r, &ctx->json_rules, node) {
		if (!strncmp("test.event", r->event.name, strlen("test.event"))) {
			break;
		}
	}
	ruleng_event_json_cb(ctx->ubus_ctx, &ctx->json_handler, "test.event", bb.head);
	assert_int_equal(1, r->hits);
}

static void test_rulengd_trigger_invoke(void **state)
{
	struct test_env *e = (struct test_env *) *state;
	struct json_object *obj = json_object_new_object();
	struct ruleng_json_rule *r = NULL;
	int rv;
	enum ruleng_bus_rc rc;
	struct ruleng_bus_ctx *ctx;
	struct blob_buf bb = {0};

	rv = setup_bus_ctx(&ctx);
	if (rv < 0)
		return;

	/* valid cfg, but no args (optional!) - should increment hits, and counter! */
	json_object_set_by_string(&obj, "if[0].event", "test.event", json_type_string);
	json_object_set_by_string(&obj, "if[0].match.placeholder", "1", json_type_int);
	json_object_set_by_string(&obj, "then[0]", "{\"object\": \"template\", \"method\": \"increment\"}", json_type_object);
	json_object_to_file_ext("/etc/test_recipe1.json", obj, JSON_C_TO_STRING_PRETTY);

	blob_buf_init(&bb, 0);
	blobmsg_add_u32(&bb, "placeholder", 1);
	rv = ruleng_bus_register_events(ctx, "ruleng-test-recipe", &rc);
	LN_LIST_FOREACH(r, &ctx->json_rules, node) {
		if (!strncmp("test.event", r->event.name, strlen("test.event"))) {
			break;
		}
	}
	ruleng_event_json_cb(ctx->ubus_ctx, &ctx->json_handler, "test.event", bb.head);
	assert_int_equal(1, r->hits);
	invoke_template(state, "status", invoke_status_cb, e);
	assert_int_equal(1, e->counter);

	/* valid cfg, with args, albeit empty - should increment hits, and counter! */
	json_object_set_by_string(&obj, "then[0]", "{\"object\": \"template\", \"method\": \"increment\", \"args\":{}}", json_type_object);
	json_object_to_file("/etc/test_recipe1.json", obj);

	rv = ruleng_bus_register_events(ctx, "ruleng-test-recipe", &rc);
	LN_LIST_FOREACH(r, &ctx->json_rules, node) {
		if (!strncmp("test.event", r->event.name, strlen("test.event"))) {
			break;
		}
	}
	ruleng_event_json_cb(ctx->ubus_ctx, &ctx->json_handler, "test.event", bb.head);
	assert_int_equal(1, r->hits);
	invoke_template(state, "status", invoke_status_cb, e);
	assert_int_equal(2, e->counter);
}

static int setup(void** state) {
	struct test_env *e = (struct test_env *) *state;

	remove("/tmp/test_file.txt");
	invoke_template(state, "reset", NULL, NULL);
	e->counter = 0;

	return 0;
}

static int teardown(void** state) {
	(void) state;

	return 0;
}

static int group_setup(void** state) {
	struct test_env *e = calloc(1, sizeof(struct test_env));

	if (!e)
		return 1;

	e->ctx = ubus_connect(NULL);
	if (!e->ctx)
		return 1;

	ubus_add_uloop(e->ctx);
	ubus_lookup_id(e->ctx, "template", &e->template_id);


	*state = e;
	return 0;
}

static int group_teardown(void** state) {
	struct test_env *e = (struct test_env *) *state;

	uloop_done();
	ubus_free(e->ctx);
	free(e);
	return 0;
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		//cmocka_unit_test_setup_teardown(test_rulengd_barebones_recipe, setup, teardown),
		cmocka_unit_test_setup_teardown(test_rulengd_invalid_recipes, setup, teardown),
		//cmocka_unit_test_setup_teardown(test_rulengd_valid_recipe, setup, teardown),
		//cmocka_unit_test_setup_teardown(test_rulengd_register_listener, setup, teardown),
		//cmocka_unit_test_setup_teardown(test_rulengd_trigger_event_fail, setup, teardown),
		//cmocka_unit_test_setup_teardown(test_rulengd_trigger_event, setup, teardown),
		//cmocka_unit_test_setup_teardown(test_rulengd_trigger_invoke_fail, setup, teardown),
		//cmocka_unit_test_setup_teardown(test_rulengd_trigger_invoke, setup, teardown),
	};

	return cmocka_run_group_tests(tests, group_setup, group_teardown);
}
