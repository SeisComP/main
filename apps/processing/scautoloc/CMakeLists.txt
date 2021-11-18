SET(LOC_TARGET scautoloc)

SET(
	LOC_SOURCES
		lib/util.cpp
		lib/associator.cpp
		lib/datamodel.cpp
 		lib/stationconfig.cpp
		lib/stationlocationfile.cpp
		lib/sc3adapters.cpp
		lib/nucleator.cpp
		lib/locator.cpp
		lib/autoloc.cpp
		lib/config.cpp
		app.cpp
		main.cpp
)

SET(
	LOC_HEADERS
		app.h
		lib/associator.h
		lib/autoloc.h
		lib/nucleator.h
		lib/locator.h
		util.h
		lib/sc3adapters.h
		lib/datamodel.h
		lib/util.h
 		lib/stationconfig.h
		lib/stationlocationfile.h
)

SET(
	LOC_DATA
		config/grid.conf
		config/station.conf
		config/station-locations.conf
)

INCLUDE_DIRECTORIES(.)

SC_ADD_EXECUTABLE(LOC ${LOC_TARGET})
SC_LINK_LIBRARIES_INTERNAL(${LOC_TARGET} client autoloc)

SC_INSTALL_DATA(LOC ${LOC_TARGET})
SC_INSTALL_INIT(${LOC_TARGET} ${INIT_TEMPLATE})

FILE(GLOB descs "${CMAKE_CURRENT_SOURCE_DIR}/descriptions/*.xml")
INSTALL(FILES ${descs} DESTINATION ${SC3_PACKAGE_APP_DESC_DIR})

SUBDIRS(lib)
