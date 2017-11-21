find_path(UCI_INCLUDE_DIR uci.h)
find_library(UCI_LIBRARY NAMES uci)

if (UCI_INCLUDE_DIR AND UCI_LIBRARY)
	SET(UCI_FOUND TRUE)
endif (UCI_INCLUDE_DIR AND UCI_LIBRARY)

if (UCI_FOUND)
	message(STATUS "Found libuci: ${UCI_LIBRARY}")
else (UCI_FOUND)
	if (UCI_FOUND REQUIRED)
		if (NOT UCI_INCLUDE_DIR)
			message(FATAL_ERROR "Could not find uci.h header file!")
		endif (NOT UCI_INCLUDE_DIR)

		if (NOT UCI_LIBRARY)
			message(FATAL_ERROR "Could not find libuci.so library!")
		endif (NOT UCI_LIBRARY)
	endif (UCI_FOUND REQUIRED)
endif (UCI_FOUND)
