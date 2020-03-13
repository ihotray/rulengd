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

The rulengd build pipe has two test suite, a unit
testi suite and functional test suite.

### Unit Tests

The rulengd unit tests are written in cmocka, and consists of two individual
suites, one validating uci configuration and one validating json recipes,
to verify that no memory leaks or segmentation faults occurr in either scenario.

| Execution ID	| Method     			| Test Case Name												                    | Function ID Coverage		|
| :---			| :--- 					| :---															                    | :---						|
| 1				| disconnect    		| [test_rulengd_non_existing_ubus_socket](#test_rulengd_non_existing_ubus_socket)   |                     		|
| 2				| request_transition	| [test_rulengd_invalid_recipes](#test_rulengd_invalid_recipes)	                    |                           |
| 3				| request_neighbor		| [test_rulengd_valid_recipe](#test_rulengd_valid_recipe) 	                        |                           |

#### test_rulengd_non_existing_ubus_socket

##### Description

Tests initializing a rulengd context through `ruleng_bus_init(4)` with an
invalid socket path.

##### Test Steps

Allocate a `struct ruleng_rules_ctx` context, and create a
`struct ruleng_bus_ctx` pointer to pass as arguments, along with a valid uci
configuration file, and an invalid ubus socket path.

##### Test Expected Results

The expected result is to receive return code 2, `RULENG_BUS_ERR_CONNECT`.

#### test_rulengd_invalid_recipes

##### Description

Tests various invalid json recipe variations to verify that no invalid
configuration is fully handled, or leaks memory on failure.

##### Test Steps

Invalid recipes with variations such as incorrect key types, missing keys and
excess keys are passed as input to `ruleng_process_json(3)`.

##### Test Expected Results

The expected result is for no rule to be added to the rule list, `json_rules`,
and no memory leaks to be observed through valgrind.

#### test_rulengd_valid_recipe

##### Description

Tests that some valid json recipe variations to verify that they are fully
parsed, and no memory leaks upon using the methods to free the rules.

##### Test Steps

Provide one recipe with empty if and then keys, and one with some dummy data.

##### Test Expected Results

Two recipes are parsed and added to the `json_rules` list, no memory leaks are
observed.


### Functional Tests

Similarily to the unit tests, the functional tests are written in Cmocka, and
consists of two individual test suites, one using uci configuration files, and
one using json recipes. The functional tests are aimed at testing the desired
functionality of rulengd, meaning verifying that after events are observed,
certain action is taken.

| Serial ID		| Test Case Name	                                                                            |
| :---			| :---				                                                                            |
| 1				| [test_rulengd_register_listener](#test_rulengd_register_listener)	                            |
| 2				| [test_rulengd_trigger_event_fail](#test_rulengd_trigger_event_fail)	                        |
| 3				| [test_rulengd_trigger_event](#test_rulengd_trigger_event)	                                    |
| 4				| [test_rulengd_trigger_invoke_fail](#test_rulengd_trigger_invoke_fail)	                        |
| 5				| [test_rulengd_trigger_invoke](#test_rulengd_trigger_invoke)	                                |
| 6				| [test_rulengd_trigger_invoke_multi_condition](#test_rulengd_trigger_invoke_multi_condition)	|
| 7				| [test_rulengd_trigger_invoke_multi_then](#test_rulengd_trigger_invoke_multi_then)	            |
| 8				| [test_rulengd_execution_interval](#test_rulengd_execution_interval)             	            |
| 9				| [test_rulengd_multi_recipe](#test_rulengd_multi_recipe)	                                    |
| 10			| [test_rulengd_regex](#test_rulengd_regex)	                                                    |


#### test_rulengd_register_listener

##### Description

Attempts to register ubus listeners, with both valid and invalid configuration
recipes.

##### Test Steps

Setup listeners through `ruleng_bus_register_events(3)` with various valid,
and invalid recipes, validate return value against expected numbers of listeners
to be setup.

##### Test Expected Results

Listeners to be setup if and only if there is an `if` key with valid parameters.

#### test_rulengd_trigger_event_fail

##### Description

Register ubus listeners, with and without a `match` key, however, no match key
should not register a hit, neither should a match key with wrong type.

##### Test Steps

Setup listeners through `ruleng_bus_register_events(3)` with different recipe
variations and simulate events, without any matches

##### Test Expected Results

The `struct ruleng_json_rule` should never record a hit, meaning, its `hit`
variable should never be increased.

#### test_rulengd_trigger_event

##### Description

Register ubus listeners, with a `match` key, and trigger an event with the
corresponding key-value pair.

##### Test Steps

Setup a listener through `ruleng_bus_register_events(3)`, and simulate an event
with the appropriate type and key-value pair to trigger a hit.

##### Test Expected Results

The `struct ruleng_json_rule` should record a hit, meaning, its `hit`
variable should be incremented.

#### test_rulengd_trigger_invoke_fail

##### Description

Test different variations of the `then` key, none which has a valid match on
ubus, meaning they should record hits, but not cause segfaults or leaks.

##### Test Steps

Setup a listener through `ruleng_bus_register_events(3)`, and simulate an event
with the appropriate type and key-value pair to trigger a hit.

##### Test Expected Results

The `struct ruleng_json_rule` should record a hit, meaning, its `hit`
variable should be incremented. No segfaults or leaks caused by the missing
keys.







## Writing New Tests

### Writing Tests
