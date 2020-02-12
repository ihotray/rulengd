# Rule Engine

## Overview

rulengd is OpenWrt version of IFTTT (If This Then That) mechanism. It allows configuring rules
where options are ubus events with list of data and ubus methods with list of arguments.
rulengd registers to all ubus events on the system and keeps track of each configured rule.
When an event of interest is received, it compares it with the configured rule and if the
expected data of the event matches with the data of the event received, it invokes the
configured ubus method with the given arguments.

With rulengd it is possible to add high level rules, for example presence rule where event is
client and data is the MAC address of a specific person and the method to be executed is to
sending an SMS to a specific email address with a specific message which are the arguments.
It is also possible to create lower level rules such as when DSL training event is received blink
Broadband LED.

With rulengd in the system, instead of creating applications to execute actions upon
receiving specific events, it suffices to create corresponding rule entries to ruleng UCI
configuration file.

```
config rule
    option event 'client'
    list event_data "{'action': 'connect'}"
    list event_data "{'macaddr': '00:e0:4c:68:05:9a'}"
    option method 'smtp.client->send'
    list method_data "{'email': 'email@domain.com'}"
    list method_data "{'data': 'Alice is home'}"
```

The rule above means we are listening for ```client``` event. If that
event occurs, and contains arguments ```{'action': 'connect'}``` and ```{'macaddr': '00:e0:4c:68:05:9a'}```,
on ubus: ```{ "client": {"action":"connect","macaddr":"00:e0:4c:68:05:9a"} }```
we execute method ```send``` on object ```smtp.client``` with arguments
```{'email':'email@domain.com'}``` and ```{'data': 'Alice is home'}```.

```
config rule
    option event 'dsl'
    list event_data "{'line': 0}"
    list event_data "{'link': 'training'}"
    option method 'led.broadband->set'
    list method_data "{'state': 'notice'}"
```

This means we are listening for ```dsl``` event. If that
event occurs, and contains arguments ```{'line': 0}``` and ```{'link': 'training'}```,
on ubus: ```{ "dsl": {"line":0, "link":"training"} }```
we execute method ```set``` on object 'led.broadband' with argument ```{'state':'notice'}```.

Note: Object and array arguments must be primitive types (we can't have object in the array).

For more granular event(s) and method(s) mapping, it is possible to create JSON files as recipes
such as in the example below:
```
config rule
    option recipe "/etc/recipe_1.json"

# cat /etc/recipe_1.json
{
    "time" : {
        "event_period" : 10,
        "execution_interval" : 1
    },
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
```
It means if you receive ```wifi.sta``` and ```client``` events with the matching arguments within 10 seconds after each other, execute the methods in then array with 1 second intervals

## Building

```
mkdir build && cd build
cmake ..
make
```
### Example

After building, do ```sudo make install```'. This will copy
```./test/ruleng-test-rules``` to ```/etc/config```. Start ```rulengd```, and test it with

```
ubus send "test.event" "{'radio':0, 'reason':1, 'channels': [1,2,3], 'non-specified-key': 'non-specified-value'}"
```

This should write 'test event received!' to the ```/tmp/test_event.txt```.

## Unit Tests

Rule engine has unit tests prepared through Cmocka, testing a variety of configuration and json recipe setups.

It is recommended these are run through the IOPSYS docker hub image, containing
ubus, rpcd and various other dependencies needed to compile and run the tests.

To start the container run the command:

```
docker run -it --rm -v ${PWD}/schemas:/usr/share/rpcd/schemas -v ${PWD}:/builds/iopsys/rulengd iopsys/code-analysis:0.7
```

To run tests the build has to be prepared with the ```Debug``` build type.

```
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make
sudo make install
```

The tests support two targets, `make unit-test` and `make functional-test`.
The unit tests can be run in a sterile environment, while the functional tests
have some dependencies to other daemons such as `ubusd` and `template`. To start
these dependencies run the following commands from the build directory:

```
$ ubusd &
[1] 173
$ ./test/cmocka/template_obj/template &
[2] 174
$ connected as c1839b44
$ rpcd &
[3] 463
```

With a debug build prepared, before the cmocka test suite can be run, start the template daemon used from the tests.

```
$ sudo make functional-test
root@4547a81ee0ec:/builds/iopsys/rulengd/build# make functional-test
==477== Memcheck, a memory error detector
==477== Copyright (C) 2002-2017, and GNU GPL'd, by Julian Seward et al.
==477== Using Valgrind-3.13.0 and LibVEX; rerun with -h for copyright info
==477== Command: ./functional_tests_json
==477== 
[==========] Running 10 test(s).
[ RUN      ] test_rulengd_register_listener
ERROR: ruleng_rules_rules_parse_event_name(): "rule: failed to find event name"
...
INFO: ruleng_bus_register_events(): "Regiser ubus event[test.event2]"
[       OK ] test_rulengd_register_listener
[ RUN      ] test_rulengd_trigger_event_fail
ERROR: ruleng_rules_rules_parse_event_name(): "rule: failed to find event name"
...
INFO: ruleng_event_json_cb(): "Process event [test.event+]"
[       OK ] test_rulengd_trigger_event_fail
[ RUN      ] test_rulengd_trigger_event
ERROR: ruleng_rules_rules_parse_event_name(): "rule: failed to find event name"
...
INFO: ruleng_event_json_cb(): "All rules matched within time[test.event+]"
[       OK ] test_rulengd_trigger_event
[ RUN      ] test_rulengd_trigger_invoke_fail
ERROR: ruleng_rules_rules_parse_event_name(): "rule: failed to find event name"
...
[       OK ] test_rulengd_trigger_invoke_fail
[ RUN      ] test_rulengd_trigger_invoke
ERROR: ruleng_rules_rules_parse_event_name(): "rule: failed to find event name"
...
INFO: ruleng_take_json_action(): "calling[file->write]"
[       OK ] test_rulengd_trigger_invoke
[ RUN      ] test_rulengd_trigger_invoke_multi_condition
INFO: ruleng_ubus_complete_cb(): "ubus call completed, ret = 0"
...
INFO: ruleng_event_json_cb(): "Process event [test.event+test.event.two+test.event.three+]"
[       OK ] test_rulengd_trigger_invoke_multi_condition
[ RUN      ] test_rulengd_trigger_invoke_multi_then
ERROR: ruleng_rules_rules_parse_event_name(): "rule: failed to find event name"
...
INFO: ruleng_ubus_complete_cb(): "ubus call completed, ret = 0"
[       OK ] test_rulengd_trigger_invoke_multi_then
[ RUN      ] test_rulengd_execution_interval
INFO: ruleng_ubus_complete_cb(): "ubus call completed, ret = 0"
...
INFO: ruleng_ubus_complete_cb(): "ubus call completed, ret = 0"
[       OK ] test_rulengd_execution_interval
[ RUN      ] test_rulengd_multi_recipe
INFO: ruleng_ubus_complete_cb(): "ubus call completed, ret = 0"
...
INFO: ruleng_ubus_complete_cb(): "ubus call completed, ret = 0"
[       OK ] test_rulengd_multi_recipe
[ RUN      ] test_rulengd_regex
ERROR: ruleng_rules_rules_parse_event_name(): "rule: failed to find event name"
...
INFO: ruleng_event_json_cb(): "Process event [test.event+]"
[       OK ] test_rulengd_regex
[==========] 10 test(s) run.
[  PASSED  ] 10 test(s).
==477== 
==477== HEAP SUMMARY:
==477==     in use at exit: 0 bytes in 0 blocks
==477==   total heap usage: 3,356 allocs, 3,356 frees, 657,471 bytes allocated
==477== 
==477== All heap blocks were freed -- no leaks are possible
==477== 
==477== For counts of detected and suppressed errors, rerun with: -v
==477== ERROR SUMMARY: 0 errors from 0 contexts (suppressed: 0 from 0)
==479== Memcheck, a memory error detector
==479== Copyright (C) 2002-2017, and GNU GPL'd, by Julian Seward et al.
==479== Using Valgrind-3.13.0 and LibVEX; rerun with -h for copyright info
==479== Command: ./functional_tests_uci
==479== 
[==========] Running 2 test(s).
INFO: ruleng_rules_rules_parse_event(): "wifi.radio.channel_changed event data: { "radio": 0, "reason": 1, "channels": [ 1, 2, 3 ] }"
...
INFO: ruleng_event_cb(): "{ \"wifi.radio.channel_changed\": {"radio":1,"reason":1,"channels":[1,2,3]} }\n"
[       OK ] test_rulengd_test_event_uci_fail
[==========] 2 test(s) run.
[  PASSED  ] 2 test(s).
==479== 
==479== HEAP SUMMARY:
==479==     in use at exit: 0 bytes in 0 blocks
==479==   total heap usage: 384 allocs, 384 frees, 122,438 bytes allocated
==479== 
==479== All heap blocks were freed -- no leaks are possible
==479== 
==479== For counts of detected and suppressed errors, rerun with: -v
==479== ERROR SUMMARY: 0 errors from 0 contexts (suppressed: 0 from 0)
==481== Memcheck, a memory error detector
==481== Copyright (C) 2002-2017, and GNU GPL'd, by Julian Seward et al.
==481== Using Valgrind-3.13.0 and LibVEX; rerun with -h for copyright info
==481== Command: ./functional_tests_generic
==481== 
[==========] Running 1 test(s).
[ RUN      ] test_rulengd_test_init
INFO: ruleng_rules_rules_parse_event(): "wifi.radio.channel_changed event data: { "radio": 0, "reason": 1, "channels": [ 1, 2, 3 ] }"
...
ERROR: ruleng_rules_get(): "asdasd: uci lookup failed"
[       OK ] test_rulengd_test_init
[==========] 1 test(s) run.
[  PASSED  ] 1 test(s).
==481== 
==481== HEAP SUMMARY:
==481==     in use at exit: 0 bytes in 0 blocks
==481==   total heap usage: 460 allocs, 460 frees, 194,839 bytes allocated
==481== 
==481== All heap blocks were freed -- no leaks are possible
==481== 
==481== For counts of detected and suppressed errors, rerun with: -v
==481== ERROR SUMMARY: 0 errors from 0 contexts (suppressed: 0 from 0)
Built target functional-test
```

Alternatively, the scripts prepared for the IOPSYS CI/CD pipeline can be used,
from the root directory:

```
$ ./gitlab-ci/setup.sh 
preparation script
/builds/iopsys/rulengd
mkdir: cannot create directory 'build': File exists
/builds/iopsys/rulengd/build /builds/iopsys/rulengd
-- Found libuci: /usr/lib/libuci.so
-- Found libubus: /usr/lib/libubus.so
-- Found libjson-c: /usr/local/lib/libjson-c.so
-- Appending code coverage compiler flags: -g -O0 --coverage -fprofile-arcs -ftest-coverage
-- Building tests
-- Configuring done
-- Generating done
-- Build files have been written to: /builds/iopsys/rulengd/build
[ 25%] Built target rulengd
[ 50%] Built target rulengd-api
[ 58%] Built target functional_tests_generic
[ 66%] Built target functional_tests_uci
[ 75%] Built target functional_tests_json
[ 83%] Built target unit_tests_uci
[ 91%] Built target unit_tests_json
[100%] Built target template
[ 25%] Built target rulengd
[ 50%] Built target rulengd-api
[ 58%] Built target functional_tests_generic
[ 66%] Built target functional_tests_uci
[ 75%] Built target functional_tests_json
[ 83%] Built target unit_tests_uci
[ 91%] Built target unit_tests_json
[100%] Built target template
Install the project...
-- Install configuration: "Debug"
-- Installing: /etc/config/ruleng-test-uci
-- Installing: /etc/config/ruleng-test-recipe
-- Installing: /etc/recipe_1.json
-- Installing: /usr/local/bin/template
-- Set runtime path of "/usr/local/bin/template" to ""
/builds/iopsys/rulengd
$ ./gitlab-ci/functional-test.sh 
preparation script
/builds/iopsys/rulengd
rpcd: added process group
template: added process group
ubusd: added process group
rpcd                             RUNNING   pid 298, uptime 0:00:03
template                         RUNNING   pid 299, uptime 0:00:03
ubusd                            RUNNING   pid 296, uptime 0:00:04
make: Entering directory '/builds/iopsys/rulengd/build'
make[1]: Entering directory '/builds/iopsys/rulengd/build'
make[2]: Entering directory '/builds/iopsys/rulengd/build'
make[3]: Entering directory '/builds/iopsys/rulengd/build'
make[3]: Leaving directory '/builds/iopsys/rulengd/build'
make[3]: Entering directory '/builds/iopsys/rulengd/build'
==312== Memcheck, a memory error detector
==312== Copyright (C) 2002-2017, and GNU GPL'd, by Julian Seward et al.
==312== Using Valgrind-3.13.0 and LibVEX; rerun with -h for copyright info
==312== Command: ./functional_tests_json
==312== 
[==========] Running 10 test(s).
[ RUN      ] test_rulengd_register_listener
ERROR: ruleng_rules_rules_parse_event_name(): "rule: failed to find event name"
ERROR: ruleng_rules_rules_parse_event_name(): "rule: failed to find event name"
ERROR: ruleng_process_json(): "Invalid JSON recipe at 'if' key!\n"
ERROR: ruleng_rules_rules_parse_event_name(): "rule: failed to find event name"
ERROR: ruleng_rules_rules_parse_event_name(): "rule: failed to find event name"
...
```