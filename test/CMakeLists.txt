CMAKE_MINIMUM_REQUIRED(VERSION 3.0)
INCLUDE_DIRECTORIES ("${PROJECT_SOURCE_DIR}/src")
FILE(COPY data DESTINATION .)

find_library(JSON_EDITOR_LIBRARIES NAMES json-editor)

SET(unit_tests unit_tests_json unit_tests_uci)
SET(functional_tests functional_tests_json functional_tests_uci functional_tests_generic)
SET(all_tests ${unit_tests} ${functional_tests})
FOREACH(test_name IN LISTS all_tests)
    ADD_EXECUTABLE(${test_name} ${test_name}.c)
	TARGET_LINK_LIBRARIES(
		${test_name}
		${CMOCKA_LIBRARIES}
		rulengd-api
		${LIBUBOX_LIBRARIES}
		${LIBUCI_LIBRARIES}
		${LIBUBUS_LIBRARIES}
		${JSON-C_LIBRARIES}
		${JSON_EDITOR_LIBRARIES}
		pthread
	)
	#ADD_TEST(NAME ${test_name} COMMAND $<TARGET_FILE:${test_name}>)
ENDFOREACH(test_name)

ADD_CUSTOM_TARGET(functional-test)
FOREACH(test_name IN LISTS functional_tests)
	ADD_CUSTOM_COMMAND(TARGET functional-test
		COMMAND valgrind --leak-check=full --show-reachable=no --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 --track-origins=yes ./${test_name}
	)
	ADD_TEST(NAME ${test_name} COMMAND $<TARGET_FILE:${test_name}> LABEL functional)
	SET_TESTS_PROPERTIES(${test_name} PROPERTIES LABELS "functional")
ENDFOREACH(test_name)

ADD_CUSTOM_TARGET(unit-test)
FOREACH(test_name IN LISTS unit_tests)
	ADD_CUSTOM_COMMAND(TARGET unit-test
		COMMAND valgrind --leak-check=full --show-reachable=no --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 --track-origins=yes ./${test_name}
	)
	ADD_TEST(NAME ${test_name} COMMAND $<TARGET_FILE:${test_name}> LABEL unit)
	SET_TESTS_PROPERTIES(${test_name} PROPERTIES LABELS "unit")
ENDFOREACH(test_name)

SETUP_TARGET_FOR_COVERAGE(
	NAME functional-coverage
	EXECUTABLE ctest -L functional
	DEPENDENCIES rulengd-api
)

SETUP_TARGET_FOR_COVERAGE(
	NAME unit-coverage
	EXECUTABLE ctest -L unit
	DEPENDENCIES rulengd-api
)

SETUP_TARGET_FOR_COVERAGE(
	NAME all-coverage
	EXECUTABLE ctest
	DEPENDENCIES rulengd-api
)