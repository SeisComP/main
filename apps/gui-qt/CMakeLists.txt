SET(CPACK_PACKAGE_EXECUTABLES "scrttv;scrttv" "scmv;scmv" "scolv;scolv" "scesv;scesv")

SET(Boost_DETAILED_FAILURE_MSG ON)
FIND_PACKAGE(Boost REQUIRED COMPONENTS program_options thread filesystem iostreams regex)
IF (WIN32)
	ADD_DEFINITIONS("-DBOOST_ALL_NO_LIB")
	ADD_DEFINITIONS("-DBOOST_LIB_DIAGNOSTIC")
ENDIF (WIN32)

LINK_DIRECTORIES(${Boost_LIBRARY_DIRS})
INCLUDE_DIRECTORIES(${Boost_INCLUDE_DIRS})

IF(SC_GLOBAL_GUI_QT5)
	FIND_PACKAGE(Qt5 REQUIRED COMPONENTS Xml)
	INCLUDE_DIRECTORIES(${Qt5Core_INCLUDE_DIRS})
	INCLUDE_DIRECTORIES(${Qt5Widgets_INCLUDE_DIRS})
	INCLUDE_DIRECTORIES(${Qt5Xml_INCLUDE_DIRS})
ELSEIF(SC_GLOBAL_GUI_QT6)
	FIND_PACKAGE(Qt6 REQUIRED COMPONENTS Xml)
	INCLUDE_DIRECTORIES(${Qt6Core_INCLUDE_DIRS})
	INCLUDE_DIRECTORIES(${Qt6Widgets_INCLUDE_DIRS})
	INCLUDE_DIRECTORIES(${Qt6Xml_INCLUDE_DIRS})
ELSE()
	MESSAGE(FATAL_ERROR "No Qt5/6 support enabled")
ENDIF()

SC_ADD_SUBDIRS()
