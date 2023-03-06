find_package(PkgConfig)
pkg_check_modules(PC_JSON-C QUIET json-c)

find_path(JSON-C_INCLUDE_DIR json.h
	HINTS ${PC_JSON-C_INCLUDEDIR} ${PC_JSON-C_INCLUDE_DIRS} PATH_SUFFIXES json-c)

find_library(JSON-C_LIBRARY NAMES json-c libjson-c
	HINTS ${PC_JSON-C_LIBDIR} ${PC_JSON-C_LIBRARY_DIRS})

set(JSON-C_LIBRARIES ${JSON-C_LIBRARY})
set(JSON-C_INCLUDE_DIRS ${JSON-C_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(JSON-C DEFAULT_MSG JSON-C_LIBRARY JSON-C_INCLUDE_DIR)

mark_as_advanced(JSON-C_INCLUDE_DIR JSON-C_LIBRARY)

if (JSON-C_INCLUDE_DIRS AND JSON-C_LIBRARY)
  SET(JSON-C_FOUND TRUE)
endif (JSON-C_INCLUDE_DIRS AND JSON-C_LIBRARY)

if (JSON-C_FOUND)
	message(STATUS "Found libjson-c: ${JSON-C_LIBRARY}")
else (JSON-C_FOUND)
	if (JSON-C_FOUND REQUIRED)
		if (NOT JSON-C_INCLUDE_DIRS)
			message(FATAL_ERROR "Could not find json.h header file!")
		endif (NOT JSON-C_INCLUDE_DIRS)

		if (NOT JSON-C_LIBRARY)
			message(FATAL_ERROR "Could not find libjson-c.so library!")
		endif (NOT JSON-C_LIBRARY)
	endif (JSON-C_FOUND REQUIRED)
endif (JSON-C_FOUND)
