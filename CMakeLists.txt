cmake_minimum_required(VERSION 2.6)

project(rulengd C)

add_definitions(
  -Wall
  -Wextra
  -Wuninitialized
  -Wundef
  -Wshadow
  -Wunreachable-code
  -Wfloat-equal
  -std=gnu11
  )

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules")

find_package(LIBUBOX REQUIRED)
find_package(LIBUCI REQUIRED)
find_package(LIBUBUS REQUIRED)
find_package(JSON-C REQUIRED)

set(RULENGD_LINK
  ${LIBUBOX_LIBRARIES}
  ${LIBUCI_LIBRARIES}
  ${LIBUBUS_LIBRARIES}
  ${JSON-C_LIBRARIES}
  )

set(SOURCES
  src/main.c
  src/ruleng.c
  src/ruleng_ubus.c
  src/ruleng_rules.c
  src/ruleng_json.c
  )

add_executable(rulengd ${SOURCES})
target_link_libraries(rulengd ${RULENGD_LINK})

#testing
IF(CMAKE_BUILD_TYPE STREQUAL Debug)
	OPTION(ENABLE_BUILD_TESTS "Build tests" ON)
	#OPTION(ENABLE_VALGRIND_TESTS "Build tests with valgrind" ON)
ELSE()
	OPTION(ENABLE_BUILD_TESTS "Build tests" OFF)
	#OPTION(ENABLE_VALGRIND_TESTS "Build tests with valgrind" OFF)
ENDIF()

IF(ENABLE_BUILD_TESTS)
	FIND_PACKAGE(CMocka)
	if(CMOCKA_FOUND)
		#INCLUDE(CodeCoverage)
		#APPEND_COVERAGE_COMPILER_FLAGS()
		ADD_LIBRARY(${PROJECT_NAME}-api SHARED ${SOURCES})
		MESSAGE("-- Building tests")
		ENABLE_TESTING()
		ADD_SUBDIRECTORY(test)
		ADD_SUBDIRECTORY(test/template_obj)
	ELSE(CMOCKA_FOUND)
		MESSAGE("-- CMocka not found")
	ENDIF(CMOCKA_FOUND)
ENDIF(ENABLE_BUILD_TESTS)

install(FILES test/data/ruleng-test-uci DESTINATION /etc/config)
install(FILES test/data/ruleng-test-recipe DESTINATION /etc/config)
install(FILES test/data/recipe_1.json DESTINATION /etc)
