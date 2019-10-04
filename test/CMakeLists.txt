CMAKE_MINIMUM_REQUIRED(VERSION 3.0)
INCLUDE_DIRECTORIES ("${PROJECT_SOURCE_DIR}/src")
FILE(COPY data DESTINATION .)

SET(api_tests rulengd_test)
FOREACH(test_name IN LISTS api_tests)
    ADD_EXECUTABLE(${test_name} ${test_name}.c)
	TARGET_LINK_LIBRARIES(
		${test_name}
		${CMOCKA_LIBRARIES}
		rulengd-api
		${LIBUBOX_LIBRARIES}
		${LIBUCI_LIBRARIES}
		${LIBUBUS_LIBRARIES}
		${JSON-C_LIBRARIES}
		pthread
	)
	ADD_TEST(NAME ${test_name} COMMAND $<TARGET_FILE:${test_name}>)
ENDFOREACH(test_name)

IF(ENABLE_VALGRIND_TESTS)
	FIND_PROGRAM(valgrind_FOUND valgrind)
	IF(valgrind_FOUND)
		FOREACH(test_name IN LISTS api_tests)
			ADD_TEST(${test_name}_valgrind valgrind --leak-check=full
				--show-reachable=yes --show-leak-kinds=all
				--errors-for-leak-kinds=all --error-exitcode=1
				 ${CMAKE_BINARY_DIR}/test/${test_name})
		ENDFOREACH(test_name)
	ELSE(valgrind_FOUND)
		MESSAGE("-- Valgrind executable not found! Disabling memory leaks tests")
	ENDIF(valgrind_FOUND)
ENDIF(ENABLE_VALGRIND_TESTS)

#SET(COVERAGE_EXCLUDES 'src/main.c' '/usr/include/*')
#
#SETUP_TARGET_FOR_COVERAGE(
#    NAME test_coverage
#	EXECUTABLE ctest
#	DEPENDENCIES rulengd-api
#)
