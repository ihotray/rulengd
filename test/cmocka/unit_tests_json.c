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
    struct ruleng_bus_ctx *_ctx;

	remove("/etc/test_recipe1.json");

	ruleng_rules_ctx_init(&com_ctx);

    *ctx = calloc(1, sizeof(struct ruleng_bus_ctx));
    if (NULL == *ctx) {
        fprintf(stderr, "error allocating main bus context");
        return -1;
    }

	_ctx = *ctx;
    _ctx->com_ctx = com_ctx;

	LN_LIST_HEAD_INITIALIZE(_ctx->json_rules);
	return 0;
}

static void test_rulengd_invalid_recipes(void **state)
{
	struct test_env *e = (struct test_env *) *state;
	struct ruleng_json_rule *r = NULL;
	struct ruleng_bus_ctx *ctx = e->r_ctx;

	printf("*state = %p, e = %p\n", *state, e);

	ruleng_process_json(ctx->com_ctx, &ctx->json_rules, "ruleng-test-recipe");
	/* no rule should be added, always fail if enter foreach */
	LN_LIST_FOREACH(r, &ctx->json_rules, node) {
		assert_int_equal(0, 1);
    }

	printf("%s %d\n", __func__, __LINE__);

	json_object_set_by_string(&e->obj, "if", "1", json_type_int);
	json_object_set_by_string(&e->obj, "then", "1", json_type_int);
	json_object_to_file_ext("/etc/test_recipe1.json", e->obj, JSON_C_TO_STRING_PRETTY);
	ruleng_process_json(ctx->com_ctx, &ctx->json_rules, "ruleng-test-recipe");
	LN_LIST_FOREACH(r, &ctx->json_rules, node) {
		assert_int_equal(0, 1);
    }

	printf("%s %d\n", __func__, __LINE__);

	json_object_set_by_string(&e->obj, "if", "[1, 2, 3]", json_type_array);
	json_object_to_file_ext("/etc/test_recipe1.json", e->obj, JSON_C_TO_STRING_PRETTY);
	ruleng_process_json(ctx->com_ctx, &ctx->json_rules, "ruleng-test-recipe");
	LN_LIST_FOREACH(r, &ctx->json_rules, node) {
		assert_int_equal(0, 1);
    }

	printf("%s %d\n", __func__, __LINE__);

	json_object_del_by_string(e->obj, "if");
	json_object_set_by_string(&e->obj, "if[0].event", "test.sta", json_type_string);
	json_object_to_file_ext("/etc/test_recipe1.json", e->obj, JSON_C_TO_STRING_PRETTY);
	ruleng_process_json(ctx->com_ctx, &ctx->json_rules, "ruleng-test-recipe");
	LN_LIST_FOREACH(r, &ctx->json_rules, node) {
		assert_int_equal(0, 1);
	}

	printf("%s %d\n", __func__, __LINE__);

	json_object_set_by_string(&e->obj, "if[0].match.placeholder", "1", json_type_int);
	json_object_to_file_ext("/etc/test_recipe1.json", e->obj, JSON_C_TO_STRING_PRETTY);
	ruleng_process_json(ctx->com_ctx, &ctx->json_rules, "ruleng-test-recipe");
	LN_LIST_FOREACH(r, &ctx->json_rules, node) {
		assert_int_equal(0, 1);
	}

	printf("%s %d\n", __func__, __LINE__);

	json_object_set_by_string(&e->obj, "then.object", "1", json_type_int);
	json_object_to_file_ext("/etc/test_recipe1.json", e->obj, JSON_C_TO_STRING_PRETTY);
	ruleng_process_json(ctx->com_ctx, &ctx->json_rules, "ruleng-test-recipe");
	LN_LIST_FOREACH(r, &ctx->json_rules, node) {
		assert_int_equal(0, 1);
	}

	printf("%s %d\n", __func__, __LINE__);

	json_object_del_by_string(e->obj, "if");
	json_object_set_by_string(&e->obj, "then[0].object", "template", json_type_string);
	json_object_set_by_string(&e->obj, "then[0].method", "increment", json_type_string);
	json_object_to_file_ext("/etc/test_recipe1.json", e->obj, JSON_C_TO_STRING_PRETTY);
	ruleng_process_json(ctx->com_ctx, &ctx->json_rules, "ruleng-test-recipe");
	LN_LIST_FOREACH(r, &ctx->json_rules, node) {
		assert_int_equal(0, 1);
	}

	printf("%s %d\n", __func__, __LINE__);
}

static void test_rulengd_valid_recipe(void **state)
{
	struct test_env *e = (struct test_env *) *state;
	struct ruleng_json_rule *r = NULL;
	struct ruleng_bus_ctx *ctx = e->r_ctx;

	ruleng_process_json(ctx->com_ctx, &ctx->json_rules, "ruleng-test-recipe");

	/* no rule should be added, always fail if enter foreach */
	LN_LIST_FOREACH(r, &ctx->json_rules, node) {
		assert_int_equal(0, 1);
    }

	json_object_set_by_string(&e->obj, "if", "[]", json_type_array);
	json_object_set_by_string(&e->obj, "then", "[]", json_type_array);
	json_object_to_file_ext("/etc/test_recipe1.json", e->obj, JSON_C_TO_STRING_PRETTY);
	/* allow invalid cfg's as long as if and then keys are of correct type */
	ruleng_process_json(ctx->com_ctx, &ctx->json_rules, "ruleng-test-recipe");
	LN_LIST_FOREACH(r, &ctx->json_rules, node) {
		assert_int_equal(1, 1);
	}
}

static int setup(void** state) {
	struct test_env *e = (struct test_env *) *state;

	e->obj = json_object_new_object();

	remove("/tmp/test_file.txt");
	remove("/etc/test_recipe2.json");
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

	*state = e;

	return 0;
}

static int group_teardown(void** state) {
	struct test_env *e = (struct test_env *) *state;

	ruleng_rules_ctx_free(e->r_ctx->com_ctx);

    free(e->r_ctx);
	free(e);
	return 0;
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test_setup_teardown(test_rulengd_invalid_recipes, setup, teardown), // unit
		cmocka_unit_test_setup_teardown(test_rulengd_valid_recipe, setup, teardown), // unit
	};

	return cmocka_run_group_tests(tests, group_setup, group_teardown);
}
