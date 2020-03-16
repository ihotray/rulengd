# Function Specification

Rulengd is designed to allow configuration of rules which listens for a set of
ubus events, and based on event type and argument, may invoke a set of ubus
methods.

## Requirements

Four basic user requirements were identified for rulengd.

| Requirement												|
| :---														|
| [Read Configuration](#read_configuration)					|
| [Read Recipe](#read_recipe) 								|
| [Register Ubus Listener](#register_ubus_listener)			|
| [Trigger Conditions](#trigger_conditions)					|

### Read Configuration

The first and most basic requirement for rulengd is to be able to read UCI
configuration, in order to be able to parse either a UCI configurable rule, or
the path to JSON recipe to parse. For this the `libuci` library is used to read
and parse a UCI configuration file. The default the UCI configuration path is
set by:

```
#define RULENG_DEFAULT_RULES_PATH "ruleng-test-rules"
```

Meaning it will read `/etc/config/ruleng-test-rules`, however this may be set
by the flag `-r <uci_file>`.

The UCI rules are parsed in `ruleng_rules_get(3)`, iterating all the sections
found in the configuration passed as the third argument `path` (originating
from the `-r` flag), storing the parsed rules in a `struct ruleng_rule`.

```
struct ruleng_rule {
    LN_LIST_NODE(ruleng_rule) node;
    struct ruleng_rules_event {
        const char *name;
        struct json_object *args;
    } event;
    struct ruleng_rules_action {
        const char *object;
        const char *name;
        struct json_object *args;
    } action;
};
```

### Read Recipe

Similarily to UCI rules, JSON recipes are found by reading the UCI configuration
file, by iterating all the sections of type `rule`, for the configuration file
specified by the `-r` flag. The UCI configurations is passed as the third
argument, `package`, to the `ruleng_process_json(3)` method. On an encountered
rule with a `recipe` option, the rule will be processed to a
`struct ruleng_json_rule`.

```
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
    uint8_t hits;
	int time_wasted;
	time_t last_hit_time;
};
```

The JSON struct offers a bit more advanced configuration to be set.

| Variable		| Description																								|
| :---			| :---																										|
| regex			| True if regex matching enabled for the arguments															|
| time			| Represents total_time (`event_period`) and sleep_time (`execution_interval`) 								|
| event			| Represents the `if` clause of the recipe, where name represents the name of each event, separated by `+` 	|
| action		| Represents the `then` clause of the recipe, holding the array in the args variable 						|
| rules_bitmask	| Bitmask where each bit represents an entry in the if condition 											|
| rules_hit		| Bitmask which is used to represent rules hit, by events, zero-ed out when all conditions are met 			|
| time_wasted	| Calculate time since last event hit if multiple conditions 												|
| last_hit_time	| Last time an event was hit																				|

### Register Ubus Listeners

From the structures `ruleng_rule` and `ruleng_json_rule` ubus listeners have to
be setup, which is done by `ruleng_bus_register_events(3)` which also calls the
functions parsing the configuration and recipes themselves. Iterating all parsed
rules and invoking `ubus_register_event_handler(3)`, from `libubus`, for each.
For simple UCI configuration rules, the callback `ruleng_event_cb` is set,
whereas for JSON rules the callback is set to `ruleng_event_json_cb`.

### Trigger Conditions

For normal UCI rules, the callback `ruleng_event_cb` is invoked on recorded
event, iterating all the rules and parsing the ones of the same name for their
arguments through `ruleng_bus_take_action(3)`. On a match the ubus method
provided is invoked through `ruleng_ubus_call(2)`.

For JSON recipes, the callback `ruleng_event_json_cb` is used, and invoked on
recorded event. The callback will iterate the `+` sign separated list and match
against the event name, on a match validate the arguments through
`ruleng_bus_take_action(3)`. On an argument match, validate the time against
through `last_hit_time`, `time_wasted` and `total_time`. On a registered hit,
unset the correspoding bit in `rules_hit`, and if it is zero-ed out, trigger
the invokes through `ruleng_take_json_action`.