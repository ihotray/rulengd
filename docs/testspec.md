# Test Specification

Rulengd primarily is tested through functional tests, seeing as it is closely
dependent on ubus, to invoke other objects on received ubus events.

There are no functional API tests at all, seeing as rulengd does not publish any
ubus objects and methods.

A few unit tests are written which test basic functionality such as reading
configurations.


## Prerequisites

Apart from the required library dependencies specified in the [README](../README.md#dependencies),
the `ubusd`, `template_obj` and `rpcd` should be running in the test
environment for the functional tests to be ran, additionally, test configuration
files should be installed in the environment. These things are all prepared
by the gitlab-ci pipeline test scripts.

## Test Suites

The rulengd build pipe has two primary test suite, a unit test suite and
functional test suite.

### Unit Tests

The rulengd unit tests are written in cmocka, and consists of two individual
suites, one validating uci configuration and one validating json recipes,
to verify that no memory leaks or segmentation faults occur in either scenario.

#### UCI Suite

| Execution ID	| Test Case Name												                    |
| :---			| :---															                    |
| 1				| [test_rulengd_non_existing_ubus_socket](#test_rulengd_non_existing_ubus_socket)   |

##### test_rulengd_non_existing_ubus_socket

###### Description

Tests initializing a rulengd context through `ruleng_bus_init(4)` with an
invalid socket path.

###### Test Steps

Allocate a `struct ruleng_rules_ctx` context, and create a
`struct ruleng_bus_ctx` pointer to pass as arguments, along with a valid uci
configuration file, and an invalid ubus socket path.

###### Test Expected Results

The expected result is to receive return code 2, `RULENG_BUS_ERR_CONNECT`.

#### JSON Recipe Suite

| Execution ID	| Test Case Name												|
| :---			| :---															|
| 1				| [test_rulengd_invalid_recipes](#test_rulengd_invalid_recipes)	|
| 2				| [test_rulengd_valid_recipe](#test_rulengd_valid_recipe) 	    |

##### test_rulengd_invalid_recipes

###### Description

Tests various invalid json recipe variations to verify that no invalid
configuration is fully handled, or leaks memory on failure.

###### Test Steps

Invalid recipes with variations such as incorrect key types, missing keys and
excess keys are passed as input to `ruleng_process_json(3)`.

###### Test Expected Results

The expected result is for no rule to be added to the rule list, `json_rules`,
and no memory leaks to be observed through valgrind.

##### test_rulengd_valid_recipe

###### Description

Tests that some valid json recipe variations to verify that they are fully
parsed, and no memory leaks upon using the methods to free the rules.

###### Test Steps

Provide one recipe with empty if and then keys, and one with some dummy data.

###### Test Expected Results

Two recipes are parsed and added to the `json_rules` list, no memory leaks are
observed.


### Functional Tests

Similarily to the unit tests, the functional tests are written in Cmocka, and
consists of two individual test suites, one using uci configuration files, and
one using json recipes. The functional tests are aimed at testing the desired
functionality of rulengd, meaning verifying that after events are observed,
certain action is taken.

#### UCI Configuration

| Serial ID		| Test Case Name	                                                          |
| :---			| :---				                                                          |
| 1				| [test_rulengd_test_event_uci](#test_rulengd_test_event_uci)	              |
| 2				| [test_rulengd_test_event_uci_fail](#test_rulengd_test_event_uci_fail)	      |

##### test_rulengd_test_event_uci

###### Description

Register an if-this-then-that event trigger through a UCI configuration and verify
that the invoke is triggered on recorded event.

###### Test Steps

Setup a UCI configuration that listens to `wifi.radio.channel_changed` events,
when triggered creating a file `/tmp/test_file.txt`. Prepare a `struct blob_buf`
with the expected arguments and simulate the event through `ruleng_event_cb(4)`.

###### Test Expected Results

After triggering the event the file `/tmp/test_file.txt` is created through
`file->write`.

##### test_rulengd_test_event_uci_fail

###### Description

Register an if-this-then-that event trigger through a UCI configuration and attempt
to trigger it through an invalid event.

###### Test Steps

Setup a UCI configuration that listens to `wifi.radio.channel_changed` events,
when triggered creating a file `/tmp/test_file.txt`. Prepare a `struct blob_buf`
with invalid arguments and simulate the event through `ruleng_event_cb(4)`.

###### Test Expected Results

After triggering the event the file `/tmp/test_file.txt` should not be created.

#### JSON Recipe

| Serial ID		| Test Case Name																					|
| :---			| :---																								|
| 1				| [test_rulengd_register_listener](#test_rulengd_register_listener)									|
| 2				| [test_rulengd_trigger_event_fail](#test_rulengd_trigger_event_fail)								|
| 3				| [test_rulengd_trigger_event](#test_rulengd_trigger_event)											|
| 4				| [test_rulengd_trigger_invoke_fail](#test_rulengd_trigger_invoke_fail)								|
| 5				| [test_rulengd_trigger_invoke](#test_rulengd_trigger_invoke)										|
| 6				| [test_rulengd_trigger_invoke_cli](#test_rulengd_trigger_invoke_cli)								|
| 7				| [test_rulengd_trigger_invoke_multi_condition](#test_rulengd_trigger_invoke_multi_condition)		|
| 8				| [test_rulengd_trigger_invoke_multi_condition_or](#test_rulengd_trigger_invoke_multi_condition_or)	|
| 9				| [test_rulengd_trigger_invoke_multi_then](#test_rulengd_trigger_invoke_multi_then)					|
| 10			| [test_rulengd_execution_interval](#test_rulengd_execution_interval)								|
| 11			| [test_rulengd_multi_rule](#test_rulengd_multi_rule)												|
| 12			| [test_rulengd_multi_recipe](#test_rulengd_multi_recipe)											|
| 13			| [test_rulengd_regex](#test_rulengd_regex)															|


##### test_rulengd_register_listener

###### Description

Attempts to register ubus listeners, with both valid and invalid configuration
recipes.

###### Test Steps

Setup listeners through `ruleng_bus_register_events(3)` with various valid,
and invalid recipes, validate return value against expected numbers of listeners
to be setup.

###### Test Expected Results

Listeners to be setup if and only if there is an `if` key with valid parameters.

##### test_rulengd_trigger_event_fail

###### Description

Register ubus listeners, with and without a `match` key, however, no match key
should not register a hit, neither should a match key with wrong type.

###### Test Steps

Setup listeners through `ruleng_bus_register_events(3)` with different recipe
variations and simulate events, without any matches.

###### Test Expected Results

The `struct ruleng_json_rule` should never record a hit, meaning, its `hit`
variable should never be increased.

##### test_rulengd_trigger_event

###### Description

Register ubus listeners, with a `match` key, and trigger an event with the
corresponding key-value pair.

###### Test Steps

Setup a listener through `ruleng_bus_register_events(3)`, and simulate an event
with the appropriate type and key-value pair to trigger a hit.

###### Test Expected Results

The `struct ruleng_json_rule` should record a hit, meaning, its `hit`
variable should be incremented.

##### test_rulengd_trigger_invoke_fail

###### Description

Test different variations of the `then` key, none which has a valid match on
ubus, meaning they should record hits, but not cause segfaults or leaks.

###### Test Steps

Setup a listener through `ruleng_bus_register_events(3)`, and simulate an event
with the appropriate type and key-value pair to trigger a hit.

###### Test Expected Results

The `struct ruleng_json_rule` should record a hit, meaning, its `hit`
variable should be incremented. No segfaults or leaks caused by the missing
keys.

##### test_rulengd_trigger_invoke

###### Description

Test different variations of the `then` key, which has a valid match on
ubus, meaning they should record hits, and increment the template.

###### Test Steps

Setup a listener through `ruleng_bus_register_events(3)`, and simulate an event
with the appropriate type and key-value pair to trigger a hit.

###### Test Expected Results

The `struct ruleng_json_rule` should record a hit, meaning, its `hit`
variable and the template should be incremented.

##### test_rulengd_trigger_invoke_cli

###### Description

Test different variations of the `then` key, which has a valid match on
ubus, meaning they should record hits, and execute the given command, which
increases the template.

###### Test Steps

Setup a listener through `ruleng_bus_register_events(3)`, and simulate an event
with the appropriate type and key-value pair to trigger a hit, causing a
shell command to be executed twice.

###### Test Expected Results

The `struct ruleng_json_rule` should record a hit, meaning, its `hit`
variable should be incremented, the shell command executed and the template
should be incremented.

##### test_rulengd_trigger_invoke_multi_condition

###### Description

Test recipes with multiple conditions and an `event_period` set in order to
trigger an if-this-and-this-then-that case.

###### Test Steps

Setup a JSON recipe containing multiple `if` conditions with an "AND"
`if_operator`and one `then` condition, validating that it is invoked after
simulating the two events through `ruleng_event_json_cb`, then repeat the
process with a `event_period`.

###### Test Expected Results

If two events are recorded that match the `if` case within the specified
`event_period`, the `template` objects `increment` method is invoked and
recorded as increased by validating it through its `status` method.

##### test_rulengd_trigger_invoke_multi_condition_or

###### Description

Test recipes with multiple conditions and an `event_period` set in order to
trigger an if-this-or-this-then-that case.

###### Test Steps

Setup a JSON recipe containing multiple `if` conditions, and one `then`
condition, validating that it is invoked after simulating only one events
through `ruleng_event_json_cb`, then repeat the process with an "OR"
`if_operator`.

###### Test Expected Results

If two events are recorded that match the `if` case within the specified
`event_period`, the `template` objects `increment` method is invoked and
recorded as increased by validating it through its `status` method.

##### test_rulengd_trigger_invoke_multi_then

###### Description

Test recipes with multiple invoke conditions on successful if-this-then-that trigger.

###### Test Steps

Setup recipes with multiple entries from different objects and methods to the
`then` array (in this case, invoking `template->increment` multiple times,
and creating a file through `file->write`), causing multiple ubus invokes on
recorded events.

###### Test Expected Results

The `template` counter to be updated multiple times per event triggered, the
file `/tmp/test_file.txt` to be created.

##### test_rulengd_execution_interval

###### Description

Test a recipe with `execution_interval` set and multiple invokes, validating
that the second invoke is not triggered till after the `execution_interval`
timer is hit.

###### Test Steps

Prepare a recipe with an `execution_interval` of five and depend on two events.
Record the time prior to simulating the first event, then simulate the second,
and record the time.

###### Test Expected Results

The second invoke should trigger the `then` chain, and take more than five
seconds.

##### test_rulengd_multi_rule

###### Description

Test a JSON configuration involving two rules in a single JSON recipe.

###### Test Steps

Prepare two rules within files, with different `if` conditions.

###### Test Expected Results

Trigger the `if` conditions from all the JSON rules and validate that the
conditions triggered their expected ubus invokes.

##### test_rulengd_multi_recipe

###### Description

Test a UCI configuration linking to two different JSON recipes.

###### Test Steps

Prepare two recipe files, with different `if` conditions.

###### Test Expected Results

Trigger the `if` conditions from all the JSON recipes and validate that the
conditions triggered their expected ubus invokes.

##### test_rulengd_regex

###### Description

Test a recipe that allows regex based key-value pair matching.

###### Test Steps

Setup a recipe that allows matching based on any regex, i.e. `^test\\..*`,
simulate an event matching the regex and one that does not.

###### Test Expected Results

The payload matching the regex should trigger an invoke to the
`template->increment` method, while the one that does not match should not.

## Writing New Tests

When writing new rulengd tests, there are some factors to take into
consideration, firstly, whether it is a unit test (no external dependencies),
or functional test (depends on other processes, such as the `template` object)
to determine which test suite it belongs under. In the case of a unit test it
should go under one of the `unit_tests_*.c` files, and in the case of functional
tests it belongs in one of the `functional_tests_*.c` files. Note that most of
the rulengd tests belong in the functional test category. In the case of a new
`.c` test file being created, it should be added under the appropriate suite
in the `./test/cmocka/CMakeLists.txt` file.

### JSON Recipe Tests

When writing new rulengd test which exercises some JSON recipe functionality,
it is recommended to first prepare the desired JSON recipe through
[libjson-editor](#https://dev.iopsys.eu/iopsys/json-editor) and write it to a
file which is linked by the `recipe` option in the UCI configuration. With the
recipe prepared the recipe should be parsed through
`ruleng_bus_register_events(3)`. Afterwards, the context can be iterated,
through `LN_LIST_FOREACH(3)`, to find the `struct ruleng_json_rule` of the newly
created rule and observe information such as `hits` after a triggered event.

To simulate events, prepare a `struct blob_buf` and simulate an event through
`ruleng_event_json_cb(4)`, alternatively, an actual ubus event can be generated
assuming it is a functional test.

### UCI Configuration Recipe

The currently implemented UCI configuration tests are few and done primitively,
by statically preparing the configuration recipes and reading it when setting up
the context. If many UCI configuration tests were to be created, it might be a
good idea to implement some functionality to read, write and create new recipes
through `libuci`.