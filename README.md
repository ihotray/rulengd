# Rule Engine

Rule engine daemon(`rulengd`) is OpenWrt version of IFTTT (If This Then That) service.
It allows user to configuring rules, to monitor for ubus events and take action when event happens.

## Overview

Rulengd registers to ubus events on the system and keeps track of each configured rule. When an event of interest is received, it compares it with the configured rule and if the expected data of the event matches with the data of the event received, it invokes the configured ubus/cli method with the given arguments.

With rulengd it is possible to create rules with actions on both northbound(Higher layer) and southbound(Lower layer):

- An example of Higher layer action, send an email when user arrive at home, by creating a rule where event is client and data is the MAC address of a specific user and the action to be executed is to sending a message to a specific email address with a configured arguments.
- An example of Lower layer action, when DSL training event is received blink Broadband LED.

With rulengd in the system, instead of creating applications to execute actions upon receiving specific events, it suffices to create corresponding rule entries to ruleng UCI configuration file, or json recipes.


## UCI Config

Example UCI rules:

```bash
config rule
    option event 'client'
    list event_data '{"action": "connect"}'
    list event_data '{"macaddr": "00:e0:4c:68:05:9a"}'
    option method 'smtp.client->send'
    list method_data '{"email": "email@domain.com"}'
    list method_data '{"data": "Alice is home"}'
```

On a received event, if it contains the key-value pairs `{"action": "connect", "macaddr": "00:e0:4c:68:05:9a"}` the method `send` is invoked through the `smtp.client` object, with the arguments `{"email":"email@domain.com", data": "Alice is home"}`, to sending an email to email@domain.com with the message "Alice is home".

```bash
config rule
    option event 'wifi.sta'
    list event_data '{"event": "assoc"}'
    list event_data '{"ifname": "wl0"}'
    option method 'smtp.client->send'
    list method_data '{"email": "email@domain.com"}'
    list method_data '{"data": "&wifi.sta->data.macaddr"}'
```

In the same way on receiving of wifi.sta event, '{ "wifi.sta": {"ifname":"wl0","event":"assoc","data":{"macaddr":"14:85:7f:17:fd:40"}} }', if it contains the key-value pairs `{"event": "assoc", "ifname": "wl0"}`, then the method `send` is invoked through the `smtp.client` ubus object, with the arguments `{"email":"email@domain.com", data": "14:85:7f:17:fd:40"}`, to send an email to `email@domain.com` with the message "14:85:7f:17:fd:40".

Note: Object and array arguments must be primitive types (we can't have object in the array).

## JSON Recipe

For more granular event(s) and method(s) mapping, it is possible to create JSON files as recipes. If JSON recipes are used, the path to the recipe that should be read by rulengd needs to be specified in the rulengd UCI configuration file:

```bash
config rule
    option recipe "/etc/recipe_1.json"
```

The JSON recipes follow a similar logic as the UCI configurations, but it allows user to club multiple events and actions.

JSON recipe provides following keys/operators which can be used within multiple root rules:

| Key/Operator | Meaning |
| ------------------ | ------------- |
| if                 | To specify condition/events for a rule |
| then               | To perform one or more action when a condition/event matches |
| if_operator        | Relation between rules, valid values are 'AND', 'OR'         |
| if_event_period    | Wait period in seconds between two events         |
| then               | Define ubus/cli action if condition matches |
| then_exec_interval | Wait time in seconds between two actions |

> Notes: 
> 1. The time-related keys(if_event_period, then_exec_interval) are necessary if the ITTT condition depends on multiple events with `if_operator` is set to *AND*.
> 2. In case of more than one condition defined for a rule but `if_operator` not defined then 'OR' shall be used to evaluate the conditions, meaning match any of the events.

### JSON Recipe examples

In the following example recipe, listens for different wps events like:
 - WPS button pressed (wps_active)
 - Client connected with WPS (wps_success)
 - WPS Timeout (wps_timeout)
 - Client failed to onboard (wps_fail)

and performs different actions in case of matching some key-value pairs.

> Note: The order of the objects in the `then` array is important as the invokes will be performed in the specified order.

```JSON
{
	"wps_active": {
		"if_operator" : "AND",	
		"if_event_period": 5,
		"if" : [
			{
				"event": "button.WPS",
				"match": {
					"action":"released"
				}
			},
			{
				"event": "wifi.ap",
				"match": {
					"event":"wps-pbc-active"
				}
			}			
		],
		"then_exec_interval" : 2,
		"then" : [
			{
				"object": "led.wps",
				"method":"set",
				"args" : {
					"state": "notice"
				}
			},
			{
				"object": "led.wps",
				"method":"set",
				"args" : {
					"state": "alert"
				},
				"timeout": 1
			}			
		]
	},
	"wps_success": {
		"if_operator" : "OR",
		"if" : [
			{
				"event": "wifi.ap",
				"match": {
					"event":"wps-reg-success"
				}
			},
			{
				"event": "wifi.bsta",
				"match": {
					"event":"wps-success"
				}
			}
		],
		"then" : [
			{
				"object": "led.wps",
				"method":"set",
				"args" : {
					"state": "ok",
					"timeout": 30,					
				},
				"timeout": 1			
			}
		]
	},
	"wps_timeout": {
		"if_operator" : "OR",
		"if" : [
			{
				"event": "wifi.ap",
				"match": {
					"event":"wps-timeout"
				}
			},
			{
				"event": "wifi.bsta",
				"match": {
					"event":"wps-timeout"
				}
			}
		],
		"then" : [
			{
				"object": "led.wps",
				"method":"set",
				"args" : {
					"state": "off"				
				},
				"timeout": 2
			}
		]
	},
	"wps_fail": {
		"if_operator" : "OR",
		"if" : [
			{
				"event": "wifi.ap",
				"match": {
					"event":"wps-fail"
				}
			},
			{
				"event": "wifi.bsta",
				"match": {
					"event":"wps-fail"
				}
			}
		],
		"then" : [
			{
				"cli": "/sbin/test arg1 arg2",
				"timeout": 10
			}
		]
	}	
}
```

#### Pass the event data to the defined action if configured

- Event: { "wifi.sta": {"ifname":"wl0","event":"assoc","data":{"macaddr":"14:85:7f:17:fd:40"}} }
- Action: ubus call smtp.client send '{"email": "email@domain.com", "data": "&wifi.sta->data.macaddr"}'

```JSON
{
	"wifi_assoc": {
		"if" : [
			{
				"event": "wifi.sta",
				"match": {
					"event":"assoc"
				}
			}			
		],
		"then" : [
			{
				"object": "smtp.client",
				"method":"send",
				"args" : {
					"email": "email@domain.com",
					"data": "&wifi.sta->data.macaddr"
				},
				"timeout": 1
			}			
		]
	}
}
```

#### Pass environment variables and arguments to CLI command actions

- Event: { "ethport": {"ifname":"eth3","link":"down","speed":0,"duplex":"full"} }
- Cli-Action: LINK=down PORT=eth3 /sbin/hotplug-call ethernet

```JSON
{
	"ethernet_down": {
		"if" : [
			{
				"event": "ethport"
			}
		],
		"then" : [
			{
				"cli": "/sbin/hotplug-call ethernet",
				"envs": {
					"LINK": "&ethport->link",
					"PORT": "&ethport->ifname"
				},
				"timeout": 1
			}
		]
	}
}
```

This recipes will call hotplug-call for all ethport events.

## Building

```bash
mkdir build && cd build
cmake ..
make
```

## Tests

Rule engine tests are written in Cmocka. The scope of the tests is to test valid
and invalid uci configurations, and json recipes, stressing all the rulengd
functionality.

The scope of the uci tests assumes `rpcd` and `ubusd` are active in the system
and tests:
1. Incomplete configurations
2. Valid if-then-that ubus call

The json recipes also assumes `rpcd` and `ubusd` to be running on the system and
allow for more advanced configurable if-then-that logic. The json recipe tests
focus on valid and invalid ways to:

1. Setup one or multiple listeners
2. Triggering events
3. Invoking ubus methods
4. Multiple trigger conditions
5. Multiple then cases
6. Execution intervals
7. Multiple recipes
8. Regex matched events

In order to test that the if-then-that logic is performed correctly, the invokes
in the json recipe examples are primarily done through a dummy object, prepared
and compiled with the purpose of verifying rulengd.

The object, `template`, offer three methods:

```
root@4e9447c01f8a:/builds/iopsys/rulengd/build# ubus -v list
'template' @c138b721
	"increment":{}
	"reset":{}
	"status":{}
```

By incrementing a counter on successful if-then-that event, it is easy to track
whether an invoke was successful or not, by calling the status method. In the
setup phase of each test the counter is reset through the `reset` method.

For more information on how rulengd is tested, see the
[test specification](#./docs/testspec.md).

## Dependencies

To successfully build rulengd, the following libraries are needed:

| Dependency		| Link						| License		|
| ----------------- | ------------------------------------------------- | --------------------- |
| libuci	| https://git.openwrt.org/project/uci.git	 	| LGPL 2.1		|
| libubox	| https://git.openwrt.org/project/libubox.git	 	| BSD			|
| libubus	| https://git.openwrt.org/project/ubus.git	 	| LGPL 2.1		|
| libjson-c	| https://s3.amazonaws.com/json-c_releases	 	| MIT			|

Additionally, in order to build with the tests, the following libraries are needed:

| Dependency  				| Link                         	| License       |
| ------------------------- | ----------------------------------------- | ------------- |
| cmocka               	| https://cmocka.org/                          	| Apache	|
| libjson-editor	| https://dev.iopsys.eu/iopsys/json-editor   	| 	        |
