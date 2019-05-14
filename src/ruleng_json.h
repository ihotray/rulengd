#pragma once
#include <stdint.h>
#include <time.h>
#include "ruleng_rules.h"

#define JSON_RECIPE_FIELD "recipe"
#define JSON_TIME_FIELD "time"
#define JSON_IF_FIELD "if"
#define JSON_THEN_FIELD "then"
#define JSON_TOTAL_WAIT_FIELD "event_period"
#define JSON_SLEEP_FIELD "execution_interval"
#define JSON_EVENT_FIELD "event"
#define JSON_REGEX_FIELD "regex"
#define JSON_MATCH_FIELD "match"
#define JSON_OBJECT_FIELD "object"
#define JSON_METHOD_FIELD "method"
#define JSON_ARGS_FIELD "args"
#define JSON_EVENT_SEP "+"

/* set n-th bit in x */
#define B_SET(x, n)      ((x) |= (1<<(n)))
/* unset n-th bit in x */
#define B_UNSET(x, n)    ((x) &= ~(1<<(n)))
/* Return number of set bits in a number */
#define B_COUNT(x)     (__builtin_popcount(x))

struct ruleng_json_rule {
    LN_LIST_NODE(ruleng_json_rule) node;
	bool regex;
    struct ruleng_rules_time {
        int total_wait;
        int sleep_time;
    } time;
    struct ruleng_rules_if {
        char *name;
        struct json_object *args;
    } event;
    struct ruleng_rules_then {
        struct json_object *args;
    } action;
	uint8_t rules_bitmask;
	uint8_t rules_hit;
	int time_wasted;
	time_t last_hit_time;
};

LN_LIST_HEAD(ruleng_json_rules, ruleng_json_rule);
int get_file_contents(const char* filename, char** outbuffer);

enum ruleng_bus_rc
ruleng_process_json(struct ruleng_rules_ctx *ctx, struct ruleng_json_rules *rules, char *package);

void ruleng_event_json_cb(struct ubus_context *ubus_ctx, struct ubus_event_handler *handler,
                const char *type, struct blob_attr *msg);

int get_json_int_object(struct json_object *obj, const char *str);
const char *get_json_string_object(struct json_object *obj, const char *str);
void ruleng_json_rules_free(struct ruleng_json_rules *rules);
