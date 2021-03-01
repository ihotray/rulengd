#include <syslog.h>
#include <uci.h>
#include <string.h>
#include <time.h>
#include <libubox/uloop.h>
#include <libubus.h>
#include <libubox/blobmsg_json.h>
#include <libubox/list.h>
#include <regex.h>

#include "utils.h"
#include "ruleng_bus.h"
#include "ruleng_rules.h"
#include "ruleng_json.h"

int get_json_int_object(struct json_object *obj, const char *str)
{
		json_object *tmp = NULL;

		if (obj && json_object_object_get_ex(obj, str, &tmp))
			return (json_object_get_int(tmp));
		return 0;
}

const char *get_json_string_object(struct json_object *obj, const char *str)
{
		json_object *tmp = NULL;

		if (obj && json_object_object_get_ex(obj, str, &tmp))
			return (json_object_get_string(tmp));
		return NULL;
}

static void ruleng_take_json_action(
  struct ubus_context *u_ctx,
  struct ruleng_json_rule *r
) {
	int len = json_object_array_length(r->action.args);

	for(int i=0; i<len; ++i) {
		json_object *temp = json_object_array_get_idx(r->action.args, i);
		struct ruleng_rule *rr = malloc(sizeof(struct ruleng_rule));
		json_object_object_get_ex(temp, JSON_ARGS_FIELD, &rr->action.args);

		rr->action.timeout = get_json_int_object(temp, JSON_TIMEOUT_FIELD);

		// if there's no "object" field, we check for a "cli" field
		// otherwise we continue with the "method" field
		rr->action.object = get_json_string_object(temp, JSON_OBJECT_FIELD);

		if (!rr->action.object) {
			rr->action.object = get_json_string_object(temp, JSON_CLI_FIELD);

			if (!rr->action.object) {
				free(rr);
				continue;
			}

			RULENG_INFO("calling [%s]", rr->action.object);
			ruleng_cli_call(rr);
		} else {
			rr->action.name = get_json_string_object(temp, JSON_METHOD_FIELD);

			if (!rr->action.name) {
				free(rr);
				continue;
			}
			
			RULENG_INFO("calling[%s->%s]", rr->action.object, rr->action.name);
			ruleng_ubus_call(u_ctx, rr);
		}

		if (i < len-1) {
			RULENG_INFO("sleeping for [%d]", r->time.sleep_time);
			sleep(r->time.sleep_time);
		}
		free(rr);
	}
}

void ruleng_event_json_cb(
  struct ubus_context *ubus_ctx,
  struct ubus_event_handler *handler,
  const char *type,
  struct blob_attr *msg
) {
	time_t now = time(NULL);

	char *data = blobmsg_format_json(msg, true);
	RULENG_INFO("json_cb { '%s': %s }\n", type, data);
	free(data);

	struct ruleng_bus_ctx *ctx =
		container_of(handler, struct ruleng_bus_ctx, json_handler);
	struct ruleng_json_rule *r = NULL;

	list_for_each_entry(r, &ctx->json_rules, list) {
		RULENG_INFO("Process event [%s]", r->event.name);
		char *event_titles = strdup(r->event.name);
		char *orig = event_titles;
		char *event = NULL;

		for(int i=0; (event = strsep(&event_titles, JSON_EVENT_SEP)); ++i) {
			int reti;
			regex_t regex_exp;
			char msgbuf[100];

			// Dont parse after reading final separator
			if (event == orig + strlen(r->event.name))
				break;

			if (!r->regex) {
				RULENG_INFO("No regex match search!\n");
				if (strcmp(event, type) != 0)
					continue;
			} else {
				RULENG_INFO("Trying to regex match!\n");
				reti = regcomp(&regex_exp, event, 0);

				if (reti) {
					RULENG_ERR("Could not compile regex\n");
					continue;
				}

				reti = regexec(&regex_exp, type, 0, NULL, 0);

				if (reti == REG_NOMATCH) {
					regfree(&regex_exp);
					continue;
				} else if (reti) {
					regerror(reti, &regex_exp, msgbuf, sizeof(msgbuf));
					RULENG_ERR("Regex match failed: %s\n", msgbuf);
					regfree(&regex_exp);
					continue;
				}

				regfree(&regex_exp);
			}

			RULENG_INFO("Event match |%s:%s|", event, type);

			json_object *jobj = json_object_array_get_idx(r->event.args, i);
			json_object *args;
			json_object_object_get_ex(jobj, JSON_MATCH_FIELD, &args);
			if (!args)
				continue;

			struct blob_buf eargs = {0};
			blob_buf_init(&eargs, 0);
			blobmsg_add_object(&eargs, args);

			bool match = false;
			match = ruleng_bus_take_action(eargs.head, msg, r->regex);

			if (true == match && r->operator == AND) {
				++r->hits;

				if (r->last_hit_time == 0)
					r->last_hit_time = now;
				
				r->time_wasted += (now - r->last_hit_time);
				r->last_hit_time = now;

				if (r->time_wasted > r->time.total_wait) {
					r->time_wasted = 0;
					r->last_hit_time = now;
					r->rules_hit = r->rules_bitmask;
					B_UNSET(r->rules_hit, i);
					blob_buf_free(&eargs);
					break;
				}

				B_UNSET(r->rules_hit, i);

				if (r->rules_hit == 0) {
					// Clear couters and take action
					r->time_wasted = 0;
					r->last_hit_time = 0;
					r->rules_hit = r->rules_bitmask;
					RULENG_INFO("All rules matched within time [%s]", event);
					ruleng_take_json_action(ubus_ctx, r);
				}
			} else if (match == true) {
				// Clear couters and take action
				r->time_wasted = 0;
				r->last_hit_time = 0;
				r->rules_hit = r->rules_bitmask;
				RULENG_INFO("One rule matched [%s]", event);
				ruleng_take_json_action(ubus_ctx, r);
			}

			blob_buf_free(&eargs);
		}

		free(orig);
	}
}

enum ruleng_bus_rc ruleng_process_json(
  struct ruleng_rules_ctx *ctx,
  struct list_head *rules,
  char *package
) {
	enum ruleng_bus_rc rc = RULENG_BUS_OK;
	struct uci_package *p = NULL;
	struct uci_element *e = NULL;
	struct uci_section *s = NULL;

	// parse uci and initialize data
	if (uci_load(ctx->uci_ctx, package, &p)) {
		RULENG_DEBUG("failed to load uci");
		return rc;
	}

	uci_foreach_element(&p->sections, e) {
		s = uci_to_section(e);
		const char *r_name = uci_lookup_option_string(ctx->uci_ctx, s, JSON_RECIPE_FIELD);

		if (!r_name)
			continue;

		json_object *root = json_object_from_file(r_name);

		if (!root)
			continue;
		
		json_object_object_foreach(root, key, val) {
			(void)key;		// Prevent compiler "not used" warning

			struct ruleng_json_rule *rule = calloc(1, sizeof(struct ruleng_json_rule));

			if (rule == NULL) {
				rc = RULENG_BUS_ERR_ALLOC;
				RULENG_ERR("Failed to allocate rule");
				json_object_put(root);
				return(rc);
			}

			struct json_object *tmp, *if_field, *then_field;

			rule->time.total_wait = get_json_int_object(val, JSON_TOTAL_WAIT_FIELD);
			rule->time.sleep_time = get_json_int_object(val, JSON_SLEEP_FIELD);

			char if_operator[64] = {0};
			snprintf(if_operator, sizeof(if_operator), "%s",
					 get_json_string_object(val, JSON_IF_OPERATOR_FIELD));

			if (!strcmp(if_operator, "AND"))
				rule->operator = AND;
			else
				rule->operator = OR;

			json_object_object_get_ex(val, JSON_IF_FIELD, &if_field);

			if (!json_object_is_type(if_field, json_type_array)) {
				RULENG_ERR("Invalid JSON recipe at 'if' key!\n");
				free(rule);
				continue;
			}

			rule->event.args = if_field;

			json_object_object_get_ex(val, JSON_REGEX_FIELD, &tmp);

			if (tmp) {
				rule->regex = json_object_get_boolean(tmp);
				RULENG_INFO("Regex set to %d\n", rule->regex);
			}

			int len = json_object_array_length(rule->event.args);
			char event_name[256] = {0};

			for(int i=0; i<len; ++i) {
				B_SET(rule->rules_bitmask, i);
				json_object *temp = json_object_array_get_idx(rule->event.args, i);
				sprintf(event_name+strlen(event_name), "%s%s",
						get_json_string_object(temp, JSON_EVENT_FIELD),
						JSON_EVENT_SEP);
			}

			rule->rules_hit = rule->rules_bitmask;
			rule->event.name = strdup(event_name);

			json_object_object_get_ex(val, JSON_THEN_FIELD, &then_field);

			if (!json_object_is_type(then_field, json_type_array)) {
				RULENG_ERR("Invalid JSON recipe at 'then' key!\n");
				free(rule->event.name);
				free(rule);
				continue;
			}

			rule->action.args = then_field;

			list_add(&rule->list, rules);
			json_object_get(rule->event.args);
			json_object_get(rule->action.args);
		}

		json_object_put(root);
	}

	uci_unload(ctx->uci_ctx, p);
	return rc;
}
