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

	INIT_LIST_HEAD(&ctx->json_rules);
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

	INIT_LIST_HEAD(&_ctx->json_rules);
	return 0;
}

static void test_rulengd_invalid_recipes(void **state)
{
	struct test_env *e = (struct test_env *) *state;
	struct ruleng_json_rule *r = NULL;
	struct ruleng_bus_ctx *ctx = e->r_ctx;

	ruleng_process_json(ctx->com_ctx, &ctx->json_rules, "ruleng-test-recipe");
	/* no rule should be added, always fail if enter foreach */
	list_for_each_entry(r, &ctx->json_rules, list)
		assert_int_equal(0, 1);

	json_object_set_by_string(&e->obj, "test_rule.if", "1", json_type_int);
	json_object_set_by_string(&e->obj, "test_rule.then", "1", json_type_int);
	json_object_to_file_ext("/etc/test_recipe1.json", e->obj, JSON_C_TO_STRING_PRETTY);
	ruleng_process_json(ctx->com_ctx, &ctx->json_rules, "ruleng-test-recipe");
	list_for_each_entry(r, &ctx->json_rules, list)
		assert_int_equal(0, 1);

	json_object_set_by_string(&e->obj, "test_rule.if", "[1, 2, 3]", json_type_array);
	json_object_to_file_ext("/etc/test_recipe1.json", e->obj, JSON_C_TO_STRING_PRETTY);
	ruleng_process_json(ctx->com_ctx, &ctx->json_rules, "ruleng-test-recipe");
	list_for_each_entry(r, &ctx->json_rules, list)
		assert_int_equal(0, 1);
	
	json_object_set_by_string(&e->obj, "time.event_period", "1", json_type_int);
	json_object_to_file_ext("/etc/test_recipe1.json", e->obj, JSON_C_TO_STRING_PRETTY);
	ruleng_process_json(ctx->com_ctx, &ctx->json_rules, "ruleng-test-recipe");
	list_for_each_entry(r, &ctx->json_rules, list)
		assert_int_equal(0, 1);

	json_object_set_by_string(&e->obj, "time.execution_interval", "1", json_type_int);
	json_object_to_file_ext("/etc/test_recipe1.json", e->obj, JSON_C_TO_STRING_PRETTY);
	ruleng_process_json(ctx->com_ctx, &ctx->json_rules, "ruleng-test-recipe");
	list_for_each_entry(r, &ctx->json_rules, list)
		assert_int_equal(0, 1);

	json_object_del_by_string(e->obj, "test_rule.if");
	json_object_set_by_string(&e->obj, "test_rule.if[0].event", "test.sta", json_type_string);
	json_object_to_file_ext("/etc/test_recipe1.json", e->obj, JSON_C_TO_STRING_PRETTY);
	ruleng_process_json(ctx->com_ctx, &ctx->json_rules, "ruleng-test-recipe");
	list_for_each_entry(r, &ctx->json_rules, list)
		assert_int_equal(0, 1);

	json_object_set_by_string(&e->obj, "test_rule.if[0].match.placeholder", "1", json_type_int);
	json_object_to_file_ext("/etc/test_recipe1.json", e->obj, JSON_C_TO_STRING_PRETTY);
	ruleng_process_json(ctx->com_ctx, &ctx->json_rules, "ruleng-test-recipe");
	list_for_each_entry(r, &ctx->json_rules, list)
		assert_int_equal(0, 1);

	json_object_set_by_string(&e->obj, "test_rule.then.object", "1", json_type_int);
	json_object_to_file_ext("/etc/test_recipe1.json", e->obj, JSON_C_TO_STRING_PRETTY);
	ruleng_process_json(ctx->com_ctx, &ctx->json_rules, "ruleng-test-recipe");
	list_for_each_entry(r, &ctx->json_rules, list)
		assert_int_equal(0, 1);

	json_object_del_by_string(e->obj, "test_rule.if");
	json_object_set_by_string(&e->obj, "test_rule.then[0].object", "template", json_type_string);
	json_object_set_by_string(&e->obj, "test_rule.then[0].method", "increment", json_type_string);
	json_object_to_file_ext("/etc/test_recipe1.json", e->obj, JSON_C_TO_STRING_PRETTY);
	ruleng_process_json(ctx->com_ctx, &ctx->json_rules, "ruleng-test-recipe");
	list_for_each_entry(r, &ctx->json_rules, list)
		assert_int_equal(0, 1);
}

static void test_rulengd_valid_recipe(void **state)
{
	struct test_env *e = (struct test_env *) *state;
	struct ruleng_json_rule *r = NULL;
	struct ruleng_bus_ctx *ctx = e->r_ctx;
	int counter = 0;

	ruleng_process_json(ctx->com_ctx, &ctx->json_rules, "ruleng-test-recipe");

	/* no rule should be added, always fail if enter foreach */
	list_for_each_entry(r, &ctx->json_rules, list)
		assert_int_equal(0, 1);

	json_object_set_by_string(&e->obj, "test_rule.if", "[]", json_type_array);
	json_object_set_by_string(&e->obj, "test_rule.then", "[]", json_type_array);
	json_object_set_by_string(&e->obj, "test_rule.if_operator", "OR", json_type_string);
	json_object_set_by_string(&e->obj, "test_rule.if_event_period", "1", json_type_int);
	json_object_set_by_string(&e->obj, "test_rule.then_exec_interval", "1", json_type_int);
	json_object_to_file_ext("/etc/test_recipe1.json", e->obj, JSON_C_TO_STRING_PRETTY);
	/* allow invalid cfg's as long as if and then keys are of correct type */
	ruleng_process_json(ctx->com_ctx, &ctx->json_rules, "ruleng-test-recipe");
	list_for_each_entry(r, &ctx->json_rules, list)
		++counter;

	assert_int_equal(counter, 1);

	counter = 0;

	json_object_set_by_string(&e->obj, "test_rule.if[0].event", "test.event", json_type_string);
	json_object_set_by_string(&e->obj, "test_rule.if[0].match.placeholder", "1", json_type_int);
	json_object_set_by_string(&e->obj, "test_rule.then[0]", "{\"object\": \"template\"}", json_type_object);
	json_object_to_file_ext("/etc/test_recipe1.json", e->obj, JSON_C_TO_STRING_PRETTY);
	ruleng_process_json(ctx->com_ctx, &ctx->json_rules, "ruleng-test-recipe");
	list_for_each_entry(r, &ctx->json_rules, list)
		++counter;

	assert_int_equal(counter, 2);
}

static int setup(void** state) {
	struct test_env *e = (struct test_env *) *state;

	e->obj = json_object_new_object();

	remove("/tmp/test_file.txt");
	remove("/etc/test_recipe1.json");
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
