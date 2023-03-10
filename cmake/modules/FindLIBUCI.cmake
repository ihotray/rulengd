find_path(LIBUCI_INCLUDE_DIRS uci.h)
find_library(LIBUCI_LIBRARIES NAMES uci)

if (LIBUCI_INCLUDE_DIRS AND LIBUCI_LIBRARIES)
	SET(UCI_FOUND TRUE)
endif (LIBUCI_INCLUDE_DIRS AND LIBUCI_LIBRARIES)

if (UCI_FOUND)
	message(STATUS "Found libuci: ${LIBUCI_LIBRARIES}")
else (UCI_FOUND)
	if (UCI_FOUND REQUIRED)
		if (NOT LIBUCI_INCLUDE_DIRS)
			message(FATAL_ERROR "Could not find uci.h header file!")
		endif (NOT LIBUCI_INCLUDE_DIRS)

		if (NOT LIBUCI_LIBRARIES)
			message(FATAL_ERROR "Could not find libuci.so library!")
		endif (NOT LIBUCI_LIBRARIES)
	endif (UCI_FOUND REQUIRED)
endif (UCI_FOUND)
