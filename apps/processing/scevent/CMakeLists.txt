SUBDIRS(seiscomp plugins)

SET(EVENT_TARGET scevent)

SET(
	EVENT_SOURCES
		main.cpp
		eventtool.cpp
		eventinfo.cpp
		util.cpp
		constraints.cpp
)


SET(
	EVENT_HEADERS
		eventtool.h
		eventinfo.h
		config.h
		constraints.h
		util.h
)

INCLUDE_DIRECTORIES(.)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR})

# Optional SQLite3-backed persistent eventID cache. Enabled by default; turn
# OFF to build scevent without SQLite3 development files installed. When OFF,
# the eventIDSync.db / databaseRetention options are unavailable and the
# reservation cache is in-memory only.
OPTION(SCEVENT_WITH_SQLITE3
       "Build the SQLite3-backed persistent eventID cache" ON)

IF(SCEVENT_WITH_SQLITE3)
	FIND_PACKAGE(SQLite3 REQUIRED)
	INCLUDE_DIRECTORIES(${SQLITE3_INCLUDE_DIR})
	LIST(APPEND EVENT_SOURCES allocationstore.cpp)
	LIST(APPEND EVENT_HEADERS allocationstore.h)
ENDIF()

SC_ADD_EXECUTABLE(EVENT ${EVENT_TARGET})
SC_LINK_LIBRARIES_INTERNAL(${EVENT_TARGET} client evplugin)
SC_INSTALL_INIT(${EVENT_TARGET} config/scevent.py)

IF(SCEVENT_WITH_SQLITE3)
	SC_LINK_LIBRARIES(${EVENT_TARGET} ${SQLITE3_LIBRARIES})
	TARGET_COMPILE_DEFINITIONS(${EVENT_TARGET} PRIVATE SCEVENT_WITH_SQLITE3)
ENDIF()

FILE(GLOB descs "${CMAKE_CURRENT_SOURCE_DIR}/descriptions/*.xml")
INSTALL(FILES ${descs} DESTINATION ${SC3_PACKAGE_APP_DESC_DIR})
