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

static void test_rulengd_non_existing_ubus_socket(void **state)
{
	struct ruleng_bus_ctx *ctx;
	struct ruleng_rules_ctx *com_ctx = calloc(1, sizeof(*com_ctx));
	int error;

	error = ruleng_bus_init(&ctx, com_ctx, "ruleng-test-uci", "invalid_ubus.sock");

	free(com_ctx);
	assert_int_equal(error, 2);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_rulengd_non_existing_ubus_socket)
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
