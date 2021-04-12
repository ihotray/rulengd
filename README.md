# Rule Engine

Rulengd is OpenWrt version of IFTTT (If This Then That) mechanism. It allows
configuring rules where options are ubus events with list of data and ubus
methods with list of arguments.

## Overview
Rulengd registers to ubus events on the system and keeps track of each
configured rule. When an event of interest is received, it compares it with the
configured rule and if the expected data of the event matches with the data of
the event received, it invokes the configured ubus method with the given
arguments.

With rulengd it is possible to add high level rules, for example presence rule
where event is client and data is the MAC address of a specific person and the
method to be executed is to sending an SMS to a specific email address with a
specific message which are the arguments. It is also possible to create lower
level rules such as when DSL training event is received blink Broadband LED.

With rulengd in the system, instead of creating applications to execute actions
upon receiving specific events, it suffices to create corresponding rule entries
to ruleng UCI configuration file, or json recipes.


### UCI Config
An example UCI rule listening for a `client` event.

```
config rule
    option event 'client'
    list event_data '{"action": "connect"}'
    list event_data '{"macaddr": "00:e0:4c:68:05:9a"}'
    option method 'smtp.client->send'
    list method_data '{"email": "email@domain.com"}'
    list method_data '{"data": "Alice is home"}'
```
On a received event, if it contains the key-value pairs
`{"action": "connect", "macaddr": "00:e0:4c:68:05:9a"}` the method `send` is
invoked through the `smtp.client` object, with the arguments
`{"email":"email@domain.com", data": "Alice is home"}`, sending an email to
email@domain.com with the message "Alice is home".

Note: Object and array arguments must be primitive types (we can't have object
in the array).

## JSON Recipe

For more granular event(s) and method(s) mapping, it is possible to create JSON
files as recipes. If JSON recipes are used, the path to the recipe that should
be read by rulengd needs to be specified in the rulengd UCI configuration file:

```
config rule
    option recipe "/etc/recipe_1.json"
```
The JSON recipes follow a similar logic as the UCI configurations, within multiple
root rules, five keys can be found: `if_operator`, `if_event_period`, `if`,
`then_exec_interval` and `then`.

The time-related keys are necessary if the if-this-then-that condition depends on
multiple events where the `if_operator` is set to *AND*. The `if_event_period` key
specifies the time interval in which these events should be observed for the `then`
calls to be executed. The operator can also be set to *OR*, which is the default
behavior if there are no given value, and it will trigger the `then` if only one of
the `if` condition is met. If there are multiple entries in the `then` condition,
`then_exec_interval` may be provided as a key, specifying the wait time between
the execution of the calls.

The `if` key expects an array of objects, denoting events to match, each entry
containing an `event`, and `match` key.

The `then` key also expects and array of objects, representing ubus methods to
invoke on received events matching that of the `if`. The objects should contain
the keys `object` and `method`, with an optional object, `args`, containing
key-value pairs to provide as argument. If you want to execute a shell command, the
`cli` object must replace `object` in this array.


In the following example recipe, listens for `wifi.sta` and `client` events,
matching some key-value pairs. If the events are recorded within ten seconds
after each other, it will invoke
`ubus call smtp.client send '{"email":"email@domain.com, "data":"Alice is home",}'`
and `ubus call wifi.ap.wlan0 dissassociate '{"macaddr": "00:e0:4c:68:05:9a"}'`,
one second after eachother.

Note: The order of the objects in the `then` array is important as the invokes
will be performed in the specified order.

```JSON
{
    "wifi_email": {
        "if_operator" : "AND",
        "event_period" : 10,
        "execution_interval" : 1,
        "if" : [
            {
                "event": "wifi.sta",
                "match":{
                    "macaddr":"00:e0:4c:68:05:9a",
                    "action":"associated"
                }
            },
            {   "event":"client",
                "match": {
                    "action":"connect",
                    "macaddr":"00:e0:4c:68:05:9a",
                    "ipaddr":"192.168.1.231",
                    "network":"lan"
                }
            }
        ],
        "then" : [
            {
                "object": "smtp.client",
                "method":"send",
                "args" : {
                    "email":"email@domain.com",
                    "data": "Alice is home"
                }
            },
            {
                "object":"wifi.ap.wlan0",
                "method":"disassociate",
                "args": {
                    "macaddr":"00:e0:4c:68:05:9a"
                }
            }
        ]
    }
}
```


## Building

```
mkdir build && cd build
cmake ..
make
```
### Example

After building, do ```sudo make install```. This will copy
```./test/ruleng-test-rules``` to ```/etc/config```. Start ```rulengd```, and
test it with

```
ubus send "test.event" "{'radio':0, 'reason':1, 'channels': [1,2,3], 'non-specified-key': 'non-specified-value'}"
```

This should write 'test event received!' to the ```/tmp/test_event.txt```.

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

| Dependency		| Link																| License		|
| ----------------- | ----------------------------------------------------------------- | ------------- |
| libuci			| https://git.openwrt.org/project/uci.git						 	| LGPL 2.1		|
| libubox			| https://git.openwrt.org/project/libubox.git					 	| BSD			|
| libubus			| https://git.openwrt.org/project/ubus.git						 	| LGPL 2.1		|
| libjson-c			| https://s3.amazonaws.com/json-c_releases						 	| MIT			|

Additionally, in order to build with the tests, the following libraries are needed:

| Dependency  				| Link                                       				| License       |
| ------------------------- | --------------------------------------------------------- | ------------- |
| cmocka                 	| https://cmocka.org/                                    	| Apache		|
| libjson-editor			| https://dev.iopsys.eu/iopsys/json-editor   				| 	            |
