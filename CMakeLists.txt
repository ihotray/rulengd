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

install(FILES test/ruleng-test-rules DESTINATION /etc/config)
