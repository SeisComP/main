SET(PACKAGE_NAME DAPLUGIN)
SET(LIB_NAME daplugin)

SET(${PACKAGE_NAME}_HEADERS
	collector.h
	collector/sds.h
)

SET(${PACKAGE_NAME}_SOURCES
	collector.cpp
)

SC_ADD_SUBDIR_SOURCES(${PACKAGE_NAME} collector)

# Install the headers in install/seiscomp/plugins/dataavailabilty
SC_ADD_LIBRARY(${PACKAGE_NAME} ${LIB_NAME})
SC_LIB_INSTALL_HEADERS(${PACKAGE_NAME} seiscomp/plugins/dataavailabilty)

SC_LIB_LINK_LIBRARIES_INTERNAL(${LIB_NAME} core)

