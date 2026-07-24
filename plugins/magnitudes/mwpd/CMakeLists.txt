SET(PLUGIN_TARGET mwpd)

SET(
	PLUGIN_SOURCES
		config.cpp
		amplitude.cpp
		magnitude.cpp
		plugin.cpp
)

SET(
	PLUGIN_HEADERS
		mwpd.h
		version.h
)

SC_ADD_PLUGIN_LIBRARY(PLUGIN ${PLUGIN_TARGET} "")
SC_LINK_LIBRARIES_INTERNAL(${PLUGIN_TARGET} client)

FILE(GLOB descs "${CMAKE_CURRENT_SOURCE_DIR}/descriptions/*.xml")
INSTALL(FILES ${descs} DESTINATION ${SC3_PACKAGE_APP_DESC_DIR})
