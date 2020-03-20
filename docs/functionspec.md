# Function Specification

Rulengd is a daemon implementing if-then-that rules, read from configuration
files. A rule listens for a set of ubus events, and based on the arguments of
an observed event, may invoke a set of ubus methods.

## Requirements

Four basic user requirements were identified for rulengd.

| Requirement												|
| :---														|
| [Read Configuration](#read_configuration)					|
| [Read Recipe](#read_recipe) 								|
| [Register Ubus Listener](#register_ubus_listener)			|
| [Trigger Conditions](#trigger_conditions)					|

### Read Configuration

The first and most basic requirement for rulengd is to be able to read rules
from configuration.

#### Why

The OpenWrt way of configuring a daemon is to read a UCI configuration file.
Rulengd should be able to either read a basic rule directly from UCI
configuration, or read the path to a more detailed JSON recipe.

#### How

To read and parse UCI configuration the `libuci` library is used. The default
the UCI configuration path is set by:

```
#define RULENG_DEFAULT_RULES_PATH "ruleng-test-rules"
```

Meaning it will read `/etc/config/ruleng-test-rules`, however this may be
overwritten by the flag `-r <uci_file>`.

The UCI rules are parsed in the function `ruleng_rules_get(3)`, iterating all
the sections of the type `rule` found in the configuration path passed as the
third argument `path` (originating from the `-r` flag), parsing the UCI fields
representing the rule and preparing the struct `struct ruleng_rule`.

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

Because of restrictions in UCI, there are limits to the complexity of the UCI
rules. To offer more configuration options, rulengd supports JSON recipes,
describing if-then-that logic.

#### Why

The UCI configuration file offers the possibility to configure simple rules, but
to create more complex rules with multiple dependencies and invokes through UCI
would be convoluted, make for messy configurations that are difficult to
read and parse. To support more complex rules, rulengd may also read JSON
recipes, representing more advanced if-then-that logic in a cleaner, easier to
parse way.

#### How

Similarily to UCI rules, JSON recipes are found by reading the UCI configuration
file, by iterating all the sections of type `rule`. On an encountered rule with
a `recipe` option, providing the path to a JSON recipe, the recipe found at that
path will be processed to a rule, represented by the structure
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

The JSON struct allows for more advanced configuration to be set.

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

The intra-process communication used by rulengd is built ontop of ubus.

#### Why

Setting up ubus listeners for events is a part of the _if_ condition of
if-then-that logic.

#### How

When parsing the rules in `ruleng_bus_register_events(3)`, ubus listeners are
simultaneously prepared, using the structures representing the rulengd rules,
`ruleng_rule` and `ruleng_json_rule`. For each parsed rule, the libubus API
`ubus_register_event_handler(3)` is invoked. For simple UCI configuration rules,
the callback `ruleng_event_cb` is set, whereas for JSON rules the callback is
set to `ruleng_event_json_cb`.

### Trigger Conditions

The trigger conditions represents the _then-that_ part of the if-then-that
logic.

#### Why

All type of rules support the configuration of some options (arguments), which
need to be met by the recorded event prior to triggering an invoke. To support
more complex rules, rulengd spports more configuration of arguments and
conditions which need to be met before invoking a ubus, namely
multiple conditions within some time frame.

#### How

For normal UCI rules, the callback `ruleng_event_cb` is invoked on recorded
events, iterating all the rules, parsing the ones matching this event type,
calling `ruleng_bus_take_action(3)` to determine whether the event meets the
condition of the matching rule type. On a match the ubus method specified is
invoked through `ruleng_ubus_call(2)`.

For JSON recipes, the callback `ruleng_event_json_cb` is provided with the
listener, and invoked on recorded event. The callback will iterate all rules,
each rule contains a `+` sign separated list of events it depents upon, and match
against the recorded event type. On a match rulengd will validate the arguments
for the matched rule through `ruleng_bus_take_action(3)`, on an argument match,
validate the time against through `last_hit_time`, `time_wasted` and
`total_time`. On a registered hit, unset the correspoding bit in `rules_hit`,
and if the bitmap is zero-ed out, trigger the invokes conditions through
`ruleng_take_json_action`.