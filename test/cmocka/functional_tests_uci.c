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
	ruleng_rules_free(&ctx->rules);
	INIT_LIST_HEAD(&ctx->rules);
}

static void test_rulengd_test_event_uci(void **state)
{
	struct test_env *e = (struct test_env *) *state;
	struct ruleng_bus_ctx *ctx = e->r_ctx;
	struct blob_buf bb = {0};
	void *array;

	blob_buf_init(&bb, 0);
	blobmsg_add_u32(&bb, "radio", 0);
	blobmsg_add_u32(&bb, "reason", 1);
	array = blobmsg_open_array(&bb, "channels");
	blobmsg_add_u32(&bb , "", 1);
	blobmsg_add_u32(&bb, "", 2);
	blobmsg_add_u32(&bb, "", 3);
	blobmsg_close_array(&bb, array);

	ruleng_event_cb(ctx->ubus_ctx, &ctx->handler, "wifi.radio.channel_changed", bb.head);

	sleep(1); /* give the request some time to be processed */

	assert_int_equal(0, access("/tmp/test_file.txt", F_OK));
	blob_buf_free(&bb);
}

static void test_rulengd_test_event_uci_fail(void **state)
{
	struct test_env *e = (struct test_env *) *state;

	struct ruleng_bus_ctx *ctx = e->r_ctx;
	struct blob_buf bb = {0};
	void *array;

	blob_buf_init(&bb, 0);
	blobmsg_add_u32(&bb, "radio", 1);
	blobmsg_add_u32(&bb, "reason", 1);
	array = blobmsg_open_array(&bb, "channels");
	blobmsg_add_u32(&bb, "", 1);
	blobmsg_add_u32(&bb, "", 2);
	blobmsg_add_u32(&bb, "", 3);
	blobmsg_close_array(&bb, array);

	ruleng_event_cb(ctx->ubus_ctx, &ctx->handler, "wifi.radio.channel_changed", bb.head);

	sleep(1); /* give the request some time to be processed */

	assert_int_equal(-1, access("/tmp/test_file.txt", F_OK));
	blob_buf_free(&bb);
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

	remove("/etc/test_recipe.json");

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

	INIT_LIST_HEAD(&_ctx->rules);
	if (RULENG_RULES_OK != ruleng_rules_get(_ctx->com_ctx, &_ctx->rules, "ruleng-test-uci")) {
		return -1;
	}

	return 0;
}

static int setup(void** state) {
	struct test_env *e = (struct test_env *) *state;

	remove("/tmp/test_file.txt");
	invoke_template(state, "reset", NULL, NULL);
	e->counter = 0;

	return 0;
}

static int teardown(void** state) {
	struct test_env *e = (struct test_env *) *state;

	clear_rules_init(e->r_ctx);

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
		cmocka_unit_test_setup_teardown(test_rulengd_test_event_uci, setup, teardown),
		cmocka_unit_test_setup_teardown(test_rulengd_test_event_uci_fail, setup, teardown),
	};

	return cmocka_run_group_tests(tests, group_setup, group_teardown);
}
