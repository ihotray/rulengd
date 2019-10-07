#include <syslog.h>
#include <uci.h>
#include <string.h>
#include <time.h>
#include <libubox/uloop.h>
#include <libubus.h>
#include <libubox/blobmsg_json.h>

#include "utils.h"
#include "ruleng_bus.h"
#include "ruleng_rules.h"
#include "ruleng_list.h"
#include "ruleng_json.h"

int get_json_int_object(struct json_object *obj, const char *str)
{
		json_object *tmp = NULL;
		if(obj && json_object_object_get_ex(obj,str, &tmp))
				return (json_object_get_int(tmp));
		return 0;
}
const char *get_json_string_object(struct json_object *obj, const char *str)
{
		json_object *tmp = NULL;
		if(obj && json_object_object_get_ex(obj,str, &tmp))
				return (json_object_get_string(tmp));
		return NULL;
}
static void ruleng_take_json_action(struct ubus_context *u_ctx, struct ruleng_json_rule *r)
{
	int len = json_object_array_length(r->action.args);
	for(int i=0; i<len; ++i) {
		json_object *temp = json_object_array_get_idx(r->action.args, i);
		struct ruleng_rule *rr = malloc(sizeof(struct ruleng_rule));
		json_object_object_get_ex(temp, JSON_ARGS_FIELD, &rr->action.args);
		rr->action.object = get_json_string_object(temp, JSON_OBJECT_FIELD);
		if (!rr->action.object) {
			free(rr);
			continue;
		}

		rr->action.name = get_json_string_object(temp, JSON_METHOD_FIELD);
		if (!rr->action.name) {
			free(rr);
			continue;
		}

		RULENG_INFO("calling[%s->%s]", rr->action.object, rr->action.name);
		ruleng_ubus_call(u_ctx, rr);
		if(i < len-1) {
			RULENG_INFO("sleeping for [%d]", r->time.sleep_time);
			sleep(r->time.sleep_time);
		}
		free(rr);
	}
}

void ruleng_event_json_cb(struct ubus_context *ubus_ctx, \
                struct ubus_event_handler *handler, \
                const char *type, \
                struct blob_attr *msg)
{
	time_t now = time(NULL);

    char *data = blobmsg_format_json(msg, true);
    RULENG_INFO("json_cb { \"%s\": %s }\n", type, data);
    free(data);

    struct ruleng_bus_ctx *ctx =
        container_of(handler, struct ruleng_bus_ctx, json_handler);

    struct ruleng_json_rule *r = NULL;
    LN_LIST_FOREACH(r, &ctx->json_rules, node) {
		RULENG_INFO("Process event [%s]", r->event.name);
		char *event_titles = strdup(r->event.name);
		char *orig = event_titles;
		char *event = NULL;

		for(int i=0; (event = strsep(&event_titles, JSON_EVENT_SEP)); ++i) {
			if (0 != strcmp(event, type))
				continue;

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
			if (true == match) {
				r->hits++;
				if(r->last_hit_time == 0)
					r->last_hit_time = now;
				r->time_wasted += (now - r->last_hit_time);
				r->last_hit_time = now;
				if(r->time_wasted > r->time.total_wait) {
					r->time_wasted = 0;
					r->last_hit_time = 0;
					r->rules_hit = r->rules_bitmask;
					blob_buf_free(&eargs);
					break;
				}
				B_UNSET(r->rules_hit, i);
			}
			blob_buf_free(&eargs);
		}
		if(0 == r->rules_hit ) {
			// Clear couters and take action
			r->time_wasted = 0;
			r->last_hit_time = 0;
			r->rules_hit = r->rules_bitmask;
			RULENG_INFO("All rules matched within time[%s]", r->event.name);
			ruleng_take_json_action(ubus_ctx, r);
		}
		free(orig);
    }
    return;
}

enum ruleng_bus_rc
ruleng_process_json(struct ruleng_rules_ctx *ctx, struct ruleng_json_rules *rules, char *package)
{
	enum ruleng_bus_rc rc = RULENG_BUS_OK;
	struct uci_package *p = NULL;
	struct uci_element *e = NULL;
	struct uci_section *s = NULL;

	// parse uci and initialize data
	if(uci_load(ctx->uci_ctx, package, &p)) {
		RULENG_DEBUG("failed to load uci");
		return rc;
	}
	
	uci_foreach_element(&p->sections, e) {
		s = uci_to_section(e);
		const char *r_name = uci_lookup_option_string(ctx->uci_ctx, s, JSON_RECIPE_FIELD);
		if(!r_name)
				continue;

		json_object *root = json_object_from_file(r_name);
		if(!root)
				continue;
		struct ruleng_json_rule *rule = calloc(1, sizeof(struct ruleng_json_rule));
		if(NULL == rule) {
				rc = RULENG_BUS_ERR_ALLOC;
				RULENG_ERR("Failed to allocate rule");
				json_object_put(root);
				return(rc);
		}
		struct json_object *tmp, *if_field, *then_field;
		json_object_object_get_ex(root, JSON_TIME_FIELD, &tmp);

		rule->time.total_wait = get_json_int_object(tmp, JSON_TOTAL_WAIT_FIELD);
		rule->time.sleep_time = get_json_int_object(tmp, JSON_SLEEP_FIELD);

		json_object_object_get_ex(root, JSON_IF_FIELD, &if_field);
		if (!json_object_is_type(if_field, json_type_array)) {
			RULENG_ERR("Invalid JSON recipe at 'if' key!\n");
			json_object_put(root);
			free(rule);
			continue;
		}

		rule->event.args = if_field;

		int len = json_object_array_length(rule->event.args);
		char event_name[256]={'\0'};;
		for(int i=0; i < len; ++i ) {
				B_SET(rule->rules_bitmask, i);
				json_object *temp = json_object_array_get_idx(rule->event.args, i);
				sprintf(event_name+strlen(event_name),"%s%s",get_json_string_object(temp, JSON_EVENT_FIELD),
				JSON_EVENT_SEP);
				json_object_object_get_ex(temp, JSON_REGEX_FIELD, &tmp);
				rule->regex = json_object_get_boolean(tmp);
		}
		rule->rules_hit = rule->rules_bitmask;
		rule->event.name = strdup(event_name);
		json_object_object_get_ex(root, JSON_THEN_FIELD, &then_field);
		if (!json_object_is_type(then_field, json_type_array)) {
			RULENG_ERR("Invalid JSON recipe at 'then' key!\n");
			free(rule->event.name);
			json_object_put(root);
			free(rule);
			continue;
		}
		rule->action.args = then_field;


		LN_LIST_INSERT(rules, rule, node);
		json_object_get(rule->event.args);
		json_object_get(rule->action.args);
		json_object_put(root);
	}

	uci_unload(ctx->uci_ctx, p);
	return rc;
}
