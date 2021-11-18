SET(LOC_TARGET scautoloc)

SET(
	LOC_SOURCES
		app.cpp
		main.cpp
)

SET(
	LOC_HEADERS
		app.h
		util.h
)

SET(
	LOC_DATA
		config/grid.conf
		config/station.conf
		config/station-locations.conf
)

#INCLUDE_DIRECTORIES(.)

SC_ADD_EXECUTABLE(LOC ${LOC_TARGET})
SC_LINK_LIBRARIES_INTERNAL(${LOC_TARGET} client autoloc)

SC_INSTALL_DATA(LOC ${LOC_TARGET})
SC_INSTALL_INIT(${LOC_TARGET} ${INIT_TEMPLATE})

FILE(GLOB descs "${CMAKE_CURRENT_SOURCE_DIR}/descriptions/*.xml")
INSTALL(FILES ${descs} DESTINATION ${SC3_PACKAGE_APP_DESC_DIR})
