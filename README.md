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

mkdir build && cd build
cmake ..
make

### Example

After building, do ```sudo make install```'. This will copy
```./test/ruleng-test-rules``` to ```/etc/config```. Start ```rulengd```, and test it with

```
ubus send "test.event" "{'radio':0, 'reason':1, 'channels': [1,2,3], 'non-specified-key': 'non-specified-value'}"
```

This should write 'test event received!' to the ```/tmp/test_event.txt```.

## Unit Tests

Rule engine has unit tests prepared through Cmocka, testing a variety of configuration and json recipe setups.

It is recommended these are run through the prepared Docker image, containing ubus, rpcd and various other dependencies needed to compile and run the tests.

First build the image.

```
docker build -t iopsys/rulengd .
```

Then run the image to start it in the background. It is important this is done from the root directory of the repository to provide the correct /opt/work directory to the image.
```
docker run -d --name rulengd --privileged --rm -v ${PWD}:/opt/work -p 2222:22 -e LOCAL_USER_ID=`id -u $USER` iopsys/rulengd:latest
```

To gain terminal access to the image run:

```
docker exec --user=user -it rulengd bash

```

When you are finished using the docker image, stop the image through:

```
docker stop rulengd
```


To run tests the build has to be prepared with the ```Debug``` build type.

```
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make
sudo make install
```

With a debug build prepared, before the cmocka test suite can be run, start the template daemon used from the tests.

```
sudo template &
```

To run the test suite run ```sudo make test```.

```
user@a3beae748566:/opt/work/build$ sudo make test
Running tests...
Test project /opt/work/build
    Start 1: ruleng_test_json
1/4 Test #1: ruleng_test_json .................   Passed   22.06 sec
    Start 2: ruleng_test_uci
2/4 Test #2: ruleng_test_uci ..................   Passed    2.01 sec
    Start 3: ruleng_test_json_valgrind
3/4 Test #3: ruleng_test_json_valgrind ........   Passed   22.91 sec
    Start 4: ruleng_test_uci_valgrind
4/4 Test #4: ruleng_test_uci_valgrind .........   Passed    2.59 sec

100% tests passed, 0 tests failed out of 4

Total Test time (real) =  49.59 sec
```